"""Original/reconstructed strategic search setup, bit set init and search run.

Execute the native AStarStrategicSearch setup (0x9dec20 / 0x792880), bit
set init (0x9ceda0 / 0x784590) and search run (0x9cf5c0 / 0x784fb0) and their
src/astar_strategic.c replacements on the same random inputs. The callees are
stubs in both images: the grid point query, the cell point lookup (also at
its C symbol in the rebuilt image), the engine's three listener
notifications, the A* engine (which logs the disc mask's size and bits) and
operator new and delete. Each stub logs its this pointer, arguments and the
contents of the points it gets; pointers into the caller's stack frame are
logged by content only. Stubs may change the owner link, the owner's grid,
the bit count, the request's flag or the mask's words to check that the code
rereads them, and they clobber EAX, ECX and EDX. _ftol2 runs natively.
Each case starts from FNINIT plus a chosen control word; the result covers
EAX, the stub log, the whole arena, ESP, the callee-saved registers, the FPU
control and tag words and the FPU status apart from its condition codes.
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

SEARCH=ARENA;OWNERS=ARENA+0x100;POINTS=ARENA+0x300;ALTERNATE=ARENA+0x400;BITS=ARENA+0x500;REQUEST=ARENA+0x800
CONTROL=ARENA+0x600;HEAP=ARENA+0x1000;HEAP_SIZE=0x8000;TRAMPOLINE=ARENA+0x9000;PRELOAD=ARENA+0x9100
SNAPSHOT=HEAP+HEAP_SIZE-ARENA
START=POINTS;GOAL=POINTS+0x10;LOW=POINTS+0x20;HIGH=POINTS+0x30
# name: stack argument bytes
ENTRIES=dict(astar_strategic_setup=0x18,bitset_init=4,astar_strategic_run=0x14)

def f(x):return struct.unpack('<I',struct.pack('<f',x))[0]
REACH=[0,0x80000000,f(1.0),f(-1.0),f(0.2),f(0.7),f(1e9),f(-1e9),f(4.29e8),f(-4.29e8),0x7f800000,0xff800000,
       0x7fc12345,0x7f812345,1,0x807fffff,f(0.1),f(0.3),f(2.5),f(-2.5)]
RADIUS=[0,0x80000000,f(0.5),f(-0.5),f(1.0),f(1.5),f(2.0),f(-2.0),f(2.5),f(3.0),f(0.9999999),f(1.0000001),f(7.5),f(-12.25),
        f(40.0),f(64.9),0x7f800000,0xff800000,0x7fc12345,0x7f812345,0xffc00000,1,0x807fffff,f(1e10),f(-1e10),
        f(2.0**30),f(-2.0**30),f(1.5*2**30),f(1e30)]

def disc_size(radius):
    """2n + 1 as the native code computes it (n = _ftol2(|radius|), low 32 bits)."""
    value=struct.unpack('<f',struct.pack('<I',radius))[0]
    if value!=value or abs(value)>=2.0**63:n=0x80000000
    else:n=int(abs(value))&0xffffffff
    size=(2*n+1)&0xffffffff
    return size-(1<<32) if size&0x80000000 else size

def setup_cases(count):
    rng=random.Random(0x5e70);cases=[]
    for index in range(count):
        name=list(ENTRIES)[index%len(ENTRIES)]
        case=dict(name=name,cw=rng.choice(CONTROL_WORDS),words=[rng.getrandbits(32) for _ in range(0x500//4)],
                  stub_words=[rng.getrandbits(32) for _ in range(24)],
                  # 0: none; 1: point query relinks the search's owner; 2: point query changes the grid
                  perturb=rng.choice([0,0,1,2]),alternate=rng.random()<0.3,
                  eax=[rng.getrandbits(32) for _ in range(4)])
        if name=='astar_strategic_setup':
            r=rng.random()
            reach=rng.choice(REACH) if r<0.4 else f(rng.uniform(-1e3,1e3)) if r<0.7 else rng.getrandbits(32)
            case['args']=[OWNERS+rng.choice([0,0x80]),START,GOAL,reach,LOW,HIGH]
            case['ecx']=SEARCH
        elif name=='astar_strategic_run':
            while True:
                r=rng.random()
                radius=rng.choice(RADIUS) if r<0.4 else f(rng.uniform(-12,12)) if r<0.9 else rng.getrandbits(32)
                if disc_size(radius)<=129:break
            case['radius']=radius;case['flag']=rng.getrandbits(8)
            case['args']=[rng.getrandbits(32) for _ in range(5)];case['ecx']=REQUEST
            # 0: none; 1: begin notification changes the flag; 2: engine replaces the mask's words
            case['perturb']=rng.choice([0,0,1,2])
        else:
            count_bits=rng.choice([0,1,31,32,33,63,64,65,rng.randrange(0,2048),rng.randrange(0,HEAP_SIZE*8-64)])
            case['args']=[count_bits];case['ecx']=BITS
            case['new_count']=rng.choice([None,None,None,rng.randrange(0,HEAP_SIZE*8-64)])
        cases.append(case)
    return cases

class Runner:
    def __init__(self,image,pe,spec,cell_point_symbol=None):
        m=self.machine=load_machine(image,pe);self.spec=spec
        m.mem_write(TRAMPOLINE,b'\x90')
        self.preload={}
        for index,name in enumerate(ENTRIES):
            stub=PRELOAD+0x10*index;code=b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)
            code+=b'\xe9'+struct.pack('<I',(spec[name]-stub-len(code)-5)&0xffffffff)
            m.mem_write(stub,code);self.preload[name]=stub
        stubs={spec['strategic_point_query']:(self.point_query,4),spec['strategic_cell_point']:(self.cell_point,12),
               spec['vector_allocator']:(self.allocate,0),spec['vector_free']:(self.release,0),
               spec['search_notify_begin']:(self.notify('begin'),4),spec['search_notify_end']:(self.notify('end'),4),
               spec['search_notify_finish']:(self.notify('finish'),4),spec['astar_engine']:(self.engine,0x24)}
        if cell_point_symbol:stubs[cell_point_symbol]=(self.cell_point,12)
        for address,(handler,pop) in stubs.items():
            m.hook_add(UC_HOOK_CODE,self.hook(handler,pop),begin=address,end=address)
    def r32(self,p,n=1):return list(struct.unpack('<%dI'%n,self.machine.mem_read(p,4*n)))
    def w32(self,p,*v):self.machine.mem_write(p,struct.pack('<%dI'%len(v),*[x&0xffffffff for x in v]))
    def hook(self,handler,pop):
        def run(m,address,size,user):
            sp=m.reg_read(UC_X86_REG_ESP);back,*args=self.r32(sp,10)
            eax=handler(m.reg_read(UC_X86_REG_ECX),args)
            m.reg_write(UC_X86_REG_EAX,eax);m.reg_write(UC_X86_REG_ECX,0xc1c1c1c1);m.reg_write(UC_X86_REG_EDX,0xd2d2d2d2)
            m.reg_write(UC_X86_REG_ESP,sp+4+pop);m.reg_write(UC_X86_REG_EIP,back)
        return run
    def stack_pointer(self,p):return 'stack' if STACK<=p<STACK+0x10000 else p
    def point_query(self,ecx,args):
        case=self.case;p=args[0];self.log.append(('query',ecx,p,self.r32(p,3)))
        if case['perturb']==1:self.w32(SEARCH+0x18,OWNERS+0x80 if self.r32(SEARCH+0x18)[0]==OWNERS else OWNERS)
        elif case['perturb']==2:self.w32(self.r32(SEARCH+0x18)[0]+0x40,case['stub_words'][len(self.log)])
        return case['eax'][len(self.log)%4]
    def cell_point(self,ecx,args):
        out,p,layer=args[:3];self.log.append(('cell_point',ecx,self.stack_pointer(out),p,self.r32(p,3),layer))
        words=self.case['stub_words'][4+3*len(self.log):7+3*len(self.log)]
        self.w32(out,*words)
        if self.case['alternate']:self.w32(ALTERNATE,*words[::-1]);return ALTERNATE
        return out
    def notify(self,name):
        def handler(ecx,args):
            self.log.append((name,ecx,args[0]))
            if name=='begin' and self.case['perturb']==1:self.machine.mem_write(REQUEST+4,bytes([self.case['flag']^0x5a]))
            return self.case['eax'][len(self.log)%4]
        return handler
    def engine(self,ecx,args):
        mask=args[2];width,height,count,words=self.r32(mask,4)
        bits=bytes(self.machine.mem_read(words,((count>>5)+((count&31)!=0))*4)).hex()
        self.log.append(('engine',ecx,*args[:2],self.stack_pointer(mask),*args[3:],width,height,count,words,bits))
        if self.case['perturb']==2:self.w32(mask+12,HEAP+0x4000)
        return self.case['eax'][len(self.log)%4]
    def release(self,ecx,args):
        self.log.append(('delete',args[0]));return self.case['eax'][len(self.log)%4]
    def allocate(self,ecx,args):
        self.log.append(('new',args[0],self.r32(BITS,2)))
        if self.case.get('new_count') is not None:self.w32(BITS,self.case['new_count'])
        return HEAP
    def run(self,case):
        m=self.machine;self.case=case;self.log=[]
        m.mem_write(ARENA,bytes(SNAPSHOT))
        self.w32(ARENA,*case['words'])
        m.mem_write(HEAP,bytes([0xa5])*HEAP_SIZE)
        if case['name']=='astar_strategic_run':
            m.mem_write(REQUEST,bytes(range(256))+bytes(0x30));self.w32(REQUEST,case['radius'])
            m.mem_write(REQUEST+4,bytes([case['flag']]))
        sp=STACK+0x8000
        self.w32(sp,TRAMPOLINE,*case['args'])
        cw=case['cw'];m.mem_write(CONTROL,struct.pack('<H',cw))
        m.mem_write(TRAMPOLINE,b'\x90')
        for reg,value in SAVED.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0x5a5a5a5a);m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,case['ecx'])
        m.emu_start(self.preload[case['name']],TRAMPOLINE,timeout=60_000_000,count=5_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+ENTRIES[case['name']]
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        arena=bytes(m.mem_read(ARENA,SNAPSHOT))
        return dict(eax=m.reg_read(UC_X86_REG_EAX),status=m.reg_read(UC_X86_REG_FPSW)&~0x4700,log=self.log,
                    arena=hashlib.sha256(arena).hexdigest(),search=arena[:0x4c].hex())

def compare_astar_setup(original,original_pe,edited,edited_pe,spec,symbols,count=3000):
    for name in ENTRIES:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name]
    old_runner=Runner(original,original_pe,spec)
    new_runner=Runner(edited,edited_pe,spec,symbols['bfv_strategic_cell_point'])
    results=[]
    for case in setup_cases(count):
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new)
        except Exception as error:raise RuntimeError(f'strategic setup inputs {case}') from error
        results.append(dict(inputs={k:v for k,v in case.items() if k!='words'},**old))
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
        cases=compare_astar_setup(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.count)
        summary={}
        for c in cases:
            s=summary.setdefault(c['inputs']['name'],dict(cases=0,stub_calls=0))
            s['cases']+=1;s['stub_calls']+=len(c['log'])
        (work/'astar-setup-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),summary=summary,cases=cases),indent=2))
        print(f'{target}: {len(cases)} strategic setup comparisons passed {summary}',flush=True)
