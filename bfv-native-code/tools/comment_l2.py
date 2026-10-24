"""Comments for the readable C (readable/<target>/). Only comments are added;
the code, its hash (build.code_sha256) and the compiled objects stay the same.

    uv run --with pefile python tools/comment_l2.py --target both

What it writes (all of it is regenerated on every run, so the tool can be run
again after lift_l2.py):

- x87 math as formulas: a `/** a = b * c + 1.5 */` line above the FPU lines
  that compute it, `/** when a < b */` on the `if` that tests an x87 compare,
  `/** = 1.0f */` on integer stores of float constants. `[x]` is the float at
  address x, `(int)x` an integer loaded as float, `st(i)` an x87 register
  value from before the block.
- per function, in its header: `does:` (a sentence for simple shapes:
  forwarders, getters, setters, constant returns, constructors), `calls:`,
  `fields:` (offsets of `this` read and written), `globals:`, `returns:`.
- per file: a contents table after the file comment.
- hand-written notes from notes/<target>/<file path>.txt, kept across
  regeneration: `## <address>` (or `## file`) followed by free text. The text
  goes into the function header as `note:` lines and its first sentence into
  the contents table.
- shared names from notes/<target>/names-*.txt and names.txt (`Class::vfN =
  name` and `Class+0x1c = name`, see Names): `Class::vfN (name)` in headers
  and a trailing `/** Class::vfN = name; +0x1c name */` on body lines that use
  them.

Generated lines are marked `/** ... */` (own-line and trailing) or start with a
key above in the header; a run removes them first.
"""
import argparse
import re
import struct
import sys
import textwrap
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(Path(__file__).resolve().parent))
from build import code_sha256  # noqa: E402

KEYS = ('slot', 'does', 'calls', 'fields', 'globals', 'loops', 'returns', 'note')
HEAD = re.compile(r'^/\* ([0-9a-f]{8})  ([^\n]*)\n((?: \*[^\n]*\n)*?) \*/\n', re.M)
ENTRY = re.compile(r'^BFV_L1_ENTRY(?:_DEEP)?\((\w+),', re.M)
OWN = re.compile(r'^[ \t]*/\*\* .*\*/[ \t]*\n', re.M)
TRAIL = re.compile(r'(?:  /\*\* (?:(?!\*/).)* \*/)+(?=[ \t]*$)', re.M)
TOC = re.compile(r'/\*\* Contents.*?\*/\n\n', re.S)


# ---------------------------------------------------------------- x87 formulas

class E:
    """An expression: text, precedence (3 atom, 2 * /, 1 + -) and the line it started on."""
    __slots__ = ('t', 'p', 'line')

    def __init__(self, t, p=3, line=None):
        self.t, self.p, self.line = t, p, line


def start(*es):
    lines = [e.line for e in es if e.line is not None]
    return min(lines) if lines else None


def binop(a, op, b):
    p = 2 if op in '*/' else 1
    at = a.t if a.p >= p else f'({a.t})'
    bt = b.t if b.p > p or (b.p == p and op in '+*') else f'({b.t})'
    return E(f'{at} {op} {bt}', p, start(a, b))


def call(fn, *args):
    return E(f'{fn}({", ".join(a.t for a in args)})', 3, start(*args))


def unary(op, a):
    return E(op + (a.t if a.p == 3 else f'({a.t})'), 3, a.line)


def float_text(bits, double=False):
    v = struct.unpack('<d' if double else '<f', struct.pack('<Q' if double else '<I', bits))[0]
    if v != v or v in (float('inf'), float('-inf')): return None
    if bits & (0x7fffffffffffffff if double else 0x7fffffff) == (0x7fefffffffffffff if double else 0x7f7fffff):
        return ('-' if v < 0 else '') + ('DBL_MAX' if double else 'FLT_MAX')
    s = f'{v:.7g}'
    if 'e' not in s and '.' not in s: s += '.0'
    return s


def nice_float(bits):
    """Text of an integer store that is clearly a float constant, else None."""
    if bits in (0xff7fffff, 0x7f7fffff): return float_text(bits)
    if bin(bits).count('1') <= 2: return None  # more likely a bit mask
    e = (bits >> 23) & 0xff
    if bits & 0x7fffffff < 0x30000000 or not 112 <= e <= 150: return None  # 2^-15 .. 2^23
    s = float_text(bits)
    if s is None or len(s.replace('-', '').replace('.', '').lstrip('0')) > 5: return None
    return s + 'f'


SIZES = {'dword': 'f32', 'qword': 'f64', 'tbyte': 'f80', 'word': 'i16'}


def operand(addr, trail, kind='float'):
    """Readable text of a memory operand of FPU_MEM."""
    m = re.fullmatch(r'\s*/\* (= )?([^*]+?) \*/\s*', trail or '')
    if m and m.group(1): return m.group(2).rstrip('f') if kind == 'float' else m.group(2)
    if m and re.fullmatch(r'(var|arg|local)_\w+', m.group(2)): return m.group(2)
    a = re.fullmatch(r'ADDR\((\w+)\)', addr.strip())
    if a: return a.group(1)
    return f'[{addr.strip()}]'


FPU_MEM = re.compile(r'FPU_MEM\("(\w+) (?:(\w+) ptr )?\[%k0\]", (.*)\);(.*)$')
FPU_REG = re.compile(r'FPU\("(\w+)(?: (st\((\d)\)|st)(?:, (st\((\d)\)|st))?)?"\);')
FNSTSW = re.compile(r'__asm__ volatile\(INTEL\("fnstsw %w0"\) : "\+a"\((\w+)\)')
CALLISH = re.compile(r'\b(CALL|CALL_ARGS|CALL_CDECL|THISCALL|STDCALL|CDECL)\((\d+), ')
LABEL = re.compile(r'^\s*(L_[0-9a-f]+|R_\w+|case [^:]+|default):\s*$')
CONSTS = {'fldz': '0', 'fld1': '1', 'fldpi': 'pi', 'fldl2e': 'log2(e)', 'fldl2t': 'log2(10)',
          'fldlg2': 'log10(2)', 'fldln2': 'ln(2)'}
UNARY = {'fchs': None, 'fabs': 'fabs', 'fsqrt': 'sqrt', 'fsin': 'sin', 'fcos': 'cos', 'frndint': 'round', 'f2xm1': 'exp2m1'}
OPS = {'fadd': '+', 'fsub': '-', 'fmul': '*', 'fdiv': '/'}
OUTCOMES = {'>': 0x00, '<': 0x01, '==': 0x40}  # C0 0x01, C2 0x04, C3 0x40 in AH after fcom a, b
RELATIONS = {frozenset('>'): '>', frozenset('<'): '<', frozenset(['==']): '==', frozenset(['<', '==']): '<=',
             frozenset(['>', '==']): '>=', frozenset(['<', '>']): '!='}


def relation(mask, test):
    """test(ah & mask) -> bool for each ordered outcome; the relation a ? b that holds."""
    hold = frozenset(k for k, ah in OUTCOMES.items() if test(ah & mask))
    return RELATIONS.get(hold)


def parity_even(v): return bin(v & 0xff).count('1') % 2 == 0


class X87:
    def __init__(self):
        self.st = []          # st[-1] is st0
        self.ret = None       # name of the last call (its float result is st0 when the stack is empty)
        self.cmp = None       # (a, b) of the last compare
        self.status = {}      # register variable: (a, b)
        self.notes = {}       # line index: [own-line comments]
        self.trail = {}       # line index: trailing comment
        self.ops = 0
        self.lost = False     # an x87 instruction not modelled: stop for this function
        self.vals = {}        # variable: the value last assigned to it, while still current

    def get(self, i):
        if i < len(self.st): return self.st[-1 - i]
        if i == len(self.st) and i == 0 and self.ret: return E(self.ret + '()')
        return E(f'st({i})')

    def put(self, i, e):
        while len(self.st) <= i: self.st.insert(0, E(f'st({len(self.st)})'))
        self.st[-1 - i] = e

    def pop(self):
        e = self.get(0)
        if self.st: self.st.pop()
        self.ret = None
        return e

    def unknown(self):
        """Values from before a block boundary: keep the depth, forget the formulas' lines."""
        self.st = [E(e.t, e.p, None) for e in self.st]
        self.ret = None

    def reset(self):
        self.st = [E(f'st({len(self.st) - 1 - k})') for k in range(len(self.st))]
        self.ret = None

    def note(self, line, e, text):
        at = e.line if e.line is not None else line
        self.notes.setdefault(at, []).append(text)

    def mem(self, k, mn, size, addr, trail):
        name = operand(addr, trail)
        a = re.fullmatch(r'ADDR\((\w+)\)', addr.strip())
        if a and a.group(1) in self.vals and not mn.startswith(('fst', 'fist')):
            v = self.vals[a.group(1)]  # a stack slot just stored from a variable
            name = v if re.fullmatch(r'\w+', v) else f'({v})'
        elif a and mn.startswith(('fst', 'fist')): self.vals.pop(a.group(1), None)
        kind = SIZES.get(size, 'f32')
        if mn.startswith('fild'): name = f'(int){name}'
        e = E(name, 3, k)
        if mn in ('fld', 'fild'): self.st.append(e); self.ret = None; return
        if mn in ('fst', 'fstp', 'fist', 'fistp', 'fisttp'):
            v = self.get(0)
            target = operand(addr, trail)
            text = f'{target} = ' + ('(int)' + (v.t if v.p == 3 else f'({v.t})') if mn.startswith('fi') else v.t)
            if not (v.t == target): self.note(k, v, text)
            if mn.endswith('p'): self.pop()
            self.ops += 1
            return
        if mn.startswith('fi'): mn = 'f' + mn[2:]; e = E(f'(int){name}', 3, k)  # fiadd, ficomp, ...: integer operand
        if mn in OPS: self.put(0, binop(self.get(0), OPS[mn], e)); self.ops += 1; return
        if mn in ('fsubr', 'fdivr'): self.put(0, binop(e, OPS[mn[:-1]], self.get(0))); self.ops += 1; return
        if mn in ('fcom', 'fcomp', 'fucom', 'fucomp', 'ficom', 'ficomp'):
            self.cmp = (self.get(0), e)
            if mn.endswith('p'): self.pop()
            return
        if mn in ('fldcw', 'fnstcw', 'fstcw', 'fnstsw', 'fstsw'): return
        self.lost = True

    def reg(self, k, mn, a, ai, b, bi):
        i = int(ai) if ai else (0 if a == 'st' else None)
        j = int(bi) if bi else (0 if b == 'st' else None)
        if mn in CONSTS: self.st.append(E(CONSTS[mn], 3, k)); self.ret = None; return
        if mn == 'fld':
            v = self.get(i); self.st.append(E(v.t, v.p, v.line if v.line is not None else k)); self.ret = None; return
        if mn in ('fst', 'fstp'):
            v = self.get(0)
            if i not in (None, 0): self.put(i, v)
            if mn == 'fstp': self.pop()
            return
        if mn == 'fxch':
            i = 1 if i is None else i
            x, y = self.get(0), self.get(i); self.put(0, y); self.put(i, x); return
        if mn in UNARY:
            v = self.get(0)
            self.put(0, unary('-', v) if mn == 'fchs' else call(UNARY[mn], v)); self.ops += 1; return
        if mn == 'fpatan': y = self.get(1); x = self.pop(); self.put(0, call('atan2', y, x)); self.ops += 1; return
        if mn == 'fyl2x':
            y = self.get(1); x = self.pop()
            log = {'ln(2)': 'log', 'log10(2)': 'log10', '1': 'log2'}.get(y.t)
            self.put(0, call(log, x) if log else binop(y, '*', call('log2', x))); self.ops += 1; return
        if mn == 'fsincos': v = self.get(0); self.put(0, call('sin', v)); self.st.append(call('cos', v)); self.ops += 1; return
        if mn == 'fptan': v = self.get(0); self.put(0, call('tan', v)); self.st.append(E('1', 3, k)); self.ops += 1; return
        if mn == 'fscale': self.put(0, call('ldexp', self.get(0), call('trunc', self.get(1)))); self.ops += 1; return
        if mn == 'fprem' or mn == 'fprem1': self.put(0, call('fmod', self.get(0), self.get(1))); self.ops += 1; return
        base, popped = (mn[:-1], True) if mn.endswith('p') and mn[:-1] in ('fadd', 'fsub', 'fmul', 'fdiv', 'fsubr', 'fdivr') else (mn, False)
        if base in ('fadd', 'fsub', 'fmul', 'fdiv', 'fsubr', 'fdivr'):
            rev = base.endswith('r'); op = OPS[base.rstrip('r')]
            if i is None and not popped: popped = True  # no operand: fopp st(1), st(0)
            if popped: dst, other = (1 if i is None else i), 0   # fopp st(i): st(i) = st(i) op st0, pop
            elif j is None: dst, other = 0, i                     # fop st(i): st0 = st0 op st(i)
            elif i == 0: dst, other = 0, j                        # fop st(0), st(i)
            else: dst, other = i, 0                               # fop st(i), st(0): st(i) = st(i) op st0
            x, y = self.get(dst), self.get(other)
            if dst == other and op == '+': self.put(dst, binop(E('2'), '*', x))  # fadd st(0), st(0)
            else: self.put(dst, binop(y, op, x) if rev else binop(x, op, y))
            self.ops += 1
            if popped: self.pop()
            return
        if mn in ('fcom', 'fcomp', 'fucom', 'fucomp'):
            self.cmp = (self.get(0), self.get(1 if i is None else i))
            if mn.endswith('p'): self.pop()
            return
        if mn in ('fcompp', 'fucompp'):
            self.cmp = (self.get(0), self.get(1)); self.pop(); self.pop(); return
        if mn == 'ftst': self.cmp = (self.get(0), E('0')); return
        if mn in ('fwait', 'fnop', 'fnclex', 'fclex', 'ffree', 'fincstp', 'fdecstp', 'emms', 'femms'): return
        if mn in ('fninit', 'finit'): self.st = []; return
        if mn.startswith('f'): self.lost = True


def trim(t, n=200):
    return t if len(t) <= n else t[:n - 3] + '...'


SIMPLE = re.compile(r'(\w+) = ([\w +\-*&|^<>()]{1,40});')
WORD = re.compile(r'[A-Za-z_]\w*')


def track_values(vals, s):
    """Values of variables from simple `x = expr;` lines (no memory, no calls), so a
    float loaded from a stack slot just stored shows what was stored.  Conservative:
    anything else that is not x87 code, a comment or a branch forgets them all."""
    if m := SIMPLE.fullmatch(s):
        x, v = m.group(1), m.group(2)
        if WORD.fullmatch(v) and v in vals: v = vals[v]
        elif re.search(r'\bM(8|16|32)\b', v): v = None
        for key in [key for key, text in vals.items() if key == x or x in WORD.findall(text)]: del vals[key]
        if v is not None and x not in WORD.findall(v): vals[x] = v
        return
    if s.startswith(('FPU', '/*', '/**')) or not s: return
    if re.match(r'(if|else if) \(', s) and s.endswith('{') and not re.search(r'[^=!<>]=[^=]|\+\+|--', s): return
    vals.clear()


def x87_comments(lines):
    """{line: [own-line comments]}, {line: trailing comment}, float ops, float result at ret."""
    x = X87()
    saved = []  # brace stack: (kind, state at entry, arm states)
    returns = []
    hi8 = re.compile(r'HI8\((\w+)\) & (0x[0-9a-f]+|\d+)\)')
    pending = {}  # register -> (a, b, line where fnstsw was)
    last_mask = None  # (reg, mask) of `r = (HI8(reg) & mask) & 0xffu;`
    for k, line in enumerate(lines):
        s = line.strip()
        if x.lost: break
        track_values(x.vals, s)
        if m := FPU_MEM.search(s):
            x.mem(k, m.group(1), m.group(2), m.group(3), m.group(4))
            continue
        if m := FPU_REG.search(s):
            x.reg(k, m.group(1), m.group(2), m.group(3), m.group(4), m.group(5))
            continue
        if m := FNSTSW.search(s):
            pending = {m.group(1): (*x.cmp, k)} if x.cmp else {}
            x.cmp = None
            continue
        pending = {r: v for r, v in pending.items() if k - v[2] <= 6}
        if m := CALLISH.search(s):
            x.unknown()
            x.ret = call_name(s) or 'call'
            if x.st: x.ret = None
            continue
        if s.startswith('RET(') and len(x.st) == 1:
            v = x.get(0)
            returns.append(v.t)
            x.notes.setdefault(k, []).append('returns ' + v.t)
        # compares used by the branch: (HI8(reg) & mask) != 0 / == 0, or r = ...; if (pf)
        for m in hi8.finditer(s):
            reg, mask = m.group(1), int(m.group(2), 0)
            if reg in pending:
                if re.search(r'& (0x[0-9a-f]+|\d+)\) & 0xffu;', s): last_mask = (reg, mask); continue
                tail = s[m.end():]
                neg = tail.lstrip().startswith('== 0')
                rel = relation(mask, (lambda v: v == 0) if neg else (lambda v: v != 0))
                if rel:
                    a, b, _ = pending[reg]
                    x.trail[k] = f'when {trim(a.t, 90)} {rel} {trim(b.t, 90)}'
        if last_mask and re.search(r'\b!?pf\b', s) and ('if (' in s or 'while (' in s):
            reg, mask = last_mask
            neg = '!pf' in s
            rel = relation(mask, (lambda v: not parity_even(v)) if neg else parity_even)
            if rel and reg in pending:
                a, b, _ = pending[reg]
                x.trail[k] = f'when {trim(a.t, 90)} {rel} {trim(b.t, 90)}'
            last_mask = None
        # integer stores of float constants
        if m := re.search(r'(?:^M32\(.*\)|^\w+) = (0x[0-9a-f]{6,8});$', s):
            f = nice_float(int(m.group(1), 16))
            if f and k not in x.trail: x.trail[k] = '= ' + f
        # block structure
        if LABEL.match(line): x.unknown(); continue
        opens = s.endswith('{')
        closes = s.startswith('}')
        if closes:
            if saved:
                kind, entry, arms = saved.pop()
                arms.append(list(x.st))
                if opens and re.match(r'\} else\b', s):
                    x.st = list(entry); x.unknown(); saved.append(('if', entry, arms)); continue
                if kind == 'if' and len(arms) == 1:
                    # `if (x < 0) st0 += 2^32` after fild: an unsigned integer loaded as float
                    a = arms[0]
                    if len(a) == len(entry) and a and all(p.t == q.t for p, q in zip(a[:-1], entry[:-1])) \
                            and a[-1].t == entry[-1].t + ' + 4294967296.0' and entry[-1].t.startswith('(int)'):
                        a[-1] = E('(unsigned)' + entry[-1].t[5:], 3, entry[-1].line); entry = list(a)
                    arms.append(entry)
                same =all(len(arms[0]) == len(a) and all(p.t == q.t for p, q in zip(arms[0], a)) for a in arms)
                if kind == 'loop' or not same:
                    x.st = list(arms[-1]); x.reset()
                else:
                    x.st = list(arms[0]); x.unknown()
            else:
                x.unknown()
            if not opens: continue
        if opens:
            kind = 'loop' if re.match(r'(do|while|for)\b', s) else 'if' if re.match(r'(if|else if)\b', s) else 'block'
            saved.append((kind, list(x.st), []))
            if kind == 'loop': x.reset()
            else: x.unknown()
    return x.notes, x.trail, x.ops, returns


VCALL = re.compile(r'/\* (?:vf\d+ of [^,]+, )?-> ([^ =*]+)')


def call_name(s):
    """Name of the function a call line calls."""
    m = re.search(r'\b(?:CALL|CALL_ARGS|CALL_CDECL|THISCALL|STDCALL|CDECL)\(\d+, (\w+)', s)
    v = VCALL.search(s)
    if v: return v.group(1)
    c = re.search(r'/\* (sub_[0-9a-f]{8}|[A-Za-z_][\w:~]*) \*/', s)
    if m and m.group(1) not in ('M32', 'M16', 'M8'):
        if re.fullmatch(r'0x[0-9a-f]+', m.group(1)): return c.group(1) if c else f'sub_{int(m.group(1), 16):08x}'
        return m.group(1)
    if c: return c.group(1)
    t = re.search(r'\(\d+, M32\((\w+) \+ (0x[0-9a-f]+|\d+)\)\)', s)
    if t and t.group(1).startswith('vtbl'): return f'vf{int(t.group(2), 0) // 4}'
    t = re.search(r'\(\d+, M32\(([^;]*?)\)\)', s)
    return f'(*[{t.group(1)}])' if t else None


ARG_WORDS = ['first', 'second', 'third', 'fourth', 'fifth', 'sixth']


def shape(code, al):
    """A sentence for a tiny body (the lines before its RET), else None."""
    val = {}
    def ev(x):
        x = x.strip()
        if m := re.fullmatch(r'(?:LO8|LO16)\((\w+)\)', x): x = m.group(1)
        if m := re.fullmatch(r'arg(\d)', x): return f'the {ARG_WORDS[int(m.group(1))]} argument' if int(m.group(1)) < 6 else x
        if m := re.fullmatch(r'M(8|16|32)\(ADDR\(arg(\d)\)\)', x): return ev('arg' + m.group(2))
        return val.get(x, x)
    store = None; result = None
    for l in code:
        if m := re.fullmatch(r'SET(8|16)L\((\w+), (.+)\);', l): val[m.group(2)] = ev(m.group(3)); continue
        if m := re.fullmatch(rf'M(8|16|32)\(({al}) \+ (0x[0-9a-f]+|\d+)\) = (.+);', l):
            if store: return None
            store = (m.group(1), m.group(3), ev(m.group(4))); continue
        if m := re.fullmatch(r'this->(\w+) = (.+);', l):
            if store: return None
            store = (None, m.group(1), ev(m.group(2))); continue
        if m := re.fullmatch(r'(\w+) = (.+);', l): val[m.group(1)] = ev(m.group(2)); continue
        return None
    r = val.get('result')
    if store:
        bits, off, v = store
        what = f'the {bits}-bit field at this+{off}' if bits else f'this->{off}'
        return f'Sets {what} to {v}.'
    if r is None: return 'Does nothing.' if not code else None
    if re.fullmatch(r'0x[0-9a-f]+|\d+', r): return f'Returns {r}.'
    if m := re.fullmatch(rf'M(8|16|32)\(({al}) \+ (0x[0-9a-f]+|\d+)\)', r): return f'Returns the {m.group(1)}-bit field at this+{m.group(3)}.'
    if m := re.fullmatch(r'this->(\w+)', r): return f'Returns this->{m.group(1)}.'
    if m := re.fullmatch(rf'({al}) \+ (0x[0-9a-f]+|\d+)', r): return f'Returns the address of this+{m.group(2)}.'
    if r in al.split('|'): return 'Returns this.'
    return None


# ---------------------------------------------------------------- summaries

SCAFFOLD = re.compile(r'^(#|L2_BEGIN|L2_END|uint32_t |int64_t |uint64_t |out:|\{|\}$|SAVE\(|RESTORE\(|BFV_|void |/\*)')


def code_lines(body):
    return [l for l in (l.strip() for l in body.split('\n')) if l and not SCAFFOLD.match(l)]


def this_aliases(code):
    """Regex alternation of `this` and the variables copied from it."""
    aliases = {'this'}
    for l in code:
        if m := re.fullmatch(r'(\w+) = (\w+);(?:\s*/\*.*)?', l):
            if m.group(2) in aliases: aliases.add(m.group(1))
    return '|'.join(sorted(map(re.escape, aliases), key=len, reverse=True))


def summary(name, how, body):
    """Header lines (key, text) describing a function body."""
    out = []
    code = code_lines(body)
    al = this_aliases(code)
    reads, writes = {}, {}
    if 'this' in body or '#define this ' in body:
        for l in code:
            for m in re.finditer(rf'\bM(8|16|32)\(({al}) \+ (0x[0-9a-f]+|\d+)\)(\s*=(?!=))?', l):
                (writes if m.group(4) else reads).setdefault(int(m.group(3), 0), m.group(1))
            for m in re.finditer(rf'FPU_MEM\("(\w+)[^"]*", ({al}) \+ (0x[0-9a-f]+|\d+)\)', l):
                (writes if m.group(1).startswith(('fst', 'fist')) else reads).setdefault(int(m.group(3), 0), 'f')
            for m in re.finditer(r'\bthis->(\w+)(\s*=(?!=))?', l):
                (writes if m.group(2) else reads).setdefault(m.group(1), '')
            if m := re.search(rf'\bM32\(({al})\) = (vtbl_\w+|0x[0-9a-f]+);', l): writes.setdefault(0, 'vtable')
    calls = []
    for l in code:
        if CALLISH.search(l) or l.startswith('EXIT_TO('):
            n = call_name(l) if not l.startswith('EXIT_TO(') else re.match(r'EXIT_TO\(([^)]*)\)', l).group(1)
            if n and re.fullmatch(r'0x[0-9a-f]+', n): continue  # a jump back into code, not a call
            if n and n not in calls: calls.append(n)
    globs = []
    for l in code:
        for m in re.finditer(r'\b(g_\w+)', l):
            if m.group(1) not in globs: globs.append(m.group(1))
    loops = sum(bool(re.match(r'(do|while|for)\b', l)) for l in code)
    # simple shapes
    stmts = [l for l in code if not re.fullmatch(r'(\w+) = (\w+);', l) and not l.startswith('RET(')
             and not l.startswith('frame') and l != 'uint32_t frame = esp;']
    does = None
    tail = [l for l in code if l.startswith('EXIT_TO(')]
    if '::~' in name or name.startswith('~'):
        does = 'Destructor.'
    elif how == 'vtable store' or re.search(r'(\w+)::\1$', name):
        does = 'Constructor: sets the vtable and initial field values.'
    elif len(stmts) == 1 and tail:
        does = f'Forwards to {calls[0] if calls else tail[0][8:-2]}.'
    elif code and code[-1].startswith('RET(') and len(code) <= 5:
        does = shape(code[:-1], al)
    if does: out.append(('does', does))
    if calls and not (does and does.startswith('Forwards')):
        out.append(('calls', ', '.join(calls[:8]) + (f' (+{len(calls) - 8} more)' if len(calls) > 8 else '')))
    def offs(d):
        ks = sorted(d, key=lambda o: (isinstance(o, str), o if isinstance(o, int) else 0, str(o)))
        t = [(f'+{o:#x}' if isinstance(o, int) else o) for o in ks]
        return ', '.join(t[:12]) + (f' (+{len(t) - 12} more)' if len(t) > 12 else '')
    if (reads or writes) and not (does and does.startswith(('Returns the', 'Sets'))):
        parts = ([f'reads {offs(reads)}'] if reads else []) + ([f'writes {offs(writes)}'] if writes else [])
        out.append(('fields', '; '.join(parts).replace('reads ', 'reads this', 1).replace('writes ', 'writes this', 1)))
    if globs: out.append(('globals', ', '.join(globs[:8]) + (f' (+{len(globs) - 8} more)' if len(globs) > 8 else '')))
    if loops: out.append(('loops', str(loops)))
    return out, calls


# ---------------------------------------------------------------- notes

def read_notes(path):
    """{address or 'file': text} from a notes file."""
    if not path.exists(): return {}
    notes, key, buf = {}, None, []
    for line in path.read_text(encoding='utf-8').splitlines():
        if m := re.match(r'^## (file|[0-9a-fA-F]{8})\s*$', line):
            if key: notes[key] = '\n'.join(buf).strip()
            key, buf = m.group(1).lower(), []
        elif key: buf.append(line)
    if key: notes[key] = '\n'.join(buf).strip()
    return {k: v for k, v in notes.items() if v}


NAME_LINE = re.compile(r'([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*?)(?:::vf(\d+)|\+(0x[0-9a-fA-F]+))\s*=\s*([A-Za-z_]\w*)\s*(?::\s*(.*))?')
VF_REF = re.compile(r'\b([A-Za-z_]\w*)::vf(\d+)\b')
CLASS_LINE = re.compile(r'^ \* class: (\S+)$', re.M)


class Names:
    """Shared names from notes/<target>/names.txt, one per line:

        IAITimer::vf1 = aiTime: the AI clock
        BotBehaviour+0x1c = weights: per-bot weight array

    A virtual method is named wherever a comment shows `Class::vfN`; a field in
    the methods of its class. Both also hold for subclasses whose object
    starts with that class (lifted/<target>/CLASSES.tsv). Classes are matched
    by their last name component."""

    def __init__(self, target):
        self.virt, self.fields, self.up = {}, {}, {}
        # names-*.txt are generated (tools/console_names.py); names.txt is
        # written by hand and read last, so its entries win.
        folder = PROJECT / 'notes' / target
        for path in sorted(folder.glob('names-*.txt')) + [folder / 'names.txt']:
            if not path.exists(): continue
            for n, line in enumerate(path.read_text(encoding='utf-8').splitlines(), 1):
                line = line.strip()
                if not line or line.startswith('#'): continue
                m = NAME_LINE.fullmatch(line)
                if not m: raise SystemExit(f'{path}:{n}: expected "Class::vfN = name: text" or "Class+0x10 = name: text"')
                cls, vf, off, name, text = m.groups()
                key = (cls.split('::')[-1], int(vf) if vf else int(off, 16))
                (self.virt if vf else self.fields)[key] = (name, (text or '').strip())
        table = PROJECT / 'lifted' / target / 'CLASSES.tsv'
        if table.exists():
            for row in table.read_text(encoding='utf-8').splitlines()[1:]:
                cls, _, _, bases = row.split('\t')
                chain = self.up.setdefault(cls.split('::')[-1], [cls.split('::')[-1]])
                for b in filter(None, bases.split('; ')):
                    if b.split('::')[-1] not in chain: chain.append(b.split('::')[-1])

        # the vtable slots that hold each function (lifted/<target>/VTABLES.tsv)
        self.cn, self.held = None, {}
        if (PROJECT / 'lifted' / target / 'VTABLES.tsv').exists():
            import slot_names
            self.cn = slot_names.ClassNames(target)
            for c, fs in self.cn.slots.items():
                for k, a in enumerate(fs): self.held.setdefault(a, []).append((c, k))

    def __bool__(self): return bool(self.virt or self.fields or self.held)

    def slots(self, address):
        """(`vf9 getViewDistance of AISettings, ...` for the header, the slot
        name in its own class): the vtable slots that hold the function at
        address. Small bodies the linker folded are in slots of unrelated
        classes too; only the function's own class names it."""
        where = self.held.get(address)
        if not where: return '', ''
        owner = self.cn.index.get(address, ('', '', ''))[2]
        groups = {}
        for c, k in where:
            s = self.cn.slot(c, k)
            groups.setdefault((k, s[0] if s else ''), []).append(c)
        order = sorted(groups.items(), key=lambda g: (owner not in g[1], -len(g[1]), g[0]))
        parts = []
        for (k, name), classes in order[:3]:
            classes = sorted(classes, key=lambda c: (c != owner, len(c), c))
            more = f' (+{len(classes) - 3} more)' if len(classes) > 3 else ''
            parts.append(f'vf{k}{" " + name if name else ""} of {", ".join(classes[:3])}{more}')
        if len(order) > 3: parts.append(f'+{len(order) - 3} more slots')
        return '; '.join(parts), next((n for (_, n), cs in order if n and owner in cs), '')

    def find(self, table, cls, key):
        for c in self.up.get(cls.split('::')[-1], [cls.split('::')[-1]]):
            if (c, key) in table: return table[c, key]
        return None

    def calls(self, text):
        """`IAITimer::vf1` -> `IAITimer::vf1 (aiTime)` in a calls: line."""
        def one(m):
            v = self.find(self.virt, m.group(1), int(m.group(2)))
            return f'{m.group(0)} ({v[0]})' if v else m.group(0)
        return VF_REF.sub(one, text)

    def line(self, line, cls, al):
        """Trailing comment text for a body line: named virtual methods in its
        comments and named fields of `this` in its code (names only; the
        descriptions stay in names.txt)."""
        out = []
        for m in VF_REF.finditer(line):
            v = self.find(self.virt, m.group(1), int(m.group(2)))
            if v and (t := f'{m.group(0)} = {v[0]}') not in out: out.append(t)
        if cls and al:
            code = line.split('/*', 1)[0]
            for m in re.finditer(rf'\bM(?:8|16|32)\((?:{al}) \+ (0x[0-9a-f]+|\d+)\)|FPU_MEM\("[^"]*", (?:{al}) \+ (0x[0-9a-f]+|\d+)\)', code):
                off = int(m.group(1) or m.group(2), 0)
                f = self.find(self.fields, cls, off)
                if f and (t := f'+{off:#x} {f[0]}') not in out: out.append(t)
        return '; '.join(out)

    def offsets(self, text, cls):
        """`+0x1c` -> `+0x1c weights` in a fields: line."""
        def one(m):
            f = self.find(self.fields, cls, int(m.group(1), 16))
            return f'{m.group(0)} {f[0]}' if f else m.group(0)
        return re.sub(r'\+(0x[0-9a-f]+)\b', one, text) if cls else text


def clean(t):
    return t.replace('*/', '* /').replace('/*', '/ *').replace('??', '? ?')


def wrap_note(text, key='note'):
    """Header lines ' * note: ...' for a hand-written note (paragraphs kept)."""
    out = []
    for i, para in enumerate(re.split(r'\n\s*\n', text)):
        lines = textwrap.wrap(' '.join(para.split()), 74) or ['']
        for j, l in enumerate(lines):
            out.append((f' * {key}: ' if i == 0 and j == 0 else ' *       ') + clean(l))
        if i < len(re.split(r'\n\s*\n', text)) - 1: out.append(' *')
    return out


def first_sentence(text):
    t = ' '.join(text.split())
    m = re.match(r'(.+?[.!?])(\s|$)', t)
    return m.group(1) if m else t


# ---------------------------------------------------------------- files

def comment_file(path, notes, names=None):
    src = path.read_text(encoding='utf-8')
    src = OWN.sub('', src); src = TRAIL.sub('', src); src = TOC.sub('', src, count=1)
    heads = list(HEAD.finditer(src))
    out = []; pos = 0; toc = []; stats = dict(functions=0, formulas=0, compares=0, notes=0, does=0, named=0, slots=0)
    for h, nxt in zip(heads, heads[1:] + [None]):
        address, title, rest = h.group(1), h.group(2), h.group(3)
        out.append(src[pos:h.start()])
        end = nxt.start() if nxt else len(src)
        chunk = src[h.end():end]
        bstart = chunk.find('\n{\n'); bend = chunk.rfind('\n}\n')
        if bstart < 0 or bend < 0:
            out.append(src[h.start():end]); pos = end; continue
        body = chunk[bstart + 1:bend + 2]
        stats['functions'] += 1
        name, how = (title.split('  (', 1) + [''])[:2]
        how = how.rstrip(')')
        keep = [l for l in rest.splitlines() if not re.match(r' \* (%s|loops):' % '|'.join(KEYS), l)
                and not re.match(r' \*       ', l) and l != ' *']
        lines = body.split('\n')
        original_lines = list(lines)
        new_body = body
        returns = []
        own = {}
        cls = m.group(1) if (m := CLASS_LINE.search(rest)) else None
        if 'L2_BEGIN' in body:
            own, trail, ops, returns = x87_comments(lines)
            for k, t in trail.items():
                lines[k] = lines[k] + f'  /** {clean(trim(t))} */'
            stats['formulas'] += sum(len(v) for v in own.values()); stats['compares'] += sum(1 for t in trail.values() if t.startswith('when'))
        if names:
            al = this_aliases(code_lines(body)) if cls else ''
            for k, l in enumerate(original_lines):
                if (t := names.line(l, cls, al)):
                    lines[k] = lines[k] + f'  /** {clean(trim(t, 120))} */'; stats['named'] += 1
        if own or lines != original_lines:
            res = []
            for k, l in enumerate(lines):
                for t in own.get(k, []):
                    ind = re.match(r'[ \t]*', l).group(0)
                    res.append(f'{ind}/** {clean(trim(t))} */')
                res.append(l)
            new_body = '\n'.join(res)
        info, _ = summary(name, how, body)
        if names:
            info = [(k, names.calls(v) if k == 'calls' else names.offsets(v, cls) if k == 'fields' else v) for k, v in info]
        if returns: info.append(('returns', 'float (st0) = ' + trim(returns[-1], 120)))
        slot, method = names.slots(int(address, 16)) if names else ('', '')
        if slot: info.insert(0, ('slot', slot)); stats['slots'] += 1
        stats['does'] += any(k == 'does' for k, _ in info)
        hdr = [f'/* {address}  {title}'] + keep
        note = notes.get(address)
        if note: hdr += wrap_note(note); stats['notes'] += 1
        hdr += [f' * {k}: {clean(v)}' for k, v in info]
        hdr.append(' */')
        out.append('\n'.join(hdr) + '\n')
        out.append(chunk[:bstart + 1] + new_body + chunk[bend + 2:])
        role = first_sentence(note) if note else next((v for k, v in info if k == 'does'), '')
        if not role and method: role = f'virtual {method}'
        toc.append((address, name.strip(), role))
        pos = end
    out.append(src[pos:])
    text = ''.join(out)
    # contents table after the file comment
    m = re.match(r'/\*.*?\*/\n', text, re.S)
    if m and toc:
        width = min(48, max(len(n) for _, n, _ in toc))
        rows = [f'/** Contents: {len(toc)} function{"s" if len(toc) != 1 else ""}'
                + (' (notes in notes/' + path.as_posix().split('/readable/', 1)[1].replace('.c', '.txt') + ')' if notes else '')]
        if notes.get('file'): rows += [' *'] + [' * ' + clean(l) for l in textwrap.wrap(' '.join(notes['file'].split()), 76)] + [' *']
        for a, n, r in toc:
            rows.append(clean(f' *  {a}  {n:<{width}}  {trim(r, 70)}'.rstrip()))
        rows.append(' */')
        text = text[:m.end()] + '\n'.join(rows) + '\n\n' + text[m.end():]
    return src, text, stats


def check(before, after):
    """The code is unchanged: same functions, same code hash."""
    def texts(s):
        marks = list(ENTRY.finditer(s)); d = {}
        for m, nxt in zip(marks, marks[1:] + [None]):
            c = s[m.start():nxt.start() if nxt else len(s)]
            d[m.group(1)] = code_sha256(c[:c.rindex('\n}\n') + 3])
        return d
    return texts(before) == texts(after) and code_sha256(before) == code_sha256(after)


def comment_target(target, root=None, only=None, dry_run=False):
    """Comment readable/<target>/ (or root) in place; returns the counts."""
    root = root or PROJECT / 'readable' / target
    files = [root / only] if only else sorted(p for p in root.rglob('*.c'))
    total = dict(files=0, functions=0, formulas=0, compares=0, notes=0, does=0, named=0, slots=0)
    names = Names(target)
    for p in files:
        notes = read_notes(PROJECT / 'notes' / target / p.relative_to(root).with_suffix('.txt'))
        before, after, st = comment_file(p, notes, names)
        original = p.read_text(encoding='utf-8')
        if not check(original, after): raise SystemExit(f'{p}: code changed; not written')
        for k, v in st.items(): total[k] += v
        total['files'] += 1
        if after != original and not dry_run: p.write_text(after, encoding='utf-8', newline='\n')
    print(f'{target}: {total["files"]} files, {total["functions"]} functions; {total["formulas"]} x87 formulas, '
          f'{total["compares"]} x87 compares, {total["does"]} shape summaries, {total["notes"]} hand notes, {total["named"]} lines with shared names, {total["slots"]} functions in vtable slots', flush=True)
    return total


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--target', choices=('client', 'server', 'both'), default='both')
    ap.add_argument('--only', help='one file, relative to readable/<target>/')
    ap.add_argument('--dry-run', action='store_true')
    args = ap.parse_args()
    for target in ('client', 'server') if args.target == 'both' else (args.target,):
        comment_target(target, only=args.only, dry_run=args.dry_run)


if __name__ == '__main__': main()
