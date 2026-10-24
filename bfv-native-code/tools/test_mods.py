"""Test the mod hook runtime offline: build the client with mods/tests/client/*.c
into a separate executable and run the hooked functions in an emulator.

    uv run --with pefile --with capstone --with unicorn python tools/test_mods.py

The test executable goes to build/client-custom/ (the game folder and
build/client/ are not touched). Checks: an after-hook changing a float result,
a before-hook changing an argument or skipping the function (stack cleanup,
return address, eax), a before-hook replacing a function with a float result
through a game call that runs another hooked function, the log import call,
the game's x87 control word and stack kept, mod globals, and a virtual method
and a field called by name (mod_slots.h, mod_fields.h)."""
import argparse
import os
import struct
import subprocess
import sys
from pathlib import Path
import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn import x86_const as U

PROJECT = Path(__file__).resolve().parents[1]
TEB, GDT, STACK, CODE, STOP = 0x30000000, 0x30010000, 0x30100000, 0x30200000, 0x30200100
FS, SS = 0x3b, 0x10
OBJ = 0x30300000


class Machine:
    def __init__(self, path):
        pe = pefile.PE(str(path)); data = path.read_bytes()
        m = self.uc = Uc(UC_ARCH_X86, UC_MODE_32)
        base = pe.OPTIONAL_HEADER.ImageBase
        m.mem_map(base, (pe.OPTIONAL_HEADER.SizeOfImage + 0xfff) & ~0xfff)
        m.mem_write(base, data[:pe.OPTIONAL_HEADER.SizeOfHeaders])
        for s in pe.sections: m.mem_write(base + s.VirtualAddress, s.get_data()[:s.Misc_VirtualSize or None])
        m.mem_map(TEB, 0x1000); m.mem_map(GDT, 0x1000); m.mem_map(STACK, 0x40000); m.mem_map(CODE, 0x1000); m.mem_map(OBJ, 0x10000)
        m.mem_write(TEB, struct.pack('<III', 0xffffffff, STACK + 0x40000, STACK) + bytes(12) + struct.pack('<I', TEB))
        m.mem_write(GDT + (FS & ~7), struct.pack('<HHBBBB', 0xfff, TEB & 0xffff, (TEB >> 16) & 0xff, 0xf3, 0x40, TEB >> 24))
        m.mem_write(GDT + SS, struct.pack('<HHBBBB', 0xffff, 0, 0, 0x93, 0xcf, 0))
        m.reg_write(U.UC_X86_REG_GDTR, (0, GDT, 0x1000, 0)); m.reg_write(U.UC_X86_REG_SS, SS); m.reg_write(U.UC_X86_REG_FS, FS)
        m.reg_write(U.UC_X86_REG_CR4, m.reg_read(U.UC_X86_REG_CR4) | 0x600)  # OSFXSR, OSXMMEXCPT: SSE on
        # OutputDebugStringA: `ret 4`, logged
        m.mem_write(CODE, b'\xc2\x04\x00'); m.mem_write(STOP, b'\x90')
        self.log = []
        m.hook_add(UC_HOOK_CODE, lambda uc, a, s, d: self.log.append(bytes(uc.mem_read(
            struct.unpack('<I', uc.mem_read(uc.reg_read(U.UC_X86_REG_ESP) + 4, 4))[0], 200)).split(b'\0')[0].decode()),
            begin=CODE, end=CODE)
        self.pe = pe

    def import_slot(self, slot): self.uc.mem_write(slot, struct.pack('<I', CODE))

    def call(self, function, ecx, args, fpu_cw=0x027f):
        m = self.uc
        # a native caller's state: x87 control word (single precision, as Direct3D sets it), empty x87 stack
        m.mem_write(CODE + 0x80, struct.pack('<H', fpu_cw))
        esp = STACK + 0x30000
        words = [STOP, *args]
        esp -= 4 * len(words)
        m.mem_write(esp, b''.join(struct.pack('<I', w) for w in words))
        # fninit; fldcw [cw]; jmp [target]: the stub code never changes (unicorn
        # keeps translated code), only the data it reads
        m.mem_write(CODE + 0x84, struct.pack('<I', function))
        m.mem_write(CODE + 0x40, b'\xdb\xe3\xd9\x2d' + struct.pack('<I', CODE + 0x80) + b'\xff\x25'
                    + struct.pack('<I', CODE + 0x84))
        for r, v in ((U.UC_X86_REG_ESP, esp), (U.UC_X86_REG_ECX, ecx), (U.UC_X86_REG_EAX, 0xaaaa0000),
                     (U.UC_X86_REG_EBX, 0xbbbb0000), (U.UC_X86_REG_ESI, 0x5151), (U.UC_X86_REG_EDI, 0xd1d1),
                     (U.UC_X86_REG_EBP, 0xb0b0)):
            m.reg_write(r, v)
        m.emu_start(CODE + 0x40, STOP, count=2_000_000)
        if m.reg_read(U.UC_X86_REG_EIP) != STOP: raise RuntimeError(f'did not return: eip {m.reg_read(U.UC_X86_REG_EIP):x}')
        fsw = m.reg_read(U.UC_X86_REG_FPSW); fcw = m.reg_read(U.UC_X86_REG_FPCW)
        depth = (8 - ((fsw >> 11) & 7)) & 7
        # unicorn's FP0..FP7 are the physical registers; ST0 is physical register TOP
        st0 = m.reg_read(getattr(U, f'UC_X86_REG_FP{(fsw >> 11) & 7}')) if depth else None
        regs = {n: m.reg_read(getattr(U, 'UC_X86_REG_' + n.upper())) for n in ('eax', 'ebx', 'esi', 'edi', 'ebp', 'esp')}
        return dict(regs, sp_delta=regs['esp'] - esp, depth=depth, fcw=fcw, st0=st0)


def f80(v):
    """unicorn's FP0 is (mantissa, exponent) of the 80-bit value."""
    mant, exp = v
    if not mant: return 0.0
    sign = -1.0 if exp & 0x8000 else 1.0
    return sign * mant / (1 << 63) * 2.0 ** ((exp & 0x7fff) - 16383)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--tree', choices=('lifted', 'readable'), default='readable')
    parser.add_argument('--no-build', action='store_true', help='reuse build/client-custom/BfVietnam-modtest.exe')
    args = parser.parse_args()
    exe = PROJECT / 'build' / 'client-custom' / 'BfVietnam-modtest.exe'
    if not args.no_build:
        env = dict(os.environ, BFV_MOD_DIR=str(PROJECT / 'mods' / 'tests'))
        result = subprocess.run([sys.executable, PROJECT / 'tools' / 'build.py', '--target', 'client', '--tree', args.tree,
                                 '--output', exe], env=env, text=True, capture_output=True)
        print(result.stdout, result.stderr)
        if result.returncode: raise SystemExit('build failed')
    m = Machine(exe)
    m.import_slot(0xb42208)  # imp_OutputDebugStringA (client)
    uc = m.uc
    fails = []
    def check(name, ok, detail=''):
        print(('ok   ' if ok else 'FAIL ') + name + (f'  ({detail})' if detail else ''))
        if not ok: fails.append(name)
    # object: floats at +0xbc+4i (getter) and +0x94+4i, field +0x204
    uc.mem_write(OBJ + 0xbc + 4 * 3, struct.pack('<f', 2.5))
    uc.mem_write(OBJ + 0x94 + 4 * 3, struct.pack('<f', 7.0))
    uc.mem_write(OBJ + 0x204, struct.pack('<f', 1.0))
    symbols = {}
    import re
    for line in (exe.parent / 'payload.map').read_text().splitlines():
        x = re.match(r'\s*(0x[0-9a-fA-F]+)\s+_?(test_calls)\s*$', line)
        if x: symbols[x[2]] = int(x[1], 16)
    calls = lambda: struct.unpack('<I', uc.mem_read(symbols['test_calls'], 4))[0]

    r = m.call(0x00956230, OBJ, [3])
    check('after-hook float result', r['depth'] == 1 and abs(f80(r['st0']) - 102.5) < 1e-4, f'st0 {f80(r["st0"]) if r["st0"] else None}')
    check('after-hook stack cleanup (ret 4)', r['sp_delta'] == 8, f'{r["sp_delta"]}')
    check('callee-saved registers kept', (r['ebx'], r['esi'], r['edi'], r['ebp']) == (0xbbbb0000, 0x5151, 0xd1d1, 0xb0b0))
    check('game x87 control word kept', r['fcw'] == 0x027f, f'{r["fcw"]:#x}')
    check('mod global counter', calls() == 1, f'{calls()}')
    check('log through the game import', m.log[-1:] == ['getter 3 -> 102.500\n'], repr(m.log[-1:]))

    r = m.call(0x00970d00, OBJ, [struct.unpack('<I', struct.pack('<f', 2.0))[0]])
    field = struct.unpack('<f', uc.mem_read(OBJ + 0x204, 4))[0]
    check('before-hook changed argument', abs(field - 7.0) < 1e-6 and r['depth'] == 0 and r['sp_delta'] == 8, f'field {field}, depth {r["depth"]}')
    r = m.call(0x00970d00, OBJ, [struct.unpack('<I', struct.pack('<f', -1.0))[0]])
    field = struct.unpack('<f', uc.mem_read(OBJ + 0x204, 4))[0]
    check('MOD_RETURN skips the function', abs(field - 7.0) < 1e-6 and r['eax'] == 0x1234 and r['sp_delta'] == 8 and r['depth'] == 0,
          f'field {field}, eax {r["eax"]:#x}, sp {r["sp_delta"]}')

    r = m.call(0x009698a0, OBJ, [3])
    check('MOD_RETURN_F through a hooked game call', r['depth'] == 1 and abs(f80(r['st0']) - 51.25) < 1e-4 and r['sp_delta'] == 8,
          f'st0 {f80(r["st0"]) if r["st0"] else None}, depth {r["depth"]}')
    check('nested hook ran', calls() == 2, f'{calls()}')
    check('control word after nested calls', r['fcw'] == 0x027f, f'{r["fcw"]:#x}')

    # GAME_METHOD(obj, AISettings, getViewDistance) through the AISettings vtable
    uc.mem_write(OBJ, struct.pack('<I', 0x00bf1c94))
    uc.mem_write(OBJ + 0x1c, struct.pack('<f', 41.0))  # viewDistance
    uc.mem_write(OBJ + 0x8, struct.pack('<I', 1000))   # maxNBots
    r = m.call(0x00956340, OBJ, [])
    check('virtual method and field by name', r['depth'] == 1 and abs(f80(r['st0']) - 1041.0) < 1e-3 and r['sp_delta'] == 4,
          f'st0 {f80(r["st0"]) if r["st0"] else None}, depth {r["depth"]}, sp {r["sp_delta"]}')
    print('mod hook tests:', 'all passed' if not fails else f'{len(fails)} failed')
    if fails: raise SystemExit(1)


if __name__ == '__main__': main()
