"""Differential checks for the second-pass category eligibility gate.

The generation-aware eligibility helper is deliberately controlled here.  The
gate itself executes from each original/rebuilt image so the test captures the
native register contract, field-read order, branch destinations and x87 state.
"""
import argparse
import hashlib
import itertools
import json
import random
import struct

import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_EBP, UC_X86_REG_EDI, UC_X86_REG_ESI,
    UC_X86_REG_ESP, UC_X86_REG_EIP, UC_X86_REG_FPCW)

from build import GAME, PROJECT, TARGETS
from native_oracle import ARENA, STACK, load_machine
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import state_result


def write_word(machine, address, value):
    machine.mem_write(address, struct.pack('<I', value & 0xffffffff))


def read_word(machine, address):
    return struct.unpack('<I', machine.mem_read(address, 4))[0]


def run_gate(image, pe, spec, entry, helper, case):
    machine = load_machine(image, pe)
    frame, candidate, descriptor = STACK + 0x8000, ARENA + 0x400, ARENA + 0x600
    seed, start, stop = ARENA + 0x9400, ARENA + 0x9000, ARENA + 0x9800
    machine.mem_write(ARENA, b'\xa5' * 0x2000)
    machine.mem_write(frame, b'\xa5' * 0x220)
    write_word(machine, candidate + 0x10, case.get('flags', 8))
    write_word(machine, frame + 0x1c, descriptor)
    write_word(machine, descriptor, case.get('handle', 0x12340007))
    write_word(machine, frame + 0xa0, case.get('argument', 0xabcdef01))
    control_word, depth = case.get('cw', 0x37f), case.get('depth', 0)
    machine.reg_write(UC_X86_REG_FPCW, control_word)
    preload = b''
    for index in range(depth):
        write_word(machine, seed + 4 * index, [0x3f9df3b6, 0xc0b5b22d][index % 2])
        preload += b'\xd9\x05' + struct.pack('<I', seed + 4 * index)
    saved = {UC_X86_REG_EBX: candidate, UC_X86_REG_ESI: 0x23456789,
             UC_X86_REG_EDI: 0x3456789a, UC_X86_REG_EBP: 0x456789ab,
             UC_X86_REG_ECX: 0x56789abc, UC_X86_REG_EDX: 0x6789abcd,
             UC_X86_REG_ESP: frame}
    for register, value in saved.items():
        machine.reg_write(register, value)
    machine.mem_write(stop, b'\x90')
    machine.mem_write(start, preload + b'\xe9' + struct.pack('<I', (entry - start - len(preload) - 5) & 0xffffffff))
    before, calls, exits = [], [], []

    def hook(uc, address, size, unused):
        if address == entry:
            before.extend(uc.reg_read(register) for register in FLOAT_STATE)
        if address in (spec['artillery_second_category_accept'], spec['artillery_second_category_reject']):
            exits.append(address == spec['artillery_second_category_accept'])
            uc.emu_stop()
            return
        if address != helper:
            return
        stack = uc.reg_read(UC_X86_REG_ESP)
        calls.append([uc.reg_read(UC_X86_REG_ECX), uc.reg_read(UC_X86_REG_EDX)])
        mutation = case.get('mutation')
        if mutation == 'candidate_flags':
            write_word(uc, candidate + 0x10, 0)
        elif mutation == 'descriptor_handle':
            write_word(uc, descriptor, 0)
        elif mutation == 'frame':
            write_word(uc, frame + 0x1c, 0)
            write_word(uc, frame + 0xa0, 0)
        uc.reg_write(UC_X86_REG_EAX, case.get('eligible', 1))
        uc.reg_write(UC_X86_REG_EIP, read_word(uc, stack))
        uc.reg_write(UC_X86_REG_ESP, stack + 4)

    machine.hook_add(UC_HOOK_CODE, hook)
    machine.emu_start(start, 0, timeout=2_000_000, count=100000)
    assert len(exits) == 1
    for register in (UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EBP, UC_X86_REG_ESP):
        assert machine.reg_read(register) == saved[register]
    return dict(accepted=exits[0], calls=calls,
                eax=machine.reg_read(UC_X86_REG_EAX),
                ecx=machine.reg_read(UC_X86_REG_ECX),
                edx=machine.reg_read(UC_X86_REG_EDX),
                frame=bytes(machine.mem_read(frame, 0x220)).hex(),
                memory=bytes(machine.mem_read(ARENA, 0x2000)).hex(),
                state=state_result(machine, before))


def cases():
    values = [0, 1, 7, 8, 9, 0xffffffff, 0x80000000, 0x7fffffff]
    result = [{}, {'flags': 0}, {'flags': 1}, {'flags': 0x10},
              {'eligible': 0}, {'eligible': 2}, {'eligible': 0xffffffff}]
    result += [{field: value} for field, value in itertools.product(
        ('flags', 'handle', 'argument'), values)]
    result += [{'mutation': mutation} for mutation in
               ('candidate_flags', 'descriptor_handle', 'frame')]
    selected = [{}, {'flags': 0}, {'eligible': 0},
                {'handle': 0xffffffff}, {'mutation': 'frame'}]
    result += [dict(case, cw=0x7f | precision | rounding, depth=depth)
               for case, precision, rounding, depth in itertools.product(
                   selected, (0, 0x200, 0x300), (0, 0x400, 0x800, 0xc00), (0, 2, 5))]
    randomizer = random.Random(0x9a0c7e)
    result += [dict(flags=randomizer.getrandbits(32), handle=randomizer.getrandbits(32),
                    argument=randomizer.getrandbits(32), eligible=randomizer.getrandbits(32))
               for _ in range(48)]
    return result


def compare_second_category(original, original_pe, edited, edited_pe, spec, symbols):
    entry, guard = spec['artillery_second_category_gate'], bytes.fromhex('8b4310c1e803a801')
    assert original_pe.get_data(entry - original_pe.OPTIONAL_HEADER.ImageBase, len(guard)) == guard
    patch = edited_pe.get_data(entry - edited_pe.OPTIONAL_HEADER.ImageBase, len(guard))
    assert patch[0] == 0xe9
    assert entry + 5 + struct.unpack('<i', patch[1:5])[0] == symbols['bfv_artillery_second_category_gate_bridge']
    assert patch[5:] == b'\x90' * (len(guard) - 5)
    compared = []
    for index, case in enumerate(cases()):
        old = run_gate(original, original_pe, spec, entry, spec['target_handle_eligible'], case)
        new = run_gate(edited, edited_pe, spec, entry, symbols['bfv_target_handle_eligible'], case)
        assert old == new, (index, case, {key: (old[key], new[key]) for key in old if old[key] != new[key]})
        compared.append(dict(inputs=case, accepted=old['accepted'], calls=old['calls']))
    return compared


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--game-dir', type=__import__('pathlib').Path, default=GAME)
    parser.add_argument('--target', choices=('client', 'server', 'both'), default='both')
    args = parser.parse_args()
    for target in ('client', 'server') if args.target == 'both' else (args.target,):
        spec, work = TARGETS[target], PROJECT / 'build' / target
        manifest = json.loads((work / 'manifest.json').read_text(encoding='utf-8'))
        original = (args.game_dir / spec['file']).read_bytes()
        edited = __import__('pathlib').Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest() == spec['sha']
        assert hashlib.sha256(edited).hexdigest() == manifest['output_sha256']
        found = compare_second_category(original, pefile.PE(data=original), edited, pefile.PE(data=edited),
                                        spec, {key: int(value, 16) for key, value in manifest['symbols'].items()})
        (work / 'second-category-verification.json').write_text(json.dumps(
            dict(target=target, original_sha256=spec['sha'], compiled_sha256=manifest['output_sha256'],
                 passed=len(found), cases=found), indent=2) + '\n', encoding='utf-8')
        print(f'{target}: {len(found)} second category comparisons passed', flush=True)
