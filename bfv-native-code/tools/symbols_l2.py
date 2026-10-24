"""Names for the addresses in L2 code (tools/lift_l2.py).

A call, jump, global or vtable address that has a name in the notes from
tools/sort_lifted.py is written as that name:

    CALL(5, 0x500c50);  /* BitStream::writeBits */
    eax = M32(0xb3c5d4);  /* g_pPlayerManager */

becomes

    CALL(5, BitStream_writeBits);
    eax = M32(g_pPlayerManager);

The names are `#define`s in readable/runtime/symbols_<target>.h with the same
literal (`#define BitStream_writeBits 0x500c50`), so the compiled code doesn't
change. A name used for two addresses gets the address as a suffix the second
time, and a name that is a runtime identifier or C keyword gets `fn_`.
"""
import re
from capstone import CS_OP_IMM, CS_OP_MEM
from regvars import RESERVED

FUNC = re.compile(r'[A-Za-z_~][\w:~<>,]*')


def ident(note):
    """The identifier for a note, or None (strings, constants, stack slots)."""
    m = re.fullmatch(r'(\w+)\.dll!([\w@?$]+)|(\w+)!([\w@?$]+)', note, re.I)
    if m:return 'imp_' + (m[2] or m[4])
    m = re.fullmatch(r'vtable (\w[\w:]*)', note)
    if m:return 'vtbl_' + m[1]
    m = re.fullmatch(r'typeid\((\w[\w:]*)\)', note)
    if m:return 'typeid_' + m[1]
    if not FUNC.fullmatch(note) or re.fullmatch(r'(arg|var)_[0-9a-f]+|retaddr|sub_[0-9a-f]+', note):return None
    return note.replace('::~', '::dtor_').replace('~', 'dtor_').replace('::', '_')


def clean(x):
    return re.sub(r'\W', '_', x).rstrip('_')


def candidates(notes):
    """Every identifier the notes can produce: regvars must not use them."""
    out = set()
    for note in notes:
        x = ident(note)
        if x:out.add(clean(x))
    return out


class Symbols:
    def __init__(self, base, end):
        self.base, self.end = base, end
        self.by_name = {};self.by_value = {}

    def name(self, note, value):
        x = ident(note)
        if x is None:return None
        x = clean(x)
        if not x or x[0].isdigit() or x in RESERVED:x = 'fn_' + x
        key = (x, value)
        if key in self.by_value:return self.by_value[key]
        y = x if self.by_name.get(x, value) == value else f'{x}_{value:08x}'
        self.by_name.setdefault(y, value);self.by_value[key] = y
        return y

    def apply(self, insn, line, note):
        """line with the address named, and whether the note is now in the code."""
        values = {o.imm & 0xffffffff for o in insn.operands if o.type == CS_OP_IMM}
        values |= {o.mem.disp & 0xffffffff for o in insn.operands if o.type == CS_OP_MEM and not o.mem.base and not o.mem.index}
        values = [v for v in values if self.base <= v < self.end]
        if len(values) != 1:return line, False
        v = values[0];code, sep, rest = line.partition('/*')
        lit = re.compile(rf'\b0x{v:x}\b(?!\w)')
        if not lit.search(code):return line, False
        y = self.name(note, v)
        if y is None:return line, False
        return lit.sub(y, code) + sep + rest, True

    def header(self, target):
        rows = sorted((v, y) for y, v in self.by_name.items())
        return (f'/* Names of {target} addresses used in readable/{target} (tools/symbols_l2.py).\n'
                ' * Each is the original address, so the code compiles as with the number. */\n'
                f'#ifndef BFV_SYMBOLS_{target.upper()}_H\n#define BFV_SYMBOLS_{target.upper()}_H\n'
                + ''.join(f'#define {y} 0x{v:x}\n' for v, y in rows) + '#endif\n')
