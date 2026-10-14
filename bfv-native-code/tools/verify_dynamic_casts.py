"""Verify every audited dynamic_cast wrapper detour against its native body.

The runtime cast is controlled: a code hook records its five arguments and
returns a chosen result. Original and reconstructed entries run side by side
over empty/non-empty holders, handles and direct pointers, clear/set flag bytes, null/non-null
cast results, CPU flags and occupied x87 stacks. They must perform the same
data reads and casts, return the same EAX, clean the same stack, preserve
EBX/ESI/EDI/EBP and the x87 state, and write nothing outside their own frame.
ECX, EDX and flags are scratch and are not compared.
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
    UC_X86_REG_EIP,UC_X86_REG_EFLAGS,UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,
    UC_X86_REG_FP0,UC_X86_REG_FP1,UC_X86_REG_FP2,UC_X86_REG_FP3,
    UC_X86_REG_FP4,UC_X86_REG_FP5,UC_X86_REG_FP6,UC_X86_REG_FP7)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK

PRESERVED=[UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP]
FLOAT_STATE=[UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,
    UC_X86_REG_FP0,UC_X86_REG_FP1,UC_X86_REG_FP2,UC_X86_REG_FP3,
    UC_X86_REG_FP4,UC_X86_REG_FP5,UC_X86_REG_FP6,UC_X86_REG_FP7]
# (stored/passed pointer, flag byte, cast result, CPU flags, retained x87 depth)
SCENARIOS=[(0,1,0x0ca57000,0x202,0),(ARENA+0x6000,0,0x0ca57001,0x247,0),(ARENA+0x6000,1,0x0ca57002,0xad6,2),
    (ARENA+0x6004,0x80,0,0xe97,1),(0xfffffff0,0xff,0x0ca57004,0x202,2),(ARENA+0x6008,1,0,0xa83,0)]

class Runner:
    def __init__(self,image,pe,cast,payload=None):
        self.machine=load_machine(image,pe);self.payload=payload;self.cast=cast
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
            if address==self.cast:
                esp=uc.reg_read(UC_X86_REG_ESP)
                back,*arguments=struct.unpack('<6I',uc.mem_read(esp,24))
                c['events'].append(('cast',*arguments))
                c['call_float'].append([uc.reg_read(r) for r in FLOAT_STATE])
                c['call_preserved'].append([uc.reg_read(r) for r in PRESERVED])
                uc.reg_write(UC_X86_REG_EAX,c['result']);uc.reg_write(UC_X86_REG_ECX,0x5c5c5c5c)
                uc.reg_write(UC_X86_REG_EDX,0xd0d0d0d0);uc.reg_write(UC_X86_REG_ESP,esp+4)
                uc.reg_write(UC_X86_REG_EIP,back);return
            if self.payload is None:assert c['entry']<address<c['entry']+c['length'],hex(address)
            else:assert self.payload[0]<=address<self.payload[1],hex(address)
        def read(uc,access,address,size,value,data):
            c=self.current
            if c.get('inside') and outside(address,size):c['events'].append(('read',address,size))
        def write(uc,access,address,size,value,data):
            c=self.current
            if c.get('inside') and outside(address,size):c['events'].append(('write',address,size))
        self.machine.hook_add(UC_HOOK_CODE,code);self.machine.hook_add(UC_HOOK_MEM_READ,read)
        self.machine.hook_add(UC_HOOK_MEM_WRITE,write)

    def run(self,entry,pointer,flag,result,flags,depth,case_index):
        m=self.machine;sp=STACK+0x8000;slot=ARENA+0x5000+(case_index%4)*0x41
        self.current=dict(entry=int(entry['address'],16),length=len(entry['native_body'])//2,sp=sp,result=result,
            events=[],call_float=[],call_preserved=[],returned=False,inside=False)
        m.mem_write(slot,struct.pack('<I',pointer)+bytes(60))
        kind=entry['kind']
        if kind.startswith('handle'):m.mem_write(slot+entry['flag_offset'],bytes([flag]))
        argument=pointer if kind=='pointer' else slot
        m.mem_write(sp,struct.pack('<17I',self.stop,argument,*[(0x12340000+i*0x101+case_index)&0xffffffff for i in range(15)]))
        before={reg:(0x11223344+n*0x1010101+case_index)&0xffffffff for n,reg in enumerate(PRESERVED)}
        for reg,value in before.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0xabcdef01);m.reg_write(UC_X86_REG_EDX,0x13572468)
        m.reg_write(UC_X86_REG_ECX,slot if kind=='handle' else pointer if kind=='self' else 0x2468ace0)
        m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_EFLAGS,flags);m.reg_write(UC_X86_REG_FPCW,0x37f)
        m.reg_write(UC_X86_REG_FPSW,0);m.reg_write(UC_X86_REG_FPTAG,0xffff)
        preload=b''.join(b'\xd9\x05'+struct.pack('<I',self.seed+4*(i%2)) for i in range(depth))
        preload+=b'\xe9'+struct.pack('<I',(self.current['entry']-self.start-len(preload)-5)&0xffffffff)
        m.mem_write(self.start,preload);m.ctl_remove_cache(self.start,self.start+32)
        m.emu_start(self.start,0,timeout=1_000_000,count=200)
        c=self.current
        assert c['returned'] and m.reg_read(UC_X86_REG_ESP)==sp+(4 if kind in ('handle','self') else 8)
        for reg,value in before.items():assert m.reg_read(reg)==value
        for values in c['call_preserved']:assert values==[before[r] for r in PRESERVED]
        float_after=[m.reg_read(reg) for reg in FLOAT_STATE]
        assert float_after==c['float_before'] and all(f==c['float_before'] for f in c['call_float'])
        return dict(slot=slot,events=c['events'],eax=m.reg_read(UC_X86_REG_EAX),
            float_control=float_after[0],float_status=float_after[1],float_tag=float_after[2])

def expected(entry,slot,pointer,flag,result):
    src,dst=int(entry['source_type'],16),int(entry['target_type'],16)
    cast=('cast',pointer,0,src,dst,0)
    if entry['kind']=='holder':return [('read',slot,4),cast],result
    if entry['kind'] in ('pointer','self'):return [cast],result
    events=[('read',slot,4)]
    if not pointer:return events,0
    events.append(('read',slot+entry['flag_offset'],1))
    if not flag:return events,pointer
    return events+[cast],result

def compare_dynamic_casts(original,original_pe,edited,edited_pe,spec,symbols):
    target='client' if spec['file']=='BfVietnam.exe' else 'server'
    catalog=json.loads((PROJECT/'dynamic-casts.json').read_text())['targets'][target]
    assert catalog['original_sha256']==spec['sha'];cast=int(catalog['dynamic_cast'],16)
    section=next(s for s in edited_pe.sections if s.Name.rstrip(b'\0')==b'.bfvmod')
    payload=(0x400000+section.VirtualAddress,0x400000+section.VirtualAddress+section.Misc_VirtualSize)
    old,new=Runner(original,original_pe,cast),Runner(edited,edited_pe,cast,payload);results=[]
    for entry in catalog['entries']:
        address=int(entry['address'],16);body=bytes.fromhex(entry['native_body'])
        jump=edited_pe.get_data(address-0x400000,5)
        assert jump[0]==0xe9 and address+5+struct.unpack('<i',jump[1:])[0]==symbols[entry['symbol']]
        assert original_pe.get_data(address-0x400000,len(body))==body
        for index,(pointer,flag,result,flags,depth) in enumerate(SCENARIOS):
            try:
                a=old.run(entry,pointer,flag,result,flags,depth,index);b=new.run(entry,pointer,flag,result,flags,depth,index)
                events,eax=expected(entry,a['slot'],pointer,flag,result)
                assert a==b and a['events']==events and a['eax']==eax,(entry,a,b)
            except Exception as error:raise RuntimeError(f'dynamic cast {entry["address"]} ({entry["kind"]}), pointer {pointer:x}, flag {flag:x}') from error
            results.append(dict(address=entry['address'],kind=entry['kind'],pointer=f'{pointer:08x}',
                flag=flag,eax=f'{a["eax"]:08x}',casts=sum(e[0]=='cast' for e in a['events'])))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_dynamic_casts(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (PROJECT/'build'/target/'dynamic-cast-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} dynamic-cast comparisons passed',flush=True)
