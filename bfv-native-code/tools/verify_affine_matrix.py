"""Execute original/source affine composition, including overlapping buffers.

No numeric services are mocked. Compare complete backing memory, ABI, x87 status,
precision/rounding control, and retained caller values.
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
    UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,
    UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK

SPECIAL=[0,0x80000000,1,0x80000001,0x3f800000,0xbf800000,0x3eaaaaab,
         0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0xffc23456,
         0x7f812345,0xff812345]
LAYOUTS={'separate':(0,0x100,0x200),'output_left':(0,0x100,0),
         'output_right':(0,0x100,0x100),'inputs_same':(0,0,0x200),'all_same':(0,0,0),
         'partial_left':(0,0x100,4),'partial_right':(0,0x100,0x104),
         'partial_inputs':(0,32,0x200),'partial_all':(0,16,8)}

def matrix_cases():
    identity=[0x3f800000 if i in [0,5,10,15] else 0 for i in range(16)]
    cases=[dict(left=identity,right=identity)]
    for a,b in itertools.product(SPECIAL,repeat=2):
        cases.append(dict(left=[a]*16,right=[b]*16))
    rng=random.Random(0x49acd0)
    for i in range(40):
        choices=SPECIAL if i<20 else [rng.getrandbits(32) for _ in range(32)]
        cases.append(dict(left=[rng.choice(choices) for _ in range(16)],right=[rng.choice(choices) for _ in range(16)]))
    fixtures=cases[-16:]+[cases[0]]
    cases += [dict(case,layout=layout) for case,layout in itertools.product(fixtures,list(LAYOUTS)[1:])]
    selected=fixtures+[dict(left=[0x3f800001]*16,right=[0x3effffff]*16),
                       dict(left=[0x7f812345]*16,right=[0x7fc23456]*16)]
    cases += [dict(case,cw=0x7f|pc|rc,depth=depth,layout=layout)
              for case,pc,rc,depth,layout in itertools.product(selected,[0,0x200,0x300],
                    [0,0x400,0x800,0xc00],[2,6],['separate','partial_all'])]
    return cases

def run_matrix(image,pe,spec,case):
    m=load_machine(image,pe);backing=ARENA+0x1000;lo,ro,oo=LAYOUTS[case.get('layout','separate')]
    left,right,output=[backing+offset for offset in [lo,ro,oo]]
    # Initialize in the same order when input regions overlap.
    m.mem_write(backing,b'\xa5'*0x300)
    m.mem_write(left,struct.pack('<16I',*case['left']));m.mem_write(right,struct.pack('<16I',*case['right']))
    trampoline=ARENA+0x2000;start=ARENA+0x2100;seed=ARENA+0x2200;retained=ARENA+0x2300
    depth=case.get('depth',0);m.mem_write(seed,struct.pack('<6I',0x3f812345,0xbf654321,0x3e800000,0xbe800000,0x3f400000,0xbf400000))
    capture=b''.join(b'\xdb\x3d'+struct.pack('<I',retained+12*i) for i in range(depth))
    stop=trampoline+len(capture);m.mem_write(trampoline,capture+b'\x90')
    sp=STACK+0x8000;m.mem_write(sp,struct.pack('<3I',trampoline,left,right))
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,
           UC_X86_REG_EDI:0x3456789a,UC_X86_REG_EBP:0x456789ab}
    for reg,value in saved.items():m.reg_write(reg,value)
    m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,output)
    cw=case.get('cw',0x37f);m.reg_write(UC_X86_REG_FPCW,cw)
    code=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))
    entry=spec['aim_compose'];code+=b'\xe9'+struct.pack('<I',(entry-start-len(code)-5)&0xffffffff)
    m.mem_write(start,code);returned=[];status=[]
    def hook(uc,address,size,data):
        if address==trampoline:status.append(uc.reg_read(UC_X86_REG_FPSW))
        if address==stop:returned.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=1_000_000,count=10000)
    assert returned and len(status)==1 and m.reg_read(UC_X86_REG_ESP)==sp+12
    assert m.reg_read(UC_X86_REG_EAX)==output
    for reg,value in saved.items():assert m.reg_read(reg)==value
    assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
    return dict(memory=bytes(m.mem_read(backing,0x300)).hex(),return_status=status[0],
        final_status=m.reg_read(UC_X86_REG_FPSW),retained=[bytes(m.mem_read(retained+12*i,10)).hex() for i in range(depth)])

def compare_affine_matrix(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['aim_compose'];jump=edited_pe.get_data(entry-0x400000,5)
    assert jump[0]==0xe9 and entry+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_affine_compose']
    results=[]
    for case in matrix_cases():
        try:
            old=run_matrix(original,original_pe,spec,case);new=run_matrix(edited,edited_pe,spec,case)
            assert old==new,(case,old,new)
        except Exception as error:raise RuntimeError(f'affine matrix inputs {case}') from error
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
        cases=compare_affine_matrix(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (PROJECT/'build'/target/'matrix-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} affine matrix comparisons passed',flush=True)
