"""Name virtual methods and fields after the console commands that call them.

    python bfv-native-code/tools/console_names.py --target both

Writes notes/<target>/names-console.txt for tools/comment_l2.py, which reads
it before the hand-written names.txt (so names.txt wins). The evidence is the
"vfN of Command" note sort_lifted.py puts on each virtual call: the command
handlers of that console command make that call.

- A command that calls one slot names it after its method when the method
  starts with a verb (AiSettings.getViewDistance -> IAISettings::vf9 =
  getViewDistance). Other single calls (Debug.useFrustumGrid) are skipped: the
  handler may only use the slot on the way.
- A property command that calls two adjacent slots names them setX and getX.
  Which is which comes from the implementation's name (setShowBotSense) or
  from the execute handler: the slot it calls when given no argument gets,
  the one it calls with one sets (readable/<target>, so run it after
  comment_l2.py has written the readable C once). The order is not fixed:
  PlayerManager gets the active player in vf23 and sets it in vf24. A pair
  with neither kind of evidence is skipped; conflicting evidence stops the
  script.
- A slot named getX whose implementation only returns this+0xNN names that
  field of the one class built on the slot's class (AISettings+0x1c = viewDistance).

Read-only apart from the names-console.txt files.
"""
import argparse
import collections
import re
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
NOTE = re.compile(r'  ; vf(\d+) of ([^,]+), -> (\w+)::vf(\d+)(?: = ([\w:~]+))?(?: \(returns (&?)this\+0x([0-9a-f]+)\))?')
VERBS = ('get', 'set', 'create', 'delete', 'add', 'remove', 'run', 'end', 'close', 'enable', 'compile', 'draw',
         'check', 'save', 'load', 'dump', 'list', 'refresh', 'init', 'unload', 'build', 'clear', 'show', 'start',
         'stop', 'move', 'copy', 'file')


def implementers(target):
    """{short base class: [short classes whose object starts with it]} from CLASSES.tsv."""
    out = collections.defaultdict(list)
    for row in (PROJECT / 'lifted' / target / 'CLASSES.tsv').read_text(encoding='utf-8').splitlines()[1:]:
        cls, vt, _, bases = row.split('\t')
        if not vt: continue
        for b in filter(None, bases.split('; ')): out[b.split('::')[-1]].append(cls.split('::')[-1])
    return out


def collect(target):
    slots = collections.defaultdict(set)   # (class, n) -> commands
    impl = {}                               # (class, n) -> (implementation name, field offset or None)
    for path in sorted((PROJECT / 'lifted' / target).rglob('*.c')):
        for m in NOTE.finditer(path.read_text(encoding='utf-8')):
            n, cmds, cls, k, name, amp, off = m.groups()
            if n != k: continue
            slots[cls, int(k)].add(cmds)
            impl[cls, int(k)] = (name or '', int(off, 16) if off and not amp else None)
    return slots, impl


def roles(target):
    """{(class, n): 'get' or 'set'} from the readable execute handlers: a
    property command's handler branches on its argument count (this+0x1c);
    the slot called under `== 0` gets, the one under `== 1` sets."""
    out = {}
    for path in sorted((PROJECT / 'readable' / target).rglob('*.c')):
        text = path.read_text(encoding='utf-8')
        if '::execute  (' not in text: continue
        for body in re.split(r'^/\* [0-9a-f]{8}  ', text, flags=re.M):
            if not re.match(r'\S+::execute  \(', body) or not re.search(r'this_1c = M32\(this(?:_\d+)? \+ 0x1c\);', body): continue
            role = None
            for m in re.finditer(r'this_1c == ([01])\)|-> (\w+)::vf(\d+)', body):
                if m.group(1): role = ('get', 'set')[int(m.group(1))]
                elif role: out.setdefault((m.group(2), int(m.group(3))), set()).add(role)
    return {k: next(iter(v)) for k, v in out.items() if len(v) == 1}


def names(target):
    slots, impl = collect(target)
    role = roles(target)
    by_cmd = collections.defaultdict(set)
    for key, cmds in slots.items():
        if len(cmds) == 1 and re.fullmatch(r'[A-Za-z_]\w*(?:\.[A-Za-z_]\w*)?', next(iter(cmds))):
            by_cmd[next(iter(cmds))].add(key)
    out = {}
    for cmd, keys in sorted(by_cmd.items()):
        method = cmd.split('.')[-1]
        keys = sorted(keys)
        if len(keys) == 1 and method.startswith(VERBS):
            out[keys[0]] = (method, cmd)
        elif len(keys) == 2 and keys[0][0] == keys[1][0] and keys[1][1] == keys[0][1] + 1 and not method.startswith(VERBS):
            prop = method[0].upper() + method[1:]
            verbs = []
            for key in keys:
                real = impl[key][0].split('::')[-1]
                named = next((v for v in ('get', 'set') if real.startswith(v + prop)), None)
                if named and role.get(key, named) != named:
                    raise SystemExit(f'{target}: {key[0]}::vf{key[1]} is {impl[key][0]} but its handler {role[key]}s ({cmd})')
                verbs.append(named or role.get(key))
            if sorted(filter(None, verbs)) != ['get', 'set'] and verbs.count(None) != 1: continue
            if None in verbs: verbs[verbs.index(None)] = 'get' if 'set' in verbs else 'set'
            for key, verb in zip(keys, verbs):
                out[key] = (verb + prop, cmd)
    fields = {}
    built = implementers(target)
    for (cls, k), (name, cmd) in out.items():
        off = impl[cls, k][1]
        if off is None or not name.startswith('get') or len(name) < 4: continue
        owners = built.get(cls, [cls]) if cls.startswith('I') else [cls]
        if len(owners) == 1:
            fields[owners[0], off] = (name[3].lower() + name[4:], f'{cls}::vf{k} ({name}) returns it')
    return out, fields


def write(target):
    out, fields = names(target)
    lines = ['# Generated by tools/console_names.py from the console commands; do not edit.',
             '# Put corrections in names.txt, which comment_l2.py reads after this file.', '']
    for (cls, k), (name, cmd) in sorted(out.items()):
        lines.append(f'{cls}::vf{k} = {name}: called by console command {cmd}')
    lines.append('')
    for (cls, off), (name, why) in sorted(fields.items()):
        lines.append(f'{cls}+{off:#x} = {name}: {why}')
    path = PROJECT / 'notes' / target / 'names-console.txt'
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    print(f'{target}: {len(out)} methods and {len(fields)} fields in {path.relative_to(PROJECT)}')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--target', choices=('client', 'server', 'both'), default='both')
    args = p.parse_args()
    for t in (('client', 'server') if args.target == 'both' else (args.target,)):
        write(t)


if __name__ == '__main__':
    main()
