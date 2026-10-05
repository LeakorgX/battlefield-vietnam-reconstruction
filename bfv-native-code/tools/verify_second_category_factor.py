"""Differential checks for second-pass category factor setup.

The retained scorer receives its factor through ST0. Object callbacks are
controlled while original/source event-interface and node-search helpers run.
"""
import argparse
import hashlib
import itertools
import json
import random
import struct
from pathlib import Path

import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_EDX, UC_X86_REG_EBP, UC_X86_REG_EDI, UC_X86_REG_ESI,
    UC_X86_REG_ESP, UC_X86_REG_FPCW)

from build import GAME, PROJECT, TARGETS
from native_oracle import ARENA, STACK, load_machine
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import state_result


def bits(value): return struct.unpack('<I', struct.pack('<f', value))[0]
def write(machine, address, value): machine.mem_write(address, struct.pack('<I', value & 0xffffffff))
def read(machine, address): return struct.unpack('<I', machine.mem_read(address, 4))[0]
def words(machine, address, values): machine.mem_write(address, struct.pack('<' + 'I' * len(values), *values))


def run_factor(image, pe, spec, entry, case):
    machine=load_machine(image, pe); frame=STACK+0x8000; seed=ARENA+0x9400
    machine.mem_write(ARENA,b'\xa5'*0x4000);machine.mem_write(frame,b'\xa5'*0x220)
    candidate,receiver,interface,initial,descriptor=[ARENA+n for n in (0x100,0x200,0x300,0x400,0x500)]
    candidate_table,receiver_table,interface_table,bot,bot_table=[ARENA+n for n in (0x600,0x700,0x800,0x900,0xa00)]
    component,weights=[ARENA+n for n in (0xb00,0xc00)]
    vectors=[ARENA+0xd00+i*0x100 for i in range(4)]
    nodes=[ARENA+0x1300+i*0x40 for i in range(3)]
    sentinel=ARENA+0x1500;factor=ARENA+0x1600
    names=['capture','class','eligible','enabled','list','factor']
    stubs={name:ARENA+0x8000+i*0x100 for i,name in enumerate(names)}
    for name,address in stubs.items(): machine.mem_write(address,b'\xc2\x04\x00' if name in ('eligible','list') else b'\xc3')
    write(machine,candidate,candidate_table);write(machine,candidate+0x20,receiver);write(machine,candidate+0x24,interface)
    write(machine,candidate+0x30,component);write(machine,candidate_table+0x0,0)
    write(machine,receiver,receiver_table);write(machine,receiver_table+0x90,stubs['capture'])
    write(machine,interface,interface_table);write(machine,interface_table+0x3c,stubs['class'])
    write(machine,component+8,weights);words(machine,weights,case.get('weights',[bits(.25),bits(.5),bits(.75),bits(1)]))
    write(machine,bot,bot_table)
    for slot,name in ((0x70,'eligible'),(0x6c,'enabled'),(0x4c,'list'),(0xe0,'factor')):write(machine,bot_table+slot,stubs[name])
    category=case.get('category',7)&0xff;machine.mem_write(descriptor+4,bytes([category]));write(machine,initial,case.get('handle',0x12340007))
    key=case.get('handle',0x12340007)
    for index,node in enumerate(nodes):words(machine,node,[nodes[index+1] if index<2 else sentinel,0,key if index==case.get('match',1) else index+1])
    if case.get('empty'):write(machine,sentinel,sentinel)
    for vector in vectors:words(machine,vector,[0,nodes[0],case.get('count',3)])
    write(machine,vectors[2]+4,nodes[0]);write(machine,nodes[0],nodes[0])
    write(machine,vectors[3]+4,sentinel);write(machine,factor+8,case.get('factor',bits(.8)))
    for offset,value in ((0x1c,initial),(0xa4,descriptor),(0x44,case.get('index',0)),
                         (0xd0,case.get('class',1)),(0x5c,case.get('base',bits(.3)))):write(machine,frame+offset,value)
    cw,depth=case.get('cw',0x37f),case.get('depth',0);machine.reg_write(UC_X86_REG_FPCW,cw)
    preload=b''
    for index in range(depth):
        write(machine,seed+4*index,[bits(1.234),bits(-5.678)][index%2]);preload+=b'\xd9\x05'+struct.pack('<I',seed+4*index)
    saved={UC_X86_REG_EBX:candidate,UC_X86_REG_ESI:bot,UC_X86_REG_EDI:initial,
           UC_X86_REG_EBP:0x456789ab,UC_X86_REG_ESP:frame}
    for register,value in saved.items():machine.reg_write(register,value)
    start=ARENA+0x9000;machine.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];calls=[];returned=[];lists=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==spec['artillery_second_category_factor_continue']:
            returned.append(True);uc.emu_stop();return
        ecx,esp=uc.reg_read(UC_X86_REG_ECX),uc.reg_read(UC_X86_REG_ESP)
        if address==stubs['capture']:
            assert ecx==receiver;calls.append('capture');uc.reg_write(UC_X86_REG_EAX,case.get('capture',0xfeed0001))
        elif address==stubs['class']:
            assert ecx==interface;calls.append('class');uc.reg_write(UC_X86_REG_EAX,case.get('class',1))
        elif address==stubs['eligible']:
            assert ecx==bot and read(uc,esp+4)==category;calls.append('eligible');uc.reg_write(UC_X86_REG_EAX,case.get('eligible',1))
        elif address==stubs['enabled']:
            assert ecx==bot;calls.append('enabled');uc.reg_write(UC_X86_REG_EAX,case.get('enabled',1))
        elif address==stubs['list']:
            assert ecx==bot and read(uc,esp+4)==machine.mem_read(descriptor+4,1)[0];number=len(lists);which=case.get('lists',[0,0,0,0])[number]
            lists.append(which);calls.append(['list',which]);uc.reg_write(UC_X86_REG_EAX,vectors[which])
            if case.get('mutation')=='descriptor' and number==0:machine.mem_write(descriptor+4,b'\xff')
            if case.get('mutation')=='index' and number==1:write(uc,frame+0x44,2)
        elif address==stubs['factor']:
            assert ecx==bot;calls.append('factor');uc.reg_write(UC_X86_REG_EAX,factor)
            if case.get('mutation')=='candidate':write(uc,frame+0x60,ARENA+0x1f00)
    machine.hook_add(UC_HOOK_CODE,hook);machine.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned
    for register in (UC_X86_REG_ESI,UC_X86_REG_ESP):assert machine.reg_read(register)==saved[register]
    return dict(frame=bytes(machine.mem_read(frame,0x220)).hex(),memory=bytes(machine.mem_read(ARENA,0x4000)).hex(),
                calls=calls,ebx=machine.reg_read(UC_X86_REG_EBX),edi=machine.reg_read(UC_X86_REG_EDI),
                ebp=machine.reg_read(UC_X86_REG_EBP),state=state_result(machine,before,1))


def cases():
    result=[{},dict(eligible=0),dict(enabled=0),dict(index=3),dict(count=0),dict(match=-1),dict(empty=True)]
    result += [dict(**{field:value}) for field,value in itertools.product(('class','category','index','handle'),(0,1,3,7,0xffffffff))]
    result += [dict(lists=value) for value in ([1,0,0,0],[0,1,0,0],[0,0,1,0],[0,0,0,1])]
    result += [dict(mutation=value) for value in ('descriptor','index','candidate')]
    special=[0,0x80000000,1,0x80000001,bits(-1),bits(1),0x7f800000,0xff800000,0x7fc12345]
    result += [dict(base=value) for value in special]+[dict(factor=value) for value in special]
    selected=[{},dict(eligible=0),dict(match=-1),dict(factor=0x7fc12345),dict(mutation='descriptor')]
    result += [dict(case,cw=0x7f|pc|rc,depth=depth) for case,pc,rc,depth in itertools.product(selected,(0,0x200,0x300),(0,0x400,0x800,0xc00),(0,2,5))]
    rng=random.Random(0x9a0ca6)
    result += [dict(index=rng.getrandbits(32),capture=rng.getrandbits(32),base=rng.getrandbits(32),factor=rng.getrandbits(32)) for _ in range(32)]
    return result


def compare_second_category_factor(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_second_category_prefix']
    for key,guard,symbol in [
        ('artillery_second_category_prefix',bytes.fromhex('8b0f894c24248b4b20'),'bfv_artillery_second_category_prefix_bridge'),
        ('artillery_second_category_factor',bytes.fromhex('8d432485c074058b4330eb02'),'bfv_artillery_second_category_factor_bridge')]:
        address=spec[key]
        assert original_pe.get_data(address-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
        patch=edited_pe.get_data(address-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
        assert patch[0]==0xe9 and address+5+struct.unpack('<i',patch[1:5])[0]==symbols[symbol]
        assert patch[5:]==b'\x90'*(len(guard)-5)
    results=[]
    for index,case in enumerate(cases()):
        old=run_factor(original,original_pe,spec,entry,case);new=run_factor(edited,edited_pe,spec,entry,case)
        assert old==new,(index,case,{key:(old[key],new[key]) for key in old if old[key]!=new[key]})
        results.append(dict(inputs=case,calls=old['calls']))
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument('--target',choices=('client','server','both'),default='both');args=parser.parse_args()
    for target in ('client','server') if args.target=='both' else (args.target,):
        spec,work=TARGETS[target],PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        found=compare_second_category_factor(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,{key:int(value,16) for key,value in manifest['symbols'].items()})
        (work/'second-category-factor-verification.json').write_text(json.dumps(dict(target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(found),cases=found),indent=2)+'\n')
        print(f'{target}: {len(found)} second category-factor comparisons passed',flush=True)
