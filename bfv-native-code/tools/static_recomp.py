"""Static recompilation: rebuild the whole executable image from source artifacts.

Every section is re-produced by the assembler from emitted source: .text from
disassembled instruction text (with byte-authoritative fallback where GAS would
re-encode differently), data sections from byte streams with patched vtable
slots as symbol references, and the reconstructed-C payload placed at its
manifest address. The gate is byte-identity with the installed editable build.
"""
import argparse
import bisect
import hashlib
import json
import re
import struct
import subprocess
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
    """Entry address -> (guard bytes, replacement symbol name, trampoline or None).
    A two-byte guard is a `jmp short` to a five-byte trampoline in int3 padding."""
    return {int(p['entry'], 16): (bytes.fromhex(p['original']), p['source'],
                                  int(p['via'], 16) if 'via' in p else None)
            for p in manifest['entry_patches'] + manifest.get('entry_trampolines', [])}


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
# GNU as picks the shortest immediate form for a known value (imm8 for 0xffff in
# a 16-bit compare). An immediate written as a symbol defined at the end of the
# file is unknown while the instruction is encoded, so the full-width form the
# original uses is kept; the symbol is absolute, so nothing is relocated.
LAST_IMMEDIATE = re.compile(r'(, |^)(-?0x[0-9a-f]+|-?\d+)$')
WIDE_SYMBOL = re.compile(r'bfv_wide_(m?)([0-9a-f]+)')
SEGMENT_PREFIXES = {0x26: 'es', 0x2e: 'cs', 0x36: 'ss', 0x3e: 'ds', 0x64: 'fs', 0x65: 'gs'}


def wide_name(value):
    return f"bfv_wide_{'m' if value < 0 else ''}{abs(value):x}"


def wide_immediate(text):
    """`text` with its last operand, an immediate, as a wide_name symbol (None
    when the last operand is not a number)."""
    head, _, operands = text.partition(' ')
    match = LAST_IMMEDIATE.search(operands)
    if not operands or match is None:
        return None, None
    value = int(match.group(2), 0)
    return (f'{head} {operands[:match.start(2)]}offset {wide_name(value)}', value)


def spelling(mnemonic, op_str):
    """Capstone Intel text adjusted to GNU as Intel-syntax spelling."""
    op_str = op_str.replace('xword ptr ', 'tbyte ptr ')
    if mnemonic in STATE_MNEMONICS:
        op_str = re.sub(r'^\w+ ptr ', '', op_str)
    # far pointer loads (les, lds, ...) come without a size: GAS would read a bare
    # `ptr` as an undefined symbol, which only matches while nothing is linked
    op_str = re.sub(r'(^|, )ptr \[', r'\1fword ptr [', op_str)
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

    def __init__(self, data, va, labels, detours, symbols, names=None, tables=None, refs=None):
        self.data = data
        self.va = va
        self.seeds = set(labels)
        self.detours = detours
        self.symbols = symbols
        self.names = names or {}
        # symbolic mode: switch tables in .text ({start: (end, 'jump'|'byte')}) and
        # absolute references ({field address: (kind, target)}) from symscan
        self.tables = tables or {}
        self.refs = refs or {}
        self.symbolic = tables is not None
        self.c_linked = False    # detour targets resolved by the link with the C payload
        self.decorated = {}      # with c_linked: C name -> its decorated name
        self.extra_sets = []     # (global name, expression) defined in text.s
        self.data_symbol = None  # target address -> symbol, for targets outside .text
        self.exported = set()    # labels other sections refer to
        self.item_refs = {}      # instruction index -> [(field offset, kind, target)]
        self.masked = set()      # .text offsets of symbolic fields (resolved by the linker)
        self.edits = {}          # address -> ('insert'|'replace', assembly): hand edits (--insert/--replace)
        self.wide_values = set() # immediates spelled as bfv_wide_ symbols (wide_immediate)
        self.far_targets = {}    # branch targets in other sections -> their d_ symbol
        # (offset, size, kind, text, comment); kind: insn|branch|detour|bytes|table
        self.instructions = []
        self.line_map = {}
        self.targets = {}

    # -- planning -----------------------------------------------------------
    def plan(self):
        md = Cs(CS_ARCH_X86, CS_MODE_32)
        offset = 0
        detour_limits = sorted([entry - self.va for entry in self.detours] +
                               [start - self.va for start in self.tables])
        end_va = self.va + len(self.data)
        while offset < len(self.data):
            if self.va + offset in self.tables:
                end, kind = self.tables[self.va + offset]
                self.instructions.append((offset, end - self.va - offset, 'table', kind, ''))
                offset = end - self.va
                continue
            if self.va + offset in self.detours:
                guard, source, via = self.detours[self.va + offset]
                self.instructions.append((offset, len(guard), 'detour', source, ''))
                if via is not None:  # the rest of the first instruction stays as it was
                    first = next(md.disasm(self.data[offset:offset + 16], self.va + offset), None)
                    if first is not None and first.size > len(guard):
                        self.instructions.append((offset + len(guard), first.size - len(guard), 'bytes', '',
                                                  f'rest of `{first.mnemonic} {first.op_str}`'))
                        offset += first.size - len(guard)
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
                    elif form == 'near' and self.symbolic and self.data_symbol and \
                            (far := self.far_symbol(target)):
                        kind = 'branch'  # into another section of the image (.ctfsnd code)
                        text = (insn.mnemonic, target, form)
                        self.far_targets[target] = far
                self.instructions.append((insn.address - self.va, insn.size, kind, text, ''))
                last = insn.address - self.va + insn.size
            if last <= offset:
                self.instructions.append((offset, 1, 'bytes', None, '(undecodable)'))
                offset += 1
            else:
                offset = last
        if self.symbolic:
            self.attach_refs()
        self.starts = {off for off, *_ in self.instructions}
        for target in self.targets:
            self.targets[target] = self.label_name(target)

    def attach_refs(self):
        """Give each reference field the item that holds it. A field the linear
        sweep split across items (undecodable runs, code after a short guard)
        merges those items into one byte item; a field under a detour guard is
        gone (the guard replaced its bytes)."""
        offsets = [item[0] for item in self.instructions]
        merged = []
        fields = sorted((where - self.va, kind, target) for where, (kind, target) in self.refs.items()
                        if self.va <= where < self.va + len(self.data))
        spans = {}
        for field, kind, target in fields:
            first = bisect.bisect_right(offsets, field) - 1
            last = bisect.bisect_right(offsets, field + 3) - 1
            if any(self.instructions[i][2] == 'detour' for i in range(first, last + 1)):
                continue
            if first != last:
                spans.setdefault(first, last)
                spans[first] = max(spans[first], last)
        if spans:
            rebuilt, index = [], 0
            while index < len(self.instructions):
                if index in spans:
                    last = spans[index]
                    while any(i in spans and spans[i] > last for i in range(index, last + 1)):
                        last = max(spans[i] for i in range(index, last + 1) if i in spans)
                    start = self.instructions[index][0]
                    end = self.instructions[last][0] + self.instructions[last][1]
                    if any(self.instructions[i][2] in ('detour', 'table') for i in range(index, last + 1)):
                        raise RuntimeError(f'reference field across a detour or table at {self.va + start:08x}')
                    rebuilt.append((start, end - start, 'bytes', None, 'merged around a reference field'))
                    merged.append(self.va + start)
                    index = last + 1
                else:
                    rebuilt.append(self.instructions[index])
                    index += 1
            self.instructions = rebuilt
            offsets = [item[0] for item in self.instructions]
        self.item_refs = {}
        for field, kind, target in fields:
            index = bisect.bisect_right(offsets, field) - 1
            offset, size, item_kind = self.instructions[index][:3]
            if item_kind == 'detour' or field + 4 > offset + size:
                continue
            self.item_refs.setdefault(index, []).append((field - offset, kind, target))
        self.merged = merged

    def far_symbol(self, target):
        """The d_ symbol of a branch target in another image section, or None."""
        try:
            return self.data_symbol(target)
        except RuntimeError:
            return None

    def label_name(self, address):
        if address in self.tables:
            return f"{'jt' if self.tables[address][1] == 'jump' else 'bt'}_{address:08x}"
        if address in self.names:
            return self.names[address]
        return f'sub_{address:08x}' if address in self.seeds else f'loc_{address:08x}'

    def symbol(self, target, export=False):
        """Assembler expression for an absolute target address."""
        if not self.va <= target < self.va + len(self.data):
            return self.data_symbol(target)
        offset = target - self.va
        if offset in self.starts:
            name = self.targets.setdefault(target, self.label_name(target))
            expression = name
        else:
            table = next(((s, e) for s, (e, _) in self.tables.items() if s < target < e), None)
            below = next((s for s in range(target + 4, target + 17, 4) if s in self.tables), None)
            if table:
                name = self.targets.setdefault(table[0], self.label_name(table[0]))
                expression = f'{name}+{target - table[0]}'
            elif below:
                name = self.targets.setdefault(below, self.label_name(below))
                expression = f'{name}-{below - target}'
            else:  # inside an instruction: defined by .set from the section base
                name = self.targets.setdefault(target, self.label_name(target))
                expression = name
        if export:
            self.exported.add(name)
        return expression

    # -- per-instruction encoding selection ----------------------------------
    def select_encodings(self, work, log):
        """Assemble every distinct instruction spelling once, in isolated
        32-byte slots, and keep the first spelling whose bytes match."""
        pending = {}
        for index, (offset, size, kind, text, _) in enumerate(self.instructions):
            if kind == 'insn':
                pending.setdefault((text, self.data[offset:offset + size]), []).append(index)
        chosen = {}

        def spellings(key):
            """Candidate texts: the pseudo-prefixes, then a segment prefix the
            text does not show, then the immediate written wide."""
            text, raw = key
            bases = [text]
            segment = SEGMENT_PREFIXES.get(raw[0])
            if segment and f'{segment}:' not in text:
                bases.append(f'{segment} {text}')
            wide, value = wide_immediate(text)
            if wide:
                bases.append(wide)
                if segment and f'{segment}:' not in text:
                    bases.append(f'{segment} {wide}')
            return [p + b for b in bases for p in ('',) + ENCODING_VARIANTS]
        candidates = {key: spellings(key) for key in pending}
        round_number = 0
        while candidates:
            batch = [(key, texts[0]) for key, texts in candidates.items()]
            results = self.assemble_isolated(work, [t for _, t in batch])
            next_candidates = {}
            for (key, spelled), produced in zip(batch, results):
                if produced == key[1]:
                    chosen[key] = spelled
                elif len(candidates[key]) > 1:
                    next_candidates[key] = candidates[key][1:]
            log.append(f'encoding round {round_number}: {len(batch)} spellings, '
                       f'{sum(1 for k, _ in batch if k in chosen)} exact')
            candidates = next_candidates
            round_number += 1
        exact = unmatched = 0
        holdouts = []
        for key, indices in pending.items():
            for index in indices:
                offset, size, kind, text, comment = self.instructions[index]
                if key in chosen:
                    self.instructions[index] = (offset, size, 'insn', chosen[key], comment)
                    if 'bfv_wide_' in chosen[key]:
                        self.wide_values.add(wide_immediate(text)[1])
                    exact += 1
                else:
                    self.instructions[index] = (offset, size, 'bytes', text, comment)
                    holdouts.append(f'{self.va + offset:08x}\t{key[1].hex()}\t{text}\n')
                    unmatched += 1
        # the instructions no spelling reproduces, for review (holdouts.tsv)
        (work / 'holdouts.tsv').write_text('va\tbytes\ttext\n' + ''.join(sorted(holdouts)))
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
            for sign, digits in sorted({m for i in live for m in WIDE_SYMBOL.findall(texts[i])}):
                lines.append(f'.set bfv_wide_{sign}{digits}, {"-" if sign else ""}0x{digits}')
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
        guard, source, via = self.detours[entry]
        if via is not None:
            return b'\xeb' + struct.pack('<b', via - (entry + 2)), source
        target = self.symbols[source]
        return b'\xe9' + struct.pack('<i', target - (entry + 5)) + b'\x90' * (len(guard) - 5), source

    def emit(self, path):
        lines = ['.intel_syntax noprefix', '.text',
                 f'/* .text of the original image, virtual address 0x{self.va:08x} */',
                 '__text_base:']
        self.line_map = {}
        self.masked = set()
        defined = set()
        fallbacks = []  # instructions whose address field the text could not carry
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
            if address in self.edits:  # after the label: branches to it reach the edit
                mode, assembly = self.edits[address]
                lines.append(f'bfv_edit_{address:08x}_start:')
                lines.extend(f'  {line.strip()}' for line in assembly.split('\n') if line.strip())
                lines.append(f'bfv_edit_{address:08x}_end:')
                if mode == 'replace':
                    lines.append(f'  /* replaced: {text if isinstance(text, str) else raw.hex()} */')
                    continue
            if kind == 'detour' and size < 5:
                _, source = self.detour_bytes(offset)
                via = self.detours[address][2]
                # the short jmp's displacement from the trampoline's label, so it moves with the code
                lines.append(f'  .byte 0xeb /* jmp short to the trampoline at 0x{via:08x}, then to {source}; '
                             f'original {raw.hex()} */')
                lines.append(f'  .byte {self.symbol(via)} - . - 1')
            elif kind == 'detour':
                _, source = self.detour_bytes(offset)
                lines.append(f'  {{disp32}} jmp {source} /* detour to reconstructed C; '
                             f'original {raw.hex()} */')
                if self.symbolic:
                    self.masked.update(range(offset + 1, offset + 5))
                if size > 5:
                    lines.append('  .byte ' + ','.join(['0x90'] * (size - 5)))
            elif kind == 'branch':
                mnemonic, target, form = text
                prefix = '{disp32} ' if form == 'near' and mnemonic != 'call' else ''
                lines.append(f'  {prefix}{mnemonic} {self.far_targets.get(target) or self.targets[target]}')
            elif kind == 'table' and text == 'jump':
                for entry in range(0, size, 4):
                    target = struct.unpack_from('<I', raw, entry)[0]
                    lines.append(f'  .long {self.symbol(target)}')
                    self.masked.update(range(offset + entry, offset + entry + 4))
            elif kind == 'table':
                lines.append('  .byte ' + ','.join(f'0x{b:02x}' for b in raw) + ' /* switch index table */')
            elif kind == 'insn' and index in self.item_refs and \
                    (symbolic := self.symbolic_text(text, raw, self.item_refs[index])) is not None:
                lines.append(f'  {symbolic}')
                for field, _, _ in self.item_refs[index]:
                    self.masked.update(range(offset + field, offset + field + 4))
            elif index in self.item_refs:
                # bytes with the address fields as symbols: exact by construction
                if kind == 'insn':
                    fallbacks.append(f'{address:08x}\t{raw.hex()}\t{text}\n')
                parts, at = [], 0
                for field, _, target in sorted(self.item_refs[index]):
                    if field > at:
                        parts.append('.byte ' + ','.join(f'0x{b:02x}' for b in raw[at:field]))
                    parts.append(f'.long {self.symbol(target)}')
                    self.masked.update(range(offset + field, offset + field + 4))
                    at = field + 4
                if at < size:
                    parts.append('.byte ' + ','.join(f'0x{b:02x}' for b in raw[at:]))
                label = text if isinstance(text, str) and text else comment
                lines.append('  ' + '; '.join(parts) + (f' /* {label} */' if label else ''))
            elif kind == 'insn':
                lines.append(f'  {text}')
            else:
                note = f' /* {text} */' if text else f' /* {comment} */' if comment else ''
                lines.append('  .byte ' + ','.join(f'0x{b:02x}' for b in raw) + note)
            self.line_map[len(lines)] = index
        lines.append('')
        lines.append('/* Branch targets that fall inside another instruction or outside decoded code,')
        lines.append('   as offsets from the closest label before them, so they move with their code. */')
        anchors = sorted((a, n) for a, n in boundary_labels.items() if n in defined)
        anchor_addresses = [a for a, _ in anchors]
        for target, name in sorted(self.targets.items()):
            if name not in defined:
                at = bisect.bisect_right(anchor_addresses, target) - 1
                if at >= 0 and target < self.va + len(self.data):
                    lines.append(f'.set {name}, {anchors[at][1]} + 0x{target - anchors[at][0]:x}')
                else:
                    lines.append(f'.set {name}, __text_base + 0x{target - self.va:x}')
        for source in sorted({s for _, _, k, s, _ in self.instructions if k == 'detour'}):
            if self.c_linked:  # the C payload's function, linked with the image
                lines.append(f'.set {source}, "{self.decorated[source]}"')
                continue
            if self.symbolic:  # the reconstructed C sits at a fixed address
                lines.append(f'.set {source}, 0x{self.symbols[source]:x}')
            else:
                lines.append(f'.set {source}, __text_base + 0x{self.symbols[source] - self.va:x}')
        for name, expression in self.extra_sets:
            lines.append(f'.globl {name}')
            lines.append(f'.set {name}, {expression}')
        if self.wide_values:
            lines.append('/* Immediates the original encodes at full width (imm16/imm32 where an imm8')
            lines.append('   would do), defined after their use so GNU as keeps that form. */')
            for value in sorted(self.wide_values):
                lines.append(f'.set {wide_name(value)}, {value:#x}' if value >= 0 else
                             f'.set {wide_name(value)}, -{-value:#x}')
        for name in sorted(self.exported):
            lines.append(f'.globl {name}')
        path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
        if self.symbolic:
            (path.parent / 'symbolic-bytes.tsv').write_text('va\tbytes\ttext\n' + ''.join(fallbacks))

    def gate_items(self, edit_sizes):
        """items.tsv for tools/textcheck: each item of the original .text and
        each edit ({address: new size}), before the item at its address."""
        lines = []
        for offset, size, kind, text, comment in self.instructions:
            address = self.va + offset
            if address in self.edits:
                old = size if self.edits[address][0] == 'replace' else 0
                lines.append(f'{offset:x} {old} edit {edit_sizes[address]}\n')
            if kind in ('insn', 'branch', 'detour'):
                group = 'code'
            elif kind == 'table':
                group = 'jt' if text == 'jump' else 'data'
            else:  # bytes: a held instruction keeps its text, other bytes have none
                group = 'hold' if isinstance(text, str) and text else 'data'
            lines.append(f'{offset:x} {size} {group}\n')
        return ''.join(lines)

    def symbolic_text(self, text, raw, fields):
        """The instruction text with its address fields as symbols, or None when a
        field's number is not unambiguously in the text (then bytes are used)."""
        prefix = ''
        while text.startswith('{'):
            cut = text.index('} ') + 2
            prefix, text = prefix + text[:cut], text[cut:]
        head, _, operands = text.partition(' ')
        out = operands
        for field, kind, target in fields:
            value = struct.unpack_from('<I', raw, field)[0]
            token = f'0x{value:x}'
            spots = [m.start() for m in re.finditer(re.escape(token) + r'(?![0-9a-fx])', out)]
            inside = [s for s in spots if out.count('[', 0, s) > out.count(']', 0, s)]
            outside = [s for s in spots if s not in inside]
            chosen = inside if kind == 'disp' else outside
            if len(chosen) != 1 or (kind == 'disp' and out[chosen[0] - 2:chosen[0]] == '- '):
                return None
            expression = self.symbol(target)
            replacement = expression if kind == 'disp' else f'offset {expression}'
            out = out[:chosen[0]] + replacement + out[chosen[0] + len(token):]
        return f'{prefix}{head} {out}'

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
            if self.masked and len(got) == len(expected):
                # symbolic fields hold relocation addends until linked; the link is gated separately
                got = bytearray(got)
                for offset in self.masked:
                    got[offset] = expected[offset]
                got = bytes(got)
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


def emit_data_section(data, va, path, refs, words=None, labels=(), aliases=None, items=None, names=None):
    """Byte stream with .long symbol references at patched vtable slots.
    Symbolic mode adds `words` (address -> `.long SYM` / `.rva SYM` directive)
    and defines `labels` (d_XXXXXXXX names other sections refer to, plus the
    `names` some of them also have) and `aliases` (address -> a global name
    for it, the --link-c a_ symbols). `items` ({address: (size, directives)})
    write strings and small tables as what they are (import_source)."""
    lines = ['.intel_syntax noprefix', f'.section .{path.stem}', f'__{path.stem}_base:']
    refs = {rva - va: (f'.long {address}' if isinstance(address, str) else f'.long 0x{address:08x}',
                       'vtable slot', 4) for rva, address in refs.items()}
    for where, directive in (words or {}).items():
        refs.setdefault(where - va, (directive, '', 4))
    fields = sorted(refs)
    for where, (size, directives) in (items or {}).items():
        at = bisect.bisect_left(fields, where - va - 3)
        if at < len(fields) and fields[at] < where - va + size:
            raise RuntimeError(f'{path.name}: item at {where:08x} overlaps a reference field')
        refs[where - va] = ('\n  '.join(directives), '', size)
    offset = 0
    chunk = 32
    while offset < len(data):
        if offset in refs:
            directive, note, size = refs[offset]
            if note:
                lines.append(f'L{va + offset:08x}:')
            lines.append(f'  {directive}' + (f' /* {note} */' if note else ''))
            offset += size
            continue
        take = min(chunk, len(data) - offset)
        next_ref = min((r for r in refs if r > offset), default=len(data))
        take = min(take, next_ref - offset)
        blob = data[offset:offset + take]
        lines.append('  .byte ' + ','.join(f'0x{b:02x}' for b in blob))
        offset += take
    for address in sorted(labels):
        for label in (f'd_{address:08x}', *([names[address]] if address in (names or {}) else [])):
            lines.append(f'.globl {label}')
            lines.append(f'.set {label}, __{path.stem}_base + 0x{address - va:x}')
    for address, name in sorted((aliases or {}).items()):
        lines.append(f'.globl {name}')
        lines.append(f'.set {name}, __{path.stem}_base + 0x{address - va:x}')
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8')


def symscan_results(target, game_dir):
    """Run tools/symscan on the original image: switch tables in .text and the
    absolute references it decided are addresses (text fields and data words)."""
    tool = PROJECT / 'build' / 'symscan' / 'symscan.exe'
    if not tool.exists():
        raise RuntimeError(f'{tool} missing; build it with tools/symscan/build.ps1')
    prefix = PROJECT / 'build' / 'symscan' / target
    run([tool, game_dir / TARGETS[target]['file'],
         PROJECT.parent / 'reports' / target / 'all-functions.tsv', prefix], PROJECT)
    tables = {}
    for row in (prefix.parent / f'{target}-tables.tsv').read_text().splitlines()[1:]:
        start, end, kind, _ = row.split('\t')
        tables[int(start, 16)] = (int(end, 16), kind)
    text_refs, data_refs = {}, {}
    for row in (prefix.parent / f'{target}-refs.tsv').read_text().splitlines()[1:]:
        where, kind, value, _, _, decision = row.split('\t')[:6]
        if decision != 'address':
            continue
        if kind in ('disp', 'imm'):
            text_refs[int(where, 16)] = (kind, int(value, 16))
        elif kind == 'word':
            data_refs[int(where, 16)] = int(value, 16)
    return tables, text_refs, data_refs


DIRECTORIES = {0: 'export', 1: 'import', 2: 'resource', 9: 'tls', 12: 'iat'}


def pe_structure(pe):
    """The address fields of the PE directories, parsed from their structure:
    {field VA: ('rva' | 'va', target VA)}, and each directory's span (VA, VA)."""
    base = pe.OPTIONAL_HEADER.ImageBase
    word = lambda rva: pe.get_dword_at_rva(rva)
    fields, spans = {}, {}
    for index, name in DIRECTORIES.items():
        entry = pe.OPTIONAL_HEADER.DATA_DIRECTORY[index]
        if entry.VirtualAddress and entry.Size:
            spans[name] = (base + entry.VirtualAddress, base + entry.VirtualAddress + entry.Size)

    def rva_field(rva):
        if word(rva):
            fields[base + rva] = ('rva', base + word(rva))

    def thunks(rva):
        while rva and word(rva):
            if not word(rva) & 0x80000000:  # by name: RVA of the hint/name entry
                rva_field(rva)
            rva += 4
    if 'import' in spans:
        rva = spans['import'][0] - base
        while any(word(rva + k) for k in (0, 12, 16)):
            for k in (0, 12, 16):  # OriginalFirstThunk, Name, FirstThunk
                rva_field(rva + k)
            thunks(word(rva))
            thunks(word(rva + 16))
            rva += 20
    if 'export' in spans:
        rva = spans['export'][0] - base
        for k in (12, 28, 32, 36):  # Name, AddressOfFunctions, AddressOfNames, AddressOfNameOrdinals
            rva_field(rva + k)
        for i in range(word(rva + 20)):
            rva_field(word(rva + 28) + 4 * i)
        for i in range(word(rva + 24)):
            rva_field(word(rva + 32) + 4 * i)
    if 'tls' in spans:
        rva = spans['tls'][0] - base
        for k in (0, 4, 8, 12):  # raw data start/end, AddressOfIndex, AddressOfCallBacks
            if word(rva + k):
                fields[base + rva + k] = ('va', word(rva + k))
        callback = word(rva + 12) - base if word(rva + 12) else 0
        while callback and word(callback):
            fields[base + callback] = ('va', word(callback))
            callback += 4
    if 'resource' in spans:
        root = spans['resource'][0] - base

        def directory(rva):
            count = pe.get_word_at_rva(rva + 12) + pe.get_word_at_rva(rva + 14)
            for i in range(count):
                target = word(rva + 16 + 8 * i + 4)
                if target & 0x80000000:  # subdirectory, relative to the resource root
                    directory(root + (target & 0x7fffffff))
                else:  # data entry: OffsetToData is an RVA
                    rva_field(root + target)
        directory(root)
    return fields, spans


def asm_string(raw):
    """GAS string literal for bytes (printable ASCII kept, the rest escaped)."""
    return '"' + ''.join(chr(b) if 0x20 <= b < 0x7f and b not in b'"\\' else f'\\{b:03o}' for b in raw) + '"'


def import_source(pe):
    """The import and export tables written as what they are rather than bytes.

    Returns (names, notes, items): `names` gives each IAT slot a symbol made
    from its table entry (imp_<DLL>_<function>, imp_<DLL>_ord<N> by ordinal),
    used wherever code or data refers to the slot; `notes` comments each
    descriptor field and thunk with its DLL and function; `items` are the
    strings and small tables at their addresses, {VA: (size, [directives])}:
    hint/name entries, DLL names, the export name, its names and ordinals."""
    base = pe.OPTIONAL_HEADER.ImageBase
    pe.parse_data_directories(directories=[0, 1])
    names, notes, items = {}, {}, {}

    def string(rva, prefix=()):  # a name at rva, after `prefix` (16-bit words)
        raw = pe.get_string_at_rva(rva + 2 * len(prefix))
        items[base + rva] = (len(prefix) * 2 + len(raw) + 1, [*prefix, f'.asciz {asm_string(raw)}'])
        return raw.decode('ascii')
    for entry in getattr(pe, 'DIRECTORY_ENTRY_IMPORT', ()):
        dll = string(entry.struct.Name)
        stem = re.sub(r'\W', '_', dll.rsplit('.', 1)[0])
        descriptor = base + pe.get_rva_from_offset(entry.struct.get_file_offset())
        for k, field in ((0, 'OriginalFirstThunk'), (12, 'Name'), (16, 'FirstThunk')):
            notes[descriptor + k] = f'{dll} {field}'
        for imported in entry.imports:
            # by ordinal, pefile still names the well-known ones (WSOCK32, OLEAUT32)
            function = imported.name.decode('ascii') if imported.name else None
            label = f'imp_{stem}_{function}' if function else f'imp_{stem}_ord{imported.ordinal}'
            names[imported.address] = label
            what = f'{dll} {function or ""}'.rstrip()
            if imported.import_by_ordinal:
                what += f' ordinal {imported.ordinal}'
                function = None
            thunks = [imported.address]
            if entry.struct.OriginalFirstThunk:
                thunks.append(base + entry.struct.OriginalFirstThunk + imported.address - base - entry.struct.FirstThunk)
            for thunk in thunks:
                notes[thunk] = what
                if not function:
                    items[thunk] = (4, [f'.long 0x80000000 | {imported.ordinal}  /* {what} */'])
            if function:
                string(pe.get_dword_at_rva(thunks[-1] - base), (f'.short {imported.hint}',))
    export = getattr(pe, 'DIRECTORY_ENTRY_EXPORT', None)
    if export:
        x = export.struct
        string(x.Name)
        for i in range(x.NumberOfNames):
            string(pe.get_dword_at_rva(x.AddressOfNames + 4 * i))
        ordinals = [pe.get_word_at_rva(x.AddressOfNameOrdinals + 2 * i) for i in range(x.NumberOfNames)]
        if ordinals:
            items[base + x.AddressOfNameOrdinals] = (2 * len(ordinals), ['.short ' + ', '.join(map(str, ordinals))])
    return names, notes, items


def rich_entries(original, lfanew):
    """MSVC's Rich header between the DOS stub and the PE header: its offset,
    the decoded (comp.id, count) entries, the trailing zero bytes and the key.
    The key is a checksum of the DOS header, stub and entries; it is
    recomputed here and must equal the stored one."""
    end = original.find(b'Rich', 0x40, lfanew)
    if end < 0:
        return None
    key = struct.unpack_from('<I', original, end + 4)[0]
    start = next(o for o in range(0x40, end, 4)
                 if struct.unpack_from('<I', original, o)[0] ^ key == 0x536e6144)  # 'DanS'
    words = [struct.unpack_from('<I', original, o)[0] ^ key for o in range(start + 16, end, 4)]
    entries = list(zip(words[0::2], words[1::2]))
    rol = lambda v, n: ((v << n) | (v >> (32 - n))) & 0xffffffff if n else v
    checksum = start
    for i in range(start):
        if not 0x3c <= i < 0x40:
            checksum += rol(original[i], i % 32)
    for compid, count in entries:
        checksum += rol(compid, count % 32)
    if checksum & 0xffffffff != key:
        raise RuntimeError('Rich header key is not the checksum of its entries')
    return start, entries, lfanew - (end + 8), key


def structure_lines(structure, overrides=None):
    """A pefile structure as directives, one named field per line; `overrides`
    gives fields new values ({name: (value, note)})."""
    lines = []
    for kind, keys in zip(re.findall(r'\d*[A-Za-z]', structure.__format_str__[1:]), structure.__keys__):
        name = keys[-1] if keys[0] == 'Misc' else keys[0]
        value, note = (overrides or {}).get(name, (getattr(structure, name), ''))
        comment = f'  /* {name}{": " + note if note else ""} */'
        if kind.endswith('s'):
            size = int(kind[:-1])
            raw = value if isinstance(value, bytes) else bytes(value)
            text = raw.rstrip(b'\0')
            parts = ([f'.ascii {asm_string(text)}'] if text else []) + \
                ([f'.fill {size - len(text)}, 1, 0'] if len(text) < size else [])
            lines.append('  ' + '; '.join(parts) + comment)
        else:
            directive = {'B': '.byte', 'H': '.short', 'I': '.long'}[kind]
            lines.append(f'  {directive} 0x{value:x}{comment}')
    return lines


DOS_STUB = ['.code16', 'push cs', 'pop ds', 'mov dx, offset dos_message - dos_stub', 'mov ah, 9', 'int 0x21',
            'mov ax, 0x4c01', 'int 0x21', '.code32']


def header_source(pe, original, path, sections, layout, directories):
    """The PE headers as assembly (DOS header and stub, Rich header, file and
    optional headers, data directories, section table), assembled into the
    image's first SizeOfHeaders bytes. Fixed fields keep the original's
    values by name; `layout` overrides the optional-header fields the link
    decides, `directories` the data directory entries, and `sections` gives
    the section table: (name, virtual size, RVA, raw size, file pointer,
    characteristics)."""
    optional = pe.OPTIONAL_HEADER
    lfanew = pe.DOS_HEADER.e_lfanew
    lines = ['.intel_syntax noprefix', '.section .hdr', 'dos_header:']
    lines += structure_lines(pe.DOS_HEADER)[:-1]
    lines.append('  .long nt_headers - dos_header  /* e_lfanew */')
    lines += ['dos_stub:  /* prints the message and exits when run under DOS */',
              *(f'  {line}' if not line.startswith('.') else line for line in DOS_STUB)]
    message_at = original.find(b'This program', 0x40, lfanew)
    message = original[message_at:original.index(b'$', message_at) + 1]
    lines += ['dos_message:', f'  .ascii {asm_string(message)}', '  .balign 8, 0']
    rich = rich_entries(original, lfanew)
    if rich:
        start, entries, tail, key = rich
        lines += [f'  .org 0x{start:x}',
                  '/* Rich header: the MSVC tools that built the image, (product << 16 | build, count),',
                  '   XOR-masked with a key that checksums the DOS header, stub and entries */',
                  f'  .set rich_key, 0x{key:08x}',
                  '  .long 0x536e6144 ^ rich_key, rich_key, rich_key, rich_key  /* DanS */']
        lines += [f'  .long 0x{compid:08x} ^ rich_key, {count} ^ rich_key  /* product {compid >> 16}, build {compid & 0xffff} */'
                  for compid, count in entries]
        lines += ['  .ascii "Rich"', '  .long rich_key', f'  .fill {tail}, 1, 0']
    lines += ['  .balign 8, 0', 'nt_headers:', '  .ascii "PE\\0\\0"']
    lines += structure_lines(pe.FILE_HEADER, {'NumberOfSections': (len(sections), 'section table below')})
    lines += structure_lines(optional, layout)
    for index, entry in enumerate(optional.DATA_DIRECTORY):
        rva, size = directories.get(index, (entry.VirtualAddress, entry.Size))
        lines.append(f'  .long 0x{rva:x}, 0x{size:x}  /* {entry.name} */')
    lines.append('section_table:')
    for name, virtual_size, rva, raw, pointer, characteristics in sections:
        lines.append(f'  .ascii {asm_string(name.encode())}; .fill {8 - len(name)}, 1, 0'
                     f'  /* {name}: VirtualSize, VirtualAddress, SizeOfRawData, PointerToRawData */')
        lines.append(f'  .long 0x{virtual_size:x}, 0x{rva:x}, 0x{raw:x}, 0x{pointer:x}')
        lines.append(f'  .long 0, 0; .short 0, 0; .long 0x{characteristics:08x}  /* no relocations or line numbers; Characteristics */')
    lines.append(f'  .org 0x{optional.SizeOfHeaders:x}  /* SizeOfHeaders */')
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8')
    run([COMPILERS / 'as.exe', '--32', '--no-pad-sections', '-o', str(path.with_suffix('.o')), str(path)], path.parent)
    header = bytearray(object_bytes(path.with_suffix('.o'), '.hdr'))
    if len(header) != optional.SizeOfHeaders:
        raise RuntimeError(f'{path.name}: {len(header)} bytes, SizeOfHeaders is {optional.SizeOfHeaders}')
    return header


def link_sections(work, pe, wanted, layout=None, payload=None):
    """Link the section objects with GNU ld, each section at `layout[name]` (an
    RVA; default: the original address), and return {name: (RVA, bytes)} and
    the values of the `wanted` symbols.

    `payload` = (objects, address, libraries) also links the reconstructed C
    into `.bfvmod` at `address`, laid out like build.py's payload.ld. The
    section objects define its a_XXXXXXXX symbols and refer to its functions
    by their decorated names."""
    base = pe.OPTIONAL_HEADER.ImageBase
    names = [s.Name.rstrip(b'\0').decode() for s in pe.sections]
    layout = layout or {n: s.VirtualAddress for n, s in zip(names, pe.sections)}
    script = ['SECTIONS', '{']
    for name in names:
        script.append(f'  {name} 0x{base + layout[name]:x} : {{ {name[1:]}.o({name}) }}')
    inputs, options = [f'{n[1:]}.o' for n in names], []
    if payload:
        objects, address, libraries = payload
        script += [f'  .bfvmod 0x{address:x} : {{ *(.text*) *(.rdata*) *(.data*) *(.bss*) BYTE(0) }}',
                   '  .bfvshadow ALIGN(4096) (NOLOAD) : { *(.bfvshadow) }']
        inputs += [build.response(work / 'payload-objects.rsp', objects), *libraries]
        options = ['--disable-dynamicbase', '--disable-reloc-section']
        names = names + ['.bfvmod']
        layout = dict(layout, **{'.bfvmod': address - base})
    script += ['  /DISCARD/ : { *(*) }', '}']
    (work / 'link.ld').write_text('\n'.join(script) + '\n')
    run([COMPILERS / 'ld.exe', '-m', 'i386pe', '--image-base', f'0x{base:x}', '-e', '0', *options,
         '-T', 'link.ld', '-o', 'linked.exe'] + inputs, work)
    linked = pefile.PE(str(work / 'linked.exe'), fast_load=True)
    found = {s.Name.rstrip(b'\0').decode(): s for s in linked.sections}
    sections = {}
    for name in names:
        if found[name].VirtualAddress != layout[name]:
            raise RuntimeError(f'{name}: linked at RVA {found[name].VirtualAddress:x}')
        sections[name] = (layout[name], found[name].get_data()[:found[name].Misc_VirtualSize])
    linked.close()
    values = {}
    for line in run([COMPILERS / 'nm.exe', 'linked.exe'], work).splitlines():
        parts = line.split()
        if len(parts) == 3 and parts[2] in wanted:
            values[parts[2]] = int(parts[0], 16)
    missing = set(wanted) - set(values)
    if missing:
        raise RuntimeError(f'linked image lacks {sorted(missing)[:5]}')
    return sections, values


def compose_image(pe, original, sections, values, spans, entry_symbol, work, appended=None):
    """The PE image from the linked sections, its headers assembled from
    headers.s (header_source). Fixed fields keep the original's values by
    name; every layout field comes from the link: section table, directories,
    entry point and image size. SizeOfCode and SizeOfInitializedData move with
    their sections' file-aligned sizes (MSVC counted .text alone as code, and
    .data at its virtual size). `appended` = (name, virtual size, RVA, raw
    size, characteristics) adds a section after the image (the C payload);
    its bytes follow the returned image. CheckSum stays 0: it covers the
    finished file."""
    base = pe.OPTIONAL_HEADER.ImageBase
    file_align = pe.OPTIONAL_HEADER.FileAlignment
    optional = pe.OPTIONAL_HEADER
    body = bytearray()
    pointer = optional.SizeOfHeaders
    code_delta = data_delta = 0
    end = 0
    table = []
    layout = {}
    for section in pe.sections:
        name = section.Name.rstrip(b'\0').decode()
        rva, blob = sections[name]
        raw = align(len(blob), file_align)
        growth = section_growth(section, len(blob), file_align)
        if growth == 0 or name == '.text':  # .text: its raw padding is part of the blob
            virtual_size = section.Misc_VirtualSize + growth
        else:
            virtual_size = max(section.Misc_VirtualSize + growth, len(blob))
        table.append((name, virtual_size, rva, raw, pointer, section.Characteristics))
        body += blob + b'\0' * (raw - len(blob))
        pointer += raw
        end = max(end, rva + virtual_size)
        if name == '.text':
            code_delta += raw - section.SizeOfRawData
        elif section.Characteristics & 0x40:
            data_delta += align(virtual_size, file_align) - align(section.Misc_VirtualSize, file_align)
        if section.VirtualAddress == optional.BaseOfCode:
            layout['BaseOfCode'] = (rva, f'{name}, from the link')
        if section.VirtualAddress == optional.BaseOfData:
            layout['BaseOfData'] = (rva, f'{name}, from the link')
    if appended:
        name, virtual_size, rva, raw, characteristics = appended
        if rva < align(end, optional.SectionAlignment):
            raise RuntimeError(f'the linked image runs into {name} at RVA {rva:x}')
        table.append((name, virtual_size, rva, raw, pointer, characteristics))
        end = max(end, rva + virtual_size)
    layout.update(AddressOfEntryPoint=(values[entry_symbol] - base, f'{entry_symbol}, from the link'),
                  SizeOfImage=(align(end, optional.SectionAlignment), 'from the section table'),
                  SizeOfCode=(optional.SizeOfCode + code_delta, '.text, file-aligned'),
                  SizeOfInitializedData=(optional.SizeOfInitializedData + data_delta, 'data sections, file-aligned'),
                  CheckSum=(0, 'written once the file is complete'))
    directories = {}
    for index, name in DIRECTORIES.items():
        if name in spans:
            start, stop = (values[f'd_{a:08x}'] for a in spans[name])
            directories[index] = (start - base, stop - start)
    header = header_source(pe, original, work / 'headers.s', table, layout, directories)
    return header + body


def c_link_inputs(target, symbols):
    """Inputs for linking the reconstructed C with the image (tools/linkmode.py
    must have run): the link-mode objects, the a_ addresses and the C symbol
    name (with its decoration) of every manifest symbol."""
    lm = PROJECT / 'build' / 'linkmode' / target
    for need in ('objects.rsp', 'addresses.txt', 'payload.exe'):
        if not (lm / need).exists():
            raise RuntimeError(f'{lm / need} missing; run tools/linkmode.py --target {target}')
    objects = [Path(line.strip().strip('"')) for line in (lm / 'objects.rsp').read_text().splitlines() if line.strip()]
    addresses = [int(x, 16) for x in (lm / 'addresses.txt').read_text().split()]
    decorated = {}
    for line in run([COMPILERS / 'nm.exe', lm / 'payload.exe'], lm).splitlines():
        parts = line.split()
        match = len(parts) == 3 and re.fullmatch(r'[_@]?(bfv_[A-Za-z0-9_]+)(?:@\d+)?', parts[2])
        # not at the manifest's address: the link-mode payload is its own compile
        if match and match[1] in symbols:
            decorated[match[1]] = parts[2]
    libraries = []
    if any(o.parent.name == 'mods' for o in objects):
        libraries = [run([COMPILERS / 'gcc.exe', '-print-libgcc-file-name'], lm).strip()]
    return objects, addresses, decorated, libraries


def section_growth(section, length, file_align):
    """How far an assembled section of `length` bytes reaches past the
    original. .text is emitted whole, raw padding included, so its growth is
    exact (an edit of a few bytes); the other sections count in file-aligned
    raw sizes."""
    if section.Name.rstrip(b'\0') == b'.text':
        return length - section.SizeOfRawData
    return align(length, file_align) - section.SizeOfRawData


def sequential_layout(pe, outputs):
    """Section RVAs for the assembled sizes: each section at its original
    address, or right after the previous one when that has grown into it."""
    file_align, section_align = pe.OPTIONAL_HEADER.FileAlignment, pe.OPTIONAL_HEADER.SectionAlignment
    layout, end = {}, 0
    for section in pe.sections:
        name = section.Name.rstrip(b'\0').decode()
        rva = max(section.VirtualAddress, align(end, section_align))
        growth = section_growth(section, len(outputs[name]), file_align)
        layout[name] = rva
        end = rva + section.Misc_VirtualSize + growth
    return layout


def grow_gate(work, pe, baseline, output, grow, at, log, edit_items=None):
    """--grow-text against the same build without it: .text gains `grow`
    int3 bytes at offset `at` (its end when None), every byte after them
    moves up by `grow`, every section keeps its bytes except fields that point
    at code or data that moved, which moved with it (relocheck --ranges), the
    entry point and exports move the same way, and the import, resource and
    export directories still parse the same.

    With edit_items (--insert/--replace: TextEmitter.gate_items) .text is
    checked instruction by instruction instead (tools/textcheck): every
    original item is still there, moved, with its branches and address fields
    moved with their targets, plus the edits. Its moved pieces then drive the
    check of the other sections.

    Only differing bytes are checked: a field that should have moved but did
    not is not caught here. The symbolic shift relink is the evidence that
    every address field is a symbol, so ld moves all of them."""
    base = pe.OPTIONAL_HEADER.ImageBase
    old_pe, new_pe = pefile.PE(data=bytes(baseline)), pefile.PE(data=bytes(output))
    new_sections = {s.Name: s for s in new_pe.sections}
    spans, moved = [], []  # (start rva, end rva, delta)
    for section in old_pe.sections:  # the image's sections and .bfvmod with its shadow stacks
        start = section.VirtualAddress
        end = start + align(section.Misc_VirtualSize, pe.OPTIONAL_HEADER.SectionAlignment)
        delta = new_sections[section.Name].VirtualAddress - section.VirtualAddress
        if section.Name.rstrip(b'\0') == b'.text' and edit_items is not None:
            pass  # in pieces, from textcheck
        elif section.Name.rstrip(b'\0') == b'.text' and at is not None:
            spans += [(start, start + at, delta), (start + at, end, delta + grow)]
            moved.append(f'.text from +0x{at:x} +0x{grow:x}')
        else:
            spans.append((start, end, delta))
        if delta:
            moved.append(f'{section.Name.rstrip(bytes(1)).decode()} +0x{delta:x}')
    ok = True
    if edit_items is not None:
        others = ''.join(f'{base + s:x} {base + e:x} {d & 0xffffffff:x}\n' for s, e, d in spans)
        (work / 'ranges-other.tsv').write_text(others)
        (work / 'items.tsv').write_text(edit_items)
        text_old = next(s for s in old_pe.sections if s.Name.rstrip(b'\0') == b'.text')
        text_new = new_sections[text_old.Name]
        a, b = work / 'gate-old.bin', work / 'gate-new.bin'
        a.write_bytes(text_old.get_data()[:text_old.SizeOfRawData])
        b.write_bytes(text_new.get_data()[:text_new.SizeOfRawData])
        check = subprocess.run([PROJECT / 'build/textcheck/textcheck.exe', a, f'{base + text_old.VirtualAddress:x}',
                                b, f'{base + text_new.VirtualAddress:x}', work / 'items.tsv',
                                work / 'ranges-other.tsv', f'{base:x}', work / 'text-ranges.tsv'],
                               capture_output=True, text=True)
        log.extend(f'textcheck: {line}' for line in (check.stdout + check.stderr).strip().splitlines())
        ok &= check.returncode == 0
        if check.returncode == 0:  # the RVA lines of the moved .text pieces
            rows = [tuple(int(v, 16) for v in line.split())
                    for line in (work / 'text-ranges.tsv').read_text().split('\n') if line]
            spans += [(s, e, d) for s, e, d in rows if s < base]
        moved.append('.text in pieces (textcheck)')
    ranges = []
    for start, end, delta in spans:  # absolute (VA) and RVA fields
        ranges.append(f'{base + start:x} {base + end:x} {delta & 0xffffffff:x}\n')
        ranges.append(f'{start:x} {end:x} {delta & 0xffffffff:x}\n')
    (work / 'ranges.tsv').write_text(''.join(ranges))
    log.append(f'grow: moved: {", ".join(moved) or "none"}')

    def moves(rva):
        return next(((rva + delta) & 0xffffffff for start, end, delta in spans if start <= rva < end), None)

    for section in old_pe.sections:
        name = section.Name.rstrip(b'\0').decode()
        mine = new_sections[section.Name]
        old = section.get_data()[:section.SizeOfRawData]
        new = mine.get_data()[:mine.SizeOfRawData]
        va_a, va_b = base + section.VirtualAddress, base + mine.VirtualAddress
        pieces = [(name, old, new[:len(old)], va_a, va_b)]
        if name == '.text' and edit_items is not None:
            continue  # textcheck above
        if name == '.text':  # the int3 bytes at `at`; the original's raw data around them
            cut = len(old) if at is None else at
            if new[cut:cut + grow] != b'\xcc' * grow or any(new[len(old) + grow:]) \
                    or mine.Misc_VirtualSize < section.Misc_VirtualSize + grow:
                log.append(f'grow .text: the {grow} int3 bytes are not at +0x{cut:x}')
                ok = False
            pieces = [('.text head', old[:cut], new[:cut], va_a, va_b),
                      ('.text tail', old[cut:], new[cut + grow:len(old) + grow], va_a + cut, va_b + cut + grow)]
        for label, x, y, a_at, b_at in pieces:
            if not x:
                continue
            a, b = work / 'gate-old.bin', work / 'gate-new.bin'
            a.write_bytes(x)
            b.write_bytes(y)
            check = subprocess.run([PROJECT / 'build/linkmode/relocheck.exe', '--ranges', a, b,
                                    f'{a_at:x}', f'{b_at:x}', work / 'ranges.tsv'],
                                   capture_output=True, text=True)
            log.append(f'grow {label}: {check.stdout.strip() or check.stderr.strip()}')
            ok &= check.returncode == 0
    entry = moves(old_pe.OPTIONAL_HEADER.AddressOfEntryPoint) == new_pe.OPTIONAL_HEADER.AddressOfEntryPoint
    log.append(f'grow entry point: {"moved with its code" if entry else "WRONG"}')
    ok &= entry
    if hasattr(old_pe, 'DIRECTORY_ENTRY_EXPORT'):
        exports = [moves(x.address) for x in old_pe.DIRECTORY_ENTRY_EXPORT.symbols] == \
                  [x.address for x in new_pe.DIRECTORY_ENTRY_EXPORT.symbols]
        log.append(f'grow export addresses: {"moved with their code" if exports else "WRONG"}')
        ok &= exports
    for attribute, describe in (('DIRECTORY_ENTRY_IMPORT', lambda e: [(e.dll, [(i.name, i.ordinal) for i in e.imports]) for e in e]),
                                ('DIRECTORY_ENTRY_EXPORT', lambda e: [(x.name, x.ordinal) for x in e.symbols]),
                                ('DIRECTORY_ENTRY_RESOURCE', lambda e: len(e.entries))):
        if hasattr(old_pe, attribute):
            same = describe(getattr(old_pe, attribute)) == describe(getattr(new_pe, attribute, None) or [])
            log.append(f'grow {attribute[16:].lower()} directory: {"same" if same else "DIFFERENT"}')
            ok &= same
    return ok


def link_c_gate(work, pe, installed, output, moved, log):
    """--link-c against the default build (installed): every section of the
    original image may differ only in fields that pointed at a reconstructed-C
    symbol's build.py address and now point at its address in the unified
    link (`moved`), absolute or rel32 (tools/linkmode/relocheck.exe)."""
    (work / 'moved.tsv').write_text(''.join(f'{a:x} {b:x}\n' for a, b in sorted(moved.items())))
    ok = True
    old_pe = pefile.PE(data=bytes(installed), fast_load=True)
    new_sections = {s.Name: s for s in pefile.PE(data=bytes(output), fast_load=True).sections}
    for section in old_pe.sections:
        name = section.Name.rstrip(b'\0').decode()
        if name == '.bfvmod':
            continue
        mine = new_sections[section.Name]
        if mine.VirtualAddress != section.VirtualAddress or mine.SizeOfRawData != section.SizeOfRawData:
            log.append(f'gate {name}: moved or resized')
            ok = False
            continue
        a, b = work / 'gate-old.bin', work / 'gate-new.bin'
        a.write_bytes(section.get_data()[:section.SizeOfRawData])
        b.write_bytes(mine.get_data()[:mine.SizeOfRawData])
        va = old_pe.OPTIONAL_HEADER.ImageBase + section.VirtualAddress
        check = subprocess.run([PROJECT / 'build/linkmode/relocheck.exe', '--map', a, b, f'{va:x}',
                                work / 'moved.tsv'], capture_output=True, text=True)
        log.append(f'gate {name}: {check.stdout.strip() or check.stderr.strip()}')
        ok &= check.returncode == 0
    return ok


def build_recomp(target, game_dir, assemble_only=False, install=False, symbolic=False, link_c=False,
                 grow_text=0, grow_at=None, edits=None):
    spec, original, pe, payload, symbols, manifest = load_target(target, game_dir)
    base = pe.OPTIONAL_HEADER.ImageBase
    if grow_text and edits:
        raise RuntimeError('--grow-text and --insert/--replace are separate tests')
    symbolic = symbolic or link_c or bool(grow_text) or bool(edits)
    changed = bool(grow_text) or bool(edits)
    payload_address = int(manifest['payload_address'], 16)
    mode = 'linked-c' if link_c else 'symbolic' if symbolic else 'recomp'
    baseline = PROJECT / 'build' / f'{target}-{mode}' / TARGETS[target]['output']
    if changed and not baseline.exists():
        raise RuntimeError(f'--grow-text/--insert/--replace compare with {baseline}; build it first without them')
    work = PROJECT / 'build' / f"{target}-{mode}{'-grown' if grow_text else '-edited' if edits else ''}"
    work.mkdir(parents=True, exist_ok=True)
    text_section = next(s for s in pe.sections if s.Name.rstrip(b'\0') == b'.text')
    text_rva = text_section.VirtualAddress
    text = pe.get_data(text_rva, text_section.SizeOfRawData)
    vtables = vtable_map(manifest)

    seeds = collect_code_seeds(pe, manifest, text_rva, text_rva + len(text), target)
    log = []
    words, wanted = {}, {}
    if symbolic and assemble_only:
        raise RuntimeError('--symbolic regenerates its sources; it has no --assemble-only mode')
    if symbolic:
        tables, text_refs, data_refs = symscan_results(target, game_dir)
        spans = [(s.Name.rstrip(b'\0').decode(), base + s.VirtualAddress,
                  base + s.VirtualAddress + max(s.Misc_VirtualSize, s.SizeOfRawData)) for s in pe.sections]
        import_names, import_notes, import_items = import_source(pe)

        def data_symbol(address):
            name = next((n for n, start, end in spans if start <= address < end), None)
            if name is None or name == '.text':
                raise RuntimeError(f'no section symbol for 0x{address:08x}')
            wanted.setdefault(name, set()).add(address)
            return import_names.get(address, f'd_{address:08x}')
        emitter = TextEmitter(text, base + text_rva, seeds, detour_map(manifest), symbols,
                              function_names(pe, manifest, target), tables, text_refs)
        emitter.data_symbol = data_symbol
        emitter.plan()
        pe_fields, pe_spans = pe_structure(pe)
        words = {where: '.long ' + emitter.symbol(value, export=True)
                 for where, value in data_refs.items() if where not in vtables and where not in pe_fields}
        for where, (form, value) in pe_fields.items():
            if base + text_rva <= where < base + text_rva + len(text):
                raise RuntimeError(f'PE directory field in .text at 0x{where:08x}')
            words[where] = f"{'.rva' if form == 'rva' else '.long'} {emitter.symbol(value, export=True)}" + \
                (f'  /* {import_notes[where]} */' if where in import_notes else '')
        for start, end in pe_spans.values():  # directory bounds, read back for the header
            data_symbol(start)
            data_symbol(end)
        for slot in import_names:  # every IAT slot has its name, referred to or not
            data_symbol(slot)
        entry_symbol = emitter.symbol(base + pe.OPTIONAL_HEADER.AddressOfEntryPoint, export=True)
        log.append(f'symbolic: {len(tables)} switch tables, {len(text_refs)} text references, '
                   f'{len(words)} data words, {len(emitter.merged)} items merged around fields')
        if link_c:
            c_objects, c_addresses, decorated, c_libraries = c_link_inputs(target, symbols)
            emitter.c_linked = True
            # a_XXXXXXXX: the C's original-image addresses, defined in the
            # section that holds them (a linker-script assignment per symbol
            # makes ld quadratic); header addresses stay numbers
            data_aliases = {}
            for v in c_addresses:
                if emitter.va <= v < emitter.va + len(emitter.data):
                    emitter.extra_sets.append((f'_a_{v:08x}', emitter.symbol(v)))
                    continue
                section = next((n for n, start, end in spans if start <= v < end), None)
                if section is None:
                    emitter.extra_sets.append((f'_a_{v:08x}', f'0x{v:x}'))
                else:
                    data_aliases.setdefault(section, {})[v] = f'_a_{v:08x}'
            # detour and vtable sources: the image refers to the C by its plain
            # name, an equate to the decorated name (quoted: stdcall names hold @)
            sources = {s for _, s, _ in emitter.detours.values()} | set(vtables.values())
            missing = sources - set(decorated)
            if missing:
                raise RuntimeError(f'C symbols missing from the link-mode payload: {sorted(missing)[:5]}')
            emitter.decorated = decorated
            log.append(f'C linked with the image: {len(c_objects)} objects, {len(c_addresses)} address '
                       f'symbols, {len(sources)} detour/vtable symbols')
        emitter.converge(work, 'text', log)
    elif assemble_only:
        # Build from the existing (possibly hand-edited) sources without regenerating.
        for section in pe.sections:
            stem = section.Name.rstrip(b'\0').decode()[1:]
            if not (work / f'{stem}.s').exists():
                raise RuntimeError(f'{work / (stem + ".s")} missing; run without --assemble-only first')
        run([COMPILERS / 'as.exe', '--32', '--no-pad-sections', '-o', str(work / 'text.o'), str(work / 'text.s')], work)
        log.append('assembled existing text.s')
    else:
        emitter = TextEmitter(text, base + text_rva, seeds, detour_map(manifest), symbols,
                              function_names(pe, manifest, target))
        emitter.plan()
        emitter.converge(work, 'text', log)
    outputs = {'.text': object_bytes(work / 'text.o', '.text')}

    for section in pe.sections:
        name = section.Name.rstrip(b'\0').decode()
        if name == '.text':
            continue
        data = pe.get_data(section.VirtualAddress, section.SizeOfRawData)
        refs = {rva: f'"{decorated[vtables[rva]]}"' if link_c else symbols[vtables[rva]] for rva in vtables
                if section.VirtualAddress + base <= rva
                < section.VirtualAddress + base + section.SizeOfRawData}
        if not assemble_only:
            start = base + section.VirtualAddress
            emit_data_section(data, start, work / f'{name[1:]}.s', refs,
                              {w: e for w, e in words.items() if start <= w < start + len(data)},
                              wanted.get(name, ()), data_aliases.get(name, {}) if link_c else {},
                              {w: e for w, e in import_items.items() if start <= w < start + len(data)}
                              if symbolic else None, import_names if symbolic else None)
        run([COMPILERS / 'as.exe', '--32', '-o', str(work / f'{name[1:]}.o'),
             str(work / f'{name[1:]}.s')], work)
        outputs[name] = object_bytes(work / f'{name[1:]}.o', name)
    grow_offset = edit_items = None
    if edits:  # hand edits inside .text, after the identical emit has converged
        code = {emitter.va + offset: kind for offset, _, kind, _, _ in emitter.instructions}
        for address, (mode_, _) in edits.items():
            if code.get(address) not in ('insn', 'branch'):
                raise RuntimeError(f'--{mode_} {address:08x}: not an instruction written as text')
        emitter.edits = dict(edits)
        emitter.emit(work / 'text.s')
        run([COMPILERS / 'as.exe', '--32', '--no-pad-sections', '-o', str(work / 'text.o'), str(work / 'text.s')], work)
        marks = {}
        for line in run([COMPILERS / 'nm.exe', str(work / 'text.o')], work).splitlines():
            parts = line.split()
            if len(parts) == 3 and parts[2].startswith('bfv_edit_'):
                marks[parts[2]] = int(parts[0], 16)
        sizes = {a: marks[f'bfv_edit_{a:08x}_end'] - marks[f'bfv_edit_{a:08x}_start'] for a in edits}
        edit_items = emitter.gate_items(sizes)
        outputs['.text'] = object_bytes(work / 'text.o', '.text')
        log.append('edits: ' + ', '.join(f'{m} {a:08x} ({sizes[a]} bytes)' for a, (m, _) in sorted(edits.items())))
    if grow_text and grow_at is None:  # a size-changing edit: N int3 bytes at the end of .text
        with (work / 'text.s').open('a', encoding='utf-8') as f:
            f.write(f'.text\n/* --grow-text */\n  .fill {grow_text}, 1, 0xcc\n')
    elif grow_text:  # ... or before the label of grow_at, inside .text
        source = (work / 'text.s').read_text(encoding='utf-8')
        label = re.search(rf'^[A-Za-z_]\w*_{grow_at:08x}:\n', source, re.M)
        if label is None:
            raise RuntimeError(f'--grow-at {grow_at:08x}: text.s has no label there')
        source = (source[:label.start()] + f'/* --grow-text */\n  .fill {grow_text}, 1, 0xcc\n'
                  + source[label.start():])
        (work / 'text.s').write_text(source, encoding='utf-8')
        grow_offset = grow_at - base - next(s.VirtualAddress for s in pe.sections
                                            if s.Name.rstrip(b'\0') == b'.text')
    if grow_text:
        run([COMPILERS / 'as.exe', '--32', '--no-pad-sections', '-o', str(work / 'text.o'), str(work / 'text.s')], work)
        outputs['.text'] = object_bytes(work / 'text.o', '.text')
    if symbolic:
        layout = sequential_layout(pe, outputs)
        if link_c:  # the C links by name, so it can move up past a grown image
            last = pe.sections[-1].Name.rstrip(bytes(1)).decode()
            growth = section_growth(pe.sections[-1], len(outputs[last]), pe.OPTIONAL_HEADER.FileAlignment)
            end = base + layout[last] + pe.sections[-1].Misc_VirtualSize + growth
            payload_address = max(payload_address, align(end, pe.OPTIONAL_HEADER.SectionAlignment))
        wanted_symbols = {entry_symbol} | {f'd_{a:08x}' for span in pe_spans.values() for a in span}
        c_payload = None
        if link_c:
            if 'bfv_shadow_end' in symbols:
                wanted_symbols.add('_bfv_shadow_end')
            wanted_symbols |= {decorated[s] for s in sources}
            c_payload = (c_objects, payload_address, c_libraries)
        linked, values = link_sections(work, pe, wanted_symbols, layout, c_payload)
        log.append('linked with GNU ld; header layout fields from the link')
        if link_c:
            _, linked_payload = linked.pop('.bfvmod')
            separate = (PROJECT / 'build' / 'linkmode' / target / 'payload.bin').read_bytes()
            log.append(f'C payload from the same link: {len(linked_payload)} bytes, '
                       f'{"identical to" if linked_payload == separate else "DIFFERENT from"} '
                       f'tools/linkmode.py\'s payload.bin')
            gate = [linked_payload == separate or changed]
            moved = {symbols[s]: values[decorated[s]] for s in sources}
            payload = linked_payload
            if 'bfv_shadow_end' in symbols:
                symbols = dict(symbols, bfv_shadow_end=values['_bfv_shadow_end'])
    rva = payload_address - pe.OPTIONAL_HEADER.ImageBase
    raw_size = align(len(payload), pe.OPTIONAL_HEADER.FileAlignment)
    # the readable runtime's thread shadow stacks are zero memory after the payload
    virtual_size = max(len(payload), symbols.get('bfv_shadow_end', payload_address) - payload_address)
    if symbolic:  # headers.s holds the .bfvmod section entry too
        image = compose_image(pe, original, linked, values, pe_spans, entry_symbol, work,
                              ('.bfvmod', virtual_size, rva, raw_size, 0xe0000060))
        outputs = {name: blob for name, (_, blob) in linked.items()}
        log.append('headers assembled from headers.s')
    else:
        image = bytearray(original)

    output = image
    raw = align(len(output), pe.OPTIONAL_HEADER.FileAlignment)
    output.extend(b'\0' * (raw - len(output)))
    output.extend(payload)
    output.extend(b'\0' * (raw_size - len(payload)))
    if not symbolic:
        header_offset = pe.sections[-1].get_file_offset() + 40
        section_header = struct.pack('<8sIIIIIIHHI', b'.bfvmod\0', virtual_size, rva,
                                     raw_size, raw, 0, 0, 0, 0, 0xe0000060)
        output[header_offset:header_offset + 40] = section_header
        struct.pack_into('<H', output, pe.FILE_HEADER.get_field_absolute_offset('NumberOfSections'),
                         len(pe.sections) + 1)
        struct.pack_into('<I', output, pe.OPTIONAL_HEADER.get_field_absolute_offset('SizeOfImage'),
                         align(rva + virtual_size, pe.OPTIONAL_HEADER.SectionAlignment))
        struct.pack_into('<I', output, pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'), 0)

    for section in pe.sections if not symbolic else ():  # symbolic: composed from the link
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
    if changed:
        report['grow_gate'] = grow_gate(work, pe, baseline.read_bytes(), output, grow_text, grow_offset, log,
                                        edit_items)
    elif link_c:
        gate.append(link_c_gate(work, pe, installed, output, moved, log))
        report['link_c_gate'] = all(gate)
    report['mode'] = ('assemble-only' if assemble_only else 'linked-c' if link_c
                      else 'symbolic' if symbolic else 'regenerated')
    (work / 'recomp-report.json').write_text(json.dumps(report, indent=2))
    if install:
        (game_dir / spec['output']).write_bytes(bytes(output))
        print(f'{target}: installed {game_dir / spec["output"]}')
    gates = ''.join(f' {name}={report[name]}' for name in ('grow_gate', 'link_c_gate') if name in report)
    print(f"{target}: recomp {destination.name} identity={report['byte_identity']}{gates} "
          f"diffs={report['differing_bytes']} log={log}")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=('client', 'server', 'both'), default='both')
    parser.add_argument('--game-dir', type=Path, default=build.GAME)
    parser.add_argument('--assemble-only', action='store_true',
                        help='assemble the existing build/<target>-recomp/*.s (keeps hand edits) '
                             'instead of regenerating them from the image')
    parser.add_argument('--symbolic', action='store_true',
                        help='write switch tables and absolute references as symbols (from '
                             'tools/symscan) and link the sections with GNU ld into build/<target>-symbolic')
    parser.add_argument('--link-c', action='store_true',
                        help='--symbolic, and link the reconstructed C (tools/linkmode.py) with the '
                             'image in the same ld run, into build/<target>-linked-c')
    parser.add_argument('--grow-text', type=lambda v: int(v, 0), default=0, metavar='N',
                        help='size-change test: append N int3 bytes to .text, link with the later '
                             'sections moved, and check against the build without it')
    parser.add_argument('--grow-at', type=lambda v: int(v, 16), default=None, metavar='VA',
                        help='with --grow-text: insert the bytes before the text.s label for this '
                             'address (hex) instead of at the end of .text')
    parser.add_argument('--insert', action='append', default=[], metavar='VA:ASM',
                        help='edit test: insert the assembly (Intel syntax, lines separated by "|") '
                             'before the instruction at VA (hex); branches to VA reach it. Checked '
                             'instruction by instruction against the build without it (tools/textcheck)')
    parser.add_argument('--replace', action='append', default=[], metavar='VA:ASM',
                        help='edit test: replace the instruction at VA with the assembly (empty: remove it)')
    parser.add_argument('--install', action='store_true',
                        help='copy a successful build over <game-dir>/<editable output>')
    args = parser.parse_args()
    game_dir = args.game_dir.resolve()
    edits = {}
    for mode, values in (('insert', args.insert), ('replace', args.replace)):
        for value in values:
            address, _, assembly = value.partition(':')
            edits[int(address, 16)] = (mode, assembly.replace('|', '\n'))
    for target in ('client', 'server') if args.target == 'both' else (args.target,):
        build_recomp(target, game_dir, args.assemble_only, args.install, args.symbolic, args.link_c,
                     args.grow_text, args.grow_at, edits or None)


if __name__ == '__main__':
    main()
