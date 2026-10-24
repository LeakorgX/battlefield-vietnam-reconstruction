"""Names of the game's virtual methods (vtable slots) and fields, per class.

    python bfv-native-code/tools/slot_names.py FireArms               # slots and fields of one class
    python bfv-native-code/tools/slot_names.py --stats --target server

Used by find.py --class, comment_l2.py and the mod build (mods.py). Sources:

- lifted/<target>/CLASSES.tsv and VTABLES.tsv (tools/sort_lifted.py
  --classes-only): every class with a vtable, its bases at offset 0 and the
  function in each slot.
- A slot is one virtual method for a class and every class built on it. When
  the index (INDEX.tsv) gives one of the implementations of that method a
  real name in its own class (Object::update in slot 20 of Object), every
  class's slot 20 is `update`. Implementations the linker shares with other
  classes are not evidence; two different names leave the slot unnamed.
- notes/<target>/names-*.txt (generated) and names.txt (by hand, read last, so
  it wins): `Class::vfN = name: text` and `Class+0x1c = name: text`. These hold
  for the class and its subclasses.

Read-only.
"""
import argparse
import collections
import re
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
NAME_LINE = re.compile(r'([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*?)(?:::vf(\d+)|\+(0x[0-9a-fA-F]+))\s*=\s*([A-Za-z_]\w*)\s*(?::\s*(.*))?')
# Index names that say nothing about the method: vf12, sub_..., helpers,
# compiler-made thunks and destructors, accessors named after their offset.
GENERIC = re.compile(r'vf\d+.*|sub_.*|unknown_.*|.*_[0-9a-f]{8}|.*helper.*|stub_.*|.*adjustor.*|deleting_dtor.*|~.*'
                     r'|purecall.*|(?:get|set)\w*_[0-9a-f]+h|ctor_dtor')


def short(cls): return cls.split('::')[-1]


def read_names(target, skip=None):
    """({(short class, slot): (name, text, file)}, {(short class, offset): ...})
    from notes/<target>/names-*.txt, then names.txt (later lines win); `skip`
    leaves out one file (a generator reading the names besides its own)."""
    virt, fields = {}, {}
    folder = PROJECT / 'notes' / target
    for path in sorted(folder.glob('names-*.txt')) + [folder / 'names.txt']:
        if not path.exists() or path.name == skip: continue
        for n, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
            line = line.strip()
            if not line or line.startswith('#'): continue
            m = NAME_LINE.fullmatch(line)
            if not m: raise SystemExit(f'{path}:{n}: expected "Class::vfN = name: text" or "Class+0x10 = name: text"')
            cls, vf, off, name, text = m.groups()
            key = (short(cls), int(vf) if vf else int(off, 16))
            (virt if vf else fields)[key] = (name, (text or '').strip(), path.name)
    return virt, fields


class ClassNames:
    def __init__(self, target):
        self.target = target
        lifted = PROJECT / 'lifted' / target
        self.classes = {}  # full name: (vtable, slot count, [bases at offset 0, nearest first])
        for row in (lifted / 'CLASSES.tsv').read_text(encoding='utf-8').splitlines()[1:]:
            cls, vt, n, bases = row.split('\t')
            self.classes[cls] = (int(vt, 16) if vt else None, int(n or 0), [b for b in bases.split('; ') if b])
        self.by_short = collections.defaultdict(list)
        for cls in self.classes: self.by_short[short(cls)].append(cls)
        self.index = {}  # address: (file, name, class)
        for row in (lifted / 'INDEX.tsv').read_text(encoding='utf-8').splitlines()[1:]:
            c = row.split('\t')
            self.index.setdefault(int(c[0], 16), (c[1], c[2], c[3] if len(c) > 3 else ''))
        self.slots = collections.defaultdict(list)  # full class: [function in slot k]
        path = lifted / 'VTABLES.tsv'
        if path.exists():
            for row in path.read_text(encoding='utf-8').splitlines()[1:]:
                cls, k, a = row.split('\t')
                self.slots[cls].append(int(a, 16))
        self.virt, self.fields = read_names(target)
        # Fields FIELDS.tsv ties to a class: what a console command or a menu
        # / serialization table reads or writes (getActiveTemplateAs(C) is a C).
        self.settings = collections.defaultdict(dict)  # short class: {offset: commands}
        path = lifted / 'FIELDS.tsv'
        if path.exists():
            for row in path.read_text(encoding='utf-8').splitlines()[1:]:
                obj, off, kind, cmds = (row.split('\t') + [''] * 4)[:4]
                if kind.startswith('vf'): continue
                m = re.fullmatch(r'getActiveTemplateAs\(([\w:]+)\)|([A-Za-z_]\w*)', obj)
                if m and (c := short(m.group(1) or m.group(2))) in self.by_short:
                    self.settings[c][int(off, 16)] = f'{cmds} ({kind})'
        self._groups()

    def _groups(self):
        """Name each slot after the method its implementations are named after."""
        parent = {}
        def find(x):
            while parent.setdefault(x, x) != x: parent[x] = parent[parent[x]]; x = parent[x]
            return x
        for cls, (_, n, bases) in self.classes.items():
            for b in bases:
                for k in range(min(n, self.classes.get(b, (0, 0))[1])): parent[find((cls, k))] = find((b, k))
        seen = collections.defaultdict(lambda: collections.defaultdict(set))  # group: method: implementations
        for cls, fs in self.slots.items():
            for k, a in enumerate(fs):
                _, name, owner = self.index.get(a, ('', '', ''))
                if '::' not in name: continue
                cname, meth = name.rsplit('::', 1)
                # a body the linker shares with another class (Object's slot 31 is
                # PlayerManager::getLocalPlayer's `mov eax, [ecx+0x54]`) is not evidence
                if short(cname) not in {short(cls)} | {short(b) for b in self.classes[cls][2]} \
                        and (owner != cls or short(cname) in self.by_short): continue
                if GENERIC.fullmatch(meth) or meth == short(cname): continue
                seen[find((cls, k))][meth].add(a)
        self.group_name = {}
        for cls, (_, n, _) in self.classes.items():
            for k in range(n):
                ms = seen.get(find((cls, k)))
                if ms and len(ms) == 1:
                    meth, impls = next(iter(ms.items()))
                    self.group_name[cls, k] = (meth, ', '.join(f'{a:08x}' for a in sorted(impls)[:3]))

    def resolve(self, cls):
        """Full class names for a short or full name."""
        if cls in self.classes: return [cls]
        return self.by_short.get(short(cls), [])

    def chain(self, cls):
        """The class and its bases, nearest first, as short names."""
        full = self.resolve(cls)
        out = [short(cls)]
        for b in (self.classes[full[0]][2] if full else []):
            if short(b) not in out: out.append(short(b))
        return out

    def slot(self, cls, k):
        """(name, where it comes from) of slot k of the class, or None."""
        for c in self.chain(cls):
            if (c, k) in self.virt:
                name, _, src = self.virt[c, k]
                return name, f'{src}: {c}::vf{k}'
        for full in self.resolve(cls):
            if (full, k) in self.group_name:
                name, impls = self.group_name[full, k]
                return name, f'named implementation {impls}'
        return None

    def field(self, cls, off):
        for c in self.chain(cls):
            if (c, off) in self.fields: return self.fields[c, off]
        return None

    def class_fields(self, cls):
        """{offset: (name, text, file)} named for the class or its bases."""
        out = {}
        for c in reversed(self.chain(cls)):
            out.update({off: v for (cc, off), v in self.fields.items() if cc == c})
        return out


def describe(cn, cls, locate=None):
    """Print the slots and fields of a class; locate(address, file) gives
    where to show a function (default: its file in lifted/<target>/)."""
    full = cn.resolve(cls)
    if not full: raise SystemExit(f'{cls}: no such class in lifted/{cn.target}/CLASSES.tsv')
    for f in full:
        vt, n, bases = cn.classes[f]
        print(f'{f}' + (f'  vtable {vt:08x}, {n} slots' if vt else '  (no vtable)'))
        if bases: print('  bases: ' + ', '.join(bases))
        for k, a in enumerate(cn.slots.get(f, [])):
            file, name, _ = cn.index.get(a, ('', '(not lifted)', ''))
            s = cn.slot(f, k)
            print(f'  vf{k:<3} {a:08x}  {name}' + (f'  = {s[0]}' if s and s[0] != name.rsplit('::', 1)[-1] else '') + (f'  [{locate(a, file) if locate else file}]' if file else ''))
        named = cn.class_fields(f)
        settings = {off: cmds for c in reversed(cn.chain(f)) for off, cmds in cn.settings.get(c, {}).items()}
        for off in sorted(set(named) | set(settings)):
            if off in named:
                name, text, _ = named[off]
                print(f'  +{off:#x} {name}' + (f': {text}' if text else ''))
            else:
                print(f'  +{off:#x} {settings[off]}')


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('cls', nargs='?')
    p.add_argument('--target', choices=('client', 'server'), default='client')
    p.add_argument('--stats', action='store_true')
    a = p.parse_args()
    cn = ClassNames(a.target)
    if a.stats:
        slots = sum(n for _, n, _ in cn.classes.values())
        named = sum(1 for c, (_, n, _) in cn.classes.items() for k in range(n) if cn.slot(c, k))
        print(f'{a.target}: {len(cn.classes)} classes, {slots} slots, {named} named; {len(cn.fields)} named fields')
    if a.cls: describe(cn, a.cls)


if __name__ == '__main__':
    main()
