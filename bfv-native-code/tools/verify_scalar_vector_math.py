"""Compare real shared math entries with guarded source replacements.

No numeric service is stubbed. Compare extended returns, memory, stack cleanup,
nonvolatile registers and x87 status across precision and rounding modes.
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

BITS=[0,0x80000000,1,0x80000001,0x3f000000,0x3f800000,0xbf800000,
      0x7f7fffff,0xff7fffff,0x7f800000,0xff800000,0x7fc12345,0xffc23456,
      0x7f812345,0xff812345]
MODES=[0x7f|pc|rc for pc in [0,0x200,0x300] for rc in [0,0x400,0x800,0xc00]]

def math_cases():
    cases=[]
    for operation in ['float_minimum','float_maximum']:
        cases.extend(dict(operation=operation,args=list(pair)) for pair in itertools.product(BITS,repeat=2))
    for value in BITS:
        for lower,upper in [(0,0x3f800000),(0xbf800000,0x3f800000),(0x3f800000,0),
                            (0x7fc12345,0x3f800000),(0,0xff812345)]:
            cases.append(dict(operation='float_clamp',args=[lower,value,upper]))
    vectors=[[x,0x3f800000,0xbf800000] for x in BITS]
    vectors += [[0,0,0],[0x80000000]*3,[1]*3,[0x7f7fffff]*3,
                [0x3f800001,0x3effffff,0xbf800001],
                [0x7fc12345,0xffc23456,0x7f812345],
                [0x7f812345,0x7fc12345,0xffc23456]]
    for vector in vectors:
        cases.append(dict(operation='vector_length',vector=vector,args=[]))
        cases.extend(dict(operation='vector_divide',vector=vector,args=[divisor]) for divisor in BITS)
    # Repeat selected exceptional/rounding cases in every mode with two occupied
    # caller registers below the helper's operands. Those values must survive.
    selected=[c for c in cases if c['operation'].startswith('vector') or
              any(x in [1,0x7f812345,0x7fc12345] for x in c['args'])]
    cases += [dict(case,cw=cw,depth=2) for cw in MODES for case in selected]
    return cases

def run_math(image,pe,spec,case):
    m=load_machine(image,pe);vector=ARENA+0x100;result=ARENA+0x200
    trampoline=ARENA+0x300;start=ARENA+0x400;seed=ARENA+0x500
    operation=case['operation'];scalar=operation!='vector_divide';depth=case.get('depth',0)
    m.mem_write(vector,struct.pack('<3I',*case.get('vector',[0x11223344,0x55667788,0x99aabbcc])))
    m.mem_write(seed,struct.pack('<2I',0x3f812345,0xbf654321))
    # Capture the returned x87 value without rounding it to float32.
    code=(b'\xdb\x3d'+struct.pack('<I',result)) if scalar else b''
    # Also save and pop the caller's retained x87 registers after the return.
    code+=b''.join(b'\xdb\x3d'+struct.pack('<I',result+16+12*i) for i in range(depth))
    stop=trampoline+len(code);m.mem_write(trampoline,code+b'\x90')
    args=case['args'];sp=STACK+0x8000
    m.mem_write(sp,struct.pack('<'+'I'*(len(args)+1),trampoline,*args))
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,
           UC_X86_REG_EDI:0x3456789a,UC_X86_REG_EBP:0x456789ab}
    for reg,value in saved.items():m.reg_write(reg,value)
    m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,vector)
    cw=case.get('cw',0x37f);m.reg_write(UC_X86_REG_FPCW,cw)
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))
    entry=spec[operation];preload+=b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff)
    m.mem_write(start,preload);returned=[];status=[]
    def hook(uc,address,size,data):
        if address==trampoline:
            status.append(uc.reg_read(UC_X86_REG_FPSW))
            assert uc.reg_read(UC_X86_REG_ESP)==sp+4+len(args)*4
        if address==stop:returned.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=1_000_000,count=3000)
    assert returned and len(status)==1
    for reg,value in saved.items():assert m.reg_read(reg)==value
    assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
    if not scalar:assert m.reg_read(UC_X86_REG_EAX)==vector
    return dict(result=bytes(m.mem_read(result,10)).hex() if scalar else None,
        retained=[bytes(m.mem_read(result+16+12*i,10)).hex() for i in range(depth)],
        vector=bytes(m.mem_read(vector,12)).hex(),return_status=status[0],
        final_status=m.reg_read(UC_X86_REG_FPSW))

def compare_scalar_vector_math(original,original_pe,edited,edited_pe,spec,symbols):
    assert original_pe.get_data(spec['math_one']-0x400000,4)==struct.pack('<I',0x3f800000)
    for name in ['float_minimum','float_maximum','float_clamp','vector_length','vector_divide']:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name]
    results=[]
    for case in math_cases():
        try:
            old=run_math(original,original_pe,spec,case)
            new=run_math(edited,edited_pe,spec,case)
            assert old==new,(case,old,new)
        except Exception as error:raise RuntimeError(f'shared math input {case}') from error
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
        cases=compare_scalar_vector_math(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (PROJECT/'build'/target/'shared-math-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} shared scalar/vector math comparisons passed',flush=True)
