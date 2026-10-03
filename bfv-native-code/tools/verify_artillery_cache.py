"""Execute real cached-target branches with controlled bot/pattern methods.

Candidate searches are tested only for argument and extended-result forwarding.
No fabricated candidate search is presented as a gameplay parity test.
"""
import argparse
import hashlib
import itertools
import json
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_ECX, UC_X86_REG_ESP, UC_X86_REG_FPCW,
    UC_X86_REG_FPTAG, UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP)
from build import PROJECT, GAME, TARGETS
from native_oracle import load_machine, ARENA, STACK


def run_cache(image, pe, entry, spec, allowed, target_state, recompute,
              rating_bits, changing, mutate, cw=0x37f, delegated=False):
    m=load_machine(image,pe)
    names=['bot','vt','alt_vt','behavior','pattern','pattern_vt','targets',
           'alt_targets','ratings','alt_ratings','flags','alt_flags','pool','records','target']
    a={name:ARENA+i*0x200 for i,name in enumerate(names)}
    def w32(address,value):m.mem_write(address,struct.pack('<I',value))
    def field(name,offset,other):w32(a[name]+offset,a[other])
    field('bot',0,'vt');field('pattern',0,'pattern_vt')
    for offset,name in [(8,'targets'),(12,'pattern'),(0x1c,'ratings'),(0x24,'flags')]:field('behavior',offset,name)
    w32(spec['pool'],a['pool']);field('pool',0,'records')
    handle={'valid':0x20001,'stale':0x30001,'empty':0,'null':0x20001,'invalid':0xffffffff}[target_state]
    # Include the maximum low-word index without reading unmapped memory.
    if target_state=='invalid':
        handle=0x30002
    w32(a['records'],0 if target_state=='null' else a['target'])
    m.mem_write(a['records']+6,struct.pack('<H',2));m.mem_write(a['records']+14,struct.pack('<H',2))
    for name in ['targets','alt_targets']:m.mem_write(a[name],struct.pack('<4I',*[handle]*4))
    for name in ['ratings','alt_ratings']:m.mem_write(a[name],struct.pack('<4I',rating_bits,0x3eaaaaab,0x80000000,0x7fc12345))
    for name in ['flags','alt_flags']:m.mem_write(a[name],b'\x95'*4)
    cursor=ARENA+0x4000;stubs={}
    def stub(name,value,pop=0,address=None):
        nonlocal cursor
        if address is None:address=cursor;cursor+=0x40
        code=b'\xb8'+struct.pack('<I',value)+(b'\xc2'+struct.pack('<H',pop) if pop else b'\xc3')
        m.mem_write(address,code);stubs[address]=name;return address
    def method(vt,offset,name,value,pop=0):w32(a[vt]+offset,stub(name,value,pop))
    method('pattern_vt',4,'allowed',allowed,4);method('pattern_vt',8,'reset',0,4)
    for vt in ['vt','alt_vt']:
        method(vt,0xdc,'index_'+vt,0)
        method(vt,0x78,'clear_'+vt,0,4)
        method(vt,0x160,'submit_'+vt,0,4)
    result=ARENA+0x5000;trampoline=ARENA+0x5100;stop=trampoline+6
    m.mem_write(trampoline,b'\xdb\x3d'+struct.pack('<I',result)+b'\x90')
    esp=STACK+0x8000;scale=0x7fc55555;gun=ARENA+0x5800;driver=ARENA+0x5900
    m.mem_write(esp,struct.pack('<6I',trampoline,a['bot'],recompute,scale,gun,driver))
    m.reg_write(UC_X86_REG_ECX,a['behavior']);m.reg_write(UC_X86_REG_ESP,esp);m.reg_write(UC_X86_REG_FPCW,cw)
    saved={reg:0x11223344+i for i,reg in enumerate([UC_X86_REG_EBX,UC_X86_REG_ESI,UC_X86_REG_EDI,UC_X86_REG_EBP])}
    for reg,value in saved.items():m.reg_write(reg,value)
    # Return a value whose low extended-precision bits cannot survive a double store.
    extended=bytes.fromhex('0100000000000080ff3f')
    if delegated:
        address=spec['artillery_score'];data=ARENA+0x5a00;m.mem_write(data,extended)
        m.mem_write(address,b'\xdb\x2d'+struct.pack('<I',data)+b'\xc2\x14\x00');stubs[address]='native_search'
    calls=[];index_count=0;returned=[]
    def hook(uc,address,size,data):
        nonlocal index_count
        if address==stop:returned.append(True);uc.emu_stop();return
        name=stubs.get(address)
        if not name:return
        sp=uc.reg_read(UC_X86_REG_ESP);this=uc.reg_read(UC_X86_REG_ECX)
        n=5 if name=='native_search' else 1 if name in ['allowed','reset'] or name.startswith(('submit_','clear_')) else 0
        args=list(struct.unpack('<'+'I'*n,uc.mem_read(sp+4,n*4))) if n else []
        assert this==a['behavior' if name=='native_search' else 'pattern' if name in ['allowed','reset'] else 'bot']
        calls.append([name,args])
        if name.startswith('index_'):
            uc.mem_write(address+1,struct.pack('<I',index_count if changing else 0));index_count+=1
            if mutate:
                for offset,original,alternate in [(8,'targets','alt_targets'),(0x1c,'ratings','alt_ratings'),(0x24,'flags','alt_flags')]:
                    field('behavior',offset,alternate if index_count%2 else original)
                field('bot',0,'alt_vt' if index_count%2 else 'vt')
        if name.startswith('submit_') and mutate:
            field('behavior',0x1c,'alt_ratings')
            w32(a['alt_ratings']+(3 if changing else 0)*4,0x3f812345)
    m.hook_add(UC_HOOK_CODE,hook)
    m.emu_start(entry,stop+1,timeout=1_000_000,count=10000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==esp+24
    assert m.reg_read(UC_X86_REG_FPCW)==cw and m.reg_read(UC_X86_REG_FPTAG)==0xffff
    for reg,value in saved.items():assert m.reg_read(reg)==value
    if delegated:
        assert calls==[['native_search',[a['bot'],recompute,scale,gun,driver]]]
        assert bytes(m.mem_read(result,10))==extended
    else:
        assert calls[0]==['allowed',[a['bot']]]
        if not (allowed&255):assert index_count==2
        elif target_state=='valid':assert index_count==4
        else:
            assert index_count==3
            assert any(name.startswith('clear_') and args==[0xffffffff] for name,args in calls)
            assert ['reset',[a['bot']]] in calls
    return dict(calls=calls,result80=bytes(m.mem_read(result,10)).hex(),
       **{name:bytes(m.mem_read(a[name],16 if 'ratings' in name else 4)).hex()
          for name in ['ratings','alt_ratings','flags','alt_flags']})


def compare_artillery_cache(original, original_pe, edited, edited_pe, spec, symbols):
    inputs=[(*args,0x37f,False) for args in itertools.product((0,1,256,257),
        ('valid','stale','empty','null','invalid'),(0,256),
        (0,0x80000000,0x7f812345,0x7f800000),(False,True),(False,True))]
    inputs += [(1,state,0,0x7f812345,True,True,0x7f|pc|rc,False)
        for state,pc,rc in itertools.product(('valid','stale'),(0,0x200,0x300),(0,0x400,0x800,0xc00))]
    inputs += [(1,'valid',recompute,0,False,False,0x7f|pc|rc,True)
        for recompute,pc,rc in itertools.product((1,257,0xffffffff),(0,0x200,0x300),(0,0x400,0x800,0xc00))]
    cases=[]
    for args in inputs:
        try:
            old=run_cache(original,original_pe,spec['artillery_score'],spec,*args)
            new=run_cache(edited,edited_pe,symbols['bfv_artillery_evaluate'],spec,*args)
            assert old==new,(args,old,new)
        except Exception as error:raise RuntimeError(f'artillery evaluator inputs {args}') from error
        cases.append(dict(inputs=args,**old))
    from verify_artillery import run_artillery
    for allowed,handle,eligible in itertools.product((0,1),(0x20002,0),(0,1)):
        args=(1,eligible,0,0x3eaaaaab,0.0,False,False)
        cache=dict(allowed=allowed,handle=handle)
        old=run_artillery(original,original_pe,spec['artillery'],spec,*args,real_cache=cache)
        new=run_artillery(edited,edited_pe,symbols['bfv_artillery'],spec,*args,
                          scorer_entry=symbols['bfv_artillery_evaluate'],real_cache=cache)
        assert old==new,(cache,eligible,old,new)
        cases.append(dict(integration=True,inputs=[allowed,handle,eligible],**old))
    return cases


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument('--target',choices=['client','server','both'],default='both');args=parser.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        symbols={k:int(v,16) for k,v in manifest['symbols'].items()}
        cases=compare_artillery_cache(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,symbols)
        print(f'{target}: {len(cases)} artillery evaluator comparisons passed',flush=True)
