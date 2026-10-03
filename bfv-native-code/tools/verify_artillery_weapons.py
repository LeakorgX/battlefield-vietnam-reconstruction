"""Compare the entire first-pass weapon-selection loop at native continuations.

Inventory, category and availability methods are controlled ABI services. Rating
arithmetic, distance gating, selection, padding and dynamic iteration execute.
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
    UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,
    UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK
from verify_constant_returns import FLOAT_STATE


def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]


def run_weapons(image,pe,spec,case):
    m=load_machine(image,pe)
    frame=STACK+0x8000;owner=ARENA;owner_vt=ARENA+0x200
    component=ARENA+0x400;component_vt=ARENA+0x600;vector=ARENA+0x800
    entries=ARENA+0xa00;record=ARENA+0xc00;alternate_vector=ARENA+0xe00
    alternate_entries=ARENA+0xf00;behavior=ARENA+0x1800
    inventory_stub=ARENA+0x8000;category_stub=ARENA+0x8100;available_stub=ARENA+0x8200
    start=ARENA+0x9000;seed=ARENA+0x9400
    def w(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def r(p):return struct.unpack('<I',m.mem_read(p,4))[0]
    m.mem_write(frame,b'\xa5'*0x220)
    w(owner,owner_vt);w(owner_vt+0x1c,inventory_stub)
    w(component,component_vt);w(component_vt+0x3c,category_stub)
    w(frame+0x50,owner);w(frame+0x94,component);w(frame+0x18,record)
    w(frame+0x28,case.get('distance',bits(100)));w(frame+0xa4,behavior)
    weapons=[];parameters=[]
    descriptions=case.get('weapons',[{}])
    for i,description in enumerate(descriptions):
        weapon=ARENA+0x2000+i*0x200;vt=weapon+0x40;param=weapon+0x80;table=weapon+0x100
        weapons.append(weapon);parameters.append(param)
        w(entries+i*4,weapon if description is not None else 0)
        w(alternate_entries+i*4,weapon if description is not None else 0)
        w(weapon,vt);w(vt+0x24,available_stub);w(weapon+0x0c,param);w(param+0x4c,table)
        description=description or {}
        for category in range(4):w(table+4*category,description.get('rating',100)+category)
        w(param+0x28,description.get('minimum',bits(10)))
        w(param+0x2c,description.get('maximum',bits(200)))
        w(record+0x34+i*4,description.get('a',bits(3)))
        w(record+0x54+i*4,description.get('b',bits(2)))
    w(vector+4,0 if case.get('null_begin') else entries)
    w(vector+8,entries+4*len(descriptions))
    w(alternate_vector+4,alternate_entries);w(alternate_vector+8,alternate_entries+4*len(descriptions))
    for stub in [inventory_stub,category_stub,available_stub]:m.mem_write(stub,b'\xc3')
    saved={UC_X86_REG_ESI:ARENA+0x1a00,UC_X86_REG_EBP:ARENA+0x1c00,UC_X86_REG_ESP:frame}
    for reg,value in {**saved,UC_X86_REG_EBX:behavior,UC_X86_REG_EDI:0x11223344}.items():m.reg_write(reg,value)
    cw=case.get('cw',0x37f);depth=case.get('depth',0)
    m.reg_write(UC_X86_REG_FPCW,cw)
    for i in range(depth):w(seed+4*i,[0x3f812345,0xbf654321,0x400abcde][i%3])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))
    preload+=b'\xe9'+struct.pack('<I',(spec['artillery_weapons']-start-len(preload)-5)&0xffffffff)
    m.mem_write(start,preload)
    calls=[];exits=[];before=[]
    def hook(uc,address,size,data):
        if address==spec['artillery_weapons']:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in [spec['artillery_weapons_accept'],spec['artillery_filter_reject']]:
            exits.append(address==spec['artillery_weapons_accept']);uc.emu_stop();return
        ecx=uc.reg_read(UC_X86_REG_ECX)
        if address==inventory_stub:
            assert ecx==owner
            assert r(frame+0x1c)==0 and r(frame+0x34)==0 and r(frame+0x60)==0xffffffff
            calls.append(['inventory']);uc.reg_write(UC_X86_REG_EAX,vector)
            if case.get('mutation')=='inventory':
                w(frame+0x34,bits(50));w(frame+0xa4,behavior+4)
        elif address==category_stub:
            assert ecx==component;category=case.get('category',0)
            calls.append(['category',category]);uc.reg_write(UC_X86_REG_EAX,category)
            if case.get('mutation')=='category':
                w(frame+0x28,bits(0));w(frame+0xa4,behavior+8)
        elif address==available_stub:
            index=weapons.index(ecx);description=descriptions[index] or {}
            available=description.get('available',4)
            calls.append(['available',index,available]);uc.reg_write(UC_X86_REG_EAX,available&0xffffffff)
            if index==0:
                if case.get('mutation')=='shrink':w(vector+8,entries+4)
                if case.get('mutation')=='relocate':w(frame+0xac,alternate_vector);w(entries+4,0)
                if case.get('mutation')=='parameters':
                    w(parameters[index]+0x28,bits(1000));w(frame+0xbc,bits(17))
                if case.get('mutation')=='history':w(frame+0x38,record+0x58);w(frame+0xa4,behavior+12)
    m.hook_add(UC_HOOK_CODE,hook)
    m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1 and m.reg_read(UC_X86_REG_FPCW)==cw
    for reg,value in saved.items():assert m.reg_read(reg)==value
    after=[m.reg_read(reg) for reg in FLOAT_STATE]
    assert after[2]==before[2],('x87 tags',before,after)
    occupied=[i for i in range(8) if (before[2]>>(2*i))&3 != 3]
    assert all(after[3+i]==before[3+i] for i in occupied),('retained x87',before,after)
    return dict(accepted=exits[0],calls=calls,frame=bytes(m.mem_read(frame,0x220)).hex(),
                arena=bytes(m.mem_read(ARENA,0x4000)).hex(),ebx=m.reg_read(UC_X86_REG_EBX),
                float_state=after[:3]+[after[3+i] for i in occupied])


def weapon_cases():
    cases=[dict(weapons=[]),dict(weapons=[None]),dict(null_begin=True)]
    cases += [dict(weapons=[{}]*n) for n in range(1,9)]
    cases += [dict(weapons=[None,{},None,dict(rating=200),{}])]
    cases += [dict(weapons=[dict(available=n)]) for n in [0,1,2,255,65536,0x7fffffff,0x80000000,0xfffffffe,0xffffffff]]
    cases += [dict(weapons=[dict(rating=n)]) for n in [0,1,-1,0x7fffffff,-0x80000000,16777217]]
    cases += [dict(distance=d,weapons=[dict(minimum=bits(10))]) for d in
              [0,0x80000000,bits(10)-1,bits(10),bits(10)+1,bits(-10),0x7f800000,0xff800000,0x7fc12345,0x7f812345]]
    special=[0,0x80000000,1,0x80000001,bits(-2),bits(0.3333333),0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(weapons=[{field:value}]) for field,value in itertools.product(['a','b','minimum','maximum'],special)]
    for mutation in ['inventory','category','shrink','relocate','parameters','history']:
        cases.append(dict(weapons=[{},dict(rating=200),dict(rating=300)],mutation=mutation))
    selected=[{},dict(weapons=[dict(rating=16777217,available=3,a=bits(0.1),b=bits(0.2))]),
              dict(weapons=[dict(a=0x7fc12345)]),dict(weapons=[dict(maximum=0x7f812345)]),
              dict(weapons=[{},dict(rating=200),None])]
    cases += [dict(c,cw=0x7f|pc|rc,depth=depth) for c,pc,rc,depth in
              itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,6])]
    rng=random.Random(0x99f8d7)
    for _ in range(40):
        weapons=[dict(rating=rng.randint(-100,500),available=rng.choice([0,1,7,0xffffffff]),
                      a=bits(rng.uniform(-4,4)),b=bits(rng.uniform(-4,4)),
                      minimum=bits(rng.uniform(0,150)),maximum=bits(rng.uniform(0,400)))
                 for _ in range(rng.randrange(1,9))]
        cases.append(dict(weapons=weapons,category=rng.randrange(4)))
    return cases


def compare_artillery_weapons(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_weapons'];jump=edited_pe.get_data(entry-0x400000,5)
    assert jump[0]==0xe9 and entry+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_artillery_weapons_bridge']
    cases=[]
    for index,case in enumerate(weapon_cases()):
        try:
            old=run_weapons(original,original_pe,spec,case);new=run_weapons(edited,edited_pe,spec,case)
            assert old==new,(case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        except Exception as error:raise RuntimeError(f'weapon phase case {index}: {case}') from error
        # Large frame/arena snapshots are compared above, not stored in reports.
        cases.append(dict(inputs=case,accepted=old['accepted'],calls=old['calls'],ebx=old['ebx']))
    return cases


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_artillery_weapons(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
             {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'artillery-weapons-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} artillery weapon comparisons passed',flush=True)
