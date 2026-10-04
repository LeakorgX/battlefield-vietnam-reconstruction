"""Original/source region math and raw x/z projection; no numeric mocks."""
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
    UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_FPCW)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import bits,state_result

REGION_TARGETS={target:{key:spec[key] for key in ['point_xz','point_in_symmetric_bounds','region_contains_point2','region_contains_point3']} for target,spec in TARGETS.items()}


def run_region(image,pe,spec,case):
    m=load_machine(image,pe);frame=STACK+0x8000
    m.mem_write(ARENA,b'\xa5'*0x3000)
    def w(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def words(p,values):m.mem_write(p,struct.pack('<'+'I'*len(values),*values))
    point,bound,center,output,region=[ARENA+n for n in (0x100,0x200,0x300,0x400,0x800)]
    layout=case.get('layout')
    if layout=='same':output=point
    if layout=='after':output=point+4
    if layout=='after_two':output=point+8
    if layout=='before':output=point-4
    if layout=='unaligned':point+=1;output+=3;bound+=1;center+=3
    if layout=='bound':bound=point
    if layout=='center':center=point
    values=case.get('point',[bits(1),bits(0),bits(.5)])
    words(point,values);words(bound,case.get('bound',[bits(4),bits(5)]))
    words(center,case.get('center',[bits(2),bits(3)]))
    words(region+0x84,case.get('bound',[bits(4),bits(5)]));words(region+0x114,case.get('center',[bits(2),bits(3)]))
    m.mem_write(region+0x1c4,bytes([case.get('mode',0)]));w(region+0x1cc,case.get('radius',bits(5)))
    kind=case['kind'];entry=spec[kind]
    if kind=='point_xz':this,edx,args=point,output,[]
    elif kind=='point_in_symmetric_bounds':this,edx,args=point,bound,[center]
    else:this,edx,args=region,0x23456789,[point]
    start,stop,seed=ARENA+0x9000,ARENA+0x9200,ARENA+0x9400
    words(frame,[stop,*args]);m.mem_write(stop,b'\x90')
    cw=case.get('cw',0x37f);depth=case.get('depth',0);m.reg_write(UC_X86_REG_FPCW,cw)
    for i in range(depth):w(seed+4*i,[bits(1.234),bits(-5.678)][i%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    saved={reg:0x12340000+i for i,reg in enumerate([UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP])}
    for reg,v in {**saved,UC_X86_REG_ECX:this,UC_X86_REG_EDX:edx,UC_X86_REG_ESP:frame}.items():m.reg_write(reg,v)
    before=[];returned=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==stop:returned.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==frame+4+4*len(args)
    for reg,v in saved.items():assert m.reg_read(reg)==v
    result=m.reg_read(UC_X86_REG_EAX)
    if kind=='point_xz':assert result==output
    else:result&=255
    return dict(memory=bytes(m.mem_read(ARENA,0x3000)).hex(),result=result,state=state_result(m,before))


def region_cases():
    special=[0,0x80000000,1,0x80000001,bits(-5),bits(.5),bits(1),bits(4),0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases=[dict(kind='point_xz',point=[v,0x7fc23456,0x7f812346],layout=l) for v,l in itertools.product(special,['separate','same','after','after_two','before','unaligned'])]
    cases += [dict(kind='point_in_symmetric_bounds',layout=l) for l in ['bound','center','unaligned']]
    # Vary each coordinate independently so all short-circuit comparisons execute.
    for kind in ['point_in_symmetric_bounds','region_contains_point2','region_contains_point3']:
        for mode in [0,1]:
            cases.append(dict(kind=kind,mode=mode))
            for field in ['point','bound','center']:
                for coordinate,v in itertools.product(range(3 if field=='point' else 2),special):
                    values=([bits(1),bits(0),bits(.5)] if field=='point' else [bits(4),bits(5)] if field=='bound' else [bits(2),bits(3)])
                    values[coordinate]=v;cases.append(dict(kind=kind,mode=mode,**{field:values}))
            if kind!='point_in_symmetric_bounds':cases += [dict(kind=kind,mode=mode,radius=v) for v in special]
    cases += [dict(kind='point_in_symmetric_bounds',point=[x,y,0],bound=[bits(4),bits(5)],center=[bits(5),bits(6)])
              for x,y in [(bits(4),bits(5)),(bits(5),bits(6)),(bits(6),bits(7)),(bits(6)+1,bits(7)),(bits(6),bits(7)+1),(bits(4)-1,bits(5))]]
    cases += [dict(kind='region_contains_point2',point=[bits(3),bits(4),0]),
              dict(kind='region_contains_point3',point=[bits(3),0,bits(4)]),
              dict(kind='region_contains_point2',mode=255,point=[bits(4),bits(5),0]),
              dict(kind='region_contains_point3',mode=255,point=[bits(4),0,bits(5)])]
    selected=[dict(kind='point_xz',layout='after_two'),dict(kind='point_in_symmetric_bounds'),
              dict(kind='point_in_symmetric_bounds',center=[bits(2),0x7f812345]),
              dict(kind='region_contains_point2',mode=1),dict(kind='region_contains_point3',mode=0),
              dict(kind='region_contains_point3',mode=1,point=[0x7f812345,1,0x7fc23456])]
    selected += [dict(kind='point_in_symmetric_bounds',point=[bits(5),bits(6),0],center=[bits(5),bits(6)]),dict(kind='region_contains_point2',mode=1,point=[bits(4),bits(5),0]),dict(kind='region_contains_point3',point=[bits(3),0,bits(4)])]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,3])]
    rng=random.Random(0x964ac0)
    cases += [dict(kind=kind,mode=i%2,point=[rng.getrandbits(32) for _ in range(3)],bound=[rng.getrandbits(32) for _ in range(2)],center=[rng.getrandbits(32) for _ in range(2)],radius=rng.getrandbits(32))
              for kind,i in itertools.product(['point_in_symmetric_bounds','region_contains_point2','region_contains_point3'],range(16))]
    return cases


def compare_region_geometry(original,original_pe,edited,edited_pe,spec,symbols):
    for key in ['point_xz','point_in_symmetric_bounds','region_contains_point2','region_contains_point3']:
        entry=spec[key];jump=edited_pe.get_data(entry-0x400000,5)
        assert jump[0]==0xe9 and entry+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+key]
    results=[]
    for i,case in enumerate(region_cases()):
        old=run_region(original,original_pe,spec,case);new=run_region(edited,edited_pe,spec,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,result=old['result']))
    return results


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=dict(TARGETS[target],**REGION_TARGETS[target]);work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_region_geometry(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,{k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'region-geometry-verification.json').write_text(json.dumps(dict(target=target,original_sha256=spec['sha'],
            compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} region geometry comparisons passed',flush=True)
