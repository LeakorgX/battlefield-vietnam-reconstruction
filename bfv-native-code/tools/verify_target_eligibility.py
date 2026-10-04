"""Staged instruction comparisons for alternate-path target eligibility.

Object methods and component-view conversion are controlled. Handle lookup,
callback rereads, traversal and event wrapper execute original/source code.
"""
import argparse
import hashlib
import itertools
import json
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP)
from native_oracle import ARENA,STACK
from verify_category_score import setup
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import state_result
from build import GAME,PROJECT,TARGETS

ADDRESSES={'client':dict(eligible=0x97ede0,event=0x9d8100),
           'server':dict(eligible=0x728c90,event=0x78c670)}

def run_eligibility(image,pe,spec,addresses,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    # A failed +0x8c/+0x84 lookup uses the native -1 handle. Its record is
    # outside the standard 64 KiB fixture, but valid process memory provides
    # a generation mismatch rather than an emulator-only unmapped fault.
    m.mem_map(ARENA+0x10000,0x80000)
    pool,records=ARENA+0x100,ARENA+0x200
    objects=[ARENA+0x400+i*0x100 for i in range(4)]
    receivers=[ARENA+0x1000+i*0x200 for i in range(5)]
    owner=ARENA+0x2000
    components=[ARENA+0x2400+i*0x100 for i in range(4)]
    events=[ARENA+0x2a00+i*0x80 for i in range(4)]
    service,base,view=ARENA+0x3000,ARENA+0x3400,ARENA+0x3600
    names=['direct','owner','resolve','first','next','event','base','predicate']
    stubs={name:ARENA+0x8000+i*0x100 for i,name in enumerate(names)}
    for name,address in stubs.items():
        m.mem_write(address,b'\xc2\x04\x00' if name in ['resolve','first','next','event'] else b'\xc3')
    m.mem_write(spec['artillery_component_view'],b'\xc3')
    w(spec['pool'],pool);w(pool,records)
    for i,obj in enumerate(objects):
        words(records+i*8,[obj,0x00010000]);w(obj+0x20,receivers[i]);w(obj+0x30,components[i])
        w(components[i]+4,obj);w(events[i]+4,service)
    for receiver in receivers:
        w(receiver,receiver+0x80)
        for slot,name in [(0x70,'direct'),(0x94,'owner'),(0x8c,'first'),(0x84,'next'),(0xa0,'event')]:
            w(receiver+0x80+slot,stubs[name])
    w(owner,owner+0x80);w(owner+0x80+0x5c,stubs['resolve'])
    w(service,service+0x80);w(service+0x80+0x14,stubs['base'])
    w(view,view+0x80);w(view+0x80+0x18,stubs['predicate'])
    handle=case.get('handle',0x10001)
    if case.get('null_object'):w(records,0)
    if case.get('no_component'):w(objects[0]+0x30,0)
    if case.get('nested_no_component'):w(objects[1]+0x30,0)
    if case.get('null_nested'):w(records+8,0)
    stop,start=ARENA+0x9200,ARENA+0x9000
    w(frame,stop);m.mem_write(stop,b'\x90')
    saved={reg:0x11220000+i for i,reg in enumerate([UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP])}
    for reg,value in {**saved,UC_X86_REG_ECX:handle,UC_X86_REG_EDX:0xabcdef01,UC_X86_REG_ESP:frame}.items():m.reg_write(reg,value)
    gate=case.get('gate',False)
    if gate:
        key,interface=ARENA+0x3900,ARENA+0x3800
        stubs.update(metric=ARENA+0x8800,classification=ARENA+0x8900)
        m.mem_write(stubs['metric'],b'\xc3');m.mem_write(stubs['classification'],b'\xc3')
        w(receivers[0]+0x80+0x90,stubs['metric']);w(receivers[4]+0x80+0x90,stubs['metric'])
        w(interface,interface+0x40);w(interface+0x40+0x3c,stubs['classification'])
        w(objects[0]+0x24,interface);w(objects[0]+0x10,case.get('flags',8))
        w(key,handle);w(frame+0x18,key);w(frame+0xa0,0xabcdef01)
        saved[UC_X86_REG_EBP]=objects[0];m.reg_write(UC_X86_REG_EBP,objects[0])
        entry=spec['artillery_alternate_gate']
    else:entry=addresses['eligible']
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    calls=[];before=[];returned=[];counts={name:0 for name in stubs};event_index=0
    def hook(uc,address,size,unused):
        nonlocal event_index
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==stop or (gate and address in [spec['artillery_alternate_accept'],spec['artillery_filter_reject']]):
            returned.append(address);uc.emu_stop();return
        ecx,sp=uc.reg_read(UC_X86_REG_ECX),uc.reg_read(UC_X86_REG_ESP)
        if address==spec['artillery_component_view']:
            assert ecx==base;calls.append(('convert',ecx));uc.reg_write(UC_X86_REG_EAX,view);return
        if address not in stubs.values():return
        name=next(n for n,p in stubs.items() if p==address);number=counts[name];counts[name]+=1
        args=[r(sp+4)] if name in ['resolve','first','next','event'] else []
        if name in ['first','next']:
            if gate:assert STACK<=args[0]<frame
            else:assert args==[frame-4]
            calls.append((name,ecx,'scratch'))
        else:calls.append((name,ecx,*args))
        if name=='direct':
            uc.reg_write(UC_X86_REG_EAX,case.get('direct',1))
            if case.get('mutation')=='receiver':w(objects[0]+0x20,receivers[4])
        elif name=='owner':
            result=0 if case.get('no_owner') else owner;uc.reg_write(UC_X86_REG_EAX,result)
            if case.get('mutation')=='owner_receiver' and number==0:w(objects[0]+0x20,receivers[4])
        elif name=='resolve':
            assert ecx==owner and args==[0xabcdef01]
            handles=case.get('resolved',[0x10002,0x10003,0])
            uc.reg_write(UC_X86_REG_EAX,handles[min(number,len(handles)-1)])
        elif name in ['first','next']:
            assert ecx in receivers
            if case.get('scratch'):w(args[0],0x12345678+number)
            if case.get('mutation')=='pool':w(records+8,objects[3])
            if case.get('mutation')=='next_receiver':w(objects[0]+0x20,receivers[4])
            uc.reg_write(UC_X86_REG_EAX,0 if case.get('no_'+name) else owner)
        elif name=='event':
            assert args==[3] and ecx in receivers
            event_index=receivers.index(ecx)%4;uc.reg_write(UC_X86_REG_EAX,events[event_index])
            if case.get('mutation')=='component':w(events[event_index]+4,service)
        elif name=='base':assert ecx==service;uc.reg_write(UC_X86_REG_EAX,base)
        elif name=='predicate':
            assert ecx==view;values=case.get('predicates',[0,1])
            uc.reg_write(UC_X86_REG_EAX,values[min(number,len(values)-1)])
            if gate and case.get('gate_mutation')=='record':
                w(key,0x11223344)
            if gate and case.get('gate_mutation')=='receiver':
                w(objects[0]+0x20,receivers[4])
        elif name=='metric':
            assert ecx in [receivers[0],receivers[4]]
            assert r(frame+0x5c)==0 and r(frame+0x14)==0
            uc.reg_write(UC_X86_REG_EAX,case.get('metric',0x7f812345))
            if case.get('gate_mutation')=='frame':w(frame+0x5c,0xfeedbeef);w(frame+0x54,0xaabbccdd)
        elif name=='classification':
            assert ecx==interface;uc.reg_write(UC_X86_REG_EAX,case.get('classification',3))
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==frame+(0 if gate else 4)
    for reg,value in saved.items():
        if not gate or reg!=UC_X86_REG_EDI:assert m.reg_read(reg)==value
    if gate:
        accepted=returned[0]==spec['artillery_alternate_accept']
        assert m.reg_read(UC_X86_REG_EDI)==(objects[0] if accepted else saved[UC_X86_REG_EDI])
        return dict(memory=bytes(m.mem_read(ARENA,0x4000)).hex(),frame=bytes(m.mem_read(frame,0x220)).hex(),
            calls=calls,accepted=accepted,edi=m.reg_read(UC_X86_REG_EDI),state=state_result(m,before))
    return dict(memory=bytes(m.mem_read(ARENA,0x4000)).hex(),calls=calls,
        result=m.reg_read(UC_X86_REG_EAX)&255,ecx=m.reg_read(UC_X86_REG_ECX),state=state_result(m,before))

def eligibility_cases():
    cases=[{},dict(handle=0),dict(handle=0x10000),dict(handle=0x20001),dict(null_object=True),
           dict(no_component=True),dict(nested_no_component=True),dict(null_nested=True),
           dict(direct=0,no_owner=True)]
    # The failed-owner path intentionally dereferences object+0x30 at address
    # 0x30. Fault behavior needs a separate oracle; do not silently guard it.
    cases.pop()
    cases += [dict(direct=v) for v in [0,0x100,0x101,0xffffffff]]
    cases += [dict(no_first=True),dict(no_next=True,predicates=[0,0]),dict(scratch=True),
              dict(scratch=True,predicates=[0,0,0]),dict(resolved=[0x10002,0x10003,0x10004,0],predicates=[0,0,0,1])]
    cases += [dict(predicates=v) for v in [[1],[0x100,0x101],[0,0],[0,0,0],[0xffffffff],[0,0,0xffffffff]]]
    cases += [dict(resolved=v) for v in [[0],[0x20002],[0x10000],[0x10002,0x20003]]]
    cases += [dict(mutation=v,direct=0 if v=='owner_receiver' else 1,predicates=[0,0,1])
              for v in ['receiver','owner_receiver','pool','next_receiver','component']]
    samples=[{},dict(scratch=True,predicates=[0,0,0]),dict(direct=0),dict(handle=0)]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(samples,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return cases


def compare_target_eligibility(original,original_pe,edited,edited_pe,spec,symbols):
    addresses={'eligible':spec['target_handle_eligible'],'event':ADDRESSES['client' if spec['target_handle_eligible']==0x97ede0 else 'server']['event']}
    guard=bytes.fromhex('51568bc125ffff0000');entry=addresses['eligible']
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_target_handle_eligible']
    assert patched[5:]==b'\x90'*(len(guard)-5)
    compiled={'eligible':symbols['bfv_target_handle_eligible'],
              'event':symbols['bfv_artillery_component_test']}
    results=[]
    for i,case in enumerate(eligibility_cases()):
        old=run_eligibility(original,original_pe,spec,addresses,case)
        new=run_eligibility(edited,edited_pe,spec,compiled,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,calls=old['calls'],result=old['result'],ecx=old['ecx']))
    return results


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument('--target',choices=['client','server','both'],default='both');args=parser.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_target_eligibility(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'target-eligibility-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} target eligibility comparisons passed',flush=True)
