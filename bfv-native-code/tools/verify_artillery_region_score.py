"""First-pass region score modifier with actual region math, without mocks."""
import argparse
import hashlib
import itertools
import json
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,
    UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_FPCW)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import bits,state_result


def run_score(image,pe,spec,case):
    m=load_machine(image,pe);frame=STACK+0x8000;region=ARENA+0x800
    m.mem_write(ARENA,b'\xa5'*0x2000);m.mem_write(frame,b'\xa5'*0x220)
    def w(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def r(p):return struct.unpack('<I',m.mem_read(p,4))[0]
    def words(p,values):m.mem_write(p,struct.pack('<'+'I'*len(values),*values))
    if case.get('alias'):region=frame+0x10
    words(region+0x84,case.get('center',[bits(4),bits(5)]))
    words(region+0x114,case.get('bound',[bits(2),bits(3)]))
    m.mem_write(region+0x1c4,bytes([case.get('mode',0)]));w(region+0x1cc,case.get('radius',bits(5)))
    words(frame+0xc4,case.get('point',[bits(1),0,bits(.5)]))
    w(frame+0x9c,case.get('score',bits(1.234567)))
    w(frame+0x1fc,0x12345678 if case.get('driver',True) else 0)
    w(frame+0x134,region if case.get('region',True) else 0)
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,
           UC_X86_REG_EDI:0x3456789a,UC_X86_REG_EBP:0x456789ab,UC_X86_REG_ESP:frame}
    for reg,v in saved.items():m.reg_write(reg,v)
    entry=spec['artillery_region_score'];start=ARENA+0x9000;seed=ARENA+0x9400
    cw=case.get('cw',0x37f);depth=case.get('depth',0);m.reg_write(UC_X86_REG_FPCW,cw)
    for i in range(depth):w(seed+4*i,[bits(1.234),bits(-5.678)][i%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];returned=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==spec['artillery_region_continue']:returned.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned
    for reg,v in saved.items():assert m.reg_read(reg)==v
    return dict(memory=bytes(m.mem_read(ARENA,0x2000)).hex(),frame=bytes(m.mem_read(frame,0x220)).hex(),
                score=r(frame+0x98),state=state_result(m,before))


def score_cases():
    special=[0,0x80000000,1,0x80000001,bits(-5),bits(.5),bits(1),bits(4),0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases=[{},dict(driver=False),dict(region=False),dict(driver=False,region=False),dict(alias=True),dict(mode=255)]
    cases += [dict(score=v,driver=d,region=r) for v,d,r in itertools.product(special,[False,True],[False,True])]
    for mode in [0,1]:
        for field in ['point','bound','center']:
            for coordinate,v in itertools.product(range(3 if field=='point' else 2),special):
                values=[bits(1),0,bits(.5)] if field=='point' else [bits(2),bits(3)] if field=='bound' else [bits(4),bits(5)]
                values[coordinate]=v;cases.append(dict(mode=mode,**{field:values}))
        cases += [dict(mode=mode,radius=v) for v in special]
    cases += [dict(mode=0,point=[bits(3),0,bits(4)]),dict(mode=1,point=[bits(4),0,bits(5)])]
    selected=[{},dict(driver=False,score=0x7f812345),dict(region=False),dict(mode=1),dict(alias=True),
              dict(mode=0,point=[bits(3),0,bits(4)]),dict(mode=1,point=[bits(4),0,bits(5)]),
              dict(mode=0,center=[bits(4),0x7f812345]),dict(score=0x7f812345)]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,3])]
    return cases


def compare_artillery_region_score(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_region_score'];jump=edited_pe.get_data(entry-0x400000,5)
    assert jump[0]==0xe9 and entry+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_artillery_region_score_bridge']
    results=[]
    for i,case in enumerate(score_cases()):
        old=run_score(original,original_pe,spec,case);new=run_score(edited,edited_pe,spec,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,score=old['score']))
    return results


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_artillery_region_score(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,{k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'artillery-region-score-verification.json').write_text(json.dumps(dict(target=target,original_sha256=spec['sha'],
            compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} region score comparisons passed',flush=True)
