"""Original/reconstructed local A* search estimate, goal test and end points.

Execute the native AStarLocalSearch methods (estimate, reached, set start,
set goal) and their src/astar_local.c replacements on the same random search
object and node. The layer lookup (a fastcall into the owner's map) is
replaced on both sides by a stub that logs its arguments and returns a chosen
layer; the terrain object's height method is a stub that logs its float
arguments and returns a chosen float. The rounding helper, ftol2 and the
bounds test run natively. Nodes may overlap the search object, so the order
of field reads and writes shows. Each case starts from FNINIT plus a chosen
control word; the result covers EAX, the whole arena (search, node, owner,
stub logs), ESP, the callee-saved registers, the FPU control and tag words and
the FPU status apart from its condition codes.
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
SEARCH=ARENA;NODE=ARENA+0x200;CONTROL=ARENA+0x2f0;TRAMPOLINE=ARENA+0x300
LAYER_STUB=ARENA+0x340;HEIGHT_STUB=ARENA+0x380;TERRAIN=ARENA+0x3c0;VTABLE=ARENA+0x400
LOG=ARENA+0x480;LAYER_VALUE=LOG;LAYER_ECX=LOG+4;LAYER_EDX=LOG+8;LAYER_COUNT=LOG+12
HEIGHT_VALUE=LOG+0x10;HEIGHT_ECX=LOG+0x14;HEIGHT_X=LOG+0x18;HEIGHT_Z=LOG+0x1c;HEIGHT_COUNT=LOG+0x20
PRELOAD=ARENA+0x600;OWNER=ARENA+0x8000
ENTRIES=['astar_local_estimate','astar_local_reached','astar_local_set_start','astar_local_set_goal']
# Node placements: a separate node, or one overlapping the search object's points and goal cell
# (never the owner at +0x18, which set goal dereferences).
NODES=[NODE]*6+[SEARCH+o for o in (0x8,0x1c,0x20,0x28,0x2c,0x34,0x38,0x40)]

def a32(v):return struct.pack('<I',v&0xffffffff)
def layer_stub():
    return (b'\x89\x0d'+a32(LAYER_ECX)+b'\x89\x15'+a32(LAYER_EDX)+b'\xff\x05'+a32(LAYER_COUNT)
            +b'\xa1'+a32(LAYER_VALUE)+b'\xb9'+a32(0x33333333)+b'\xba'+a32(0x44444444)+b'\xc3')
def height_stub():
    return (b'\x89\x0d'+a32(HEIGHT_ECX)+b'\x8b\x44\x24\x04\xa3'+a32(HEIGHT_X)+b'\x8b\x44\x24\x08\xa3'+a32(HEIGHT_Z)
            +b'\xff\x05'+a32(HEIGHT_COUNT)+b'\xd9\x05'+a32(HEIGHT_VALUE)+b'\xb8'+a32(0x55555555)
            +b'\xb9'+a32(0x66666666)+b'\xba'+a32(0x77777777)+b'\xc2\x08\x00')

def coordinate(rng):
    return rng.choice([rng.randrange(-2048,2048),rng.randrange(0,1024),rng.randrange(-4,4)*256+rng.choice([0,1,255]),
        rng.choice([0x7fffffff,0x80000000,0x80000001,0x1000001,-0x1000001,0x7ffffff3]),rng.getrandbits(32)])&0xffffffff
def float_bits(rng):
    return rng.choice([struct.unpack('<I',struct.pack('<f',rng.uniform(-500,500)))[0],
        struct.unpack('<I',struct.pack('<f',rng.choice([0.5,1.5,2.5,-0.5,-2.5,1e9,-3e9,1e30])))[0],
        rng.choice([0,0x80000000,0x7f800000,0xff800000,0x7fc00000,0x7f800001,0xffc00001,1,0x807fffff,0x00400000]),
        rng.getrandbits(32)])
def layer(rng):return rng.choice([0,1,2,3,4,5,8,rng.randrange(0,8),31,32,33,0xffffffff,rng.getrandbits(32)])
def near(rng,p,spread):return (p+rng.randrange(-spread,spread+1))&0xffffffff

def local_cases(count=4000):
    rng=random.Random(0x10ca1);cases=[]
    for index in range(count):
        name=ENTRIES[index%len(ENTRIES)]
        search=[rng.getrandbits(32) for _ in range(0x28)]
        search[1]=rng.choice([0,1,16,64,256,0x7fffffff,0xffffffff,rng.getrandbits(32)])
        search[6]=OWNER
        for base in (7,10,13):search[base:base+3]=[coordinate(rng),float_bits(rng),coordinate(rng)]
        if rng.random()<0.5:search[13]=near(rng,search[10],8);search[15]=near(rng,search[12],8)
        cell_layer=layer(rng);mask=~((1<<(cell_layer&31))-1)
        if rng.random()<0.6:search[16]=search[13]&mask;search[18]=search[15]&mask
        search[19]=rng.choice([0xffffffff,0x80000000,0xfffffffb,0,2,5,rng.getrandbits(32)])
        search[20]=rng.choice([0xffffffff,0x80000000,0xfffffffb,0,2,5,rng.getrandbits(32)])
        search[23]=rng.choice([0,16,64,128,0x7fffffff,0xffffffff,rng.getrandbits(32)])
        low=[coordinate(rng),rng.getrandbits(32),coordinate(rng)]
        search[24:27]=low;search[27:30]=[near(rng,low[0],512),rng.getrandbits(32),near(rng,low[2],512)]
        search[39]=rng.choice([0,1,2,3,0,1,2,3,4,0x100,0xffffffff,rng.getrandbits(32)])
        target=search[13],search[15]
        node=[near(rng,target[0],rng.choice([0,4,32,300]))if rng.random()<0.7 else coordinate(rng),float_bits(rng),
              near(rng,target[1],rng.choice([0,4,32,300]))if rng.random()<0.7 else coordinate(rng),
              rng.choice([cell_layer,cell_layer,layer(rng),0])]
        if search[39]&0xffffffff==3 and rng.random()<0.5:node[0]=near(rng,low[0],300);node[2]=near(rng,low[2],300)
        # Equal and whole-number heights (an exact height difference; rounding has nothing to round).
        if rng.random()<0.2:node[1]=search[11]
        elif rng.random()<0.25:
            whole=lambda:struct.unpack('<I',struct.pack('<f',rng.randrange(-500,500)))[0]
            search[11]=whole();node[1]=whole()
        cases.append(dict(name=name,search=search,node=node,node_at=rng.choice(NODES),
            layer=rng.choice([0xffffffff,0xffffffff,0,1,3,5,cell_layer,rng.getrandbits(32)]),
            default_layer=layer(rng),height=float_bits(rng),cw=rng.choice(CONTROL_WORDS),seed=rng.getrandbits(32)))
    return cases

class Runner:
    def __init__(self,image,pe,spec):
        m=self.machine=load_machine(image,pe);self.spec=spec
        m.mem_unmap(ARENA,0x10000);m.mem_map(ARENA,ARENA_SIZE)
        # The layer lookup is replaced on both sides (the native and the C call the same address).
        m.mem_write(spec['local_layer_at'],layer_stub())
    def run(self,case):
        m=self.machine;spec=self.spec;rng=random.Random(case['seed'])
        def w32(p,*v):m.mem_write(p,b''.join(a32(x) for x in v))
        m.mem_write(ARENA,bytes(ARENA_SIZE))
        m.mem_write(OWNER,rng.randbytes(0x100));w32(OWNER+0xc508,case['default_layer'])
        m.mem_write(TRAMPOLINE,b'\x90');m.mem_write(HEIGHT_STUB,height_stub())
        w32(spec['local_terrain'],TERRAIN);w32(TERRAIN,VTABLE);w32(VTABLE+0x4c,HEIGHT_STUB)
        w32(LAYER_VALUE,case['layer']);w32(HEIGHT_VALUE,case['height'])
        w32(NODE-0x10,*[rng.getrandbits(32) for _ in range(12)])
        w32(SEARCH,*case['search']);w32(case['node_at'],*case['node'])
        # One preload stub per entry, the same bytes every case: Unicorn keeps code it has
        # translated, so a stub rewritten to jump elsewhere would still jump to the old entry.
        for index,name in enumerate(ENTRIES):
            stub=PRELOAD+0x10*index;code=b'\xdb\xe3\xd9\x2d'+a32(CONTROL)
            m.mem_write(stub,code+b'\xe9'+a32(spec[name]-stub-len(code)-5))
        cw=case['cw'];m.mem_write(CONTROL,struct.pack('<H',cw))
        sp=STACK+0x8000-4*rng.randrange(16);w32(sp,TRAMPOLINE,case['node_at'])
        for reg,value in SAVED.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0x5a5a5a5a);m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,SEARCH)
        m.emu_start(PRELOAD+0x10*ENTRIES.index(case['name']),TRAMPOLINE,timeout=10_000_000,count=1_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+8
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        arena=bytes(m.mem_read(ARENA,ARENA_SIZE))
        return dict(eax=m.reg_read(UC_X86_REG_EAX),status=m.reg_read(UC_X86_REG_FPSW)&~0x4700,
                    arena=hashlib.sha256(arena).hexdigest(),search=arena[:0xa0].hex(),
                    node=bytes(m.mem_read(case['node_at'],16)).hex(),log=arena[LOG-ARENA:LOG-ARENA+0x24].hex(),
                    terrain=bytes(m.mem_read(spec['local_terrain'],4)).hex())

def compare_astar_local(original,original_pe,edited,edited_pe,spec,symbols,count=4000):
    for name in ENTRIES:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name]
    old_runner=Runner(original,original_pe,spec);new_runner=Runner(edited,edited_pe,spec)
    results=[]
    for case in local_cases(count):
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new)
        except Exception as error:raise RuntimeError(f'local A* inputs {case}') from error
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
        cases=compare_astar_local(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.count)
        summary={}
        for c in cases:
            s=summary.setdefault(c['inputs']['name'],dict(cases=0,nonzero_al=0,layer_lookups=0,heights=0))
            s['cases']+=1;s['nonzero_al']+=(c['eax']&0xff)!=0
            log=bytes.fromhex(c['log']);s['layer_lookups']+=struct.unpack_from('<I',log,12)[0]
            s['heights']+=struct.unpack_from('<I',log,0x20)[0]
        (work/'astar-local-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),summary=summary,cases=cases),indent=2))
        print(f'{target}: {len(cases)} local A* comparisons passed {summary}',flush=True)
