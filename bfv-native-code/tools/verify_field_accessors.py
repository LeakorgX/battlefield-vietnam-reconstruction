"""Verify every audited field/global accessor detour against its native body.

The compiled C body must be byte-identical to the original body. Original and
reconstructed entries then execute side by side across stored bit patterns,
unaligned objects, CPU flag states and occupied x87 stacks, comparing EAX,
every preserved register, flags, stack cleanup, full x87 state and the exact
sequence of data reads and writes (with values).
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
PATTERNS=[b'\0'*8,bytes.fromhex('0100000000000000'),bytes.fromhex('ffffff7fffffef7f'),
    bytes.fromhex('00000080000000c0'),b'\xff'*8,bytes.fromhex('efbeadde78563412'),
    bytes.fromhex('0000803f0000f03f'),bytes.fromhex('4523c17f010000f0')]
CLEANUP={'word_setter':4}
STORE={'word_setter','byte_set_one'}

class Runner:
    def __init__(self,image,pe,payload=None):
        self.machine=load_machine(image,pe);self.payload=payload
        self.start=ARENA+0x1000;self.stop=ARENA+0x1200;self.seed=ARENA+0x1400
        self.machine.mem_write(self.stop,b'\x90');self.machine.mem_write(self.seed,struct.pack('<2I',0x3f812345,0xbf654321))
        self.current={}
        def code(uc,address,size,data):
            c=self.current
            if address==c['entry']:c['float_before']=[uc.reg_read(r) for r in FLOAT_STATE];c['inside']=True
            if address==self.stop:c['returned']=True;uc.emu_stop()
            elif c.get('inside') and address!=c['entry']:
                # Original bodies stay inside themselves; compiled C must stay
                # inside the appended payload until it returns.
                if self.payload is None:assert c['entry']<address<c['entry']+16,hex(address)
                else:assert self.payload[0]<=address<self.payload[1],hex(address)
        def read(uc,access,address,size,value,data):
            if self.current.get('inside'):self.current['reads'].append((address,size))
        def write(uc,access,address,size,value,data):
            if self.current.get('inside'):self.current['writes'].append((address,size,value&((1<<(8*size))-1)))
        self.machine.hook_add(UC_HOOK_CODE,code);self.machine.hook_add(UC_HOOK_MEM_READ,read)
        self.machine.hook_add(UC_HOOK_MEM_WRITE,write)

    def run(self,entry,pattern,flags,depth,case_index):
        m=self.machine;sp=STACK+0x8000;kind=entry['kind']
        self.current=dict(entry=int(entry['address'],16),reads=[],writes=[],returned=False,float_before=None,inside=False)
        argument=struct.unpack('<I',pattern[4:])[0]^case_index
        m.mem_write(sp,struct.pack('<17I',self.stop,argument,*[(0x12340000+i*0x101+case_index)&0xffffffff for i in range(15)]))
        before={reg:(0x11223344+n*0x1010101+case_index)&0xffffffff for n,reg in enumerate(REGISTERS)}
        object_address=ARENA+0x4000+(case_index%4)
        before[UC_X86_REG_ECX]=object_address
        for reg,value in before.items():m.reg_write(reg,value)
        if kind=='global_word':field=int(entry['global_address'],16)
        else:field=object_address+entry['field_offset']
        saved=bytes(m.mem_read(field,8));m.mem_write(field,pattern)
        m.reg_write(UC_X86_REG_EAX,0xabcdef01);m.reg_write(UC_X86_REG_ESP,sp)
        m.reg_write(UC_X86_REG_EFLAGS,flags);m.reg_write(UC_X86_REG_FPCW,0x37f)
        m.reg_write(UC_X86_REG_FPSW,0);m.reg_write(UC_X86_REG_FPTAG,0xffff)
        preload=b''.join(b'\xd9\x05'+struct.pack('<I',self.seed+4*i) for i in range(depth))
        preload+=b'\xe9'+struct.pack('<I',(self.current['entry']-self.start-len(preload)-5)&0xffffffff)
        m.mem_write(self.start,preload);m.ctl_remove_cache(self.start,self.start+32)
        m.emu_start(self.start,0,timeout=1_000_000,count=100)
        after=bytes(m.mem_read(field,8));m.mem_write(field,saved)
        assert self.current['returned']
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+CLEANUP.get(kind,0)
        for reg,value in before.items():assert m.reg_read(reg)==value,(kind,reg)
        assert m.reg_read(UC_X86_REG_EFLAGS)==flags
        float_after=[m.reg_read(reg) for reg in FLOAT_STATE]
        if kind not in ('float_getter','double_getter'):assert float_after==self.current['float_before']
        if kind not in STORE:assert not self.current['writes']
        eax=m.reg_read(UC_X86_REG_EAX)
        expected={'global_word':struct.unpack('<I',pattern[:4])[0],'field_address':field,
                  'word_setter':argument}.get(kind)
        if expected is not None:assert eax==expected,(kind,hex(eax),hex(expected))
        if kind=='word_setter':assert after[:4]==struct.pack('<I',argument) and after[4:]==pattern[4:]
        if kind=='byte_set_one':assert after==b'\x01'+pattern[1:]
        return dict(eax=f'{eax:08x}',flags=flags,stack_cleanup=CLEANUP.get(kind,0),
            float_control=float_after[0],float_status=float_after[1],float_tag=float_after[2],
            st0=str(float_after[3]),field_after=after.hex(),
            reads=[(a-field if field<=a<field+8 else ('stack',a-sp) if sp<=a<sp+68 else hex(a),s) for a,s in self.current['reads']],
            writes=[(a-field,s,v) for a,s,v in self.current['writes']])

def compare_field_accessors(original,original_pe,edited,edited_pe,spec,symbols):
    target='client' if spec['file']=='BfVietnam.exe' else 'server'
    catalog=json.loads((PROJECT/'field-accessors.json').read_text())['targets'][target]
    assert catalog['original_sha256']==spec['sha']
    section=next(s for s in edited_pe.sections if s.Name.rstrip(b'\0')==b'.bfvmod')
    payload=(0x400000+section.VirtualAddress,0x400000+section.VirtualAddress+section.Misc_VirtualSize)
    old,new=Runner(original,original_pe),Runner(edited,edited_pe,payload);results=[]
    for entry in catalog['entries']:
        address=int(entry['address'],16);body=bytes.fromhex(entry['native_body'])
        jump=edited_pe.get_data(address-0x400000,5)
        assert jump[0]==0xe9 and address+5+struct.unpack('<i',jump[1:])[0]==symbols[entry['symbol']]
        assert original_pe.get_data(address-0x400000,len(body))==body
        assert edited_pe.get_data(symbols[entry['symbol']]-0x400000,len(body))==body,entry['symbol']
        for pattern_index,pattern in enumerate(PATTERNS):
            for index,(flags,depth) in enumerate(SCENARIOS):
                case_index=pattern_index*len(SCENARIOS)+index
                try:
                    a=old.run(entry,pattern,flags,depth,case_index);b=new.run(entry,pattern,flags,depth,case_index)
                    assert a==b,(entry,a,b)
                except Exception as error:raise RuntimeError(f'accessor {entry["address"]} ({entry["kind"]}), pattern {pattern.hex()}, flags {flags:x}, x87 depth {depth}') from error
                results.append(dict(address=entry['address'],kind=entry['kind'],retained_depth=depth,
                    object_alignment=case_index%4,eax=a['eax'],field_after=a['field_after']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_field_accessors(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (PROJECT/'build'/target/'field-accessor-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{target}: {len(cases)} field/global accessor comparisons passed',flush=True)
