"""Original/source traversal wrapper ABI, callback captures and stack aliases."""
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

def run_traversal(image,pe,spec,entry,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    obj,receiver,owner,table,alternate,scratch=[ARENA+n for n in (0x100,0x300,0x500,0x700,0x900,0xb00)]
    next_stub,resolve_stub=ARENA+0x8000,ARENA+0x8200
    w(obj+0x20,receiver);w(receiver,table);w(table+0x84,next_stub)
    w(owner,table);w(table+0x5c,resolve_stub);w(alternate+0x5c,resolve_stub)
    m.mem_write(next_stub,b'\xc2\x04\x00');m.mem_write(resolve_stub,b'\xc2\x04\x00')
    if case.get('alias')=='argument':scratch=frame+8
    if case.get('alias')=='owner_table':scratch=owner
    if case.get('alias')=='receiver':scratch=obj+0x20
    argument=case.get('argument',0xabcdef01)
    start,stop=ARENA+0x9000,ARENA+0x9200
    words(frame,[stop,scratch,argument]);m.mem_write(stop,b'\x90')
    saved={reg:0x11220000+i for i,reg in enumerate([UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP])}
    for reg,value in {**saved,UC_X86_REG_ECX:obj,UC_X86_REG_ESP:frame}.items():m.reg_write(reg,value)
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];calls=[];returned=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==stop:returned.append(True);uc.emu_stop();return
        ecx,sp=uc.reg_read(UC_X86_REG_ECX),uc.reg_read(UC_X86_REG_ESP)
        if address==next_stub:
            assert ecx==receiver and r(sp+4)==scratch;calls.append(('next',ecx,scratch))
            if case.get('alias')=='owner_table':w(scratch,alternate)
            else:w(scratch,case.get('scratch_value',0xfeedbeef))
            if case.get('mutation')=='receiver':w(obj+0x20,0x12345678)
            uc.reg_write(UC_X86_REG_EAX,0 if case.get('null') else owner)
        elif address==resolve_stub:
            arg=r(sp+4);assert ecx==owner and arg==r(frame+8)
            calls.append(('resolve',ecx,arg));uc.reg_write(UC_X86_REG_EAX,arg^0x12345678)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==frame+12
    for reg,value in saved.items():assert m.reg_read(reg)==value
    return dict(memory=bytes(m.mem_read(ARENA,0x4000)).hex(),frame=bytes(m.mem_read(frame,16)).hex(),
        calls=calls,result=m.reg_read(UC_X86_REG_EAX),state=state_result(m,before))

def traversal_cases():
    cases=[{},dict(null=True),dict(mutation='receiver')]
    cases += [dict(alias=v) for v in ['argument','owner_table','receiver']]
    cases += [dict(argument=v) for v in [0,0xffffffff,0x80000000,0x7f812345]]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        [{},dict(null=True),dict(alias='argument'),dict(alias='owner_table')],
        [0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return cases

def compare_target_traversal(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_next_target'];guard=bytes.fromhex('8b49208b542404')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_next_target_handle']
    assert patched[5:]==b'\x90'*(len(guard)-5)
    results=[]
    for i,case in enumerate(traversal_cases()):
        old=run_traversal(original,original_pe,spec,entry,case)
        new=run_traversal(edited,edited_pe,spec,symbols['bfv_next_target_handle'],case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,calls=old['calls'],result=old['result']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_target_traversal(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'target-traversal-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} target traversal comparisons passed',flush=True)
