"""Original/reconstructed AI task scheduler routines.

Execute the native Task::sleep, task-tree removal, scheduler add/remove and
slice time-left routines and their src/task_scheduler.c replacements on the
same random task tree. getTimeSeconds is replaced in both images by a stub
that loads a chosen double. Each case starts from FNINIT plus a chosen
control word; the result covers EAX (not for Task::sleep, which natively
leaves the FPU status in AX and the caller's upper bits), the time-left
result as an 80-bit value, the whole arena (scheduler and tasks), ESP, the
callee-saved registers, the FPU control and tag words and the FPU status apart
from its condition codes (the native compare leaves C0-C3 that no caller reads).
"""
import argparse
import hashlib
import json
import random
import struct
from pathlib import Path
import pefile
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,UC_X86_REG_EIP,
    UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG)
from build import PROJECT,GAME,TARGETS
from verify_aimer import load_machine,ARENA,STACK,SAVED,CONTROL_WORDS

SCHEDULER=ARENA;TIME=ARENA+0x80;RESULT=ARENA+0x90;CONTROL=ARENA+0xa0
TASKS=ARENA+0x400;TASK_SIZE=0x40;TASK_COUNT=40;SNAPSHOT=TASKS+TASK_SIZE*TASK_COUNT-ARENA
STORE=ARENA+0x3000;TRAMPOLINE=ARENA+0x3010;PRELOAD=ARENA+0x3100
# name: stack argument bytes
ENTRIES=dict(task_sleep=8,task_tree_remove=12,scheduler_time_left=0,scheduler_add=16,scheduler_remove=4)

def d(x):return struct.unpack('<Q',struct.pack('<d',x))[0]
SPECIAL=[d(0.0),d(-0.0),d(-1.0),d(1.0),d(1e308),d(-1e308),d(float('inf')),d(float('-inf')),
         0x7ff8000000000000,0xfff8000000012345,0x7ff0000000000001,0x7ff4000000000000,1,0x800fffffffffffff,
         d(1.7976931348623157e308),d(2.2250738585072014e-308)]

def double(rng):
    r=rng.random()
    if r<0.25:return rng.choice(SPECIAL)
    if r<0.35:return rng.getrandbits(64)
    return d(rng.choice([rng.uniform(0,1000),rng.uniform(-5,5),rng.uniform(0,1e6)/3,float(rng.randrange(0,100))]))
def delay(rng):return rng.choice([d(-1.0),d(-1.0),double(rng),d(rng.uniform(0,10)),d(-1.0000000000000002)])
def task(i):return TASKS+TASK_SIZE*i

def tree(rng):
    """Random tasks: a tree under task 0 (child lists in random order), some
    detached tasks, random sequels anywhere and random doubles and flags."""
    words=[[rng.getrandbits(32) for _ in range(TASK_SIZE//4)] for _ in range(TASK_COUNT)]
    size=rng.choice([1,2,4,12,TASK_COUNT])
    parent={i:rng.randrange(0,i) for i in range(1,size) if rng.random()<0.9}
    children={i:[] for i in range(TASK_COUNT)}
    for child,p in parent.items():children[p].append(child)
    for i,w in enumerate(words):
        rng.shuffle(children[i])
        w[0x20//4]=task(children[i][0]) if children[i] else 0
        w[0x08//4:0x10//4]=struct.unpack('<2I',struct.pack('<Q',double(rng)))
        w[0x28//4:0x30//4]=struct.unpack('<2I',struct.pack('<Q',double(rng)))
        w[0x18//4]=task(rng.randrange(TASK_COUNT)) if rng.random()<0.5 else 0
    for i in range(TASK_COUNT):
        for a,b in zip(children[i],children[i][1:]):words[a][0x1c//4]=task(b)
        if children[i]:words[children[i][-1]][0x1c//4]=0
    for i in range(TASK_COUNT):
        if i not in parent:words[i][0x1c//4]=rng.choice([0,0,task(rng.randrange(TASK_COUNT))]) if i else 0
    # A detached task's next link may point into the tree; the child lists stay acyclic.
    for i in range(TASK_COUNT):
        if i not in parent and i and words[i][0x1c//4]:
            j=(words[i][0x1c//4]-TASKS)//TASK_SIZE
            if j==i or j not in parent:words[i][0x1c//4]=0
    return words,[i for i in range(size) if i==0 or i in parent]

def scheduler_cases(count=3000):
    rng=random.Random(0x7a5c);cases=[]
    for index in range(count):
        name=list(ENTRIES)[index%len(ENTRIES)]
        words,members=tree(rng)
        sched=[rng.getrandbits(32) for _ in range(8)]
        sched[0]=task(0)
        sched[2:4]=struct.unpack('<2I',struct.pack('<Q',double(rng)))
        sched[4:6]=struct.unpack('<2I',struct.pack('<Q',double(rng)))
        sched[6]=task(rng.choice(members+[rng.randrange(TASK_COUNT)]))
        sched[7]=rng.choice([0,0,task(rng.randrange(TASK_COUNT)),task(rng.choice(members))])
        target=rng.choice(members+members+[rng.randrange(TASK_COUNT)])
        flag=rng.choice([0,0,0,1,0x100,0xff,0xffffff00])
        if name=='task_sleep':ecx=task(target);args=list(struct.unpack('<2I',struct.pack('<Q',delay(rng))))
        elif name=='task_tree_remove':
            ecx=SCHEDULER;root=rng.choice([0,0,0]+members);args=[task(root),task(target),flag]
        elif name=='scheduler_add':
            ecx=SCHEDULER;fresh=rng.random()<0.7
            if fresh:target=rng.randrange(TASK_COUNT)
            args=[task(target),*struct.unpack('<2I',struct.pack('<Q',delay(rng))),rng.choice([0,0,1,0x100,0xff])]
        elif name=='scheduler_remove':ecx=SCHEDULER;args=[task(target)]
        else:ecx=SCHEDULER;args=[]
        cases.append(dict(name=name,tasks=words,scheduler=sched,ecx=ecx,args=args,
            time=double(rng),cw=rng.choice(CONTROL_WORDS)))
    return cases

class Runner:
    def __init__(self,image,pe,spec):
        m=self.machine=load_machine(image,pe);self.spec=spec
        # getTimeSeconds: mov eax, 13572468h; fld qword [TIME]; ret. The native
        # one also overwrites EAX, so EAX after time-left is this value.
        m.mem_write(spec['get_time_seconds'],b'\xb8'+struct.pack('<I',0x13572468)+b'\xdd\x05'+struct.pack('<I',TIME)+b'\xc3')
        m.mem_write(STORE,b'\xdb\x3d'+struct.pack('<I',RESULT)+b'\xe9'+struct.pack('<I',(TRAMPOLINE-STORE-11)&0xffffffff))
        m.mem_write(TRAMPOLINE,b'\x90')
        self.preload={}
        for index,name in enumerate(ENTRIES):
            stub=PRELOAD+0x10*index;code=b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)
            code+=b'\xe9'+struct.pack('<I',(spec[name]-stub-len(code)-5)&0xffffffff)
            m.mem_write(stub,code);self.preload[name]=stub
    def run(self,case):
        m=self.machine
        def w32(p,*v):m.mem_write(p,struct.pack('<%dI'%len(v),*[x&0xffffffff for x in v]))
        m.mem_write(ARENA,bytes(SNAPSHOT))
        w32(SCHEDULER,*case['scheduler'])
        for i,words in enumerate(case['tasks']):w32(task(i),*words)
        m.mem_write(TIME,struct.pack('<Q',case['time']));m.mem_write(RESULT,b'\xee'*10)
        sp=STACK+0x8000;back=STORE if case['name']=='scheduler_time_left' else TRAMPOLINE
        w32(sp,back,*case['args'])
        cw=case['cw'];m.mem_write(CONTROL,struct.pack('<H',cw))
        for reg,value in SAVED.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0x5a5a5a5a);m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,case['ecx'])
        m.emu_start(self.preload[case['name']],TRAMPOLINE,timeout=10_000_000,count=1_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+ENTRIES[case['name']]
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        arena=bytes(m.mem_read(ARENA,SNAPSHOT))
        return dict(eax=None if case['name']=='task_sleep' else m.reg_read(UC_X86_REG_EAX),
                    status=m.reg_read(UC_X86_REG_FPSW)&~0x4700,result=arena[RESULT-ARENA:RESULT-ARENA+10].hex(),
                    arena=hashlib.sha256(arena).hexdigest(),scheduler=arena[:0x20].hex())

def compare_task_scheduler(original,original_pe,edited,edited_pe,spec,symbols,count=3000):
    for name in ENTRIES:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name]
    old_runner=Runner(original,original_pe,spec);new_runner=Runner(edited,edited_pe,spec)
    results=[]
    for case in scheduler_cases(count):
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new)
        except Exception as error:raise RuntimeError(f'task scheduler inputs {case}') from error
        results.append(dict(inputs=case,**old))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both')
    p.add_argument('--count',type=int,default=3000)
    p.add_argument('--work',help='build work directory name, e.g. {target}-custom for --output builds');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];work=PROJECT/'build'/(args.work.replace('{target}',target) if args.work else target)
        manifest=json.loads((work/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_task_scheduler(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.count)
        summary={}
        for c in cases:
            s=summary.setdefault(c['inputs']['name'],dict(cases=0,al_set=0))
            s['cases']+=1;s['al_set']+=c['eax'] is not None and c['eax']&0xff!=0
        (work/'task-scheduler-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),summary=summary,cases=cases),indent=2))
        print(f'{target}: {len(cases)} task scheduler comparisons passed {summary}',flush=True)
