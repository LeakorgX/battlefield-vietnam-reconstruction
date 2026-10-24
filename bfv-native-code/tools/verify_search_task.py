"""Original/reconstructed TaskAStarLocalSearch::execute and
TaskAStarStrategicSearch::execute.

Execute the native methods and their src/task_scheduler.c replacements on the
same random search task. The scheduler's time-left routine is replaced in both
images by code that loads a chosen double; Task::sleep, the searcher's step and
its path copy are Python stubs that log their arguments (the step also logs the
time left it is handed), store a chosen "still running" byte and return chosen
EAX values. Some steps rewrite the task's result pointer and path fields, its
in-progress byte or the current-task slot, to check the rereads after the call.
Each case starts from FNINIT plus a chosen control word and compares EAX, the
stub log, the arena (task and result words), both current-task slots, ESP,
callee-saved registers and the FPU control, tag and status words (apart from
C0-C3).
"""
import argparse
import hashlib
import json
import random
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP,UC_X86_REG_EIP,
    UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG)
from build import PROJECT,GAME,TARGETS
from verify_aimer import load_machine,ARENA,STACK,SAVED,CONTROL_WORDS

TASK=ARENA;OTHER=ARENA+0x80;RESULT=ARENA+0x100;RESULT2=ARENA+0x140;TIME=ARENA+0x180;CONTROL=ARENA+0x190
SNAPSHOT=0x200;TRAMPOLINE=ARENA+0x9000;PRELOAD=ARENA+0x9100
# name: stack argument bytes
ENTRIES=dict(task_local_search_execute=0,task_strategic_search_execute=0)
SLOTS=dict(task_local_search_execute='local_search_current',task_strategic_search_execute='strategic_search_current')

def d(x):return struct.unpack('<Q',struct.pack('<d',x))[0]
TIMES=[d(0.0),d(-0.0),d(0.005),d(-0.25),d(1e308),d(float('inf')),d(float('-inf')),0x7ff8000000000000,
       0x7ff4000000000000,0xfff0000000000001,1,0x800fffffffffffff]

def search_cases(count):
    rng=random.Random(0x5ea7c4);cases=[]
    for index in range(count):
        name=list(ENTRIES)[index%len(ENTRIES)]
        words=[rng.getrandbits(32) for _ in range(0x20)]
        words[0x40//4]=rng.choice([RESULT,RESULT,RESULT2])
        words[0x4c//4]=rng.choice([0,1,2,7,0x7fffffff,-1,-5,-0x80000000,rng.getrandbits(32)])&0xffffffff
        eax=[rng.getrandbits(32)&rng.choice([0xffffff00,0xffffffff,0xffffffff]) for _ in range(4)]
        eax[1]=rng.choice([eax[1],0,1,-1&0xffffffff,0x80000000,rng.randrange(-4,4)&0xffffffff])
        time=rng.choice(TIMES) if rng.random()<0.3 else d(rng.choice([rng.uniform(-1,1),rng.uniform(0,0.05),rng.uniform(0,1e4)]))
        cases.append(dict(name=name,words=words,slot=rng.choice([0,0,TASK,TASK,OTHER]),
            other_slot=rng.choice([0,TASK,OTHER]),running=rng.choice([0,0,1,2,0xff]),eax=eax,time=time,
            perturb=rng.choice([0,0,0,1,2,3]),stub_words=[rng.getrandbits(32) for _ in range(4)],
            cw=rng.choice(CONTROL_WORDS)))
    return cases

class Runner:
    def __init__(self,image,pe,spec):
        m=self.machine=load_machine(image,pe);self.spec=spec
        # Scheduler time left: mov eax, 24681357h; fld qword [TIME]; ret (logged by a hook).
        m.mem_write(spec['scheduler_time_left'],b'\xb8'+struct.pack('<I',0x24681357)+b'\xdd\x05'+struct.pack('<I',TIME)+b'\xc3')
        m.hook_add(UC_HOOK_CODE,lambda m,a,s,u:self.log.append(('time_left',m.reg_read(UC_X86_REG_ECX))),
                   begin=spec['scheduler_time_left'],end=spec['scheduler_time_left'])
        m.mem_write(TRAMPOLINE,b'\x90')
        self.preload={}
        for index,name in enumerate(ENTRIES):
            stub=PRELOAD+0x10*index;code=b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)
            code+=b'\xe9'+struct.pack('<I',(spec[name]-stub-len(code)-5)&0xffffffff)
            m.mem_write(stub,code);self.preload[name]=stub
        stubs={spec['task_sleep']:(self.sleep,8),spec['searcher_step']:(self.step,0x10),spec['searcher_path']:(self.path,0xc)}
        for address,(handler,pop) in stubs.items():
            m.hook_add(UC_HOOK_CODE,self.hook(handler,pop),begin=address,end=address)
    def r32(self,p,n=1):return list(struct.unpack('<%dI'%n,self.machine.mem_read(p,4*n)))
    def w32(self,p,*v):self.machine.mem_write(p,struct.pack('<%dI'%len(v),*[x&0xffffffff for x in v]))
    def hook(self,handler,pop):
        def run(m,address,size,user):
            sp=m.reg_read(UC_X86_REG_ESP);back,*args=self.r32(sp,5)
            eax=handler(m.reg_read(UC_X86_REG_ECX),args)
            m.reg_write(UC_X86_REG_EAX,eax);m.reg_write(UC_X86_REG_ECX,0xc1c1c1c1);m.reg_write(UC_X86_REG_EDX,0xd2d2d2d2)
            m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,back)
        return run
    def stack_pointer(self,p):return 'stack' if STACK<=p<STACK+0x10000 else p
    def sleep(self,ecx,args):
        self.log.append(('sleep',ecx,args[0],args[1]));return self.case['eax'][0]
    def step(self,ecx,args):
        case=self.case;search,in_progress,running,time=args[:4]
        self.log.append(('step',ecx,search,in_progress,self.stack_pointer(running),self.stack_pointer(time),
                         bytes(self.machine.mem_read(time,8)).hex()))
        self.machine.mem_write(running,bytes([case['running']]))
        words=case['stub_words']
        if case['perturb']==1:self.w32(TASK+0x40,RESULT2 if self.r32(TASK+0x40)[0]!=RESULT2 else RESULT);self.w32(TASK+0x44,*words[:3])
        elif case['perturb']==2:self.w32(self.spec[SLOTS[case['name']]],OTHER)
        elif case['perturb']==3:self.machine.mem_write(TASK+0x38,bytes([words[0]&0xff]))
        return case['eax'][1]
    def path(self,ecx,args):
        self.log.append(('path',ecx,*args[:3]));return self.case['eax'][2]
    def run(self,case):
        m=self.machine;self.case=case;self.log=[]
        m.mem_write(ARENA,bytes(SNAPSHOT))
        self.w32(TASK,*case['words'])
        m.mem_write(RESULT,bytes([0xee])*0x80)
        m.mem_write(TIME,struct.pack('<Q',case['time']))
        mine=self.spec[SLOTS[case['name']]];slots=[self.spec['local_search_current'],self.spec['strategic_search_current']]
        for slot in slots:self.w32(slot,case['slot'] if slot==mine else case['other_slot'])
        sp=STACK+0x8000
        self.w32(sp,TRAMPOLINE)
        cw=case['cw'];m.mem_write(CONTROL,struct.pack('<H',cw))
        for reg,value in SAVED.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0x5a5a5a5a);m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,TASK)
        m.emu_start(self.preload[case['name']],TRAMPOLINE,timeout=10_000_000,count=1_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+ENTRIES[case['name']]
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        arena=bytes(m.mem_read(ARENA,SNAPSHOT))
        return dict(eax=m.reg_read(UC_X86_REG_EAX),status=m.reg_read(UC_X86_REG_FPSW)&~0x4700,log=self.log,
                    slots=[self.r32(slot)[0] for slot in slots],arena=hashlib.sha256(arena).hexdigest(),
                    task=arena[:0x50].hex(),result=arena[RESULT-ARENA:RESULT-ARENA+4].hex(),
                    result2=arena[RESULT2-ARENA:RESULT2-ARENA+4].hex())

def compare_search_task(original,original_pe,edited,edited_pe,spec,symbols,count=2000):
    for name in ENTRIES:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name]
    old_runner=Runner(original,original_pe,spec);new_runner=Runner(edited,edited_pe,spec)
    results=[]
    for case in search_cases(count):
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new)
        except Exception as error:raise RuntimeError(f'search task inputs {case}') from error
        results.append(dict(inputs={k:v for k,v in case.items() if k!='words'},**old))
    return results

def outcome(c):
    kinds=[entry[0] for entry in c['log']]
    if 'step' not in kinds:return 'slot_taken'
    if 'sleep' in kinds:return 'still_running'
    if 'path' in kinds:return 'path'
    return 'failed' if c['inputs']['eax'][1]&0xff==0 else 'no_points'

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both')
    p.add_argument('--count',type=int,default=2000)
    p.add_argument('--work',help='build work directory name, e.g. {target}-custom for --output builds');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];work=PROJECT/'build'/(args.work.replace('{target}',target) if args.work else target)
        manifest=json.loads((work/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_search_task(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.count)
        summary={}
        for c in cases:
            s=summary.setdefault(c['inputs']['name'],{})
            s[outcome(c)]=s.get(outcome(c),0)+1
        (work/'search-task-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),summary=summary,cases=cases),indent=2))
        print(f'{target}: {len(cases)} search task comparisons passed {summary}',flush=True)
