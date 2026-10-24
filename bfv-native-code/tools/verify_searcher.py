"""Original/reconstructed A* searcher step (client 0x00a09340, server
0x007bd990) against src/astar_searcher.c.

Both images run the step on the same random searcher. The searcher's native
helpers (reset, node alloc, heap push/pop/update, closed-list removal) and the
problem's and job's methods are Python stubs that log their arguments and act
on the arena: the heap is kept sorted by node total, alloc hands out pool
nodes until the case's cap, and successors land on a small grid so open and
closed matches happen. The timer global points at a code stub returning a
per-case sequence of doubles (crossing the deadline at a chosen call, or NaN),
the log global at a logging stub. Cases start fresh searches and resume
prepared ones (open heap and hash, closed list and hash, empty or partly empty
successor slots), with the time pointer's low byte varied since the native
code reuses that argument slot as a flag. Each case starts from FNINIT plus a
chosen control word and compares EAX, the stub log, the arena, ESP,
callee-saved registers and the FPU control, tag and status words (apart from
C0-C3).

The heap helpers, node alloc, the path output routines and the run wrapper are also run
directly (HeapRunner, OutputRunner) on random heaps, hashes, closed lists,
parent chains and rings.
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

SEARCHER=ARENA;NODES=ARENA+0x1000;OPEN_HASH=ARENA+0x2000;CLOSED_HASH=ARENA+0x2080;HEAP=ARENA+0x2100
PROBLEM=ARENA+0x2400;PROBLEM_VTABLE=ARENA+0x2440;JOB=ARENA+0x2480;JOB_VTABLE=ARENA+0x2490
TIMER=ARENA+0x24c0;TIMER_VTABLE=ARENA+0x24d0;LOG=ARENA+0x2500;LOG_VTABLE=ARENA+0x2510
TIMES=ARENA+0x2600;COUNTER=ARENA+0x2800;TIME_LEFT=ARENA+0x2900;RUNNING=ARENA+0x2a00;CONTROL=ARENA+0x2a10
SNAPSHOT=0x2b00;CODE=ARENA+0x9000;TRAMPOLINE=ARENA+0x9400;PRELOAD=ARENA+0x9410
NODE=0x34;POOL=48;BUCKETS=16
HELPERS=dict(searcher_reset=0,searcher_alloc=0,searcher_heap_push=4,searcher_heap_pop=0,
             searcher_heap_update=4,searcher_unclose=4)
# problem vtable slot: (name, stack argument bytes)
PROBLEM_SLOTS={0:('delete',4),2:('estimate',4),3:('successors',12),4:('is_goal',4),6:('set_start',4),7:('set_goal',4)}

def d(x):return struct.unpack('<Q',struct.pack('<d',x))[0]
def f(x):return struct.unpack('<I',struct.pack('<f',x))[0]
TIME_LEFTS=[d(0.5),d(0.0),d(-1.0),d(1e-9),d(float('inf')),0x7ff8000000000000,d(3.0)]
COSTS=[f(1.0),f(1.5),f(2.5),f(-1.0),f(0.0),0x80000000,f(1e10),0x7fc00000,0x7f800000,f(3.49999),1]
POSITIONS=[0,1,2,3,-1,0x7fffffff,-0x80000000,0x4000000]

def node_words(rng):
    return [rng.getrandbits(32) for _ in range(NODE//4)]

def cases(count):
    rng=random.Random(0xa09340);out=[]
    for index in range(count):
        fresh=index%2==0
        case=dict(fresh=fresh,seed=rng.getrandbits(32),cw=rng.choice(CONTROL_WORDS),
                  in_progress=0 if fresh else rng.choice([1,1,0x80,0xff]),
                  time_offset=rng.choice([0,8,0x18,0x21,0x40]),running=rng.getrandbits(8),
                  time_left=rng.choice(TIME_LEFTS),buckets=rng.choice([1,2,5,7,16,-7,-16]),
                  node_limit=rng.choice([3,5,10,40,100]),pool_cap=rng.choice([2,5,12,POOL,POOL]),
                  problem_slots=rng.choice([0,1,3,4,8,-1]),problem_steps=rng.choice([0,1,2,5,20,20,-1]),
                  goal_rate=rng.choice([0,0.05,0.3]),deadline_call=rng.choice([1,2,3,5,8,30,30]),
                  nan_call=rng.choice([None,None,None,2,4]),
                  nodes=[node_words(rng) for _ in range(POOL)],words=[rng.getrandbits(32) for _ in range(0x884//4)])
        if index%4>=2:
            # long searches on a 3x3 grid, so successors meet open and closed
            # nodes and cheap (zero or negative) steps reopen closed ones
            case.update(node_limit=100,pool_cap=POOL,problem_slots=rng.choice([4,8]),problem_steps=20,
                        goal_rate=rng.choice([0,0.02]),deadline_call=30,nan_call=None,small=True,
                        time_left=d(0.5))
        out.append(case)
    return out

class Runner:
    def __init__(self,image,pe,spec):
        m=self.machine=load_machine(image,pe);self.spec=spec
        m.mem_write(TRAMPOLINE,b'\x90')
        code=b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)
        code+=b'\xe9'+struct.pack('<I',(spec['searcher_step']-PRELOAD-len(code)-5)&0xffffffff)
        m.mem_write(PRELOAD,code)
        # the timer's method 3: ST0 = TIMES[counter++ & 63]
        timer=CODE+0x200
        m.mem_write(timer,b'\xa1'+struct.pack('<I',COUNTER)+b'\xff\x05'+struct.pack('<I',COUNTER)
                    +b'\x83\xe0\x3f\xdd\x04\xc5'+struct.pack('<I',TIMES)+b'\xc3')
        self.timer_method=timer
        for name,pop in HELPERS.items():
            m.hook_add(UC_HOOK_CODE,self.hook(getattr(self,name),pop),begin=spec[name],end=spec[name])
        self.problem_methods={}
        for slot,(name,pop) in PROBLEM_SLOTS.items():
            address=CODE+0x10*slot;m.mem_write(address,b'\xc3');self.problem_methods[slot]=address
            m.hook_add(UC_HOOK_CODE,self.hook(getattr(self,'problem_'+name),pop),begin=address,end=address)
        self.job_method=CODE+0x100;m.mem_write(self.job_method,b'\xc3')
        m.hook_add(UC_HOOK_CODE,self.hook(self.job_problem,0),begin=self.job_method,end=self.job_method)
        self.log_method=CODE+0x110;m.mem_write(self.log_method,b'\xc3')
        m.hook_add(UC_HOOK_CODE,self.hook(self.log_call,0),begin=self.log_method,end=self.log_method)
    def r32(self,p,n=1):return list(struct.unpack('<%dI'%n,self.machine.mem_read(p,4*n)))
    def s32(self,p):return struct.unpack('<i',self.machine.mem_read(p,4))[0]
    def w32(self,p,*v):self.machine.mem_write(p,struct.pack('<%dI'%len(v),*[x&0xffffffff for x in v]))
    def hook(self,handler,pop):
        def run(m,address,size,user):
            sp=m.reg_read(UC_X86_REG_ESP);back,*args=self.r32(sp,5);ecx=m.reg_read(UC_X86_REG_ECX)
            eax=handler(ecx,args)
            m.reg_write(UC_X86_REG_EAX,eax&0xffffffff);m.reg_write(UC_X86_REG_ECX,0xc1c1c1c1)
            m.reg_write(UC_X86_REG_EDX,0xd2d2d2d2)
            m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,back)
        return run
    def junk(self):return self.rng.getrandbits(32)
    # the heap: slot 0 unused, kept sorted by total (stable)
    def heap_nodes(self):
        begin,end=self.r32(SEARCHER+4,2)
        return self.r32(begin+4,(end-begin)//4-1) if end-begin>4 else []
    def set_heap(self,nodes):
        nodes=sorted(nodes,key=lambda n:self.s32(n+0x1c))
        self.w32(HEAP+4,*nodes) if nodes else None
        self.w32(SEARCHER+4,HEAP,HEAP+4+4*len(nodes))
    def searcher_reset(self,ecx,args):
        self.log.append(('reset',ecx))
        self.w32(SEARCHER+4,HEAP,HEAP+4);self.w32(SEARCHER+0x30,0)
        self.w32(OPEN_HASH,*[0]*BUCKETS);self.w32(CLOSED_HASH,*[0]*BUCKETS);self.w32(SEARCHER+0x28,0)
        return self.junk()
    def searcher_alloc(self,ecx,args):
        used=self.r32(SEARCHER+0x30)[0];self.log.append(('alloc',ecx,used))
        if used>=min(POOL,self.case['pool_cap']):return 0
        self.w32(SEARCHER+0x30,used+1);return NODES+NODE*used
    def searcher_heap_push(self,ecx,args):
        self.log.append(('push',ecx,args[0],*self.r32(args[0],NODE//4)))
        self.set_heap(self.heap_nodes()+[args[0]]);return self.junk()
    def searcher_heap_pop(self,ecx,args):
        nodes=self.heap_nodes();self.log.append(('pop',ecx,*nodes))
        self.set_heap(nodes[1:]);return self.junk()
    def searcher_heap_update(self,ecx,args):
        self.log.append(('update',ecx,args[0],*self.r32(args[0],NODE//4)))
        self.set_heap(self.heap_nodes());return self.junk()
    def searcher_unclose(self,ecx,args):
        node=args[0];self.log.append(('unclose',ecx,node))
        # closed_prev is only set on a node that stops being the head, so the
        # model finds the predecessor by walking the list
        link=SEARCHER+0x28
        for _ in range(POOL+1):
            current=self.r32(link)[0]
            if not current or not NODES<=current<NODES+NODE*POOL:break
            if current==node:self.w32(link,self.r32(node+0x24)[0]);break
            link=current+0x24
        for bucket in range(BUCKETS):
            link=CLOSED_HASH+4*bucket
            for _ in range(POOL+1):
                if not self.r32(link)[0]:break
                if self.r32(link)[0]==node:self.w32(link,self.r32(node+0x28)[0]);break
                link=self.r32(link)[0]+0x28
        return self.junk()
    def job_problem(self,ecx,args):
        self.log.append(('job',ecx));return PROBLEM
    def log_call(self,ecx,args):
        self.log.append(('log',ecx));return self.junk()
    def problem_delete(self,ecx,args):
        self.log.append(('delete',ecx,args[0]));return self.junk()
    def problem_estimate(self,ecx,args):
        self.log.append(('estimate',ecx,args[0],*self.r32(args[0],4)))
        return self.rng.choice([0,1,5,100,-3,0x7fffffff,self.junk()])
    def place(self,node,start):
        rng=self.rng
        if self.case.get('small'):
            self.w32(node,rng.randrange(3),rng.getrandbits(32),rng.randrange(3))
            if not start:self.w32(node+0xc,rng.choice([0,0,0,1]))
            return
        self.w32(node,rng.choice(POSITIONS[:4]) if rng.random()<0.85 else rng.choice(POSITIONS),rng.getrandbits(32),
                 rng.choice(POSITIONS[:4]) if rng.random()<0.85 else rng.choice(POSITIONS))
        if not start:self.w32(node+0xc,rng.choice([0,0,1]))
    def problem_set_start(self,ecx,args):
        self.log.append(('set_start',ecx,args[0]));self.place(args[0],True);return self.junk()
    def problem_set_goal(self,ecx,args):
        self.log.append(('set_goal',ecx,args[0]));self.place(args[0],True);return self.junk()
    def problem_is_goal(self,ecx,args):
        self.log.append(('is_goal',ecx,args[0]))
        return (self.junk()&~0xff)|(self.rng.random()<self.case['goal_rate'])
    def problem_successors(self,ecx,args):
        node,slots,costs=args[:3];rng=self.rng
        self.log.append(('successors',ecx,node,slots,costs,*self.r32(node,NODE//4)))
        usable=0
        while usable<0x100 and usable<max(self.case['problem_slots'],0) and self.r32(slots+4*usable)[0]:usable+=1
        count=rng.randrange(0,usable+1) if rng.random()<0.9 else -1
        for i in range(max(count,0)):
            target=self.r32(slots+4*i)[0];self.place(target,False)
            self.w32(target+0x10,rng.choice([0,1,2,7,-1,self.junk()]))
            if self.case.get('small'):cost=f(rng.choice([0.0,0.0,1.0,-1.0,-2.5,0.5]))
            else:cost=rng.choice(COSTS) if rng.random()<0.4 else f(rng.choice([1.0,1.0,1.4142135,2.0,10.0]))
            self.w32(costs+4*i,cost)
        return count
    def prepare(self,case):
        """A resumed search: the problem set, some open nodes in the heap and
        open hash, some closed nodes in the closed list and hash, and the
        successor slots partly filled."""
        rng=self.rng;used=rng.randrange(1,10);buckets=abs(case['buckets'])
        self.w32(SEARCHER+0x30,used);self.w32(SEARCHER+0x880,PROBLEM)
        self.w32(OPEN_HASH,*[0]*BUCKETS);self.w32(CLOSED_HASH,*[0]*BUCKETS);self.w32(SEARCHER+0x28,0)
        nodes=[NODES+NODE*i for i in range(used)];rng.shuffle(nodes)
        split=rng.randrange(0,len(nodes)+1);opened,closed=nodes[:split],nodes[split:]
        for node in nodes:
            self.place(node,False);self.w32(node+0x18,rng.choice([0,1,3,10,rng.getrandbits(8)]))
            self.w32(node+0x1c,rng.getrandbits(6))
            z,x=self.s32(node+8),self.s32(node)
            mixed=struct.unpack('<i',struct.pack('<I',(z*31+x*17)&0xffffffff))[0]
            bucket=abs(mixed)%buckets  # |idiv remainder|
            assert bucket<buckets
            table=OPEN_HASH if node in opened else CLOSED_HASH
            self.w32(node+0x28,self.r32(table+4*bucket)[0]);self.w32(table+4*bucket,node)
        previous=0
        for node in reversed(closed):
            self.w32(node+0x24,previous);self.w32(node+0x20,0)
            if previous:self.w32(previous+0x20,node)
            previous=node
        self.w32(SEARCHER+0x28,previous)
        self.set_heap(opened)
        if rng.random()<0.1:self.w32(SEARCHER+4,0)
        self.w32(SEARCHER+0x64,rng.choice(nodes))
        self.w32(SEARCHER+0x6c,rng.choice([0,1,2,5,20,-1]))
        slots=rng.choice([0,1,3,4,8,-2])
        self.w32(SEARCHER+0x70,slots)
        self.w32(SEARCHER+0x80,*[0]*0x100)
        for i in range(max(slots,0)):
            spare=NODES+NODE*(used+i) if rng.random()<0.6 and used+i<POOL else 0
            self.w32(SEARCHER+0x80+4*i,spare)
        self.w32(SEARCHER+0x30,used+max(slots,0))
    def run(self,case):
        m=self.machine;self.case=case;self.log=[];self.rng=random.Random(case['seed'])
        m.mem_write(ARENA,bytes(SNAPSHOT))
        self.w32(SEARCHER,*case['words'])
        for i,words in enumerate(case['nodes']):self.w32(NODES+NODE*i,*words)
        self.w32(SEARCHER+0x24,OPEN_HASH);self.w32(SEARCHER+0x2c,CLOSED_HASH)
        self.w32(SEARCHER+0x58,case['buckets']);self.w32(SEARCHER+0x5c,case['node_limit'])
        self.w32(SEARCHER+0x30,0)
        self.w32(PROBLEM,PROBLEM_VTABLE);self.w32(PROBLEM+0x10,case['problem_slots']);self.w32(PROBLEM+0x14,case['problem_steps'])
        for slot,address in self.problem_methods.items():self.w32(PROBLEM_VTABLE+4*slot,address)
        self.w32(JOB,JOB_VTABLE);self.w32(JOB_VTABLE+4,self.job_method)
        self.w32(TIMER,TIMER_VTABLE);self.w32(TIMER_VTABLE+0xc,self.timer_method)
        self.w32(LOG,LOG_VTABLE);self.w32(LOG_VTABLE+0xcc,self.log_method)
        self.w32(self.spec['search_timer'],TIMER);self.w32(self.spec['search_log'],LOG)
        # times: the first sets the deadline; later calls stay before it until
        # the chosen call, then pass it
        left=struct.unpack('<d',struct.pack('<Q',case['time_left']))[0]
        start=self.rng.uniform(0,1000);times=[start]
        for call in range(1,64):
            if call==case['nan_call']:times.append(float('nan'))
            elif call<case['deadline_call']:times.append(start+self.rng.uniform(-0.1,0.4)*(left if left==left and abs(left)<1e300 else 1))
            else:times.append(start+left+self.rng.choice([0.0,0.25,1e6]) if left==left else start)
        m.mem_write(TIMES,struct.pack('<64d',*times))
        if not case['fresh']:self.prepare(case)
        time_left=TIME_LEFT+case['time_offset'];m.mem_write(time_left,struct.pack('<Q',case['time_left']))
        m.mem_write(RUNNING,bytes([case['running']]))
        sp=STACK+0x8000
        self.w32(sp,TRAMPOLINE,JOB,case['in_progress'],RUNNING,time_left)
        cw=case['cw'];m.mem_write(CONTROL,struct.pack('<H',cw))
        for reg,value in SAVED.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0x5a5a5a5a);m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,SEARCHER)
        m.emu_start(PRELOAD,TRAMPOLINE,timeout=20_000_000,count=2_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+0x10
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        arena=bytes(m.mem_read(ARENA,SNAPSHOT))
        globals_=self.r32(self.spec['search_timer'])+self.r32(self.spec['search_log'])
        return dict(eax=m.reg_read(UC_X86_REG_EAX),status=m.reg_read(UC_X86_REG_FPSW)&~0x4700,log=self.log,
                    arena=hashlib.sha256(arena).hexdigest(),globals=globals_,running=arena[RUNNING-ARENA])

# The heap helpers run directly, natively nested (pop calls sift-down, push
# calls update) with the game's random generator live for ties. Their callers
# (the step, pop, push) reload EAX, ECX and EDX after each call, so those are
# not compared.
HEAP_HELPERS=dict(searcher_sift_down=0,searcher_heap_pop=0,searcher_unclose=4,searcher_heap_update=4,
                  searcher_heap_push=4)
HEAP_CAPACITY=0xb0

def heap_cases(count):
    rng=random.Random(0xa08870);out=[]
    names=list(HEAP_HELPERS)
    for index in range(count):
        out.append(dict(helper=names[index%len(names)],seed=rng.getrandbits(32),cw=rng.choice(CONTROL_WORDS),
                        size=rng.choice([1,1,2,3,4,5,7,8,9,15,16,17,31,POOL-8]),ties=rng.random()<0.6,
                        ordered=rng.random()<0.5,buckets=rng.choice([1,2,5,7,16,-7,-16]),
                        random_state=rng.getrandbits(32),
                        nodes=[node_words(rng) for _ in range(POOL)],words=[rng.getrandbits(32) for _ in range(0x884//4)]))
    return out

class HeapRunner:
    def __init__(self,image,pe,spec):
        m=self.machine=load_machine(image,pe);self.spec=spec
        m.mem_write(TRAMPOLINE,b'\x90')
        # one preload per helper: unicorn keeps translated blocks across
        # mem_write, so a shared rewritten stub would keep its first target
        self.preload={}
        for i,name in enumerate(HEAP_HELPERS):
            address=PRELOAD+0x10*i;code=b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)
            code+=b'\xe9'+struct.pack('<I',(spec[name]-address-len(code)-5)&0xffffffff)
            m.mem_write(address,code);self.preload[name]=address
        m.hook_add(UC_HOOK_CODE,self.push_back,begin=spec['searcher_push_back'],end=spec['searcher_push_back'])
        # counted, not stubbed: the generator itself runs
        m.hook_add(UC_HOOK_CODE,lambda m,a,s,u:self.log.append(('random',)),
                   begin=spec['search_random'],end=spec['search_random'])
    r32=Runner.r32;s32=Runner.s32;w32=Runner.w32
    def push_back(self,m,address,size,user):
        """The heap vector's push_back (thiscall, value pointer, RET 4): the
        native one grows through the allocator, so it is modelled."""
        sp=m.reg_read(UC_X86_REG_ESP);back,value=self.r32(sp,2);ecx=m.reg_read(UC_X86_REG_ECX)
        begin,end=self.r32(ecx+4,2);word=self.r32(value)[0];self.log.append(('push_back',ecx,word))
        if not begin:begin=end=HEAP
        assert end<HEAP+4*HEAP_CAPACITY
        self.w32(end,word);self.w32(ecx+4,begin,end+4)
        m.reg_write(UC_X86_REG_EAX,0xa1a1a1a1);m.reg_write(UC_X86_REG_ECX,0xc1c1c1c1);m.reg_write(UC_X86_REG_EDX,0xd2d2d2d2)
        m.reg_write(UC_X86_REG_ESP,sp+8);m.reg_write(UC_X86_REG_EIP,back)
    def bucket(self,node,buckets):
        z,x=self.s32(node+8),self.s32(node)
        mixed=struct.unpack('<i',struct.pack('<I',(z*31+x*17)&0xffffffff))[0]
        return abs(mixed)%buckets
    def chain(self,table,nodes,buckets):
        rng=self.rng;nodes=list(nodes);rng.shuffle(nodes)
        for node in nodes:
            b=self.bucket(node,buckets);self.w32(node+0x28,self.r32(table+4*b)[0]);self.w32(table+4*b,node)
    def run(self,case):
        m=self.machine;self.case=case;self.log=[];rng=self.rng=random.Random(case['seed'])
        helper=case['helper'];buckets=abs(case['buckets'])
        m.mem_write(ARENA,bytes(SNAPSHOT))
        self.w32(SEARCHER,*case['words'])
        for i,words in enumerate(case['nodes']):self.w32(NODES+NODE*i,*words)
        self.w32(SEARCHER+0x24,OPEN_HASH);self.w32(SEARCHER+0x2c,CLOSED_HASH);self.w32(SEARCHER+0x58,case['buckets'])
        self.w32(OPEN_HASH,*[0]*BUCKETS);self.w32(CLOSED_HASH,*[0]*BUCKETS)
        self.w32(self.spec['search_random_state'],case['random_state'])
        pool=[NODES+NODE*i for i in range(POOL)];rng.shuffle(pool)
        for node in pool:
            self.w32(node,rng.randrange(4) if rng.random()<0.9 else rng.getrandbits(32))
            self.w32(node+8,rng.randrange(4) if rng.random()<0.9 else rng.getrandbits(32))
            self.w32(node+0x1c,rng.randrange(4) if case['ties'] else rng.getrandbits(32))
        size=case['size'] if helper in ('searcher_sift_down','searcher_heap_pop') else rng.choice([0,case['size']])
        heap=pool[:size];rest=pool[size:]
        if case['ordered']:heap.sort(key=lambda n:self.s32(n+0x1c))
        for i,node in enumerate(heap):self.w32(node+0x14,i+1 if rng.random()<0.9 else rng.getrandbits(32))
        self.w32(HEAP,rng.choice(pool),*heap)
        self.w32(SEARCHER+4,HEAP,HEAP+4+4*size,HEAP+4*HEAP_CAPACITY)
        argument=0
        if helper in ('searcher_sift_down','searcher_heap_pop'):
            self.chain(OPEN_HASH,heap+rest[:rng.randrange(4)],buckets)
        elif helper=='searcher_heap_update':
            if not heap:heap=[rest.pop()];self.w32(HEAP+4,heap[0]);self.w32(SEARCHER+8,HEAP+8)
            index=rng.randrange(len(heap));argument=heap[index];self.w32(argument+0x14,index+1)
            total=self.s32(argument+0x1c)
            self.w32(argument+0x1c,rng.choice([total,total-1,total-rng.randrange(1,8),rng.getrandbits(32)]))
        elif helper=='searcher_heap_push':
            if heap and rng.random()<0.25:
                index=rng.randrange(len(heap));argument=heap[index];self.w32(argument+0x14,index+1)
            else:
                argument=rest.pop()
                self.w32(argument+0x14,rng.choice([0,0,1,len(heap),len(heap)+1,rng.getrandbits(32)]))
        else:
            closed=rest[:rng.randrange(1,12)];self.chain(CLOSED_HASH,closed,buckets)
            # the closed list, newest first; the head's back link is null
            for i,node in enumerate(closed):
                self.w32(node+0x20,closed[i-1] if i else 0)
                self.w32(node+0x24,closed[i+1] if i+1<len(closed) else 0)
            self.w32(SEARCHER+0x28,closed[0])
            argument=rng.choice(closed)
            if argument!=closed[0] and rng.random()<0.1:self.w32(argument+0x20,0)
        sp=STACK+0x8000;self.w32(sp,TRAMPOLINE,argument,0x5b5b5b5b)
        cw=case['cw'];m.mem_write(CONTROL,struct.pack('<H',cw))
        for reg,value in SAVED.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0x5a5a5a5a);m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,SEARCHER)
        m.emu_start(self.preload[helper],TRAMPOLINE,timeout=20_000_000,count=2_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+HEAP_HELPERS[helper]
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        arena=bytes(m.mem_read(ARENA,SNAPSHOT))
        return dict(status=m.reg_read(UC_X86_REG_FPSW)&~0x4700,log=self.log,arena=hashlib.sha256(arena).hexdigest(),
                    random_state=self.r32(self.spec['search_random_state'])[0],
                    heap=self.r32(SEARCHER+4,2))

def compare_searcher_heap(original,original_pe,edited,edited_pe,spec,symbols,count=2000):
    for name in HEAP_HELPERS:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9,name
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name],name
    old_runner=HeapRunner(original,original_pe,spec);new_runner=HeapRunner(edited,edited_pe,spec)
    results=[]
    for case in heap_cases(count):
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new)
        except Exception as error:
            raise RuntimeError(f'searcher heap inputs { {k:v for k,v in case.items() if k not in ("nodes","words")} }') from error
        results.append(dict(inputs={k:v for k,v in case.items() if k not in ('nodes','words')},**old))
    return results

# node alloc and the path output routines: (stack argument bytes)
OUTPUT_HELPERS=dict(searcher_reset=0,searcher_alloc=0,searcher_path=12,path_ring_extend=4,searcher_path_to_ring=12,
                    searcher_closed_points=12,searcher_run=4,searcher_deliver=4)
POINTS=ARENA+0x2000;IDS=ARENA+0x2400;RING=ARENA+0x2600;NODE_VECTOR=ARENA+0x2700;PRELOAD_OUTPUT=ARENA+0x9600
DEQUE_MAP=ARENA+0x2800;DEQUE_BLOCKS=ARENA+0x2840;CALLBACK=ARENA+0x2980;CALLBACK_METHOD=ARENA+0x9800

def output_cases(count):
    rng=random.Random(0xa08ad0);out=[]
    names=list(OUTPUT_HELPERS)
    for index in range(count):
        out.append(dict(helper=names[index%len(names)],seed=rng.getrandbits(32),cw=rng.choice(CONTROL_WORDS),
                        length=rng.choice([0,1,1,2,3,5,8,13,20,33,40]),ids=rng.random()<0.6,
                        nodes=[node_words(rng) for _ in range(POOL)],words=[rng.getrandbits(32) for _ in range(0x884//4)]))
    return out

class OutputRunner:
    """Node alloc and the path output routines on a random searcher: parent
    chains, closed lists and rings. The pool allocator and the node vector's
    push_back are logged models."""
    def __init__(self,image,pe,spec):
        m=self.machine=load_machine(image,pe);self.spec=spec
        m.mem_write(TRAMPOLINE,b'\x90')
        self.preload={}
        for i,name in enumerate(OUTPUT_HELPERS):
            address=PRELOAD_OUTPUT+0x10*i;code=b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)
            code+=b'\xe9'+struct.pack('<I',(spec[name]-address-len(code)-5)&0xffffffff)
            m.mem_write(address,code);self.preload[name]=address
        m.hook_add(UC_HOOK_CODE,self.push_back,begin=spec['searcher_push_back'],end=spec['searcher_push_back'])
        m.hook_add(UC_HOOK_CODE,self.allocate,begin=spec['collision_allocate'],end=spec['collision_allocate'])
        m.hook_add(UC_HOOK_CODE,self.free,begin=spec['searcher_pool_free'],end=spec['searcher_pool_free'])
        m.hook_add(UC_HOOK_CODE,self.resize,begin=spec['searcher_resize'],end=spec['searcher_resize'])
        m.hook_add(UC_HOOK_CODE,self.step,begin=spec['searcher_step'],end=spec['searcher_step'])
        m.hook_add(UC_HOOK_CODE,self.deque_push,begin=spec['path_deque_push_back'],end=spec['path_deque_push_back'])
        m.hook_add(UC_HOOK_CODE,self.deque_destroy,begin=spec['path_deque_destroy'],end=spec['path_deque_destroy'])
        m.hook_add(UC_HOOK_CODE,self.deliver_step,begin=CALLBACK_METHOD,end=CALLBACK_METHOD)
    r32=Runner.r32;s32=Runner.s32;w32=Runner.w32
    def ret(self,sp,back,pop,eax):
        m=self.machine
        m.reg_write(UC_X86_REG_EAX,eax);m.reg_write(UC_X86_REG_ECX,0xc1c1c1c1);m.reg_write(UC_X86_REG_EDX,0xd2d2d2d2)
        m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,back)
    def push_back(self,m,address,size,user):
        sp=m.reg_read(UC_X86_REG_ESP);back,value=self.r32(sp,2);ecx=m.reg_read(UC_X86_REG_ECX)
        begin,end=self.r32(ecx+4,2);word=self.r32(value)[0];self.log.append(('push_back',ecx,word))
        if not begin:begin=end=NODE_VECTOR
        self.w32(end,word);self.w32(ecx+4,begin,end+4)
        self.ret(sp,back,4,0xa1a1a1a1)
    def allocate(self,m,address,size,user):
        """The pool allocator (thiscall: size, source, flags; RET 0Ch) hands
        out a pool node with its old contents, or fails."""
        sp=m.reg_read(UC_X86_REG_ESP);back,*args=self.r32(sp,4)
        self.log.append(('allocate',m.reg_read(UC_X86_REG_ECX),*args))
        self.ret(sp,back,12,self.fresh)
    def free(self,m,address,size,user):
        sp=m.reg_read(UC_X86_REG_ESP);back,node=self.r32(sp,2)
        self.log.append(('free',m.reg_read(UC_X86_REG_ECX),node));self.ret(sp,back,4,0xf4f4f4f4)
    def resize(self,m,address,size,user):
        """A vector's resize (thiscall: count, value; RET 8): shrinks or
        appends the value; the native one reallocates, so it is modelled."""
        sp=m.reg_read(UC_X86_REG_ESP);back,count,value=self.r32(sp,3);ecx=m.reg_read(UC_X86_REG_ECX)
        self.log.append(('resize',ecx,count,value))
        begin,end=self.r32(ecx+4,2)
        if begin:
            held=(end-begin)>>2
            if count<held:self.w32(ecx+8,begin+4*count)
            elif count>held:self.w32(end,*[value]*(count-held));self.w32(ecx+8,begin+4*count)
        self.ret(sp,back,8,0x7e000000+len(self.log))
    def step(self,m,address,size,user):
        """The search step (thiscall: job, in_progress, running, time_left;
        RET 10h), entered from the run wrapper. Its stack addresses are logged
        relative to the wrapper's entry, because the step reads the low byte
        of the time pointer."""
        sp=m.reg_read(UC_X86_REG_ESP);back,job,in_progress,running,time_left=self.r32(sp,5)
        entry=self.entry_sp
        self.log.append(('step',m.reg_read(UC_X86_REG_ECX),job,in_progress,running-entry,time_left-entry,
                         bytes(m.mem_read(time_left,8)).hex(),sp-entry))
        m.mem_write(running,bytes([self.step_running]))
        self.ret(sp,back,16,self.step_eax)
    def deque_push(self,m,address,size,user):
        """The node deque's push_back (thiscall: value pointer; RET 4). The
        native one grows the map through operator new, so it is modelled: the
        first push sets up a map of this case's size with a random front, so
        block indexes wrap around the map end."""
        sp=m.reg_read(UC_X86_REG_ESP);back,value=self.r32(sp,2);deque=m.reg_read(UC_X86_REG_ECX)
        word=self.r32(value)[0];self.log.append(('deque_push',word))
        mapped,map_size,offset,held=self.r32(deque+4,4)
        if not mapped:
            mapped,map_size,offset=DEQUE_MAP,self.map_size,self.front
            self.w32(deque+4,mapped,map_size,offset)
        index=offset+held;block=index>>2
        if map_size<=block:block-=map_size
        if not self.r32(mapped+4*block)[0]:self.w32(mapped+4*block,DEQUE_BLOCKS+0x10*block)
        self.w32(self.r32(mapped+4*block)[0]+4*(index&3),word);self.w32(deque+0x10,held+1)
        self.deque=deque;self.ret(sp,back,4,0xd0d0d0d0)
    def deque_destroy(self,m,address,size,user):
        sp=m.reg_read(UC_X86_REG_ESP);back=self.r32(sp)[0];deque=m.reg_read(UC_X86_REG_ECX)
        self.log.append(('deque_destroy',*self.r32(deque+4,4)))
        self.w32(deque+4,0,0,0,0);self.ret(sp,back,0,0xdeadd0d0)
    def deliver_step(self,m,address,size,user):
        """The delivery callback's method 0 (thiscall: step pointer; RET 4)."""
        sp=m.reg_read(UC_X86_REG_ESP);back,step=self.r32(sp,2)
        self.log.append(('deliver',m.reg_read(UC_X86_REG_ECX),*self.r32(step,3),*self.r32(self.deque+0xc,2)))
        self.ret(sp,back,4,self.rng.getrandbits(32))
    def chain(self,nodes,link):
        for a,b in zip(nodes,nodes[1:]+[0]):self.w32(a+link,b)
    def run(self,case):
        m=self.machine;self.case=case;self.log=[];rng=random.Random(case['seed'])
        helper=case['helper'];length=case['length']
        m.mem_write(ARENA,bytes(SNAPSHOT))
        self.w32(SEARCHER,*case['words'])
        for i,words in enumerate(case['nodes']):self.w32(NODES+NODE*i,*words)
        self.w32(POINTS,*[rng.getrandbits(32) for _ in range(0x400//4)])
        pool=[NODES+NODE*i for i in range(POOL)];rng.shuffle(pool)
        ids=IDS if case['ids'] else 0
        args=[0x5b5b5b5b]*3
        eax=0x5a5a5a5a
        if helper=='searcher_reset':
            held=rng.choice([0,0,1,2,length,length])
            begin=rng.choice([NODE_VECTOR,NODE_VECTOR,NODE_VECTOR,0])
            self.w32(SEARCHER+0x14,begin,begin+4*held if begin else rng.choice([0,0x100]),NODE_VECTOR+0x100)
            self.w32(NODE_VECTOR,*pool[:held])
            self.w32(SEARCHER+0x24,OPEN_HASH);self.w32(SEARCHER+0x2c,CLOSED_HASH)
            self.w32(OPEN_HASH,*[rng.getrandbits(32) for _ in range(0x100//4)])
            self.w32(SEARCHER+0x58,rng.choice([0,1,5,16,32,-1,-16]))
            heap=rng.choice([0,HEAP]);self.w32(SEARCHER+4,heap,heap+4*rng.randrange(8) if heap else 0)
            eax=rng.getrandbits(32)
        elif helper=='searcher_alloc':
            held=rng.choice([0,0,1,3,length])
            begin=rng.choice([NODE_VECTOR,NODE_VECTOR,0]) if held==0 else NODE_VECTOR
            self.w32(SEARCHER+0x14,begin,begin+4*held if begin else rng.choice([0,0x100]),NODE_VECTOR+0x100)
            self.w32(NODE_VECTOR,*pool[:held])
            self.w32(SEARCHER+0x5c,rng.choice([held,held+1,held-1,0,-1,0x7fffffff,rng.getrandbits(32)]))
            self.fresh=0 if rng.random()<0.2 else pool[-1]
        elif helper in ('searcher_path','searcher_path_to_ring'):
            path=pool[:length]
            # the node reached first; parents lead back to the start
            self.chain(path,0x2c);self.w32(SEARCHER+0x60,path[0] if path else 0)
            if helper=='searcher_path':
                args=[POINTS,rng.choice([0,1,length,length,length-1,length+1,max(length//2,0),64]),ids]
                if args[1]<0:args[1]=0
            else:
                capacity=rng.choice([1,2,5,8,16,33,48,length,length+1,-3,-8]) or 1
                head=rng.randrange(-2,abs(capacity)+2) if rng.random()<0.15 else rng.randrange(abs(capacity))
                tail=rng.randrange(-2,abs(capacity)+2) if rng.random()<0.15 else rng.randrange(abs(capacity))
                self.w32(RING,capacity,head,tail,POINTS+0x100)
                args=[RING,rng.getrandbits(32),ids]
        elif helper=='path_ring_extend':
            capacity=rng.choice([1,2,5,8,16,33,48,-3,-8,0x7fffffff])
            head,tail=(rng.randrange(-4,abs(capacity)+4) if rng.random()<0.2 else rng.randrange(min(abs(capacity),1000)) for _ in range(2))
            self.w32(RING,capacity,head,tail,POINTS)
            args=[rng.choice([0,1,2,length,abs(capacity),abs(capacity)+3,-1,-length,rng.getrandbits(32)])]
        elif helper=='searcher_deliver':
            path=pool[:length];self.chain(path,0x2c);self.w32(SEARCHER+0x60,path[0] if path else 0)
            # reset afterwards: mostly nothing allocated, sometimes the path's nodes
            begin=rng.choice([0,0,NODE_VECTOR])
            self.w32(SEARCHER+0x14,begin,begin+4*len(path) if begin else 0,NODE_VECTOR+0x100)
            self.w32(NODE_VECTOR,*path)
            self.w32(SEARCHER+0x24,OPEN_HASH);self.w32(SEARCHER+0x2c,CLOSED_HASH)
            self.w32(SEARCHER+0x58,rng.choice([0,1,5,16]))
            self.w32(SEARCHER+4,HEAP,HEAP+4*rng.randrange(8))
            self.map_size=(len(path)+3)//4+rng.randrange(1,4);self.front=rng.randrange(4*self.map_size)
            self.w32(CALLBACK,CALLBACK+0x10);self.w32(CALLBACK+0x10,CALLBACK_METHOD)
            m.mem_write(CALLBACK_METHOD,bytes([0xc2,4,0]))
            self.rng=random.Random(case['seed']^0x5a5a);self.deque=0
            args=[CALLBACK]
        elif helper=='searcher_run':
            args=[rng.getrandbits(32)]
            self.step_running=rng.choice([0,1,rng.getrandbits(8)]);self.step_eax=rng.getrandbits(32)
        else:
            closed=pool[:length];other=pool[length:length+rng.choice([0,0,1,4])]
            self.chain(closed,0x24);self.chain(other,0x24)
            self.w32(SEARCHER+0x28,closed[0] if closed else 0);self.w32(SEARCHER+0x20,other[0] if other else 0)
            args=[POINTS,rng.choice([0,1,length,length,length-1,length+1,length+len(other),64,-1,-0x80000000]),ids]
        sp=STACK+0x8000-4*rng.randrange(64);self.entry_sp=sp;self.w32(sp,TRAMPOLINE,*args)
        cw=case['cw'];m.mem_write(CONTROL,struct.pack('<H',cw))
        for reg,value in SAVED.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,eax);m.reg_write(UC_X86_REG_ESP,sp)
        m.reg_write(UC_X86_REG_ECX,RING if helper=='path_ring_extend' else SEARCHER)
        m.emu_start(self.preload[helper],TRAMPOLINE,timeout=20_000_000,count=2_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+OUTPUT_HELPERS[helper]
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        arena=bytes(m.mem_read(ARENA,SNAPSHOT))
        # the run wrapper's frame (time_left, the job slot holding `running`)
        slots=bytes(m.mem_read(sp-8,0x10)).hex() if helper=='searcher_run' else ''
        return dict(eax=m.reg_read(UC_X86_REG_EAX),status=m.reg_read(UC_X86_REG_FPSW)&~0x4700,log=self.log,
                    arena=hashlib.sha256(arena).hexdigest(),slots=slots)

def compare_searcher_output(original,original_pe,edited,edited_pe,spec,symbols,count=2400):
    for name in OUTPUT_HELPERS:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9,name
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name],name
    old_runner=OutputRunner(original,original_pe,spec);new_runner=OutputRunner(edited,edited_pe,spec)
    results=[]
    for case in output_cases(count):
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new)
        except Exception as error:
            raise RuntimeError(f'searcher output inputs { {k:v for k,v in case.items() if k not in ("nodes","words")} }') from error
        results.append(dict(inputs={k:v for k,v in case.items() if k not in ('nodes','words')},**old))
    return results

def compare_searcher(original,original_pe,edited,edited_pe,spec,symbols,count=2000):
    jump=edited_pe.get_data(spec['searcher_step']-0x400000,5)
    assert jump[0]==0xe9
    assert spec['searcher_step']+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_searcher_step']
    old_runner=Runner(original,original_pe,spec);new_runner=Runner(edited,edited_pe,spec)
    results=[]
    for case in cases(count):
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new)
        except Exception as error:
            raise RuntimeError(f'searcher inputs { {k:v for k,v in case.items() if k not in ("nodes","words")} }') from error
        results.append(dict(inputs={k:v for k,v in case.items() if k not in ('nodes','words')},**old))
    return results

def outcome(case):
    names=[entry[0] for entry in case['log']]
    if case['running']:return 'still_running'
    if 'is_goal' in names and case['eax']&0xff:return 'goal_found'
    return 'failed'

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
        results=compare_searcher(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.count)
        summary=dict(cases=len(results),stub_calls=sum(len(c['log']) for c in results),
                     expansions=sum(sum(1 for e in c['log'] if e[0]=='successors') for c in results),
                     relaxations=sum(sum(1 for e in c['log'] if e[0] in ('push','update')) for c in results))
        for c in results:
            key=outcome(c);summary[key]=summary.get(key,0)+1
        heap=compare_searcher_heap(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.count)
        heap_summary=dict(cases=len(heap),random_draws=sum(sum(1 for e in c['log'] if e[0]=='random') for c in heap),
                          push_backs=sum(sum(1 for e in c['log'] if e[0]=='push_back') for c in heap))
        for c in heap:heap_summary[c['inputs']['helper']]=heap_summary.get(c['inputs']['helper'],0)+1
        output=compare_searcher_output(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.count*6//5)
        output_summary=dict(cases=len(output),allocations=sum(sum(1 for e in c['log'] if e[0]=='allocate') for c in output),
                            push_backs=sum(sum(1 for e in c['log'] if e[0]=='push_back') for c in output))
        for c in output:output_summary[c['inputs']['helper']]=output_summary.get(c['inputs']['helper'],0)+1
        (work/'searcher-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(results)+len(heap)+len(output),summary=summary,heap_summary=heap_summary,output_summary=output_summary,
            cases=results,heap_cases=heap,output_cases=output),indent=2))
        print(f'{target}: {len(results)} searcher step comparisons passed {summary}',flush=True)
        print(f'{target}: {len(heap)} searcher heap helper comparisons passed {heap_summary}',flush=True)
        print(f'{target}: {len(output)} searcher alloc and path output comparisons passed {output_summary}',flush=True)
