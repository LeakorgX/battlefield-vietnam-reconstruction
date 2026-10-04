"""Second-pass filter differential checks; virtual methods/history are controlled.

Both native and reconstructed filter execute. Later scoring/cleanup is excluded.
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

def run_second_filter(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    behavior,alternate_behavior,iterator,alternate_iterator=[ARENA+n for n in (0x100,0x200,0x300,0x400)]
    candidate,alternate,component,table,component_table=[ARENA+n for n in (0x500,0x600,0x700,0x800,0x900)]
    pool,entries,timestamp,metric=[ARENA+n for n in (0x1000,0x1200,0x1500,0x1600)]
    identity_stub,metric_stub=ARENA+0x8000,ARENA+0x8100
    state=case.get('handle_state');handle={'empty':0,'stale':0x80001}.get(state,0x70001)
    if state=='sentinel':candidate=0xffffffdc;m.mem_map(0xfffff000,0x1000)
    w(spec['pool'],pool);w(pool,entries);w(entries,0 if state=='null' else candidate)
    m.mem_write(entries+6,struct.pack('<H',7))
    w(iterator,handle);w(alternate_iterator,0x12340002)
    for obj in [candidate,alternate]:
        w(obj,table)
        if obj!=0xffffffdc:
            w(obj+4,case.get('flags',0));w(obj+0x24,0 if case.get('no_component') else component)
    w(component,component_table);w(table+0x2c,identity_stub);w(component_table+0x10,metric_stub)
    w(metric,case.get('metric_bits',0x3f800000));w(timestamp,case.get('history_bits',0))
    w(frame+0x1c,iterator);w(frame+0x18,0x55667788);w(frame+0x94,0x66778899)
    w(frame+0xa0,7);w(frame+0xa4,behavior);w(frame+0x58,case.get('reference_bits',0x41f00000))
    m.mem_write(identity_stub,b'\xb8'+struct.pack('<I',case.get('identity',8))+b'\xc3')
    m.mem_write(metric_stub,b'\xd9\x05'+struct.pack('<I',metric)+b'\xc3')
    lookup=spec['target_history'];m.mem_write(lookup,b'\xb8'+struct.pack('<I',timestamp)+b'\xc2\x04\x00')
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:ARENA+0x1800,UC_X86_REG_EDI:0x23456789,
        UC_X86_REG_EBP:0x34567890,UC_X86_REG_ESP:frame}
    for reg,value in saved.items():m.reg_write(reg,value)
    start,entry=ARENA+0x9000,spec['artillery_second_filter']
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    calls=[];exits=[];before=[];mutation=case.get('mutation')
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in [spec['artillery_second_accept'],spec['artillery_second_reject']]:
            exits.append(address==spec['artillery_second_accept']);uc.emu_stop();return
        ecx,sp=uc.reg_read(UC_X86_REG_ECX),uc.reg_read(UC_X86_REG_ESP)
        if address==identity_stub:
            assert ecx==candidate;calls.append('identity')
            if mutation=='excluded':w(frame+0xa0,case.get('identity',8))
            elif mutation=='candidate':w(frame+0x18,alternate);w(candidate+4,0x2000)
            elif mutation=='component':w(candidate+0x24,0)
            elif mutation=='flags':w(candidate+4,0x80000)
            elif mutation=='wrap':w(frame+0x18,0xffffffdc)
        elif address==metric_stub:
            assert ecx==component;calls.append('metric')
            if mutation=='iterator':w(frame+0x1c,alternate_iterator)
            elif mutation=='handle':w(iterator,0x12340002)
            elif mutation=='behavior':w(frame+0xa4,alternate_behavior)
            elif mutation=='metric_candidate':w(frame+0x18,alternate)
        elif address==lookup:
            expected=alternate_behavior if mutation=='behavior' else behavior
            assert ecx==expected;calls.append(['history',ecx,r(sp+4)])
            if mutation=='history':w(frame+0x58,0x41800000);w(timestamp,0x80000000)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg,value in saved.items():assert m.reg_read(reg)==value
    return dict(accepted=exits[0],calls=calls,frame=bytes(m.mem_read(frame,0x220)).hex(),
        memory=bytes(m.mem_read(ARENA,0x4000)).hex(),state=state_result(m,before))

def second_filter_cases():
    cases=[{}]+[dict(handle_state=s) for s in ['empty','stale','null','sentinel']]
    cases += [dict(identity=7),dict(no_component=True)]
    cases += [dict(flags=f) for f in [1<<13,1<<16,1<<19,0x92000,1,0xffffffff]]
    cases += [dict(mutation=s) for s in ['excluded','candidate','component','flags','wrap',
        'iterator','handle','behavior','metric_candidate','history']]
    metrics=[0,0x80000000,0xbf800000,0x3f800000,1,0x80000001,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    references=[0,0x419fffff,0x41a00000,0x41a00001,0x41f00000,0x7fc12345,0x7f800000,0xff800000]
    cases += [dict(metric_bits=m,reference_bits=r) for m,r in itertools.product(metrics,references)]
    modes=[dict(metric_bits=0x7fc12345),dict(reference_bits=0x41a00000,history_bits=1),
        dict(reference_bits=0x41a00000,history_bits=0x80000001),
        dict(metric_bits=0x7f812345,reference_bits=0x7f800000,history_bits=0x7f800000),
        dict(reference_bits=0x7fc12345),dict(mutation='candidate'),dict(mutation='iterator')]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        modes,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return cases

def compare_second_filter(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_second_filter'];guard=bytes.fromhex('8b44241c8b00')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_second_filter_bridge']
    assert patched[5:]==b'\x90'
    results=[]
    for i,case in enumerate(second_filter_cases()):
        old=run_second_filter(original,original_pe,spec,case);new=run_second_filter(edited,edited_pe,spec,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,accepted=old['accepted'],calls=old['calls']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_second_filter(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'second-filter-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} second-filter comparisons passed',flush=True)
