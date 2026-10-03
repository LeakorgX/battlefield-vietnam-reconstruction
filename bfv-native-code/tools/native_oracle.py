"""Run narrowly scoped native AI paths directly under an x86 emulator.

This is not a replacement AI implementation. It supplies controlled object memory
and one virtual-call stub to the original BBBailOut slot-9 function, with its second
argument false, and records the actual returned float32 bits. It also exercises
BAPInterpreter routing with controlled external virtual methods and null context.
Complete behavior selection is deliberately outside this oracle's scope.
"""
import argparse
import hashlib
import json
from pathlib import Path
import struct

import pefile
from unicorn import Uc, UC_ARCH_X86, UC_MODE_32, UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_EIP, UC_X86_REG_ESP, UC_X86_REG_FPCW

PROJECT = Path(__file__).resolve().parents[1]
BINARIES = {
    '79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5':
        dict(bailout=0x984c20, interpreter=0x9be180, pool=0xe0cd58),
    '86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d':
        dict(bailout=0x72eef0, interpreter=0x774ff0, pool=0xc2f7c0),
}
ARENA = 0x02000000
STACK = 0x03000000


def load_machine(image: bytes, pe):
    machine = Uc(UC_ARCH_X86, UC_MODE_32)
    base = pe.OPTIONAL_HEADER.ImageBase
    image_size = (pe.OPTIONAL_HEADER.SizeOfImage + 4095) & ~4095
    machine.mem_map(base, image_size)
    machine.mem_write(base, image[:pe.OPTIONAL_HEADER.SizeOfHeaders])
    for section in pe.sections:
        machine.mem_write(base+section.VirtualAddress, section.get_data())
    machine.mem_map(ARENA, 0x10000)
    machine.mem_map(STACK, 0x10000)
    return machine


def run_case(image: bytes, pe, entry: int, index: int, ratings: list[float], delegated=None) -> dict:
    machine = load_machine(image, pe)
    obj, vtable, this, table = ARENA, ARENA+0x400, ARENA+0x800, ARENA+0xc00
    selector_stub, return_trampoline, result_address, stop = ARENA+0x1000, ARENA+0x1100, ARENA+0x1200, ARENA+0x1106
    machine.mem_write(obj, struct.pack('<I', vtable))
    machine.mem_write(vtable+0xdc, struct.pack('<I', selector_stub))
    machine.mem_write(this+0x1c, struct.pack('<I', table))
    encoded = struct.pack('<'+'f'*len(ratings), *ratings)
    machine.mem_write(table, encoded)
    if delegated is not None:
        # A controlled native callee verifies the wrapper's arguments/float return,
        # not the full vehicle evaluation against fabricated incomplete objects.
        machine.mem_write(delegated, b'\xd9\x05'+struct.pack('<I',table+index*4)+b'\xc2\x0c\x00')
    # mov eax, index; ret -- implements only the object's selector virtual call.
    machine.mem_write(selector_stub, b'\xb8'+struct.pack('<I', index)+b'\xc3')
    # fstp DWORD PTR [result_address]; nop -- save the real x87 function result.
    machine.mem_write(return_trampoline, b'\xd9\x1d'+struct.pack('<I', result_address)+b'\x90')
    esp = STACK+0x8000
    machine.mem_write(esp, struct.pack('<4I', return_trampoline, obj, 0, 0))
    machine.reg_write(UC_X86_REG_ESP, esp)
    machine.reg_write(UC_X86_REG_ECX, this)
    machine.reg_write(UC_X86_REG_FPCW, 0x037f)
    reached_return = []

    def hook(uc, address, size, user_data):
        if address == delegated:
            sp = uc.reg_read(UC_X86_REG_ESP)
            assert uc.reg_read(UC_X86_REG_ECX) == this
            assert struct.unpack('<3I',uc.mem_read(sp+4,12)) == (obj,0,0)
        if address == stop:
            reached_return.append(True)
            uc.emu_stop()
    machine.hook_add(UC_HOOK_CODE, hook)
    machine.emu_start(entry, stop+1, timeout=1_000_000, count=1000)
    if not reached_return:
        raise RuntimeError('Native path did not return within the instruction/time budget')
    actual = bytes(machine.mem_read(result_address, 4))
    expected = encoded[index*4:index*4+4]
    if actual != expected:
        raise AssertionError(f'Native float32 output differs for selector {index}: {actual.hex()} != {expected.hex()}')
    return dict(selector=index, expected_bits=expected.hex(), actual_bits=actual.hex(), returned_float=struct.unpack('<f', actual)[0])


def run_dispatch(image, pe, addresses, plan_type, object_type, early, allowed, override, handler_result,
                 with_context=False, event_components=False, handler_flags=(1, 0)):
    """Execute the real interpreter; only its external virtual methods are stubs.

    Optional context and event components use controlled methods. Native helper
    bodies are stubbed equally in original/recompiled comparisons. No low-memory
    null access is hidden.
    Native dispatch routing, flags, shift masking, and stack cleanup are observed.
    """
    m = load_machine(image, pe)
    interpreter, plan, obj, handler = [ARENA+n for n in (0x100, 0x200, 0x300, 0x400)]
    plan_vt, obj_vt, handler_vt = [ARENA+n for n in (0x800, 0xa00, 0xc00)]
    masks, handlers, row = [ARENA+n for n in (0x1000, 0x1400, 0x1800)]
    out_a, out_b, stop = ARENA+0x2000, ARENA+0x2001, ARENA+0x2100
    entity, pool, records = ARENA+0x3000, ARENA+0x3400, ARENA+0x3800
    context, context_vt = ARENA+0x5000, ARENA+0x5200

    def write32(a, value): m.mem_write(a, struct.pack('<I', value))
    stub_cursor = ARENA+0x4000
    invoked = []
    named_stubs = {}

    def method(vt, offset, value, pop=0, name=None, set_flags=False):
        nonlocal stub_cursor
        target = stub_cursor
        stub_cursor += 0x40
        code = b''
        if set_flags:
            code += b'\xc6\x05'+struct.pack('<I', out_a)+bytes([handler_flags[0]])
            code += b'\xc6\x05'+struct.pack('<I', out_b)+bytes([handler_flags[1]])
        code += b'\xb8'+struct.pack('<I', value)
        code += b'\xc2'+struct.pack('<H', pop) if pop else b'\xc3'
        m.mem_write(target, code)
        write32(vt+offset, target)
        if name: named_stubs[target] = name

    write32(plan, plan_vt); write32(obj, obj_vt); write32(handler, handler_vt)
    write32(interpreter+8, masks); write32(interpreter+12, handlers)
    write32(masks+object_type*4, (1 << (plan_type & 31)) if allowed else 0)
    write32(handlers+plan_type*4, row)
    write32(row+object_type*4, handler if override else 0)
    write32(addresses['pool'], pool); write32(pool, records)
    write32(records, entity); m.mem_write(records+6, struct.pack('<H', 2))
    method(obj_vt, 0x134, int(early))
    method(obj_vt, 0x13c, context if with_context else 0)
    method(obj_vt, 0xdc, 2)
    method(obj_vt, 8, object_type)
    method(obj_vt, 0xcc, 0x20001)
    method(plan_vt, 12, plan_type)
    method(plan_vt, 0, handler_result, 12, 'fallback', True)
    method(handler_vt, 4, handler_result, 16, 'specialized', True)
    if with_context:
        write32(context, context_vt)
        method(context_vt, 12, 0, name='context-reset')
        method(context_vt, 4, 0, 16, 'context-notify')
        for key, pop, name in [('context_begin', 4, 'context-begin'),
                               ('context_end', 4, 'context-end'),
                               ('context_record', 0, 'context-record')]:
            address = addresses[key]
            m.mem_write(address, b'\x31\xc0'+(b'\xc2'+struct.pack('<H', pop) if pop else b'\xc3'))
            named_stubs[address] = name
    if event_components:
        for n, field in enumerate((0x2c, 0x30)):
            component, owner, receiver, vt = [ARENA+0x6000+n*0x800+x for x in (0,0x100,0x200,0x400)]
            write32(entity+field, component)
            write32(component+4, owner)
            write32(owner+0x20, receiver)
            write32(receiver, vt)
            method(vt, 0xa0, 0, 4, f'event-{n+2}')
    m.mem_write(out_a, b'\x7f\x7f')
    m.mem_write(stop, b'\x90')
    esp = STACK+0x8000
    m.mem_write(esp, struct.pack('<5I', stop, plan, obj, out_a, out_b))
    m.reg_write(UC_X86_REG_ESP, esp); m.reg_write(UC_X86_REG_ECX, interpreter)
    returned = []

    def hook(uc, address, size, data):
        if address in named_stubs:
            name = named_stubs[address]
            if name == 'context-notify':
                sp = uc.reg_read(UC_X86_REG_ESP)
                phase = struct.unpack('<I',uc.mem_read(sp+12,4))[0]
                name = f'context-notify-{phase}'
            invoked.append(name)
        if address == stop: returned.append(True); uc.emu_stop()
    m.hook_add(UC_HOOK_CODE, hook)
    try:
        m.emu_start(addresses['interpreter'], stop+1, timeout=1_000_000, count=2000)
    except Exception as error:
        raise RuntimeError(f"Native execution failed at {m.reg_read(UC_X86_REG_EIP):08x}; calls={invoked}; ecx={m.reg_read(UC_X86_REG_ECX):08x}") from error
    calls = []
    flags = [1, 0] if early else list(handler_flags) if allowed else [0, 1]
    result = 1 if early else handler_result if allowed else 0
    if not early:
        if with_context: calls += ['context-reset','context-begin','context-record','context-notify-0']
        if allowed: calls += ['specialized' if override else 'fallback']
        elif event_components: calls += ['event-2','event-3']
        if with_context:
            calls += ['context-record','context-notify-1']
            if flags[1] or not flags[0] or not result: calls += ['context-end']
    expected = dict(result=result, flags=flags, calls=calls)
    actual = dict(result=m.reg_read(UC_X86_REG_EAX) & 255,
                  flags=list(m.mem_read(out_a, 2)), calls=invoked)
    if not returned or actual != expected or m.reg_read(UC_X86_REG_ESP) != esp+20:
        raise AssertionError(f'Interpreter mismatch: actual={actual}, expected={expected}, returned={returned}')
    return dict(plan_type=plan_type, object_type=object_type, early=early, allowed=allowed,
                override=override, handler_result=handler_result, **actual)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--exe', type=Path, default=PROJECT.parent / 'BfVietnam.exe')
    parser.add_argument('--output', type=Path, default=PROJECT.parent / 'bfv-reference-local/native-logic/oracle-bailout-table.json')
    args = parser.parse_args()
    image = args.exe.read_bytes()
    digest = hashlib.sha256(image).hexdigest()
    if digest not in BINARIES:
        parser.error('This oracle is pinned to the inspected binary hash; relocate and verify the entry before using another image')
    pe = pefile.PE(data=image)
    addresses = BINARIES[digest]
    ratings = [-100., -0., 0., 0.125, 1., 1.25, 0.333333333, 123.456] * 4
    cases = [run_case(image, pe, addresses['bailout'], index, ratings) for index in range(len(ratings))]
    dispatch = [run_dispatch(image, pe, addresses, pt, ot, early, allowed, override, value)
        for pt in (0, 1, 7, 31, 33) for ot in (0, 3) for early in (False, True)
        for allowed in (False, True) for override in (False, True) for value in (0, 1)]
    result = dict(binary_sha256=digest, entries={k: f'{v:08x}' for k, v in addresses.items()},
        scope='Bailout cached-rating path; interpreter null-context paths with controlled external virtual methods. Full AI, context hooks, event delivery and recomputation are not validated.',
        cases=cases, dispatch_cases=dispatch, passed=len(cases)+len(dispatch))
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(result, indent=2), encoding='utf-8')
    print(f'Original native AI: {len(cases)} exact float32 cases and {len(dispatch)} interpreter routing/flags/stack cases passed')


if __name__ == '__main__':
    main()
