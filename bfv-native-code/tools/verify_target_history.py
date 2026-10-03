"""Compare timestamp lookup/construction; execute the original tree search.

Heap allocation and native tree insertion are controlled ABI fixtures. The edited
test enters through the patched native address, including its five-byte detour.
"""
import argparse
import hashlib
import itertools
import json
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,
    UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_EDI)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK


def run_history(image,pe,spec,shape,key,duplicate,mutation):
    m=load_machine(image,pe)
    behavior=ARENA;header=ARENA+0x200;alternate=ARENA+0x300
    allocated=ARENA+0x4000;stop=ARENA+0x5000
    def w32(address,value):m.mem_write(address,struct.pack('<I',value))
    def r32(address):return struct.unpack('<I',m.mem_read(address,4))[0]
    w32(behavior+0x3c,header)
    for sentinel in [header,alternate]:
        m.mem_write(sentinel+21,b'\x01');w32(sentinel,sentinel);w32(sentinel+8,sentinel)
    keys=[[],[0],[0,1,0x7fffffff,0x80000000,0xfffffffe,0xffffffff],
          [3,7,11],[7,7,7],[0,1,0x7fffffff,0x80000000,0xfffffffe,0xffffffff]][shape]
    nodes=[ARENA+0x600+i*0x100 for i in range(len(keys))]
    for i,(node,k) in enumerate(zip(nodes,keys)):
        w32(node+12,k);w32(node+16,ARENA+0x2000+i*4)
        w32(ARENA+0x2000+i*4,0x3f800000+i);m.mem_write(node+21,b'\x00')
    def tree(indices,parent):
        if not indices:return header
        middle=0 if shape==5 else len(indices)//2
        i=indices[middle];node=nodes[i]
        w32(node+4,parent);w32(node,tree(indices[:middle],node));w32(node+8,tree(indices[middle+1:],node))
        return node
    w32(header+4,tree(list(range(len(keys))),header));w32(alternate+4,alternate)
    m.mem_write(allocated,b'\x95'*16)
    allocator=spec['vector_allocator'];insert=spec['target_history_insert']
    m.mem_write(allocator,b'\xb8'+struct.pack('<I',allocated)+b'\xc3')
    m.mem_write(insert,b'\xb8'+struct.pack('<I',0)+b'\xc2\x08\x00')
    esp=STACK+0x8000;m.mem_write(esp,struct.pack('<2I',stop,key))
    m.reg_write(UC_X86_REG_ECX,behavior);m.reg_write(UC_X86_REG_ESP,esp)
    saved={reg:0x11223344+i for i,reg in enumerate([UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_EDI])}
    for reg,value in saved.items():m.reg_write(reg,value)
    calls=[];returned=[]
    def hook(uc,address,size,data):
        if address==stop:returned.append(True);uc.emu_stop();return
        sp=uc.reg_read(UC_X86_REG_ESP)
        if address==allocator:
            assert r32(sp+4)==4;calls.append(['allocate',4])
            if mutation=='header':w32(behavior+0x3c,alternate)
        if address==insert:
            assert uc.reg_read(UC_X86_REG_ECX)==behavior+0x38
            result,pair=struct.unpack('<2I',uc.mem_read(sp+4,8));handle,pointer=struct.unpack('<2I',uc.mem_read(pair,8))
            assert handle==key and pointer==allocated and r32(pointer)==0xc61c4000
            calls.append(['insert',handle,pointer,r32(pointer),duplicate])
            w32(result,nodes[0] if duplicate and nodes else header)
            uc.mem_write(result+4,bytes([0 if duplicate else 1]))
            if mutation=='timestamp':w32(pointer,0x3f812345)
    m.hook_add(UC_HOOK_CODE,hook)
    m.emu_start(spec['target_history'],stop+1,timeout=1_000_000,count=10000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==esp+8
    for reg,value in saved.items():assert m.reg_read(reg)==value
    result=m.reg_read(UC_X86_REG_EAX)
    present=key in keys
    if present:
        assert calls==[]
        # Lower_bound picks the first equal key in the in-order sequence.
        assert result==ARENA+0x2000+keys.index(key)*4
    else:assert result==allocated and len(calls)==2
    return dict(calls=calls,result=result,value_bits=bytes(m.mem_read(result,4)).hex(),
                header=r32(behavior+0x3c),allocation=bytes(m.mem_read(allocated,16)).hex(),
                existing_values=bytes(m.mem_read(ARENA+0x2000,len(keys)*4)).hex())


def compare_target_history(original,original_pe,edited,edited_pe,spec,symbols):
    jump=edited_pe.get_data(spec['target_history']-0x400000,5)
    assert jump[0]==0xe9
    assert (spec['target_history']+5+struct.unpack('<i',jump[1:])[0])==symbols['bfv_target_history']
    cases=[]
    for args in itertools.product(range(6),(0,1,2,3,6,7,8,11,0x7fffffff,0x80000000,0xfffffffe,0xffffffff),
                                  (False,True),('none','header','timestamp')):
        try:
            old=run_history(original,original_pe,spec,*args)
            new=run_history(edited,edited_pe,spec,*args)
            assert old==new,(args,old,new)
        except Exception as error:raise RuntimeError(f'target history inputs {args}') from error
        cases.append(dict(inputs=args,**old))
    return cases


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');args=p.parse_args()
    for target in ['client','server'] if args.target=='both' else [args.target]:
        spec=TARGETS[target];manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
        original=(args.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_target_history(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        print(f'{target}: {len(cases)} target-history comparisons passed',flush=True)
