"""Verify every audited scalar deleting destructor detour against its native body.

The destructor and shared deallocator are controlled: a code hook records each
call and returns. Original and reconstructed entries must produce the same
ordered sequence of vtable stores and calls (destructor receiver, deallocator
argument), free exactly when bit 0 of the flags is set, return the object in
EAX, pop the flags argument, restore EBX/ESI/EDI/EBP on return, keep the x87 state, and
write nothing else outside their own stack frame. ECX, EDX and flags are
scratch under the calling convention and are not compared.
"""
import argparse
import hashlib
import json
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,
    UC_X86_REG_EIP,UC_X86_REG_EFLAGS,UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,
    UC_X86_REG_FP0,UC_X86_REG_FP1,UC_X86_REG_FP2,UC_X86_REG_FP3,
    UC_X86_REG_FP4,UC_X86_REG_FP5,UC_X86_REG_FP6,UC_X86_REG_FP7)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK

PRESERVED=[UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
FLOAT_STATE=[UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,
    UC_X86_REG_FP0,UC_X86_REG_FP1,UC_X86_REG_FP2,UC_X86_REG_FP3,
    UC_X86_REG_FP4,UC_X86_REG_FP5,UC_X86_REG_FP6,UC_X86_REG_FP7]
# (argument flags, CPU flags, retained x87 depth, initial scratch value)
SCENARIOS=[(0,0x202,0,0),(1,0x247,0,0xffffffff),(2,0xad6,2,0x80000001),(3,0xe97,2,0x12345678),
    (0xfffffffe,0x202,1,0xdeadbeef),(0xffffffff,0xa83,1,0x00000001)]

class Runner:
    def __init__(self,image,pe,deallocate,payload=None):
        self.machine=load_machine(image,pe);self.payload=payload;self.deallocate=deallocate
        self.start=ARENA+0x1000;self.stop=ARENA+0x1200;self.seed=ARENA+0x1400
        self.machine.mem_write(self.stop,b'\x90');self.machine.mem_write(self.seed,struct.pack('<2I',0x3f812345,0xbf654321))
        self.current={}
        def code(uc,address,size,data):
            c=self.current
            if address==c['entry']:c['inside']=True;c['float_before']=[uc.reg_read(r) for r in FLOAT_STATE]
            if address==self.stop:c['returned']=True;uc.emu_stop();return
            if not c.get('inside') or address==c['entry']:return
            if address in (c['destructor'],self.deallocate):
                esp=uc.reg_read(UC_X86_REG_ESP)
                back,argument=struct.unpack('<2I',uc.mem_read(esp,8))
                if address==c['destructor']:c['events'].append(('destructor',uc.reg_read(UC_X86_REG_ECX)))
                else:c['events'].append(('deallocate',argument))
                c['call_float'].append([uc.reg_read(r) for r in FLOAT_STATE])
                # Callee clobbers its scratch registers, then returns (both callers clean up).
                uc.reg_write(UC_X86_REG_EAX,0xc1c1c1c1);uc.reg_write(UC_X86_REG_ECX,0x5c5c5c5c)
                uc.reg_write(UC_X86_REG_EDX,0xd0d0d0d0);uc.reg_write(UC_X86_REG_ESP,esp+4)
                uc.reg_write(UC_X86_REG_EIP,back);return
            if self.payload is None:assert c['entry']<address<c['entry']+40,hex(address)
            else:assert self.payload[0]<=address<self.payload[1],hex(address)
        def write(uc,access,address,size,value,data):
            c=self.current
            if not c.get('inside'):return
            sp=c['sp']
            if sp-0x100<=address and address+size<=sp:return
            c['events'].append(('write',address,size,value&((1<<(8*size))-1)))
        self.machine.hook_add(UC_HOOK_CODE,code);self.machine.hook_add(UC_HOOK_MEM_WRITE,write)

    def run(self,entry,argument,flags,depth,scratch,case_index):
        m=self.machine;sp=STACK+0x8000;obj=ARENA+0x4000+(case_index%4)*0x41
        destructor=int(entry['destructor'],16) if entry['destructor'] else None
        self.current=dict(entry=int(entry['address'],16),destructor=destructor,sp=sp,
            events=[],call_float=[],returned=False,inside=False)
        m.mem_write(obj,struct.pack('<4I',0x0badf00d,1,2,3))
        m.mem_write(sp,struct.pack('<17I',self.stop,argument,*[(0x12340000+i*0x101+case_index)&0xffffffff for i in range(15)]))
        before={reg:(0x11223344+n*0x1010101+case_index)&0xffffffff for n,reg in enumerate(PRESERVED)}
        for reg,value in before.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,scratch);m.reg_write(UC_X86_REG_EDX,scratch);m.reg_write(UC_X86_REG_ECX,obj)
        m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_EFLAGS,flags);m.reg_write(UC_X86_REG_FPCW,0x37f)
        m.reg_write(UC_X86_REG_FPSW,0);m.reg_write(UC_X86_REG_FPTAG,0xffff)
        preload=b''.join(b'\xd9\x05'+struct.pack('<I',self.seed+4*(i%2)) for i in range(depth))
        preload+=b'\xe9'+struct.pack('<I',(self.current['entry']-self.start-len(preload)-5)&0xffffffff)
        m.mem_write(self.start,preload);m.ctl_remove_cache(self.start,self.start+32)
        m.emu_start(self.start,0,timeout=1_000_000,count=200)
        c=self.current
        assert c['returned'] and m.reg_read(UC_X86_REG_ESP)==sp+8
        for reg,value in before.items():assert m.reg_read(reg)==value
        float_after=[m.reg_read(reg) for reg in FLOAT_STATE]
        assert float_after==c['float_before'] and all(f==c['float_before'] for f in c['call_float'])
        return dict(object=obj,events=c['events'],eax=m.reg_read(UC_X86_REG_EAX),
            float_control=float_after[0],float_status=float_after[1],float_tag=float_after[2])

def expected_events(entry,obj,argument):
    events=[]
    if entry['vtable']:events.append(('write',obj,4,int(entry['vtable'],16)))
    if entry['destructor']:events.append(('destructor',obj))
    if argument&1:events.append(('deallocate',obj))
    return events

def compare_deleting_destructors(original,original_pe,edited,edited_pe,spec,symbols):
    target='client' if spec['file']=='BfVietnam.exe' else 'server'
    catalog=json.loads((PROJECT/'deleting-destructors.json').read_text())['targets'][target]
    assert catalog['original_sha256']==spec['sha'];deallocate=int(catalog['deallocate'],16)
    section=next(s for s in edited_pe.sections if s.Name.rstrip(b'\0')==b'.bfvmod')
    payload=(0x400000+section.VirtualAddress,0x400000+section.VirtualAddress+section.Misc_VirtualSize)
    old,new=Runner(original,original_pe,deallocate),Runner(edited,edited_pe,deallocate,payload);results=[]
    for entry in catalog['entries']:
        address=int(entry['address'],16);body=bytes.fromhex(entry['native_body'])
        jump=edited_pe.get_data(address-0x400000,5)
        assert jump[0]==0xe9 and address+5+struct.unpack('<i',jump[1:])[0]==symbols[entry['symbol']]
        assert original_pe.get_data(address-0x400000,len(body))==body
        for index,(argument,flags,depth,scratch) in enumerate(SCENARIOS):
            try:
                a=old.run(entry,argument,flags,depth,scratch,index);b=new.run(entry,argument,flags,depth,scratch,index)
                assert a==b and a['events']==expected_events(entry,a['object'],argument) and a['eax']==a['object'],(entry,a,b)
            except Exception as error:raise RuntimeError(f'deleting destructor {entry["address"]}, flags argument {argument:x}, x87 depth {depth}') from error
            results.append(dict(address=entry['address'],kind=entry['kind'],flags_argument=f'{argument:08x}',
                retained_depth=depth,events=len(a['events'])))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_deleting_destructors(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (PROJECT/'build'/target/'deleting-destructor-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} deleting-destructor comparisons passed',flush=True)
