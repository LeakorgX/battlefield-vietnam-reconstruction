"""Compare compiled post-query weights and iterator against original instructions.

Staging preserves installed EXEs/manifests. Object index callbacks are controlled;
parameter tables and arithmetic execute without numeric service mocks. CPU flags
are excluded: both continuation paths overwrite them before conditional use.
"""
import argparse,csv,hashlib,itertools,json,os,re,struct,subprocess
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,
    UC_X86_REG_ESP,UC_X86_REG_FPCW)
from build import GAME,PROJECT,TARGETS,COMPILERS
from native_oracle import ARENA,STACK,load_machine
from verify_constant_returns import FLOAT_STATE

REGS=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,
      UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_ESP]
def bits(value):return struct.unpack('<I',struct.pack('<f',value))[0]
def w(m,address,value):m.mem_write(address,struct.pack('<I',value&0xffffffff))
def r(m,address):return struct.unpack('<I',m.mem_read(address,4))[0]

def compile_staged(target,source='artillery_final_weights',entry='bfv_artillery_final_weights_bridge',folder='final-weights-staged'):
    spec=TARGETS[target];work=PROJECT/'build'/target/folder;work.mkdir(exist_ok=True)
    constants={('OBJECT_POOL' if k=='pool' else k.upper()):v for k,v in spec.items() if isinstance(v,int)}
    (work/'target.h').write_text(''.join(f'#define BFV_{k} 0x{v:08x}u\n' for k,v in constants.items()))
    env=dict(os.environ,PATH=str(COMPILERS)+os.pathsep+os.environ['PATH'])
    def command(arguments):
        result=subprocess.run([str(v) for v in arguments],env=env,capture_output=True,text=True)
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    command([COMPILERS/'gcc.exe','-m32','-std=c11','-O2','-Wall','-Wextra','-Werror',
        '-ffreestanding','-fno-builtin','-frounding-math','-fno-stack-protector',
        '-fno-asynchronous-unwind-tables','-fno-unwind-tables','-I',work,'-c',
        PROJECT/'src'/f'{source}.c','-o',work/'stage.o'])
    (work/'payload.ld').write_text('SECTIONS { . = 0x0200a000; .payload : { *(.text*) *(.rdata*) *(.data*) *(.bss*) BYTE(0) } /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.drectve*) } }')
    command([COMPILERS/'ld.exe','-T',work/'payload.ld','--image-base','0',
        '--entry','_'+entry,'--disable-dynamicbase',
        '--disable-reloc-section','-Map',work/'payload.map','-o',work/'payload.exe',work/'stage.o'])
    command([COMPILERS/'objcopy.exe','-O','binary','--only-section','.payload',work/'payload.exe',work/'payload.bin'])
    symbols={name:int(address,16) for address,name in re.findall(r'(0x[0-9a-f]+)\s+[_@]?(bfv_\w+)(?:@\d+)?\s*$',(work/'payload.map').read_text(),re.M)}
    payload=(work/'payload.bin').read_bytes();assert len(payload)<0x1000
    return payload,symbols

def run(image,pe,spec,case,staged=None,patched=False):
    m=load_machine(image,pe);frame=STACK+0x8000
    m.mem_write(ARENA,b'\xa5'*0x10000);m.mem_write(frame,b'\xa5'*0x220)
    bot,table,other_table=ARENA+0x100,ARENA+0x200,ARENA+0x400
    stub,other_stub=ARENA+0x8000,ARENA+0x8100
    m.mem_write(stub,b'\xc3');m.mem_write(other_stub,b'\xc3')
    w(m,bot,table);w(m,table+0xdc,stub);w(m,other_table+0xdc,other_stub)
    managers=[ARENA+0x1000,ARENA+0x1200,ARENA+0x1400]
    rows=[[ARENA+0x2000+i*0x1000+j*0x200 for j in range(4)] for i in range(3)]
    for i,manager in enumerate(managers):
        pointers=manager+0x80;w(m,manager+4,pointers)
        for j,row in enumerate(rows[i]):
            w(m,pointers+j*4,row)
            for ident,value in [(0x53,.25+i*.1+j*.05),(0x57,.5+i*.1+j*.05),(0x54,.2+i*.1+j*.05)]:
                w(m,row+ident*4,case.get(f'p{ident:x}',bits(value)))
    w(m,spec['artillery_parameter_manager'],managers[0])
    w(m,frame,0xfedc1234);w(m,frame+0x2c,case.get('score',bits(.7)))
    iterator=case.get('iterator',ARENA+0x6000);end=case.get('end',iterator+4)
    w(m,frame+0x1c,iterator);w(m,frame+0x104,end)
    w(m,frame+0x100,case.get('allocation',0))
    # Combined cases traverse the retained cleanup with a controlled allocator.
    # They verify argument/routing/stack integration, not real heap behavior.
    m.mem_write(spec['vector_free'],b'\xc3')
    for reg,value in zip(REGS,[0x11112222,0x33334444,0x55556666,0x77778888,0x9999aaaa,bot,0xbbbbcccc,frame]):m.reg_write(reg,value)
    m.reg_write(UC_X86_REG_FPCW,case.get('cw',0x37f));seed=ARENA+0x9000
    depth=case.get('depth',0)
    for i in range(depth):w(m,seed+i*4,[bits(1.234),bits(-5.678)][i%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+i*4) for i in range(depth))
    iterator_case=case.get('operation')=='iterator'
    starts_at_iterator=case.get('operation') in ('iterator','combined')
    entry=spec['artillery_second_iterator' if starts_at_iterator else 'artillery_final_weights']
    if staged:
        payload,symbols=staged;m.mem_write(ARENA+0xa000,payload)
        if patched:
            for key,symbol in [('artillery_second_iterator','bfv_artillery_second_iterator_bridge'),('artillery_final_weights','bfv_artillery_final_weights_bridge')]:
                destination=spec[key]
                m.mem_write(destination,b'\xe9'+struct.pack('<I',(symbols[symbol]-destination-5)&0xffffffff))
    start=ARENA+0x8800;m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    calls=[];exits=[];frees=[]
    def hook(uc,address,size,unused):
        if address==spec['vector_free']:
            argument=r(uc,uc.reg_read(UC_X86_REG_ESP)+4)
            assert argument==case['allocation'];frees.append(argument)
        elif address in (stub,other_stub):
            assert uc.reg_read(UC_X86_REG_ECX)==bot
            n=len(calls);assert n<3
            calls.append(dict(index=n,stub=address,score=r(uc,frame+0x2c)))
            index=case.get('indices',[0,0,0])[n];uc.reg_write(UC_X86_REG_EAX,index)
            if case.get('mutation')=='manager':w(uc,spec['artillery_parameter_manager'],managers[min(n+1,2)])
            if case.get('mutation')=='table':w(uc,bot,other_table)
            if case.get('mutation')=='row':w(uc,managers[min(n,2)]+0x80+index*4,rows[2][index])
            if case.get('mutation')=='score':w(uc,frame+0x2c,bits(.3+n*.2))
            if case.get('mutation')=='one':w(uc,spec['math_one'],bits(.75+n*.1))
        elif address==spec['artillery_final_weights_continue'] and not iterator_case:
            exits.append('weights');uc.emu_stop()
        elif iterator_case and address in (spec['artillery_second_filter'],spec['artillery_second_iterator_done']):
            exits.append('loop' if address==spec['artillery_second_filter'] else 'done');uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1,case
    assert len(calls)==(0 if iterator_case else 3),case
    assert len(frees)==int(case.get('operation')=='combined' and bool(case.get('allocation',0))),case
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),arena=bytes(m.mem_read(ARENA,0x10000)).hex(),
        regs=[m.reg_read(reg) for reg in REGS],fpu=[m.reg_read(reg) for reg in FLOAT_STATE],
        manager=r(m,spec['artillery_parameter_manager']),one=r(m,spec['math_one']),calls=calls,frees=frees,exit=exits[0])

def cases():
    out=[{},dict(indices=[1,2,3])]+[dict(mutation=v,indices=[1,2,3]) for v in ['manager','table','row','score','one']]
    special=[0,1,0x80000000,0xbf800000,0x7f800000,0xff800000,0x7fc12345]
    out += [{field:value} for field,value in itertools.product(['p53','p57','p54'],special)]
    out += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        [{},dict(mutation='manager'),dict(mutation='score'),dict(p54=0x7fc12345)],
        [0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    out += [dict(operation='iterator',iterator=begin,end=end) for begin,end in
        [(ARENA+0x6000,ARENA+0x6004),(ARENA+0x6000,ARENA+0x6010),(0xfffffffc,0),(0xfffffffc,4),(0,4),(0,0)]]
    out += [dict(operation='combined',allocation=allocation,mutation=mutation) for allocation,mutation in
        itertools.product([0,ARENA+0x6000],['manager','score','table'])]
    return out

def compare(original,edited,spec,staged=None,symbols=None):
    op=pefile.PE(data=original);ep=pefile.PE(data=edited)
    guards=[('artillery_second_iterator','bfv_artillery_second_iterator_bridge',bytes.fromhex('8b44241c8b8c2404010000')),
        ('artillery_final_weights','bfv_artillery_final_weights_bridge',bytes.fromhex('8b168b3d')+struct.pack('<I',spec['artillery_parameter_manager'])+bytes.fromhex('8bceff92dc000000'))]
    for key,symbol,guard in guards:
        entry=spec[key];assert op.get_data(entry-0x400000,len(guard))==guard
        if staged is None:
            patched=ep.get_data(entry-0x400000,len(guard))
            assert patched[0]==0xe9 and patched[5:]==b'\x90'*(len(guard)-5)
            assert symbols is not None and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols[symbol]
    results=[]
    for i,case in enumerate(cases()):
        old=run(original,op,spec,case,staged);new=run(edited,ep,spec,case,staged,True)
        differences=[k for k in old if old[k]!=new[k]]
        assert not differences,(i,case,{k:(old[k],new[k]) for k in differences if k not in ['arena','frame']},differences)
        results.append(dict(inputs=case,calls=old['calls'],exit=old['exit']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');p.add_argument('--staged',action='store_true');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        with (PROJECT.parent/'reports'/target/'artillery-final-weights-audit.tsv').open() as f:
            audits=list(csv.DictReader(f,delimiter='\t'))
        assert len(audits)==2
        for row in audits:
            entry=int(row['address'],16);guard=bytes.fromhex(row['patch_hex'])
            assert row['original_sha256']==spec['sha'] and row['status']=='eligible'
            assert pefile.PE(data=original).get_data(entry-0x400000,len(guard))==guard
        staged=compile_staged(target) if a.staged else None
        results=compare(original,edited,spec,staged,{k:int(v,16) for k,v in manifest['symbols'].items()})
        destination=work/('final-weights-staged/verification.json' if staged else 'final-weights-verification.json')
        destination.write_text(json.dumps(dict(target=target,original_sha256=spec['sha'],
            compiled_sha256=manifest['output_sha256'],staged=bool(staged),
            source_sha256=hashlib.sha256((PROJECT/'src/artillery_final_weights.c').read_bytes()).hexdigest(),
            passed=len(results),cases=results),indent=2)+'\n')
        print(target,len(results),'final weights/iterator comparisons passed','staged' if staged else '',flush=True)
