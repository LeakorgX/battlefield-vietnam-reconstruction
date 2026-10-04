"""Compare second-pass timestamp weight with actual original/source clamp math.

Timestamp lookup is controlled; later aiming and scoring do not execute.
"""
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

def run_second_weight(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    bot,iterator,table,alternate_table= [ARENA+n for n in (0x100,0x300,0x500,0x700)]
    stub,alternate_stub=ARENA+0x8000,ARENA+0x8100
    handle=case.get('handle',0x12340002)
    if case.get('alias')=='iterator':iterator=frame+0x10c
    w(bot,table);w(table+0x1a4,stub);w(alternate_table+0x1a4,alternate_stub)
    w(iterator,handle);w(frame+0x1c,iterator)
    w(frame+0x58,case.get('reference_bits',0x41f00000))
    if case.get('alias')!='iterator':w(frame+0x10c,case.get('initial_timestamp',0xa5a5a5a5))
    for key in ['artillery_second_weight_scale','artillery_movement_scale','artillery_category_scale']:
        if key+'_bits' in case:w(spec[key],case[key+'_bits'])
    m.mem_write(stub,b'\xc2\x08\x00');m.mem_write(alternate_stub,b'\xc2\x08\x00')
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:bot,UC_X86_REG_EDI:0x23456789,
        UC_X86_REG_EBP:0x34567890,UC_X86_REG_ESP:frame}
    for reg,value in saved.items():m.reg_write(reg,value)
    start,entry=ARENA+0x9000,spec['artillery_second_accept']
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    calls=[];before=[];exits=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==spec['artillery_second_weight_continue']:
            exits.append(True);uc.emu_stop();return
        if address in [stub,alternate_stub]:
            assert address==stub and uc.reg_read(UC_X86_REG_ECX)==bot
            sp=uc.reg_read(UC_X86_REG_ESP)
            assert [r(sp+4),r(sp+8)]==[handle,frame+0x10c]
            calls.append(['timestamp',handle,frame+0x10c])
            if case.get('write_timestamp',True):w(frame+0x10c,case.get('timestamp_bits',0))
            uc.reg_write(UC_X86_REG_EAX,case.get('found',1))
            mutation=case.get('mutation')
            if mutation=='reference':w(frame+0x58,0x3f800001)
            elif mutation=='iterator':w(frame+0x1c,0xffffffff)
            elif mutation=='table':w(bot,alternate_table)
            elif mutation=='weight':w(frame+0x2c,0xfeedbeef)
            elif mutation=='scale':w(spec['artillery_second_weight_scale'],0x3f000000)
            elif mutation=='finish_constants':
                w(spec['artillery_movement_scale'],0x3f400001);w(spec['artillery_category_scale'],0x40000001)
        if address==spec['float_clamp']:
            sp=uc.reg_read(UC_X86_REG_ESP);calls.append(['clamp',r(sp+4),r(sp+8),r(sp+12)])
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg,value in saved.items():assert m.reg_read(reg)==value
    return dict(calls=calls,frame=bytes(m.mem_read(frame,0x220)).hex(),
        memory=bytes(m.mem_read(ARENA,0x4000)).hex(),state=state_result(m,before))

def second_weight_cases():
    cases=[{}]+[dict(found=v) for v in [0,0x100,0x101,0x80000000,0xffffffff]]
    cases += [dict(mutation=v) for v in ['reference','iterator','table','weight','scale','finish_constants']]
    cases += [dict(alias='iterator'),dict(write_timestamp=False),dict(write_timestamp=False,found=0)]
    values=[0,0x80000000,0xbf800000,0x3f800000,1,0x80000001,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(reference_bits=a,timestamp_bits=b) for a,b in itertools.product(values,values)]
    cases += [dict(reference_bits=v) for v in [0x419fffff,0x41a00000,0x41a00001]]
    for key in ['artillery_second_weight_scale','artillery_movement_scale','artillery_category_scale']:
        cases += [{key+'_bits':v} for v in values]
    modes=[{},dict(found=0),dict(reference_bits=0x3f800001,timestamp_bits=0x3f800000),
        dict(reference_bits=0x7fc12345),dict(reference_bits=0x7f812345),
        dict(reference_bits=0x7f800000,timestamp_bits=0x7f800000)]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        modes,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return cases

def compare_second_weight(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_second_accept'];guard=bytes.fromhex('8b4c241c8b16')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_second_weight_bridge']
    assert patched[5:]==b'\x90'
    results=[]
    for i,case in enumerate(second_weight_cases()):
        old=run_second_weight(original,original_pe,spec,case);new=run_second_weight(edited,edited_pe,spec,case)
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
        results=compare_second_weight(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'second-weight-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} second-weight comparisons passed',flush=True)
