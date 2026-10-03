"""Verify every audited constant-return detour against its actual native body.

Reuse immutable machines: tested functions may only read their return address
and (for C) source-owned constant storage. No writes or delegated calls are allowed.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,
    UC_X86_REG_EFLAGS,UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,
    UC_X86_REG_FP0,UC_X86_REG_FP1,UC_X86_REG_FP2,UC_X86_REG_FP3,
    UC_X86_REG_FP4,UC_X86_REG_FP5,UC_X86_REG_FP6,UC_X86_REG_FP7)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK

REGISTERS=[UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
FLOAT_STATE=[UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,
    UC_X86_REG_FP0,UC_X86_REG_FP1,UC_X86_REG_FP2,UC_X86_REG_FP3,
    UC_X86_REG_FP4,UC_X86_REG_FP5,UC_X86_REG_FP6,UC_X86_REG_FP7]
SCENARIOS=[(0x202,0),(0x247,0),(0xad6,0),(0xe97,0),(0x202,2),(0xe97,2)]

class Runner:
    def __init__(self,image,pe,payload=None):
        self.machine=load_machine(image,pe);self.payload=payload
        self.start=ARENA+0x1000;self.stop=ARENA+0x1200;self.seed=ARENA+0x1400
        self.machine.mem_write(self.stop,b'\x90');self.machine.mem_write(self.seed,struct.pack('<2I',0x3f812345,0xbf654321))
        self.current={}
        def code(uc,address,size,data):
            if address==self.current['entry']:
                self.current['float_before']=[uc.reg_read(reg) for reg in FLOAT_STATE]
            if address==self.stop:self.current['returned']=True;uc.emu_stop()
            elif address!=self.start and address>=self.start and address<self.start+32:pass
            elif address not in [self.current['entry']] and self.current.get('float_before') is not None:
                # Each original body is MOV/RET; compiled C must remain inside
                # the appended source payload until it returns.
                if self.payload is None:assert address==self.current['entry']+5
                else:assert self.payload[0]<=address<self.payload[1],hex(address)
        def read(uc,access,address,size,value,data):
            if self.current.get('float_before') is None:return
            self.current['reads'].append((address,size))
        def write(uc,access,address,size,value,data):self.current['writes'].append((address,size))
        self.machine.hook_add(UC_HOOK_CODE,code);self.machine.hook_add(UC_HOOK_MEM_READ,read)
        self.machine.hook_add(UC_HOOK_MEM_WRITE,write)

    def run(self,entry,flags,depth,case_index):
        m=self.machine;sp=STACK+0x8000
        self.current=dict(entry=int(entry['address'],16),reads=[],writes=[],returned=False,float_before=None)
        m.mem_write(sp,struct.pack('<17I',self.stop,*[(0x12340000+i*0x101+case_index)&0xffffffff for i in range(16)]))
        before={reg:(0x11223344+n*0x1010101+case_index)&0xffffffff for n,reg in enumerate(REGISTERS)}
        for reg,value in before.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0xabcdef01);m.reg_write(UC_X86_REG_ESP,sp)
        m.reg_write(UC_X86_REG_EFLAGS,flags);m.reg_write(UC_X86_REG_FPCW,0x37f)
        m.reg_write(UC_X86_REG_FPSW,0);m.reg_write(UC_X86_REG_FPTAG,0xffff)
        preload=b''.join(b'\xd9\x05'+struct.pack('<I',self.seed+4*i) for i in range(depth))
        preload+=b'\xe9'+struct.pack('<I',(self.current['entry']-self.start-len(preload)-5)&0xffffffff)
        m.mem_write(self.start,preload)
        # Reused machines need explicit invalidation after changing trampoline
        # instructions; otherwise a prior entry/depth may remain translated.
        m.ctl_remove_cache(self.start,self.start+32)
        m.emu_start(self.start,0,timeout=1_000_000,count=100)
        assert self.current['returned'] and not self.current['writes']
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+entry['stack_cleanup']
        assert m.reg_read(UC_X86_REG_EAX)==int(entry['word'],16)
        for reg,value in before.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_EFLAGS)==flags
        float_after=[m.reg_read(reg) for reg in FLOAT_STATE]
        assert float_after==self.current['float_before']
        for address,size in self.current['reads']:
            assert (address,size)==(sp,4) or (self.payload and size==4 and self.payload[0]<=address<self.payload[1])
        return dict(word=f"{m.reg_read(UC_X86_REG_EAX):08x}",flags=m.reg_read(UC_X86_REG_EFLAGS),
                    stack_cleanup=entry['stack_cleanup'],float_control=float_after[0],float_status=float_after[1],float_tag=float_after[2])

def compare_constant_returns(original,original_pe,edited,edited_pe,spec,symbols):
    target='client' if spec['file']=='BfVietnam.exe' else 'server'
    catalog=json.loads((PROJECT/'constant-returns.json').read_text())['targets'][target]
    assert catalog['original_sha256']==spec['sha']
    section=next(s for s in edited_pe.sections if s.Name.rstrip(b'\0')==b'.bfvmod')
    payload=(0x400000+section.VirtualAddress,0x400000+section.VirtualAddress+section.Misc_VirtualSize)
    old,new=Runner(original,original_pe),Runner(edited,edited_pe,payload);results=[]
    for entry in catalog['entries']:
        address=int(entry['address'],16);jump=edited_pe.get_data(address-0x400000,5)
        assert jump[0]==0xe9 and address+5+struct.unpack('<i',jump[1:])[0]==symbols[entry['symbol']]
        assert original_pe.get_data(address-0x400000,5).hex()==entry['guarded_prefix']
        for index,(flags,depth) in enumerate(SCENARIOS):
            try:
                a=old.run(entry,flags,depth,index);b=new.run(entry,flags,depth,index)
                assert a==b,(entry,a,b)
            except Exception as error:raise RuntimeError(f'constant entry {entry["address"]}, flags {flags:x}, x87 depth {depth}') from error
            results.append(dict(address=entry['address'],retained_depth=depth,**a))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_constant_returns(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (PROJECT/'build'/target/'constant-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} constant-return comparisons passed',flush=True)
