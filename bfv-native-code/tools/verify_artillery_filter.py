"""Run the original inline candidate block and its source/bridge replacement.

Stops at the native accept/reject continuation. Scoring beyond the filter is not
executed. Candidate methods and timestamp lookup are controlled ABI services.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,
    UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK


def run_filter(image,pe,spec,case):
    m=load_machine(image,pe)
    behavior=ARENA;iterator=ARENA+0x400;record=ARENA+0x600;alternate=ARENA+0x700
    candidate=ARENA+0x800;vt=ARENA+0xa00;component=ARENA+0xc00;component_vt=ARENA+0xe00
    pool=ARENA+0x1000;records=ARENA+0x1200;history=ARENA+0x1400;metric=ARENA+0x1500
    identity_stub=ARENA+0x2000;metric_stub=ARENA+0x2100
    frame=STACK+0x8000;incoming_ebp=0x22334455
    def w32(address,value):m.mem_write(address,struct.pack('<I',value))
    def r32(address):return struct.unpack('<I',m.mem_read(address,4))[0]
    if case.get('handle_state')=='sentinel':
        candidate=0xffffffdc;m.mem_map(0xfffff000,0x1000)
    handle={'empty':0,'stale':0x80001}.get(case.get('handle_state'),0x70001)
    w32(spec['pool'],pool);w32(pool,records);w32(records,0 if case.get('handle_state')=='null' else candidate)
    m.mem_write(records+6,struct.pack('<H',7))
    w32(iterator+8,record);w32(record,handle);w32(alternate,0x12340002)
    for ptr in [record,alternate]:
        m.mem_write(ptr+0x28,bytes([case.get('record_flag',0)]));w32(ptr+0x30,case.get('record_word',0))
    w32(candidate,vt)
    if candidate!=0xffffffdc:
        w32(candidate+4,case.get('flags',0));w32(candidate+0x24,0 if case.get('no_component') else component)
    w32(component,component_vt);w32(vt+0x2c,identity_stub);w32(component_vt+0x10,metric_stub)
    w32(metric,case.get('metric_bits',0x3f800000));w32(history,case.get('history_bits',0))
    w32(frame+0x40,iterator);w32(frame+0x18,0x55667788);w32(frame+0x94,0x66778899)
    w32(frame+0xa0,case.get('excluded_id',7));w32(frame+0x58,case.get('reference_bits',0x41f00000))
    m.mem_write(identity_stub,b'\xb8'+struct.pack('<I',case.get('identity',8))+b'\xc3')
    m.mem_write(metric_stub,b'\xd9\x05'+struct.pack('<I',metric)+b'\xc3')
    lookup=spec['target_history'];m.mem_write(lookup,b'\xb8'+struct.pack('<I',history)+b'\xc2\x04\x00')
    saved={UC_X86_REG_EBX:behavior,UC_X86_REG_ESI:ARENA+0x1800,UC_X86_REG_ESP:frame}
    for reg,value in {**saved,UC_X86_REG_EBP:incoming_ebp,UC_X86_REG_EDI:iterator,
                      UC_X86_REG_EAX:0x33445566,UC_X86_REG_ECX:0x44556677,UC_X86_REG_EDX:0x778899aa}.items():m.reg_write(reg,value)
    cw=case.get('cw',0x37f);m.reg_write(UC_X86_REG_FPCW,cw)
    calls=[];exits=[]
    def hook(uc,address,size,data):
        if address in [spec['artillery_filter_accept'],spec['artillery_filter_reject']]:
            exits.append(address==spec['artillery_filter_accept']);uc.emu_stop();return
        ecx=uc.reg_read(UC_X86_REG_ECX)
        if address==identity_stub:
            assert ecx==candidate;calls.append(['identity'])
            if case.get('identity_mutation'):
                w32(frame+0xa0,case.get('identity',8));w32(candidate+4,0x2000)
        if address==metric_stub:
            assert ecx==component;calls.append(['metric'])
            if case.get('metric_mutation'):
                w32(frame+0x18,alternate);w32(candidate+4,0x2000)
        if address==lookup:
            assert ecx==behavior
            sp=uc.reg_read(UC_X86_REG_ESP);calls.append(['history',r32(sp+4)])
            if case.get('history_mutation'):w32(frame+0x58,0x41800000);w32(history,0x80000000)
    m.hook_add(UC_HOOK_CODE,hook)
    m.emu_start(spec['artillery_filter'],0,timeout=1_000_000,count=10000)
    assert len(exits)==1 and m.reg_read(UC_X86_REG_FPCW)==cw
    assert m.reg_read(UC_X86_REG_FPTAG)==0xffff
    for reg,value in saved.items():assert m.reg_read(reg)==value
    return dict(accepted=exits[0],calls=calls,ebp=m.reg_read(UC_X86_REG_EBP),
        record=r32(frame+0x18),component=r32(frame+0x94),candidate_flags=r32(candidate+4),
        history_bits=r32(history),reference_bits=r32(frame+0x58),fpsw=m.reg_read(UC_X86_REG_FPSW))


def filter_cases():
    cases=[{}]
    cases += [dict(handle_state=state) for state in ['empty','stale','null','sentinel']]
    cases += [dict(identity=7),dict(no_component=True)]
    cases += [dict(flags=flags) for flags in [1<<13,1<<16,1<<19,0x92000,1,0xffffffff]]
    cases += [dict(record_flag=flag,record_word=value) for flag in [0,1,255] for value in [0,1,0x7fffffff,0x80000000,0xffffffff]]
    metrics=[0,0x80000000,0xbf800000,0x3f800000,1,0x80000001,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    references=[0,0x419fffff,0x41a00000,0x41a00001,0x41f00000,0x7fc12345,0x7f800000,0xff800000]
    for metric in metrics:
        for ref in references:cases.append(dict(metric_bits=metric,reference_bits=ref))
    for pc in [0,0x200,0x300]:
        for rc in [0,0x400,0x800,0xc00]:
            for metric,ref,history in [(0x7fc12345,0x41a00000,0),(0x3f800000,0x41a00000,1),
                  (0x3f800000,0x41a00000,0x80000001),(0x7f812345,0x7f800000,0x7f800000),
                  (0x3f800000,0x7fc12345,0)]:
                cases.append(dict(metric_bits=metric,reference_bits=ref,history_bits=history,cw=0x7f|pc|rc))
    for mutation in ['identity_mutation','metric_mutation','history_mutation']:
        cases.append({mutation:True})
    return cases


def compare_artillery_filter(original,original_pe,edited,edited_pe,spec,symbols):
    results=[]
    for case in filter_cases():
        try:
            old=run_filter(original,original_pe,spec,case);new=run_filter(edited,edited_pe,spec,case)
            assert old==new,(case,old,new)
        except Exception as error:raise RuntimeError(f'artillery filter inputs {case}') from error
        results.append(dict(inputs=case,**old))
    return results


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_artillery_filter(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
              {k:int(v,16) for k,v in manifest['symbols'].items()})
        print(f'{target}: {len(results)} inline candidate-filter comparisons passed',flush=True)
