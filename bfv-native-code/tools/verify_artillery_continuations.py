"""Validate short evaluator entries separately and across adjacent bridges."""
import argparse,csv,hashlib,itertools,json,struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,
    UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_ESP,UC_X86_REG_FPCW)
from build import GAME,PROJECT,TARGETS
from native_oracle import ARENA,STACK,load_machine
from verify_constant_returns import FLOAT_STATE
from verify_final_weights import compile_staged,bits,w,r

def run(image,pe,spec,case,staged=None,patched=False):
    m=load_machine(image,pe);frame=STACK+0x8000
    m.mem_write(ARENA,b'\xa5'*0x10000);m.mem_write(frame,b'\xa5'*0x220)
    pointer=case.get('pointer',ARENA+0x1000)
    w(m,frame+0x100,pointer);w(m,frame+0x1fc,case.get('driver',0x12345678));w(m,frame+0x34,0x7fc12345)
    regs=[UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_ESP]
    for reg,val in zip(regs,[pointer,0x11223344,0x22334455,0x33445566,0x44556677,0x55667788,0x66778899,frame]):m.reg_write(reg,val)
    m.reg_write(UC_X86_REG_FPCW,case.get('cw',0x37f));seed=ARENA+0x9000
    depth=case.get('depth',0)
    for j in range(depth):w(m,seed+j*4,[bits(1.234),bits(-5.678)][j%2])
    preload=b''.join(b'\xd9\x05'+struct.pack('<I',seed+j*4) for j in range(depth))
    # A controlled release that disturbs an inactive physical FP slot.
    w(m,seed+0x40,bits(.345));m.mem_write(spec['vector_free'],b'\xd9\x05'+struct.pack('<I',seed+0x40)+b'\xdd\xd8\xb8\x34\x12\x00\x00\xc3')
    rows=[('artillery_second_movement_unit_score','bfv_artillery_unit_score_bridge'),
        ('artillery_driver_reload','bfv_artillery_driver_reload_bridge'),
        ('artillery_allocation_reload','bfv_artillery_allocation_reload_bridge'),
        ('artillery_second_empty','bfv_artillery_query_cleanup_bridge')]
    if staged:
        payload,symbols=staged;m.mem_write(ARENA+0xa000,payload)
    if patched and staged:
        for key,symbol in rows:
            address=spec[key];m.mem_write(address,b'\xe9'+struct.pack('<I',(symbols[symbol]-address-5)&0xffffffff))
    operation=case['operation'];index={'unit':0,'driver':1,'allocation':2,'cleanup':3,'unit_driver':0,'allocation_cleanup':2}[operation]
    entry=spec[rows[index][0]]
    stop=spec[{'unit':'artillery_driver_reload','driver':'artillery_second_driver_gate',
        'allocation':'artillery_second_empty','cleanup':'artillery_final_weights',
        'unit_driver':'artillery_second_driver_gate','allocation_cleanup':'artillery_final_weights'}[operation]]
    start=ARENA+0x8800;m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    calls=[];exits=[]
    def hook(uc,address,size,unused):
        if address==stop:exits.append(True);uc.emu_stop()
        elif address==spec['vector_free']:
            argument=r(uc,uc.reg_read(UC_X86_REG_ESP)+4);assert argument==pointer;calls.append(argument)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert exits and len(calls)==int('cleanup' in operation and bool(pointer))
    # All scratch results of free are overwritten before the next weight call.
    selected=[UC_X86_REG_EBX,UC_X86_REG_EDI,UC_X86_REG_ESI,UC_X86_REG_EBP,UC_X86_REG_ESP] if 'cleanup' in operation else regs
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),arena=bytes(m.mem_read(ARENA,0x10000)).hex(),
        regs=[m.reg_read(reg) for reg in selected],fpu=[m.reg_read(reg) for reg in FLOAT_STATE],calls=calls)

def cases():
    operations=['unit','driver','allocation','cleanup','unit_driver','allocation_cleanup']
    out=[dict(operation=op,pointer=p,driver=d) for op,p,d in itertools.product(operations,[0,ARENA+0x1000],[0,0xffffffff])]
    out += [dict(operation=op,cw=0x7f|pc|rc,depth=depth) for op,pc,rc,depth in itertools.product(operations,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    return out

def compare(original,edited,spec,staged=None,symbols=None):
    op=pefile.PE(data=original);ep=pefile.PE(data=edited)
    rows=[('artillery_second_movement_unit_score','bfv_artillery_unit_score_bridge','c74424340000803f'),
        ('artillery_driver_reload','bfv_artillery_driver_reload_bridge','8bbc24fc010000'),
        ('artillery_allocation_reload','bfv_artillery_allocation_reload_bridge','8b842400010000'),
        ('artillery_second_empty','bfv_artillery_query_cleanup_bridge','85c0740950')]
    for key,symbol,hex_guard in rows:
        entry=spec[key];guard=bytes.fromhex(hex_guard)
        assert op.get_data(entry-0x400000,len(guard))==guard
        if staged is None:
            patch=ep.get_data(entry-0x400000,len(guard))
            assert patch[0]==0xe9 and patch[5:]==b'\x90'*(len(guard)-5)
            assert symbols is not None and entry+5+struct.unpack('<i',patch[1:5])[0]==symbols[symbol]
    results=[]
    for i,case in enumerate(cases()):
        old=run(original,op,spec,case,staged);new=run(edited,ep,spec,case,staged,True)
        differences=[k for k in old if old[k]!=new[k]]
        assert not differences,(i,case,differences,{k:(old[k],new[k]) for k in differences if k not in ('arena','frame')})
        results.append(dict(inputs=case,calls=old['calls']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME);p.add_argument('--target',choices=['client','server','both'],default='both');p.add_argument('--staged',action='store_true');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text());original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        with (PROJECT.parent/'reports'/target/'artillery-continuations-audit.tsv').open() as f:audits=list(csv.DictReader(f,delimiter='\t'))
        assert len(audits)==4 and all(r['status']=='eligible' and r['original_sha256']==spec['sha'] for r in audits)
        staged=compile_staged(target,'artillery_continuations','bfv_artillery_unit_score_bridge','continuations-staged') if a.staged else None
        results=compare(original,edited,spec,staged,{k:int(v,16) for k,v in manifest['symbols'].items()})
        destination=work/('continuations-staged/verification.json' if a.staged else 'continuations-verification.json')
        destination.write_text(json.dumps(dict(target=target,original_sha256=spec['sha'],
            compiled_sha256=manifest['output_sha256'],source_sha256=hashlib.sha256((PROJECT/'src/artillery_continuations.c').read_bytes()).hexdigest(),
            staged=bool(staged),passed=len(results),cases=results),indent=2)+'\n')
        print(target,len(results),'continuation comparisons passed','staged' if staged else 'installed',flush=True)
