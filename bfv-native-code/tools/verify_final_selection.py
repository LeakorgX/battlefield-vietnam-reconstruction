"""Stage the entire three-return artillery final-selection tail.

Object/curve callbacks are controlled. Compares full fixture memory, callback
arguments/order, return stack, callee-preserved registers and physical x87 state.
Caller-scratch registers/flags, unmasked exceptions and live matches are excluded.
"""
import argparse,csv,hashlib,itertools,json,struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,
    UC_X86_REG_ESP,UC_X86_REG_FPCW)
from build import GAME,PROJECT,TARGETS
from native_oracle import ARENA,STACK,load_machine
from verify_constant_returns import FLOAT_STATE
from verify_final_weights import compile_staged,bits,w,r

def run(image,pe,spec,case,staged=None,patched=False):
    m=load_machine(image,pe);frame=STACK+0x8000
    m.mem_write(ARENA,b'\xa5'*0x10000);m.mem_write(frame,b'\xa5'*0x240)
    bot,table,other_table,output,out_table,collection,col_table=[ARENA+n for n in (0x100,0x300,0x700,0x1200,0x1600,0x2000,0x2200)]
    targets,weapons,states,ratings,mode,factor=[ARENA+n for n in (0x3000,0x3200,0x3400,0x3600,0x4000,0x4200)]
    w(m,bot,table);w(m,output,out_table);w(m,collection,col_table)
    w(m,output+4,case.get('mode',3));w(m,output+8,targets);w(m,output+0xc,collection)
    w(m,output+0x1c,ratings);w(m,output+0x24,states);w(m,output+0x30,weapons)
    for j in range(8):w(m,weapons+j*4,j+10);w(m,ratings+j*4,bits(.35+j*.01));w(m,targets+j*4,0x11110000+j)
    w(m,mode+8,case.get('occupied',0));w(m,factor+8,case.get('factor',bits(.5)))
    for off,value in [(0,0x11111111),(4,0x22222222),(8,0x33333333),(0x28,bits(.8)),
        (0x44,case.get('alternate',bits(.4))),(0x48,5),(0x80,case.get('best',bits(.6))),
        (0xa0,output),(0xa4,7),(0xd0,6),(0xd8,0x55550002),(0x13c,0xabc001),
        (0x140,case.get('remembered_score',bits(.9))),(0x144,0x55550001),
        (0x148,0xabc002),(0x1f0,bits(.75))]:w(m,frame+off,value)
    m.mem_write(frame+0xf,bytes([case.get('remembered',0)]))
    stop=ARENA+0x8900;w(m,frame+0x1e4,stop)
    stubs={name:ARENA+0x8000+i*0x40 for i,name in enumerate(['index','index2','mode_test','ready','mode_data','factor','rating','reset','select','remove','curve'])}
    slots=[(0xdc,'index',0),(0x70,'mode_test',4),(0x6c,'ready',0),(0x4c,'mode_data',4),
        (0xe0,'factor',0),(0x160,'rating',4),(0x78,'reset',4)]
    for slot,name,cleanup in slots:
        w(m,table+slot,stubs[name]);w(m,other_table+slot,stubs['index2'] if name=='index' else stubs[name])
        m.mem_write(stubs[name],b'\xc3' if not cleanup else b'\xc2'+struct.pack('<H',cleanup))
    m.mem_write(stubs['index2'],b'\xc3')
    w(m,out_table+0x34,stubs['select']);w(m,col_table+8,stubs['remove'])
    m.mem_write(stubs['select'],b'\xc2\x14\x00');m.mem_write(stubs['remove'],b'\xc2\x04\x00')
    curve_value=ARENA+0x4600;w(m,curve_value,case.get('curve',bits(.7)))
    native_curve_table=ARENA+0x4800
    w(m,spec['artillery_final_curve_table'],native_curve_table)
    for j in range(101):w(m,native_curve_table+j*4,bits((j+.25)/101))
    if not case.get('real_curve'):
        m.mem_write(spec['artillery_final_curve'],b'\xe9'+struct.pack('<I',(stubs['curve']-spec['artillery_final_curve']-5)&0xffffffff))
    m.mem_write(stubs['curve'],b'\xd9\x05'+struct.pack('<I',curve_value)+b'\xc2\x04\x00')
    regs={UC_X86_REG_EAX:0x44556677,UC_X86_REG_EBX:0x8899aabb,UC_X86_REG_ECX:0x11223344,
        UC_X86_REG_EDX:0x55667788,UC_X86_REG_EDI:0x99aabbcc,UC_X86_REG_ESI:bot,
        UC_X86_REG_EBP:0xddccbbaa,UC_X86_REG_ESP:frame}
    for reg,value in regs.items():m.reg_write(reg,value)
    m.reg_write(UC_X86_REG_FPCW,case.get('cw',0x37f))
    seed=ARENA+0x9000;depth=case.get('depth',0)
    for i in range(depth):w(m,seed+i*4,[bits(1.234),bits(-5.678)][i%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+i*4) for i in range(depth))
    entry=spec['artillery_final_selection']
    if staged:
        payload,symbols=staged;m.mem_write(ARENA+0xa000,payload)
        if patched:m.mem_write(entry,b'\xe9'+struct.pack('<I',(symbols['bfv_artillery_final_selection_bridge']-entry-5)&0xffffffff))
    start=ARENA+0x8800;m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    calls=[];index_calls=0;returned=[]
    def hook(uc,address,size,unused):
        nonlocal index_calls
        if address==stop:returned.append(True);uc.emu_stop();return
        name='curve-native' if case.get('real_curve') and address==spec['artillery_final_curve'] else next((n for n,a in stubs.items() if a==address),None)
        if name is None:return
        sp=uc.reg_read(UC_X86_REG_ESP);receiver=uc.reg_read(UC_X86_REG_ECX)
        count={'mode_test':1,'mode_data':1,'rating':1,'reset':1,'select':5,'remove':1,'curve':1,'curve-native':1}.get(name,0)
        arguments=[r(uc,sp+4+j*4) for j in range(count)]
        if name=='select':assert receiver==output and arguments[0]==bot and arguments[2:4]==[0,0]
        elif name=='remove':assert receiver==collection and arguments==[bot]
        elif name=='reset':assert receiver==bot and arguments==[0xffffffff]
        elif name not in ('curve','curve-native'):assert receiver==bot
        calls.append(dict(name=name,args=arguments))
        if name in ('index','index2'):
            indices=case.get('indices',[0]);uc.reg_write(UC_X86_REG_EAX,indices[index_calls%len(indices)]);index_calls+=1
            mutation=case.get('mutation')
            if mutation=='table':w(uc,bot,other_table)
            elif mutation=='ratings':w(uc,output+0x1c,ratings+0x80)
            elif mutation=='score':w(uc,frame+0x80,bits(.2));w(uc,frame+0x44,bits(.3))
        elif name=='mode_test':uc.reg_write(UC_X86_REG_EAX,case.get('mode_test',1))
        elif name=='ready':uc.reg_write(UC_X86_REG_EAX,case.get('ready',1))
        elif name=='mode_data':uc.reg_write(UC_X86_REG_EAX,mode)
        elif name=='factor':uc.reg_write(UC_X86_REG_EAX,factor)
        elif name=='curve' and case.get('mutation')=='curve':w(uc,frame+0x28,bits(.9));w(uc,frame+0x44,bits(.6))
        elif name=='select' and case.get('mutation')=='select':w(uc,frame+0x80,bits(.7));w(uc,frame+0x44,bits(.8));m.mem_write(frame+0xf,b'\x01')
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==frame+0x1fc
    assert [m.reg_read(reg) for reg in (UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBX,UC_X86_REG_EBP)]==[0x11111111,0x22222222,0x33333333,0xddccbbaa]
    return dict(frame=bytes(m.mem_read(frame,0x240)).hex(),arena=bytes(m.mem_read(ARENA,0x10000)).hex(),
        calls=calls,fpu=[m.reg_read(reg) for reg in FLOAT_STATE],stack=m.reg_read(UC_X86_REG_ESP))

def cases():
    base=[{},dict(best=0),dict(best=0,alternate=0),dict(remembered=1),dict(remembered=1,remembered_score=0),
        dict(mode_test=0),dict(ready=0),dict(occupied=1),dict(mode_test=0x100),dict(ready=0x101),dict(indices=[1,2,3])]
    out=base+[dict(c,mutation=mutation) for c,mutation in itertools.product(base[:3],['table','ratings','score','select','curve'])]
    out += [{field:v} for field,v in itertools.product(['best','alternate','remembered_score','factor','curve'],[0,1,0x80000000,0x7f800000,0xff800000,0x7fc12345])]
    out += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(base[:4],[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    out += [dict(c,real_curve=True,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        [dict(best=bits(.2)),dict(best=0,alternate=bits(.2))],
        [0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return out

def compare(original,edited,spec,staged=None,symbols=None):
    op=pefile.PE(data=original);ep=pefile.PE(data=edited);results=[]
    entry=spec['artillery_final_selection'];guard=bytes.fromhex('d9842480000000')
    assert op.get_data(entry-0x400000,len(guard))==guard
    if staged is None:
        patch=ep.get_data(entry-0x400000,len(guard))
        assert patch[0]==0xe9 and patch[5:]==b'\x90\x90'
        assert symbols is not None and entry+5+struct.unpack('<i',patch[1:5])[0]==symbols['bfv_artillery_final_selection_bridge']
    for i,case in enumerate(cases()):
        old=run(original,op,spec,case,staged);new=run(edited,ep,spec,case,staged,True)
        differences=[k for k in old if old[k]!=new[k]]
        assert not differences,(i,case,differences,{k:(old[k],new[k]) for k in differences if k not in ('frame','arena')})
        results.append(dict(inputs=case,calls=old['calls']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME);p.add_argument('--target',choices=['client','server','both'],default='both');p.add_argument('--staged',action='store_true');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        with (PROJECT.parent/'reports'/target/'artillery-final-selection-audit.tsv').open() as f:audits=list(csv.DictReader(f,delimiter='\t'))
        assert len(audits)==1 and audits[0]['original_sha256']==spec['sha'] and audits[0]['status']=='eligible'
        staged=compile_staged(target,'artillery_final_selection','bfv_artillery_final_selection_bridge','final-selection-staged') if a.staged else None
        results=compare(original,edited,spec,staged,{k:int(v,16) for k,v in manifest['symbols'].items()})
        destination=work/('final-selection-staged/verification.json' if a.staged else 'final-selection-verification.json')
        destination.write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],staged=bool(staged),
            source_sha256=hashlib.sha256((PROJECT/'src/artillery_final_selection.c').read_bytes()).hexdigest(),
            passed=len(results),cases=results),indent=2)+'\n')
        print(target,len(results),'complete final-selection tail comparisons passed','staged' if staged else 'installed',flush=True)
