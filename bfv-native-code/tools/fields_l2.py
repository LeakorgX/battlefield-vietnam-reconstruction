"""`this->field` in L2 methods (tools/lift_l2.py).

lift_l2 names a field access it has a note for: `M32(this + readBitPos)` with
`#define readBitPos 0x14  /* field offset */` in the function. Here those
offsets are collected per class over all its methods into packed structs
(readable/runtime/layouts_<target>.h):

    struct BitStream {
        uint8_t _0[0x14];
        uint32_t readBitPos;  /* 0x14 */
        ...
    } __attribute__((packed));

and a method that never writes `this` gets the struct view of ecx:

    #define this ((volatile struct BitStream *)(uintptr_t)ecx)
    this->readBitPos += 8;           (was M32(this + readBitPos) += 8)
    v2 = (uint32_t)this;             (other uses of the value)

The member has the access's offset and width, so `this->x` reads and writes
the same bytes as `M32(this + x)`. A name is a member only when every access
to it in the class has one offset and width and no other member overlaps it.
Accesses through other registers keep their offset names.
"""
import re
from collections import defaultdict

FIELD_DEF = re.compile(r'^#define ([A-Za-z_]\w*) (0x[0-9a-f]+)  /\* field offset \*/$', re.M)
THIS_FIELD = re.compile(r'M(8|16|32)\(this \+ ([A-Za-z_]\w*)\)')
ANY_FIELD = re.compile(r'M(?:8|16|32)\(([A-Za-z_]\w*) \+ ([A-Za-z_]\w*)\)')
THIS_WRITE = re.compile(r'\bthis\s*(?:[-+*/%&|^]|<<|>>)?=(?!=)|\bthis\s*(?:\+\+|--)|(?:\+\+|--)\s*this\b'
                        r'|\bSET\w*\(\s*this\b|\bRESTORE\([^)]*\bthis\b')
THIS_VALUE = re.compile(r'\bthis\b(?!->)')
DEFINE = re.compile(r'^#define ([A-Za-z_]\w*)', re.M)
CLASS = re.compile(r'^ \* class: ([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)$', re.M)
NAMED = re.compile(r'/\* [0-9a-f]{8}  ([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)::~?[A-Za-z_]\w*  ')
TYPE = {8: 'uint8_t', 16: 'uint16_t', 32: 'uint32_t'}


def code_part(line):
    return line.split('  /* ')[0]


def method_class(header):
    """The class of a method: its `class:` line, else the name before `::`."""
    m = CLASS.search(header) or NAMED.match(header)
    return m[1] if m else None


def uses(text):
    """[(name, offset, width)] of `M<w>(this + name)` in a method with `this`."""
    if '\n#define this ecx\n' not in text:return []
    offs = {x: int(o, 16) for x, o in FIELD_DEF.findall(text)}
    return [(x, offs[x], int(w)) for w, x in THIS_FIELD.findall(text) if x in offs]


def layouts(methods, reserved):
    """methods: [(header, text)]. {class: [(offset, width, name)]} of the members
    every access agrees on, and {class: struct name}."""
    seen = defaultdict(lambda:defaultdict(set))
    full = defaultdict(set)  # short class name -> full names from class: lines
    for header, text in methods:
        m = CLASS.search(header)
        if m:full[m[1].split('::')[-1]].add(m[1])
    for header, text in methods:
        c = method_class(header)
        if not c:continue
        if '::' not in c and len(full.get(c, ())) == 1:c = next(iter(full[c]))
        for x, off, w in uses(text):seen[c][x].add((off, w))
    out = {}
    for c, names in seen.items():
        members = sorted((next(iter(v))[0], next(iter(v))[1], x) for x, v in names.items() if len(v) == 1 and x not in reserved)
        at = defaultdict(int)
        for off, w, x in members:at[off] += 1
        members = [m for m in members if at[m[0]] == 1]
        keep = []
        for k, (off, w, x) in enumerate(members):
            prev = members[k - 1] if k else None
            nxt = members[k + 1] if k + 1 < len(members) else None
            if prev and prev[0] + prev[1] // 8 > off or nxt and off + w // 8 > nxt[0]:continue
            keep.append((off, w, x))
        # Game.aiLevel (noted as another object's field) in a Game method: aiLevel
        short = c.split('::')[-1] + '_'
        bare = [x[len(short):] if x.startswith(short) and re.fullmatch(r'[A-Za-z_]\w*', x[len(short):])
                and x[len(short):] not in reserved else x for _, _, x in keep]
        keep = [(off, w, x, m if bare.count(m) == 1 else x) for (off, w, x), m in zip(keep, bare)]
        if keep:out[c] = keep
    short = defaultdict(list)
    for c in out:short[c.split('::')[-1]].append(c)
    struct = {c:c.split('::')[-1] if len(short[c.split('::')[-1]]) == 1 else c.replace('::', '_') for c in out}
    return out, struct


def header(target, out, struct):
    lines = [f'/* Class layouts of {target} methods for `this->field` (tools/fields_l2.py):',
             ' * the fields L2 methods access through `this`, at their offset and width. */',
             f'#ifndef BFV_LAYOUTS_{target.upper()}_H', f'#define BFV_LAYOUTS_{target.upper()}_H', '']
    for c in sorted(out, key=lambda c:struct[c]):
        lines.append(f'/* {c} */')
        lines.append(f'struct {struct[c]} {{')
        at = 0
        for off, w, x, m in out[c]:
            if off > at:lines.append(f'    uint8_t _{at:x}[{off - at:#x}];')
            lines.append(f'    {TYPE[w]} {m};  /* {off:#x} */')
            at = off + w // 8
        lines += ['} __attribute__((packed));', '']
    lines += ['#endif', '']
    return '\n'.join(lines)


def rewrite(header, text, out, struct, full):
    """text with `this->x` for the members of its class, or text unchanged."""
    c = method_class(header)
    if not c:return text
    if '::' not in c and len(full.get(c, ())) == 1:c = next(iter(full[c]))
    if c not in out or '\n#define this ecx\n' not in text:return text
    lines = text.split('\n')
    if '    L2_BEGIN;' not in lines or '    L2_END;' not in lines:return text
    k0, k1 = lines.index('    L2_BEGIN;'), lines.index('    L2_END;')
    body = [l for l in lines[k0 + 1:k1] if not l.startswith('#') and not l.lstrip().startswith('/*')]
    if any(THIS_WRITE.search(code_part(l)) for l in body):return text
    member = {x:(off, w, m) for off, w, x, m in out[c]}
    macros = set(DEFINE.findall(text))
    offs = {x:int(o, 16) for x, o in FIELD_DEF.findall(text)}
    other = {x for b, x in ANY_FIELD.findall('\n'.join(map(code_part, body))) if b != 'this'}
    used = {x for w, x in THIS_FIELD.findall(text)}
    conv = {x for x in used if x in member and offs.get(x) == member[x][0] and x not in other
            and (member[x][2] == x or member[x][2] not in macros)
            and all(int(w) == member[x][1] for w, y in THIS_FIELD.findall(text) if y == x)}
    if not conv:return text
    res = []
    for n, l in enumerate(lines):
        if l == '#define this ecx':res.append(f'#define this ((volatile struct {struct[c]} *)(uintptr_t)ecx)');continue
        d = FIELD_DEF.match(l)
        if d and d[1] in conv:continue
        if l.startswith('#undef ') and l[7:] in conv:continue
        if l.startswith('#') or not k0 < n < k1 or l.lstrip().startswith('/*'):res.append(l);continue
        code, sep, note = l.partition('  /* ')
        code = THIS_FIELD.sub(lambda m:f'this->{member[m[2]][2]}' if m[2] in conv else m[0], code)
        code = THIS_VALUE.sub('(uint32_t)this', code)
        res.append(code + sep + note)
    return '\n'.join(res)


def full_names(methods):
    full = defaultdict(set)
    for header, text in methods:
        m = CLASS.search(header)
        if m:full[m[1].split('::')[-1]].add(m[1])
    return full
