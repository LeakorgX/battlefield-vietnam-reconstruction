"""Compare first-pass category scoring and its node iterator search.

Virtual classification/list/factor methods are controlled. Actual list search,
float selectors, candidate arithmetic and pointer/field rereads execute.
"""
import argparse
import hashlib
import itertools
import json
import random
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE,UC_HOOK_MEM_READ
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_FPCW)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import bits,state_result


def setup(image,pe,case):
    m=load_machine(image,pe);frame=STACK+0x8000;seed=ARENA+0x9400
    m.mem_write(ARENA,b'\xa5'*0x4000);m.mem_write(frame,b'\xa5'*0x220)
    def w(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def r(p):return struct.unpack('<I',m.mem_read(p,4))[0]
    def words(p,values):m.mem_write(p,struct.pack('<'+'I'*len(values),*values))
    m.reg_write(UC_X86_REG_FPCW,case.get('cw',0x37f))
    for i in range(case.get('depth',0)):w(seed+4*i,[bits(1.234),bits(-5.678)][i%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(case.get('depth',0)))
    return m,frame,w,r,words,preload


def run_search(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    output,key,sentinel=ARENA+0x100,ARENA+0x200,ARENA+0x300
    count=case.get('count',4);nodes=[ARENA+0x500+0x40*i for i in range(count)]
    if case.get('unaligned'):output+=1;key+=3;sentinel+=1;nodes=[n+1 for n in nodes]
    value=case.get('value',0xfedcba98);match=case.get('match',1)
    for i,node in enumerate(nodes):words(node,[nodes[i+1] if i+1<count else sentinel,0, value if i==match else i+1])
    first=nodes[0] if count else sentinel
    if case.get('layout')=='node':output=first+8
    if case.get('layout')=='next':output=first
    if case.get('layout')=='key':output=key
    w(key,value)
    if case.get('invalid_key'):assert count==0;key=0x1234
    stop,start=ARENA+0x9200,ARENA+0x9000;entry=spec['list_find_word']
    words(frame,[stop,first,sentinel]);m.mem_write(stop,b'\x90')
    saved={reg:0x12340000+i for i,reg in enumerate([UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP])}
    for reg,v in {**saved,UC_X86_REG_ECX:output,UC_X86_REG_EDX:key,UC_X86_REG_ESP:frame}.items():m.reg_write(reg,v)
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];returned=[];reads=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==stop:returned.append(True);uc.emu_stop()
    def read(uc,access,address,size,value,unused):
        if address==key:reads.append(size)
    m.hook_add(UC_HOOK_CODE,hook);m.hook_add(UC_HOOK_MEM_READ,read)
    m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==frame+12 and m.reg_read(UC_X86_REG_EAX)==output
    assert reads==([4] if count else [])
    for reg,v in saved.items():assert m.reg_read(reg)==v
    return dict(memory=bytes(m.mem_read(ARENA,0x4000)).hex(),result=r(output),state=state_result(m,before))


def run_category(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    bot,target,descriptor,interface=[ARENA+n for n in (0x100,0x300,0x600,0x700)]
    vectors=[ARENA+0xa00+i*0x100 for i in range(4)]
    sentinels=[ARENA+0x1000+i*0x100 for i in range(4)]
    nodes=[ARENA+0x1800+i*0x40 for i in range(3)]
    record,iterator,data,ratings_owner,ratings,weapon,factor=[ARENA+n for n in (0x2200,0x2400,0x2500,0x2600,0x2800,0x2a00,0x2c00)]
    names=['class','eligible','enabled','list','factor'];stubs={name:ARENA+0x8000+i*0x100 for i,name in enumerate(names)}
    for name,stub in stubs.items():m.mem_write(stub,b'\xc2\x04\x00' if name in ['eligible','list'] else b'\xc3')
    w(bot,bot+0x80);w(interface,interface+0x80)
    for slot,name in [(0x70,'eligible'),(0x6c,'enabled'),(0x4c,'list'),(0xe0,'factor')]:w(bot+0x80+slot,stubs[name])
    w(interface+0x80+0x3c,stubs['class']);w(target+0x24,interface)
    m.mem_write(target+7,bytes([case.get('flags',1)]));m.mem_write(descriptor+4,bytes([case.get('category',7)]))
    w(target+0x14,case.get('target_weight',bits(1.25)))
    key=0xfedcba98;w(record,key)
    for i,node in enumerate(nodes):words(node,[nodes[i+1] if i<2 else sentinels[0],0,key if i==case.get('match',1) else i+1])
    for i,(vector,sentinel) in enumerate(zip(vectors,sentinels)):
        words(vector,[0, sentinel, case.get('count',3)]);w(sentinel,nodes[0])
    if case.get('empty'):w(sentinels[0],sentinels[0])
    w(iterator+8,data);w(data,0xabcdef01);w(ratings_owner+8,ratings)
    words(ratings,case.get('ratings',[bits(1),bits(2),bits(3),bits(4)]))
    words(weapon,case.get('weapon',[bits(1),bits(.5),bits(.25),bits(.125)]))
    w(factor+8,case.get('factor',bits(.75)))
    for offset,value in [(0x18,record),(0x40,iterator),(0xd8,ratings_owner),(0xf0,weapon),(0x60,3),
        (0x44,case.get('index',0)),(0x28,case.get('distance',bits(10))),(0x1c,case.get('parameter',bits(100))),
        (0x14,case.get('base_score',bits(.2))),(0x48,case.get('best',bits(0))),
        (0x20,case.get('movement_score',bits(.8))),(0x74,case.get('aim_weight',bits(.9))),
        (0x34,case.get('weapon_score',bits(1.1))),(0x2c,case.get('weight',bits(1.2))),
        (0x98,case.get('region_score',bits(1.3)))]:w(frame+offset,value)
    if case.get('invalid_record'):w(frame+0x18,0x1234)
    if case.get('identity_alias'):w(iterator+8,frame+0x48)
    saved={UC_X86_REG_EBX:descriptor,UC_X86_REG_ESI:bot,UC_X86_REG_EBP:target,UC_X86_REG_ESP:frame}
    for reg,value in {**saved,UC_X86_REG_EDI:0x12345678}.items():m.reg_write(reg,value)
    entry,start=spec['artillery_category_score'],ARENA+0x9000
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];calls=[];exits=[];lists=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in [spec['artillery_category_alternate'],spec['artillery_filter_reject']]:
            exits.append('alternate' if address==spec['artillery_category_alternate'] else 'next_candidate');uc.emu_stop();return
        ecx,sp=uc.reg_read(UC_X86_REG_ECX),uc.reg_read(UC_X86_REG_ESP);mutation=case.get('mutation')
        if address==stubs['class']:
            assert ecx==interface;calls.append('class');uc.reg_write(UC_X86_REG_EAX,case.get('class',1))
        elif address==stubs['eligible']:
            arg=r(sp+4);assert ecx==bot and arg==m.mem_read(descriptor+4,1)[0]
            calls.append(('eligible',arg));uc.reg_write(UC_X86_REG_EAX,case.get('eligible',1))
            if mutation=='descriptor':m.mem_write(descriptor+4,b'\xff')
            if mutation=='flags':m.mem_write(target+7,b'\x00')
        elif address==stubs['enabled']:
            assert ecx==bot;calls.append('enabled');uc.reg_write(UC_X86_REG_EAX,case.get('enabled',1))
            if mutation=='index':w(frame+0x44,2)
        elif address==stubs['list']:
            arg=r(sp+4);assert ecx==bot and arg==m.mem_read(descriptor+4,1)[0]
            number=len(lists);which=case.get('lists',[0,0,0,0])[number]
            lists.append(which);calls.append(('list',number,arg,which));uc.reg_write(UC_X86_REG_EAX,vectors[which])
            if mutation=='record' and number==3:w(record,0x12345678)
            if mutation=='wrap' and number==1:w(frame+0x44,0xffffffff)
            if mutation=='nodes' and number==3:w(nodes[1]+8,key);w(record,key)
        elif address==stubs['factor']:
            assert ecx==bot;calls.append('factor');uc.reg_write(UC_X86_REG_EAX,factor)
            if mutation=='math':w(frame+0x28,bits(90));w(frame+0xbc,3);w(frame+0x98,bits(-.5))
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg,value in saved.items():assert m.reg_read(reg)==value
    expected_edi=sentinels[lists[1]] if len(lists)>1 else 0x12345678
    assert m.reg_read(UC_X86_REG_EDI)==expected_edi
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),memory=bytes(m.mem_read(ARENA,0x4000)).hex(),
                calls=calls,exit=exits[0],edi=expected_edi,state=state_result(m,before),score=r(frame+0x14),best=r(frame+0x48),selected=r(frame+0x4c)==r(frame+0x60))


def search_cases():
    cases=[dict(count=c,match=i) for c in [0,1,4,16] for i in [-1,0,c//2,c-1]]
    cases += [dict(layout=l,unaligned=u) for l,u in itertools.product(['node','next','key'],[False,True])]
    cases += [dict(value=v) for v in [0,1,0x80000000,0xffffffff,0x7f812345]]
    cases += [dict(count=0,invalid_key=True)]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product([{},dict(count=0,invalid_key=True),dict(layout='key')],[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,3,5])]
    return cases


def category_cases():
    cases=[{},dict(flags=0),dict(flags=2),dict(flags=255),dict(match=-1),dict(empty=True),dict(count=0),dict(index=3),dict(index=0xffffffff),dict(identity_alias=True)]
    cases += [dict(**{field:v}) for field,v in itertools.product(['eligible','enabled'],[0,1,0x100,0x101,0xffffffff])]
    cases += [dict(mutation=x) for x in ['descriptor','flags','index','record','wrap','nodes','math']]
    cases += [dict(lists=x) for x in [[1,0,0,0],[0,0,1,0],[0,0,0,1],[0,1,0,1]]]
    cases += [dict(match=-1,lists=[0,0,0,1]),dict(match=-1,mutation='nodes'),dict(empty=True,invalid_record=True),dict(count=0xffffffff,index=0xfffffffe)]
    cases += [dict(best=v) for v in [0x3f92b429-1,0x3f92b429,0x3f92b429+1]]
    cases += [dict(**{'class':v}) for v in [0,2,3]]
    special=[0,0x80000000,1,0x80000001,bits(-100),bits(100),0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(**{field:v}) for field,v in itertools.product(['distance','parameter','target_weight','factor','base_score','best','movement_score','region_score','weight'],special)]
    cases += [dict(ratings=[v,v,v,v],weapon=[v,v,v,v]) for v in special]
    selected=[{},dict(flags=0),dict(match=-1),dict(identity_alias=True),dict(best=0x7f812345),dict(distance=0x7f812345),dict(factor=0x7f812345),dict(base_score=bits(.3))]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,3])]
    # Native baseline rounded outputs for each control word. Equal stored bits
    # can still win when the retained extended score exceeds that float32 value.
    rounded_baselines={127: 1066578986, 1151: 1066578982, 2175: 1066578988, 3199: 1066578982, 639: 1066578985, 1663: 1066578984, 2687: 1066578985, 3711: 1066578984, 895: 1066578985, 1919: 1066578984, 2943: 1066578985, 3967: 1066578984}
    cases += [dict(cw=cw,best=rounded) for cw,rounded in rounded_baselines.items()]
    rng=random.Random(0x9a007a)
    cases += [dict(distance=bits(rng.uniform(-10,500)),parameter=bits(rng.uniform(-10,500)),factor=bits(rng.uniform(-2,2)),base_score=bits(rng.uniform(-2,2)),best=bits(rng.uniform(-2,2))) for _ in range(24)]
    return cases


def compare_category_score(original,original_pe,edited,edited_pe,spec,symbols):
    for key,symbol in [('list_find_word','bfv_find_word_in_nodes'),('artillery_category_score','bfv_artillery_category_score_bridge')]:
        entry=spec[key];jump=edited_pe.get_data(entry-0x400000,5)
        assert jump[0]==0xe9 and entry+5+struct.unpack('<i',jump[1:])[0]==symbols[symbol]
    results=[]
    for group,runner,cases in [('search',run_search,search_cases()),('category',run_category,category_cases())]:
        for i,case in enumerate(cases):
            old=runner(original,original_pe,spec,case);new=runner(edited,edited_pe,spec,case)
            assert old==new,(group,i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
            results.append(dict(group=group,inputs=case,calls=old.get('calls'),score=old.get('score'),selected=old.get('selected')))
    return results


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_category_score(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,{k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'category-score-verification.json').write_text(json.dumps(dict(target=target,original_sha256=spec['sha'],
            compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} category score/search comparisons passed',flush=True)
