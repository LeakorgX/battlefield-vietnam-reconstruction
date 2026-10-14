"""Verify every audited static-object initializer detour against its native body.

Covers static-initializers.json and constructor-calls.json (initializers whose
constructor takes immediate arguments, with or without atexit registration).
The constructor and atexit are controlled: a code hook records each call and
returns a marker. Original and reconstructed entries must make the same calls
in the same order with the same receiver/arguments, return the last callee's
result in EAX, balance the stack, preserve EBX/ESI/EDI/EBP and the x87 state, and write
nothing outside their own stack frame. EAX before each call, ECX, EDX and flags
are scratch under the calling convention and are not compared.
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
SCENARIOS=[(0x202,0,0x00000000),(0x247,0,0xffffffff),(0xad6,2,0x80000001),(0xe97,2,0x12345678)]

class Runner:
    def __init__(self,image,pe,atexit,payload=None):
        self.machine=load_machine(image,pe);self.payload=payload;self.atexit=atexit
        self.start=ARENA+0x1000;self.stop=ARENA+0x1200;self.seed=ARENA+0x1400
        self.machine.mem_write(self.stop,b'\x90');self.machine.mem_write(self.seed,struct.pack('<2I',0x3f812345,0xbf654321))
        self.current={}
        def code(uc,address,size,data):
            c=self.current
            if address==c['entry']:c['inside']=True;c['float_before']=[uc.reg_read(r) for r in FLOAT_STATE]
            if address==self.stop:c['returned']=True;uc.emu_stop();return
            if not c.get('inside') or address==c['entry']:return
            if address in (c['constructor'],self.atexit):
                esp=uc.reg_read(UC_X86_REG_ESP)
                back,argument=struct.unpack('<2I',uc.mem_read(esp,8))
                pop=0
                if address==c['constructor']:
                    count=len(c['arguments']);pop=4*count
                    arguments=struct.unpack(f'<{count}I',uc.mem_read(esp+4,pop)) if count else ()
                    c['calls'].append(('constructor',uc.reg_read(UC_X86_REG_ECX),*arguments));result=0x0c0c0000|c['case']
                else:
                    c['calls'].append(('atexit',argument));result=0xa7e00000|c['case']
                c['call_float'].append([uc.reg_read(r) for r in FLOAT_STATE])
                c['call_preserved'].append([uc.reg_read(r) for r in PRESERVED])
                # Callee clobbers its scratch registers, then returns.
                uc.reg_write(UC_X86_REG_EAX,result);uc.reg_write(UC_X86_REG_ECX,0x5c5c5c5c)
                uc.reg_write(UC_X86_REG_EDX,0xd0d0d0d0);uc.reg_write(UC_X86_REG_ESP,esp+4+pop)
                uc.reg_write(UC_X86_REG_EIP,back);return
            if self.payload is None:assert c['entry']<address<c['entry']+c['length'],hex(address)
            else:assert self.payload[0]<=address<self.payload[1],hex(address)
        def write(uc,access,address,size,value,data):
            if self.current.get('inside'):self.current['writes'].append((address,size))
        self.machine.hook_add(UC_HOOK_CODE,code);self.machine.hook_add(UC_HOOK_MEM_WRITE,write)

    def run(self,entry,flags,depth,scratch,case_index):
        m=self.machine;sp=STACK+0x8000
        self.current=dict(entry=int(entry['address'],16),constructor=int(entry['constructor'],16),case=case_index,
            arguments=entry.get('arguments',[]),length=len(entry['native_body'])//2,
            calls=[],call_float=[],call_preserved=[],writes=[],returned=False,inside=False)
        m.mem_write(sp,struct.pack('<17I',self.stop,*[(0x12340000+i*0x101+case_index)&0xffffffff for i in range(16)]))
        before={reg:(0x11223344+n*0x1010101+case_index)&0xffffffff for n,reg in enumerate(PRESERVED)}
        for reg,value in before.items():m.reg_write(reg,value)
        for reg in (UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX):m.reg_write(reg,scratch)
        m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_EFLAGS,flags);m.reg_write(UC_X86_REG_FPCW,0x37f)
        m.reg_write(UC_X86_REG_FPSW,0);m.reg_write(UC_X86_REG_FPTAG,0xffff)
        preload=b''.join(b'\xd9\x05'+struct.pack('<I',self.seed+4*i) for i in range(depth))
        preload+=b'\xe9'+struct.pack('<I',(self.current['entry']-self.start-len(preload)-5)&0xffffffff)
        m.mem_write(self.start,preload);m.ctl_remove_cache(self.start,self.start+32)
        m.emu_start(self.start,0,timeout=1_000_000,count=200)
        c=self.current
        assert c['returned'] and m.reg_read(UC_X86_REG_ESP)==sp+4
        for reg,value in before.items():assert m.reg_read(reg)==value
        for values in c['call_preserved']:assert values==[before[r] for r in PRESERVED]
        float_after=[m.reg_read(reg) for reg in FLOAT_STATE]
        assert float_after==c['float_before'] and all(f==c['float_before'] for f in c['call_float'])
        # Only the initializer's own frame below the return address may change.
        for address,size in c['writes']:assert sp-0x100<=address and address+size<=sp,hex(address)
        return dict(calls=c['calls'],eax=f'{m.reg_read(UC_X86_REG_EAX):08x}',
            float_control=float_after[0],float_status=float_after[1],float_tag=float_after[2])

def compare_constructor_calls(original,original_pe,edited,edited_pe,spec,symbols):
    return compare_static_initializers(original,original_pe,edited,edited_pe,spec,symbols,'constructor-calls.json')

def compare_static_initializers(original,original_pe,edited,edited_pe,spec,symbols,catalog_name='static-initializers.json'):
    target='client' if spec['file']=='BfVietnam.exe' else 'server'
    catalog=json.loads((PROJECT/catalog_name).read_text())['targets'][target]
    assert catalog['original_sha256']==spec['sha'];atexit=int(catalog['atexit'],16)
    section=next(s for s in edited_pe.sections if s.Name.rstrip(b'\0')==b'.bfvmod')
    payload=(0x400000+section.VirtualAddress,0x400000+section.VirtualAddress+section.Misc_VirtualSize)
    old,new=Runner(original,original_pe,atexit),Runner(edited,edited_pe,atexit,payload);results=[]
    for entry in catalog['entries']:
        address=int(entry['address'],16)
        jump=edited_pe.get_data(address-0x400000,5)
        assert jump[0]==0xe9 and address+5+struct.unpack('<i',jump[1:])[0]==symbols[entry['symbol']]
        assert original_pe.get_data(address-0x400000,len(entry['native_body'])//2).hex()==entry['native_body']
        expected=[('constructor',int(entry['object'],16),*[int(x,16) for x in entry.get('arguments',[])])]
        if entry.get('destructor'):expected.append(('atexit',int(entry['destructor'],16)))
        marker=0xa7e00000 if entry.get('destructor') else 0x0c0c0000
        for index,(flags,depth,scratch) in enumerate(SCENARIOS):
            try:
                a=old.run(entry,flags,depth,scratch,index);b=new.run(entry,flags,depth,scratch,index)
                assert a==b and a['calls']==expected and a['eax']==f'{marker|index:08x}',(entry,a,b)
            except Exception as error:raise RuntimeError(f'initializer {entry["address"]}, flags {flags:x}, x87 depth {depth}') from error
            results.append(dict(address=entry['address'],retained_depth=depth,calls=len(a['calls']),eax=a['eax']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both')
    p.add_argument('--catalog',default='static-initializers.json');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_static_initializers(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.catalog)
        (PROJECT/'build'/target/args.catalog.replace('.json','-verification.json')).write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} {args.catalog} comparisons passed',flush=True)
