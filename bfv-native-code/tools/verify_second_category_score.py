"""Differential checks for the second-pass category scorer boundary."""
import argparse, hashlib, itertools, json, os, re, struct, subprocess
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_FPCW)
from build import GAME,PROJECT,TARGETS,COMPILERS
from native_oracle import ARENA,STACK,load_machine
from verify_constant_returns import FLOAT_STATE

def bits(value): return struct.unpack('<I',struct.pack('<f',value))[0]
def w(m,a,v): m.mem_write(a,struct.pack('<I',v&0xffffffff))
def r(m,a): return struct.unpack('<I',m.mem_read(a,4))[0]
def words(m,a,values): m.mem_write(a,struct.pack('<'+'I'*len(values),*values))

def run(image,pe,spec,case,staged=None,patch_stage=False):
    m=load_machine(image,pe); frame=STACK+0x8000; seed=ARENA+0x9000
    m.mem_write(ARENA,b'\xa5'*0x10000);m.mem_write(frame,b'\xa5'*0x220)
    candidate,component,receiver,queried=[ARENA+x for x in (0x100,0x200,0x300,0x400)]
    bot,bot_table,output,output_table,iterator=[ARENA+x for x in (0x500,0x600,0x700,0x800,0x900)]
    class_owner,class_values,component_values,weapons=[ARENA+x for x in (0xa00,0xb00,0xc00,0xd00)]
    query,handle,index=[ARENA+x for x in (0x8000,0x8100,0x8200)]
    m.mem_write(query,b'\xc2\x04\x00');m.mem_write(handle,b'\xc2\x04\x00');m.mem_write(index,b'\xb8'+struct.pack('<I',case.get('output_index',0))+b'\xc3')
    w(m,candidate,0);w(m,candidate+0x20,receiver);w(m,candidate+0x30,component);w(m,candidate+0x14,case.get('candidate',bits(.25)))
    w(m,receiver,receiver+0x100);w(m,receiver+0x100+0x84,query);w(m,queried,queried+0x100);w(m,queried+0x100+0x5c,handle)
    w(m,component+8,component_values);words(m,component_values,case.get('component_values',[bits(.5)]*8))
    w(m,class_owner+8,class_values);words(m,class_values,case.get('class_values',[bits(.75)]*8));words(m,weapons,case.get('weapon_values',[bits(1.25)]*8))
    w(m,bot,bot_table);w(m,bot_table+0xdc,index);w(m,bot+0x2c,case.get('limit',10));w(m,output+8,output_table)
    record=case.get('record',0x12340007);w(m,iterator,record);w(m,frame+0x148,case.get('selected_record',0))
    for off,val in ((0x18,candidate),(0x1c,iterator),(0x20,case.get('base',bits(2))),
                    (0x28,case.get('denominator',bits(1))),(0x2c,case.get('post_scale',bits(1))),
                    (0x30,case.get('category',1)),(0x34,case.get('factor_a',bits(.8))),
                    (0x3c,component),(0x40,case.get('factor_b',bits(.9))),
                    (0x48,case.get('alternate_best',bits(.1))),(0x5c,case.get('limit_score',bits(5))),
                    (0x74,case.get('weapon_index',3)),(0x84,case.get('best',bits(.1))),
                    (0x98,case.get('region',bits(.7))),(0xa0,case.get('argument',0xabc00001)),
                    (0xd0,case.get('component_index',2)),(0xd8,class_owner),(0xf0,weapons)):
        w(m,frame+off,val)
    m.reg_write(UC_X86_REG_FPCW,case.get('cw',0x37f));factor=ARENA+0x1000;w(m,factor,case.get('factor',bits(.6)))
    depth=case.get('depth',0)
    for i in range(depth):w(m,seed+4*i,[bits(1.234),bits(-5.678)][i%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+4*i) for i in range(depth))+b'\xd9\x05'+struct.pack('<I',factor)
    saved={UC_X86_REG_EAX:0x11112222,UC_X86_REG_EBX:candidate,UC_X86_REG_ECX:0x33334444,UC_X86_REG_EDI:output,UC_X86_REG_ESI:bot,UC_X86_REG_EBP:0x77778888,UC_X86_REG_ESP:frame}
    for reg,val in saved.items():m.reg_write(reg,val)
    entry=spec['artillery_second_category_score']; start=ARENA+0x8800
    if staged:
        payload,bridge=staged
        m.mem_write(ARENA+0xa000,payload)
        if patch_stage:
            m.mem_write(entry,b'\xe9'+struct.pack('<I',(bridge-entry-5)&0xffffffff))
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    records=ARENA+0x10000
    m.mem_map(records,0x80000)
    w(m,spec['pool'],ARENA+0xe00);w(m,ARENA+0xe00,records)
    chosen_handle=case.get('handle',0)
    handle_index=chosen_handle&0xffff
    if handle_index:
        record_address=records+handle_index*8-8
        w(m,record_address,case.get('pool_object',candidate))
        m.mem_write(record_address+6,struct.pack('<H',case.get('generation',chosen_handle>>16)))
    calls=[];exits=[]
    def hook(uc,address,size,unused):
        if address==query:
            sp=uc.reg_read(UC_X86_REG_ESP)
            assert uc.reg_read(UC_X86_REG_ECX)==receiver and r(uc,sp+4)==frame+0xec
            calls.append('query')
            for off,value in case.get('query_frame_writes',{}).items():w(uc,frame+int(off,16),value)
            uc.reg_write(UC_X86_REG_EAX,0 if case.get('null_query') else queried)
        elif address==handle:
            sp=uc.reg_read(UC_X86_REG_ESP)
            assert uc.reg_read(UC_X86_REG_ECX)==queried and r(uc,sp+4)==r(uc,frame+0xa0)
            calls.append('handle');uc.reg_write(UC_X86_REG_EAX,chosen_handle)
            for off,value in case.get('handle_frame_writes',{}).items():w(uc,frame+int(off,16),value)
        elif address==index:
            assert uc.reg_read(UC_X86_REG_ECX)==bot
            calls.append('index')
            for off,value in case.get('index_frame_writes',{}).items():w(uc,frame+int(off,16),value)
        elif address==spec['artillery_second_category_score_continue']:
            exits.append('continue');uc.emu_stop()
        elif address==spec['artillery_second_category_factor']:
            exits.append('factor');uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1,case
    assert calls[:1]==['query']
    assert ('handle' in calls)==(not case.get('null_query',False))
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),arena=bytes(m.mem_read(ARENA,0x10000)).hex(),
                regs=[m.reg_read(reg) for reg in saved],fpu=[m.reg_read(reg) for reg in FLOAT_STATE],calls=calls,exit=exits[0])

def cases():
    values=[0,1,0x80000000,0x7f800000,0xff800000,0x7fc12345]
    out=[{},dict(limit=0),dict(best=bits(.9)),dict(denominator=bits(2)),dict(post_scale=bits(2)),dict(selected_record=0x12340007)]
    out += [{field:v} for field in ('factor','base','region','candidate') for v in values]
    out += [dict(limit=0,best=bits(.9)),dict(alternate_best=bits(.9)),
            dict(limit=0,selected_record=0x12340007),dict(limit_score=bits(10)),
            dict(limit=0xffffffff),dict(limit=0x80000000)]
    out += [dict(null_query=True),dict(handle=0x20001),dict(handle=0x20001,generation=3),
            dict(handle=0x20001,pool_object=0),dict(handle=0x10000),dict(handle=0xffffffff,generation=2),
            dict(limit=0,output_index=2),dict(query_frame_writes={'a0':0x11110002}),
            dict(handle_frame_writes={'14':bits(.123)}),dict(limit=0,index_frame_writes={'14':bits(.99)})]
    out += [dict(c,cw=0x7f|p|q,depth=d) for c,p,q,d in itertools.product(out[:6],[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return out

def compile_staged(target):
    """Compile only this stage without overwriting installed EXEs/manifests."""
    work=PROJECT/'build'/target/'category-score-staged';work.mkdir(exist_ok=True)
    environment=dict(os.environ,PATH=str(COMPILERS)+os.pathsep+os.environ['PATH'])
    def command(args):
        result=subprocess.run([str(x) for x in args],env=environment,capture_output=True,text=True)
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    obj=work/'score.o'
    command([COMPILERS/'gcc.exe','-m32','-std=c11','-O2','-Wall','-Wextra','-Werror',
        '-ffreestanding','-fno-builtin','-frounding-math','-fno-stack-protector',
        '-fno-asynchronous-unwind-tables','-fno-unwind-tables','-I',work.parent,
        '-c',PROJECT/'src/artillery_second_category_score.c','-o',obj])
    (work/'payload.ld').write_text('SECTIONS { . = 0x0200a000; .payload : { *(.text*) *(.rdata*) *(.data*) *(.bss*) BYTE(0) } /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.drectve*) } }')
    command([COMPILERS/'ld.exe','-T',work/'payload.ld','--image-base','0',
        '--entry','_bfv_artillery_second_category_score_bridge','--disable-dynamicbase',
        '--disable-reloc-section','-Map',work/'payload.map','-o',work/'payload.exe',obj])
    command([COMPILERS/'objcopy.exe','-O','binary','--only-section','.payload',work/'payload.exe',work/'payload.bin'])
    symbols={name:int(address,16) for address,name in re.findall(r'(0x[0-9a-f]+)\s+[_@]?(bfv_\w+)(?:@\d+)?\s*$',(work/'payload.map').read_text(),re.M)}
    payload=(work/'payload.bin').read_bytes()
    assert len(payload)<0x2000
    return payload,symbols['bfv_artillery_second_category_score_bridge']

def compare(original,edited,spec,staged=None):
    guard=bytes.fromhex('8b8c24d80000008b51088b44243c'); op=pefile.PE(data=original); ep=pefile.PE(data=edited);entry=spec['artillery_second_category_score']
    assert op.get_data(entry-op.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    results=[]
    for i,case in enumerate(cases()):
        old=run(original,op,spec,case,staged);new=run(edited,ep,spec,case,staged,True)
        assert old==new,(i,case)
        results.append(case)
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME);p.add_argument('--target',choices=['client','server','both'],default='both');p.add_argument('--staged',action='store_true');a=p.parse_args()
    for target in ('client','server') if a.target=='both' else (a.target,):
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text());original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        staged=compile_staged(target) if a.staged else None
        found=compare(original,edited,spec,staged)
        destination=work/('category-score-staged/verification.json' if staged else 'second-category-score-verification.json')
        destination.write_text(json.dumps(dict(target=target,compiled_sha256=manifest['output_sha256'],source_sha256=hashlib.sha256((PROJECT/'src/artillery_second_category_score.c').read_bytes()).hexdigest(),staged=bool(staged),passed=len(found),cases=found),indent=2)+'\n')
        print(target,len(found),'second category-score comparisons passed', '(staged)' if staged else '',flush=True)
