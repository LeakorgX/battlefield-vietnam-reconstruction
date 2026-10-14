"""Verify every audited constant global-store detour against its native body.

Original and reconstructed entries run side by side under varied CPU flags,
scratch values and x87 stack depths. They must perform exactly the catalogued
stores (address, width, value) in native order, read nothing outside their own
frame, return with a balanced stack, and preserve EBX/ESI/EDI/EBP and the x87
state. The functions return void, so EAX, ECX, EDX and flags are scratch.
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

PRESERVED=[UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
FLOAT_STATE=[UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,
    UC_X86_REG_FP0,UC_X86_REG_FP1,UC_X86_REG_FP2,UC_X86_REG_FP3,
    UC_X86_REG_FP4,UC_X86_REG_FP5,UC_X86_REG_FP6,UC_X86_REG_FP7]
# (CPU flags, retained x87 depth)
SCENARIOS=[(0x202,0),(0x247,0),(0xad6,2),(0xe97,1)]

class Runner:
    def __init__(self,image,pe,payload=None):
        self.machine=load_machine(image,pe);self.payload=payload
        self.start=ARENA+0x1000;self.stop=ARENA+0x1200;self.seed=ARENA+0x1400
        self.machine.mem_write(self.stop,b'\x90');self.machine.mem_write(self.seed,struct.pack('<2I',0x3f812345,0xbf654321))
        self.current={}
        def outside(address,size):
            sp=self.current['sp'];return not (sp-0x100<=address and address+size<=sp+0x48)
        def code(uc,address,size,data):
            c=self.current
            if address==c['entry']:c['inside']=True;c['float_before']=[uc.reg_read(r) for r in FLOAT_STATE]
            if address==self.stop:c['returned']=True;uc.emu_stop();return
            if not c.get('inside') or address==c['entry']:return
            if self.payload is None:assert c['entry']<address<c['entry']+c['length'],hex(address)
            else:assert self.payload[0]<=address<self.payload[1],hex(address)
        def read(uc,access,address,size,value,data):
            c=self.current
            if c.get('inside') and outside(address,size):c['events'].append(('read',address,size))
        def write(uc,access,address,size,value,data):
            c=self.current
            if c.get('inside') and outside(address,size):c['events'].append(('write',address,size,value&((1<<(8*size))-1)))
        self.machine.hook_add(UC_HOOK_CODE,code);self.machine.hook_add(UC_HOOK_MEM_READ,read)
        self.machine.hook_add(UC_HOOK_MEM_WRITE,write)

    def run(self,entry,flags,depth,case_index):
        m=self.machine;sp=STACK+0x8000
        self.current=dict(entry=int(entry['address'],16),length=len(entry['native_body'])//2,sp=sp,
            events=[],returned=False,inside=False)
        for g,w,_ in entry['stores']:  # poison targets so a skipped store is visible
            m.mem_write(int(g,16),bytes((0xa5+case_index+i)&0xff for i in range(w)))
        m.mem_write(sp,struct.pack('<17I',self.stop,*[(0x12340000+i*0x101+case_index)&0xffffffff for i in range(16)]))
        before={reg:(0x11223344+n*0x1010101+case_index)&0xffffffff for n,reg in enumerate(PRESERVED)}
        for reg,value in before.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0xabcdef01);m.reg_write(UC_X86_REG_EDX,0x13572468);m.reg_write(UC_X86_REG_ECX,0x2468ace0)
        m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_EFLAGS,flags);m.reg_write(UC_X86_REG_FPCW,0x37f)
        m.reg_write(UC_X86_REG_FPSW,0);m.reg_write(UC_X86_REG_FPTAG,0xffff)
        preload=b''.join(b'\xd9\x05'+struct.pack('<I',self.seed+4*(i%2)) for i in range(depth))
        preload+=b'\xe9'+struct.pack('<I',(self.current['entry']-self.start-len(preload)-5)&0xffffffff)
        m.mem_write(self.start,preload);m.ctl_remove_cache(self.start,self.start+32)
        m.emu_start(self.start,0,timeout=1_000_000,count=200)
        c=self.current
        assert c['returned'] and m.reg_read(UC_X86_REG_ESP)==sp+4
        for reg,value in before.items():assert m.reg_read(reg)==value
        float_after=[m.reg_read(reg) for reg in FLOAT_STATE]
        assert float_after==c['float_before']
        memory=[bytes(m.mem_read(int(g,16),w)).hex() for g,w,_ in entry['stores']]
        return dict(events=c['events'],memory=memory,
            float_control=float_after[0],float_status=float_after[1],float_tag=float_after[2])

def expected(entry):
    return [('write',int(g,16),w,int(v,16)) for g,w,v in entry['stores']]

def compare_global_stores(original,original_pe,edited,edited_pe,spec,symbols):
    target='client' if spec['file']=='BfVietnam.exe' else 'server'
    catalog=json.loads((PROJECT/'global-stores.json').read_text())['targets'][target]
    assert catalog['original_sha256']==spec['sha']
    section=next(s for s in edited_pe.sections if s.Name.rstrip(b'\0')==b'.bfvmod')
    payload=(0x400000+section.VirtualAddress,0x400000+section.VirtualAddress+section.Misc_VirtualSize)
    old,new=Runner(original,original_pe),Runner(edited,edited_pe,payload);results=[]
    for entry in catalog['entries']:
        address=int(entry['address'],16);body=bytes.fromhex(entry['native_body'])
        jump=edited_pe.get_data(address-0x400000,5)
        assert jump[0]==0xe9 and address+5+struct.unpack('<i',jump[1:])[0]==symbols[entry['symbol']]
        assert original_pe.get_data(address-0x400000,len(body))==body
        for index,(flags,depth) in enumerate(SCENARIOS):
            try:
                a=old.run(entry,flags,depth,index);b=new.run(entry,flags,depth,index)
                assert a==b and a['events']==expected(entry),(entry,a,b)
            except Exception as error:raise RuntimeError(f'global store {entry["address"]}, flags {flags:x}, x87 depth {depth}') from error
            results.append(dict(address=entry['address'],stores=len(entry['stores']),retained_depth=depth))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_global_stores(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (PROJECT/'build'/target/'global-store-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} global-store comparisons passed',flush=True)
