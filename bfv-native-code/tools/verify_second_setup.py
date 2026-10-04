"""Second-pass query routing and initialization with controlled object methods."""
import itertools,struct
import argparse,hashlib,json
from pathlib import Path
import pefile
from build import TARGETS,PROJECT,GAME
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP)
from native_oracle import ARENA
from verify_category_score import setup
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import state_result

def run_second_setup(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    pattern,bot,manager,alternate=[ARENA+n for n in (0x100,0x300,0x500,0x700)]
    pattern_stub,query_stub=ARENA+0x8000,ARENA+0x8200
    for obj in [pattern,manager,alternate]:w(obj,obj+0x80)
    w(pattern+0x80+0x28,pattern_stub)
    w(manager+0x80+0x24,query_stub);w(alternate+0x80+0x24,query_stub)
    m.mem_write(pattern_stub,b'\xc3');m.mem_write(query_stub,b'\xc2\x0c\x00')
    w(frame+0x50,pattern);w(frame+0x24,manager)
    saved={UC_X86_REG_EBX:0x11223344,UC_X86_REG_ESI:bot,UC_X86_REG_EDI:0xaabbccdd,
        UC_X86_REG_EBP:0x12345678,UC_X86_REG_ESP:frame}
    for reg,value in saved.items():m.reg_write(reg,value)
    start,entry=ARENA+0x9000,spec['artillery_after_first_pass']
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];calls=[];exits=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in [spec['artillery_second_filter'],spec['artillery_second_empty'],spec['artillery_second_skip']]:
            exits.append(address);uc.emu_stop();return
        ecx,sp=uc.reg_read(UC_X86_REG_ECX),uc.reg_read(UC_X86_REG_ESP)
        if address==pattern_stub:
            assert ecx==pattern;calls.append('pattern');uc.reg_write(UC_X86_REG_EAX,case.get('skip',0))
            if case.get('mutation')=='manager':w(frame+0x24,alternate)
        elif address==query_stub:
            expected=alternate if case.get('mutation')=='manager' else manager
            assert ecx==expected and [r(sp+4),r(sp+8),r(sp+12)]==[frame+0xfc,bot,0x44548000]
            assert [r(frame+n) for n in [0x100,0x104,0x108]]==[0,0,0]
            assert r(frame+0xfc)==0xa5a5a5a5
            calls.append(('query',ecx,frame+0xfc,bot,0x44548000))
            begin=case.get('begin',ARENA+0x1000);end=case.get('end',begin if case.get('empty') else begin+4)
            words(frame+0x100,[begin,end&0xffffffff,case.get('capacity',0xffffffff)])
            if case.get('mutation')=='receiver':w(frame+0x24,alternate)
            if case.get('mutation')=='header':w(frame+0xfc,0xfeedbeef)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg,value in saved.items():assert m.reg_read(reg)==value
    route={spec['artillery_second_filter']:'filter',spec['artillery_second_empty']:'empty',spec['artillery_second_skip']:'skip'}[exits[0]]
    eax=m.reg_read(UC_X86_REG_EAX) if route!='skip' else None
    if route!='skip':assert eax==r(frame+0x100)
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),memory=bytes(m.mem_read(ARENA,0x4000)).hex(),
        calls=calls,route=route,eax=eax,state=state_result(m,before))

def second_setup_cases():
    cases=[{},dict(empty=True)]
    cases += [dict(skip=v) for v in [1,0x100,0x101,0xffffffff,0x80000000]]
    cases += [dict(mutation=v) for v in ['manager','receiver','header']]
    cases += [dict(begin=v,empty=e) for v,e in itertools.product([0,0x80000000,0xffffffff],[False,True])]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        [{},dict(empty=True),dict(skip=0x101),dict(mutation='manager')],
        [0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return cases

def compare_second_setup(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_after_first_pass'];guard=bytes.fromhex('8b4c24508b11')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_second_setup_bridge']
    assert patched[5:]==b'\x90'*(len(guard)-5)
    results=[]
    for i,case in enumerate(second_setup_cases()):
        old=run_second_setup(original,original_pe,spec,case);new=run_second_setup(edited,edited_pe,spec,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,calls=old['calls'],route=old['route'],eax=old['eax']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_second_setup(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'second-setup-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} second-setup comparisons passed',flush=True)
