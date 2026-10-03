"""Execute curve helpers directly; compare unrounded 80-bit x87 returns."""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ESP, UC_X86_REG_FPCW, UC_X86_REG_FPTAG
from native_oracle import load_machine, ARENA, STACK


def run_curve(image, pe, entry, table_global, input_bits, values, control_word=0x037f):
    m = load_machine(image, pe)
    table, result = ARENA+0x6000, ARENA+0x2000
    trampoline, stop = ARENA+0x2100, ARENA+0x2106
    m.mem_write(table_global, struct.pack('<I', table))
    m.mem_write(table, struct.pack('<102f', *values))
    # FSTP tbyte captures all ST0 precision, rather than rounding to float32.
    m.mem_write(trampoline, b'\xdb\x3d'+struct.pack('<I', result)+b'\x90')
    esp = STACK+0x8000
    m.mem_write(esp, struct.pack('<2I', trampoline, input_bits))
    m.reg_write(UC_X86_REG_ESP, esp)
    m.reg_write(UC_X86_REG_FPCW, control_word)
    returned = []

    def hook(uc, address, size, data):
        if address == stop: returned.append(True); uc.emu_stop()

    m.hook_add(UC_HOOK_CODE, hook)
    m.emu_start(entry, stop+1, timeout=1_000_000, count=5000)
    assert returned and m.reg_read(UC_X86_REG_ESP) == esp+8
    assert m.reg_read(UC_X86_REG_FPCW) == control_word
    assert m.reg_read(UC_X86_REG_FPTAG) == 0xffff, 'Helper leaked an x87 stack value'
    return bytes(m.mem_read(result, 10)).hex()


def native_tables(image, pe, spec):
    """Run original initializers with a controlled allocator; publish no table dump."""
    tables = {}
    for kind in ('curve', 'score'):
        m = load_machine(image, pe)
        table, stop, esp = ARENA+0x6000, ARENA+0x2100, STACK+0x8000
        # Native initializers clean up the allocator's single cdecl argument.
        m.mem_write(spec['curve_allocator'], b'\xb8'+struct.pack('<I', table)+b'\xc3')
        m.mem_write(esp, struct.pack('<I', stop))
        m.reg_write(UC_X86_REG_ESP, esp)
        returned, allocations = [], []

        def hook(uc, address, size, data):
            if address == spec['curve_allocator']:
                sp = uc.reg_read(UC_X86_REG_ESP)
                allocations.append(struct.unpack('<I', uc.mem_read(sp+4, 4))[0])
            if address == stop: returned.append(True); uc.emu_stop()

        m.hook_add(UC_HOOK_CODE, hook)
        m.emu_start(spec['bailout_'+kind+'_initializer'], stop+1, timeout=1_000_000, count=5000)
        assert returned and allocations == [101*4] and m.reg_read(UC_X86_REG_ESP) == esp+4
        assert struct.unpack('<I', m.mem_read(spec['bailout_'+kind+'_table'], 4))[0] == table
        # The score helper can read the word after its 101-element allocation.
        # Control that neighboring word as zero; other fixtures also exercise NaN.
        tables[kind] = list(struct.unpack('<101f', m.mem_read(table, 404))) + [0.0]
    return tables


def curve_cases(native_values=None):
    # Neighboring float32 values around clamp/interpolation boundaries, and
    # NaN payloads, infinities, signed zero and the smallest subnormal inputs.
    bits = {0, 0x80000000, 1, 0x80000001, 0x7f800000, 0xff800000,
            0x7fc00000, 0xffc12345, 0x7f812345, 0xff812345}
    for value in (-2.0, -0.01, 0.01, 0.125, 0.3333333, 0.5, 0.99, 1.0, 1.01, 100.0):
        raw = struct.unpack('<I', struct.pack('<f', value))[0]
        bits.update((raw-1, raw, raw+1))
    tables = {
        'curve': [x*x/10000 for x in range(102)],
        'alternating': [(-1 if x & 1 else 1)*1e20 for x in range(102)],
        'signed-zero/nan-neighbor': [-0.0 if x & 1 else 0.0 for x in range(101)] + [float('nan')],
    }
    if native_values is not None: tables['native-initialized'] = native_values
    # All four rounding directions, with x87 24-, 53- and 64-bit precision.
    for name, values in tables.items():
        for pc in (0, 0x200, 0x300):
            for rc in (0, 0x400, 0x800, 0xc00):
                for raw in sorted(bits):
                    yield name, values, raw, 0x7f | pc | rc
