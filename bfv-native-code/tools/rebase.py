"""Compare a newer build of a supported executable with the image the C was made from.

The hand-written and lifted C, the catalogs and the function index
(reports/<target>/all-functions.tsv) were all made from one image, the
reference. A later build with the same section layout (patch code added in new
sections and section slack, some engine code changed in place) still runs
nearly all of that code byte for byte. This tool lists what differs and writes
it to rebase.json, which build.py, verify_lifted.py and verify.py read:

- reference_sha256 / sha256: the image the C was made from and the one the
  target now builds against (TARGETS[target]['sha']).
- changed: byte runs (virtual addresses) that differ in the memory both images
  map, section by section.
- new_sections, grown_sections, entry_point: code the reference does not have.
- native_functions: indexed functions with changed bytes. Every hand-written or
  catalogued entry inside one stays native, and so does a vtable patch whose
  replaced function is one.
- native_lifted: lifted/ and readable/ catalog entries whose ranges touch a
  changed run. verify_lifted.py reports them as native instead of checking
  them, and build.py does not detour them.
- unindexed: changed runs outside every indexed function (patch code written
  into padding, jump-table words, the entry stub).

build.py and verify_lifted.py recompute the native lists from `changed` and
stop when they differ, so a stale rebase.json fails instead of replacing
changed code with C.

  uv run --with pefile python bfv-native-code/tools/rebase.py --target client \
    --reference <reference BfVietnam.exe> --exe <new BfVietnam.exe>
"""
import argparse
import hashlib
import json
from pathlib import Path
import pefile

from build import PROJECT, TARGETS, align, function_index, overlaps

TREES = ('lifted', 'readable')
CATALOGS = ('constant-returns', 'word-getters', 'field-accessors', 'static-initializers', 'deleting-destructors',
            'constructor-calls', 'dynamic-casts', 'clones', 'global-stores')


def mapped(pe):
    """{section name: (address, bytes the loader maps, characteristics)}: the raw data
    up to the section's page-rounded virtual size, zero-filled to its virtual size."""
    base = pe.OPTIONAL_HEADER.ImageBase
    out = {}
    for s in pe.sections:
        size = align(max(s.Misc_VirtualSize, 1), pe.OPTIONAL_HEADER.SectionAlignment)
        data = s.get_data()[:min(s.SizeOfRawData, size)]
        data += bytes(max(s.Misc_VirtualSize, len(data)) - len(data))
        out[s.Name.rstrip(b'\0').decode()] = (base + s.VirtualAddress, data, s.Characteristics)
    return out


def differing_runs(reference, image):
    """Byte runs that differ inside the memory both images map, [(lo, hi)]."""
    runs = []
    for name, (va, old, _) in reference.items():
        if name not in image:
            raise RuntimeError(f'{name}: the reference section is missing from the image')
        new_va, new, _ = image[name]
        if new_va != va:
            raise RuntimeError(f'{name} moved from {va:08x} to {new_va:08x}: addresses in the C would not hold')
        n = min(len(old), len(new))
        i = 0
        while i < n:
            if old[i] != new[i]:
                j = i
                while j < n and old[j] != new[j]:
                    j += 1
                runs.append((va + i, va + j))
                i = j
            else:
                i += 1
    return sorted(runs)


def native_lists(target, runs):
    """The native functions and lifted entries for `runs` (build.py recomputes them the same way)."""
    touches = overlaps(runs)
    functions = [f for f in function_index(target) if touches(f[0], f[1])]
    lifted = {}
    for tree in TREES:
        path = PROJECT / tree / target / 'catalog.json'
        if path.exists():
            entries = json.loads(path.read_text())['entries']
            lifted[tree] = sorted(e['address'] for e in entries
                                  if any(touches(*(int(x, 16) for x in r.split('-'))) for r in e['ranges']))
    return functions, lifted


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=sorted(TARGETS), required=True)
    parser.add_argument('--reference', type=Path, required=True, help='the image the C and catalogs were made from')
    parser.add_argument('--exe', type=Path, required=True, help='the newer image to build against')
    args = parser.parse_args()
    old_bytes, new_bytes = args.reference.read_bytes(), args.exe.read_bytes()
    old_pe, new_pe = pefile.PE(data=old_bytes), pefile.PE(data=new_bytes)
    for pe in (old_pe, new_pe):
        if pe.OPTIONAL_HEADER.ImageBase != 0x400000 or pe.OPTIONAL_HEADER.DllCharacteristics & 0x40:
            raise RuntimeError('Only the inspected fixed-base executable layout is supported')
    reference, image = mapped(old_pe), mapped(new_pe)
    runs = differing_runs(reference, image)
    functions, lifted = native_lists(args.target, runs)
    # The audited catalogs replace whole short bodies; none may touch a change
    # (build.py checks their guards again). A constant return has no stored
    # body: its indexed function, or `mov eax, imm; ret n` after the guard.
    touches = overlaps(runs)
    ends = {lo: hi for lo, hi, _ in function_index(args.target)}
    for name in CATALOGS:
        for e in json.loads((PROJECT / f'{name}.json').read_text())['targets'][args.target]['entries']:
            a = int(e['address'], 16)
            size = len(bytes.fromhex(e['native_body'])) if 'native_body' in e else \
                ends.get(a, a + len(bytes.fromhex(e['guarded_prefix'])) + 3) - a
            if touches(a, a + size):
                raise RuntimeError(f'{name}.json entry {e["address"]} changed: drop it from the catalog first')
    covered = overlaps([(lo, hi) for lo, hi, _ in functions])
    base = new_pe.OPTIONAL_HEADER.ImageBase
    record = dict(
        reference_sha256=hashlib.sha256(old_bytes).hexdigest(),
        sha256=hashlib.sha256(new_bytes).hexdigest(),
        entry_point=[f'{base + p.OPTIONAL_HEADER.AddressOfEntryPoint:08x}' for p in (old_pe, new_pe)],
        new_sections=[f'{n} {va:08x}-{va + len(d):08x} {c:08x}' for n, (va, d, c) in image.items() if n not in reference],
        grown_sections=[f'{n} {va:08x}-{va + len(d):08x} (was {va:08x}-{va + len(reference[n][1]):08x})'
                        for n, (va, d, c) in image.items() if n in reference and len(d) > len(reference[n][1])],
        changed=[f'{lo:08x}-{hi:08x}' for lo, hi in runs],
        native_functions=[f'{lo:08x}-{hi:08x} {name}' for lo, hi, name in functions],
        native_lifted=lifted,
        unindexed=[f'{lo:08x}-{hi:08x}' for lo, hi in runs if not covered(lo, hi)])
    path = PROJECT / 'rebase.json'
    document = json.loads(path.read_text()) if path.exists() else {}
    document.setdefault('targets', {})[args.target] = record
    path.write_text(json.dumps(document, indent=1) + '\n', newline='\n')
    print(f'{args.target}: {len(runs)} changed runs, {len(functions)} native functions, '
          + ', '.join(f'{len(v)} native {k} entries' for k, v in lifted.items())
          + f', {len(record["unindexed"])} unindexed runs; wrote {path.name}')


if __name__ == '__main__':
    main()
