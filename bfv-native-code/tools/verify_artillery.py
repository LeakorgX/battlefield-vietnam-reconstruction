"""Compare artillery driver routing against the original x86 methods.

Target selection and component conversion are controlled external services.
The component predicate executes its real original body, not a boolean stub.
"""
import argparse
import hashlib
import itertools
import json
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP
from build import PROJECT, GAME, TARGETS
from native_oracle import load_machine, ARENA, STACK


def run_artillery(image, pe, entry, spec, predicate, eligible, recompute,
                  scale_bits, score, changing, relocate):
    m = load_machine(image, pe)
    names = ['bot','bot_vt','behavior','driver','driver_body','driver_vt',
             'entries','entries_vt','gun','component','owner','receiver',
             'receiver_vt','notified','notify_owner','notify_owner_vt',
             'base','view','view_vt','scorer','scorer_vt','fallback','fallback_vt',
             'pool','handles','ratings','flags','alt_ratings','alt_flags']
    a = {name: ARENA + i*0x200 for i,name in enumerate(names)}
    def w32(p, v): m.mem_write(p, struct.pack('<I', v))
    def field(name, offset, other): w32(a[name]+offset,a[other])
    for obj,vt in [('bot','bot_vt'),('driver_body','driver_vt'),('entries','entries_vt'),
                   ('receiver','receiver_vt'),('notify_owner','notify_owner_vt'),
                   ('view','view_vt'),('scorer','scorer_vt'),('fallback','fallback_vt')]:
        field(obj,0,vt)
    field('behavior',0x1c,'ratings'); field('behavior',0x24,'flags')
    field('behavior',0x30,'fallback'); field('behavior',0x34,'scorer')
    field('driver',0x20,'driver_body'); field('driver',0x2c,'component')
    field('gun',0x30,'component'); field('component',4,'owner')
    field('owner',0x20,'receiver'); field('notified',4,'notify_owner')
    w32(spec['pool'],a['pool']); field('pool',0,'handles')
    for i,name in enumerate(['driver','gun']):
        w32(a['handles']+i*8,a[name])
        m.mem_write(a['handles']+i*8+6,struct.pack('<H',2))
    for name in ['ratings','alt_ratings']:
        m.mem_write(a[name],struct.pack('<4f',-11,-22,-33,-44))
    for name in ['flags','alt_flags']: m.mem_write(a[name],bytes([0x95]*4))
    cursor=ARENA+0x5000
    stubs={}
    score_data=ARENA+0x6000; m.mem_write(score_data,struct.pack('<d',score))
    def stub(name, value, pop=0, floating=False, address=None):
        nonlocal cursor
        if address is None: address=cursor; cursor+=0x40
        code=(b'\xdd\x05'+struct.pack('<I',score_data)) if floating else (b'\xb8'+struct.pack('<I',value))
        code+=b'\xc2'+struct.pack('<H',pop) if pop else b'\xc3'
        m.mem_write(address,code); stubs[address]=name
        return address
    def method(obj,offset,name,value,pop=0,floating=False):
        w32(a[obj]+offset,stub(name,value,pop,floating))
    method('bot_vt',0xcc,'driver_handle',0x20001)
    method('bot_vt',0xd4,'entry_id',0xabcdef01)
    method('bot_vt',0xdc,'selector',0)
    method('driver_vt',0x8c,'entry_view',a['entries'],4)
    method('entries_vt',0x5c,'gun_handle',0x20002,4)
    method('receiver_vt',0xa0,'notify',a['notified'],4)
    method('notify_owner_vt',0x14,'base',a['base'])
    stub('convert',a['view'],address=spec['artillery_component_view'])
    method('view_vt',0x18,'predicate',predicate)
    method('scorer_vt',0x10,'eligible',eligible,4)
    method('fallback_vt',0x24,'fallback',0,12,True)
    stub('score',0,20,True,spec['artillery_score'])
    stop=ARENA+0x6106; result=ARENA+0x6200; trampoline=stop-6
    m.mem_write(trampoline,b'\xd9\x1d'+struct.pack('<I',result)+b'\x90')
    sp=STACK+0x8000
    m.mem_write(sp,struct.pack('<4I',trampoline,a['bot'],recompute,scale_bits))
    m.reg_write(UC_X86_REG_ECX,a['behavior']); m.reg_write(UC_X86_REG_ESP,sp)
    calls=[]; selected=0; returned=[]
    def hook(uc,address,size,data):
        nonlocal selected
        if address==stop: returned.append(True); uc.emu_stop(); return
        name=stubs.get(address)
        if not name: return
        esp=uc.reg_read(UC_X86_REG_ESP)
        ecx=uc.reg_read(UC_X86_REG_ECX)
        args=[]
        owner={'driver_handle':'bot','entry_id':'bot','selector':'bot',
               'entry_view':'driver_body','gun_handle':'entries','notify':'receiver',
               'base':'notify_owner','convert':'base','predicate':'view',
               'eligible':'scorer','score':'scorer','fallback':'fallback'}[name]
        assert ecx==a[owner],(name,hex(ecx),owner)
        if name=='entry_view':
            output=struct.unpack('<I',uc.mem_read(esp+4,4))[0]
            # The out slot is temporary stack storage; do not compare addresses.
            assert STACK <= output < STACK+0x10000
            assert struct.unpack('<I',uc.mem_read(output,4))[0]==a['bot']
            uc.mem_write(output,struct.pack('<I',0x13579bdf))
        else:
            count={'gun_handle':1,'notify':1,'eligible':1,'score':5,'fallback':3}.get(name,0)
            args=list(struct.unpack('<'+'I'*count,uc.mem_read(esp+4,count*4))) if count else []
        if name=='selector':
            index=selected if changing else 0
            uc.mem_write(address+1,struct.pack('<I',index)); selected+=1
            if relocate:
                field('behavior',0x1c,'alt_ratings' if selected==1 else 'ratings')
                field('behavior',0x24,'alt_flags' if selected==2 else 'flags')
        calls.append([name,args])
    m.hook_add(UC_HOOK_CODE,hook)
    m.emu_start(entry,stop+1,timeout=1_000_000,count=10000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==sp+16
    assert ['notify',[3]] in calls
    expected=[a['bot'],recompute,scale_bits]
    if predicate & 255:
        assert ['score',expected+[a['gun'],a['component']]] in calls
        assert ['eligible',[a['bot']]] in calls
        assert not any(x[0]=='fallback' for x in calls)
    else:
        assert ['fallback',expected] in calls
        assert not any(x[0]=='score' for x in calls)
    assert selected==3
    return dict(calls=calls,result_bits=bytes(m.mem_read(result,4)).hex(),
                **{name:bytes(m.mem_read(a[name],16 if 'ratings' in name else 4)).hex()
                   for name in ['ratings','flags','alt_ratings','alt_flags']})


def compare_artillery(original, original_pe, edited, edited_pe, spec, symbols):
    cases=[]
    for args in itertools.product((0,1,256,257),(0,1,256,257),(0,1,256),
             (0x00000000,0x80000000,0x3eaaaaab,0x7fc12345),
             (-0.0,1.0000000596046448),(False,True),(False,True)):
        try:
            old=run_artillery(original,original_pe,spec['artillery'],spec,*args)
            new=run_artillery(edited,edited_pe,symbols['bfv_artillery'],spec,*args)
            assert old==new,(args,old,new)
        except Exception as error: raise RuntimeError(f'artillery input {args}') from error
        cases.append(dict(inputs=args,**old))
    return cases


if __name__=='__main__':
    parser=argparse.ArgumentParser(); parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument('--target',choices=['client','server','both'],default='both')
    args=parser.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target]; manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes(); edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        symbols={k:int(v,16) for k,v in manifest['symbols'].items()}
        cases=compare_artillery(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,symbols)
        print(f'{target}: {len(cases)} artillery driver comparisons passed',flush=True)
