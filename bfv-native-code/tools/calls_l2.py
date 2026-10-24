"""Calls with their arguments, and register saves, in L2 code (tools/lift_l2.py).

The pushes in front of a call become its argument list, first argument first:

    PUSH(0x0);                         this = p0;
    PUSH(name);                   ->   CALL_CDECL(1, Foo, name, 0x0);
    this = p0;
    CALL(1, Foo);
    esp += 8;

The macros in readable/runtime/l2.h expand to the same pushes in the same
order. How many pushes belong to the call:

- the caller frees the arguments (`esp += n` right after the call): n / 4;
- a direct call of a function that pops n bytes (`ret n`): n / 4; of one that
  pops none: the stack arguments its own code reads (arg_N notes);
- otherwise (calls through pointers, native code) the pushes right in front,
  up to a save of a callee-saved register.

A call is rewritten only when exactly that many pushes come before it, with
nothing in between but statements that can run before the pushes instead:
register and variable assignments that touch no memory and don't write what
an earlier push reads (esp in them is rewritten for the higher esp:
`v = esp + 0x18` after two pushes becomes `v = esp + 0x10` in front of them).
Memory accesses keep their order against the pushes, which are stores too.

A push of ebx/esi/edi/ebp whose raw register name the body never assigns
(the caller's value; values the function sets have their own names) is a save:
`SAVE(ebx, esi);`, and the pops back into them `RESTORE(esi, ebx);`.
"""
import re

PUSH = re.compile(r'    PUSH\((.*)\);(?:  /\* (.*) \*/)?')
CALL = re.compile(r'    CALL\((\d+), (.*)\);(?:  /\* (.*) \*/)?')
FREE = re.compile(r'    esp \+= (0x[0-9a-f]+|\d+);')
ASSIGN = re.compile(r'    ([A-Za-z_]\w*) ([-+*/%&|^]|<<|>>)?= (.+?);(?:  /\* (.*) \*/)?')
STEP = re.compile(r'    ([A-Za-z_]\w*)(\+\+|--);(?:  /\* (.*) \*/)?')
SETX = re.compile(r'    (SET8L|SET8H|SET16)\(([A-Za-z_]\w*), (.+)\);(?:  /\* (.*) \*/)?')
MEMORY = re.compile(r'M(8|16|32)\(')
STOP = {'POP', 'PUSH', 'CALL', 'CALL_ARGS', 'CALL_CDECL', 'SAVE', 'RESTORE', 'RET', 'EXIT_TO', 'FPU', 'FPU_MEM',
        'goto', 'if', 'else', 'while', 'for', 'do', 'switch', 'return', 'out', 'c', 'ADDR_OF'}
TEMPS = {'t', 'a', 'b', 'r', 'n', 'p', 'u', 'c0', 'ea', 'lhs', 'rhs', 'frame',
         'cf', 'pf', 'af', 'zf', 'sf', 'df', 'of', 'eflags_rest'}
POPL = re.compile(r'    (ebx|esi|edi|ebp) = POP\(\);')
SAVED = ('ebx', 'esi', 'edi', 'ebp')
IDENT = re.compile(r'[A-Za-z_]\w*')
REGS = {'eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi'}
MAX_ARGS = 24


def caller_values(body):
    """Callee-saved registers whose raw name the body assigns only by a pop."""
    text = '\n'.join(l for l in body if not POPL.fullmatch(l))
    return {r for r in SAVED
            if not re.search(rf'\b{r}\b\s*(?:[-+*/%&|^]|<<|>>)?=(?!=)|\b{r}\s*(?:\+\+|--)'
                             rf'|(?:\+\+|--)\s*{r}\b|\bSET\w*\(\s*{r}\b|\bRESTORE\([^)]*\b{r}\b', text)}


ESP_AT = re.compile(r'esp(?: ([-+]) (0x[0-9a-f]+|\d+))?')
ESP_WORD = re.compile(r'(?<!\w)esp(?!\w)')


def esp_plus(c):
    return f'esp + {c:#x}' if c > 0 else f'esp - {-c:#x}' if c < 0 else 'esp'


def fold(body, targets, pops, defines, variables=(), args_of=None, saved=None, inputs=None, dropped=None):
    """(lines, stats). targets: {k: direct target or None}; pops: {address: bytes its
    ret pops}; args_of: {address: stack arguments} of functions that pop none;
    defines: {name: expansion} of the function's #defines; variables: its C variables;
    inputs: {address: registers it reads} (reg_abi) for typed calls; dropped: a set
    that gets the #defines the typed calls no longer use."""
    dropped = set() if dropped is None else dropped
    saved = caller_values(body) if saved is None else saved
    args_of = args_of or {}
    slot = {x for x, d in defines.items() if MEMORY.match(d)}
    names = set(variables) | REGS | TEMPS | {x for x, d in defines.items() if d in REGS} | slot

    def identity(x):
        """What a name stands for: its register, or the variable or slot itself."""
        return defines[x] if defines.get(x) in REGS else x

    def reads(text):
        """(names read, reads memory) of an expression; ADDR(slot) is only an address."""
        r = {identity(x) for x in IDENT.findall(text) if x in names}
        bare = re.sub(r'\bADDR\(\w+\)', '0', text)
        return r, bool(MEMORY.search(bare)) or any(x in slot for x in IDENT.findall(bare))

    def shift(text, k4):
        """text evaluated k4 bytes of pushes earlier (esp k4 higher)."""
        if k4 and ESP_WORD.search(text):
            e = ESP_AT.fullmatch(text)
            if e:return esp_plus((int(e[2], 0) * (-1 if e[1] == '-' else 1) if e[2] else 0) - k4)
            return ESP_WORD.sub(f'(esp - {k4:#x})', text)
        return text

    def move(l, k4):
        """(l rewritten to run before k4 bytes of pushes, the register or variable
        it writes), or None when it isn't an assignment that touches no memory."""
        code = l.split('  /* ')[0]
        if any(x in STOP for x in IDENT.findall(code)) or reads(code)[1]:return None
        m = STEP.fullmatch(l)
        if m:dst, op, expr, comment = m[1], m[2][0], '1', m[3];macro = ''
        else:
            m = ASSIGN.fullmatch(l)
            if m:dst, op, expr, comment = m[1], m[2], m[3], m[4];macro = ''
            else:
                m = SETX.fullmatch(l)
                if not m:return None
                macro, dst, expr, comment, op = m[1], m[2], m[3], m[4], None
        if dst in ('esp', 'frame') or dst in defines and defines[dst] not in REGS:return None
        if not ESP_WORD.search(code):return l, identity(dst)
        expr = shift(expr, k4)
        text = f'{macro}({dst}, {expr});' if macro else f'{dst} {op or ""}= {expr};'
        return '    ' + text.replace('  =', ' =') + (f'  /* {comment} */' if comment else ''), identity(dst)

    out = [];stats = dict(calls=0, cdecl=0, saves=0, restores=0)
    n = 0
    while n < len(body):
        m = CALL.fullmatch(body[n])
        if not m:out.append(body[n]);n += 1;continue
        k, dest, note = int(m[1]), m[2], m[3]
        free = FREE.fullmatch(body[n + 1]) if n + 1 < len(body) else None
        t = targets.get(k)
        if free and int(free[1], 0) % 4 == 0:want = int(free[1], 0) // 4
        elif t is not None and pops.get(t):want = pops[t] // 4;free = None
        elif t is not None and t in pops:want = args_of.get(t, 0);free = None
        else:free = None;want = None  # the pushes in front, up to a save
        # back from the call: pushes and statements that could move in front of them
        group = [];j = len(out);count = 0
        while j and (want is None or count < want) and count < MAX_ARGS:
            p = PUSH.fullmatch(out[j - 1])
            if p:
                if want is None and p[1] in saved:break
                group.append(('push', p));count += 1;j -= 1;continue
            if move(out[j - 1], 0) is None:break
            group.append(('move', out[j - 1]));j -= 1
        group.reverse()
        while group and group[0][0] == 'move':group.pop(0);j += 1  # those stay where they are
        if count == 0 or want is not None and count != want:out.append(body[n]);n += 1;continue
        moved = [];before = [];ok = True
        for g in group:
            if g[0] == 'push':before.append(reads(g[1][1]));continue
            r = move(g[1], 4 * len(before))
            if r is None:ok = False;break
            text, writes = r
            for names_read, mem in before:  # a push that ran before it reads what it writes
                if writes in names_read:ok = False
            moved.append(text)
        if not ok:out.append(body[n]);n += 1;continue
        used = [g[1] for g in group if g[0] == 'push']
        args = ', '.join(p[1] for p in reversed(used))
        notes = [p[2] for p in reversed(used) if p[2]] + ([note] if note else [])
        del out[j:]
        out += moved
        out.append(f'    {"CALL_CDECL" if free else "CALL_ARGS"}({k}, {dest}, {args});' + (f'  /* {"; ".join(notes)} */' if notes else ''))
        stats['calls'] += 1;stats['cdecl'] += bool(free)
        n += 2 if free else 1
    # saves and restores
    res = [];n = 0
    while n < len(out):
        run = []
        while n + len(run) < len(out):
            p = PUSH.fullmatch(out[n + len(run)])
            if not p or p[1] not in saved or p[2] or len(run) == 4:break
            run.append(p[1])
        if run:res.append(f'    SAVE({", ".join(run)});');n += len(run);stats['saves'] += 1;continue
        while n + len(run) < len(out):
            p = POPL.fullmatch(out[n + len(run)])
            if not p or p[1] not in saved or len(run) == 4:break
            run.append(p[1])
        if run:res.append(f'    RESTORE({", ".join(run)});');n += len(run);stats['restores'] += 1;continue
        res.append(out[n]);n += 1
    if inputs is not None:res = typed(res, targets, pops, args_of, defines, inputs, stats, dropped)
    return res, stats


ANY_CALL = re.compile(r'    (CALL|CALL_ARGS|CALL_CDECL)\((\d+), ([A-Za-z_]\w*|0x[0-9a-f]+)(?:, (.*?))?\);(  /\* .* \*/)?')
BARRIER = re.compile(r'^(L_[0-9a-f]{8}|R_\d+):|CALL|goto |__asm__|\bRET\(|EXIT_TO\(|[{}]\s*$')
ECX_BIT, SCRATCH = 2, 7  # reg_abi bits: eax ecx edx ebx esp ebp esi edi


def typed(lines, targets, pops, args_of, defines, reads, stats, dropped):
    """CALL_ARGS / CALL_CDECL / CALL of a direct callee whose inputs are known
    become STDCALL / THISCALL / CDECL, written `r = STDCALL(k, f, a, b)` when
    the code after it reads eax first.

    Typed: the callee reads none of eax, ecx and edx but (for THISCALL) ecx
    (reg_abi.reads; ebx, esi, edi and ebp are the caller's in every call),
    and pops its stack arguments (`ret n`, or has none), or the caller frees
    them right after the call (CDECL). The receiver is the name of ecx at the
    call: `X = expr;` right in front of the call, with X (a #define of ecx)
    used nowhere else, is taken in as `THISCALL(k, f, expr, ...)` (ecx = expr
    runs where the assignment did, before the pushes); otherwise it is the last
    name of ecx in the straight code before the call (ecx = ecx, no code). The
    result name is the first name of eax read in the straight code after the
    call; any name of eax is eax, so naming it changes nothing."""
    ecx_names = {x for x, d in defines.items() if d == 'ecx'} | {'ecx'}
    eax_names = {x for x, d in defines.items() if d == 'eax'} | {'eax'}
    text = '\n'.join(lines)
    count = lambda x:len(re.findall(rf'(?<![\w.]){x}(?!\w)', text))
    out = list(lines)
    for n, l in enumerate(lines):
        m = ANY_CALL.fullmatch(l)
        if not m:continue
        form, k, dest, args, note = m[1], m[2], m[3], m[4], m[5] or ''
        t = targets.get(int(k))
        if t is None or t not in pops or t not in reads:continue
        r = reads[t] & SCRATCH  # ebx/esi/edi/ebp pass through every call
        if r & ~ECX_BIT:continue
        if form == 'CALL_CDECL':
            if r:continue
            macro = 'CDECL'
        elif form == 'CALL_ARGS' and not pops[t] or form == 'CALL' and (pops[t] or args_of.get(t, 0)):continue  # caller frees later / pushes unfolded
        else:macro = 'THISCALL' if r else 'STDCALL'
        parts = [k, dest]
        if macro == 'THISCALL':
            obj = 'ecx'
            for j in range(n - 1, -1, -1):
                x = out[j]
                if BARRIER.search(x.split('  /* ')[0]):break
                code = x.split('  /* ')[0]
                a = ASSIGN.fullmatch(x)
                if a and a[1] in ecx_names:
                    if j == n - 1 and a[2] is None and count(a[1]) == 1 \
                            and not any(re.search(rf'\b{y}\b', a[3]) for y in ecx_names):
                        obj = a[3];out[j] = None;dropped.add(a[1])
                        if a[4]:note = f'  /* {a[4]}' + (f'; {note[5:-3]}' if note else '') + ' */'
                    else:obj = a[1]
                    break
                seen = {y for y in IDENT.findall(code) if y in ecx_names}
                if seen:
                    obj = seen.pop() if len(seen) == 1 else 'ecx'
                    break
            parts.append(obj)
        if args:parts.append(args)
        call = f'{macro}({", ".join(parts)})'
        name = None
        for x in lines[n + 1:]:
            code = x.split('  /* ')[0]
            a = ASSIGN.fullmatch(x)
            seen = [y for y in IDENT.findall(code) if y in eax_names]
            if a and a[1] in eax_names and a[2] is None and not any(y in eax_names for y in IDENT.findall(a[3])):break
            if seen:
                name = a[1] if a and a[1] in eax_names else seen[0]
                break
            if BARRIER.search(code) or code.lstrip().startswith('if ('):break
        out[n] = f'    {name + " = " if name else ""}{call};{note}'
        stats['typed'] = stats.get('typed', 0) + 1
    return [l for l in out if l is not None]
