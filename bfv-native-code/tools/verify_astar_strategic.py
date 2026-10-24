"""Original/reconstructed strategic A* neighbour expansion and grid queries.

Execute the native AStarStrategicSearch methods and strategic-grid queries
and their src/astar_strategic.c replacements on the same random grid, search
object and points. The owner's indexed object (a native call) returns a stub
whose slot 3 records its arguments and can change the point it is given. Each
case starts from FNINIT plus a chosen control word; the result covers EAX, the
whole arena (grid, search, outputs, stub log), the grid masks, ESP, the
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

ARENA_SIZE=0x40000
SEARCH=ARENA;INDEXED=ARENA+0x200;TABLE=ARENA+0x280;OBJECT=ARENA+0x2c0;VTABLE=ARENA+0x2e0
VISIT=ARENA+0x3c0;MUTATE=ARENA+0x3f0;COUNT=ARENA+0x3f4;LAST_ECX=ARENA+0x3f8;LAST_ARG=ARENA+0x3fc
GRID=ARENA+0x400;CONTROL=ARENA+0x4f0;TRAMPOLINE=ARENA+0x500;PRELOAD=ARENA+0x600
POINT_A=ARENA+0x800;POINT_B=ARENA+0x840;OUT=ARENA+0x880;ESTIMATE=ARENA+0x8c0
NODES=ARENA+0x1000;NODE_BUFFERS=ARENA+0x1100;COSTS=ARENA+0x1400
OWNER=ARENA+0x8000;CELLS=ARENA+0x30000
POINTERS=dict(a=POINT_A,b=POINT_B,out=OUT,estimate=ESTIMATE,nodes=NODES,costs=COSTS,
              start=SEARCH+0x34,goal=SEARCH+0x40,low=SEARCH+0x1c,high=SEARCH+0x28)
# name: (ecx, stack argument bytes)
ENTRIES=dict(strategic_same_cell=('grid',8),strategic_cell_point=('grid',12),
    strategic_link_z=('grid',12),strategic_link_x=('grid',12),
    strategic_link_back_z=('grid',20),strategic_link_back_x=('grid',20),
    astar_strategic_start=('search',4),astar_strategic_goal=('search',4),
    astar_strategic_estimate=('search',4),astar_strategic_reached=('search',4),
    astar_strategic_within=('a',8),astar_strategic_step=('search',16),astar_strategic_expand=('search',12))

def visit_stub():
    a=lambda v:struct.pack('<I',v)
    return (b'\x8b\x44\x24\x04'+b'\xa3'+a(LAST_ARG)+b'\x89\x0d'+a(LAST_ECX)+b'\x8b\x08'
            +b'\x33\x0d'+a(MUTATE)+b'\x89\x08'+b'\xff\x05'+a(COUNT)+b'\xb8'+a(0x77777777)+b'\xc2\x04\x00')

def coordinate(rng):
    return rng.choice([rng.randrange(-128,1152),rng.randrange(0,1024),rng.randrange(0,16)*64+rng.choice([0,63]),
                       rng.randrange(-128,1152)])&0xffffffff
def layer(rng):return rng.choice([0,1,2,3,0,1,2,3,rng.randrange(0,8),32,33])
def point(rng):return [coordinate(rng),layer(rng),coordinate(rng)]
def near(rng,p,spread=96):
    return [(p[0]+rng.randrange(-spread,spread))&0xffffffff,rng.choice([p[1],layer(rng)]),(p[2]+rng.randrange(-spread,spread))&0xffffffff]

def strategic_cases(count=4000):
    rng=random.Random(0x5a5a);cases=[]
    for index in range(count):
        name=list(ENTRIES)[index%len(ENTRIES)]
        goal=point(rng)
        low=[rng.choice([0,64,rng.randrange(-64,512)])&0xffffffff,rng.getrandbits(32),rng.choice([0,64,rng.randrange(-64,512)])&0xffffffff]
        high=[rng.choice([1024,960,low[0],rng.randrange(256,1152)])&0xffffffff,rng.getrandbits(32),
              rng.choice([1024,960,low[2],rng.randrange(256,1152)])&0xffffffff]
        search=[rng.getrandbits(32) for _ in range(20)]
        search[1]=rng.choice([0,32,64,128,256,0xffffffff,0x7fffffff])
        search[2]=rng.choice([0x7fffffff,1000000,0xffffffff,0,64,128,0x01000001,rng.getrandbits(32)])
        search[6]=OWNER;search[7:10]=low;search[10:13]=high;search[13:16]=point(rng);search[16:19]=goal
        a=near(rng,goal) if rng.random()<0.3 else point(rng)
        b=list(goal) if rng.random()<0.4 else near(rng,a,64)
        case=dict(name=name,search=search,a=a,b=b,seed=rng.getrandbits(32),
            row_shift=rng.choice([4,4,4,3,0x24]),index_slot=rng.randrange(0,4),
            x_mask=rng.choice([0x3ff,0x3ff,0x1ff,0xffffffff,0]),z_mask=rng.choice([0x3ff,0x3ff,0x1ff,0xffffffff,0]),
            mutate=rng.choice([0,0,0x40,0x80000000]),cw=rng.choice(CONTROL_WORDS),
            density=rng.choice([0.5,0.9,1.0]))
        if name=='strategic_same_cell':args=['a','b']
        elif name=='strategic_cell_point':args=[rng.choice(['out','out','a']),'a',layer(rng)]
        elif name in ['strategic_link_z','strategic_link_x']:args=['a',layer(rng),layer(rng)]
        elif name in ['strategic_link_back_z','strategic_link_back_x']:args=[*a,layer(rng),layer(rng)]
        elif name in ['astar_strategic_start','astar_strategic_goal']:args=[rng.choice(['out','goal','start'])]
        elif name in ['astar_strategic_estimate','astar_strategic_reached']:args=['a']
        elif name=='astar_strategic_within':args=[rng.choice(['low','b']),rng.choice(['high','out'])];case['out']=near(rng,a,256)
        elif name=='astar_strategic_step':
            args=['a',rng.choice([*range(16),*range(16),16,17,0x100,0xffffffff,0x7fffffff,0x80000003]),'out','estimate']
        else:
            args=['a','nodes','costs'];case['shared_node']=rng.random()<0.1
        case['args']=args;cases.append(case)
    return cases

class Runner:
    def __init__(self,image,pe,spec):
        m=self.machine=load_machine(image,pe);self.spec=spec
        m.mem_unmap(ARENA,0x10000);m.mem_map(ARENA,ARENA_SIZE)
        m.mem_write(TRAMPOLINE,b'\x90')
        self.preload={}
        for index,name in enumerate(ENTRIES):
            stub=PRELOAD+0x10*index;code=b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)
            code+=b'\xe9'+struct.pack('<I',(spec[name]-stub-len(code)-5)&0xffffffff)
            m.mem_write(stub,code);self.preload[name]=stub
        self.masks=(spec['strategic_x_mask'],spec['strategic_z_mask'])
    def run(self,case):
        m=self.machine;spec=self.spec;rng=random.Random(case['seed'])
        def w32(p,*v):m.mem_write(p,struct.pack('<%dI'%len(v),*[x&0xffffffff for x in v]))
        code=visit_stub()
        m.mem_write(ARENA,bytes(ARENA_SIZE))
        m.mem_write(PRELOAD,b''.join(b'\xdb\xe3\xd9\x2d'+struct.pack('<I',CONTROL)
            +b'\xe9'+struct.pack('<I',(spec[n]-(PRELOAD+0x10*i)-13)&0xffffffff)+b'\x90'*3 for i,n in enumerate(ENTRIES)))
        m.mem_write(TRAMPOLINE,b'\x90');m.mem_write(VISIT,code)
        # Points reach cell indices -34..306 (x, z in -128..1151, row shift 3 or 4).
        cells=bytearray(rng.randbytes(0x3000))
        for i in range(0,len(cells),16):
            if rng.random()>case['density']:cells[i:i+4]=bytes(4)
        m.mem_write(CELLS-0x800,bytes(cells))
        w32(SEARCH,*case['search']);w32(GRID+0x1c,CELLS);w32(GRID+0x28,case['row_shift'])
        w32(OWNER+0x40,GRID);w32(OWNER+0x3c,INDEXED);w32(OWNER+0xc508,case['index_slot'])
        w32(INDEXED+0x50,TABLE);w32(TABLE+4*case['index_slot'],OBJECT);w32(OBJECT,VTABLE);w32(VTABLE+0xc,VISIT)
        w32(MUTATE,case['mutate'])
        w32(POINT_A,*case['a']);w32(POINT_B,*case['b'])
        if 'out' in case:w32(OUT,*case['out'])
        else:w32(OUT,*[rng.getrandbits(32) for _ in range(4)])
        w32(ESTIMATE,0xdeadbeef)
        nodes=[NODE_BUFFERS+0x20*i for i in range(17)]
        if case.get('shared_node'):nodes[3]=nodes[2]
        w32(NODES,*nodes);w32(NODE_BUFFERS,*[rng.getrandbits(32) for _ in range(17*8)])
        w32(COSTS,*[0xcdcdcdcd]*17)
        w32(self.masks[0],case['x_mask']);w32(self.masks[1],case['z_mask'])
        argument=[POINTERS[x] if isinstance(x,str) else x for x in case['args']]
        sp=STACK+0x8000;w32(sp,TRAMPOLINE,*argument)
        cw=case['cw'];m.mem_write(CONTROL,struct.pack('<H',cw))
        for reg,value in SAVED.items():m.reg_write(reg,value)
        ecx=dict(grid=GRID,search=SEARCH,a=POINT_A)[ENTRIES[case['name']][0]]
        m.reg_write(UC_X86_REG_EAX,0x5a5a5a5a);m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,ecx)
        m.emu_start(self.preload[case['name']],TRAMPOLINE,timeout=10_000_000,count=1_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+4+ENTRIES[case['name']][1]
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        arena=bytes(m.mem_read(ARENA,ARENA_SIZE))
        # The preload stubs are the same on both sides; the arena below them is compared whole.
        return dict(eax=m.reg_read(UC_X86_REG_EAX),status=m.reg_read(UC_X86_REG_FPSW)&~0x4700,
                    arena=hashlib.sha256(arena).hexdigest(),masks=bytes(m.mem_read(self.masks[0],4)+m.mem_read(self.masks[1],4)).hex(),
                    search=arena[:0x50].hex(),out=arena[OUT-ARENA:OUT-ARENA+16].hex(),
                    estimate=arena[ESTIMATE-ARENA:ESTIMATE-ARENA+4].hex(),visits=struct.unpack_from('<I',arena,COUNT-ARENA)[0])

def compare_astar_strategic(original,original_pe,edited,edited_pe,spec,symbols,count=4000):
    for name in ENTRIES:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name]
    old_runner=Runner(original,original_pe,spec);new_runner=Runner(edited,edited_pe,spec)
    results=[]
    for case in strategic_cases(count):
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new)
        except Exception as error:raise RuntimeError(f'strategic A* inputs {case}') from error
        results.append(dict(inputs=case,**old))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both')
    p.add_argument('--count',type=int,default=4000)
    p.add_argument('--work',help='build work directory name, e.g. {target}-custom for --output builds');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];work=PROJECT/'build'/(args.work.replace('{target}',target) if args.work else target)
        manifest=json.loads((work/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_astar_strategic(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.count)
        summary={}
        for c in cases:
            s=summary.setdefault(c['inputs']['name'],dict(cases=0,nonzero=0,visits=0))
            s['cases']+=1;s['nonzero']+=c['eax']!=0;s['visits']+=c['visits']
        (work/'astar-strategic-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),summary=summary,cases=cases),indent=2))
        print(f'{target}: {len(cases)} strategic A* comparisons passed {summary}',flush=True)
