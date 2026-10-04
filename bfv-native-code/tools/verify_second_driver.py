"""Compare the second-pass distance and driver predicate stage.

Object vector/event methods are controlled. Actual original/source helper math,
field loads, callback rereads and scoring instructions execute.
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
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_FPCW)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK
from verify_constant_returns import FLOAT_STATE

def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]

def setup(image,pe,case):
    m=load_machine(image,pe);frame=STACK+0x8000;seed=ARENA+0x9400
    m.mem_write(ARENA,b'\xa5'*0x2000);m.mem_write(frame,b'\xa5'*0x220)
    def w(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def r(p):return struct.unpack('<I',m.mem_read(p,4))[0]
    def words(p,values):m.mem_write(p,struct.pack('<'+'I'*len(values),*values))
    cw=case.get('cw',0x37f);depth=case.get('depth',0);m.reg_write(UC_X86_REG_FPCW,cw)
    for i in range(depth):w(seed+4*i,[bits(1.234),bits(-5.678)][i%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))
    return m,frame,w,r,words,preload

def state_result(m,before,extra=0):
    after=[m.reg_read(reg) for reg in FLOAT_STATE]
    old=[i for i in range(8) if (before[2]>>(2*i))&3!=3]
    used=[i for i in range(8) if (after[2]>>(2*i))&3!=3]
    assert len(used)==len(old)+extra and after[0]==before[0]
    assert all(after[3+i]==before[3+i] for i in old)
    return after[:3]+[after[3+i] for i in used]

def run_second_driver(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    target=ARENA;target_table=ARENA+0x80;alternate_table=ARENA+0x100
    driver=ARENA+0x200;owner=ARENA+0x280;event_receiver=ARENA+0x300;event_table=ARENA+0x380
    event=ARENA+0x400;event_data=ARENA+0x480;alternate_data=ARENA+0x500
    receiver=ARENA+0x600;receiver_table=ARENA+0x680;alternate_receiver=ARENA+0x700;predicate_table=ARENA+0x780
    first=ARENA+0x900;second=ARENA+0xa00
    position_stub=ARENA+0x8000;alternate_position_stub=ARENA+0x8100
    notify_stub=ARENA+0x8200;predicate_stub=ARENA+0x8300;alternate_predicate_stub=ARENA+0x8400;start=ARENA+0x9000
    if case.get('layout')=='first_output':first=frame+0x130
    if case.get('layout')=='second_output':second=frame+0x138
    if case.get('layout')=='overlap':first=frame+0x130;second=frame+0x13c
    if case.get('layout')=='same':second=first
    words(first,[bits(1),bits(2),case.get('first_z',bits(3))]);words(second,[case.get('second_x',bits(4)),bits(5),bits(6)])
    w(target,target_table);w(target_table+0x18,position_stub);w(alternate_table+0x18,alternate_position_stub)
    w(driver+4,owner);w(owner+0x20,event_receiver);w(event_receiver,event_table);w(event_table+0xa0,notify_stub)
    w(event+0x14,event_data);w(event_data+4,case.get('event_word',0x12345678));w(alternate_data+4,0xfedcba98)
    w(receiver,receiver_table);w(receiver_table+0x84,predicate_stub)
    w(alternate_receiver,predicate_table);w(predicate_table+0x84,alternate_predicate_stub)
    w(frame+0x18,target);w(frame+0xf8,receiver);w(frame+0x1fc,driver)
    w(frame+0x20,case.get('parameter',bits(100)));w(frame+0x28,case.get('distance',bits(200)))
    for stub in [position_stub,alternate_position_stub]:m.mem_write(stub,b'\xc3')
    m.mem_write(notify_stub,b'\xc2\x04\x00')
    for stub in [predicate_stub,alternate_predicate_stub]:m.mem_write(stub,b'\xc2\x08\x00')
    entry=spec['artillery_second_driver_gate']
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EBP:0x34567890,
           UC_X86_REG_EDI:0 if case.get('null_driver') else driver,UC_X86_REG_ESP:frame}
    for reg,value in saved.items():m.reg_write(reg,value)
    preload+=b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff);m.mem_write(start,preload)
    before=[];calls=[];exits=[];position_calls=[]
    def hook(uc,address,size,data_unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in [spec['artillery_second_driver_accept'],spec['artillery_second_reject']]:
            exits.append(address==spec['artillery_second_driver_accept']);uc.emu_stop();return
        ecx=uc.reg_read(UC_X86_REG_ECX);mutation=case.get('mutation')
        if address in [position_stub,alternate_position_stub]:
            assert ecx==target
            position_calls.append(address==alternate_position_stub);calls.append(['position',position_calls[-1]])
            uc.reg_write(UC_X86_REG_EAX,first if len(position_calls)==1 else second)
            if len(position_calls)==1:
                if mutation=='target':w(frame+0x18,0)
                elif mutation=='target_table':w(target,alternate_table)
                elif mutation=='first_coordinate':w(first+8,bits(-9))
                elif mutation=='second_driver':w(frame+0x1fc,0)
            else:
                if mutation=='receiver':w(frame+0xf8,alternate_receiver)
                elif mutation=='receiver_table':w(receiver,predicate_table)
                elif mutation=='second_coordinate':w(second,bits(-11))
                elif mutation=='driver':w(frame+0x1fc,0)
                elif mutation=='first_after_second':w(first+8,bits(-13))
        elif address==notify_stub:
            assert ecx==event_receiver and r(uc.reg_read(UC_X86_REG_ESP)+4)==2
            calls.append(['event_2']);uc.reg_write(UC_X86_REG_EAX,event)
            if mutation=='event_receiver':w(frame+0xf8,alternate_receiver)
            elif mutation=='event_table':w(receiver,predicate_table)
            elif mutation=='event_method':w(receiver_table+0x84,alternate_predicate_stub)
            elif mutation=='event_data':w(event+0x14,alternate_data)
            elif mutation=='event_driver':w(frame+0x1fc,0)
            elif mutation=='point':words(frame+0x138,[bits(-7),bits(-8)])
        elif address in [predicate_stub,alternate_predicate_stub]:
            assert ecx in [receiver,alternate_receiver]
            esp=uc.reg_read(UC_X86_REG_ESP);value=r(esp+4);point=r(esp+8);assert point==frame+0x138
            calls.append(['predicate',address==alternate_predicate_stub,ecx,value,r(point),r(point+4)])
            uc.reg_write(UC_X86_REG_EAX,case.get('predicate',0))
            if mutation=='predicate':w(frame+0xf8,0);w(frame+0x18,0)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg in [UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_ESP]:assert m.reg_read(reg)==saved[reg]
    return dict(accepted=exits[0],calls=calls,ebx=m.reg_read(UC_X86_REG_EBX),ebp=m.reg_read(UC_X86_REG_EBP),
        frame=bytes(m.mem_read(frame,0x220)).hex(),memory=bytes(m.mem_read(ARENA,0x2000)).hex(),state=state_result(m,before))

def second_driver_cases():
    cases=[{},dict(null_driver=True),dict(distance=bits(1)),dict(distance=bits(1),null_driver=True)]
    cases += [dict(predicate=v) for v in [0,1,255,256,0x10001,0x80000000,0xffffffff]]
    special=[0,0x80000000,1,0x80000001,bits(-100),bits(100),0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(**{field:value}) for field,value in itertools.product(['parameter','distance','first_z','second_x'],special)]
    cases += [dict(mutation=v) for v in ['target','target_table','first_coordinate','second_driver','first_after_second','event_driver','receiver','receiver_table','second_coordinate','driver','event_receiver','event_table','event_method','event_data','point','predicate']]
    cases += [dict(layout=v) for v in ['first_output','second_output','overlap','same']]
    cases += [dict(parameter=bits(1),distance=bits(1)+d) for d in [-1,0,1]]
    cases += [dict(first_z=0x7f812345,second_x=0x7f812346,layout=v) for v in ['first_output','second_output','overlap','same']]
    selected=[{},dict(null_driver=True),dict(distance=bits(1)),dict(first_z=0x7f812345),dict(second_x=0x7fc12345),dict(layout='overlap')]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    rng=random.Random(0x9a0bc9)
    cases += [dict(parameter=rng.getrandbits(32),distance=rng.getrandbits(32),first_z=rng.getrandbits(32),second_x=rng.getrandbits(32),predicate=rng.getrandbits(32)) for _ in range(40)]
    return cases

def compare_second_driver(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_second_driver_gate'];guard=bytes.fromhex('d9442420d80d')+struct.pack('<I',spec['artillery_query_scale'])
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_second_driver_gate_bridge']
    assert patched[5:]==b'\x90'*5
    results=[]
    for i,case in enumerate(second_driver_cases()):
        old=run_second_driver(original,original_pe,spec,case);new=run_second_driver(edited,edited_pe,spec,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,accepted=old['accepted'],calls=old['calls'],ebx=old['ebx']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_second_driver(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'second-driver-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} second-driver comparisons passed',flush=True)
