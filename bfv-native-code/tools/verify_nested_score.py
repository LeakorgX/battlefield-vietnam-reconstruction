"""Differential fixtures for alternate linked-object score accumulation.

Object methods remain controlled; list search, target traversal wrapper, pool
lookup and ordered x87 arithmetic execute real original/source instructions.
"""
import itertools,struct
import argparse,hashlib,json
from pathlib import Path
import pefile
from build import TARGETS,PROJECT,GAME
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP)
from native_oracle import ARENA
from verify_category_score import setup
from verify_movement_score import bits,state_result
from verify_constant_returns import FLOAT_STATE

def run_nested(image,pe,spec,case,helper_entry=None):
    m,frame,w,r,words,preload=setup(image,pe,case)
    # The missing-owner wrapper returns -1; its 65535 pool record must be mapped
    # so the native generation mismatch is observed rather than a fixture fault.
    m.mem_map(ARENA+0x10000,0x80000)
    bot,base,descriptor=[ARENA+n for n in (0x100,0x300,0x600)]
    if case.get('descriptor_alias'):descriptor=frame+0x58
    objects=[ARENA+0x800+i*0x100 for i in range(3)]
    components=[ARENA+0xc00+i*0x100 for i in range(3)]
    receiver,owner,pool,records=[ARENA+n for n in (0x1000,0x1200,0x1400,0x1500)]
    vectors=[ARENA+0x1800+i*0x100 for i in range(4)]
    sentinels=[ARENA+0x1c00+i*0x40 for i in range(4)]
    nodes=[ARENA+0x2000+i*0x40 for i in range(3)]
    ratings_owner,ratings,weapon,factor,component_ratings=[ARENA+n for n in (0x2200,0x2400,0x2600,0x2800,0x2a00)]
    names=['eligible','enabled','list','factor','next','resolve']
    stubs={name:ARENA+0x8000+i*0x100 for i,name in enumerate(names)}
    for name,stub in stubs.items():m.mem_write(stub,b'\xc2\x04\x00' if name in ['eligible','list','next','resolve'] else b'\xc3')
    w(bot,bot+0x80);w(base+0x20,receiver);w(receiver,receiver+0x80);w(owner,owner+0x80)
    for slot,name in [(0x70,'eligible'),(0x6c,'enabled'),(0x4c,'list'),(0xe0,'factor')]:w(bot+0x80+slot,stubs[name])
    w(receiver+0x80+0x84,stubs['next']);w(owner+0x80+0x5c,stubs['resolve'])
    w(spec['pool'],pool);w(pool,records)
    for i,(obj,component) in enumerate(zip(objects,components)):
        words(records+i*8,[obj,0x00010000]);w(obj+0x30,component);w(component+8,component_ratings+i*0x20)
        w(obj+0x14,case.get('target_weight',bits(1.25)))
        words(component_ratings+i*0x20,case.get('component_ratings',[bits(1+i),bits(2+i),bits(3+i),bits(4+i)]))
    m.mem_write(descriptor+4,bytes([case.get('category',7)]))
    key=0x10001
    for i,node in enumerate(nodes):words(node,[nodes[i+1] if i<2 else sentinels[0],0,key if i==case.get('match',1) else i+1])
    for vector,sentinel in zip(vectors,sentinels):words(vector,[0,sentinel,case.get('count',3)]);w(sentinel,nodes[0])
    if case.get('empty'):w(sentinels[0],sentinels[0])
    w(ratings_owner+8,ratings);words(ratings,case.get('ratings',[bits(1),bits(2),bits(3),bits(4)]))
    words(weapon,case.get('weapon',[bits(1),bits(.5),bits(.25),bits(.125)]));w(factor+8,case.get('factor',bits(.75)))
    for offset,value in [(0x54,key),(0x70,0x12345678),(0x94,case.get('class',1)),(0xd0,case.get('rating_index',2)),
        (0xd8,ratings_owner),(0xf0,weapon),(0xa0,0xabcdef01),(0x44,case.get('index',0)),
        (0x14,case.get('base_score',bits(.2))),(0x5c,case.get('sum',bits(.5))),(0x98,case.get('region_score',bits(1.3)))]:w(frame+offset,value)
    saved={UC_X86_REG_EBX:descriptor,UC_X86_REG_ESI:bot,UC_X86_REG_EBP:base,UC_X86_REG_ESP:frame}
    for reg,value in {**saved,UC_X86_REG_EDI:objects[0]}.items():m.reg_write(reg,value)
    entry,start=spec['artillery_alternate_accept'],ARENA+0x9000
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];calls=[];exits=[];counts={name:0 for name in names};resolved=case.get('resolved',[0]);argument_slots=[]
    if helper_entry is None:helper_entry=spec['artillery_next_target']
    def hook(uc,address,size,unused):
        if address==helper_entry:
            # Both helpers receive the second argument at entry ESP+8. Their
            # saved registers/local frames before the virtual call can differ.
            argument_slots.append(uc.reg_read(UC_X86_REG_ESP)+8)
        if address==entry and not before:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==spec['artillery_nested_finish']:exits.append(True);uc.emu_stop();return
        if address not in stubs.values():return
        name=next(n for n,p in stubs.items() if p==address);number=counts[name];counts[name]+=1
        ecx,sp=uc.reg_read(UC_X86_REG_ECX),uc.reg_read(UC_X86_REG_ESP);mutation=case.get('mutation')
        if name in ['eligible','list']:
            expected=(case.get('sum',bits(.5))&255) if name=='eligible' and number==0 and case.get('descriptor_alias') else m.mem_read(descriptor+4,1)[0]
            arg=r(sp+4);assert ecx==bot and arg==expected;calls.append((name,arg))
        else:calls.append(name)
        if name=='eligible':
            uc.reg_write(UC_X86_REG_EAX,case.get('eligible',1))
            if mutation=='descriptor':m.mem_write(descriptor+4,b'\xff')
        elif name=='enabled':
            assert ecx==bot;uc.reg_write(UC_X86_REG_EAX,case.get('enabled',1))
            if mutation=='index':w(frame+0x44,2)
        elif name=='list':
            which=case.get('lists',[0,0,0,0])[number%4];uc.reg_write(UC_X86_REG_EAX,vectors[which])
            calls[-1]+=(which,)
            if mutation=='handle' and number%4==3:w(frame+0x54,0x12345678)
            if mutation=='wrap' and number%4==1:w(frame+0x44,0xffffffff)
        elif name=='factor':
            assert ecx==bot;uc.reg_write(UC_X86_REG_EAX,factor)
            if mutation=='math':w(frame+0x94,3);w(frame+0xd0,1);w(frame+0x98,bits(-.5))
        elif name=='next':
            assert ecx==receiver and r(sp+4)==frame+0x70
            assert argument_slots and r(argument_slots[-1])==r(frame+0xa0)
            uc.reg_write(UC_X86_REG_EAX,0 if case.get('no_owner') else owner)
            if case.get('scratch'):w(frame+0x70,0xaabbccdd+number)
            if mutation=='argument':w(frame+0xa0,0xfeedbeef);w(argument_slots[-1],0xfeedbeef)
        elif name=='resolve':
            assert ecx==owner and r(sp+4)==r(frame+0xa0)
            value=resolved[number] if number<len(resolved) else 0
            uc.reg_write(UC_X86_REG_EAX,value)
            if mutation=='pool':w(records+8,objects[2])
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert exits and m.reg_read(UC_X86_REG_EDI)==0
    for reg,value in saved.items():assert m.reg_read(reg)==value
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),memory=bytes(m.mem_read(ARENA,0x4000)).hex(),
        calls=calls,state=state_result(m,before),score=r(frame+0x14),sum=r(frame+0x5c))

def nested_cases():
    cases=[{},dict(match=-1),dict(empty=True),dict(count=0),dict(index=3),dict(index=0xffffffff),
        dict(resolved=[0x10002,0x10003,0]),dict(resolved=[0x20002]),dict(resolved=[0x10000]),dict(scratch=True),dict(no_owner=True)]
    cases += [dict(**{field:v}) for field,v in itertools.product(['eligible','enabled'],[0,0x100,0x101,0xffffffff])]
    cases += [dict(lists=v) for v in [[1,0,0,0],[0,0,1,0],[0,0,0,1],[0,1,0,1]]]
    cases += [dict(mutation=v) for v in ['descriptor','index','handle','wrap','math','argument','pool']]
    cases += [dict(descriptor_alias=True,sum=bits(.123)),dict(mutation='pool',resolved=[0x10002,0])]
    special=[0,0x80000000,1,0x80000001,bits(-100),bits(100),0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(**{field:v}) for field,v in itertools.product(['target_weight','factor','base_score','sum','region_score'],special)]
    cases += [dict(component_ratings=[v]*4,ratings=[v]*4,weapon=[v]*4) for v in special]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        [{},dict(resolved=[0x10002,0x10003,0]),dict(factor=0x7f812345),dict(mutation='math')],
        [0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return cases

def compare_nested_score(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_alternate_accept'];guard=bytes.fromhex('8d472485c0')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_nested_score_bridge']
    results=[]
    for i,case in enumerate(nested_cases()):
        old=run_nested(original,original_pe,spec,case)
        new=run_nested(edited,edited_pe,spec,case,helper_entry=symbols['bfv_next_target_handle'])
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,calls=old['calls'],score=old['score'],sum=old['sum']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_nested_score(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'nested-score-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} nested score comparisons passed',flush=True)
