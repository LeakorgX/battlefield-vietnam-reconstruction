"""Static recompilation: rebuild the whole executable image from source artifacts.

Every section is re-produced by the assembler from emitted source: .text from
disassembled instruction text (with byte-authoritative fallback where GAS would
re-encode differently), data sections from byte streams with patched vtable
slots as symbol references, and the reconstructed-C payload placed at its
manifest address. The gate is byte-identity with the installed editable build.
"""
import argparse
import hashlib
import json
import re
import struct
from pathlib import Path

import pefile
from capstone import Cs, CS_ARCH_X86, CS_MODE_32

import build
from build import PROJECT, TARGETS, COMPILERS, align, run

MAX_PASSES = 16


def load_target(target, game_dir):
    spec = TARGETS[target]
    original = (game_dir / spec['file']).read_bytes()
    if hashlib.sha256(original).hexdigest() != spec['sha']:
        raise RuntimeError('Original image hash differs from the verified ABI target')
    manifest = json.loads((PROJECT / 'build' / target / 'manifest.json').read_text(encoding='utf-8'))
    payload = (PROJECT / 'build' / target / 'payload.bin').read_bytes()
    symbols = {name: int(value, 16) for name, value in manifest['symbols'].items()}
    return spec, original, pefile.PE(data=original), payload, symbols, manifest


def collect_code_seeds(pe, manifest, text_rva, text_end, target):
    """Known function starts: entry point, exports, TLS callbacks, inventory
    entries, vtable targets and every manifest replacement target."""
    base = pe.OPTIONAL_HEADER.ImageBase
    seeds = {pe.OPTIONAL_HEADER.AddressOfEntryPoint + base}
    export = getattr(pe, 'DIRECTORY_ENTRY_EXPORT', None)
    if export:
        seeds.update(symbol.address + base for symbol in export.symbols)
    tls_dir = pe.OPTIONAL_HEADER.DATA_DIRECTORY[9]
    if tls_dir.VirtualAddress:
        tls = pe.get_data(tls_dir.VirtualAddress, 24)
        callbacks = struct.unpack_from('<I', tls, 12)[0]
        if callbacks:
            data = pe.get_data(callbacks - base, 256)
            for i in range(0, 256, 4):
                value = struct.unpack_from('<I', data, i)[0]
                if not value:
                    break
                seeds.add(value)
    seeds.update(int(p['entry'], 16) for p in manifest['entry_patches'])
    seeds.update(int(p['replacement'], 16) for p in manifest['entry_patches'])
    seeds.update(int(p['replacement'], 16) for p in manifest['patches'])
    for catalog in ('constant-returns.json', 'word-getters.json', 'field-accessors.json', 'static-initializers.json', 'deleting-destructors.json', 'constructor-calls.json', 'dynamic-casts.json', 'clones.json', 'global-stores.json'):
        entries = json.loads((PROJECT / catalog).read_text())['targets'][target]['entries']
        seeds.update(int(e['address'], 16) for e in entries)
    index = PROJECT.parent / 'reports' / target / 'all-functions.tsv'
    if index.exists():
        for row in index.read_text(encoding='utf-8').splitlines()[1:]:
            seeds.add(int(row.split('\t', 1)[0], 16))
    return {s for s in seeds if text_rva + base <= s < text_end + base}


def function_names(pe, manifest, target):
    """Readable labels for original functions that have reconstructed or
    catalogued counterparts (address -> name)."""
    names = {}
    for catalog in ('constant-returns.json', 'word-getters.json', 'field-accessors.json', 'static-initializers.json', 'deleting-destructors.json', 'constructor-calls.json', 'dynamic-casts.json', 'clones.json', 'global-stores.json'):
        for entry in json.loads((PROJECT / catalog).read_text())['targets'][target]['entries']:
            names[int(entry['address'], 16)] = 'orig_' + entry['symbol']
    for patch in manifest['entry_patches']:
        names.setdefault(int(patch['entry'], 16), f"orig_{patch['source']}_{patch['entry']}")
    return names


def detour_map(manifest):
    """Entry address -> (guard bytes, replacement symbol name)."""
    return {int(p['entry'], 16): (bytes.fromhex(p['original']), p['source'])
            for p in manifest['entry_patches']}


def vtable_map(manifest):
    return {int(p['vtable_slot'], 16): p['source'] for p in manifest['patches']}


BRANCH_MNEMONICS = {
    'jmp', 'call', 'jo', 'jno', 'jb', 'jae', 'je', 'jne', 'jbe', 'ja', 'js', 'jns',
    'jp', 'jnp', 'jl', 'jge', 'jle', 'jg', 'jecxz', 'jcxz', 'loop', 'loope', 'loopne'}
SHORT_ONLY = {'jecxz', 'jcxz', 'loop', 'loope', 'loopne'}
HEX_TARGET = re.compile(r'^0x[0-9a-f]+$')
# Pseudo-prefixes that select between equivalent encodings, tried in order when
# the assembler's default choice differs from the original bytes.
ENCODING_VARIANTS = ('{load} ', '{store} ', '{disp32} ', '{disp8} ',
                     '{load} {disp32} ', '{store} {disp32} ',
                     '{load} {disp8} ', '{store} {disp8} ')


STATE_MNEMONICS = {'fnsave', 'frstor', 'fnstenv', 'fldenv', 'fxsave', 'fxrstor'}


def spelling(mnemonic, op_str):
    """Capstone Intel text adjusted to GNU as Intel-syntax spelling."""
    op_str = op_str.replace('xword ptr ', 'tbyte ptr ')
    if mnemonic in STATE_MNEMONICS:
        op_str = re.sub(r'^\w+ ptr ', '', op_str)
    return f'{mnemonic} {op_str}'.strip()


def branch_form(code):
    """Return 'short' / 'near' for a plain relative branch encoding, else None
    (prefixed, 16-bit or otherwise unusual forms stay byte-encoded)."""
    op = code[0]
    if len(code) == 2 and (op == 0xeb or 0x70 <= op <= 0x7f or 0xe0 <= op <= 0xe3):
        return 'short'
    if len(code) == 5 and op in (0xe8, 0xe9):
        return 'near'
    if len(code) == 6 and op == 0x0f and 0x80 <= code[1] <= 0x8f:
        return 'near'
    return None


class TextEmitter:
    """Disassemble .text and emit GAS source that reassembles to the same bytes.

    Control flow is symbolic: every relative branch/call inside the image refers
    to a label (sub_XXXXXXXX for known function starts, loc_XXXXXXXX otherwise),
    and entry detours jump to the reconstructed C symbol by name. Each
    position-independent instruction's encoding is checked in isolation; where
    the assembler's default differs from the original, an encoding pseudo-prefix
    ({load}/{store}/{disp8}/{disp32}) selects the original form. Only
    instructions with no assembler spelling for their exact bytes fall back to
    .byte, and those keep the disassembly as a comment."""

    def __init__(self, data, va, labels, detours, symbols, names=None):
        self.data = data
        self.va = va
        self.seeds = set(labels)
        self.detours = detours
        self.symbols = symbols
        self.names = names or {}
        # (offset, size, kind, text, comment); kind: insn|branch|detour|bytes
        self.instructions = []
        self.line_map = {}
        self.targets = {}

    # -- planning -----------------------------------------------------------
    def plan(self):
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        offset = 0
        detour_limits = sorted(entry - self.va for entry in self.detours)
        end_va = self.va + len(self.data)
        while offset < len(self.data):
            if self.va + offset in self.detours:
                guard, source = self.detours[self.va + offset]
                self.instructions.append((offset, len(guard), 'detour', source, ''))
                offset += len(guard)
                continue
            limit = next((r for r in detour_limits if r > offset), len(self.data))
            last = offset
            for insn in md.disasm(self.data[offset:limit], self.va + offset):
                text = spelling(insn.mnemonic, insn.op_str)
                kind = 'insn'
                code = bytes(insn.bytes)
                if insn.mnemonic in BRANCH_MNEMONICS and HEX_TARGET.match(insn.op_str):
                    target = int(insn.op_str, 16)
                    form = branch_form(code)
                    if form and self.va <= target < end_va and \
                            not (form == 'near' and insn.mnemonic in SHORT_ONLY):
                        kind = 'branch'
                        text = (insn.mnemonic, target, form)
                        self.targets[target] = None
                self.instructions.append((insn.address - self.va, insn.size, kind, text, ''))
                last = insn.address - self.va + insn.size
            if last <= offset:
                self.instructions.append((offset, 1, 'bytes', None, '(undecodable)'))
                offset += 1
            else:
                offset = last
        self.starts = {off for off, *_ in self.instructions}
        for target in self.targets:
            self.targets[target] = self.label_name(target)

    def label_name(self, address):
        if address in self.names:
            return self.names[address]
        return f'sub_{address:08x}' if address in self.seeds else f'loc_{address:08x}'

    # -- per-instruction encoding selection ----------------------------------
    def select_encodings(self, work, log):
        """Assemble every distinct instruction spelling once, in isolated
        32-byte slots, and keep the first spelling whose bytes match."""
        pending = {}
        for index, (offset, size, kind, text, _) in enumerate(self.instructions):
            if kind == 'insn':
                pending.setdefault((text, self.data[offset:offset + size]), []).append(index)
        chosen = {}
        candidates = {key: [''] + list(ENCODING_VARIANTS) for key in pending}
        round_number = 0
        while candidates:
            batch = [(key, prefixes[0]) for key, prefixes in candidates.items()]
            results = self.assemble_isolated(work, [f'{p}{key[0]}' for key, p in batch])
            next_candidates = {}
            for (key, prefix), produced in zip(batch, results):
                if produced == key[1]:
                    chosen[key] = prefix
                elif len(candidates[key]) > 1:
                    next_candidates[key] = candidates[key][1:]
            log.append(f'encoding round {round_number}: {len(batch)} spellings, '
                       f'{sum(1 for k, _ in batch if k in chosen)} exact')
            candidates = next_candidates
            round_number += 1
        exact = unmatched = 0
        for key, indices in pending.items():
            for index in indices:
                offset, size, kind, text, comment = self.instructions[index]
                if key in chosen:
                    self.instructions[index] = (offset, size, 'insn', chosen[key] + text, comment)
                    exact += 1
                else:
                    self.instructions[index] = (offset, size, 'bytes', text, comment)
                    unmatched += 1
        log.append(f'instruction spellings: {exact} exact, {unmatched} byte-encoded')

    def assemble_isolated(self, work, texts):
        """Return the encoded bytes of each text (None when it fails to assemble)."""
        results = [None] * len(texts)
        live = list(range(len(texts)))
        for _ in range(8):
            if not live:
                break
            lines = ['.intel_syntax noprefix', '.text']
            line_of = {}
            for slot, index in enumerate(live):
                lines.append('.p2align 5, 0xcc')
                lines.append(f'.byte E{slot} - S{slot}')
                lines.append(f'S{slot}:')
                lines.append(f'  {texts[index]}')
                line_of[len(lines)] = slot
                lines.append(f'E{slot}:')
            path = work / 'encodings.s'
            path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
            try:
                run([COMPILERS / 'as.exe', '--32', '-o', str(work / 'encodings.o'), str(path)], work)
            except RuntimeError as error:
                bad = {line_of[int(n)] for n in re.findall(r'encodings\.s:(\d+):', str(error))
                       if int(n) in line_of}
                if not bad:
                    raise
                live = [index for slot, index in enumerate(live) if slot not in bad]
                continue
            blob = object_bytes(work / 'encodings.o', '.text')
            for slot, index in enumerate(live):
                base = slot * 32
                length = blob[base]
                results[index] = blob[base + 1:base + 1 + length]
            break
        return results

    # -- emission -------------------------------------------------------------
    def detour_bytes(self, offset):
        entry = self.va + offset
        guard, source = self.detours[entry]
        target = self.symbols[source]
        return b'\xe9' + struct.pack('<i', target - (entry + 5)) + b'\x90' * (len(guard) - 5), source

    def emit(self, path):
        lines = ['.intel_syntax noprefix', '.text',
                 f'/* .text of the original image, virtual address 0x{self.va:08x} */',
                 '__text_base:']
        self.line_map = {}
        defined = set()
        boundary_labels = {}
        for target, name in self.targets.items():
            if target - self.va in self.starts:
                boundary_labels[target] = name
        for address in self.seeds:
            if address - self.va in self.starts:
                boundary_labels.setdefault(address, self.label_name(address))
        for index, (offset, size, kind, text, comment) in enumerate(self.instructions):
            address = self.va + offset
            name = boundary_labels.get(address)
            if name:
                if address in self.seeds or address in self.names:
                    lines.append('')
                lines.append(f'{name}:')
                defined.add(name)
            raw = self.data[offset:offset + size]
            if kind == 'detour':
                _, source = self.detour_bytes(offset)
                lines.append(f'  {{disp32}} jmp {source} /* detour to reconstructed C; '
                             f'original {raw.hex()} */')
                if size > 5:
                    lines.append('  .byte ' + ','.join(['0x90'] * (size - 5)))
            elif kind == 'branch':
                mnemonic, target, form = text
                prefix = '{disp32} ' if form == 'near' and mnemonic != 'call' else ''
                lines.append(f'  {prefix}{mnemonic} {self.targets[target]}')
            elif kind == 'insn':
                lines.append(f'  {text}')
            else:
                note = f' /* {text} */' if text else f' /* {comment} */' if comment else ''
                lines.append('  .byte ' + ','.join(f'0x{b:02x}' for b in raw) + note)
            self.line_map[len(lines)] = index
        lines.append('')
        lines.append('/* Branch targets that fall inside another instruction or outside decoded code. */')
        for target, name in sorted(self.targets.items()):
            if name not in defined:
                lines.append(f'.set {name}, __text_base + 0x{target - self.va:x}')
        for source in sorted({s for _, _, k, s, _ in self.instructions if k == 'detour'}):
            lines.append(f'.set {source}, __text_base + 0x{self.symbols[source] - self.va:x}')
        path.write_text('\n'.join(lines) + '\n', encoding='utf-8')

    def suppress(self, indices):
        for index in indices:
            offset, size, kind, text, comment = self.instructions[index]
            if kind in ('insn', 'branch'):
                label = text if kind == 'insn' else f'{text[0]} 0x{text[1]:08x}'
                self.instructions[index] = (offset, size, 'bytes', label, comment)

    def expected_image(self):
        """Original bytes with the guard intervals replaced by detour patches."""
        image = bytearray(self.data)
        for offset, size, kind, _, _ in self.instructions:
            if kind == 'detour':
                blob, _ = self.detour_bytes(offset)
                image[offset:offset + size] = blob
        return bytes(image)

    def converge(self, work, name, log):
        self.select_encodings(work, log)
        expected = self.expected_image()
        for attempt in range(MAX_PASSES):
            self.emit(work / f'{name}.s')
            try:
                run([COMPILERS / 'as.exe', '--32', '-o', str(work / f'{name}.o'),
                     str(work / f'{name}.s')], work)
            except RuntimeError as error:
                numbers = sorted({int(m) for m in re.findall(rf'{name}\.s:(\d+):', str(error))})
                indices = [self.line_map[n] for n in numbers if n in self.line_map]
                if not indices:
                    raise
                log.append(f'pass {attempt}: byte-encoded {len(indices)} erroring lines')
                self.suppress(indices)
                continue
            got = object_bytes(work / f'{name}.o', '.text')[:len(expected)]
            if got == expected:
                counts = {}
                for _, _, kind, _, _ in self.instructions:
                    counts[kind] = counts.get(kind, 0) + 1
                log.append(f'pass {attempt}: byte-exact; ' +
                           ', '.join(f'{k}={v}' for k, v in sorted(counts.items())))
                return log
            # Suppress only the first differing instruction: a size change there
            # shifts every later offset, so later "differences" are not evidence.
            first = next(i for i, (offset, size, kind, _, _) in enumerate(self.instructions)
                         if got[offset:offset + size] != expected[offset:offset + size])
            offset, size, kind, text, _ = self.instructions[first]
            if kind not in ('insn', 'branch'):
                raise RuntimeError(f'{name}: byte-encoded data differs at {self.va + offset:08x}')
            log.append(f'pass {attempt}: first difference at {self.va + offset:08x} ({text})')
            self.suppress([first])
        raise RuntimeError(f'{name}: no byte-exact convergence')


def object_bytes(path, section_name):
    data = path.read_bytes()
    machine, nsec, _, _, _, opthdr, _ = struct.unpack_from('<HHIIIHH', data, 0)
    offset = 20 + opthdr
    for _ in range(nsec):
        name, vsize, vaddr, rawsize, rawptr = struct.unpack_from('<8sIIII', data, offset)
        if name.rstrip(b'\0') == section_name.encode():
            return data[rawptr:rawptr + rawsize]
        offset += 40
    raise RuntimeError(f'Section {section_name} not found in {path.name}')


def emit_data_section(data, va, path, refs):
    """Byte stream with .long symbol references at patched vtable slots."""
    lines = ['.intel_syntax noprefix', f'.section .{path.stem}']
    refs = {rva - va: address for rva, address in refs.items()}
    offset = 0
    chunk = 32
    while offset < len(data):
        if offset in refs:
            lines.append(f'L{va + offset:08x}:')
            lines.append(f'  .long 0x{refs[offset]:08x} /* vtable slot */')
            offset += 4
            continue
        take = min(chunk, len(data) - offset)
        next_ref = min((r for r in refs if r > offset), default=len(data))
        take = min(take, next_ref - offset)
        blob = data[offset:offset + take]
        lines.append('  .byte ' + ','.join(f'0x{b:02x}' for b in blob))
        offset += take
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def build_recomp(target, game_dir, assemble_only=False, install=False):
    spec, original, pe, payload, symbols, manifest = load_target(target, game_dir)
    base = pe.OPTIONAL_HEADER.ImageBase
    work = PROJECT / 'build' / f'{target}-recomp'
    work.mkdir(parents=True, exist_ok=True)
    text_section = next(s for s in pe.sections if s.Name.rstrip(b'\0') == b'.text')
    text_rva = text_section.VirtualAddress
    text = pe.get_data(text_rva, text_section.SizeOfRawData)

    seeds = collect_code_seeds(pe, manifest, text_rva, text_rva + len(text), target)
    log = []
    if assemble_only:
        # Build from the existing (possibly hand-edited) sources without regenerating.
        for section in pe.sections:
            stem = section.Name.rstrip(b'\0').decode()[1:]
            if not (work / f'{stem}.s').exists():
                raise RuntimeError(f'{work / (stem + ".s")} missing; run without --assemble-only first')
        run([COMPILERS / 'as.exe', '--32', '-o', str(work / 'text.o'), str(work / 'text.s')], work)
        log.append('assembled existing text.s')
    else:
        emitter = TextEmitter(text, base + text_rva, seeds, detour_map(manifest), symbols,
                              function_names(pe, manifest, target))
        emitter.plan()
        emitter.converge(work, 'text', log)
    outputs = {'.text': object_bytes(work / 'text.o', '.text')}

    vtables = vtable_map(manifest)
    for section in pe.sections:
        name = section.Name.rstrip(b'\0').decode()
        if name == '.text':
            continue
        data = pe.get_data(section.VirtualAddress, section.SizeOfRawData)
        refs = {rva: symbols[vtables[rva]] for rva in vtables
                if section.VirtualAddress + base <= rva
                < section.VirtualAddress + base + section.SizeOfRawData}
        if not assemble_only:
            emit_data_section(data, base + section.VirtualAddress, work / f'{name[1:]}.s', refs)
        run([COMPILERS / 'as.exe', '--32', '-o', str(work / f'{name[1:]}.o'),
             str(work / f'{name[1:]}.s')], work)
        outputs[name] = object_bytes(work / f'{name[1:]}.o', name)

    rva = align(pe.OPTIONAL_HEADER.SizeOfImage, pe.OPTIONAL_HEADER.SectionAlignment)
    output = bytearray(original)
    raw = align(len(original), pe.OPTIONAL_HEADER.FileAlignment)
    output.extend(b'\0' * (raw - len(output)))
    output.extend(payload)
    raw_size = align(len(payload), pe.OPTIONAL_HEADER.FileAlignment)
    output.extend(b'\0' * (raw_size - len(payload)))
    header_offset = pe.sections[-1].get_file_offset() + 40
    section_header = struct.pack('<8sIIIIIIHHI', b'.bfvmod\0', len(payload), rva,
                                 raw_size, raw, 0, 0, 0, 0, 0xe0000060)
    output[header_offset:header_offset + 40] = section_header
    struct.pack_into('<H', output, pe.FILE_HEADER.get_field_absolute_offset('NumberOfSections'),
                     len(pe.sections) + 1)
    struct.pack_into('<I', output, pe.OPTIONAL_HEADER.get_field_absolute_offset('SizeOfImage'),
                     align(rva + len(payload), pe.OPTIONAL_HEADER.SectionAlignment))
    struct.pack_into('<I', output, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'), 0)

    for section in pe.sections:
        name = section.Name.rstrip(b'\0').decode()
        produced = outputs[name]
        if len(produced) < section.SizeOfRawData or \
                any(produced[section.SizeOfRawData:]):
            raise RuntimeError(f'{name}: assembler produced {len(produced)} bytes, '
                               f'section holds {section.SizeOfRawData}. Edits must keep every '
                               f'section the same size: addresses in data and code are absolute.')
        file_offset = section.PointerToRawData
        output[file_offset:file_offset + section.SizeOfRawData] = produced[:section.SizeOfRawData]

    new_pe = pefile.PE(data=bytes(output))
    struct.pack_into('<I', output, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),
                     new_pe.generate_checksum())

    destination = work / spec['output']
    destination.write_bytes(bytes(output))
    installed = (game_dir / spec['output']).read_bytes()
    differences = [i for i in range(min(len(installed), len(output))) if installed[i] != output[i]]
    report = dict(target=target, original_sha256=spec['sha'],
                  recomp_sha256=hashlib.sha256(output).hexdigest(),
                  installed_sha256=manifest['output_sha256'],
                  byte_identity=bool(differences == [] and len(installed) == len(output)),
                  first_differences=[f'{d:08x}' for d in differences[:16]],
                  differing_bytes=len(differences),
                  assembly_log=log, sections={k: len(v) for k, v in outputs.items()},
                  payload_bytes=len(payload))
    report['mode'] = 'assemble-only' if assemble_only else 'regenerated'
    (work / 'recomp-report.json').write_text(json.dumps(report, indent=2))
    if install:
        (game_dir / spec['output']).write_bytes(bytes(output))
        print(f'{target}: installed {game_dir / spec["output"]}')
    print(f"{target}: recomp {destination.name} identity={report['byte_identity']} "
          f"diffs={report['differing_bytes']} log={log}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=('client', 'server', 'both'), default='both')
    parser.add_argument('--game-dir', type=Path, default=build.GAME)
    parser.add_argument('--assemble-only', action='store_true',
                        help='assemble the existing build/<target>-recomp/*.s (keeps hand edits) '
                             'instead of regenerating them from the image')
    parser.add_argument('--install', action='store_true',
                        help='copy a successful build over <game-dir>/<editable output>')
    args = parser.parse_args()
    game_dir = args.game_dir.resolve()
    for target in ('client', 'server') if args.target == 'both' else (args.target,):
        build_recomp(target, game_dir, args.assemble_only, args.install)


if __name__ == '__main__':
    main()
