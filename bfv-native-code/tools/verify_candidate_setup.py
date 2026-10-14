"""First-pass candidate setup, with real interface lookup and scalar/vector math.

Timestamp and position callbacks are controlled. Captures, low-byte predicates,
callback mutations, sequential aliases, all frame/arena memory, live registers
and occupied x87 state compare at the weapon-selection continuation.
"""
import argparse,csv,hashlib,itertools,json,random,struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_EBX,UC_X86_REG_ECX,UC_X86_REG_EDX,
    UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP,UC_X86_REG_ESP)
from build import PROJECT,GAME,TARGETS
from native_oracle import ARENA
from verify_category_score import setup
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import state_result,bits
from verify_final_weights import compile_staged

def run(image,pe,spec,case,staged=None,patched=False):
    m,frame,w,r,words,preload=setup(image,pe,case)
    bot,table,candidate,interface,record,alternate= [ARENA+n for n in (0x100,0x400,0x800,0xc00,0x1000,0x1200)]
    if case.get('alias')=='before':record=frame+0xac
    if case.get('alias')=='after':record=frame+0xac+8
    lookup,position=ARENA+0x8000,ARENA+0x8100
    w(bot,table);w(table+0x1a4,lookup);m.mem_write(lookup,b'\xc2\x08\x00')
    w(interface,interface+0x100);w(interface+0x100+0x2c,position);m.mem_write(position,b'\xc2\x04\x00')
    w(candidate+0x2c,0 if case.get('fallback') else interface)
    w(record,0x20001);w(alternate,0x30002)
    words(record+0x1c,case.get('point',[bits(4),bits(6),bits(3)]))
    words(alternate+0x1c,[bits(11),bits(12),bits(13)])
    w(frame+0x18,record);w(frame+0x58,case.get('reference',bits(10)))
    w(frame+0x10c,0x11223344)
    words(frame+0x128,case.get('origin',[bits(1),bits(2),bits(3)]))
    saved={UC_X86_REG_EBX:0x12345678,UC_X86_REG_ESI:bot,UC_X86_REG_EBP:candidate,UC_X86_REG_ESP:frame}
    for reg,v in {**saved,UC_X86_REG_EDI:0x34567890}.items():m.reg_write(reg,v)
    entry,start=spec['artillery_filter_accept'],ARENA+0x9000
    if staged:
        payload,symbols=staged;m.mem_write(ARENA+0xa000,payload)
        if patched:m.mem_write(entry,b'\xe9'+struct.pack('<I',(symbols['bfv_artillery_candidate_setup_bridge']-entry-5)&0xffffffff))
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    calls=[];before=[];exits=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==spec['artillery_weapons']:exits.append(True);uc.emu_stop();return
        sp=uc.reg_read(UC_X86_REG_ESP);ecx=uc.reg_read(UC_X86_REG_ECX)
        mutation=case.get('mutation')
        if address==lookup:
            args=[r(sp+4),r(sp+8)];assert ecx==bot and args==[0x20001,frame+0x10c]
            calls.append(['timestamp',*args]);w(args[1],case.get('timestamp',bits(5)))
            uc.reg_write(UC_X86_REG_EAX,case.get('found',1))
            if mutation=='record':w(frame+0x18,alternate)
            elif mutation=='point':words(record+0x1c,[bits(-1),bits(7),bits(8)])
            elif mutation=='origin':words(frame+0x128,[bits(9),bits(10),bits(11)])
            elif mutation=='reference':w(frame+0x58,bits(25))
            elif mutation=='interface':w(candidate+0x2c,0)
        elif address==position:
            assert ecx==interface and r(sp+4)==frame+0xc4
            calls.append(['position',r(sp+4)]);words(r(sp+4),case.get('point',[bits(4),bits(6),bits(3)]))
            if mutation=='position_origin':words(frame+0x128,[bits(-2),bits(-3),bits(-4)])
            elif mutation=='position_record':w(frame+0x18,alternate)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg,v in saved.items():assert m.reg_read(reg)==v
    expected_edi=record+0x1c if r(frame+0x30)==0 else record
    assert m.reg_read(UC_X86_REG_EDI)==expected_edi
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),arena=bytes(m.mem_read(ARENA,0x4000)).hex(),
        calls=calls,state=state_result(m,before),edi=expected_edi,weight=r(frame+0x2c),distance=r(frame+0x28),
        direction=[r(frame+n) for n in [0x64,0x68,0x6c]],
        regs=[m.reg_read(reg) for reg in [UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_EDX,UC_X86_REG_ESP]])

def cases():
    out=[{},dict(fallback=True),dict(fallback=True,alias='before'),dict(fallback=True,alias='after')]
    out += [dict(found=x,fallback=f) for x,f in itertools.product([0,1,0x100,0x101,0xffffffff],[False,True])]
    out += [dict(mutation=x,fallback=f) for x,f in itertools.product(
        ['record','point','origin','reference','interface','position_origin','position_record'],[False,True])]
    special=[0,0x80000000,1,0x80000001,bits(-100),bits(100),0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    out += [dict(**{field:v}) for field,v in itertools.product(['timestamp','reference'],special)]
    out += [dict(timestamp=bits(10-20*x)) for x in [0,.000001,.5,.999999,1,1.000001]]
    out += [dict(point=[v,0,0],origin=[0,0,0],fallback=f) for v,f in itertools.product(
        special+[0x3effffff,0x3f000000,0x3f000001],[False,True])]
    modes=[{},dict(fallback=True,mutation='record'),dict(fallback=True,alias='before'),
        dict(point=[0,0,0],origin=[0,0,0]),dict(timestamp=0x7f812345),dict(point=[0x7f812345,0x7fc23456,1])]
    out += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        modes,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,1,2])]
    rng=random.Random(0x99f7ab)
    out += [dict(point=[rng.getrandbits(32) for _ in range(3)],origin=[rng.getrandbits(32) for _ in range(3)],
        timestamp=rng.getrandbits(32),reference=rng.getrandbits(32),fallback=bool(i&1)) for i in range(32)]
    return out

def compare(original,edited,spec,staged=None,symbols=None):
    op,ep=pefile.PE(data=original),pefile.PE(data=edited)
    entry=spec['artillery_filter_accept'];guard=bytes.fromhex('8b7c24188b0f')
    assert op.get_data(entry-0x400000,len(guard))==guard
    if not staged:
        patched=ep.get_data(entry-0x400000,len(guard))
        assert patched[0]==0xe9 and patched[5:]==b'\x90'
        assert symbols and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_candidate_setup_bridge']
    results=[]
    for i,case in enumerate(cases()):
        old=run(original,op,spec,case,staged);new=run(edited,ep,spec,case,staged,True)
        diff=[k for k in old if old[k]!=new[k]]
        assert not diff,(i,case,diff,{k:(old[k],new[k]) for k in diff if k not in ['frame','arena']})
        results.append(dict(inputs=case,calls=old['calls'],weight=old['weight'],distance=old['distance'],direction=old['direction']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');p.add_argument('--staged',action='store_true');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text())
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        with (PROJECT.parent/'reports'/target/'artillery-candidate-setup-audit.tsv').open() as f:audits=list(csv.DictReader(f,delimiter='\t'))
        assert len(audits)==1 and audits[0]['status']=='eligible' and audits[0]['original_sha256']==spec['sha']
        staged=compile_staged(target,'artillery_candidate_setup','bfv_artillery_candidate_setup_bridge','candidate-setup-staged') if a.staged else None
        results=compare(original,edited,spec,staged,{k:int(v,16) for k,v in manifest['symbols'].items()})
        destination=work/('candidate-setup-staged/verification.json' if a.staged else 'candidate-setup-verification.json')
        destination.write_text(json.dumps(dict(target=target,original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],
            staged=bool(staged),source_sha256=hashlib.sha256((PROJECT/'src/artillery_candidate_setup.c').read_bytes()).hexdigest(),
            passed=len(results),cases=results),indent=2)+'\n')
        print(target,len(results),'candidate setup comparisons passed','staged' if staged else 'installed',flush=True)
