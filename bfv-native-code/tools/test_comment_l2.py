"""Check the x87 formulas comment_l2.py writes: random x87 sequences in the L2
text form are assembled with the build's GAS, run in unicorn, and the value
stored compared with the formula evaluated in Python.

    uv run --with pefile --with unicorn python tools/test_comment_l2.py
"""
import math
import random

import struct
import subprocess
import sys
import tempfile
from pathlib import Path
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32

sys.path.insert(0, str(Path(__file__).resolve().parent))
import comment_l2 as C  # noqa: E402

GAS = Path(r'C:\msys64\mingw32\bin\as.exe')
OBJCOPY = Path(r'C:\msys64\mingw32\bin\objcopy.exe')
CODE, DATA = 0x100000, 0x200000
NAMES = ['a', 'b', 'c', 'd']
BINARY = ['fadd', 'fsub', 'fmul', 'fdiv', 'fsubr', 'fdivr']


def program(rng):
    """Lines in L2 form and the matching assembly; leaves one value to store."""
    lines, asm, depth = [], [], 0
    def mem(op, n):
        lines.append(f'FPU_MEM("{op} dword ptr [%k0]", ADDR({n}));'); asm.append(f'{op} dword ptr [{DATA + 4 * NAMES.index(n)}]')
    def reg(text):
        lines.append(f'FPU("{text}");'); asm.append(text)
    for _ in range(rng.randint(2, 9)):
        choices = ['load'] * (3 if depth < 6 else 0) + (['mem', 'unary'] if depth else []) + (['reg', 'regp', 'fxch'] if depth >= 2 else [])
        c = rng.choice(choices)
        if c == 'load':
            r = rng.random()
            if r < 0.7: mem('fld', rng.choice(NAMES))
            elif r < 0.85: reg(rng.choice(['fld1', 'fldz']))
            else: reg(f'fld st({rng.randrange(depth)})') if depth else mem('fld', 'a')
            depth += 1
        elif c == 'mem': mem(rng.choice(BINARY), rng.choice(NAMES))
        elif c == 'unary': reg(rng.choice(['fchs', 'fabs', 'fadd st(0), st(0)', 'fmul st(0), st(0)']))
        elif c == 'reg':
            op, i = rng.choice(BINARY), rng.randrange(1, depth)
            reg(rng.choice([f'{op} st({i})', f'{op} st(0), st({i})', f'{op} st({i}), st(0)']))
        elif c == 'regp':
            reg(f'{rng.choice(BINARY)}p st({rng.randrange(1, depth)})'); depth -= 1
        else: reg(f'fxch st({rng.randrange(1, depth)})')
    if not depth: mem('fld', 'a'); depth = 1
    for _ in range(depth - 1): reg('fstp st(1)')  # drop everything under st0
    lines.append('FPU_MEM("fstp dword ptr [%k0]", ADDR(out));'); asm.append(f'fstp dword ptr [{DATA + 16}]')
    return lines, asm


def run(asm, values, tmp):
    s = tmp / 't.s'; o = tmp / 't.o'; b = tmp / 't.bin'
    s.write_text('.intel_syntax noprefix\n' + '\n'.join(asm) + '\nnop\n')
    subprocess.run([GAS, '--32', s, '-o', o], check=True)
    subprocess.run([OBJCOPY, '-O', 'binary', '-j', '.text', o, b], check=True)
    code = b.read_bytes()
    m = Uc(UC_ARCH_X86, UC_MODE_32)
    m.mem_map(CODE, 0x1000); m.mem_map(DATA, 0x1000)
    m.mem_write(CODE, b'\xdb\xe3' + code)  # fninit
    m.mem_write(DATA, struct.pack('<4f', *values))
    m.emu_start(CODE, CODE + 2 + len(code) - 1)
    return struct.unpack('<f', m.mem_read(DATA + 16, 4))[0]


def formula(lines):
    notes, trail, ops, returns = C.x87_comments(['{'] + lines + ['}'])
    texts = [t for v in notes.values() for t in v if t.startswith('out = ')]
    return texts[-1][6:] if texts else None


def main():
    rng = random.Random(1)
    fails = 0
    with tempfile.TemporaryDirectory() as d:
        tmp = Path(d)
        for n in range(300):
            lines, asm = program(rng)
            values = [rng.choice([0.5, 1.5, 2.0, 3.0, -4.0, 7.25]) for _ in NAMES]
            got = run(asm, values, tmp)
            text = formula(lines)
            env = dict(zip(NAMES, values), fabs=abs)
            try:
                want = eval(text, {'__builtins__': {}}, env) if text else None
            except ZeroDivisionError:  # x/0 is inf, 0/0 is nan in x87
                want = math.nan if got != got else math.inf
            ok = want is not None and (math.isclose(got, want, rel_tol=1e-5, abs_tol=1e-6) or (math.isinf(got) and abs(want) > 3e38)
                                       or (math.isnan(got) and want != want))
            if not ok:
                fails += 1
                if fails <= 5: print('FAIL', text, got, want, *lines, sep='\n  ')
    print(f'x87 formulas: {300 - fails} of 300 match')
    if fails: raise SystemExit(1)


if __name__ == '__main__': main()
