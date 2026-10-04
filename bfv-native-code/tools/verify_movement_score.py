"""Compare movement scoring and its cross-product/event-scalar helpers.

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

def run_helper(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    left=ARENA+0x100;right=ARENA+0x200
    component=ARENA+0x400;owner=ARENA+0x500;receiver=ARENA+0x600;vt=ARENA+0x700
    event=ARENA+0x800;data=ARENA+0x900;alt_event=ARENA+0xa00;alt_data=ARENA+0xb00
    notify=ARENA+0x8000;start=ARENA+0x9000;stop=ARENA+0x9200
    layout=case.get('layout','separate')
    if layout=='same':right=left
    elif layout=='before':right=left-4
    elif layout=='after':right=left+4
    elif layout=='after_two':right=left+8
    elif layout=='unaligned':left+=1;right+=3
    words(left,case.get('left',[bits(1),bits(2),bits(3)]))
    words(right,case.get('right',[bits(4),bits(5),bits(6)]))
    w(component+4,owner);w(owner+0x20,receiver);w(receiver,vt);w(vt+0xa0,notify)
    w(event+0x14,data);w(data+8,case.get('value',bits(1.25)))
    w(alt_event+0x14,alt_data);w(alt_data+8,bits(-2.5));m.mem_write(notify,b'\xc2\x04\x00')
    kind=case['kind'];entry=spec[kind]
    args=[right] if kind=='vector_cross_assign' else []
    this=left if args else component
    words(frame,[stop,*args]);m.mem_write(stop,b'\x90')
    saved={reg:0x12340000+i for i,reg in enumerate([UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP])}
    for reg,value in {**saved,UC_X86_REG_ESP:frame,UC_X86_REG_ECX:this}.items():m.reg_write(reg,value)
    preload+=b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff);m.mem_write(start,preload)
    before=[];calls=[];returned=[]
    def hook(uc,address,size,data_unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==stop:returned.append(True);uc.emu_stop();return
        if address==notify:
            assert uc.reg_read(UC_X86_REG_ECX)==receiver and r(uc.reg_read(UC_X86_REG_ESP)+4)==2
            calls.append('event_2');uc.reg_write(UC_X86_REG_EAX,event)
            if case.get('mutation')=='event':uc.reg_write(UC_X86_REG_EAX,alt_event)
            elif case.get('mutation')=='data':w(event+0x14,alt_data)
            elif case.get('mutation')=='owner':w(component+4,0);w(owner+0x20,0)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==frame+4+4*len(args)
    for reg,value in saved.items():assert m.reg_read(reg)==value
    if args:assert m.reg_read(UC_X86_REG_EAX)==left
    return dict(memory=bytes(m.mem_read(ARENA,0x2000)).hex(),calls=calls,state=state_result(m,before,0 if args else 1))

def run_score(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    movement=ARENA;driver=ARENA+0x400;alt_driver=ARENA+0x800
    vector=ARENA+0xc00;driver_vector=ARENA+0xd00;alt_vector=ARENA+0xe00
    vector_stub=ARENA+0x8000;notify_stub=ARENA+0x8100;predicate_stub=ARENA+0x8200
    start=ARENA+0x9000;source=ARENA+0x1100;target=ARENA+0x1200;gate_owner=ARENA+0x1300;gate_vt=ARENA+0x1400
    for index,obj in enumerate([movement,driver,alt_driver]):
        vt=obj+0x80;owner=obj+0x100;receiver=obj+0x180;receiver_vt=obj+0x200;event=obj+0x280;data=obj+0x300
        w(obj,vt);w(vt+0x14,vector_stub);w(obj+4,owner);w(owner+0x20,receiver)
        w(receiver,receiver_vt);w(receiver_vt+0xa0,notify_stub);w(event+0x14,data)
        w(data+8,case.get('scalar' if index==0 else 'driver_scalar',bits(10) if index==0 else bits(2)))
    words(vector,case.get('vector',[bits(1),bits(2),bits(3)]))
    words(driver_vector,case.get('driver_vector',[bits(.1),bits(.2),bits(.3)]))
    words(alt_vector,[bits(-1),bits(-2),bits(-3)])
    w(frame+0x1fc,driver if case.get('driver',True) else 0)
    w(frame+0x30,movement);w(frame+0x28,case.get('distance',bits(200)));w(frame+0x1c,case.get('parameter',bits(100)))
    words(frame+0x64,case.get('basis',[bits(1),0,0]))
    layout=case.get('layout','separate')
    if layout in ['projection_a','projection_b','driver_copy','basis','cross']:
        vector=frame+{'projection_a':0x38,'projection_b':0x54,'driver_copy':0x88,'basis':0x64,'cross':0x180}[layout]
        words(vector,case.get('vector',[bits(1),bits(2),bits(3)]))
    elif layout=='driver_overlap':driver_vector=frame+0x8c;words(driver_vector,case.get('driver_vector',[bits(.1),bits(.2),bits(.3)]))
    for stub in [vector_stub,predicate_stub]:m.mem_write(stub,b'\xc3')
    m.mem_write(notify_stub,b'\xc2\x04\x00')
    w(frame+0x1f8,source);w(source+4,case.get('source_flags',0));w(target+4,case.get('target_flags',2))
    w(frame+0x50,gate_owner);w(gate_owner,gate_vt);w(gate_vt+0x28,predicate_stub)
    entry=spec['artillery_movement_gate'] if case.get('integrated') else spec['artillery_movement_scoring']
    movement_value=0 if case.get('null_movement') else movement
    w(frame+0x30,movement_value)
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EBP:target,
           UC_X86_REG_EDI:movement_value,UC_X86_REG_ESP:frame}
    for reg,value in saved.items():m.reg_write(reg,value)
    preload+=b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff);m.mem_write(start,preload)
    before=[];calls=[];returned=[]
    def hook(uc,address,size,data_unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==spec['artillery_movement_score']:returned.append(True);uc.emu_stop();return
        ecx=uc.reg_read(UC_X86_REG_ECX);mutation=case.get('mutation')
        if address==vector_stub:
            assert ecx in [movement,driver,alt_driver]
            who={movement:'movement',driver:'driver',alt_driver:'alternate_driver'}[ecx];calls.append(who+'_vector')
            uc.reg_write(UC_X86_REG_EAX,{movement:vector,driver:driver_vector,alt_driver:alt_vector}[ecx])
            if who=='driver' and mutation=='driver_vector':w(frame+0xc0,bits(20));words(frame+0x64,[0,bits(1),0])
            if who=='movement' and mutation=='movement_vector':words(frame+0xb0,[bits(3),bits(-2),bits(1)]);w(frame+0x30,0)
        elif address==notify_stub:
            receivers={movement+0x180:movement,driver+0x180:driver,alt_driver+0x180:alt_driver}
            assert ecx in receivers and r(uc.reg_read(UC_X86_REG_ESP)+4)==2
            obj=receivers[ecx];calls.append(('movement' if obj==movement else 'driver')+'_event_2')
            uc.reg_write(UC_X86_REG_EAX,obj+0x280)
            if obj==driver and mutation=='driver_event':w(frame+0x1fc,alt_driver)
            if obj==movement and mutation=='movement_event':w(frame+0xc0,bits(-10));w(frame+0x30,0);words(frame+0x64,[0,0,bits(1)])
            if obj==movement and mutation=='scalar_field':w(obj+0x280+0x14,driver+0x300)
        elif address==predicate_stub:
            assert ecx==gate_owner;calls.append('predicate');uc.reg_write(UC_X86_REG_EAX,case.get('predicate',0))
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned
    for reg,value in saved.items():assert m.reg_read(reg)==value
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),memory=bytes(m.mem_read(ARENA,0x2000)).hex(),
                calls=calls,state=state_result(m,before),score=r(frame+0x20))

def helper_cases():
    special=[0,0x80000000,1,0x80000001,0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cross=[{},dict(left=[bits(.1),bits(-.2),bits(1.2345)],right=[bits(2.3456),bits(.3333333),1])]
    cross += [dict(left=[v,0x7fc23456,1],right=[bits(-1),v,0x7f812346]) for v in special]
    cases=[dict(c,kind='vector_cross_assign',layout=l) for c,l in itertools.product(cross,['separate','same','before','after','after_two','unaligned'])]
    cases += [dict(kind='component_event2_scalar',value=v) for v in special+[bits(1.25),bits(-2.5)]]
    cases += [dict(kind='component_event2_scalar',mutation=x) for x in ['event','data','owner']]
    selected=[dict(kind='vector_cross_assign',**cross[1]),dict(kind='vector_cross_assign',**cross[-1]),
              dict(kind='component_event2_scalar',value=0x7f812345)]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    rng=random.Random(0x6eb030)
    cases += [dict(kind='vector_cross_assign',left=[rng.getrandbits(32) for _ in range(3)],right=[rng.getrandbits(32) for _ in range(3)]) for _ in range(24)]
    return cases

def score_cases():
    cases=[{},dict(null_movement=True),dict(driver=False),dict(distance=bits(100)),dict(distance=bits(50))]
    special=[0,0x80000000,1,0x80000001,bits(-100),bits(100)-1,bits(100),bits(100)+1,0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(**{field:value}) for field,value in itertools.product(['distance','parameter','scalar','driver_scalar'],special)]
    cases += [dict(scalar=v) for v in [bits(7)-1,bits(7),bits(7)+1]]
    cases += [dict(vector=[v,0x7fc23456,1]) for v in special]
    cases += [dict(basis=[v,1,0x7f812346]) for v in special]
    cases += [dict(layout=l) for l in ['projection_a','projection_b','driver_copy','basis','cross','driver_overlap']]
    cases += [dict(mutation=x) for x in ['driver_event','driver_vector','movement_event','movement_vector','scalar_field']]
    selected=[{},dict(driver=False),dict(null_movement=True),dict(distance=bits(50)),
              dict(scalar=bits(7)),dict(scalar=0x7f812345),dict(vector=[0x7f812345,0,0]),dict(layout='projection_a')]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,1,3])]
    cases += [dict(c,integrated=True) for c in [{},dict(driver=False),dict(null_movement=True),dict(predicate=1),
              dict(distance=bits(40)),dict(source_flags=2),dict(mutation='driver_event'),dict(vector=[0x7f812345,0,0])]]
    rng=random.Random(0x99fc01)
    for _ in range(40):
        cases.append(dict(distance=bits(rng.uniform(-20,500)),parameter=bits(rng.uniform(-20,300)),
            scalar=bits(rng.uniform(-20,50)),driver_scalar=bits(rng.uniform(-20,50)),
            vector=[bits(rng.uniform(-20,20)) for _ in range(3)],basis=[bits(rng.uniform(-1,1)) for _ in range(3)]))
    return cases

def compare_movement_score(original,original_pe,edited,edited_pe,spec,symbols):
    for key,symbol in [('vector_cross_assign','bfv_vector_cross_assign'),('component_event2_scalar','bfv_component_event2_scalar'),
                       ('artillery_movement_scoring','bfv_artillery_movement_score_bridge')]:
        entry=spec[key];jump=edited_pe.get_data(entry-0x400000,5)
        assert jump[0]==0xe9 and entry+5+struct.unpack('<i',jump[1:])[0]==symbols[symbol]
    results=[]
    for group,runner,cases in [('helper',run_helper,helper_cases()),('score',run_score,score_cases())]:
        for index,case in enumerate(cases):
            try:
                old=runner(original,original_pe,spec,case);new=runner(edited,edited_pe,spec,case)
                assert old==new,(case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
            except Exception as error:raise RuntimeError(f'movement {group} case {index}: {case}') from error
            results.append(dict(group=group,inputs=case,calls=old['calls'],score=old.get('score')))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_movement_score(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
             {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'movement-score-verification.json').write_text(json.dumps(dict(target=target,original_sha256=spec['sha'],
            compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} movement score/helper comparisons passed',flush=True)
