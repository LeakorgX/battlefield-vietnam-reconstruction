"""Original/reconstructed Task::vf3 (unschedule), Task queue,
TaskPlayerMapSearch::execute and TaskStrategicPlayerMapSearch::execute,
TaskStrategicSearch's run, TaskGetStrategicCellInfoNo::execute and the
strategic cell distance and the cell query and strategic player map search
inits (run directly, with their stub hooks stepping aside; the inits must
also return with ECX unchanged, as their native callers rely on it).

Execute the native routines and their src/task_scheduler.c replacements on the
same random task. Scheduler add and remove and the AStarLocalSearch prepare
and finish calls are Python stubs that log their arguments (the delay as the
bits handed to add) and return chosen EAX values; some prepare calls rewrite
the task's result, path and point fields to check that they are read after the
call. The player map executes run the native or rebuilt queue routine between
them and the add stub. Each case starts from FNINIT plus a chosen control word
and compares EAX, the stub log, the arena, ESP, callee-saved registers and the
FPU control, tag and status words (apart from C0-C3).
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

TASK=ARENA;CHILD=ARENA+0x400;SCHEDULER=ARENA+0x500;RESULT=ARENA+0x540;CONTROL=ARENA+0x5f0
MAP=ARENA+0x600;GRID=ARENA+0x680;CELLS=ARENA+0x700
SNAPSHOT=0xb00;TRAMPOLINE=ARENA+0x9000;PRELOAD=ARENA+0x9100
# name: stack argument bytes
ENTRIES=dict(task_unschedule=0,task_queue=16,task_player_map_execute=0,task_strategic_player_map_execute=0,
             task_strategic_search_run=0,task_cell_query_execute=0,strategic_cell_distance=4,
             cell_query_init=0x10,player_map_search_init=0x24)
QUEUE_ENTRIES=list(ENTRIES)[:4]

def d(x):return struct.unpack('<Q',struct.pack('<d',x))[0]
DELAYS=[d(0.0),d(-0.0),d(-1.0),d(1.5),d(float('inf')),0x7ff8000000000000,0x7ff4000000000000,0xfff0000000000001,
        1,0x800fffffffffffff,d(1e308)]

def queue_cases(count):
    rng=random.Random(0x7a5c9e);cases=[]
    for index in range(count):
        name=QUEUE_ENTRIES[index%len(QUEUE_ENTRIES)]
        words=[rng.getrandbits(32) for _ in range(0x200//4)]
        words[0x14//4]=rng.choice([0,SCHEDULER,SCHEDULER,SCHEDULER+0x10])
        words[0x16c//4]=rng.choice([RESULT,RESULT+8,rng.getrandbits(32)])
        args=[]
        if name=='task_queue':
            delay=rng.choice(DELAYS) if rng.random()<0.4 else d(rng.uniform(-10,10))
            args=[rng.choice([CHILD,TASK+0x58,rng.getrandbits(32)]),delay&0xffffffff,delay>>32,
                  rng.choice([0,1,0x100,0xff,rng.getrandbits(32)])]
        cases.append(dict(name=name,words=words,args=args,eax=[rng.getrandbits(32) for _ in range(4)],
            perturb=rng.choice([0,0,1,2]),stub_words=[rng.getrandbits(32) for _ in range(5)],
            cw=rng.choice(CONTROL_WORDS)))
    return cases

def f(x):return struct.unpack('<I',struct.pack('<f',x))[0]
REACHES=[f(64.0),f(64.0)+1,f(64.0)-1,f(0.0),0x80000000,f(-64.0),0x7f800000,0xff800000,0x7fc00000,0x7fa00000,
         0xffc00001,1,0x80000001]

def run_cases(count):
    rng=random.Random(0x5e2a17);cases=[]
    for index in range(count):
        words=[rng.getrandbits(32) for _ in range(0x308//4)]
        words[0x14//4]=SCHEDULER
        words[0x2ec//4]=rng.choice([-1&0xffffffff,-1&0xffffffff,0,rng.getrandbits(32)])
        words[0x2f0//4]=rng.choice([-1&0xffffffff,-1&0xffffffff,0,rng.getrandbits(32)])
        words[0x2f4//4]=rng.choice([RESULT,RESULT+8])
        words[0x300//4]=rng.choice([0,1,1,2,2,3,0xffffffff,rng.getrandbits(32)])
        words[0x304//4]=rng.choice(REACHES) if rng.random()<0.5 else f(rng.uniform(0,128))
        cases.append(dict(name='task_strategic_search_run',words=words,args=[],eax=[rng.getrandbits(32) for _ in range(4)],
            perturb=rng.choice([0,0,1,2,3]),stub_words=[rng.getrandbits(32) for _ in range(5)],
            cw=rng.choice(CONTROL_WORDS)))
    return cases

DISTANCES=[-5,0,1,100,0xfffff,0x100000,0x100001,-0x80000000]

def cell_cases(count):
    rng=random.Random(0x3c11a9);cases=[]
    for index in range(count):
        words=[rng.getrandbits(32) for _ in range(0x1f4//4)]
        words[0x14//4]=SCHEDULER
        words[0x38//4]=rng.randrange(0,512);words[0x40//4]=rng.randrange(0,256)
        words[0x44//4]=MAP
        words[0x1e0//4]=rng.choice([RESULT,RESULT+8])
        words[0x1e4//4]=rng.choice([-1&0xffffffff,0,1,3,5,rng.getrandbits(32)])
        words[0x1e8//4]=rng.choice([0,0,rng.getrandbits(4),rng.getrandbits(32)])
        words[0x1f0//4]=rng.choice([-1&0xffffffff,0,7,rng.getrandbits(32)])
        cells=bytes(rng.choice([0,0xff,rng.getrandbits(8)]) for _ in range(0x400))
        cases.append(dict(name='task_cell_query_execute',words=words,args=[],eax=[rng.getrandbits(32) for _ in range(4)],
            perturb=rng.choice([0,0,1,2]),stub_words=[rng.getrandbits(32) for _ in range(5)],
            distances=[rng.choice(DISTANCES) if rng.random()<0.5 else rng.randrange(-100,0x200000) for _ in range(8)],
            shift=rng.choice([3,3,0x23]),cells=cells,cw=rng.choice(CONTROL_WORDS)))
    return cases

COORDINATES=[0,1,-1,63,64,0x7fffffff,-0x80000000,0x1000000,0x1000001,-0x1000001,0x7fffff80]

def distance_cases(count):
    rng=random.Random(0x9de5c0);cases=[]
    for index in range(count):
        words=[rng.choice(COORDINATES) if rng.random()<0.3 else rng.choice([rng.randrange(-4096,4096),rng.getrandbits(32)])
               for _ in range(0x30//4)]
        cases.append(dict(name='strategic_cell_distance',words=words,args=[rng.choice([TASK,TASK+0x10,TASK+0x20])],
            eax=[0]*4,stub_words=[0]*5,cw=rng.choice(CONTROL_WORDS)))
    return cases

# Direct init cases: the point and bound arguments may overlap the task, and
# out may point into it, so copy order and the late *out store show up.
def init_cases(count):
    rng=random.Random(0x9de100);cases=[]
    def pointer():return rng.choice([TASK+0x40,TASK+0x1e0,TASK+0x17c,TASK+0x188,RESULT,RESULT+4,CHILD])
    for index in range(count):
        name=('cell_query_init','player_map_search_init')[index%2]
        words=[rng.getrandbits(32) for _ in range(0x200//4)]
        values=[rng.getrandbits(32) for _ in range(3)]
        if name=='cell_query_init':
            args=[rng.getrandbits(32),pointer(),values[0],pointer()]
        else:
            args=[rng.getrandbits(32),pointer(),pointer(),pointer(),values[0],pointer(),pointer(),pointer(),values[1]]
        cases.append(dict(name=name,words=words,args=args,eax=[0]*4,stub_words=[0]*5,cw=rng.choice(CONTROL_WORDS),
                          extra=[rng.getrandbits(32) for _ in range(0x40//4)]))
    return cases

class Runner:
    def __init__(self,image,pe,spec):
        m=self.machine=load_machine(image,pe);self.spec=spec
        m.mem_write(TRAMPOLINE,b'\x90')
        self.preload={}
        for index,name in enumerate(ENTRIES):
            stub=PRELOAD+0x10*index;code=b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)
            code+=b'\xe9'+struct.pack('<I',(spec[name]-stub-len(code)-5)&0xffffffff)
            m.mem_write(stub,code);self.preload[name]=stub
        stubs={spec['scheduler_remove']:(self.stub('remove',1),4),spec['scheduler_add']:(self.stub('add',4),0x10),
               spec['local_search_prepare']:(self.prepare('local_prepare',5),0x14),
               spec['local_search_finish']:(self.prepare('local_finish',3),0xc),
               spec['strategic_map_prepare']:(self.prepare('strategic_prepare',5),0x14),
               spec['task_sleep']:(self.stub('sleep',2),8),
               spec['astar_strategic_setup']:(self.setup,0x18)}
        for address,(handler,pop) in stubs.items():
            m.hook_add(UC_HOOK_CODE,self.hook(handler,pop),begin=address,end=address)
        m.hook_add(UC_HOOK_CODE,self.hook(self.layer_point,0xc),begin=spec['strategic_layer_point'],end=spec['strategic_layer_point'])
        m.hook_add(UC_HOOK_CODE,self.hook(self.ground_height,4),begin=spec['strategic_ground_height'],end=spec['strategic_ground_height'])
        m.hook_add(UC_HOOK_CODE,self.hook(self.distance,4),begin=spec['strategic_cell_distance'],end=spec['strategic_cell_distance'])
        # the native player map search init keeps ECX, as the cell query init does
        m.hook_add(UC_HOOK_CODE,self.hook(self.player_map_init,0x24,keep_ecx=True),
                   begin=spec['player_map_search_init'],end=spec['player_map_search_init'])
        # the native cell query init returns with ECX still the query
        m.hook_add(UC_HOOK_CODE,self.hook(self.cell_query,0x10,keep_ecx=True),
                   begin=spec['cell_query_init'],end=spec['cell_query_init'])
    def r32(self,p,n=1):return list(struct.unpack('<%dI'%n,self.machine.mem_read(p,4*n)))
    def w32(self,p,*v):self.machine.mem_write(p,struct.pack('<%dI'%len(v),*[x&0xffffffff for x in v]))
    def hook(self,handler,pop,keep_ecx=False):
        def run(m,address,size,user):
            sp=m.reg_read(UC_X86_REG_ESP);back,*args=self.r32(sp,10);ecx=m.reg_read(UC_X86_REG_ECX)
            eax=handler(ecx,args)
            if eax is None:return
            m.reg_write(UC_X86_REG_EAX,eax);m.reg_write(UC_X86_REG_ECX,ecx if keep_ecx else 0xc1c1c1c1)
            m.reg_write(UC_X86_REG_EDX,0xd2d2d2d2)
            m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,back)
        return run
    def stub(self,name,count):
        def handler(ecx,args):
            self.log.append((name,ecx,*args[:count]));return self.case['eax'][len(self.log)%4]
        return handler
    def prepare(self,name,count):
        def handler(ecx,args):
            case=self.case;self.log.append((name,ecx,*args[:count]))
            if case['perturb']==1:self.w32(TASK+0x16c,*case['stub_words'])
            elif case['perturb']==2:self.w32(TASK+0x14,case['stub_words'][0]);self.w32(TASK+0x170,case['stub_words'][1])
            return case['eax'][len(self.log)%4]
        return handler
    def setup(self,ecx,args):
        case=self.case
        self.log.append(('setup',ecx,*args[:4],*self.r32(args[4],3),*self.r32(args[5],3)))
        if case['perturb']==1:self.w32(TASK+0x2f4,RESULT+0x10,*case['stub_words'][1:3])
        elif case['perturb']==2:self.w32(TASK+0x14,case['stub_words'][0])
        return case['eax'][len(self.log)%4]
    def cell_query(self,ecx,args):
        if self.case['name']=='cell_query_init':return None
        case=self.case;self.log.append(('cell_query',ecx,*args[:4]))
        if case['perturb']==3:self.w32(TASK+0x300,case['stub_words'][3])
        elif case['perturb']==2:self.w32(TASK+0x14,case['stub_words'][0])
        return case['eax'][len(self.log)%4]
    def layer_point(self,ecx,args):
        case=self.case;out,point,layer=args[:3]
        self.log.append(('layer_point',ecx,point,layer))
        self.w32(out,*case['stub_words'][:3])
        if case['perturb']==1:self.w32(TASK+0x1e8,case['stub_words'][3])
        elif case['perturb']==2:self.w32(TASK+0x1e0,RESULT+0x10)
        return out
    def ground_height(self,ecx,args):
        edx=self.machine.reg_read(UC_X86_REG_EDX)
        self.log.append(('ground_height',ecx,*self.r32(edx,3),args[0]&0xff))
        self.w32(edx+4,self.case['stub_words'][4]+len(self.log))
        return self.case['eax'][len(self.log)%4]
    def distance(self,ecx,args):
        if self.case['name']=='strategic_cell_distance':return None
        self.log.append(('distance',*self.r32(ecx,3),args[0]))
        return self.case['distances'][len(self.log)%8]
    def player_map_init(self,ecx,args):
        if self.case['name']=='player_map_search_init':return None
        self.log.append(('player_map_init',ecx,args[0],args[1],*self.r32(args[2],3),*args[3:6],
                         *self.r32(args[6],3),*self.r32(args[7],3),args[8]))
        return self.case['eax'][len(self.log)%4]
    def run(self,case):
        m=self.machine;self.case=case;self.log=[]
        m.mem_write(ARENA,bytes(SNAPSHOT))
        self.w32(TASK,*case['words'])
        if 'extra' in case:self.w32(CHILD,*case['extra'])
        if 'cells' in case:
            self.w32(MAP+0x40,GRID);self.w32(GRID+0x1c,CELLS);self.w32(GRID+0x28,case['shift'])
            m.mem_write(CELLS,case['cells'])
        sp=STACK+0x8000
        self.w32(sp,TRAMPOLINE,*case['args'])
        cw=case['cw'];m.mem_write(CONTROL,struct.pack('<H',cw))
        for reg,value in SAVED.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0x5a5a5a5a);m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,TASK)
        m.emu_start(self.preload[case['name']],TRAMPOLINE,timeout=10_000_000,count=1_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+ENTRIES[case['name']]
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        arena=bytes(m.mem_read(ARENA,SNAPSHOT))
        if case['name'] in ('cell_query_init','player_map_search_init'):
            assert m.reg_read(UC_X86_REG_ECX)==TASK,'ECX not kept'
        return dict(eax=m.reg_read(UC_X86_REG_EAX),status=m.reg_read(UC_X86_REG_FPSW)&~0x4700,log=self.log,
                    arena=hashlib.sha256(arena).hexdigest(),child=arena[0x58:0xa8].hex())

def compare_task_queue(original,original_pe,edited,edited_pe,spec,symbols,count=2000):
    for name in ENTRIES:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name]
    old_runner=Runner(original,original_pe,spec);new_runner=Runner(edited,edited_pe,spec)
    results=[]
    for case in queue_cases(count)+run_cases(count//2)+cell_cases(count//2)+distance_cases(count//2)+init_cases(count//2):
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new)
        except Exception as error:raise RuntimeError(f'task queue inputs {case}') from error
        results.append(dict(inputs={k:v for k,v in case.items() if k not in ('words','cells')},**old))
    return results

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
        cases=compare_task_queue(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.count)
        summary={}
        for c in cases:
            s=summary.setdefault(c['inputs']['name'],dict(cases=0,stub_calls=0))
            s['cases']+=1;s['stub_calls']+=len(c['log'])
            if c['inputs']['name'] in ('task_strategic_search_run','task_cell_query_execute'):
                names=[entry[0] for entry in c['log']]
                if c['inputs']['name']=='task_strategic_search_run':calls='+'.join(names) or 'none'
                elif 'player_map_init' in names:calls='queued_search'
                else:calls='no_layer' if names else 'no_stub_calls'
                s[calls]=s.get(calls,0)+1
        (work/'task-queue-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),summary=summary,cases=cases),indent=2))
        print(f'{target}: {len(cases)} task queue comparisons passed {summary}',flush=True)
