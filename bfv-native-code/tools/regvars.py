"""Register values as C variables (L2b).

A register value that no call, `ret` or exit can see is renamed to its own
variable (`v1`, `v2`, ...). Values the outside sees keep the register name:

- at entry every register is the caller's (pinned);
- at a call, the callee's input registers (tools/reg_abi.py), the callee-saved
  registers that flow through it, and after it every register (the body
  resumes from the guest state);
- at `ret`, eax, the callee-saved registers and the ecx/edx some caller reads;
- wherever the body leaves to native code (EXIT_TO), every register.

A web is the set of definitions and uses of one register connected by
reaching definitions; an instruction that reads and writes a register (also a
partial write) keeps one web. Renaming a web that touches no pinned point
can't change what calls, `ret` or native code see.

Every web gets a name from where its value comes from (`this`, `result`, `p1`,
`r_getLocalPlayer`, `vtbl`, `this_endBit`, `playerManager`, else `v1`, ...).
A pinned web's name is a `#define` of its register for the function, so it
changes no code.
"""
import re
from pathlib import Path
from capstone import CS_OP_REG
from reg_abi import ORDER, BIT, REGS, access, whole

NAMES = [r for r in ORDER if r != 'esp']
EAX_BIT = BIT['eax']
# runtime locals and macros (l1.h, l2.h), C keywords and the register names
RESERVED = set('''eax ecx edx ebx esp ebp esi edi c t a b r n p u c0 ea lhs rhs cf pf af zf sf df of eflags_rest out
    auto break case char const continue default do double else enum extern float for goto if inline int long register
    restrict return short signed sizeof static struct switch typedef union unsigned void volatile while
    M8 M16 M32 PUSH POP CALL RET EXIT_TO FPU FPU_MEM INTEL LO8 HI8 LO16 SET8L SET8H SET16 SIGN L2_FN L2_PREFIX frame ADDR'''.split())
# names are also `#define`s, so no identifier of the runtime headers may be one
_RT = Path(__file__).resolve().parent.parent / 'readable' / 'runtime'
for _h in ('l1.h', 'l2.h'):RESERVED |= set(re.findall(r'[A-Za-z_]\w*', re.sub(r'/\*.*?\*/|//[^\n]*', '', (_RT / _h).read_text(encoding='utf-8'), flags=re.S)))
AVOID = set()  # address names (symbols_l2.py) in use as #defines
WORD = re.compile(r'\b(eax|ecx|edx|ebx|ebp|esi|edi)\b')
PLUS = re.compile(r'"\+\w"\((\w+)\)')
CODE = re.compile(r'("(?:[^"\\]|\\.)*"|/\*.*?\*/)')  # strings and comments are kept as they are


def mentioned(lines):
    """(registers named in code, registers named inside string literals)."""
    code = text = 0
    for l in lines:
        for k, part in enumerate(CODE.split(l)):
            if k % 2 == 0:
                for w in WORD.findall(part):code |= BIT[w]
            elif part.startswith('"'):
                for w in WORD.findall(part):text |= BIT[w]
    return code, text


def substitute(line, names):
    parts = CODE.split(line)
    for k in range(0, len(parts), 2):
        parts[k] = WORD.sub(lambda m: names.get(m.group(1), m.group(1)), parts[k])
    return ''.join(parts)


def bits(x):
    while x:
        b = x & -x;yield b.bit_length() - 1;x ^= b


def rename(insns, chunks, entry, succ, abi, ret_set, post_exit, restart, notes=None, keeps=None):
    """chunks[n]: the C lines of instruction n. succ[n]: successor indices in
    the body. abi(i) -> pinned-use mask of a call instruction. post_exit[n]: the
    body leaves to native code after n; restart[n]: and is re-entered.
    notes[n]: the comment of instruction n (names for the values it defines).
    keeps(i) -> registers a call instruction's callee leaves as they were.
    Returns (new chunks, variable names, {call address: pinned-use mask},
    [(alias, register)], [{register: name} per instruction]): a value the
    outside sees still lives in its register, and its name is a `#define` of
    that register."""
    N = len(insns)
    use = [0] * N;dfn = [0] * N;said = [0] * N;calls_at = [False] * N;pin_use = [0] * N;pin_def = [0] * N;calls = {}
    for n, i in enumerate(insns):
        rd, wr = access(i)
        u = whole(rd) & REGS
        full = sum(BIT[r] for k, r in enumerate(ORDER) if wr >> 3 * k & 7 == 7)
        part = whole(wr) & ~full & REGS
        d = (full | part) & REGS
        code, text = mentioned(chunks[n]);said[n] = code
        only = code & ~(u | d)
        for w in PLUS.findall(' '.join(chunks[n])):  # asm "+r"(reg): read as well
            if w in BIT:only |= BIT[w] & d
        u |= part | only  # partial writes and text-only mentions (fused conditions) read
        d |= only         # ... and write: one web
        if i.mnemonic in ('xor', 'sub') and len(i.operands) == 2 and i.operands[0].type == i.operands[1].type == CS_OP_REG \
                and i.operands[0].reg == i.operands[1].reg and not part:
            u &= ~d  # `r = 0;`
        use[n] = u;dfn[n] = d
        lines = ' '.join(chunks[n])
        if text:pin_use[n] |= text;pin_def[n] |= text;use[n] |= text;dfn[n] |= text
        try:i.regs_access()
        except Exception:pin_use[n] |= REGS;pin_def[n] |= REGS;use[n] |= REGS;dfn[n] |= REGS
        calls_at[n] = 'CALL(' in lines
        if 'CALL(' in lines:
            m = REGS if 'EXIT_TO(' in lines else abi(i);calls[i.address + i.size] = m
            k = 0 if 'EXIT_TO(' in lines or not keeps else keeps(i)
            pin_use[n] |= m;use[n] |= m;dfn[n] |= REGS & ~k;pin_def[n] |= REGS & ~k
        if 'RET(' in lines:pin_use[n] |= ret_set;use[n] |= ret_set
        if 'EXIT_TO(' in lines:pin_use[n] |= REGS;use[n] |= REGS;pin_def[n] |= dfn[n]
        # defined-not-written: a def the text doesn't spell stays the register
        pin_def[n] |= d & ~code
    # def ids: 7 entry defs, then (n, r), then restart re-entry (n, r)
    R = len(NAMES);ids = {}
    def did(n, r):
        k = (n, r)
        if k not in ids:ids[k] = len(ids) + R
        return ids[k]
    for n in range(N):
        for r in range(R):
            if dfn[n] & BIT[NAMES[r]]:did(n, r)
            if restart[n]:did(('post', n), r)
    total = len(ids) + R
    parent = list(range(total));pinned = [False] * total
    for r in range(R):pinned[r] = True
    for (n, r), k in ids.items():
        if isinstance(n, tuple) or pin_def[n] & BIT[NAMES[r]]:pinned[k] = True

    def find(x):
        while parent[x] != x:parent[x] = parent[parent[x]];x = parent[x]
        return x

    def union(a, b):
        a = find(a);b = find(b)
        if a != b:parent[b] = a;pinned[a] = pinned[a] or pinned[b]

    # reaching definitions
    IN = [None] * N;OUT = [None] * N
    start = [1 << r for r in range(R)]
    preds = [[] for _ in range(N)]
    for n in range(N):
        for s in succ[n]:preds[s].append(n)

    def out_of(n, inn):
        o = list(inn)
        for r in range(R):
            if restart[n]:o[r] = 1 << ids[(('post', n), r)]
            elif dfn[n] & BIT[NAMES[r]]:o[r] = 1 << ids[(n, r)]
        return o
    work = list(range(N - 1, -1, -1));queued = set(work)
    while work:
        n = work.pop();queued.discard(n)
        inn = list(start) if n == entry else [0] * R
        for p in preds[n]:
            if OUT[p] is not None:
                for r in range(R):inn[r] |= OUT[p][r]
        if IN[n] == inn and OUT[n] is not None:continue
        IN[n] = inn;o = out_of(n, inn)
        if o != OUT[n]:
            OUT[n] = o
            for s in succ[n]:
                if s not in queued:work.append(s);queued.add(s)
    for n in range(N):
        if IN[n] is None:IN[n] = [0] * R
        for r in range(R):
            b = BIT[NAMES[r]]
            reach = list(bits(IN[n][r]))
            if use[n] & b:
                if not reach:  # not reached from the entry: keep the register
                    pin_use[n] |= b
                    if dfn[n] & b:pinned[find(ids[(n, r)])] = True
                for x in reach[1:]:union(reach[0], x)
                # read and written by one instruction: one web (a call's inputs and
                # results are separate values; both stay registers)
                if reach and dfn[n] & b and not (calls_at[n] and pin_use[n] & b):union(reach[0], ids[(n, r)])
                if reach and pin_use[n] & b:pinned[find(reach[0])] = True
            if post_exit[n]:
                if dfn[n] & b:pinned[find(ids[(n, r)])] = True
                else:
                    for x in reach:pinned[find(x)] = True
    # what each web is: where it is defined and read
    used_web = lambda n, r: find(next(bits(IN[n][r]))) if use[n] & BIT[NAMES[r]] and IN[n][r] else None
    def_web = lambda n, r: find(ids[(n, r)]) if dfn[n] & BIT[NAMES[r]] else None
    # the value an instruction's text names: what it writes, but a call's text
    # (the target) is read before the call writes the registers
    root_of = lambda n, r: (used_web(n, r) if calls_at[n] and used_web(n, r) is not None else def_web(n, r)) if dfn[n] & BIT[NAMES[r]] else used_web(n, r)
    seen_at = {};origin = {};returned = set()
    for n in range(N):
        for r in range(R):
            k = root_of(n, r)
            if k is None:continue
            if said[n] & BIT[NAMES[r]]:seen_at.setdefault(k, []).append(n)
            if def_web(n, r) is not None and def_web(n, r) not in origin:origin[def_web(n, r)] = (n, r)
            if 'RET(' in ' '.join(chunks[n]) and k is not None and use[n] & BIT[NAMES[r]] & EAX_BIT:returned.add(k)
    for r in range(R):
        k = find(r)
        if k in seen_at:origin[k] = ('entry', r)
    taken = set();name = {};stem = {};loaded = set()
    def fresh(base):
        base = re.sub(r'\W', '_', base).strip('_') or 'v'
        if base[0].isdigit() or base in RESERVED or base in AVOID or re.fullmatch(r'local_[0-9a-f]+|arg[0-9]+(_[0-9])?', base):base = 'v_' + base
        x = base;k = 2
        while x in taken:x = f'{base}_{k}';k += 1
        taken.add(x);stem[x] = base;return x
    def calls_used(k):
        """Web k is read only as the base of a call target (a vtable)."""
        return all(re.search(r'CALL\(\d+, M32\(\w+ \+ 0x[0-9a-f]+\)\)|CALL\(\d+, M32\(\w+\)\)', ' '.join(chunks[n]))
                   or origin.get(k, (None,))[0] == n for n in seen_at.get(k, ()))
    def label(k, depth=0):
        """A name for web k from where it comes from, or None (numbered)."""
        if k in returned:return 'result'
        how = origin.get(k)
        if how is None:return None
        n, r = how
        if n == 'entry':
            return 'this' if NAMES[r] == 'ecx' else f'in_{NAMES[r]}'
        note = (notes or {}).get(n) or ''
        line = ' '.join(chunks[n])
        if 'CALL(' in line and NAMES[r] == 'eax':
            m = re.search(r'= ([\w:~]+)', note) or re.match(r'([\w:~]+)', note)
            if m and not re.fullmatch(r'sub_[0-9a-f]+|vf\d+', m[1].split('::')[-1]):
                return 'r_' + m[1].split('::')[-1]
            return None
        src = re.fullmatch(rf'\s*{NAMES[r]} = (?:M32\((\w+)(?: \+ 0x([0-9a-f]+))?\)|(\w+));', line)
        if src and (src[1] or src[3]) in BIT:  # a copy or a load through a named value
            r2 = NAMES.index(src[1] or src[3]) if (src[1] or src[3]) in NAMES else None
            k2 = root_of(n, r2) if r2 is not None else None
            base = stem.get(name.get(k2)) or (label(k2, depth + 1) if k2 is not None and depth < 4 else None)
            if base and not re.fullmatch(r'v\d+|vtbl', base):
                if src[3]:return base
                if calls_used(k):return 'vtbl' if not src[2] else None
                # one level of field names: this_4c, but not this_4c_10
                if base not in loaded:
                    f = re.fullmatch(r'field ([A-Za-z_]\w*)(?: \(.*\))?', note)  # this_endBit for this->endBit
                    x = f"{base}_{f[1] if f else src[2] or '0'}";loaded.add(x);return x
        m = re.fullmatch(r'arg_([0-9a-f]+)', note)
        if m and re.match(rf'\s*{NAMES[r]} = ', line):return f'p{int(m[1], 16) // 4}'
        m = re.fullmatch(r'g_p?(\w+)', note)
        if m and re.match(rf'\s*{NAMES[r]} = M32\(0x[0-9a-f]+\);', line):return m[1][0].lower() + m[1][1:]
        return None
    def keeps_register(k):
        """The caller's value saved and restored around the body: `PUSH(esi)` /
        `esi = POP()` stay as they are."""
        return NAMES[origin.get(k, (0, 0))[1]] in ('ebx', 'esi', 'edi', 'ebp') and all(insns[n].mnemonic in ('push', 'pop') and insns[n].operands and
                   insns[n].operands[0].type == CS_OP_REG for n in seen_at[k])
    def through_call(k):
        """The web a callee-saved register had before the call that starts web k."""
        n, r = origin.get(k, (None, None))
        if n is None or n == 'entry' or not calls_at[n] or NAMES[r] not in ('ebx', 'esi', 'edi', 'ebp'):return None
        reach = list(bits(IN[n][r]))
        return find(reach[0]) if len(reach) == 1 else None
    # names, in text order
    out = [];variables = [];aliases = [];maps = []
    for n in range(N):
        m = {}
        for r in range(R):
            k = root_of(n, r) if said[n] & BIT[NAMES[r]] else None
            if k is None:continue
            if k not in name:
                k2 = through_call(k)
                if k2 is not None and k2 in name and pinned[k] and pinned[k2]:name[k] = name[k2]  # the callee keeps it
                elif pinned[k] and keeps_register(k):name[k] = None
                else:
                    name[k] = fresh(label(k) or f'v{len(variables) + len(aliases) + 1}')
                    (aliases.append((name[k], NAMES[r])) if pinned[k] else variables.append(name[k]))
            if name[k]:m[NAMES[r]] = name[k]
        maps.append(m);out.append([substitute(l, m) for l in chunks[n]] if m else chunks[n])
    return out, variables, calls, aliases, maps
