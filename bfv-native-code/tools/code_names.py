"""Name fields from what the machine code shows about the object layout.

    uv run --with pefile --with capstone python bfv-native-code/tools/code_names.py --target both

Writes notes/<target>/names-code.txt for tools/comment_l2.py (read before
names-console.txt and names.txt, so both of those win). Three kinds of
evidence, each written with its source address:

- Base parts: MSVC RTTI lists every non-virtual base class of a class with its
  offset in the object (base class descriptor `mdisp`). A base at +0x4 means
  the object holds that base's part there (AISettings+0x4 =
  part_IAISettingsModifier). Bases at +0 are CLASSES.tsv's chain already.
- Created objects: a method of class C calls operator new and then X's
  constructor (the function that stores X's vtable) and stores the result in
  this+N, so C+N points to an X (p_X). Only when every such store agrees.
- Accessors: a named method (getViewDistance, setGravity, isAiLevel) of the
  class whose body only returns or stores this+N names that field
  (viewDistance, gravity, isAiLevel). Setters named is/has are skipped.

A (class, offset) that already has a name in names.txt or names-console.txt
is left out, and classes are keyed by their last name component, so short
names shared by two classes are skipped. Read-only apart from names-code.txt.
"""
import argparse
import collections
import re
import struct
from pathlib import Path

import capstone
import pefile
from capstone import x86_const as X

import slot_names

PROJECT = Path(__file__).resolve().parents[1]
EXE = {'client': 'BfVietnam.exe', 'server': 'bfvietnam_w32ded.exe'}
REGS = {X.X86_REG_EAX: 'eax', X.X86_REG_ECX: 'ecx', X.X86_REG_EDX: 'edx', X.X86_REG_EBX: 'ebx',
        X.X86_REG_ESI: 'esi', X.X86_REG_EDI: 'edi', X.X86_REG_EBP: 'ebp'}
VERB = re.compile(r'(get|set|is|has|Get|Set|Is|Has)([A-Z_]\w*)')


class Image:
    def __init__(self, path):
        self.pe = pefile.PE(str(path))
        self.base = self.pe.OPTIONAL_HEADER.ImageBase
        self.img = self.pe.get_memory_mapped_image()
        self.md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_32); self.md.detail = True

    def dword(self, a, signed=False): return struct.unpack_from('<i' if signed else '<I', self.img, a - self.base)[0]

    def code(self, a, size, count=None):
        out = []
        for i in self.md.disasm(self.img[a - self.base:a - self.base + size], a):
            out.append(i)
            if count and len(out) >= count: break
        return out

    def type_name(self, td):
        raw = self.img[td + 8 - self.base:].split(b'\0', 1)[0].decode('latin-1')
        if not raw.startswith('.?A') or raw.startswith(('.?AV?$', '.?AU?$')): return None
        return '::'.join(reversed(raw[4:-2].split('@')))


def base_parts(im, cn):
    """{(class, offset): (name, text)} for RTTI bases at non-zero offsets."""
    out = {}
    for cls, (vt, _, _) in cn.classes.items():
        if not vt: continue
        chd = im.dword(im.dword(vt - 4) + 16)
        count, bca = im.dword(chd + 8), im.dword(chd + 12)
        best = {}
        for k in range(1, min(count, 128)):
            bcd = im.dword(bca + 4 * k)
            contained, mdisp, pdisp = im.dword(bcd + 4), im.dword(bcd + 8, True), im.dword(bcd + 12, True)
            b = im.type_name(im.dword(bcd))
            if pdisp != -1 or mdisp <= 0 or not b: continue
            if mdisp not in best or contained > best[mdisp][1]: best[mdisp] = (b, contained)
        for off, (b, _) in best.items():
            out[cls, off] = (f'part_{slot_names.short(b)}', f'the {b} base of the object (RTTI of vtable {vt:08x})')
    return out


def created(im, cn):
    """{(class, offset): (name, text)} for `this->N = new X(...)` in methods of the class."""
    ctors = {a: slot_names.short(c) for a, (_, name, c) in cn.index.items()
             if '::' in name and name.rsplit('::', 1)[1] == slot_names.short(c) == slot_names.short(name.rsplit('::', 1)[0])}
    evidence = {int(r.split('\t')[0], 16): r.split('\t')[4] for r in
                (PROJECT / 'lifted' / cn.target / 'INDEX.tsv').read_text(encoding='utf-8').splitlines()[1:]}
    ctors = {a: x for a, x in ctors.items() if evidence.get(a) == 'vtable store'}
    starts = sorted(cn.index)
    found = collections.defaultdict(set)
    for n, a in enumerate(starts):
        _, name, cls = cn.index[a]
        if not cls or '::' not in name: continue
        end = starts[n + 1] if n + 1 < len(starts) else a + 0x400
        ins = im.code(a, min(end - a, 0x2000))
        this = {'ecx'}
        for j, i in enumerate(ins):
            if i.mnemonic == 'call' and i.operands[0].type == X.X86_OP_IMM and i.operands[0].imm in ctors:
                # the store after `new`'s null check joins: jmp / xor eax, eax / mov [this+N], eax
                for k in ins[j + 1:j + 7]:
                    s = f'{k.mnemonic} {k.op_str}'
                    m = re.fullmatch(r'mov dword ptr \[(e[a-z]{2}) \+ (0x[0-9a-f]+|\d+)\], eax', s)
                    if m:
                        if m[1] in this and m[1] != 'ecx':
                            found[slot_names.short(cls), int(m[2], 0)].add((ctors[i.operands[0].imm], k.address))
                        break
                    if s != 'xor eax, eax' and k.mnemonic != 'jmp': break
            # registers holding `this`: ecx at entry and its copies until overwritten
            if (i.mnemonic == 'mov' and len(i.operands) == 2 and i.operands[0].type == X.X86_OP_REG
                    and i.operands[1].type == X.X86_OP_REG and REGS.get(i.operands[1].reg) in this):
                this.add(REGS[i.operands[0].reg]); continue
            written = {REGS[r] for r in i.regs_access()[1] if r in REGS}
            if i.mnemonic == 'call': written |= {'eax', 'ecx', 'edx'}
            this -= written
    out = {}
    for key, v in found.items():
        if len({x for x, _ in v}) != 1: continue
        x = next(iter(v))[0]
        where = ', '.join(f'{a:08x}' for _, a in sorted(v)[:3])
        out[key] = (f'p_{x}', f'points to the {x} object the class creates (new, then the {x} constructor; stored at {where})')
    return out


def accessor(im, a):
    """('get' | 'set', offset) when the body only returns or stores one field of this."""
    s = ' ; '.join(f'{i.mnemonic} {i.op_str}' for i in im.code(a, 32, 3)).strip()
    off = r'\[ecx(?: \+ (0x[0-9a-f]+|\d+))?\]'
    for kind, pattern in (('get', rf'(?:mov|movzx|movsx) (?:eax|al|ax), (?:byte|word|dword) ptr {off} ; ret\b.*'),
                          ('get', rf'fld dword ptr {off} ; ret\b.*'),
                          ('set', rf'mov (?:eax|al|ax|edx|dl), (?:byte |word |dword )?ptr \[esp \+ 4\] ; '
                                  rf'mov (?:byte|word|dword) ptr {off}, (?:eax|al|ax|edx|dl) ; ret 4'),
                          ('set', rf'fld dword ptr \[esp \+ 4\] ; fstp dword ptr {off} ; ret 4')):
        m = re.fullmatch(pattern, s)
        if m: return kind, int(m[1] or '0', 0)
    return None


def accessors(im, cn):
    """{(class, offset): (name, text)} from named one-field getters and setters."""
    found = collections.defaultdict(set)
    def add(cls, method, a, where):
        m = VERB.fullmatch(method)
        acc = m and accessor(im, a)
        if not acc: return
        verb, rest = m.groups()
        if verb.lower() in ('is', 'has') and acc[0] == 'set': return
        if acc[1] < 4 and cn.classes.get(cls, (None,))[0]: return  # the vtable pointer
        field = rest[0].lower() + rest[1:] if verb.lower() in ('get', 'set') else method
        found[slot_names.short(cls), acc[1]].add((field, f'{method} ({where}) {acc[0]}s it'))
    for cls, fs in cn.slots.items():
        for k, a in enumerate(fs):
            if (s := cn.slot(cls, k)): add(cls, s[0], a, f'{slot_names.short(cls)}::vf{k}, {a:08x}')
    in_slots = {a for fs in cn.slots.values() for a in fs}
    for a, (_, name, owner) in cn.index.items():
        if '::' not in name or a in in_slots: continue
        c, method = name.rsplit('::', 1)
        if slot_names.GENERIC.fullmatch(method) or (owner and slot_names.short(owner) != slot_names.short(c)): continue
        full = cn.resolve(c)
        if len(full) == 1: add(full[0], method, a, f'{a:08x}')
    out = {}
    for key, v in found.items():
        if len({f for f, _ in v}) != 1: continue
        out[key] = (next(iter(v))[0], '; '.join(sorted({t for _, t in v})[:2]))
    return out


def write(target):
    cn = slot_names.ClassNames(target)
    im = Image(GAME / EXE[target])
    # names written by hand or from the console commands win; skip what they name
    path = PROJECT / 'notes' / target / 'names-code.txt'
    virt, named = slot_names.read_names(target, skip=path.name)
    unique = {s for s, full in cn.by_short.items() if len(full) == 1}
    sections = [('RTTI base parts', base_parts(im, cn)), ('objects the class creates', created(im, cn)),
                ('one-field getters and setters', accessors(im, cn))]
    lines = ['# Generated by tools/code_names.py from the machine code; do not edit.',
             '# Put corrections in names.txt, which comment_l2.py reads after this file.']
    taken = set()
    counts = []
    for title, found in sections:
        rows = []
        for (cls, off), (name, text) in sorted(found.items(), key=lambda kv: (slot_names.short(kv[0][0]), kv[0][1])):
            c = slot_names.short(cls)
            if c not in unique or (c, off) in named or (c, off) in taken: continue
            taken.add((c, off))
            rows.append(f'{c}+{off:#x} = {name}: {text}')
        lines += ['', f'# ---- {title} ({len(rows)})', *rows]
        counts.append(f'{len(rows)} {title}')
    path.write_text('\n'.join(lines) + '\n', encoding='utf-8', newline='\n')
    print(f'{target}: {", ".join(counts)} in {path.relative_to(PROJECT)}')


GAME = None


def main():
    global GAME
    import os
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument('--target', choices=('client', 'server', 'both'), default='both')
    p.add_argument('--game-dir', default=os.environ.get('BFV_GAME_DIR', PROJECT.parents[1]),
                   help='folder with the original executables')
    args = p.parse_args()
    GAME = Path(args.game_dir)
    for t in (('client', 'server') if args.target == 'both' else (args.target,)):
        write(t)


if __name__ == '__main__':
    main()
