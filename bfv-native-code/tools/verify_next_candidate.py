"""First-pass node advance: captured table/node, callback state and boundary."""
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

def run_next_candidate(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    bot,node,next_node,list,table=[ARENA+n for n in (0x100,0x300,0x500,0x700,0x900)]
    if case.get('table_alias'):bot=frame+0x40;table=node
    stub=ARENA+0x8000;w(bot,table);w(table+0x84,stub)
    next_value=case.get('next',next_node);w(node,next_value);w(frame+0x40,node)
    w(list+4,case.get('boundary',next_value if case.get('last') else node))
    m.mem_write(stub,b'\xc3')
    saved={UC_X86_REG_EBX:0x11223344,UC_X86_REG_ESI:bot,UC_X86_REG_EBP:0x12345678,UC_X86_REG_ESP:frame}
    for reg,value in {**saved,UC_X86_REG_EDI:0xaabbccdd}.items():m.reg_write(reg,value)
    start,entry=ARENA+0x9000,spec['artillery_filter_reject']
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];calls=[];exits=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in [spec['artillery_filter'],spec['artillery_after_first_pass']]:exits.append(address);uc.emu_stop();return
        if address==stub:
            assert uc.reg_read(UC_X86_REG_ECX)==bot and r(frame+0x40)==next_value
            calls.append('list');uc.reg_write(UC_X86_REG_EAX,list)
            if case.get('mutation')=='boundary':w(list+4,next_value)
            if case.get('mutation')=='node':w(frame+0x40,0xfeedbeef)
            if case.get('mutation')=='table':w(bot,0x12345678)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1 and m.reg_read(UC_X86_REG_EDI)==next_value
    for reg,value in saved.items():assert m.reg_read(reg)==value
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),memory=bytes(m.mem_read(ARENA,0x4000)).hex(),
        calls=calls,more=exits[0]==spec['artillery_filter'],edi=next_value,state=state_result(m,before))

def next_candidate_cases():
    cases=[{},dict(last=True),dict(table_alias=True),dict(table_alias=True,last=True)]
    cases += [dict(mutation=v) for v in ['boundary','node','table']]
    cases += [dict(next=v,last=last) for v,last in itertools.product([0,0x80000000,0xffffffff],[False,True])]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        [{},dict(last=True),dict(table_alias=True),dict(mutation='node')],
        [0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return cases

def compare_next_candidate(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_filter_reject'];guard=bytes.fromhex('8b5424408b3a')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_next_candidate_bridge']
    assert patched[5:]==b'\x90'*(len(guard)-5)
    results=[]
    for i,case in enumerate(next_candidate_cases()):
        old=run_next_candidate(original,original_pe,spec,case);new=run_next_candidate(edited,edited_pe,spec,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,calls=old['calls'],more=old['more'],edi=old['edi']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_next_candidate(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'next-candidate-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} next-candidate comparisons passed',flush=True)
