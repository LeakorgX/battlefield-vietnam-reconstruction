"""Build a local class-to-function navigation index from binary evidence."""
import argparse
import csv
import json
from pathlib import Path


def link(label, path):
    return f'[{label}](<{path.resolve().as_posix()}>)'


def make_index(root):
    classes = json.loads((root / 'rtti-vtables.json').read_text(encoding='utf-8'))
    with (root / 'decompiled/decompilation.tsv').open(encoding='utf-8') as f:
        functions = {r['address']: r for r in csv.DictReader(f, delimiter='\t')}
    exports = {}
    for category in {f['category'] for f in functions.values()}:
        for path in (root / 'decompiled' / category).glob('*.c'):
            exports.setdefault((category, path.name.split('_', 1)[0]), []).append(path)
    out = ['# Native AI, networking and gameplay class index', '',
           'Class membership comes from MSVC RTTI. Links open the native C-like export.',
           'Slots shared by multiple classes can use an alias in the exported filename.',
           'Signatures and field types remain inferred; consult assembly and runtime checks.', '']
    for category in ('ai', 'gameplay'):
        out.extend([f'## {category.capitalize()}', '', '| Class | Vtable / object offset | Virtual slots |',
                    '| --- | --- | --- |'])
        for cls in sorted((c for c in classes if c['category'] == category), key=lambda c: c['name']):
            for table in cls['vtables']:
                slots = []
                for method in table['methods']:
                    address = method['address']
                    f = functions.get(address)
                    if f and f['status'] == 'ok':
                        # The manifest records the qualified name; the exporter
                        # filename uses the unqualified function name. Resolve by
                        # unique entry address so C++ namespaces cannot break links.
                        paths = exports.get((f['category'], address), [])
                        if len(paths) != 1:
                            raise RuntimeError(f'Expected one export for {address}: {paths}')
                        path = paths[0]
                        slots.append(link(f"{method['slot']}: {address}", path))
                    else:
                        slots.append(f"{method['slot']}: {address} (not decompiled)")
                out.append(f"| `{cls['name']}` | `{table['vtable']}` / {table['object_offset']} | {'; '.join(slots)} |")
        out.append('')
    (root / 'CLASS_INDEX.md').write_text('\n'.join(out), encoding='utf-8')
    return sum(f['status'] == 'ok' for f in functions.values()), sum(f['status'] != 'ok' for f in functions.values())


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, default=Path(__file__).resolve().parents[2] / 'bfv-reference-local/native-logic')
    args = p.parse_args()
    for root in (args.root, args.root / 'server'):
        ok, failed = make_index(root)
        print(f'{root.name}: {ok} successful function exports; {failed} failures; class index written')


if __name__ == '__main__':
    main()
