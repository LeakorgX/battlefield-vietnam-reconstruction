"""Structured control flow for L2 bodies: `if`/`else`, `while`, `do`/`while`.

Works on the flat statement list of one lifted body. A forward
`if (c) goto L; A; L:` becomes `if (!c) { A }`, and with `A` ending in
`goto M; L: B; M:` it becomes `if (!c) { A } else { B }`. A label with a later
branch back to it becomes `do { ... } while (c);` or `for (;;) { ... }`, and
`while (!c) { ... }` when the loop opens with its own exit test. Other jumps
stay `goto`. C allows a goto into a block, so a label that is still
referenced from elsewhere (or from the resume switch) keeps working inside
the new blocks: every rewrite here is the same program, only nested.
"""
import re

LABEL = re.compile(r'^(L_[0-9a-f]{8}):$')
IFGOTO = re.compile(r'^    if \((.*)\) goto (L_[0-9a-f]{8});(  /\* .* \*/)?$')
GOTO = re.compile(r'^    goto (L_[0-9a-f]{8});(  /\* .* \*/)?$')
REF = re.compile(r'goto (L_[0-9a-f]{8})')
TERMINAL = re.compile(r'^    (RET\(|EXIT_TO\(|goto L_)')
FLIP = {'==': '!=', '!=': '==', '<': '>=', '>=': '<', '>': '<=', '<=': '>'}


def top_level(c):
    """Top-level operators of a C expression (outside parentheses)."""
    ops = [];depth = 0;k = 0
    while k < len(c):
        ch = c[k]
        if ch in '([':depth += 1
        elif ch in ')]':depth -= 1
        elif depth == 0:
            two = c[k:k + 2]
            if two in ('<<', '>>', '->'):k += 2;continue
            if two in ('==', '!=', '<=', '>=', '&&', '||'):ops.append((k, two));k += 2;continue
            if ch in '<>?&|^=,':ops.append((k, ch))
        k += 1
    return ops


def wrapped(c):
    """c is one parenthesised group."""
    if not (c.startswith('(') and c.endswith(')')):return False
    depth = 0
    for k, ch in enumerate(c):
        depth += ch == '('
        depth -= ch == ')'
        if depth == 0 and k < len(c) - 1:return False
    return True


def negate(c):
    c = c.strip()
    if c.startswith('!') and wrapped(c[1:]):return c[2:-1]
    ops = top_level(c)
    if len(ops) == 1 and ops[0][1] in FLIP:
        k, op = ops[0]
        return c[:k] + FLIP[op] + c[k + len(op):]
    if re.fullmatch(r'[A-Za-z_]\w*', c):return '!' + c
    if c.startswith('!') and re.fullmatch(r'![A-Za-z_]\w*', c):return c[1:]
    return f'!({c})'


def group(lines):
    """Statements: a line, or a `{ ... }` block the lifter emitted, kept whole."""
    items = [];k = 0
    while k < len(lines):
        line = lines[k]
        if line.rstrip().endswith('{') and not line.startswith('    switch (c->eip)'):
            depth = 0;block = []
            while True:
                l = lines[k];block.append(l)
                depth += l.count('{') - l.count('}')
                k += 1
                if depth <= 0 or k >= len(lines):break
            items.append(block)
        else:
            items.append([line]);k += 1
    return items


NOOP = re.compile(r'^    (\w+) = \1;(  /\* .* \*/)?$')
ANYGOTO = re.compile(r'^(\s*)(?:if \((.*)\) )?goto (L_[0-9a-f]{8});(  /\* .* \*/)?$')
TAIL_END = re.compile(r'^(RET\(\d+\)|EXIT_TO\(.*\));(  /\* .* \*/)?$')
TAIL_MAX = 6


def inline_tails(lines, refs):
    """A goto to a short straight run of statements that ends the function
    (`L: RESTORE(esi); esp += 0xc; RET(8);`) gets its own copy of the run:
    `if (c) { RESTORE(esi); esp += 0xc; RET(8); }`. The copy runs the same
    statements in the same state as the jump would, and the label goes once
    nothing jumps to it. Runs with calls (one return stub each), labels or
    blocks are left alone."""
    tails = {}
    for k, l in enumerate(lines):
        m = LABEL.match(l)
        if not m:continue
        run = [];depth = None
        for x in lines[k + 1:k + 2 + TAIL_MAX]:
            s = x.strip()
            ind = len(x) - len(x.lstrip())
            if depth is None:depth = ind
            if ind != depth or not s or s.endswith(('{', ':', ': ;')) or s.startswith('}') or 'goto ' in s \
                    or 'CALL' in s or 'switch' in s or 'R_' in s.split('/*')[0] or '__asm__' in s:break
            run.append(s)
            if TAIL_END.match(s):tails[m.group(1)] = run;break
    if not tails:return lines
    out = []
    for l in lines:
        m = ANYGOTO.match(l)
        if not m or m.group(3) not in tails:out.append(l);continue
        ind, cond, label, note = m.group(1), m.group(2), m.group(3), m.group(4) or ''
        run = tails[label];refs[label] -= 1
        if cond is None:
            out += [ind + run[0] + note] + [ind + s for s in run[1:]]
        else:
            out += [f'{ind}if ({cond}) {{{note}'] + [ind + '    ' + s for s in run] + [ind + '}']
    return out


def structure(lines):
    """Rewrite the body lines (between the resume switch and L2_END)."""
    items = group([l for l in lines if not NOOP.match(l)])
    kind = []
    for it in items:
        l = it[0] if len(it) == 1 else ''
        m = LABEL.match(l)
        if m:kind.append(('label', m.group(1)));continue
        m = IFGOTO.match(l)
        if m:kind.append(('if', m.group(2), m.group(1), m.group(3) or ''));continue
        m = GOTO.match(l)
        if m:kind.append(('goto', m.group(1), m.group(2) or ''));continue
        kind.append(('stmt',))
    refs = {}
    for it in items:
        for l in it:
            for r in REF.findall(l):refs[r] = refs.get(r, 0) + 1
    where = {k[1]: n for n, k in enumerate(kind) if k[0] == 'label'}
    out = []

    def emit(text, depth):out.append('    ' * depth + text)

    def stmt(n, depth):
        for l in items[n]:emit(l[4:] if l.startswith('    ') else l, depth)

    def terminal(n):
        return kind[n][0] == 'goto' or kind[n][0] == 'stmt' and len(items[n]) == 1 and bool(TERMINAL.match(items[n][0]))

    def region(lo, hi, depth, loop=None):
        """Emit items[lo:hi]. loop=(head label, exit index, kind) of the innermost
        loop; kind 'do' (a jump to the head skips the test), 'while' or 'for'."""
        n = lo
        while n < hi:
            k = kind[n]
            if k[0] == 'label':
                name = k[1]
                # farthest branch back to this label within the region
                back = None
                for j in range(hi - 1, n, -1):
                    if kind[j][0] in ('if', 'goto') and kind[j][1] == name:back = j;break
                if back is not None:
                    refs[name] -= 1
                    head = n + 1
                    last = kind[back]
                    test = kind[head] if head < back else None
                    exit_label = kind[back + 1][1] if back + 1 < hi and kind[back + 1][0] == 'label' else None
                    if last[0] == 'goto' and test and test[0] == 'if' and test[1] == exit_label:
                        refs[exit_label] -= 1
                        if refs[name]:emit(f'{name}:', 0)
                        emit(f'while ({negate(test[2])}) {{{test[3]}', depth)
                        region(head + 1, back, depth + 1, (name, back + 1, 'while'))
                        emit('}', depth)
                    elif last[0] == 'goto':
                        emit('for (;;) {', depth)
                        if refs[name]:emit(f'{name}:', 0)
                        region(head, back, depth + 1, (name, back + 1, 'for'))
                        emit('}', depth)
                    else:
                        emit('do {', depth)
                        if refs[name]:emit(f'{name}:', 0)
                        region(head, back, depth + 1, (name, back + 1, 'do'))
                        emit(f'}} while ({last[2]});{last[3]}', depth)
                    n = back + 1
                    continue
                if refs.get(name):emit(f'{name}:', 0)
                n += 1
                continue
            if k[0] in ('if', 'goto'):
                target = k[1];t = where.get(target)
                if t == n + 1:  # jump to the next statement
                    refs[target] -= 1
                    if k[0] == 'if':emit(f'(void)({k[2]});{k[3]}', depth)
                    n += 1
                    continue
                if loop and t == loop[1] and loop[1] < len(kind):
                    # leaving the innermost loop
                    refs[target] -= 1
                    emit(f'if ({k[2]}) break;{k[3]}' if k[0] == 'if' else f'break;{k[2]}', depth)
                    n += 1
                    continue
                if loop and target == loop[0] and loop[2] != 'do':
                    # back to the head of a `while` (its test) or `for (;;)`
                    refs[target] -= 1
                    emit(f'if ({k[2]}) continue;{k[3]}' if k[0] == 'if' else f'continue;{k[2]}', depth)
                    n += 1
                    continue
                if k[0] == 'if' and t is not None and n + 1 < t <= hi:
                    refs[target] -= 1
                    end = t
                    # then-part ends `goto M` with M after L inside the region: else
                    j = t - 1
                    if j > n and kind[j][0] == 'goto':
                        m = where.get(kind[j][1])
                        # M may be the label right after this region (hi), where
                        # the enclosing block falls through to as well
                        if m is not None and t < m <= hi:
                            refs[kind[j][1]] -= 1
                            emit(f'if ({negate(k[2])}) {{{k[3]}', depth)
                            region(n + 1, j, depth + 1, loop)
                            emit('} else {', depth)
                            region(t, m, depth + 1, loop)
                            emit('}', depth)
                            n = m
                            continue
                    emit(f'if ({negate(k[2])}) {{{k[3]}', depth)
                    region(n + 1, end, depth + 1, loop)
                    emit('}', depth)
                    n = end
                    continue
            stmt(n, depth)
            n += 1

    region(0, len(items), 1)
    out = inline_tails(out, refs)
    # labels removed after their last reference was rewritten
    final = []
    for l in out:
        m = LABEL.match(l)
        if m and not refs.get(m.group(1)):continue
        final.append(l)
    # a label needs a statement after it
    for k, l in enumerate(final):
        if re.fullmatch(r'\s*\w+:', l) and (k + 1 == len(final) or final[k + 1].lstrip().startswith('}')):final[k] = l + ' ;'
    return final
