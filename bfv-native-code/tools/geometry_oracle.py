"""Compare geometric helpers using their full x87 return and original ABI."""
import itertools
import random
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_FPCW, UC_X86_REG_FPTAG
from native_oracle import load_machine, ARENA, STACK


def run_geometry(image, pe, entry, words, cw):
    m = load_machine(image, pe)
    result, trampoline, stop = ARENA+0x2000, ARENA+0x2100, ARENA+0x2106
    m.mem_write(trampoline, b'\xdb\x3d'+struct.pack('<I',result)+b'\x90')
    esp = STACK+0x8000
    m.mem_write(esp,struct.pack('<10I',trampoline,*words))
    m.reg_write(UC_X86_REG_ESP,esp);m.reg_write(UC_X86_REG_FPCW,cw)
    returned=[]
    def hook(uc,a,size,data):
        if a==stop: returned.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook)
    m.emu_start(entry,stop+1,timeout=1_000_000,count=5000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==esp+40
    assert m.reg_read(UC_X86_REG_FPCW)==cw
    assert m.reg_read(UC_X86_REG_FPTAG)==0xffff
    return bytes(m.mem_read(result,10)).hex()


def geometry_cases():
    def bits(values):return struct.unpack('<9I',struct.pack('<9f',*values))
    inputs=[]
    # Before, inside and beyond horizontal/vertical/diagonal segments, plus zero length.
    for p in ((-1,9,2),(0,9,2),(1,9,2),(2,9,2),(3,9,2),(0,0,0)):
        for end in ((2,0,0),(0,0,2),(2,0,2),(0,0,0)):
            inputs.append(bits((*p,0,0,0,*end)))
    rng=random.Random(0xBF1942)
    for _ in range(32):inputs.append(bits([rng.uniform(-1e4,1e4) for _ in range(9)]))
    baseline=list(bits((1,2,3,4,5,6,7,8,9)))
    for raw in (0,0x80000000,1,0x80000001,0x7f7fffff,0xff7fffff,
                0x7f800000,0xff800000,0x7fc12345,0xffc12345,0x7f812345):
        for slot in (0,1,8):
            words=baseline.copy();words[slot]=raw;inputs.append(tuple(words))
    for pc,rc in itertools.product((0,0x200,0x300),(0,0x400,0x800,0xc00)):
        for words in inputs:yield words,0x7f|pc|rc
