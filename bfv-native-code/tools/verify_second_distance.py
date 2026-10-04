"""Second-pass distance arithmetic with actual length/maximum/division helpers."""
import argparse,hashlib,itertools,json,random,struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP)
from build import PROJECT,GAME,TARGETS
from native_oracle import ARENA
from verify_category_score import setup
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import state_result,bits

def run_second_distance(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    words(frame+0xc4,case.get('point',[bits(4),bits(6),bits(3)]))
    words(frame+0x128,case.get('origin',[bits(1),bits(2),bits(3)]))
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EDI:0x34567890,
        UC_X86_REG_EBP:0x45678901,UC_X86_REG_ESP:frame}
    for reg,value in saved.items():m.reg_write(reg,value)
    start,entry=ARENA+0x9000,spec['artillery_second_position_continue']
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];exits=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==spec['artillery_second_distance_continue']:
            exits.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg,value in saved.items():assert m.reg_read(reg)==value
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),state=state_result(m,before),
        distance=r(frame+0x28),direction=[r(frame+n) for n in [0x78,0x7c,0x80]])

def second_distance_cases():
    cases=[{},dict(point=[0,0,0],origin=[0,0,0])]
    cases += [dict(point=[v,0,0],origin=[0,0,0]) for v in [0x3effffff,0x3f000000,0x3f000001,
        0x80000000,1,0x80000001,0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]]
    cases += [dict(point=[v,0x7fc23456,0xbf800000],origin=[v,1,0x7f812346]) for v in [0,1,0x7f800000,0x7fc12345,0x7f812345]]
    modes=[{},dict(point=[0,0,0],origin=[0,0,0]),dict(point=[0x3effffff,0,0],origin=[0,0,0]),
        dict(point=[0x3f000001,0,0],origin=[0,0,0]),cases[-1]]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        modes,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,1,2])]
    rng=random.Random(0x9a06f6)
    cases += [dict(point=[rng.getrandbits(32) for _ in range(3)],origin=[rng.getrandbits(32) for _ in range(3)]) for _ in range(64)]
    return cases

def compare_second_distance(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_second_position_continue'];guard=bytes.fromhex('d98424c4000000')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_second_distance_bridge']
    assert patched[5:]==b'\x90\x90'
    results=[]
    for i,case in enumerate(second_distance_cases()):
        old=run_second_distance(original,original_pe,spec,case);new=run_second_distance(edited,edited_pe,spec,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,distance=old['distance'],direction=old['direction']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_second_distance(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'second-distance-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} second-distance comparisons passed',flush=True)
