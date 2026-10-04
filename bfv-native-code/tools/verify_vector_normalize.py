"""Actual normalization instructions, rounded stores, return/ECX and x87 state."""
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

def run_vector_normalize(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    vector=ARENA+0x100
    if case.get('unaligned'):vector+=1
    if case.get('alias')=='one':vector=spec['math_one']
    elif case.get('alias')=='tolerance':vector=spec['normalize_tolerance']
    words(vector,case.get('vector',[bits(3),bits(4),0]))
    if 'tolerance_bits' in case:w(spec['normalize_tolerance'],case['tolerance_bits'])
    stop,start=ARENA+0x9200,ARENA+0x9000;entry=spec['vector_normalize']
    w(frame,stop);m.mem_write(stop,b'\x90')
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EDI:0x34567890,
        UC_X86_REG_EBP:0x45678901}
    for reg,value in {**saved,UC_X86_REG_ECX:vector,UC_X86_REG_ESP:frame}.items():m.reg_write(reg,value)
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];returned=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==stop:returned.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==frame+4
    for reg,value in saved.items():assert m.reg_read(reg)==value
    return dict(vector=[r(vector+n) for n in [0,4,8]],result=m.reg_read(UC_X86_REG_EAX),
        ecx=m.reg_read(UC_X86_REG_ECX),memory=bytes(m.mem_read(ARENA,0x4000)).hex(),state=state_result(m,before))

def vector_normalize_cases():
    vectors=[[0,0,0],[0x80000000,0,0],[bits(1),0,0],[bits(3),bits(4),0],
        [bits(-2),bits(7),bits(0.25)], [0x3f7fffff,0,0],[0x3f800001,0,0],
        [0x7f7fffff,0,0],[1,0,0],[0x80000001,0,0],
        [0x7f800000,0,0],[0xff800000,0,0],[0x7fc12345,0,0],[0x7f812345,0,0],
        [bits(1),0x7fc23456,0x7fc34567]]
    cases=[dict(vector=v) for v in vectors]
    cases += [dict(vector=v,unaligned=True) for v in vectors]
    cases += [dict(alias=a,vector=v) for a,v in itertools.product(['one','tolerance'],vectors[:5])]
    cases += [dict(tolerance_bits=v) for v in [0,0x80000000,bits(-1),bits(100),0x7f800000,0x7fc12345,0x7f812345]]
    rng=random.Random(128)
    cases += [dict(vector=[rng.getrandbits(32) for _ in range(3)]) for _ in range(100)]
    cases += [dict(vector=v,cw=0x7f|pc|rc,depth=d) for v,pc,rc,d in itertools.product(
        vectors,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,1,2])]
    return cases

def compare_vector_normalize(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['vector_normalize'];guard=bytes.fromhex('51d94108d94104')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_vector_normalize']
    assert patched[5:]==b'\x90\x90'
    results=[]
    for i,case in enumerate(vector_normalize_cases()):
        old=run_vector_normalize(original,original_pe,spec,case);new=run_vector_normalize(edited,edited_pe,spec,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,vector=old['vector'],result=old['result']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_vector_normalize(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'vector-normalize-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} vector-normalize comparisons passed',flush=True)
