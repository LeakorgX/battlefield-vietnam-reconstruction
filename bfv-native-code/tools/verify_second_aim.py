"""Actual second-pass position/difference/normalization with controlled methods.

The final aiming predicate is controlled; complete firing behavior is excluded.
"""
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
from verify_aim_geometry import IDENTITY

def run_second_aim(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    component,alternate_component,owner,receiver,receiver_table=[ARENA+n for n in (0x100,0x200,0x300,0x400,0x500)]
    view,view_owner,view_table,mount,world=[ARENA+n for n in (0x600,0x700,0x800,0x900,0xb00)]
    candidate,alternate_candidate,candidate_table,point=[ARENA+n for n in (0xd00,0xe00,0xf00,0x1000)]
    notify_stub,world_stub,point_stub=ARENA+0x8000,ARENA+0x8100,ARENA+0x8200
    w(component+4,owner);w(alternate_component+4,owner);w(owner+0x20,receiver)
    w(receiver,receiver_table);w(receiver_table+0xa0,notify_stub)
    w(view+4,view_owner);w(view+8,mount);w(view_owner,view_table);w(view_table+0x1c,world_stub)
    words(mount+0x80,case.get('matrix',IDENTITY));words(world,IDENTITY)
    words(world+0x30,case.get('origin',[bits(1),bits(2),bits(3)]))
    w(candidate,candidate_table);w(alternate_candidate,candidate_table);w(candidate_table+0x18,point_stub)
    words(point,case.get('point',[bits(4),bits(6),bits(3)]))
    w(frame+0xf4,component);w(frame+0x18,candidate);w(frame+0x1fc,case.get('driver',0))
    m.mem_write(notify_stub,b'\xc2\x04\x00');m.mem_write(world_stub,b'\xc3');m.mem_write(point_stub,b'\xc3')
    aim=spec['aim_direction'];m.mem_write(aim,b'\xb8'+struct.pack('<I',case.get('allowed',1))+b'\xc2\x04\x00')
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,
        UC_X86_REG_EBP:0x34567890,UC_X86_REG_ESP:frame}
    for reg,value in {**saved,UC_X86_REG_EDI:0x45678901}.items():m.reg_write(reg,value)
    start,entry=ARENA+0x9000,spec['artillery_second_weight_continue']
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    calls=[];before=[];exits=[];mutation=case.get('mutation')
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in [spec['artillery_second_aim_accept'],spec['artillery_second_reject']]:
            exits.append(address==spec['artillery_second_aim_accept']);uc.emu_stop();return
        ecx,sp=uc.reg_read(UC_X86_REG_ECX),uc.reg_read(UC_X86_REG_ESP)
        if address==notify_stub:
            assert ecx==receiver and r(sp+4)==5;calls.append('position_event');uc.reg_write(UC_X86_REG_EAX,view)
            if mutation=='component':w(frame+0xf4,alternate_component)
            elif mutation=='candidate':w(frame+0x18,alternate_candidate)
        elif address==world_stub:
            assert ecx==view_owner;calls.append('world');uc.reg_write(UC_X86_REG_EAX,world)
            if mutation=='origin':words(world+0x30,[bits(-2),bits(0.25),bits(5)])
        elif address==point_stub:
            assert ecx==(alternate_candidate if mutation=='candidate' else candidate)
            calls.append(['point',ecx]);result=point
            if mutation=='point_output':result=frame+0x18c
            elif mutation=='partial_origin':result=frame+0x190
            elif mutation=='output':result=frame+0x1c4;words(result,[bits(3),bits(-4),0])
            elif mutation=='component_after_point':w(frame+0xf4,alternate_component)
            uc.reg_write(UC_X86_REG_EAX,result)
        elif address==aim:
            assert ecx==component and r(sp+4)==frame+0xe0
            calls.append(['aim',ecx,[r(frame+n) for n in [0xe0,0xe4,0xe8]]])
            if mutation=='aim_frame':w(frame+0x18,alternate_candidate)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg,value in saved.items():assert m.reg_read(reg)==value
    assert m.reg_read(UC_X86_REG_EDI)==(0x45678901 if case.get('driver',0) else component)
    return dict(accepted=exits[0],calls=calls,frame=bytes(m.mem_read(frame,0x220)).hex(),
        memory=bytes(m.mem_read(ARENA,0x4000)).hex(),state=state_result(m,before))

def second_aim_cases():
    cases=[{}]+[dict(driver=d) for d in [1,0x100,0xffffffff]]
    cases += [dict(allowed=a) for a in [0,0x100,0x101,0x80000000,0xffffffff]]
    cases += [dict(mutation=v) for v in ['component','candidate','origin','point_output','partial_origin',
        'output','component_after_point','aim_frame']]
    values=[0,0x80000000,1,0x80000001,0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    fixtures=[{},dict(point=[bits(1),bits(2),bits(3)]),
        dict(point=[bits(1),bits(2),bits(4)]),dict(mutation='component'),dict(allowed=0)]
    fixtures += [dict(point=[v,0x7fc23456,0xbf800000],origin=[v,1,0x7f812346]) for v in values]
    cases += fixtures[1:]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        [fixtures[0],fixtures[1],fixtures[2],fixtures[-1]],
        [0,0x200,0x300],[0,0x400,0x800,0xc00],[0,1,2])]
    rng=random.Random(0x9a062d)
    cases += [dict(point=[rng.getrandbits(32) for _ in range(3)],origin=[rng.getrandbits(32) for _ in range(3)]) for _ in range(32)]
    return cases

def compare_second_aim(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_second_weight_continue'];guard=bytes.fromhex('8b8424fc010000')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_second_aim_bridge']
    assert patched[5:]==b'\x90\x90'
    results=[]
    for i,case in enumerate(second_aim_cases()):
        old=run_second_aim(original,original_pe,spec,case);new=run_second_aim(edited,edited_pe,spec,case)
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
        results=compare_second_aim(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'second-aim-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} second-aim comparisons passed',flush=True)
