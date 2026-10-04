"""Compare position/point helpers and the candidate aiming gate with native x86.

Math helpers execute without numeric mocks. World/position/event object methods
are controlled; selected gate cases execute real aiming predicates and trig.
"""
import argparse
import hashlib
import itertools
import json
import random
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,
    UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK
from verify_constant_returns import FLOAT_STATE

IDENTITY=[0x3f800000 if i in [0,5,10,15] else 0 for i in range(16)]
def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]


def run_geometry(image,pe,spec,case):
    m=load_machine(image,pe);frame=STACK+0x8000
    matrix=ARENA+0x1000;point=ARENA+0x1100;output=ARENA+0x1200;origin=ARENA+0x1300
    view=ARENA;owner=ARENA+0x100;owner_vt=ARENA+0x200;mount=ARENA+0x400
    altmount=ARENA+0x600;world=ARENA+0x800;component=ARENA+0xa00
    component_owner=ARENA+0xb00;receiver=ARENA+0xc00;receiver_vt=ARENA+0xd00
    alternate_view=ARENA+0xe00
    world_stub=ARENA+0x8000;notify_stub=ARENA+0x8100
    start=ARENA+0x9000;stop=ARENA+0x9200;seed=ARENA+0x9400
    def w(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def r(p):return struct.unpack('<I',m.mem_read(p,4))[0]
    def words(p,values):m.mem_write(p,struct.pack('<'+'I'*len(values),*values))
    m.mem_write(ARENA,b'\xa5'*0x2000);m.mem_write(frame,b'\xa5'*0x220)
    kind=case['kind'];layout=case.get('layout','separate')
    words(matrix,case.get('matrix',IDENTITY));words(point,case.get('point',[bits(1),bits(2),bits(3)]))
    words(origin,case.get('origin',[bits(0.1),bits(-2),bits(7)]))
    w(view+4,owner);w(view+8,mount);w(owner,owner_vt);w(owner_vt+0x1c,world_stub)
    w(alternate_view+4,owner);w(alternate_view+8,altmount)
    words(mount+0x80,case.get('matrix',IDENTITY));words(altmount+0x80,IDENTITY)
    w(altmount+0xb0,bits(7));w(altmount+0xb4,bits(8));w(altmount+0xb8,bits(9))
    words(world,IDENTITY);words(world+48,case.get('point',[bits(1),bits(2),bits(3)]))
    w(component+4,component_owner);w(component_owner+0x20,receiver)
    w(receiver,receiver_vt);w(receiver_vt+0xa0,notify_stub)
    m.mem_write(world_stub,b'\xc3');m.mem_write(notify_stub,b'\xc2\x04\x00')
    m.mem_write(stop,b'\x90')
    if layout=='output_point':output=point
    elif layout=='partial_point':output=point+4
    elif layout=='output_matrix':output=matrix
    elif layout=='partial_matrix':output=matrix+4
    elif layout=='input_matrix':point=matrix+4
    elif layout=='output_origin':output=origin
    elif layout=='partial_origin':output=origin+4
    elif layout=='all_same':origin=point;output=point
    elif layout=='output_world':output=world+48
    elif layout=='partial_world':output=world+52
    elif layout=='output_mount':output=mount+48+0x80
    elif layout=='partial_mount':output=mount+4+0x80
    expected_return=output
    if kind=='transform_point':entry=spec[kind];this=matrix;args=[point,output]
    elif kind=='vector_difference':entry=spec[kind];this=origin;args=[output,point]
    elif kind in ['aim_world_position','component_position']:
        entry=spec[kind];this=view if kind=='aim_world_position' else component;args=[output];expected_return=world
    else:raise AssertionError(kind)
    m.mem_write(frame,struct.pack('<'+'I'*(1+len(args)),stop,*args))
    saved={reg:0x12340000+i for i,reg in enumerate([UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP])}
    for reg,value in {**saved,UC_X86_REG_ESP:frame,UC_X86_REG_ECX:this}.items():m.reg_write(reg,value)
    cw=case.get('cw',0x37f);depth=case.get('depth',0);m.reg_write(UC_X86_REG_FPCW,cw)
    for i in range(depth):w(seed+4*i,[bits(1.234),bits(-5.678)][i%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))
    preload+=b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff);m.mem_write(start,preload)
    calls=[];before=[];returned=[]
    def hook(uc,address,size,data):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==stop:returned.append(True);uc.emu_stop();return
        if address==world_stub:
            assert uc.reg_read(UC_X86_REG_ECX)==owner;calls.append('world')
            if case.get('mutation')=='mount':w(view+8,altmount)
            if case.get('mutation')=='world':w(world+48,bits(-3.5));w(world+52,bits(4.25))
            uc.reg_write(UC_X86_REG_EAX,world)
        elif address==notify_stub:
            assert uc.reg_read(UC_X86_REG_ECX)==receiver
            assert r(uc.reg_read(UC_X86_REG_ESP)+4)==5;calls.append('notify_5')
            uc.reg_write(UC_X86_REG_EAX,alternate_view if case.get('mutation')=='view' else view)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==frame+4+4*len(args)
    assert m.reg_read(UC_X86_REG_EAX)==expected_return
    for reg,value in saved.items():assert m.reg_read(reg)==value
    after=[m.reg_read(reg) for reg in FLOAT_STATE];occupied=[i for i in range(8) if (before[2]>>(2*i))&3!=3]
    assert after[0]==cw and after[2]==before[2]
    assert all(after[3+i]==before[3+i] for i in occupied)
    return dict(memory=bytes(m.mem_read(ARENA,0x2000)).hex(),calls=calls,
                state=after[:3]+[after[3+i] for i in occupied])


def geometry_cases():
    fixtures=[dict(matrix=IDENTITY,point=[bits(1),bits(-2),bits(3)]),
              dict(matrix=[bits((i-5)*0.1) for i in range(16)],point=[bits(.3333333),bits(-1.234567),1])]
    special=[0,0x80000000,1,0x80000001,0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    fixtures += [dict(matrix=[x if i in [0,5,10,12,13,14] else IDENTITY[i] for i in range(16)],
                      point=[x,0x7fc23456,0xbf800000],origin=[x,1,0x7f812346]) for x in special]
    cases=[]
    for kind in ['transform_point','vector_difference','aim_world_position','component_position']:
        cases += [dict(f,kind=kind) for f in fixtures]
        layouts={'transform_point':['output_point','partial_point','output_matrix','partial_matrix','input_matrix'],
                 'vector_difference':['output_point','partial_point','output_origin','partial_origin','all_same'],
                 'aim_world_position':['output_world','partial_world','output_mount','partial_mount'],
                 'component_position':['output_world','partial_world','output_mount','partial_mount']}[kind]
        cases += [dict(f,kind=kind,layout=layout) for f,layout in itertools.product(fixtures,layouts)]
        cases += [dict(f,kind=kind,cw=0x7f|pc|rc,depth=depth) for f,pc,rc,depth in
                  itertools.product([fixtures[1],fixtures[-1]],[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,4])]
        if kind in ['aim_world_position','component_position']:
            cases += [dict(kind=kind,mutation=x) for x in ['mount','world']]
        if kind=='component_position':cases.append(dict(kind=kind,mutation='view'))
    rng=random.Random(0x49b340)
    for _ in range(24):
        for kind in ['transform_point','vector_difference','aim_world_position','component_position']:
            cases.append(dict(kind=kind,matrix=[rng.getrandbits(32) for _ in range(16)],
                point=[rng.getrandbits(32) for _ in range(3)],origin=[rng.getrandbits(32) for _ in range(3)]))
    return cases


def run_gate(image,pe,spec,case):
    m=load_machine(image,pe);frame=STACK+0x8000
    record=ARENA;alternate_record=ARENA+0x100;target=ARENA+0x200;target_vt=ARENA+0x300
    component=ARENA+0x400;owner=ARENA+0x500;receiver=ARENA+0x600;receiver_vt=ARENA+0x700
    view=ARENA+0x800;view_owner=ARENA+0x900;view_owner_vt=ARENA+0xa00;mount=ARENA+0xb00
    world=ARENA+0xd00;target_matrix=ARENA+0xe00;weapon=ARENA+0xf00;weapon_vt=ARENA+0x1000
    origin=ARENA+0x1100;alt_component=ARENA+0x1200
    world_stub=ARENA+0x8000;notify_stub=ARENA+0x8100;target_stub=ARENA+0x8200;origin_stub=ARENA+0x8300
    start=ARENA+0x9000;seed=ARENA+0x9400
    def w(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def r(p):return struct.unpack('<I',m.mem_read(p,4))[0]
    def words(p,values):m.mem_write(p,struct.pack('<'+'I'*len(values),*values))
    m.mem_write(ARENA,b'\xa5'*0x1800);m.mem_write(frame,b'\xa5'*0x220)
    for rec in [record,alternate_record]:
        words(rec+4,case.get('point',[0,0,bits(1)]));m.mem_write(rec+0x14,bytes([case.get('flag',0)]))
        w(rec+0x18,case.get('record_value',0))
    w(alternate_record+4,bits(.25));w(frame+0x18,record);w(frame+0x58,case.get('reference',bits(10)))
    w(frame+0x1fc,case.get('driver',0));w(frame+0xf4,component);w(frame+0xec,weapon)
    w(component+4,owner);w(alt_component+4,owner);w(owner+0x20,receiver)
    w(receiver,receiver_vt);w(receiver_vt+0xa0,notify_stub)
    w(view+4,view_owner);w(view+8,mount);w(view_owner,view_owner_vt);w(view_owner_vt+0x1c,world_stub)
    words(mount+0x80,IDENTITY);w(mount+0x68,case.get('lower',bits(-.5)));w(mount+0x74,case.get('upper',bits(.5)))
    words(world,IDENTITY);words(target_matrix,IDENTITY);words(origin,case.get('origin',[0,0,0]))
    w(target,target_vt);w(target_vt+0x24,target_stub);w(weapon,weapon_vt);w(weapon_vt+0x38,origin_stub)
    for stub in [world_stub,target_stub,origin_stub]:m.mem_write(stub,b'\xc3')
    m.mem_write(notify_stub,b'\xc2\x04\x00')
    if not case.get('real_aim'):
        m.mem_write(spec['aim_direction'],b'\xb8'+struct.pack('<I',case.get('allowed',1))+b'\xc2\x04\x00')
    saved={UC_X86_REG_EBX:ARENA+0x1600,UC_X86_REG_ESI:ARENA+0x1700,UC_X86_REG_EBP:target,UC_X86_REG_ESP:frame}
    for reg,value in {**saved,UC_X86_REG_EDI:0x12345678}.items():m.reg_write(reg,value)
    cw=case.get('cw',0x37f);depth=case.get('depth',0);m.reg_write(UC_X86_REG_FPCW,cw)
    for i in range(depth):w(seed+4*i,[bits(1.234),bits(-5.678)][i%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))
    entry=spec['artillery_aim_gate'];preload+=b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff)
    m.mem_write(start,preload);calls=[];before=[];exits=[]
    def hook(uc,address,size,data):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in [spec['artillery_aim_gate_accept'],spec['artillery_filter_reject']]:
            exits.append(address==spec['artillery_aim_gate_accept']);uc.emu_stop();return
        ecx=uc.reg_read(UC_X86_REG_ECX);sp=uc.reg_read(UC_X86_REG_ESP)
        if address==notify_stub:
            assert ecx==receiver and r(sp+4)==5;calls.append('notify_5');uc.reg_write(UC_X86_REG_EAX,view)
            if case.get('mutation')=='notify':w(frame+0x18,alternate_record);w(frame+0xf4,alt_component)
        elif address==world_stub:
            assert ecx==view_owner;calls.append('world');uc.reg_write(UC_X86_REG_EAX,world)
            if case.get('mutation')=='world':w(mount+0xb0,bits(3));w(frame+0x18,alternate_record)
        elif address==target_stub:
            assert ecx==target;calls.append('target_matrix');uc.reg_write(UC_X86_REG_EAX,target_matrix)
            if case.get('mutation')=='target':w(frame+0x18,alternate_record);w(record+4,bits(.125))
        elif address==origin_stub:
            assert ecx==weapon;calls.append('origin');uc.reg_write(UC_X86_REG_EAX,origin)
            if case.get('mutation')=='origin':w(frame+0x1b0,bits(.25));w(frame+0xf4,alt_component)
        elif address==spec['aim_direction']:
            assert ecx==component and r(sp+4)==frame+0x1a4
            calls.append(['aim',bytes(uc.mem_read(frame+0x1a4,12)).hex()])
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg,value in saved.items():assert m.reg_read(reg)==value
    after=[m.reg_read(reg) for reg in FLOAT_STATE];occupied=[i for i in range(8) if (before[2]>>(2*i))&3!=3]
    assert after[0]==cw and after[2]==before[2]
    assert all(after[3+i]==before[3+i] for i in occupied)
    return dict(accepted=exits[0],calls=calls,frame=bytes(m.mem_read(frame,0x220)).hex(),
        arena=bytes(m.mem_read(ARENA,0x1800)).hex(),state=after[:3]+[after[3+i] for i in occupied])


def gate_cases():
    cases=[dict(flag=flag,driver=driver,allowed=allowed) for flag,driver,allowed in
           itertools.product([0,1,255],[0,1],[0,1,256,257])]
    values=[0,0x80000000,1,0x80000001,bits(-20),bits(-20)-1,bits(-20)+1,bits(20),0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(flag=1,reference=ref,record_value=record) for ref,record in itertools.product(values,[0,bits(1),bits(-1)])]
    selected=[dict(flag=1,reference=bits(.1),record_value=bits(.2)),dict(flag=1,reference=0x7f812345),
              dict(flag=1,reference=bits(-20)),dict(),dict(point=[0x7f812345,0x80000000,0x7fc23456])]
    cases += [dict(c,cw=0x7f|pc|rc,depth=depth) for c,pc,rc,depth in
              itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2])]
    cases += [dict(mutation=x) for x in ['notify','world','target','origin']]
    cases += [dict(real_aim=True,point=point,cw=cw,depth=depth) for point,cw,depth in
              itertools.product([[0,0,bits(1)],[bits(1),0,0],[bits(-1),0,0],[0,0,bits(-1)]],
                                [0x7f,0x27f,0x37f],[0,2])]
    return cases


def compare_aim_geometry(original,original_pe,edited,edited_pe,spec,symbols):
    for key in ['transform_point','vector_difference','aim_world_position','component_position','artillery_aim_gate']:
        entry=spec[key];jump=edited_pe.get_data(entry-0x400000,5)
        symbol='bfv_'+key+('_bridge' if key=='artillery_aim_gate' else '')
        assert jump[0]==0xe9 and entry+5+struct.unpack('<i',jump[1:])[0]==symbols[symbol]
    results=[]
    for group,runner,cases in [('geometry',run_geometry,geometry_cases()),('gate',run_gate,gate_cases())]:
        for index,case in enumerate(cases):
            try:
                old=runner(original,original_pe,spec,case);new=runner(edited,edited_pe,spec,case)
                assert old==new,(case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
            except Exception as error:raise RuntimeError(f'{group} case {index}: {case}') from error
            results.append(dict(group=group,inputs=case,calls=old['calls'],**({'accepted':old['accepted']} if group=='gate' else {})))
    return results


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_aim_geometry(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
             {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'aim-geometry-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} aiming geometry/gate comparisons passed',flush=True)
