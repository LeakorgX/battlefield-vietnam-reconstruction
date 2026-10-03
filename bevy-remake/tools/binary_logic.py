"""Recover MSVC x86 RTTI/vtable seeds for native game-logic decompilation.

This does not read or extract any game archives. The executable is read-only.
Run using uv run --with capstone --with pefile python tools/binary_logic.py.
"""
import argparse
from collections import defaultdict
import hashlib
import json
from pathlib import Path
import re
import struct

import capstone
import pefile

PROJECT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--exe", type=Path, default=PROJECT.parent / "BfVietnam.exe")
    parser.add_argument("--output", type=Path, default=PROJECT.parent / "bfv-reference-local/native-logic")
    args = parser.parse_args()
    output = args.output.resolve()
    if output == PROJECT or PROJECT in output.parents:
        parser.error("Decompiled reference output must remain outside the source project")
    output.mkdir(parents=True, exist_ok=True)
    data = args.exe.read_bytes()
    pe = pefile.PE(data=data)
    if pe.FILE_HEADER.Machine != 0x14c:
        parser.error("Only MSVC x86 RTTI is supported")
    base = pe.OPTIONAL_HEADER.ImageBase
    sections = [(base+s.VirtualAddress, s.PointerToRawData, s.SizeOfRawData, s.Name.rstrip(b'\0').decode()) for s in pe.sections]

    def va_to_offset(va):
        for start, offset, size, _ in sections:
            if start <= va < start + size:
                return offset + va - start
        return None

    def offset_to_va(offset):
        for start, raw, size, _ in sections:
            if raw <= offset < raw + size:
                return start + offset - raw
        return None

    def u32_va(va):
        offset = va_to_offset(va)
        return struct.unpack_from('<I', data, offset)[0] if offset is not None and offset + 4 <= len(data) else None

    pointers = defaultdict(list)
    # PE data structures are DWORD aligned. Avoid O(types * executable size) scans.
    for start, raw, size, section_name in sections:
        for offset in range(raw, min(raw + size, len(data)) - 3, 4):
            value = struct.unpack_from('<I', data, offset)[0]
            if base <= value < base + pe.OPTIONAL_HEADER.SizeOfImage:
                pointers[value].append(start + offset - raw)
    decoder = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32)
    decoder.detail = True
    executable_ranges = [(base+s.VirtualAddress, base+s.VirtualAddress+s.SizeOfRawData)
                         for s in pe.sections if s.Characteristics & 0x20000000]

    def plausible_code(va):
        offset = va_to_offset(va)
        if offset is None:
            return False
        # A byte stream decoding as x86 is insufficient: vtable data can also
        # decode as instructions. Only accept targets in executable sections.
        if not any(start <= va < end for start, end in executable_ranges):
            return False
        if data[offset] in (0, 0xff) and data[offset:offset+2] != b'\xff\x25':
            return False
        instructions = list(decoder.disasm(data[offset:offset+24], va, count=3))
        return len(instructions) >= 1 and instructions[0].mnemonic not in ('int3', 'hlt', 'invalid')

    classes = []
    seeds = defaultdict(list)
    core_pattern = re.compile(r'^(BB|BAP|AI)|Bot|Strategic|Strategy|PathFind|NavMesh|Behaviour|Behavior')
    gameplay_pattern = re.compile(r'Soldier|HandFireArms|Projectile|PlayerControl|ControlPoint|SpawnPoint|Network|GameEvent|Physics')
    for match in re.finditer(rb'\.\?A[VU][A-Za-z0-9_?$@]+\x00', data):
        name = match.group()[:-1].decode('ascii')
        simple = name[4:].split('@')[0]
        type_va = offset_to_va(match.start() - 8)
        if type_va is None:
            continue
        tables = []
        for reference in pointers.get(type_va, []):
            col = reference - 12
            sig, object_offset, cd_offset = u32_va(col), u32_va(col+4), u32_va(col+8)
            hierarchy = u32_va(col+16)
            if sig != 0 or object_offset is None or object_offset > 0x100000 or cd_offset is None or cd_offset > 0x100000 or hierarchy is None:
                continue
            hierarchy_sig, bases, base_array = u32_va(hierarchy), u32_va(hierarchy+8), u32_va(hierarchy+12)
            if hierarchy_sig != 0 or bases is None or not 1 <= bases <= 1024 or base_array is None or va_to_offset(base_array) is None:
                continue
            for col_reference in pointers.get(col, []):
                vtable = col_reference + 4
                methods = []
                for index in range(512):
                    entry = u32_va(vtable + index*4)
                    if entry is None or not plausible_code(entry):
                        break
                    methods.append(dict(slot=index, address=f'{entry:08x}'))
                if not methods:
                    continue
                tables.append(dict(vtable=f'{vtable:08x}', complete_object_locator=f'{col:08x}', object_offset=object_offset, methods=methods))
        if not tables:
            continue
        category = 'ai' if core_pattern.search(simple) else 'gameplay' if gameplay_pattern.search(simple) else 'other'
        classes.append(dict(name=name, simple=simple, category=category, type_descriptor=f'{type_va:08x}', vtables=tables))
        if category in ('ai', 'gameplay'):
            for table in tables:
                for method in table['methods']:
                    label = re.sub(r'[^A-Za-z0-9_]', '_', simple)[:100] + f"__v{table['object_offset']}_slot{method['slot']:02}"
                    seeds[method['address']].append((category, label, f"RTTI:{table['vtable']}"))
    # Previously recorded entry points are retained as independently labelled leads.
    # Ghidra must find/create a function before any lead is decompiled.
    for address, label in [(0x93fae0, 'AI_BBBailOut_factory_candidate'), (0x93fba0, 'AI_BBChange_factory_candidate'),
        (0x984d40, 'AI_BBPBailOut_ctor_candidate'), (0x986cf0, 'AI_BBPChange_ctor_candidate'),
        (0x4ece20, 'Gameplay_EnterAction_candidate'), (0x442310, 'Gameplay_ModeParser_candidate')]:
        if args.exe.name.casefold() == 'bfvietnam.exe' and plausible_code(address):
            seeds[f'{address:08x}'].append(('ai' if label.startswith('AI_') else 'gameplay', label, 'previous-local-notes; unverified name'))
    (output / 'rtti-vtables.json').write_text(json.dumps(classes, indent=2), encoding='utf-8')
    lines = []
    for address, labels in sorted(seeds.items(), key=lambda item: (not any(v[0] == 'ai' for v in item[1]), item[0])):
        category, label, evidence = labels[0]
        lines.append('\t'.join((address, category, label, evidence, ';'.join(v[1] for v in labels))))
    (output / 'function-seeds.tsv').write_text('\n'.join(lines)+'\n', encoding='utf-8')
    summary = dict(binary=str(args.exe.resolve()), sha256=hashlib.sha256(data).hexdigest(),
        recovered_classes=len(classes), ai_classes=sum(c['category']=='ai' for c in classes),
        gameplay_classes=sum(c['category']=='gameplay' for c in classes), function_seeds=len(seeds),
        ai_function_seeds=sum(any(v[0]=='ai' for v in values) for values in seeds.values()),
        limitations=['Vtable slot boundaries and indirect targets require further verification',
            'Names identify class membership, not fully recovered method semantics',
            'This index is not reconstructed original source code'])
    (output / 'summary.json').write_text(json.dumps(summary, indent=2), encoding='utf-8')
    print(json.dumps(summary, indent=2))


if __name__ == '__main__':
    main()

