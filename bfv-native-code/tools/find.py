"""Find game code in the lifted C.

    python bfv-native-code/tools/find.py beginRepair            # string, class, file or address
    python bfv-native-code/tools/find.py BotMain --target server
    python bfv-native-code/tools/find.py 009d42e0 --callers     # who calls it
    python bfv-native-code/tools/find.py 009d42e0 --calls       # what it calls

Prints file:line (clickable in most editors), address, class and strings.
Searches the comment above each function in the .c files (class, evidence and
up to four strings, like INDEX.tsv). Read-only.
"""
import argparse
import re
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
HEAD = re.compile(r'^/\* ([0-9a-f]{8})  (?:(.*?)  )?\((.*?)\)(?: \*/|\n \* strings: (.*?) \*/)', re.M)
CALL = re.compile(r'^    /\* ([0-9a-f]{8})  call (0x[0-9a-f]+) \*/', re.M)


def functions(folder):
    """Yield (address, path, line, class, evidence, strings, text) for every lifted function."""
    for path in sorted(folder.rglob('*.c')):
        src = path.read_text(encoding='utf-8')
        marks = list(HEAD.finditer(src))
        for m, nxt in zip(marks, marks[1:] + [None]):
            line = src.count('\n', 0, m.start()) + 1
            text = src[m.start():nxt.start() if nxt else len(src)]
            yield m.group(1), path, line, m.group(2) or '', m.group(3), m.group(4) or '', text


def show(root, f):
    address, path, line, cls, evidence, strings, _ = f
    print(f'{path.relative_to(root).as_posix()}:{line}  {address}  {cls or "-"}  ({evidence})' + (f'  {strings}' if strings else ''))


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('query', help='text (case-insensitive) or an 8-digit address')
    p.add_argument('--target', choices=['client', 'server'], default='client')
    p.add_argument('--callers', action='store_true', help='functions that call the address')
    p.add_argument('--calls', action='store_true', help='direct calls made by the address')
    p.add_argument('--limit', type=int, default=200)
    a = p.parse_args()
    folder = PROJECT / 'lifted' / a.target
    root = PROJECT.parent
    q = a.query.lower().removeprefix('0x')
    every = list(functions(folder))
    if a.callers or a.calls:
        q = q.zfill(8)
        by_address = {f[0]: f for f in every}
        if a.calls:
            if q not in by_address: raise SystemExit(f'{q}: not a lifted function')
            for _, target in CALL.findall(by_address[q][6]):
                t = int(target, 16)
                print(f'{t:08x}  ' if f'{t:08x}' not in by_address else '', end='')
                if f'{t:08x}' in by_address: show(root, by_address[f'{t:08x}'])
                else: print('(native or hand-written C)')
            return
        hits = [f for f in every if any(int(t, 16) == int(q, 16) for _, t in CALL.findall(f[6]))]
    else:
        hits = [f for f in every if q == f[0] or q in f[3].lower() or q in f[5].lower()
                or q in f[1].relative_to(folder).as_posix().lower()]
    for f in hits[:a.limit]: show(root, f)
    print(f'{len(hits)} functions' + (f' (first {a.limit} shown)' if len(hits) > a.limit else ''))


if __name__ == '__main__':
    main()
