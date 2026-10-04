"""Compare final alternate-path scaling and rounded best-target selection.

Actual float selectors execute; the source reads the setting directly as the
original four-byte getter does. No numeric service is mocked.
"""
import itertools,struct
import argparse,hashlib,json
from pathlib import Path
import pefile
from build import TARGETS,PROJECT,GAME
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,
    UC_X86_REG_EBP,UC_X86_REG_ESP)
from native_oracle import ARENA
from verify_category_score import setup
from verify_movement_score import bits,state_result
from verify_constant_returns import FLOAT_STATE

def run_finish(image,pe,spec,case):
    m,frame,w,r,words,preload=setup(image,pe,case)
    bot,target,record,data=[ARENA+n for n in (0x100,0x300,0x500,0x700)]
    if case.get('flags_alias'):target=frame+4
    w(bot+0x2c,case.get('limit',3));w(target+0x10,case.get('flags',8))
    w(record+8,data);w(data,0xabcdef01)
    for offset,value in [(0x40,record),(0x60,3),(0x28,case.get('distance',bits(10))),
        (0x1c,case.get('parameter',bits(100))),(0x14,case.get('score',bits(2))),
        (0x48,case.get('best',bits(0))),(0x5c,case.get('sum',bits(2.5))),
        (0x20,case.get('movement_score',bits(.8))),(0x74,case.get('aim_weight',bits(.9))),
        (0x34,case.get('weapon_score',bits(1.1))),(0x2c,case.get('weight',bits(1.2)))]:w(frame+offset,value)
    if case.get('identity_alias'):w(record+8,frame+0x48)
    if case.get('setting_alias'):
        bot=frame-0x18
        # The setting is then the score after scaling at frame+0x14, interpreted
        # as a signed integer, before its frame+0x30 store.
    saved={UC_X86_REG_EBX:0x11223344,UC_X86_REG_ESI:bot,UC_X86_REG_EDI:0,
        UC_X86_REG_EBP:target,UC_X86_REG_ESP:frame}
    for reg,value in saved.items():m.reg_write(reg,value)
    entry,start=spec['artillery_nested_finish'],ARENA+0x9000
    m.mem_write(start,preload+b'\xe9'+struct.pack('<I',(entry-start-len(preload)-5)&0xffffffff))
    before=[];exits=[]
    def hook(uc,address,size,unused):
        if address==entry:before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address==spec['artillery_filter_reject']:exits.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert exits
    for reg,value in saved.items():assert m.reg_read(reg)==value
    selected=r(frame+0x4c)==r(frame+0x60)
    if 'expect_selected' in case:assert selected==case['expect_selected']
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),memory=bytes(m.mem_read(ARENA,0x4000)).hex(),
        state=state_result(m,before),score=r(frame+0x14),selected=selected)

def finish_cases():
    cases=[{},dict(flags=0),dict(flags=7),dict(flags=0xffffffff),dict(identity_alias=True),
        dict(flags_alias=True,score=0x3f812345),dict(setting_alias=True)]
    cases += [dict(limit=v) for v in [0,1,2,3,0x7fffffff,0x80000000,0xffffffff]]
    special=[0,0x80000000,1,0x80000001,bits(-100),bits(100),0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(**{field:v}) for field,v in itertools.product(
        ['distance','parameter','score','best','sum','movement_score','aim_weight','weapon_score','weight'],special)]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(
        [{},dict(sum=0x7f812345),dict(score=0x7f812345),dict(identity_alias=True),dict(setting_alias=True)],
        [0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,5])]
    # Unlike the earlier category stage, this comparison reloads the rounded
    # score. Equal stored bits must never select, including 53/64-bit precision.
    rounded={127:1057521183,1151:1057521180,2175:1057521185,3199:1057521180,
        639:1057521183,1663:1057521182,2687:1057521183,3711:1057521182,
        895:1057521183,1919:1057521182,2943:1057521183,3967:1057521182}
    cases += [dict(cw=cw,best=value+delta,expect_selected=delta<0) for cw,value in rounded.items() for delta in [-1,0,1]]
    return cases

def compare_alternate_finish(original,original_pe,edited,edited_pe,spec,symbols):
    entry=spec['artillery_nested_finish'];guard=bytes.fromhex('d944242851')
    assert original_pe.get_data(entry-original_pe.OPTIONAL_HEADER.ImageBase,len(guard))==guard
    patched=edited_pe.get_data(entry-edited_pe.OPTIONAL_HEADER.ImageBase,len(guard))
    assert patched[0]==0xe9 and entry+5+struct.unpack('<i',patched[1:5])[0]==symbols['bfv_artillery_alternate_finish_bridge']
    results=[]
    for i,case in enumerate(finish_cases()):
        old=run_finish(original,original_pe,spec,case);new=run_finish(edited,edited_pe,spec,case)
        assert old==new,(i,case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
        results.append(dict(inputs=case,score=old['score'],selected=old['selected']))
    return results

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target
        manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha'] and hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        results=compare_alternate_finish(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'alternate-finish-verification.json').write_text(json.dumps(dict(target=target,
            original_sha256=spec['sha'],compiled_sha256=manifest['output_sha256'],passed=len(results),cases=results),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(results)} alternate finish comparisons passed',flush=True)
