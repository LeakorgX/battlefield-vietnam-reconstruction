"""Second-pass weapon scan with controlled inventory/category/availability.

Actual signed rating arithmetic, strict selection, dynamic scan and padding run.
"""
import argparse,hashlib,itertools,json,random,struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP)
from build import PROJECT,GAME,TARGETS
from native_oracle import ARENA
from verify_category_score import setup
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import state_result,bits

def run_second_weapons(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    owner,owner_table,component,component_table,vector,entries=[ARENA+n for n in (0x100,0x200,0x300,0x400,0x500,0x600)]
    inventory_stub,category_stub,available_stub=ARENA+0x8000,ARENA+0x8100,ARENA+0x8200
    if case.get('alias')=='owner':owner=frame+0x40
    elif case.get('alias')=='vector':vector=frame+0x38
    w(owner,owner_table);w(owner_table+0x1c,inventory_stub)
    w(component,component_table);w(component_table+0x3c,category_stub)
    w(frame+0x50,owner);w(frame+0x94,component)
    descriptions=case.get('weapons',[{}]);weapons=[];parameters=[]
    alternate_parameters=ARENA+0x1800
    w(alternate_parameters+0x2c,bits(333))
    for i,description in enumerate(descriptions):
        weapon=ARENA+0x2000+i*0x200;table=weapon+0x40;parameter=weapon+0x80;ratings=weapon+0x100
        if i==0 and case.get('alias')=='weapon':weapon=frame+0x70
        weapons.append(weapon);parameters.append(parameter);w(entries+i*4,0 if description is None else weapon)
        w(weapon,table);w(weapon+0x0c,parameter);w(table+0x24,available_stub);w(parameter+0x4c,ratings)
        description=description or {}
        for category in range(4):w(ratings+category*4,description.get('rating',100)+category)
        w(parameter+0x2c,description.get('maximum',bits(200)))
    w(vector+4,0 if case.get('null_begin') else entries)
    w(vector+8,entries+4*len(descriptions)+case.get('end_extra',0))
    for stub in [inventory_stub,category_stub,available_stub]:m.mem_write(stub,b'\xc3')
    saved={UC_X86_REG_ESI:0x12345678,UC_X86_REG_ESP:frame}
    for reg,value in {**saved,UC_X86_REG_EBX:0x23456789,UC_X86_REG_EBP:0x34567890,
        UC_X86_REG_EDI:0x45678901}.items():m.reg_write(reg,value)
    start,entry=ARENA+0x9000,spec['artillery_second_distance_continue']
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];calls=[];exits=[];mutation=case.get('mutation')
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in [spec['artillery_second_weapons_accept'],spec['artillery_second_reject']]:
            exits.append(address==spec['artillery_second_weapons_accept']);uc.emu_stop();return
        ecx=uc.reg_read(UC_X86_REG_ECX)
        if address==inventory_stub:
            assert ecx==owner and [r(frame+n) for n in [0x20,0x74,0x40]]==[0,0xffffffff,0]
            calls.append('inventory');uc.reg_write(UC_X86_REG_EAX,vector)
            if mutation=='best':w(frame+0x40,case.get('best',bits(-1)))
        elif address==category_stub:
            assert ecx==component;category=case.get('category',0)
            calls.append(['category',category]);uc.reg_write(UC_X86_REG_EAX,category)
            if mutation=='category':w(parameters[0]+0x2c,bits(444))
        elif address==available_stub:
            i=weapons.index(ecx);description=descriptions[i] or {};available=description.get('available',4)
            calls.append(['available',i,available]);uc.reg_write(UC_X86_REG_EAX,available&0xffffffff)
            if i==0:
                if mutation=='shrink':w(vector+8,entries+4)
                elif mutation=='empty':w(vector+4,0)
                elif mutation=='relocate':w(vector+4,entries+4);w(vector+8,entries+4*len(descriptions))
                elif mutation=='parameters':w(weapons[i]+0x0c,alternate_parameters)
                elif mutation=='rating':w(frame+0x70,case.get('rating_bits',bits(17)))
                elif mutation=='scores':w(frame+0x40,case.get('best',bits(1000)))
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg,value in saved.items():assert m.reg_read(reg)==value
    assert m.reg_read(UC_X86_REG_EBP)==vector
    return dict(accepted=exits[0],calls=calls,ebx=m.reg_read(UC_X86_REG_EBX),
        frame=bytes(m.mem_read(frame,0x220)).hex(),memory=bytes(m.mem_read(ARENA,0x4000)).hex(),state=state_result(m,before))

def second_weapons_cases():
    cases=[dict(weapons=[]),dict(weapons=[None]),dict(null_begin=True)]
    cases += [dict(weapons=[{}]*n) for n in range(1,11)]
    cases += [dict(weapons=[None,{},None,dict(rating=200),{}])]
    cases += [dict(weapons=[dict(available=v)]) for v in [0,1,2,255,65536,0x7fffffff,0x80000000,0xfffffffe,0xffffffff]]
    cases += [dict(weapons=[dict(rating=v)]) for v in [0,1,-1,0x7fffffff,-0x80000000,16777217]]
    values=[0,0x80000000,1,0x80000001,bits(-2),bits(0.3333333),0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(weapons=[dict(maximum=v)]) for v in values]
    cases += [dict(mutation=m,weapons=[{},dict(rating=200),dict(rating=300)]) for m in
        ['best','category','shrink','empty','relocate','parameters','rating','scores']]
    cases += [dict(mutation='rating',rating_bits=v) for v in values]
    cases += [dict(mutation='best',best=v,weapons=[]) for v in values]
    cases += [dict(alias='owner'),dict(alias='weapon'),dict(alias='weapon',weapons=[dict(rating=16777217)]),
        dict(alias='vector',weapons=[dict(available=0)])]
    cases += [dict(end_extra=v) for v in [1,2,3]]
    modes=[{},dict(weapons=[dict(rating=16777217,available=3)]),dict(alias='weapon'),
        dict(weapons=[dict(maximum=0x7f812345)]),dict(weapons=[None,{},dict(rating=200)])]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        modes,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,6])]
    rng=random.Random(0x9a078e)
    cases += [dict(weapons=[dict(rating=rng.randint(-100,500),available=rng.choice([0,1,7,0xffffffff]),
        maximum=rng.getrandbits(32)) for _ in range(rng.randrange(1,10))],category=rng.randrange(4)) for _ in range(40)]
    return cases

def compare_second_weapons(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_second_distance_continue'];guard=bytes.fromhex('8b4c24508b01')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_second_weapons_bridge']
    assert patched[5:]==b'\x90'
    results=[]
    for i,case in enumerate(second_weapons_cases()):
        old=run_second_weapons(original,original_pe,spec,case);new=run_second_weapons(edited,edited_pe,spec,case)
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
        results=compare_second_weapons(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'second-weapons-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} second-weapons comparisons passed',flush=True)
