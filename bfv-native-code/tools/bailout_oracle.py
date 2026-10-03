"""Compare bailout recomputation while executing the real native numeric helpers.

Object virtual calls use controlled ABI stubs. Lookup tables are deterministic
fixtures, not recovered gameplay data. The original pattern helper also runs.
"""
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_FPCW
from native_oracle import load_machine, ARENA, STACK


def run_bailout(image, pe, entry, spec, metric, influence, scale,
                group_result, special_result, changing_selector=False, relocate_tables=False):
    m = load_machine(image, pe)
    obj, vt, behavior, ratings = [ARENA+n for n in (0, 0x400, 0x800, 0xc00)]
    entity, component, component_vt, record = [ARENA+n for n in (0x1000, 0x1400, 0x1800, 0x1c00)]
    pool, handles, pattern, flags = [ARENA+n for n in (0x2000, 0x2400, 0x2800, 0x2c00)]
    result, trampoline, stop = ARENA+0x3000, ARENA+0x3100, ARENA+0x3106
    float_data = ARENA+0x3200

    def w32(address, value): m.mem_write(address, struct.pack('<I', value))
    w32(obj, vt); w32(behavior+0x1c, ratings); w32(behavior+0xc, pattern)
    m.mem_write(behavior+4, b'\xe7')
    m.mem_write(ratings, struct.pack('<4f', -11.0, -22.0, -33.0, -44.0))
    alternate_ratings, alternate_flags = ARENA+0xe00, flags+0x100
    m.mem_write(alternate_ratings, struct.pack('<4f', -55.0, -66.0, -77.0, -88.0))
    w32(spec['pool'], pool); w32(pool, handles); w32(handles, entity)
    m.mem_write(handles+6, struct.pack('<H', 2))
    w32(entity+0x24, component); w32(component, component_vt)
    w32(pattern+4, flags)
    m.mem_write(flags, b'\x00'*4)
    m.mem_write(alternate_flags, b'\x00'*4)
    m.mem_write(record+8, struct.pack('<f', influence))
    m.mem_write(float_data, struct.pack('<f', metric))
    # Both numeric helpers use 100 bins plus one neighboring interpolation value.
    for key, offset, curve in [('bailout_curve_table', 0x6000, lambda x: x*x/10000),
                               ('bailout_score_table', 0x6800, lambda x: 1+x/200)]:
        table = ARENA+offset
        w32(spec[key], table)
        m.mem_write(table, struct.pack('<102f', *(curve(x) for x in range(102))))

    cursor = ARENA+0x4000
    stubs = {}

    def method(vtable, offset, value, name, pop=0, floating=False):
        nonlocal cursor
        address = cursor; cursor += 0x40
        if floating:
            code = b'\xd9\x05'+struct.pack('<I', float_data)
        else:
            code = b'\xb8'+struct.pack('<I', value)
        code += b'\xc2'+struct.pack('<H', pop) if pop else b'\xc3'
        m.mem_write(address, code); w32(vtable+offset, address)
        stubs[address] = name
        return address

    method(vt, 0xcc, 0x20001, 'handle')
    method(vt, 0x70, group_result, 'group', 4)
    method(vt, 0x6c, special_result, 'special')
    method(vt, 0xe0, record, 'influence')
    selector_stub = method(vt, 0xdc, 0, 'selector')
    selector_value = float_data+4
    m.mem_write(selector_stub, b'\xa1'+struct.pack('<I', selector_value)+b'\xc3')
    method(component_vt, 0x10, 0, 'metric', floating=True)
    m.mem_write(trampoline, b'\xd9\x1d'+struct.pack('<I', result)+b'\x90')
    esp = STACK+0x8000
    m.mem_write(esp, struct.pack('<3If', trampoline, obj, 1, scale))
    m.reg_write(UC_X86_REG_ECX, behavior); m.reg_write(UC_X86_REG_ESP, esp)
    m.reg_write(UC_X86_REG_FPCW, 0x037f)
    calls = []
    selector_count = 0
    returned = []

    def hook(uc, address, size, data):
        nonlocal selector_count
        name = stubs.get(address)
        if name:
            calls.append(name)
            if name == 'group':
                assert uc.reg_read(UC_X86_REG_ECX) == obj
                sp = uc.reg_read(UC_X86_REG_ESP)
                assert struct.unpack('<I', uc.mem_read(sp+4, 4))[0] == 0xe7
            if name == 'metric': assert uc.reg_read(UC_X86_REG_ECX) == component
            if name == 'selector':
                index = selector_count if changing_selector else 0
                uc.mem_write(selector_value, struct.pack('<I', index))
                if relocate_tables:
                    if selector_count == 0: w32(behavior+0x1c, alternate_ratings)
                    if selector_count == 1: w32(pattern+4, alternate_flags)
                    if selector_count == 2: w32(behavior+0x1c, ratings)
                selector_count += 1
        if address == stop:
            returned.append(True); uc.emu_stop()

    m.hook_add(UC_HOOK_CODE, hook)
    m.emu_start(entry, stop+1, timeout=1_000_000, count=10000)
    assert returned and m.reg_read(UC_X86_REG_ESP) == esp+16
    expected_calls = ['handle', 'group']
    if group_result & 255:
        expected_calls += ['special']
        if special_result & 255: expected_calls += ['influence']
    expected_calls += ['metric', 'selector', 'selector', 'selector']
    assert calls == expected_calls, (calls, expected_calls)
    expected_flags = [0, 1, 0, 0] if changing_selector else [1, 0, 0, 0]
    assert list(m.mem_read(alternate_flags if relocate_tables else flags, 4)) == expected_flags
    return dict(result_bits=bytes(m.mem_read(result, 4)).hex(),
                rating_bits=bytes(m.mem_read(ratings, 16)).hex(),
                alternate_rating_bits=bytes(m.mem_read(alternate_ratings, 16)).hex(),
                flags=list(m.mem_read(flags, 4)),
                alternate_flags=list(m.mem_read(alternate_flags, 4)), calls=calls)
