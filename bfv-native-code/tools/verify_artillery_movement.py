"""Compare first-pass movement gates at their three original continuations.

Movement-vector and owner-predicate methods are controlled ABI services. All
arithmetic, vector copying, flag tests and native branch decisions execute.
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

def run_movement(image,pe,spec,case):
    m=load_machine(image,pe);frame=STACK+0x8000
    movement=ARENA;movement_vt=ARENA+0x100;vector=ARENA+0x200
    source=ARENA+0x300;target=ARENA+0x400;owner=ARENA+0x500;owner_vt=ARENA+0x600
    alternate_source=ARENA+0x700;alternate_movement=ARENA+0x800
    vector_stub=ARENA+0x8000;predicate_stub=ARENA+0x8100
    start=ARENA+0x9000;seed=ARENA+0x9400
    def w(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def r(p):return struct.unpack('<I',m.mem_read(p,4))[0]
    def words(p,values):m.mem_write(p,struct.pack('<'+'I'*len(values),*values))
    m.mem_write(ARENA,b'\xa5'*0x1000);m.mem_write(frame,b'\xa5'*0x220)
    w(movement,movement_vt);w(movement_vt+0x14,vector_stub)
    words(vector,case.get('vector',[0,0,0]))
    w(source+4,case.get('source_flags',0));w(target+4,case.get('target_flags',2))
    w(alternate_source+4,2);w(owner,owner_vt);w(owner_vt+0x28,predicate_stub)
    w(frame+0x30,0 if case.get('null_movement') else movement)
    w(frame+0x1f8,source);w(frame+0x50,owner)
    w(frame+0x1c,case.get('parameter',bits(100)));w(frame+0x28,case.get('distance',bits(40)))
    # Return vectors that overlap the frame to establish load-before-store order.
    layout=case.get('layout','separate')
    if layout=='output':vector=frame+0x78
    elif layout=='output_before':vector=frame+0x74
    elif layout=='output_after':vector=frame+0x7c
    if layout!='separate':words(vector,case.get('vector',[bits(1),bits(2),bits(3)]))
    for stub in [vector_stub,predicate_stub]:m.mem_write(stub,b'\xc3')
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EBP:target,UC_X86_REG_ESP:frame}
    for reg,value in {**saved,UC_X86_REG_EDI:0x3456789a}.items():m.reg_write(reg,value)
    cw=case.get('cw',0x37f);depth=case.get('depth',0);m.reg_write(UC_X86_REG_FPCW,cw)
    for i in range(depth):w(seed+4*i,[bits(1.234),bits(-5.678)][i%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))
    entry=spec['artillery_movement_gate']
    preload+=b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff)
    m.mem_write(start,preload)
    exits={spec['artillery_movement_continue']:'continue',spec['artillery_movement_score']:'score',
           spec['artillery_movement_unit_score']:'unit_score'}
    calls=[];before=[];returned=[]
    def hook(uc,address,size,data):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in exits:returned.append(exits[address]);uc.emu_stop();return
        ecx=uc.reg_read(UC_X86_REG_ECX)
        if address==vector_stub:
            assert ecx==movement
            assert [r(frame+x) for x in [0x78,0x7c,0x80]]==[0,0,0]
            calls.append('vector');uc.reg_write(UC_X86_REG_EAX,vector)
            mutation=case.get('mutation')
            if mutation=='movement':w(frame+0x30,alternate_movement)
            elif mutation=='source':w(frame+0x1f8,alternate_source)
            elif mutation=='target':w(target+4,0)
            elif mutation=='distance':w(frame+0x28,bits(60))
            elif mutation=='vector':words(vector,[bits(16),0,0])
        elif address==predicate_stub:
            assert ecx==owner;calls.append('predicate')
            uc.reg_write(UC_X86_REG_EAX,case.get('predicate',0))
            mutation=case.get('mutation')
            if mutation=='predicate_distance':w(frame+0x28,bits(60))
            elif mutation=='predicate_parameter':w(frame+0x1c,bits(70))
            elif mutation=='predicate_vector':words(frame+0x78,[bits(16),0,0])
            elif mutation=='predicate_flags':w(source+4,2);w(target+4,0)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(returned)==1
    for reg,value in saved.items():assert m.reg_read(reg)==value
    assert m.reg_read(UC_X86_REG_EDI)==(0 if case.get('null_movement') else movement)
    after=[m.reg_read(reg) for reg in FLOAT_STATE]
    occupied=[i for i in range(8) if (before[2]>>(2*i))&3!=3]
    assert after[0]==cw and after[2]==before[2]
    assert all(after[3+i]==before[3+i] for i in occupied)
    return dict(exit=returned[0],calls=calls,frame=bytes(m.mem_read(frame,0x220)).hex(),
                arena=bytes(m.mem_read(ARENA,0x1000)).hex(),
                state=after[:3]+[after[3+i] for i in occupied])

def movement_cases():
    cases=[{},dict(null_movement=True)]
    cases += [dict(source_flags=s,target_flags=t) for s,t in itertools.product([0,1,2,3,0x80000000,0xfffffffd],repeat=2)]
    cases += [dict(predicate=p) for p in [0,1,0x100,0x101,0x80000000,0xffffffff]]
    special=[0,0x80000000,1,0x80000001,bits(-100),bits(50)-1,bits(50),bits(50)+1,
             bits(90)-1,bits(90),bits(90)+1,0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(**{field:value},predicate=p) for field,value,p in itertools.product(['parameter','distance'],special,[0,1])]
    cases += [dict(vector=v) for v in [[0,0,0],[bits(9),bits(12),0],[bits(15)-1,0,0],
             [bits(15),0,0],[bits(15)+1,0,0]]]
    cases += [dict(vector=[v,0x7fc23456,1]) for v in special]
    cases += [dict(vector=[v,0,0]) for v in special]
    cases += [dict(layout=l) for l in ['output','output_before','output_after']]
    cases += [dict(mutation=x) for x in ['movement','source','target','distance','vector',
              'predicate_distance','predicate_parameter','predicate_vector','predicate_flags']]
    selected=[{},dict(null_movement=True),dict(distance=bits(60)),dict(predicate=1),
              dict(vector=[bits(15)+1,0,0]),dict(vector=[0x7f812345,0,0]),
              dict(distance=0x7fc12345),dict(parameter=0x7f812345),dict(source_flags=2)]
    cases += [dict(c,cw=0x7f|pc|rc,depth=depth) for c,pc,rc,depth in
              itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    rng=random.Random(0x99fb31)
    for _ in range(40):
        cases.append(dict(parameter=bits(rng.uniform(-100,500)),distance=bits(rng.uniform(-100,500)),
            vector=[bits(rng.uniform(-20,20)) for _ in range(3)],predicate=rng.choice([0,1,256,257])))
    return cases

def compare_artillery_movement(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_movement_gate'];jump=edited_pe.get_data(entry-0x400000,5)
    assert jump[0]==0xe9 and entry+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_artillery_movement_gate_bridge']
    cases=[]
    for index,case in enumerate(movement_cases()):
        try:
            old=run_movement(original,original_pe,spec,case);new=run_movement(edited,edited_pe,spec,case)
            assert old==new,(case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        except Exception as error:raise RuntimeError(f'movement gate case {index}: {case}') from error
        cases.append(dict(inputs=case,exit=old['exit'],calls=old['calls']))
    return cases

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_artillery_movement(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
             {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'artillery-movement-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} artillery movement comparisons passed',flush=True)
