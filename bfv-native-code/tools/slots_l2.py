"""Names for stack slots in L2 code (tools/lift_l2.py).

tools/sort_lifted.py notes stack operands as arg_<n> (n bytes above the return
address) and var_<n> (n bytes below it). Where the address is exact, the
access is written as a named slot:

    v1 = M32(esp + 0x14);  /* arg_4 */      ->   v1 = arg1;
    M32(esp + 8) = this;  /* var_4 */       ->   local_4 = this;
    ecx = esp + 0x10;  /* &var_8 */         ->   ecx = ADDR(local_8);

`arg1` is `M32(frame + 0x8)`, `local_4` is `M32(frame - 0x4)`, with
`frame = esp` set when the body is entered at the function's start. A slot is
named only where that is exact: at instructions that no path reaches from a
call (after a call the body is re-entered and `frame` isn't set; the callee's
pop count would be a guess). Slots addressed through an EBP frame are named
anywhere: `local_4` is then `M32(ebp - 0x4)` as written.
"""
import re

NOTE = re.compile(r'(&?)(var|arg)_([0-9a-f]+)(?:, (.*))?')
NUM = r'(?: ([-+]) (0x[0-9a-f]+|\d+))?'
CHANGES_ESP = {'pop', 'call', 'ret', 'leave', 'enter', 'popal', 'pushal', 'popfd'}


def slot_name(kind, off):
    if kind == 'var':return f'local_{off:x}'
    return f'arg{off // 4}' if off % 4 == 0 else f'arg{off // 4}_{off % 4}'


def name_slots(insns, chunks, notes, maps, exact):
    """(chunks, [(name, definition)], uses_frame, {n: rest of the note})"""
    defs = {};order = [];rest = {};frame = False;out = list(chunks)
    for n, i in enumerate(insns):
        m = NOTE.fullmatch(notes.get(n) or '')
        if not m or i.mnemonic in CHANGES_ESP or not chunks[n]:continue
        amp, kind, off = m[1], m[2], int(m[3], 16)
        pos = -off if kind == 'var' else 4 + off  # from the return address
        text = '\n'.join(chunks[n])
        # the register the operand is based on, as the instruction has it
        based = re.findall(r'\[(esp|ebp)\b', i.op_str)
        if len(based) != 1 or len(re.findall(r'\besp\b', text)) != (based[0] == 'esp'):continue
        if based[0] == 'esp':
            if n not in exact:continue
            reg, d = 'esp', f'frame {"-" if pos < 0 else "+"} {abs(pos):#x}'
        elif re.match(r'(e?bp|bpl)\b', i.op_str) or i.mnemonic == 'xchg':continue  # writes ebp: which name?
        else:reg, d = (maps[n].get('ebp', 'ebp') if maps else 'ebp'), None
        acc = re.compile(rf'M(8|16|32)\(\b{reg}\b{NUM}\)')
        bare = re.compile(rf'\b{reg}\b{NUM}(?![\w(])')
        hits = acc.findall(text) or bare.findall(text)
        if len(hits) != 1:continue
        hit = acc.search(text) or bare.search(text)
        if reg != 'esp':  # EBP frame: the slot is the address as written
            sign, num = (hit[2], hit[3]) if hit.re is acc else (hit[1], hit[2])
            disp = (int(num, 0) if num else 0) * (-1 if sign == '-' else 1)
            d = f'{reg} {"-" if disp < 0 else "+"} {abs(disp):#x}'
        x = slot_name(kind, off)
        if defs.get(x, d) != d:continue  # the same name for another expression
        if x not in defs:defs[x] = d;order.append(x)
        frame |= reg == 'esp'
        if hit.re is acc:
            new = x if hit[1] == '32' else f'M{hit[1]}(ADDR({x}))'
        else:new = f'ADDR({x})'
        text = text[:hit.start()] + new + text[hit.end():]
        out[n] = text.split('\n');rest[n] = m[4]
    return out, [(x, f'M32({defs[x]})') for x in order], frame, rest
