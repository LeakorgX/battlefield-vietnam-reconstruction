"""Whole-image register ABI: which registers a callee reads, and which a caller
reads after a call returns.

L2 keeps a register value in its own C variable when no call, `ret` or exit
can see it. What a call sees is its callee's input registers. Per-function
verification can't tell those (callees are stubs there), so they come from
the whole image here, like the flag ABI in lift_l2.flag_abi:

- reads[t]: registers the code from entry t may read before writing them.
  Calls inside count with their callee's reads; `push` of a callee-saved
  register (a save) and `xor r, r` don't read. Jump tables are followed.
- gives[t]: of ecx and edx, the ones some caller of t reads after the call
  before writing them (eax is always the result; ebx, esi, edi and ebp are
  always preserved).
- indirect: the registers assumed read by a call through a pointer
  (INDIRECT_IN), checked against the reads of every function whose address is
  stored in data (`pointer_reads` reports the union).

Register bit k is the index in GPRS order: eax ecx edx ebx esp ebp esi edi.
"""
import struct
from capstone import CS_OP_IMM, CS_OP_MEM, CS_OP_REG
from lift_c import GPR, BRANCHES

ORDER = ['eax', 'ecx', 'edx', 'ebx', 'esp', 'ebp', 'esi', 'edi']
BIT = {r: 1 << k for k, r in enumerate(ORDER)}
EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI = (BIT[r] for r in ORDER)
REGS = 0xff & ~ESP           # registers that can hold a C variable (esp never)
SAVED = EBX | ESI | EDI | EBP  # callee-saved
SCRATCH = EAX | ECX | EDX
INDIRECT_IN = EAX | ECX | EDX  # a few pointer targets read eax too


def names(mask):
    return [r for r in ORDER if mask & BIT[r]]


class Image:
    """Instruction cache over .text with register read/write masks."""

    def __init__(self, pe, md):
        text = next(x for x in pe.sections if x.Name.rstrip(b'\0') == b'.text')
        self.base = pe.OPTIONAL_HEADER.ImageBase + text.VirtualAddress
        self.code = text.get_data()[:text.Misc_VirtualSize]
        self.end = self.base + len(self.code)
        self.pe = pe;self.md = md;self.cache = {}
        self.image_base = pe.OPTIONAL_HEADER.ImageBase

    def at(self, a):
        c = self.cache
        if a not in c:
            i = next(self.md.disasm(self.code[a - self.base:a - self.base + 16], a, 1), None) if self.base <= a < self.end else None
            c[a] = i and (i, *access(i))
        return c[a]

    def table(self, i):
        """Targets of a `jmp [r*4 + table]` jump table (over-approximated)."""
        o = i.operands[0]
        if o.type != CS_OP_MEM or o.mem.base or not o.mem.index or o.mem.scale != 4:return []
        out = []
        for k in range(512):
            try:v = struct.unpack('<I', self.pe.get_data(o.mem.disp + 4 * k - self.image_base, 4))[0]
            except Exception:break
            if not self.base <= v < self.end:break
            out.append(v)
        return out


# Registers are tracked in pieces (low byte, high byte, upper half) so that
# `mov dl, [eax]; test dl, dl` doesn't read edx. Piece masks: 3 bits per register.
PIECES = {32: 7, 16: 3, 'l': 1, 'h': 2}


def piece(mask):
    """Register mask -> piece mask."""
    return sum(7 << 3 * k for k in range(8) if mask >> k & 1)


def whole(pmask):
    """Piece mask -> mask of registers with any piece set."""
    return sum(1 << k for k in range(8) if pmask >> 3 * k & 7)


PREGS = piece(REGS)


def access(i):
    """(read pieces, written pieces) of general registers."""
    try:r, w = i.regs_access()
    except Exception:return PREGS, 0
    rd = wr = 0
    for x in r:
        g = GPR.get(i.reg_name(x))
        if g:rd |= PIECES[g[1]] << 3 * ORDER.index(g[0])
    for x in w:
        g = GPR.get(i.reg_name(x))
        if g:wr |= PIECES[g[1]] << 3 * ORDER.index(g[0])
    m = i.mnemonic;ops = i.operands
    if m in ('xor', 'sub') and len(ops) == 2 and ops[0].type == ops[1].type == CS_OP_REG and ops[0].reg == ops[1].reg:
        g = GPR.get(i.reg_name(ops[0].reg))
        if g:rd &= ~(PIECES[g[1]] << 3 * ORDER.index(g[0]))
    if m == 'push' and ops and ops[0].type == CS_OP_REG:
        g = GPR.get(i.reg_name(ops[0].reg))
        if g and BIT[g[0]] & SAVED:rd &= ~(7 << 3 * ORDER.index(g[0]))
    return rd & PREGS, wr & PREGS


def is_jcc(m):
    return m.startswith('j') and m != 'jmp' or m in BRANCHES


def gives_after(img, ret, limit=400):
    """ecx/edx read after a call returns to `ret`, before being written.
    The walk stops at the next call, ret or indirect jump."""
    found = 0;seen = {};work = [(ret, 0)];steps = 0
    while work and steps < limit:
        a, defined = work.pop()
        while steps < limit:
            if a in seen and seen[a] & defined == seen[a]:break  # visited with fewer defined
            defined = seen[a] = seen.get(a, defined) & defined;steps += 1
            e = img.at(a)
            if e is None:break
            i, rd, wr = e;m = i.mnemonic
            if m in ('call', 'ret', 'int3', 'hlt'):break
            found |= rd & piece(ECX | EDX) & ~defined
            defined |= wr
            if defined & piece(ECX | EDX) == piece(ECX | EDX):break
            o = i.operands
            if m == 'jmp':
                if o[0].type != CS_OP_IMM:break
                a = o[0].imm;continue
            if is_jcc(m) and o and o[0].type == CS_OP_IMM:work.append((o[0].imm, defined))
            a = i.address + i.size
    return whole(found)


def scan(img, t, gives):
    """Local summary of the code from entry t: (exposed reads, [(callee, defined
    at the call)]) where callee None is an indirect call."""
    exposed = 0;calls = [];seen = {};work = [(t, 0)]
    while work:
        a, defined = work.pop()
        while True:
            if a in seen and seen[a] & defined == seen[a]:break
            defined = seen[a] = seen.get(a, defined) & defined
            e = img.at(a)
            if e is None:break
            i, rd, wr = e;m = i.mnemonic;o = i.operands
            if m == 'call':
                if o[0].type == CS_OP_IMM:
                    calls.append((o[0].imm, defined));g = gives.get(o[0].imm, 0)
                else:
                    exposed |= rd & ~defined;calls.append((None, defined));g = 0
                defined |= piece(EAX | (ECX | EDX) & ~g)
                a = i.address + i.size;continue
            exposed |= rd & ~defined
            if m in ('ret', 'int3', 'hlt'):break
            defined |= wr
            if m == 'jmp':
                if o[0].type == CS_OP_IMM:a = o[0].imm;continue
                targets = img.table(i)
                if targets:
                    work += [(x, defined) for x in targets];break
                calls.append((None, defined));break  # tail call through a pointer
            if is_jcc(m) and o and o[0].type == CS_OP_IMM:work.append((o[0].imm, defined))
            a = i.address + i.size
    return whole(exposed), calls  # call `defined` stays in pieces


def transparent(img, t):
    """Whether the callee at t writes no register on the path that returns:
    MSVC's __security_check_cookie, `cmp ecx, [cookie]; jne fail; ret` (the
    jne leads to __report_gsfailure, which does not return). Epilogues call it
    after the result is in eax, so eax, ecx and edx all live across it."""
    a = img.at(t);b = a and img.at(a[0].address + a[0].size);c = b and img.at(b[0].address + b[0].size)
    if not c:return False
    i, j, k = a[0], b[0], c[0]
    return i.mnemonic == 'cmp' and len(i.operands) == 2 and i.operands[0].type == CS_OP_REG and i.reg_name(i.operands[0].reg) == 'ecx' \
        and i.operands[1].type == CS_OP_MEM and not i.operands[1].mem.base and not i.operands[1].mem.index \
        and j.mnemonic == 'jne' and k.mnemonic == 'ret' and not k.operands


def reg_abi(pe, md, entries, pointers=()):
    """reads and gives for every direct call target and every entry, and the
    direct jumps into each address (edges: target -> [jump addresses]).
    entries: function addresses to summarize besides the call targets;
    pointers: code addresses stored in data (indirect call targets)."""
    img = Image(pe, md)
    calls = [];edges = {};offset = 0;code = img.code
    while offset < len(code):
        last = offset
        for i in md.disasm(code[offset:], img.base + offset):
            last = i.address - img.base + i.size
            if i.mnemonic == 'call':calls.append((i.address + i.size, i.operands[0].imm if i.operands[0].type == CS_OP_IMM else None))
            elif (i.mnemonic == 'jmp' or is_jcc(i.mnemonic)) and i.operands and i.operands[0].type == CS_OP_IMM:
                edges.setdefault(i.operands[0].imm, []).append(i.address)
        offset = last if last > offset else offset + 1
    gives = {};indirect_gives = 0
    for ret, t in calls:
        g = gives_after(img, ret)
        if t is None:indirect_gives |= g
        elif g:gives[t] = gives.get(t, 0) | g
    targets = {t for _, t in calls if t is not None} | set(entries) | set(pointers)
    local = {t: scan(img, t, gives) for t in targets}
    reads = {t: local[t][0] for t in targets}
    changed = True
    while changed:
        changed = False
        for t, (exposed, sub) in local.items():
            v = exposed
            for g, defined in sub:
                v |= whole(piece(reads.get(g, REGS) if g is not None else INDIRECT_IN) & ~defined)
            if v != reads[t]:reads[t] = v;changed = True
    pointer_reads = 0
    for p in pointers:pointer_reads |= reads.get(p, 0)
    return reads, gives, indirect_gives, pointer_reads, edges
