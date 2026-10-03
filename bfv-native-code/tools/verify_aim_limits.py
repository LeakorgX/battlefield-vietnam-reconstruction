"""Original/reconstructed aim predicates with controlled transform/trig services.

Execute the real wrapper and angular predicate. Matrix composition and inverse
sine are controlled calls, not claims of reconstructed math dependencies.
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
    UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,
    UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK

def aim_cases():
    cases=[]
    angles=[0,0x80000000,0x3e800000,0xbe800000,0x3f000000,0xbf000000,
            0x40490fdb,0xc0490fdb,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    bounds=[(0xbf000000,0x3f000000),(0,0),(0x3f000000,0xbf000000),
            (0xc0490fdb,0x40490fdb),(0x7fc12345,0x7fc23456)]
    for angle,side,(lower,upper),wrapper in itertools.product(angles,
            [0,0x80000000,0x3f800000,0xbf800000,0x7fc12345],bounds,[False,True]):
        cases.append(dict(angle=angle,side=side,lower=lower,upper=upper,wrapper=wrapper))
    projections=[0,0x80000000,1,0x80000001,0x3f800000,0xbf800000,0x3f800001,
                 0xbf800001,0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(projection=x,wrapper=wrapper) for x,wrapper in itertools.product(projections,[False,True])]
    for mutation in ['owner_mount','matrix_limits','matrix_pointer','trig_direction','trig_limits']:
        for wrapper in [False,True]:cases.append(dict(mutation=mutation,wrapper=wrapper,side=0xbf800000))
    cases += [dict(mutation=mutation,wrapper=True) for mutation in ['notify_pointer','notify_view']]
    for cw in [0x7f|pc|rc for pc in [0,0x200,0x300] for rc in [0,0x400,0x800,0xc00]]:
        for angle,side,projection in [(0x3eaaaaab,0xbf800000,0x3f800001),
                 (0xbeaaaaab,0xbf800000,0xbf800001),(0,0xbf800000,1),
                 (0x7f812345,0xbf800000,0x7fc12345),(0x7fc12345,0x7fc23456,0x7f812345)]:
            cases.append(dict(angle=angle,side=side,projection=projection,cw=cw,depth=2,wrapper=True))
    # Execute the actual retained inverse-sine wrapper/runtime on valid inputs.
    cases += [dict(projection=projection,side=side,wrapper=wrapper,cw=cw,real_trig=True)
              for projection,side,wrapper,cw in itertools.product(
                  [0,0x3f000000,0xbf000000,0x3f800000,0xbf800000],
                  [0x3f800000,0xbf800000],[False,True],[0x7f,0x27f,0x37f])]
    cases += [dict(real_trig=True,real_compose=True,transform=transform,wrapper=wrapper,cw=cw)
              for transform,wrapper,cw in itertools.product(range(4),[False,True],[0x7f,0x27f,0x37f])]
    return cases

def run_aim(image,pe,spec,symbols,case):
    m=load_machine(image,pe);view=ARENA;owner=ARENA+0x100;owner_vt=ARENA+0x200
    mount=ARENA+0x400;alternate_mount=ARENA+0x600;world=ARENA+0x800;alternate_view=ARENA+0x900
    direction=ARENA+0xa00;alternate_direction=ARENA+0xb00
    component=ARENA+0xc00;component_owner=ARENA+0xd00;receiver=ARENA+0xe00;receiver_vt=ARENA+0xf00
    owner_stub=ARENA+0x1000;notify_stub=ARENA+0x1100;angle_storage=ARENA+0x1200
    trampoline=ARENA+0x1300;preload=ARENA+0x1400;seed=ARENA+0x1500;retained=ARENA+0x1600
    def w32(p,v):m.mem_write(p,struct.pack('<I',v))
    def r32(p):return struct.unpack('<I',m.mem_read(p,4))[0]
    w32(view+4,owner);w32(view+8,mount);w32(owner,owner_vt);w32(owner_vt+0x1c,owner_stub)
    w32(alternate_view+4,owner);w32(alternate_view+8,alternate_mount)
    w32(mount+0x68,case.get('lower',0xbf000000));w32(mount+0x74,case.get('upper',0x3f000000))
    w32(alternate_mount+0x68,0x40490fdb);w32(alternate_mount+0x74,0xc0490fdb)
    identity=[0x3f800000 if i in [0,5,10,15] else 0 for i in range(16)]
    transform=identity.copy()
    if case.get('transform')==1:transform[0]=0;transform[2]=0x3f800000;transform[8]=0xbf800000;transform[10]=0
    if case.get('transform')==2:transform[0]=0xbf800000;transform[10]=0xbf800000
    if case.get('transform')==3:transform[0]=0x3f3504f3;transform[2]=0x3f3504f3;transform[8]=0xbf3504f3;transform[10]=0x3f3504f3
    m.mem_write(world,struct.pack('<16I',*identity));m.mem_write(mount+0x80,struct.pack('<16I',*transform))
    m.mem_write(direction,struct.pack('<3I',0x3f800000,0,0));m.mem_write(alternate_direction,struct.pack('<3I',0,0x3f800000,0))
    w32(component+4,component_owner);w32(component_owner+0x20,receiver);w32(receiver,receiver_vt)
    w32(receiver_vt+0xa0,notify_stub);w32(angle_storage,case.get('angle',0x3e800000))
    m.mem_write(owner_stub,b'\xb8'+struct.pack('<I',world)+b'\xc3')
    returned_view=alternate_view if case.get('mutation')=='notify_view' else view
    m.mem_write(notify_stub,b'\xb8'+struct.pack('<I',returned_view)+b'\xc2\x04\x00')
    if not case.get('real_compose'):m.mem_write(spec['aim_compose'],b'\x8b\xc1\xc2\x08\x00')
    if not case.get('real_trig'):
        m.mem_write(spec['aim_inverse_sine'],b'\xd9\x05'+struct.pack('<I',angle_storage)+b'\xc2\x04\x00')
    depth=case.get('depth',0);m.mem_write(seed,struct.pack('<2I',0x3f812345,0xbf654321))
    capture=b''.join(b'\xdb\x3d'+struct.pack('<I',retained+i*12) for i in range(depth))
    stop=trampoline+len(capture);m.mem_write(trampoline,capture+b'\x90')
    sp=STACK+0x8000;m.mem_write(sp,struct.pack('<2I',trampoline,direction))
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,
           UC_X86_REG_EDI:0x3456789a,UC_X86_REG_EBP:0x456789ab}
    for reg,value in saved.items():m.reg_write(reg,value)
    m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,component if case.get('wrapper') else view)
    cw=case.get('cw',0x37f);m.reg_write(UC_X86_REG_FPCW,cw)
    entry=spec['aim_direction' if case.get('wrapper') else 'aim_within_limits']
    code=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))
    code+=b'\xe9'+struct.pack('<I',(entry-preload-len(code)-5)&0xffffffff);m.mem_write(preload,code)
    calls=[];returned=[];arg_slot=[];statuses=[]
    def hook(uc,address,size,data):
        ecx=uc.reg_read(UC_X86_REG_ECX);current_sp=uc.reg_read(UC_X86_REG_ESP)
        if address in [spec['aim_within_limits'],symbols.get('bfv_aim_within_limits')]:
            # Entry detour and compiled entry may both be observed at the same SP.
            if not arg_slot:arg_slot.append(current_sp+4)
        if address==notify_stub:
            assert ecx==receiver and r32(current_sp+4)==5
            calls.append(['notify',5])
            if case.get('mutation')=='notify_pointer':w32(sp+4,alternate_direction)
        if address==owner_stub:
            assert ecx==owner;calls.append(['world'])
            if case.get('mutation')=='owner_mount':w32(view+8,alternate_mount)
        if address==spec['aim_compose']:
            expected_mount=alternate_mount if case.get('mutation')=='notify_view' else mount
            assert r32(current_sp+4)==expected_mount+0x80 and r32(current_sp+8)==world
            matrix=[0]*16;matrix[0]=case.get('projection',0x3f000000);matrix[8]=case.get('side',0x3f800000)
            matrix[1]=0x3eaaaaab;matrix[2]=0xbeaaaaab
            if not case.get('real_compose'):uc.mem_write(ecx,struct.pack('<16I',*matrix))
            calls.append(['compose'])
            if case.get('mutation')=='matrix_limits':w32(mount+0x68,0xc0490fdb);w32(mount+0x74,0x40490fdb)
            if case.get('mutation')=='matrix_pointer':w32(arg_slot[0],alternate_direction)
        if address==spec['aim_inverse_sine']:
            calls.append(['inverse_sine',r32(current_sp+4),r32(arg_slot[0])])
            if case.get('mutation')=='trig_direction':w32(direction,0xbf800000)
            if case.get('mutation')=='trig_limits':w32(mount+0x68,0);w32(mount+0x74,0)
        if address==trampoline:statuses.append(uc.reg_read(UC_X86_REG_FPSW))
        if address==stop:returned.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(preload,0,timeout=1_000_000,count=10000)
    assert returned and len(statuses)==1 and m.reg_read(UC_X86_REG_ESP)==sp+8
    for reg,value in saved.items():assert m.reg_read(reg)==value
    assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
    return dict(accepted=m.reg_read(UC_X86_REG_EAX)&0xff,calls=calls,return_status=statuses[0],
        final_status=m.reg_read(UC_X86_REG_FPSW),retained=[bytes(m.mem_read(retained+i*12,10)).hex() for i in range(depth)],
        direction=bytes(m.mem_read(direction,12)).hex(),limits=[r32(mount+0x68),r32(mount+0x74)])

def compare_aim_limits(original,original_pe,edited,edited_pe,spec,symbols):
    for name in ['aim_direction','aim_within_limits']:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name]
    results=[]
    for case in aim_cases():
        try:
            old=run_aim(original,original_pe,spec,{},case);new=run_aim(edited,edited_pe,spec,symbols,case)
            assert old==new,(case,old,new)
        except Exception as error:raise RuntimeError(f'aim inputs {case}') from error
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
        cases=compare_aim_limits(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (PROJECT/'build'/target/'aim-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} aiming-limit comparisons passed',flush=True)
