"""Names recovered from the game's own data, for sort_lifted.py.

- Class IDs: `.rdata` constants returned by one-line virtual methods
  (`mov eax, [id]; ret`); the owning RTTI class names the constant.
- Console categories: static initializers call a setter with the category name
  ("ObjectTemplate", "Game", ...) before they construct that file's commands;
  each command constructor copies the current category. Replaying the CRT
  initializer table in order gives every command its full name.
- Fields: a small symbolic evaluator follows registers through a function
  (globals, call results, loads, stack spills). In console handlers the value
  returned for a "get" or stored for a "set" names an object field; the same
  object expression in any other function names the same field.
"""
import bisect
import re
import collections
import struct

from capstone import CS_ARCH_X86, CS_MODE_32, Cs
from capstone.x86 import X86_OP_IMM, X86_OP_MEM, X86_OP_REG

SAVED = ('ebx', 'esi', 'edi', 'ebp', 'esp')


def class_ids(im, owners, md):
    """{id global: class} from virtual methods that only return the global."""
    lo, hi = im.sections['.rdata']
    found = collections.defaultdict(set)
    for f, (cls, *_) in owners.items():
        ins = list(md.disasm(im.img[f - im.base:f - im.base + 16], f))[:2]
        if len(ins) == 2 and ins[1].mnemonic == 'ret' and ins[0].mnemonic == 'mov' and ins[0].op_str.startswith('eax, dword ptr [0x'):
            v = ins[0].operands[1].mem.disp
            if lo <= v < hi: found[v].add(cls)
    return {v: c.pop() for v, c in found.items() if len(c) == 1}


def initializer_table(im):
    """(start, end) of the run of .text pointers in .rdata/.data with the most
    pointers (zero gaps of up to 3 words allowed): the CRT initializer table."""
    tl, th = im.sections['.text']
    best, score = (0, 0), 0
    for sec in ('.rdata', '.data'):
        lo, hi = im.sections.get(sec, (0, 0))
        words = struct.unpack_from(f'<{(hi - lo) // 4}I', im.img, lo - im.base) + (1,)
        run, n, zeros = None, 0, 0
        for k, w in enumerate(words):
            if tl <= w < th:
                if run is None: run, n = k, 0
                n += 1; zeros = 0; last = k
            elif w == 0 and run is not None and zeros < 3: zeros += 1
            else:
                if run is not None and n > score: best, score = (lo + 4 * run, lo + 4 * last + 4), n
                run, zeros = None, 0
    return best


def console_categories(im, md, ctors, starts):
    """{command constructor: category} by replaying the static initializers in order.
    starts: sorted function entry addresses."""
    # The category global: what a command constructor copies into the object at +8.
    glob = collections.Counter()
    for c in list(ctors)[:50]:
        for i in md.disasm(im.img[c - im.base:c - im.base + 24], c):
            if i.mnemonic == 'mov' and i.op_str.startswith('eax, dword ptr [0x'): glob[i.operands[1].mem.disp] += 1; break
    if not glob: return {}
    needle = struct.pack('<I', glob.most_common(1)[0][0])
    tl, th = im.sections['.text']
    setter, o = None, im.img.find(needle, tl - im.base)
    while 0 <= o < th - im.base:  # the function with `mov dword ptr [global], reg`
        if im.img[o - 2] == 0x89 and im.img[o - 1] & 0xc7 == 0x05:
            k = bisect.bisect_right(starts, im.base + o) - 1
            if k >= 0: setter = starts[k]
            break
        o = im.img.find(needle, o + 1)
    start, end = initializer_table(im)
    cur, out = None, {}
    for p in range(start, end, 4):
        f = im.dword(p)
        if not f: continue
        last = None
        for i in md.disasm(im.img[f - im.base:f - im.base + 4000], f):
            if i.mnemonic == 'ret': break
            if i.mnemonic == 'push' and i.operands[0].type == X86_OP_IMM: last = i.operands[0].imm
            elif i.mnemonic == 'call' and i.operands[0].type == X86_OP_IMM:
                t = i.operands[0].imm
                if t == setter and last:
                    n = im.string(last); cur = n[:1].upper() + n[1:] if n else None
                elif t in ctors and cur: out.setdefault(t, cur)
                last = None
    return out


# Symbolic values: ('this',) entry ecx; ('arg', off) stack argument;
# ('abs', va) address of a global; ('add', base, off); ('idx', base, off) array element;
# ('load', addr); ('call', target, ecx); ('ret', addr) result of an indirect call;
# ('imm', v); ('sp', off) stack pointer.
THIS = ('this',)


def _mem(i, op, st):
    m = op.mem
    if m.segment: return None
    b = st.get(i.reg_name(m.base)) if m.base else None
    if m.base and b is None: return None
    if b is None: return None if m.index else ('abs', m.disp & 0xffffffff)
    if b[0] == 'imm': return None
    if m.index: return ('idx', b, m.disp)
    if b[0] == 'add': return ('add', b[1], b[2] + m.disp)
    if b[0] == 'sp': return ('slot', b[1] + m.disp)
    return ('add', b, m.disp)


def evaluate(insns):
    """[(address, event, location, value)] for one function: 'read', 'write' and
    'vcall' events, and ('call', target, ecx) for direct calls."""
    st, slots, fp, ev = {'ecx': THIS, 'esp': ('sp', 0)}, {}, [], []
    saved, dead = {}, False
    for i in insns:
        m, ops, a = i.mnemonic, i.operands, i.address
        if dead or a in saved and not st:
            st, slots = saved.get(a, ({}, {})); st, slots, fp, dead = dict(st), dict(slots), [], False
        if m in ('ret', 'jmp'): dead = True; continue
        if m.startswith('j') or m.startswith('loop'):
            if ops and ops[0].type == X86_OP_IMM: saved.setdefault(ops[0].imm, (dict(st), dict(slots)))
            continue
        sp = st.get('esp')
        if m == 'push' and sp:
            o = ops[0]
            v = st.get(i.reg_name(o.reg)) if o.type == X86_OP_REG else ('imm', o.imm) if o.type == X86_OP_IMM else None
            if o.type == X86_OP_MEM:
                x = _mem(i, o, st)
                if x and x[0] != 'slot': ev.append((a, 'read', x, None)); v = ('load', x)
            st['esp'] = ('sp', sp[1] - 4)
            if v: slots[sp[1] - 4] = v
            else: slots.pop(sp[1] - 4, None)
            continue
        if m == 'pop' and sp:
            st['esp'] = ('sp', sp[1] + 4)
            if ops[0].type == X86_OP_REG:
                r = i.reg_name(ops[0].reg)
                if sp[1] in slots: st[r] = slots[sp[1]]
                else: st.pop(r, None)
            continue
        if m in ('sub', 'add') and sp and ops[0].type == X86_OP_REG and i.reg_name(ops[0].reg) == 'esp' and ops[1].type == X86_OP_IMM:
            st['esp'] = ('sp', sp[1] + (ops[1].imm if m == 'add' else -ops[1].imm)); continue
        if m == 'call':
            o, v = ops[0], None
            if o.type == X86_OP_IMM: v = ('call', o.imm, st.get('ecx')); ev.append((a, 'call', o.imm, st.get('ecx')))
            elif o.type == X86_OP_MEM:
                x = _mem(i, o, st)
                if x: ev.append((a, 'vcall', x, st.get('ecx'))); v = ('ret', x)
            st = {k: w for k, w in st.items() if k in SAVED}
            if v: st['eax'] = v
            fp = []
            continue
        if m in ('mov', 'lea', 'movzx', 'movsx') and ops[0].type == X86_OP_REG:
            d, s = i.reg_name(ops[0].reg), ops[1]
            if ops[0].size != 4: st.pop(d, None); continue
            if s.type == X86_OP_REG: v = st.get(i.reg_name(s.reg))
            elif s.type == X86_OP_IMM: v = ('imm', s.imm)
            else:
                x = _mem(i, s, st)
                if x is None: v = None
                elif m == 'lea': v = ('sp', x[1]) if x[0] == 'slot' else x
                elif x[0] == 'slot':
                    v = slots.get(x[1])
                    if v is None and sp and x[1] >= 4: v = ('load', ('arg', x[1] - 4))
                else: v = ('load', x); ev.append((a, 'read', x, None))
            if v is None: st.pop(d, None)
            else: st[d] = v
            continue
        if m == 'mov' and ops[0].type == X86_OP_MEM:
            x, s = _mem(i, ops[0], st), ops[1]
            v = st.get(i.reg_name(s.reg)) if s.type == X86_OP_REG else ('imm', s.imm)
            if x is not None and x[0] == 'slot':
                if v and ops[0].size == 4: slots[x[1]] = v
                else: slots.pop(x[1], None)
            elif x is not None: ev.append((a, 'write', x, v))
            continue
        if m.startswith(('fld', 'fild')) and ops and ops[0].type == X86_OP_MEM:
            x = _mem(i, ops[0], st)
            fp.append(('load', x) if x and x[0] != 'slot' else slots.get(x[1]) if x else None)
            if x and x[0] != 'slot': ev.append((a, 'read', x, None))
            continue
        if m.startswith('fst') and ops and ops[0].type == X86_OP_MEM:
            x = _mem(i, ops[0], st); v = fp[-1] if fp else None
            if m.endswith('p') and fp: fp.pop()
            if x is not None and x[0] == 'slot':
                if v: slots[x[1]] = v
            elif x is not None: ev.append((a, 'write', x, v))
            continue
        if m == 'xor' and len(ops) == 2 and ops[0].type == ops[1].type == X86_OP_REG and ops[0].reg == ops[1].reg:
            st[i.reg_name(ops[0].reg)] = ('imm', 0); continue
        for o in ops:  # any other memory operand is still an access to it
            if o.type == X86_OP_MEM and m not in ('lea', 'nop'):
                x = _mem(i, o, st)
                if x and x[0] != 'slot': ev.append((a, 'read', x, None))
        try: written = i.regs_access()[1]
        except Exception: written = []
        for r in written:
            n = i.reg_name(r)
            if n == 'esp': st.pop('esp', None)
            st.pop({'ax': 'eax', 'al': 'eax', 'cx': 'ecx', 'cl': 'ecx', 'dx': 'edx', 'dl': 'edx'}.get(n, n), None)
    return ev


def _self(x):
    return x == THIS or isinstance(x, tuple) and len(x) > 1 and x[0] in ('add', 'load', 'idx') and _self(x[1])


ARGS = ('load', ('add', THIS, 0x20))  # the command's argument list


def _from_arg(v):
    return isinstance(v, tuple) and v[0] == 'load' and isinstance(v[1], tuple) and v[1][0] == 'add' and v[1][1] == ARGS


def _fixed(x):
    """True for object expressions that do not depend on the function's own arguments or stack."""
    if not isinstance(x, tuple): return True
    if x[0] in ('this', 'arg', 'sp', 'slot'): return False
    return all(_fixed(y) for y in x[1:] if isinstance(y, tuple))


def console_fields(handlers, insns_of):
    """{(object, offset): {command, ...}}, {(object, slot): {command, ...}} and
    {global variable: {command, ...}} from command handlers.

    handlers: {command full name: [handler address]}."""
    fields, methods, variables = collections.defaultdict(set), collections.defaultdict(set), collections.defaultdict(set)
    for cmd, hs in handlers.items():
        for h in hs:
            for a, kind, x, v in evaluate(insns_of(h)):
                if kind == 'write':
                    if x[0] == 'add' and x[1] == THIS and x[2] >= 0xa4 and v and v[0] == 'load' and v[1][0] == 'add' and _fixed(v[1][1]):
                        fields[v[1][1], v[1][2]].add(cmd)  # get: result <- [object + offset]
                    elif x[0] == 'add' and x[1] == THIS and x[2] >= 0xa4 and v and v[0] == 'load' and v[1][0] == 'abs':
                        variables[v[1][1]].add(cmd)  # get: result <- [global]
                    elif x[0] == 'abs' and _from_arg(v):
                        variables[x[1]].add(cmd)  # set: [global] <- argument
                    elif x[0] == 'add' and _fixed(x[1]) and _from_arg(v):
                        fields[x[1], x[2]].add(cmd)  # set: [object + offset] <- argument
                elif kind == 'vcall' and x[0] == 'add' and x[1][0] == 'load' and x[1][1][0] == 'add' and x[1][1][2] == 0:
                    obj = x[1][1][1]
                    if _fixed(obj) and v == obj: methods[obj, x[2]].add(cmd)
    return fields, methods, variables


def describe(obj, ids, names, objects):
    """Short readable name of an object expression."""
    k = obj[0]
    if k == 'call':
        _, t, ecx = obj
        base = names.get(t, f'sub_{t:08x}')
        if ecx and ecx[0] == 'load' and ecx[1][0] == 'abs' and ecx[1][1] in ids: return f'{base}({ids[ecx[1][1]]})'
        if ecx and ecx[0] == 'abs': return f'{describe(ecx, ids, names, objects)}.{base}()'
        return f'{base}()'
    if k == 'abs': return objects.get(obj[1], f'0x{obj[1]:08x}')
    if k == 'load':
        x = obj[1]
        if x[0] == 'add' and x[2]: return f'{describe(x[1], ids, names, objects)}->+0x{x[2]:x}'
        if x[0] == 'add': return f'*{describe(x[1], ids, names, objects)}'
        return f'*{describe(x, ids, names, objects)}'
    if k == 'add': return f'{describe(obj[1], ids, names, objects)}+0x{obj[2]:x}'
    if k == 'idx': return f'{describe(obj[1], ids, names, objects)}[i]' + (f'{obj[2]:+#x}' if obj[2] else '')
    if k == 'ret':
        x = obj[1]
        if x[0] == 'add' and x[1][0] == 'load' and x[1][1][0] == 'add' and x[1][1][2] == 0:
            return f'{describe(x[1][1][1], ids, names, objects)}.vf{x[2] // 4}()'
        return 'result'
    if k == 'imm': return f'0x{obj[1]:x}'
    return k


def class_bases(im):
    """{class: {classes at offset 0 of it, itself included}} from the RTTI class hierarchy descriptors."""
    tds = {}
    for m in re.finditer(rb'\.\?A[VU][ -~]+?@@\x00', im.img):
        tds[im.base + m.start() - 8] = '::'.join(reversed(m.group()[4:-3].decode().split('@')))
    lo, hi = im.sections['.rdata']
    words = struct.unpack_from(f'<{(hi - lo) // 4}I', im.img, lo - im.base)
    out = {}
    for i in range(len(words) - 5):
        if words[i] == 0 and words[i + 3] in tds and tds[words[i + 3]] not in out:
            chd = words[i + 4]
            if not im.inside(chd, '.rdata'): continue
            n, arr = im.dword(chd + 8), im.dword(chd + 12)
            out[tds[words[i + 3]]] = {tds.get(im.dword(im.dword(arr + 4 * k)), '?') for k in range(min(n, 64))
                                      if im.dword(im.dword(arr + 4 * k) + 8) == 0} - {'?'}
    return out
