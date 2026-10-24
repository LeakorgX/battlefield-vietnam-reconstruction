"""Link-mode build of the reconstructed C: every original-image address is a
linker symbol a_XXXXXXXX instead of a number (tools/linkmode/linkmode.cpp).

    python tools/linkmode.py --target client

1. Runs build/linkmode/linkmode.exe on lifted/<t>, readable/<t> and
   readable/runtime/symbols_<t>.h into build/linkmode/<t>/.
2. Compiles the tree the last build.py run used (build/<t>/objects.rsp) with
   the build's flags.
   The hand-written modules (src/*.c with build/<t>/target.h) and the mods
   are compiled the same way from linkmode.exe --src copies: target.h names
   and the src literals that address the image become a_ symbols, decided
   against the native function each catalog entry replaces.
3. Links the payload exactly like build.py, with the same objects in their
   link-mode compiles, and every a_ symbol defined at its original address
   (defs.ld). GCC folds and schedules a known number differently from a
   symbol, so this is a different compile of the same C, not a copy of
   build/<t>/payload.bin. Guest memory accesses are volatile, so GCC's
   assumption that distinct symbols do not overlap cannot reorder them.
4. Links again with every symbol moved by SHIFT and checks with
   build/linkmode/relocheck.exe that only 32-bit fields moved, by SHIFT.

Build the tool first: tools/linkmode/build.ps1. Needs build/symscan/<t>-refs.tsv
(tools/symscan) and a finished build.py run for the target.
"""
import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import build  # noqa: E402  (flags, targets, run)

PROJECT = build.PROJECT
SHIFT = 0x135791  # odd, so every byte of a moved field changes
IMAGE = {'client': (0x400000, 0xe26000), 'server': (0x400000, 0xc37000)}


def rsp_paths(path):
    return [Path(line.strip().strip('"')) for line in path.read_text().splitlines() if line.strip()]


CATALOGS = ('clones', 'constant-returns', 'constructor-calls', 'deleting-destructors', 'dynamic-casts',
            'field-accessors', 'global-stores', 'static-initializers', 'word-getters')


def compile_cached(source, o, command, cwd, headers):
    """Compiles unless o's key (command, headers and source) is unchanged; returns an error text or None."""
    o.parent.mkdir(parents=True, exist_ok=True)
    h = hashlib.sha256(str([str(c) for c in command]).encode() + bytes(1) + headers + source.read_bytes())
    stamp = o.with_suffix('.key')
    if o.exists() and stamp.exists() and stamp.read_text() == h.hexdigest():
        return None
    try:
        build.run([*command, '-c', source, '-o', o], cwd)
    except RuntimeError as error:
        return str(error)[-1500:]
    stamp.write_text(h.hexdigest())
    return None


def hand_written(target, work, out, objdir, runtime, tree, tool, lo, hi):
    """Link-mode compiles of the hand-written modules (src/*.c, with target.h)
    and the mods: linkmode.exe --src decides their literals against the native
    function each one replaces (the catalogs' native bytes). Returns
    {build.py object: link-mode object}."""
    import mods
    bodies = out / 'src-bodies.tsv'
    with bodies.open('w', newline='\n') as f:
        for name in CATALOGS:
            for e in json.loads((PROJECT / f'{name}.json').read_text())['targets'][target]['entries']:
                f.write(f"{e['symbol']}\t{e['address']}\t{e.get('native_body') or e['guarded_prefix']}\n")
    print(build.run([tool, '--src', PROJECT / f'build/symscan/{target}-refs.tsv', bodies, PROJECT / 'src', target,
                     work / 'target.h', out / 'src', f'{lo:x}', f'{hi:x}'], PROJECT).strip())
    lm = ['-include', runtime / 'linkmode.h']
    jobs = [(out / 'src' / f'{p.stem}.c', objdir / 'src' / f'{p.stem}.o',
             [build.COMPILERS / 'gcc.exe', *build.SRC_FLAGS, '-Wno-sign-compare', *lm, '-I', out / 'src'], work / f'{p.stem}.o')
            for p in sorted((PROJECT / 'src').glob('*.c'))]
    if (work / 'mod_hooks.c').exists() and mods.sources(target):
        # the hooked function's address in each stub becomes its symbol
        hooks = (work / 'mod_hooks.c').read_text()
        used = sorted(set(re.findall(r'bfv_mod_run\(c, 0x([0-9a-f]{8})u', hooks)))
        hooks = re.sub(r'(bfv_mod_run\(c, )0x([0-9a-f]{8})u', r'\1BFV_A(\2)', hooks)
        hooks = hooks.replace('#include "bfv_mod.h"\n', '#include "bfv_mod.h"\n' + ''.join(f'extern char a_{v}[];\n' for v in used), 1)
        (out / 'mods').mkdir(exist_ok=True)
        (out / 'mods' / 'mod_hooks.c').write_text(hooks, newline='\n')
        log = re.search(r'^#define imp_OutputDebugStringA (0x[0-9a-f]+)$',
                        (PROJECT / tree / 'runtime' / f'symbols_{target}.h').read_text(), re.M)
        # mod_runtime.c leaves the symbols header out (BFV_MOD_NO_NAMES), so the
        # import slot's symbol is declared in a header of its own
        (out / 'mods' / 'log_import.h').write_text(f'extern char a_{int(log[1], 16):08x}[];\n', newline='\n')
        command = [build.COMPILERS / 'gcc.exe', *mods.FLAGS, *lm, '-include', out / 'mods' / 'log_import.h',
                   f'-DBFV_MOD_{target.upper()}',
                   f'-DBFV_MOD_LOG_IMPORT=BFV_A({int(log[1], 16):08x})', '-I', mods.MODS / 'include', '-I', runtime,
                   '-I', out / 'src', '-I', work]
        for f in [mods.MODS / 'runtime' / 'mod_runtime.c', *mods.sources(target), out / 'mods' / 'mod_hooks.c']:
            name = ('' if f.parent.name == target or f.parent == out / 'mods' else f.parent.name + '_') + f.stem + '.o'
            jobs.append((f, objdir / 'mods' / name, command, work / 'mods' / name))
        with (out / 'src' / 'addresses.txt').open('a') as a:
            a.write(''.join(f'{v}\n' for v in used) + f'{int(log[1], 16):08x}\n')
    headers = hashlib.sha256()
    for d in (runtime, out / 'src', mods.MODS / 'include', work):
        for f in sorted(d.glob('*.h')):
            headers.update(f.name.encode() + bytes(1) + f.read_bytes())
    headers = headers.digest()
    with ThreadPoolExecutor(max(6, os.cpu_count() or 6)) as pool:
        errors = [e for e in pool.map(lambda j: compile_cached(j[0], j[1], j[2], out, headers), jobs) if e]
    if errors:
        for e in errors[:5]:
            print(e)
        raise RuntimeError(f'{target}: {len(errors)} hand-written link-mode file(s) failed to compile')
    print(f'  {target}: {len(jobs)} hand-written and mod file(s) compiled or current')
    return {original: o for _, o, _, original in jobs}


def link_mode(target):
    work = PROJECT / 'build' / target
    out = PROJECT / 'build' / 'linkmode' / target
    objects = rsp_paths(work / 'objects.rsp')
    store = 'l2' if any('/l2/' in o.as_posix() for o in objects) else 'l1'
    tree = 'readable' if store == 'l2' else 'lifted'
    lo, hi = IMAGE[target]
    tool = PROJECT / 'build' / 'linkmode' / 'linkmode.exe'
    if out.exists():
        shutil.rmtree(out)
    print(build.run([tool, PROJECT / f'build/symscan/{target}-refs.tsv', PROJECT / 'lifted' / target,
                     PROJECT / 'readable' / target, PROJECT / f'readable/runtime/symbols_{target}.h',
                     out, f'{lo:x}', f'{hi:x}'], PROJECT).strip())
    # runtime: the tree's own files, with the link-mode symbols header
    runtime = out / 'runtime'
    for f in (PROJECT / tree / 'runtime').iterdir():
        if f.is_file() and not (runtime / f.name).exists():
            shutil.copy2(f, runtime / f.name)
    (runtime / 'linkmode.h').write_text(
        '/* Link mode: an original-image address is the linker symbol a_XXXXXXXX. */\n'
        '#define BFV_A(x) ((uint32_t)(uintptr_t)a_##x)  /* was 0x...u */\n'
        '#define BFV_AI(x) ((int)(uintptr_t)a_##x)  /* was 0x... (int) */\n')
    sources = [runtime / 'l1_runtime.c', *sorted((out / tree).rglob('*.c'))]
    store_dir = work / store
    objdir = PROJECT / 'build' / 'linkmode' / f'{target}-obj'

    def object_for(source):
        if source.parent == runtime:
            return objdir / 'runtime' / source.with_suffix('.o').name
        return objdir / target / source.relative_to(out / tree).with_suffix('.o')

    # -Wno-sign-compare: GCC exempts a nonnegative int constant compared with an
    # unsigned value, not the same value as a symbol; a diagnostic, no code change
    flags = [*build.LIFTED_FLAGS, '-Wno-sign-compare', '-include', runtime / 'linkmode.h', '-I', runtime]

    common = hashlib.sha256(str([str(build.COMPILERS / 'gcc.exe'), *map(str, flags)]).encode())
    for f in sorted(runtime.iterdir()):
        common.update(f.name.encode() + bytes(1) + f.read_bytes())

    def compile_one(source):
        """Compiles one file unless its object (kept in build/linkmode/<t>-obj
        across runs) has the same key; returns an error text or None."""
        o = object_for(source)
        o.parent.mkdir(parents=True, exist_ok=True)
        h = common.copy()
        h.update(source.read_bytes())
        stamp = o.with_suffix('.key')
        if o.exists() and stamp.exists() and stamp.read_text() == h.hexdigest():
            return None
        try:
            build.run([build.COMPILERS / 'gcc.exe', *flags, '-c', source, '-o', o], out)
        except RuntimeError as error:
            return str(error)[-1500:]
        stamp.write_text(h.hexdigest())
        return None

    with ThreadPoolExecutor(max(6, os.cpu_count() or 6)) as pool:
        errors = [e for e in pool.map(compile_one, sources) if e]
    if errors:
        for e in errors[:5]:
            print(e)
        raise RuntimeError(f'{target}: {len(errors)} link-mode file(s) failed to compile')
    print(f'  {target}: {len(sources)} link-mode file(s) compiled or current')
    hand = hand_written(target, work, out, objdir, runtime, tree, tool, lo, hi)
    # same objects in the same order, lifted ones from the link-mode tree,
    # hand-written and mod ones from their link-mode compiles
    mapped = []
    for o in objects:
        if o in hand:
            mapped.append(hand[o])
            continue
        try:
            rel = o.relative_to(store_dir)
        except ValueError:
            raise RuntimeError(f'{target}: {o} has no link-mode compile')
        mapped.append(objdir / rel)
    missing = [m for m in mapped if not m.exists()]
    if missing:
        raise RuntimeError(f'{target}: {len(missing)} link-mode object(s) missing, e.g. {missing[0]}')
    values = sorted({int(x, 16) for x in [*(out / 'addresses.txt').read_text().split(),
                                           *(out / 'src' / 'addresses.txt').read_text().split()]})
    (out / 'addresses.txt').write_text(''.join(f'{v:08x}\n' for v in values))
    libgcc = []
    if any(o.parent.name == 'mods' for o in objects):
        libgcc = [build.run([build.COMPILERS / 'gcc.exe', '-print-libgcc-file-name'], out).strip()]
    rsp = build.response(out / 'objects.rsp', mapped)

    def link(name, shift):
        defs = out / f'defs{name}.ld'
        defs.write_text(''.join(f'_a_{v:08x} = 0x{v + shift:08x};\n' for v in values))
        exe, binary = out / f'payload{name}.exe', out / f'payload{name}.bin'
        build.run([build.COMPILERS / 'ld.exe', '-T', work / 'payload.ld', '--image-base', '0x400000',
                   '--entry', '_bfv_interpret', '--disable-dynamicbase', '--disable-reloc-section',
                   '-Map', exe.with_suffix('.map'), '-o', exe, rsp, defs, *libgcc], out)
        build.run([build.COMPILERS / 'objcopy.exe', '-O', 'binary', '--only-section', '.payload', exe, binary], out)
        return binary

    mine = link('', 0).read_bytes()
    # the gate: a second link with every a_ symbol moved by SHIFT must differ
    # only in 32-bit fields that moved by SHIFT; the payload then depends on
    # the image's layout only through plain symbol references the image link
    # can resolve wherever the symbols land
    shifted = link('-shifted', SHIFT)
    check = subprocess.run([PROJECT / 'build/linkmode/relocheck.exe', out / 'payload.bin', shifted, f'{SHIFT:x}'],
                           capture_output=True, text=True)
    theirs = (work / 'payload.bin').read_bytes()
    print(f'  {target}: {len(values)} address symbols; payload {len(mine)} bytes '
          f'(sha256 {hashlib.sha256(mine).hexdigest()[:16]}), '
          f'{"identical to" if mine == theirs else "a different compile from"} the numeric payload; '
          f'shift check: {check.stdout.strip() or check.stderr.strip()}')
    return check.returncode == 0


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--target', choices=('client', 'server', 'both'), default='both')
    args = parser.parse_args()
    os.environ['PATH'] = str(build.COMPILERS) + os.pathsep + os.environ['PATH']
    ok = all([link_mode(t) for t in (('client', 'server') if args.target == 'both' else (args.target,))])
    sys.exit(0 if ok else 1)


if __name__ == '__main__':
    main()
