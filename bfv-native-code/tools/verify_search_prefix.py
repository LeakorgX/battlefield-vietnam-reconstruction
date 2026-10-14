"""Fresh-search prefix with actual handles/tree advance/region-array lookup.

Object callbacks, allocation release, protected tree erase and region containment
are controlled. Full frame/arena memory, callback arguments/order, live preserved
registers, saved EBP layout and physical x87 state compare at both continuations.
"""
import argparse,csv,hashlib,itertools,json,struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_FPCW)
from build import GAME,PROJECT,TARGETS
from native_oracle import ARENA,STACK,load_machine
from verify_constant_returns import FLOAT_STATE
from verify_final_weights import compile_staged,bits,w,r

def run(image,pe,spec,case,staged=None,patched=False):
    m=load_machine(image,pe);early=STACK+0x8000;frame=early-4
    m.mem_write(ARENA,b'\xa5'*0x10000);m.mem_write(frame,b'\xa5'*0x240)
    names=['behavior','bot','gun','component','clock','events','body','view','query','manager','registry','region','container']
    objects={name:ARENA+0x100+i*0x200 for i,name in enumerate(names)}
    for obj in objects.values():w(m,obj,obj+0x100)
    behavior,bot,gun=[objects[n] for n in ['behavior','bot','gun']]
    w(m,gun+0x20,objects['body']);w(m,gun+0x24,objects['component'])
    w(m,gun+0x30,0x11223344);w(m,gun+0x34,0x22334455);w(m,gun+0x38,0x33445566)
    m.mem_write(behavior+0x44,bytes([case.get('dynamic',0)]));w(m,behavior+0x34,case.get('old_time',bits(0)))
    positions=ARENA+0x2000
    for j in range(3):w(m,positions+j*4,[bits(1.5),bits(2.5),bits(3.5)][j])
    targets,weapons,header,iterator=ARENA+0x2800,ARENA+0x2900,ARENA+0x2a00,ARENA+0x2b00
    w(m,behavior+8,targets);w(m,behavior+0x30,weapons)
    for j in range(4):w(m,targets+j*4,0x11110000+j);w(m,weapons+j*4,j+10)
    w(m,objects['container']+4,header);w(m,header,iterator)
    w(m,early+0xa0,behavior);w(m,early+0x1e8,bot);w(m,early+0x1f4,gun)
    w(m,spec['collision_clock'],objects['clock']);w(m,spec['collision_event_manager'],objects['events'])
    w(m,spec['artillery_prefix_context'],0x55556666);w(m,spec['artillery_prefix_query_manager'],objects['manager'])
    w(m,spec['artillery_prefix_query_seed'],0x77778888);w(m,spec['artillery_prefix_refresh_threshold'],case.get('threshold',bits(1)))
    w(m,spec['artillery_prefix_region_registry'],objects['registry'])
    region_rows=ARENA+0x3000;w(m,objects['registry']+8,0 if case.get('null_regions') else region_rows)
    w(m,objects['registry']+0xc,region_rows+16)
    for j in range(4):w(m,region_rows+j*4,objects['region'])
    pool,records=ARENA+0x3100,ARENA+0x3200;w(m,spec['pool'],pool);w(m,pool,records)
    w(m,records,gun);m.mem_write(records+6,struct.pack('<H',2))
    sentinel,a,b=ARENA+0x4000,ARENA+0x4100,ARENA+0x4200
    w(m,behavior+0x3c,sentinel)
    tree=case.get('tree','empty');w(m,sentinel,sentinel if tree=='empty' else a);m.mem_write(sentinel+0x15,b'\x01')
    for node,parent,right in [(a,sentinel,b if tree=='two' else sentinel),(b,a,sentinel)]:
        w(m,node,sentinel);w(m,node+4,parent);w(m,node+8,right);m.mem_write(node+0x15,b'\x00')
        w(m,node+0xc,0 if tree=='stale' else 0x20001);w(m,node+0x10,ARENA+0x4300)
    stubs={};cursor=ARENA+0x8000
    def stub(name,pop=0,address=None):
        nonlocal cursor
        if address is None:address=cursor;cursor+=0x40
        m.mem_write(address,b'\xc3' if not pop else b'\xc2'+struct.pack('<H',pop));stubs[address]=name;return address
    def method(obj,slot,name,pop=0):w(m,objects[obj]+0x100+slot,stub(name,pop))
    for slot,name,pop in [(0xd8,'action',0),(0xd4,'entry',0),(0xdc,'index',0),(0x84,'container',0)]:method('bot',slot,name,pop)
    method('events',0x7c,'descriptor');method('component',0x3c,'class');method('gun',0x18,'position')
    method('body',0x8c,'view',4);method('view',0x5c,'handle',4);method('manager',0x34,'query',4);method('query',0x7c,'region_index',4)
    clock_stub=stub('clock');w(m,objects['clock']+0x100+4,clock_stub)
    now=ARENA+0x9000;w(m,now,case.get('now',bits(2)))
    m.mem_write(clock_stub,b'\xd9\x05'+struct.pack('<I',now)+b'\xc3')
    stub('free',address=spec['vector_free']);stub('erase',8,spec['artillery_prefix_tree_erase']);stub('contains',4,spec['region_contains_point3'])
    values={UC_X86_REG_EAX:0x11112222,UC_X86_REG_EBX:behavior,UC_X86_REG_ECX:0x33334444,
        UC_X86_REG_EDI:0x55556666,UC_X86_REG_ESI:0x77778888,UC_X86_REG_EBP:0x99aabbcc,UC_X86_REG_ESP:early}
    for reg,val in values.items():m.reg_write(reg,val)
    m.reg_write(UC_X86_REG_FPCW,case.get('cw',0x37f));seed=ARENA+0x9100;depth=case.get('depth',0)
    for j in range(depth):w(m,seed+j*4,[bits(1.234),bits(-5.678)][j%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+j*4) for j in range(depth))
    entry=spec['artillery_search_prefix']
    if staged:
        payload,symbols=staged;m.mem_write(ARENA+0xa000,payload)
        if patched:m.mem_write(entry,b'\xe9'+struct.pack('<I',(symbols['bfv_artillery_search_prefix_bridge']-entry-5)&0xffffffff))
    start=ARENA+0x8800;m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    calls=[];exits=[];indices=0;containers=0
    def hook(uc,address,size,unused):
        nonlocal indices,containers
        if address in (spec['artillery_search_prefix_continue'],spec['artillery_after_first_pass']):
            exits.append('filter' if address==spec['artillery_search_prefix_continue'] else 'empty');uc.emu_stop();return
        name=stubs.get(address)
        if name is None:return
        sp=uc.reg_read(UC_X86_REG_ESP);receiver=uc.reg_read(UC_X86_REG_ECX)
        count={'view':1,'handle':1,'query':1,'region_index':1,'contains':1,'erase':2,'free':1}.get(name,0)
        args=[r(uc,sp+4+j*4) for j in range(count)]
        if name=='erase':assert receiver==behavior+0x38 and args==[early+0xa4,a]
        elif name=='view':assert receiver==objects['body'] and args==[frame+0x1bc]
        elif name=='contains':assert receiver==objects['region'] and args==[frame+0x128]
        elif name=='query':assert receiver==objects['manager'] and args==[0x123]
        elif name=='handle':assert receiver==objects['view'] and args==[r(uc,frame+0xd4)]
        calls.append(dict(name=name,args=args))
        result={'action':0xabc00001,'entry':0x123,'descriptor':0xabc00002,'class':2,'position':positions,
            'view':objects['view'],'handle':0x20001,'query':objects['query'],'region_index':case.get('region_index',0),'contains':case.get('inside',1)}.get(name)
        if result is not None:uc.reg_write(UC_X86_REG_EAX,result)
        if name=='index':
            sequence=case.get('indices',[0]);uc.reg_write(UC_X86_REG_EAX,sequence[indices%len(sequence)]);indices+=1
        elif name=='container':
            containers+=1
            if containers==2 and case.get('empty'):w(uc,objects['container']+4,iterator)
            uc.reg_write(UC_X86_REG_EAX,objects['container'])
        elif name=='erase':
            destination=ARENA+0x4400 if case.get('erase_alias') else args[0]
            w(uc,destination,sentinel);uc.reg_write(UC_X86_REG_EAX,destination)
        elif name=='clock' and case.get('mutation')=='dynamic':m.mem_write(behavior+0x44,b'\x01')
        elif name=='descriptor' and case.get('mutation')=='clock':w(uc,early+0x54,bits(.5))
        elif name=='position' and case.get('mutation')=='position_alias':uc.reg_write(UC_X86_REG_EAX,frame+0x124)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1 and m.reg_read(UC_X86_REG_ESP)==frame
    assert r(m,frame)==values[UC_X86_REG_EBP]
    return dict(frame=bytes(m.mem_read(frame,0x240)).hex(),arena=bytes(m.mem_read(ARENA,0x10000)).hex(),
        regs=[m.reg_read(reg) for reg in [UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP]],
        fpu=[m.reg_read(reg) for reg in FLOAT_STATE],calls=calls,exit=exits[0])

def cases():
    base=[{},dict(empty=True),dict(dynamic=1),dict(tree='valid'),dict(tree='two'),dict(tree='stale'),dict(tree='stale',erase_alias=True),dict(now=bits(.5))]
    out=base+[dict(region_index=i) for i in [1,3,4,0xffffffff,0x80000000]]
    out += [dict(inside=i) for i in [0,0x100,0x101,0xffffffff]]+[dict(null_regions=True),dict(indices=[1,2,3])]
    out += [dict(mutation=v) for v in ['dynamic','clock','position_alias']]
    out += [dict(now=v) for v in [0,1,0x80000000,0x7f800000,0xff800000,0x7fc12345]]
    out += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(base[:4],[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return out

def compare(original,edited,spec,staged=None,symbols=None):
    op=pefile.PE(data=original);ep=pefile.PE(data=edited)
    entry=spec['artillery_search_prefix'];guard=b'\x8b\x0d'+struct.pack('<I',spec['collision_clock'])
    assert op.get_data(entry-0x400000,len(guard))==guard
    if staged is None:
        patch=ep.get_data(entry-0x400000,len(guard))
        assert patch[0]==0xe9 and patch[5:]==b'\x90'
        assert symbols is not None and entry+5+struct.unpack('<i',patch[1:5])[0]==symbols['bfv_artillery_search_prefix_bridge']
    results=[]
    for i,case in enumerate(cases()):
        old=run(original,op,spec,case,staged);new=run(edited,ep,spec,case,staged,True)
        differences=[k for k in old if old[k]!=new[k]]
        assert not differences,(i,case,differences,{k:(old[k],new[k]) for k in differences if k not in ('frame','arena')})
        results.append(dict(inputs=case,calls=old['calls'],exit=old['exit']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME);p.add_argument('--target',choices=['client','server','both'],default='both');p.add_argument('--staged',action='store_true');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text());original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        with (PROJECT.parent/'reports'/target/'artillery-search-prefix-audit.tsv').open() as f:audits=list(csv.DictReader(f,delimiter='\t'))
        assert len(audits)==1 and audits[0]['status']=='eligible' and audits[0]['original_sha256']==spec['sha']
        staged=compile_staged(target,'artillery_search_prefix','bfv_artillery_search_prefix_bridge','search-prefix-staged') if a.staged else None
        results=compare(original,edited,spec,staged,{k:int(v,16) for k,v in manifest['symbols'].items()})
        destination=work/('search-prefix-staged/verification.json' if a.staged else 'search-prefix-verification.json')
        destination.write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],staged=bool(staged),
            source_sha256=hashlib.sha256((PROJECT/'src/artillery_search_prefix.c').read_bytes()).hexdigest(),passed=len(results),cases=results),indent=2)+'\n')
        print(target,len(results),'fresh-search prefix comparisons passed','staged' if staged else 'installed',flush=True)
