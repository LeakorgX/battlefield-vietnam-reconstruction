"""Original/reconstructed local A* successors (vf3 and its two cell helpers).

Execute the native AStarLocalSearch successor generation (vf3), its cell
helpers (add_cell, add_cell_avoiding) and their src/astar_local.c
replacements on the same random search object, owner and node. The per-layer cell map's blocked test, the terrain's surface method
(height and normal) and the search's estimate are logging stubs that answer
from chosen tables; the bounds test and the rounding helper stay native.
Rejected cells split recursively down to the owner's default layer, so one
call may add many successors. The node being expanded may be one of the
searcher's output nodes, so the order of reads and writes shows. Each case
starts from FNINIT plus a chosen control word; the result covers the whole
arena (search, owner, output nodes and costs, stub logs), ESP, the
callee-saved registers, the FPU control and tag words, the FPU status
apart from its condition codes, and EAX for vf3 (the helpers return nothing).
vf3 cases put the node on the edges of coarser cells, so the coarse search
adds cells, and cover every goal mode.
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
SEARCH=ARENA;FROM=ARENA+0x200;CONTROL=ARENA+0x2f0;TRAMPOLINE=ARENA+0x300
BLOCK_STUB=ARENA+0x400;SURFACE_STUB=ARENA+0x480;ESTIMATE_STUB=ARENA+0x500;PRELOAD=ARENA+0x600
TERRAIN=ARENA+0x700;TERRAIN_VTABLE=ARENA+0x740;SEARCH_VTABLE=ARENA+0x7c0
CELL_MAPS=ARENA+0x800;CELL_VTABLE=ARENA+0xa00;MAPS=ARENA+0xa40;CELL_TABLE=ARENA+0xb00;LAYER_COSTS=ARENA+0xc00
LOG=ARENA+0x1000;BLOCK_COUNT=LOG;BLOCK_MASK=LOG+4;BLOCK_VALUE=LOG+8;SURFACE_COUNT=LOG+0xc;ESTIMATE_COUNT=LOG+0x10
BLOCK_LOG=ARENA+0x1100;SURFACE_LOG=ARENA+0x1500;SURFACES=ARENA+0x1600;ESTIMATES=ARENA+0x1680;ESTIMATE_LOG=ARENA+0x1700
NODE_POINTERS=ARENA+0x2000;OUT_COSTS=ARENA+0x2200;AVOID=ARENA+0x2400;NODES=ARENA+0x3000;OWNER=ARENA+0x8000
HELPERS=['astar_local_add_cell','astar_local_add_cell_avoiding']
ENTRIES=HELPERS+['astar_local_successors']

def a32(v):return struct.pack('<I',v&0xffffffff)
def block_stub():
    # Log (this, x, y, z) of the cell asked about; answer bit `count` of BLOCK_MASK, as 0 or BLOCK_VALUE.
    return (b'\x53\x8b\x1d'+a32(BLOCK_COUNT)+b'\x83\xe3\x3f\xc1\xe3\x04\x89\x8b'+a32(BLOCK_LOG)
            +b'\x8b\x54\x24\x08\x8b\x02\x89\x83'+a32(BLOCK_LOG+4)+b'\x8b\x42\x04\x89\x83'+a32(BLOCK_LOG+8)
            +b'\x8b\x42\x08\x89\x83'+a32(BLOCK_LOG+12)+b'\x8b\x0d'+a32(BLOCK_COUNT)+b'\xff\x05'+a32(BLOCK_COUNT)
            +b'\xa1'+a32(BLOCK_MASK)+b'\xd3\xe8\x83\xe0\x01\xf7\xd8\x23\x05'+a32(BLOCK_VALUE)
            +b'\xba'+a32(0x77777777)+b'\xb9'+a32(0x66666666)+b'\x5b\xc2\x04\x00')
def surface_stub():
    # Log (this, x, z); the normal pointer is a stack slot, so not logged. Write normal[3] and return the height from entry `count & 7`.
    return (b'\x53\x8b\x1d'+a32(SURFACE_COUNT)+b'\x83\xe3\x0f\xc1\xe3\x04\x89\x8b'+a32(SURFACE_LOG)
            +b'\x8b\x44\x24\x08\x89\x83'+a32(SURFACE_LOG+4)+b'\x8b\x44\x24\x0c\x89\x83'+a32(SURFACE_LOG+8)
            +b'\x8b\x54\x24\x10'
            +b'\x8b\x1d'+a32(SURFACE_COUNT)+b'\x83\xe3\x07\xc1\xe3\x04'
            +b'\x8b\x83'+a32(SURFACES)+b'\x89\x02\x8b\x83'+a32(SURFACES+4)+b'\x89\x42\x04'
            +b'\x8b\x83'+a32(SURFACES+8)+b'\x89\x42\x08\xd9\x83'+a32(SURFACES+12)
            +b'\xff\x05'+a32(SURFACE_COUNT)+b'\xb8'+a32(0x55555555)+b'\xb9'+a32(0x66666666)+b'\xba'+a32(0x77777777)
            +b'\x5b\xc2\x0c\x00')
def estimate_stub():
    # Log (this, x, z, y) of the node; return entry `count & 7` of ESTIMATES.
    return (b'\x53\x8b\x1d'+a32(ESTIMATE_COUNT)+b'\x83\xe3\x3f\xc1\xe3\x04\x89\x8b'+a32(ESTIMATE_LOG)
            +b'\x8b\x54\x24\x08\x8b\x02\x89\x83'+a32(ESTIMATE_LOG+4)+b'\x8b\x42\x08\x89\x83'+a32(ESTIMATE_LOG+8)
            +b'\x8b\x42\x04\x89\x83'+a32(ESTIMATE_LOG+12)+b'\x8b\x1d'+a32(ESTIMATE_COUNT)+b'\x83\xe3\x07'
            +b'\x8b\x04\x9d'+a32(ESTIMATES)+b'\xff\x05'+a32(ESTIMATE_COUNT)
            +b'\xb9'+a32(0x66666666)+b'\xba'+a32(0x77777777)+b'\x5b\xc2\x04\x00')

def float_bits(rng):
    return rng.choice([struct.unpack('<I',struct.pack('<f',rng.uniform(-500,500)))[0],
        struct.unpack('<I',struct.pack('<f',float(rng.randrange(-300,300))))[0],
        struct.unpack('<I',struct.pack('<f',rng.choice([0.5,1.5,2.5,-0.5,-2.5,1e9,-3e9,1e30,0.25,-1.0,1.0])))[0],
        rng.choice([0,0x80000000,0x7f800000,0xff800000,0x7fc00000,0x7f800001,0xffc00001,1,0x807fffff,0x00400000]),
        rng.getrandbits(32)])
def normal_bits(rng):
    return rng.choice([struct.unpack('<I',struct.pack('<f',rng.uniform(-1,1)))[0],
        struct.unpack('<I',struct.pack('<f',rng.choice([1.0,-1.0,0.0,0.5,0.70710677])))[0],float_bits(rng)])
def offset(rng):return rng.choice([-1,0,1,-1,0,1,-1,1,2,-2,rng.randrange(-40,40),rng.getrandbits(32)])&0xffffffff
def signed(v):return v-(1<<32) if v&0x80000000 else v

def cell_cases(count=4000):
    rng=random.Random(0xce115);cases=[]
    for index in range(count):
        name=HELPERS[index%len(HELPERS)]
        layer=rng.choice([0,0,1,2,3,4,5,6,rng.randrange(0,32),31,32,33])
        depth=rng.choice([0,0,1,2,3,4,5])
        # Splits stop at the default layer; one below the node's layer would never be reached.
        default_layer=(layer-min(depth,layer))&0xffffffff if rng.random()<0.9 else rng.choice([layer,layer+1&63])
        default_layer=min(default_layer,layer)
        world=rng.choice([(1<<k)-1 for k in (8,10,11,12,16)]+[0xffffffff,0,rng.getrandbits(32)])
        world_z=world if rng.random()<0.7 else rng.choice([(1<<k)-1 for k in (8,10,12)]+[0xffffffff])
        size=min(world,0xffff)
        def coordinate():
            return rng.choice([rng.randrange(0,size+1),rng.randrange(0,size+1),rng.choice([0,1,size,size-1&0xffffffff]),
                rng.randrange(-64,64),rng.getrandbits(32)])&0xffffffff
        out_count=rng.randrange(0,12)
        search=[rng.getrandbits(32) for _ in range(0x30)]
        search[0]=SEARCH_VTABLE;search[6]=OWNER
        low=[coordinate(),rng.getrandbits(32),coordinate()]
        search[24:27]=low;search[27:30]=[(low[0]+rng.randrange(0,600))&0xffffffff,rng.getrandbits(32),(low[2]+rng.randrange(0,600))&0xffffffff]
        search[40]=rng.choice([0,1,0x100,0x101,rng.getrandbits(32)])
        search[41]=NODE_POINTERS;search[42]=OUT_COSTS;search[43]=out_count
        areas=[]
        for _ in range(rng.choice([0,0,1,2,4])):
            areas+=[coordinate(),coordinate(),rng.choice([0,1,8,40,300,0x7fffffff,0x80000000,rng.getrandbits(32)]),
                    rng.choice([0,1,16,80,600,0x7fffffff,0x80000000,rng.getrandbits(32)])]
        search[46]=AVOID;search[47]=AVOID+4*len(areas)
        from_node=[coordinate(),float_bits(rng),coordinate(),layer]
        if areas and rng.random()<0.6:
            areas[0]=(from_node[0]+rng.randrange(-20,20))&0xffffffff;areas[1]=(from_node[2]+rng.randrange(-20,20))&0xffffffff
        dx,dz=offset(rng),offset(rng)
        if areas and rng.random()<0.5:
            # Radius or sum exactly the distance to the layer-0 neighbour, so < and <= differ.
            ax=abs(signed((areas[0]-from_node[0]-dx)&0xffffffff));az=abs(signed((areas[1]-from_node[2]-dz)&0xffffffff))
            choice=rng.randrange(3)
            areas[2]=[ax,az,max(ax,az)+1][choice]&0xffffffff
            areas[3]=(ax+az if choice==2 else 0x7fffffff)&0xffffffff
        # The expanded node: separate, the next output node (overwritten first) or a later one.
        from_at=rng.choice([FROM,FROM,FROM,NODES+0x20*out_count,NODES+0x20*(out_count+1),NODES+0x20*(out_count+2)])
        # A third of the cases open the world, the bounds and the blocked map, so cells are added and split.
        friendly=rng.random()<0.35
        if friendly:world=world_z=0xffffffff;search[40]&=~0xff
        cases.append(dict(name=name,search=search,areas=areas,from_node=from_node,from_at=from_at,
            args=[from_at,dx,dz],default_layer=default_layer,world=[world,world_z],
            block_mask=rng.choice([0,0,0,1,2,3,0x55555555,0xfffffffe]) if friendly else
                rng.choice([0,0,0xffffffff,1,2,3,0x55555555,rng.getrandbits(32),rng.getrandbits(32)]),
            block_value=rng.choice([1,1,0x100,0x80000000,0xffffffff]),
            surfaces=[[normal_bits(rng),normal_bits(rng),normal_bits(rng),
                       from_node[1] if rng.random()<0.2 else float_bits(rng)] for _ in range(8)],
            estimates=[rng.choice([0,1,5,100,0x10000000,0x7fffffff,rng.getrandbits(32)]) for _ in range(8)],
            cw=rng.choice(CONTROL_WORDS),seed=rng.getrandbits(32)))
    return cases

def successor_cases(count=2000):
    rng=random.Random(0x5ecc)
    cases=[]
    for index in range(count):
        layer=rng.choice([0,0,0,1,2,3,4,rng.randrange(0,12),31,32])
        coarse=rng.choice([layer,layer+1,layer+2,layer+3,layer+5,rng.randrange(0,34),0])&0xffffffff
        depth=rng.choice([0,0,1,2,3])
        default_layer=(layer-min(depth,layer)) if rng.random()<0.9 else rng.choice([layer,layer+1])
        friendly=rng.random()<0.5
        world=0xffffffff if friendly else rng.choice([(1<<k)-1 for k in (8,12,16)]+[0xffffffff,rng.getrandbits(32)])
        def coordinate():
            # Often on the low or high edge of a coarser cell, so the coarse search has candidates.
            base=rng.randrange(0,1<<16)
            top=rng.choice([coarse,coarse,coarse-1,layer+1,rng.randrange(0,20)])&31
            base&=~((1<<top)-1)
            edge=rng.choice([0,(1<<top)-(1<<min(layer,31)),rng.randrange(0,1<<top) if top else 0])
            return rng.choice([base+edge,base+edge,rng.randrange(-64,64),rng.getrandbits(32)])&0xffffffff
        search=[rng.getrandbits(32) for _ in range(0x30)]
        search[0]=SEARCH_VTABLE;search[6]=OWNER
        search[19]=rng.choice([0,1,5,0xffffffff,0x80000000,rng.getrandbits(32)])
        search[20]=rng.choice([0,2,7,0xffffffff,rng.getrandbits(32)])
        search[39]=rng.choice([0,1,2,1,0,2,3,4,0xffffffff])
        low=[coordinate(),rng.getrandbits(32),coordinate()]
        search[24:27]=low;search[27:30]=[(low[0]+rng.randrange(0,1<<16))&0xffffffff,rng.getrandbits(32),(low[2]+rng.randrange(0,1<<16))&0xffffffff]
        search[40]=0 if friendly and rng.random()<0.7 else rng.choice([0,1,0x100,rng.getrandbits(32)])
        from_node=[coordinate(),float_bits(rng),coordinate(),layer]
        areas=[]
        for _ in range(rng.choice([0,0,1,2,4])):
            areas+=[(from_node[0]+rng.randrange(-80,80))&0xffffffff,(from_node[2]+rng.randrange(-80,80))&0xffffffff,
                    rng.choice([0,1,8,40,300,0x7fffffff,rng.getrandbits(32)]),rng.choice([0,1,16,80,600,0x7fffffff,rng.getrandbits(32)])]
        # The node: separate, or the first, second or third output node (written first).
        from_at=rng.choice([FROM,FROM,FROM,FROM,NODES,NODES+0x20,NODES+0x40])
        cases.append(dict(name='astar_local_successors',search=search,areas=areas,from_node=from_node,from_at=from_at,
            args=[from_at,NODE_POINTERS,OUT_COSTS],default_layer=default_layer,max_layer=coarse,world=[world,world],
            block_mask=rng.choice([0,0,0,1,2,0x55555555,0xfffffffe,rng.getrandbits(32)]) if friendly else
                rng.choice([0,0xffffffff,1,0x55555555,rng.getrandbits(32),rng.getrandbits(32)]),
            block_value=rng.choice([1,1,0x100,0x80000000,0xffffffff]),
            surfaces=[[normal_bits(rng),normal_bits(rng),normal_bits(rng),
                       from_node[1] if rng.random()<0.2 else float_bits(rng)] for _ in range(8)],
            estimates=[rng.choice([0,1,5,100,0x10000000,0x7fffffff,rng.getrandbits(32)]) for _ in range(8)],
            cw=rng.choice(CONTROL_WORDS),seed=rng.getrandbits(32)))
    return cases

class Runner:
    def __init__(self,image,pe,spec):
        m=self.machine=load_machine(image,pe);self.spec=spec
        m.mem_unmap(ARENA,0x10000);m.mem_map(ARENA,ARENA_SIZE)
    def run(self,case):
        m=self.machine;spec=self.spec;rng=random.Random(case['seed'])
        def w32(p,*v):m.mem_write(p,b''.join(a32(x) for x in v))
        m.mem_write(ARENA,bytes(ARENA_SIZE))
        m.mem_write(OWNER,rng.randbytes(0x100))
        w32(OWNER+0x3c,MAPS);w32(OWNER+0x48,LAYER_COSTS);w32(OWNER+0xc508,case['default_layer'],case.get('max_layer',0))
        w32(MAPS+0x50,CELL_TABLE)
        for layer in range(64):
            w32(CELL_TABLE+4*layer,CELL_MAPS+8*layer);w32(CELL_MAPS+8*layer,CELL_VTABLE,layer)
            w32(LAYER_COSTS+4*layer,rng.choice([0,1,2,4,16,rng.randrange(0,64),rng.getrandbits(32)]))
        w32(CELL_VTABLE+0xc,BLOCK_STUB);w32(TERRAIN,TERRAIN_VTABLE);w32(TERRAIN_VTABLE+0x5c,SURFACE_STUB)
        w32(SEARCH_VTABLE+8,ESTIMATE_STUB)
        m.mem_write(TRAMPOLINE,b'\x90');m.mem_write(BLOCK_STUB,block_stub())
        m.mem_write(SURFACE_STUB,surface_stub());m.mem_write(ESTIMATE_STUB,estimate_stub())
        w32(spec['local_terrain'],TERRAIN)
        w32(spec['local_world_mask_x'],case['world'][0]);w32(spec['local_world_mask_z'],case['world'][1])
        w32(BLOCK_MASK,case['block_mask']);w32(BLOCK_VALUE,case['block_value'])
        for i,s in enumerate(case['surfaces']):w32(SURFACES+16*i,*s)
        w32(ESTIMATES,*case['estimates'])
        for i in range(128):w32(NODE_POINTERS+4*i,NODES+0x20*i);w32(NODES+0x20*i,*[rng.getrandbits(32) for _ in range(8)])
        w32(OUT_COSTS,*[rng.getrandbits(32) for _ in range(128)])
        w32(SEARCH,*case['search']);w32(AVOID,*case['areas']);w32(case['from_at'],*case['from_node'])
        for index,name in enumerate(ENTRIES):
            stub=PRELOAD+0x10*index;code=b'\xdb\xe3\xd9\x2d'+a32(CONTROL)
            m.mem_write(stub,code+b'\xe9'+a32(spec[name]-stub-len(code)-5))
        cw=case['cw'];m.mem_write(CONTROL,struct.pack('<H',cw))
        sp=STACK+0x8000-4*rng.randrange(16);w32(sp,TRAMPOLINE,*case['args'])
        for reg,value in SAVED.items():m.reg_write(reg,value)
        m.reg_write(UC_X86_REG_EAX,0x5a5a5a5a);m.reg_write(UC_X86_REG_ESP,sp);m.reg_write(UC_X86_REG_ECX,SEARCH)
        m.emu_start(PRELOAD+0x10*ENTRIES.index(case['name']),TRAMPOLINE,timeout=20_000_000,count=2_000_000)
        assert m.reg_read(UC_X86_REG_EIP)==TRAMPOLINE,'did not return'
        assert m.reg_read(UC_X86_REG_ESP)==sp+16
        for reg,value in SAVED.items():assert m.reg_read(reg)==value
        assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
        arena=bytes(m.mem_read(ARENA,ARENA_SIZE));self.last=arena
        counts=struct.unpack_from('<5I',arena,LOG-ARENA)
        return dict(eax=m.reg_read(UC_X86_REG_EAX) if case['name']=='astar_local_successors' else None,
                    status=m.reg_read(UC_X86_REG_FPSW)&~0x4700,arena=hashlib.sha256(arena).hexdigest(),
                    out_count=struct.unpack_from('<I',arena,0xac)[0],blocked_calls=counts[0],surface_calls=counts[3],
                    estimate_calls=counts[4],costs=arena[OUT_COSTS-ARENA:OUT_COSTS-ARENA+0x40].hex())

def compare_astar_local_cells(original,original_pe,edited,edited_pe,spec,symbols,count=4000,successors=2000):
    for name in ENTRIES:
        jump=edited_pe.get_data(spec[name]-0x400000,5)
        assert jump[0]==0xe9
        assert spec[name]+5+struct.unpack('<i',jump[1:])[0]==symbols['bfv_'+name]
    old_runner=Runner(original,original_pe,spec);new_runner=Runner(edited,edited_pe,spec)
    results=[]
    for case in cell_cases(count)+successor_cases(successors):
        try:
            old=old_runner.run(case);new=new_runner.run(case)
            assert old==new,(old,new,[hex(i) for i,(a,b) in enumerate(zip(old_runner.last,new_runner.last)) if a!=b][:24])
        except Exception as error:raise RuntimeError(f'local A* cell inputs {case}') from error
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
        cases=compare_astar_local_cells(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()},args.count)
        summary={}
        for c in cases:
            s=summary.setdefault(c['inputs']['name'],dict(cases=0,added=0,blocked_calls=0,surface_calls=0,nothing_added=0))
            s['cases']+=1;added=(c['out_count']-(0 if c['inputs']['name']=='astar_local_successors' else c['inputs']['search'][43]))&0xffffffff;s['added']+=added
            s['nothing_added']+=added==0;s['blocked_calls']+=c['blocked_calls'];s['surface_calls']+=c['surface_calls']
        (work/'astar-local-cells-verification.json').write_text(json.dumps(dict(
            target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            passed=len(cases),summary=summary,cases=cases),indent=2))
        print(f'{target}: {len(cases)} local A* cell comparisons passed {summary}',flush=True)
