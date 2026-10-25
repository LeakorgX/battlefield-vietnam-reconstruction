"""Original/reconstructed Aimer tracking, prediction and ballistic solve.

Execute the four native Aimer methods and their src/aimer.c replacements on the
same controlled object, argument and trace-ring memory. The solver runs the real
retained _ftol2 and _finite runtime. Each case starts from FNINIT plus a chosen
control word; the result covers AL, FPU status on return, object and output
memory, the trace ring, ESP and the callee-saved registers.
"""
import argparse
import hashlib
import itertools
import json
import random
import struct
from pathlib import Path
import pefile
from unicorn import Uc,UC_ARCH_X86,UC_MODE_32
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_EIP,
    UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG,UC_X86_REG_GDTR,UC_X86_REG_SS,
    UC_X86_REG_FS,UC_X86_REG_CR4)
from build import PROJECT,GAME,TARGETS

# Above the image: a rebuilt executable with the lifted payload reaches past
# native_oracle's arena at 0x02000000.
ARENA=0x70000000;STACK=0x70100000;TEB=0x70200000;GDT=TEB+0x1000
FS_SELECTOR=3*8|3;SS_SELECTOR=4*8

def load_machine(image,pe):
    machine=Uc(UC_ARCH_X86,UC_MODE_32);base=pe.OPTIONAL_HEADER.ImageBase
    assert base+pe.OPTIONAL_HEADER.SizeOfImage<=ARENA
    machine.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
    machine.mem_write(base,image[:pe.OPTIONAL_HEADER.SizeOfHeaders])
    for section in pe.sections:machine.mem_write(base+section.VirtualAddress,section.get_data())
    machine.mem_map(ARENA,0x10000);machine.mem_map(STACK,0x10000)
    # Lifted runtime entries read the TEB stack bounds through fs, as in test_mods.
    machine.mem_map(TEB,0x2000)
    machine.mem_write(TEB,struct.pack('<III',0xffffffff,STACK+0x10000,STACK)+bytes(12)+struct.pack('<I',TEB))
    machine.mem_write(GDT+(FS_SELECTOR&~7),struct.pack('<HHBBBB',0xfff,TEB&0xffff,(TEB>>16)&0xff,0xf3,0x40,TEB>>24))
    machine.mem_write(GDT+SS_SELECTOR,struct.pack('<HHBBBB',0xffff,0,0,0x93,0xcf,0))
    machine.reg_write(UC_X86_REG_GDTR,(0,GDT,0x1000,0))
    machine.reg_write(UC_X86_REG_SS,SS_SELECTOR);machine.reg_write(UC_X86_REG_FS,FS_SELECTOR)
    machine.reg_write(UC_X86_REG_CR4,machine.reg_read(UC_X86_REG_CR4)|0x600)
    return machine

THIS=ARENA;BUFFER_A=ARENA+0x100;BUFFER_B=ARENA+0x140;BUFFER_C=ARENA+0x180;AIM_TIME=ARENA+0x1c0
SNAPSHOT=0x200;CONTROL=ARENA+0x2f0;TRAMPOLINE=ARENA+0x300;PRELOAD=ARENA+0x340
POINTERS=dict(this=THIS,a=BUFFER_A,b=BUFFER_B,c=BUFFER_C,velocity=THIS+0x24,position=THIS+0x18,null=0)
ARGUMENT_BYTES=dict(aimer_track=12,aimer_predict=8,aimer_projectile_offset=12,aimer_solve=24)
SAVED={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:0x23456789,UC_X86_REG_EDI:0x3456789a,UC_X86_REG_EBP:0x456789ab}

def f(x):return struct.unpack('<I',struct.pack('<f',x))[0]
SPECIAL=[0,0x80000000,1,0x7f800000,0xff800000,0x7fc12345,0x7f812345,0xffc00000,f(1e30),f(-1e30)]
CONTROL_WORDS=[0x7f|pc|rc for pc in [0,0x200,0x300] for rc in [0,0x400,0x800,0xc00]]

def aimer_object(rng,drag=None,samples=None,start=None,last=None):
    words=[f(rng.uniform(-200,200)) for _ in range(32)]
    words[0]=f(rng.choice([0.0,0.0,0.02,0.1,0.5,-0.05])) if drag is None else drag
    words[1]=rng.getrandbits(32);words[2]=f(rng.choice([9.81,-9.81,4.9,0.0,20.0]))
    words[3]=f(rng.choice([30.0,80.0,150.0,300.0,700.0]))
    words[4]=f(rng.uniform(0,20)) if start is None else start
    words[5]=f(rng.uniform(0,20)) if last is None else last
    words[0x1e]=rng.choice([0,1,2,0xffffffff]) if samples is None else samples
    return words

def vec(rng,scale=200):return [f(rng.uniform(-scale,scale)) for _ in range(3)]

def aimer_cases():
    rng=random.Random(0xa1e5);cases=[]
    # Tracking: sample gate, time ordering, NaN times and self-aliasing inputs.
    times=[f(1.0),f(1.0001),f(0.999),f(5.0)]+SPECIAL
    for samples,time,source in itertools.product([0,1,7],times,['a','position','velocity']):
        cases.append(dict(name='aimer_track',object=aimer_object(rng,samples=samples,last=f(1.0)),
            a=vec(rng),b=vec(rng),args=[source,'b' if source=='a' else 'a',time]))
    # Prediction: elapsed-time extrapolation, special times, output aliasing the object.
    for time,out in itertools.product([f(0.0),f(2.5),f(-3.0),f(1e6)]+SPECIAL,['a','this','position']):
        cases.append(dict(name='aimer_predict',object=aimer_object(rng),args=[time,out]))
    for special in SPECIAL:
        o=aimer_object(rng);o[4]=special;o[7]=special
        cases.append(dict(name='aimer_predict',object=o,args=[f(1.5),'a']))
    # Projectile offset: zero, positive, negative and special drag; aliasing outputs.
    for drag,time,out in itertools.product([0,0x80000000,f(0.02),f(0.3),f(-0.1),f(1.0),0x7fc12345,0x7f800000],
            [f(0.0),f(0.5),f(3.0),0x7fc12345,0x7f800000],['a','b','this']):
        cases.append(dict(name='aimer_projectile_offset',object=aimer_object(rng,drag=drag),
            a=vec(rng,1),b=[f(0.6),f(0.0),f(0.8)],args=[out,'b',time]))
    # Solve: reachable and unreachable targets, drag, high arc, search widths and null output.
    for index in range(1200):
        o=aimer_object(rng);o[0x1e]=1
        o[6:9]=[f(rng.uniform(-400,400)),f(rng.uniform(-30,60)),f(rng.uniform(-400,400))]
        o[9:12]=[f(rng.uniform(-15,15)),f(rng.uniform(-2,2)),f(rng.uniform(-15,15))]
        scale=rng.choice([0x80000000,f(0.0),f(0.1),f(0.5),f(1.0),f(2.0),f(4.0),f(-0.5)])
        args=[f(rng.uniform(0,25)),scale,rng.choice([0,1,0x100,0xff]),'b','c',rng.choice(['aim','null'])]
        if index%10==0:o[rng.choice([0,2,3,6,7,8,9])]=rng.choice(SPECIAL)
        if index%17==0:args[0]=rng.choice(SPECIAL)
        cases.append(dict(name='aimer_solve',object=o,args=args,cw=rng.choice(CONTROL_WORDS)))
    for case in cases[:]:
        if case['name']!='aimer_solve' and rng.random()<0.25:
            cases.append(dict(case,cw=rng.choice(CONTROL_WORDS)))
    for index,case in enumerate(cases):case.setdefault('trace_index',index&15)
    return cases

class Runner:
    def __init__(self,image,pe,spec):
        self.machine=load_machine(image,pe);self.spec=spec
        self.ring=spec['trace_lines'];self.ring_size=spec['trace_index']+4-self.ring
        self.machine.mem_write(TRAMPOLINE,b'\x90')
        # One fixed stub per entry: unicorn keeps translated blocks across runs,
        # so rewriting a single stub would keep jumping to the previous entry.
        self.preload={}
        for index,name in enumerate(ARGUMENT_BYTES):
            stub=PRELOAD+0x10*index;code=b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)
            code+=b'\xe9'+struct.pack('<I',(spec[name]-stub-len(code)-5)&0xffffffff)
            self.machine.mem_write(stub,code);self.preload[name]=stub
        self.ring_reset=bytes(self.machine.mem_read(self.ring,self.ring_size))
    def run(self,case):
        m=self.machine;spec=self.spec
        def w32(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
        m.mem_write(ARENA,bytes(SNAPSHOT));m.mem_write(THIS,struct.pack('<32I',*case['object']))
        for key,address in [('a',BUFFER_A),('b',BUFFER_B),('c',BUFFER_C)]:
            if key in case:m.mem_write(address,struct.pack('<3I',*case[key]))
        w32(AIM_TIME,0xdeadbeef)
        m.mem_write(self.ring,self.ring_reset);w32(spec['trace_index'],case['trace_index'])
        argument=[POINTERS.get(x,AIM_TIME if x=='aim' else None) if isinstance(x,str) else x for x in case['args']]
        sp=STACK+0x8000;m.mem_write(sp,struct.pack('<%dI'%(1+len(argument)),TRAMPOLINE,*argument))
        cw=case.get('cw',0x37f);m.mem_write(CONTROL,struct.pack('<H',cw))
        for reg,value in SAVED.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0x5a5a5a5a);m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,THIS)
        m.emu_start(self.preload[case['name']],TRAMPOLINE,timeout=10_000_000,count=5_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+ARGUMENT_BYTES[case['name']]
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        result=dict(status=m.reg_read(UC_X86_REG_FPSW),memory=bytes(m.mem_read(ARENA,SNAPSHOT)).hex(),
                    trace=bytes(m.mem_read(self.ring,self.ring_size)).hex())
        if case['name'] in ['aimer_track','aimer_solve']:result['accepted']=m.reg_read(UC_X86_REG_EAX)&0xff
        return result

def compare_aimer(original,original_pe,edited,edited_pe,spec,symbols,native=()):
    # native: entries the build kept native (verify.py); their cases are left out
    for name in ARGUMENT_BYTES:
        if name in native:continue
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name]
    old_runner=Runner(original,original_pe,spec);new_runner=Runner(edited,edited_pe,spec)
    results=[]
    for case in aimer_cases():
        if case['name'] in native:continue
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new)
        except Exception as error:raise RuntimeError(f'aimer inputs {case}') from error
        results.append(dict(inputs=case,**old))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both')
    p.add_argument('--work',help='build work directory name, e.g. client-custom for --output builds');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];work=PROJECT/'build'/(args.work.replace('{target}',target) if args.work else target)
        manifest=json.loads((work/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_aimer(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        summary={name:[sum(1 for c in cases if c['inputs']['name']==name),
                       sum(1 for c in cases if c['inputs']['name']==name and c.get('accepted')==1)] for name in ARGUMENT_BYTES}
        (work/'aimer-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),summary=summary,cases=cases),indent=2))
        print(f'{target}: {len(cases)} Aimer comparisons passed {summary}',flush=True)
