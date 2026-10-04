"""Second-pass position retrieval; actual interface helper, controlled methods."""
import argparse,hashlib,itertools,json,struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP)
from build import PROJECT,GAME,TARGETS
from native_oracle import ARENA
from verify_category_score import setup
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import state_result

def run_second_position(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    candidate,alternate,table,interface,interface_table,point=[ARENA+n for n in (0x100,0x200,0x300,0x400,0x500,0x600)]
    get_stub,write_stub=ARENA+0x8000,ARENA+0x8100
    layout=case.get('layout');values=case.get('point',[0x3f800000,0xc0000000,0x40400000])
    if layout=='candidate_output':candidate=frame+0xc4
    elif layout=='candidate_store':candidate=frame+0x24
    elif layout=='wrap':candidate=0xffffffdc;m.mem_map(0xfffff000,0x1000)
    w(candidate,table);w(alternate,table);w(table+0x18,get_stub)
    if layout!='wrap':w(candidate+0x2c,0 if case.get('fallback') else interface)
    w(interface,interface_table);w(interface_table+0x2c,write_stub)
    if layout=='point_output':point=frame+0xc4
    elif layout=='point_before':point=frame+0xc0
    elif layout=='point_after':point=frame+0xc8
    elif layout=='unaligned':point+=1
    words(point,values);w(frame+0x18,candidate)
    if layout!='candidate_store':w(frame+0x24,0xfeedbeef)
    m.mem_write(get_stub,b'\xc3');m.mem_write(write_stub,b'\xc2\x04\x00')
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,
        UC_X86_REG_EBP:0x34567890,UC_X86_REG_ESP:frame}
    for reg,value in {**saved,UC_X86_REG_EDI:0x45678901}.items():m.reg_write(reg,value)
    start,entry=ARENA+0x9000,spec['artillery_second_aim_accept']
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];calls=[];exits=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==spec['artillery_second_position_continue']:
            exits.append(True);uc.emu_stop();return
        ecx,sp=uc.reg_read(UC_X86_REG_ECX),uc.reg_read(UC_X86_REG_ESP)
        if address==get_stub:
            assert ecx==candidate and r(frame+0x24)==0;calls.append('fallback')
            uc.reg_write(UC_X86_REG_EAX,point)
        elif address==write_stub:
            assert ecx==interface and r(sp+4)==frame+0xc4 and r(frame+0x24)==interface
            calls.append(['interface',ecx,frame+0xc4]);words(frame+0xc4,values)
        if address in [get_stub,write_stub]:
            if case.get('mutation')=='candidate':w(frame+0x18,alternate)
            elif case.get('mutation')=='interface':w(frame+0x24,0x12345678)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1 and m.reg_read(UC_X86_REG_EDI)==candidate
    for reg,value in saved.items():assert m.reg_read(reg)==value
    return dict(calls=calls,frame=bytes(m.mem_read(frame,0x220)).hex(),
        memory=bytes(m.mem_read(ARENA,0x4000)).hex(),state=state_result(m,before))

def second_position_cases():
    cases=[{},dict(fallback=True),dict(layout='wrap')]
    cases += [dict(fallback=f,mutation=v) for f,v in itertools.product([False,True],['candidate','interface'])]
    cases += [dict(fallback=True,layout=v) for v in ['point_output','point_before','point_after','unaligned','candidate_output']]
    cases += [dict(layout='candidate_store'),dict(layout='candidate_output')]
    cases += [dict(fallback=f,point=v) for f,v in itertools.product([False,True],
        [[0,0x80000000,0xffffffff],[0x7fc12345,0x7f812345,0x7f800000],[1,0x80000001,0xff800000]])]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        [{},dict(fallback=True),dict(fallback=True,layout='point_before'),dict(fallback=True,mutation='candidate')],
        [0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return cases

def compare_second_position(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_second_aim_accept'];guard=bytes.fromhex('8b7c24186a02')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_second_position_bridge']
    assert patched[5:]==b'\x90'
    results=[]
    for i,case in enumerate(second_position_cases()):
        old=run_second_position(original,original_pe,spec,case);new=run_second_position(edited,edited_pe,spec,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,calls=old['calls']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_second_position(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'second-position-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} second-position comparisons passed',flush=True)
