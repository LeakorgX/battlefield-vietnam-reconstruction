"""Find game code in the lifted C.

    python bfv-native-code/tools/find.py beginRepair            # string, class, file or address
    python bfv-native-code/tools/find.py BotMain --target server
    python bfv-native-code/tools/find.py 009d42e0 --callers     # who calls it
    python bfv-native-code/tools/find.py 009d42e0 --calls       # what it calls
    python bfv-native-code/tools/find.py repairFactor --uses    # every instruction that uses a field, global, ...
    python bfv-native-code/tools/find.py 00626890 --hook        # a mod hook for it (mods/<target>/*.c)
    python bfv-native-code/tools/find.py FireArms --class       # its virtual methods (slots) and named fields

Prints file:line (clickable in most editors), address, name and strings, in the
readable C (readable/<target>/) by default or the L1 C (--tree lifted).
Searches the comment above each function in the .c files (name, class, evidence
and up to four strings, like INDEX.tsv); calls and --uses notes come from the
L1 C. Read-only.
"""
import argparse
import re
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
HEAD = re.compile(r'^/\* ([0-9a-f]{8})  (\S+)  \((.*?)\)\n((?: \* .*\n)*) \*/$', re.M)
CALL = re.compile(r'^    /\* ([0-9a-f]{8})  call (0x[0-9a-f]+)(?:  ; .*?)? \*/', re.M)


def index(target):
    """{address: (file, name, class, evidence, strings, command)} from lifted/<target>/INDEX.tsv."""
    rows = {}
    path = PROJECT / 'lifted' / target / 'INDEX.tsv'
    for line in path.read_text(encoding='utf-8').splitlines()[1:]:
        cols = line.split(chr(9))
        rows[cols[0]] = tuple(cols[1:7]) + ('',) * (7 - len(cols))
    return rows


def functions(folder):
    """Yield (address, path, line, name, class, evidence, strings, text) for every lifted function."""
    for path in sorted(folder.rglob('*.c')):
        yield from functions_in(path)


def functions_in(path):
    if path.is_file():
        src = path.read_text(encoding='utf-8')
        marks = list(HEAD.finditer(src))
        for m, nxt in zip(marks, marks[1:] + [None]):
            line = src.count('\n', 0, m.start()) + 1
            text = src[m.start():nxt.start() if nxt else len(src)]
            info = dict(l[3:].split(': ', 1) for l in m.group(4).splitlines() if ': ' in l)
            yield m.group(1), path, line, m.group(2), info.get('class', ''), m.group(3), info.get('strings', ''), text


def show(root, f, where=None):
    address, path, line, name, cls, evidence, strings, _ = f
    if where and address in where: path, line = where[address]
    print(f'{path.relative_to(root).as_posix()}:{line}  {address}  {name}  ({evidence})' + (f'  {strings}' if strings else ''))


def hook(target, f, where):
    """A mod hook skeleton for the function, and whether the build runs it as C."""
    import json
    address, path, line, name, *_ = f
    text = f[7]
    if address in where:
        rpath, rline = where[address]
        lines = rpath.read_text(encoding='utf-8').splitlines()[rline - 1:rline + 40]
        text = '\n'.join(lines)
    abi = re.search(r'^/\* ((?:this|std|fast)call: .*?|.*stack argument.*?) \*/$', text, re.M)
    manifest = PROJECT / 'build' / target / 'manifest.json'
    state = 'unknown (build once first)'
    try:
        patches = {p['entry']: p['source'] for p in json.loads(manifest.read_text())['entry_patches']}
    except (OSError, ValueError, KeyError):
        patches = None  # not built yet, or a build is writing it
    if patches is not None:
        source = patches.get(address, '')
        state = ('yes' if source.startswith((f'bfv_{target}_l1_', f'bfv_mod_{target}_')) else
                 f'no: hand-written C ({source})' if source else 'no: runs natively (not verified or not detourable)')
    c_name = re.sub(r'[^A-Za-z0-9_]', '_', name.replace('::', '_'))
    print(f'{address}  {name}' + (f'  ({abi.group(1)})' if abi else ''))
    print(f'hookable: {state}')
    hook_name = 'my_' + c_name.lower()[:40]
    print(f'\n#include "bfv_mod.h"\n\n'
          f'MOD_BEFORE(0x{address}, {hook_name}_before)  /* or MOD_BEFORE({c_name}, ...) */\n{{\n'
          f'    /* MOD_THIS, MOD_ARG(0), ...; MOD_RETURN(value) skips the function */\n}}\n\n'
          f'MOD_AFTER(0x{address}, {hook_name}_after)\n{{\n'
          f'    /* MOD_RESULT / MOD_SET_RESULT(value), MOD_RESULT_F for floats */\n}}')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('query', help='name, class, string, file (case-insensitive) or an 8-digit address')
    p.add_argument('--target', choices=['client', 'server'], default='client')
    p.add_argument('--callers', action='store_true', help='functions that call the address')
    p.add_argument('--calls', action='store_true', help='direct calls made by the address')
    p.add_argument('--uses', action='store_true', help='instructions whose note (after ;) contains the query')
    p.add_argument('--hook', action='store_true', help='print a mod hook for the address')
    p.add_argument('--class', dest='cls', action='store_true',
                   help='the class: each vtable slot with its function and name, and its named fields')
    p.add_argument('--tree', choices=['readable', 'lifted'], default='readable',
                   help='where to point: readable C (default) or L1 C')
    p.add_argument('--limit', type=int, default=200)
    a = p.parse_args()
    folder = PROJECT / 'lifted' / a.target
    root = PROJECT.parent
    q = a.query.lower().removeprefix('0x')
    readable = PROJECT / 'readable' / a.target
    use_readable = a.tree == 'readable' and readable.is_dir()
    rows = index(a.target)
    if a.cls:
        import slot_names
        lines = {}
        def locate(address, name):
            path = (readable if use_readable else folder) / name
            if path not in lines:
                text = path.read_text(encoding='utf-8') if path.exists() else ''
                lines[path] = {m.group(1): text.count('\n', 0, m.start()) + 1 for m in re.finditer(r'^/\* ([0-9a-f]{8})  ', text, re.M)}
            line = lines[path].get(f'{address:08x}')
            return path.relative_to(root).as_posix() + (f':{line}' if line else '')
        slot_names.describe(slot_names.ClassNames(a.target), a.query, locate)
        return
    if not (a.uses or a.callers):
        # the INDEX.tsv names the file: read only the files that can match
        if a.hook or a.calls:
            files = {rows[q.zfill(8)][0]} if q.zfill(8) in rows else set()
        else:
            files = {r[0] for addr, r in rows.items() if q == addr or any(q in c.lower() for c in r)}
        every = [f for name in sorted(files) for f in functions_in(folder / name)]
        where = {f[0]: (f[1], f[2]) for name in sorted(files) for f in functions_in(readable / name)} if use_readable else {}
    else:
        every = list(functions(folder))
        where = {f[0]: (f[1], f[2]) for f in functions(readable)} if use_readable else {}
    if a.hook:
        q = q.zfill(8)
        f = next((f for f in every if f[0] == q), None)
        if not f: raise SystemExit(f'{q}: not a lifted function')
        show(root, f, where); hook(a.target, f, where)
        return
    if a.uses:
        n = 0
        for f in every:
            for m in re.finditer(r'^    /\* ([0-9a-f]{8})  (.*?)  ; (.*?) \*/$', f[7], re.M):
                if q in m.group(3).lower():
                    n += 1
                    if n <= a.limit:
                        line = f[2] + f[7].count('\n', 0, m.start())
                        print(f'{f[1].relative_to(root).as_posix()}:{line}  {f[3]}  {m.group(2)}  ; {m.group(3)}'
                              + (f'  [readable: {where[f[0]][0].relative_to(root).as_posix()}:{where[f[0]][1]}]' if f[0] in where else ''))
        print(f'{n} instructions' + (f' (first {a.limit} shown)' if n > a.limit else ''))
        return
    if a.callers or a.calls:
        q = q.zfill(8)
        by_address = {f[0]: f for f in every}
        if a.calls:
            if q not in by_address: raise SystemExit(f'{q}: not a lifted function')
            targets = {f'{int(t, 16):08x}' for _, t in CALL.findall(by_address[q][7])}
            for name in sorted({rows[t][0] for t in targets if t in rows and t not in by_address}):
                by_address.update((f[0], f) for f in functions_in(folder / name))
                if use_readable: where.update((f[0], (f[1], f[2])) for f in functions_in(readable / name))
            for _, target in CALL.findall(by_address[q][7]):
                t = int(target, 16)
                print(f'{t:08x}  ' if f'{t:08x}' not in by_address else '', end='')
                if f'{t:08x}' in by_address: show(root, by_address[f'{t:08x}'], where)
                else: print('(native or hand-written C)')
            return
        hits = [f for f in every if any(int(t, 16) == int(q, 16) for _, t in CALL.findall(f[7]))]
    else:
        hits = [f for f in every if q == f[0] or q in f[3].lower() or q in f[4].lower() or q in f[6].lower()
                or q in f[1].relative_to(folder).as_posix().lower()]
    for f in hits[:a.limit]: show(root, f, where)
    print(f'{len(hits)} functions' + (f' (first {a.limit} shown)' if len(hits) > a.limit else ''))


if __name__ == '__main__':
    main()
