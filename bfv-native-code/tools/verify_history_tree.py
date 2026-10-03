"""Compare complete target-history tree mutations with original x86 execution.

The game node allocator, constructor and protected normal-return path execute.
Only the raw heap allocation is controlled for normal insertions. Capacity-error
tests separately control the string/exception services and inspect throw inputs.
"""
import argparse
import hashlib
import itertools
import json
import random
import struct
from pathlib import Path
import pefile
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import (UC_X86_REG_EAX,UC_X86_REG_ECX,UC_X86_REG_ESP,
    UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_EDI,
    UC_X86_REG_FPCW,UC_X86_REG_FPSW,UC_X86_REG_FPTAG)
from build import PROJECT,GAME,TARGETS
from native_oracle import load_machine,ARENA,STACK
from verify_constant_returns import FLOAT_STATE

MAP=ARENA;HEADER=ARENA+0x100;RESULT=ARENA+0x200;PAIR=ARENA+0x240
HEAP=ARENA+0x1000;STOP=ARENA+0xf000
SAVED=[UC_X86_REG_EBX,UC_X86_REG_EBP,UC_X86_REG_ESI,UC_X86_REG_EDI]

class Runner:
    def __init__(self,image,pe,spec,symbols=None):
        self.m=load_machine(image,pe);self.spec=spec;self.symbols=symbols
        self.m.mem_map(0,4096);self.m.mem_write(STOP,b'\x90')
        self.m.mem_write(spec['vector_allocator'],b'\xc3')
        self.m.hook_add(UC_HOOK_CODE,self.hook)
        self.allocated=[];self.returned=False;self.next_node=HEAP;self.mode='normal';self.calls=[]

    def w(self,p,v):self.m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def r(self,p):return struct.unpack('<I',self.m.mem_read(p,4))[0]
    def byte(self,p):return self.m.mem_read(p,1)[0]
    def hook(self,uc,address,size,data):
        if address==STOP:self.returned=True;uc.emu_stop();return
        sp=uc.reg_read(UC_X86_REG_ESP)
        if address==self.spec['vector_allocator']:
            count=self.r(sp+4);assert count in [4,24],count
            pointer=self.next_node;self.next_node+=32
            uc.reg_write(UC_X86_REG_EAX,pointer)
            self.allocated.append((pointer,count))
            return
        if self.mode!='error':return
        this=uc.reg_read(UC_X86_REG_ECX)
        if address==self.spec['tree_string_assign']:
            source,count=self.r(sp+4),self.r(sp+8)
            assert source==self.spec['tree_length_message'] and count==19
            assert self.r(this+20)==0 and self.r(this+24)==15 and self.byte(this+4)==0
            self.calls.append(['string_assign',bytes(uc.mem_read(source,19)).hex(),count])
            self.error_text=this;self.w(this+20,19);self.w(this+24,31);self.w(this+4,ARENA+0xe000)
        elif address==self.spec['tree_exception_construct']:
            self.calls.append(['exception_construct']);self.error_object=this
            self.w(this,0);self.w(this+4,0);self.w(this+8,0)
        elif address==self.spec['tree_string_copy']:
            source,position,length=[self.r(sp+n) for n in [4,8,12]]
            assert this==self.error_object+12 and source==self.error_text
            assert (position,length)==(0,0xffffffff)
            assert self.r(this-12)==self.spec['tree_logic_error_vtable']
            assert self.r(this+20)==0 and self.r(this+24)==15 and self.byte(this+4)==0
            self.calls.append(['string_copy',position,length])
            self.w(this+4,ARENA+0xe000);self.w(this+20,19);self.w(this+24,31)
        elif address==self.spec['vector_throw']:
            obj,info=self.r(sp+4),self.r(sp+8)
            assert obj==self.error_object and info==self.spec['tree_length_throw_info']
            assert self.r(obj)==self.spec['tree_length_error_vtable']
            assert self.r(obj+4)==0 and self.r(obj+8)==0 and self.r(obj+32)==19
            self.calls.append(['throw',self.r(obj),info,self.r(obj+16),self.r(obj+32),self.r(obj+36)])
            self.returned=True;uc.emu_stop()

    def reset(self):
        self.m.mem_write(ARENA,b'\xa5'*0xe000)
        self.w(0,0xffffffff);self.w(MAP+4,HEADER);self.w(MAP+8,0)
        for offset in [0,4,8]:self.w(HEADER+offset,HEADER)
        self.m.mem_write(HEADER+20,b'\x01\x01')
        self.next_node=HEAP;self.allocated=[];self.calls=[]

    def invoke(self,entry,this,args,returns_pointer=None,depth=0):
        sp=STACK+0x8000;self.returned=False
        self.m.mem_write(sp,struct.pack('<'+'I'*(1+len(args)),STOP,*args))
        self.m.reg_write(UC_X86_REG_ESP,sp);self.m.reg_write(UC_X86_REG_ECX,this)
        saved={reg:0x12345678+i for i,reg in enumerate(SAVED)}
        for reg,val in saved.items():self.m.reg_write(reg,val)
        # Occupied register tags are retained unchanged: these integer routines
        # must not perform any floating-point operations, including cleanup.
        self.m.reg_write(UC_X86_REG_FPCW,0x27f if depth else 0x37f)
        self.m.reg_write(UC_X86_REG_FPSW,0x3041 if depth else 0)
        self.m.reg_write(UC_X86_REG_FPTAG,0x0fff if depth else 0xffff)
        before=[self.m.reg_read(reg) for reg in FLOAT_STATE]
        self.m.emu_start(entry,0,timeout=2_000_000,count=100000)
        assert self.returned,hex(entry)
        if self.mode=='error':return
        assert self.m.reg_read(UC_X86_REG_ESP)==sp+4+4*len(args)
        for reg,val in saved.items():assert self.m.reg_read(reg)==val
        assert [self.m.reg_read(reg) for reg in FLOAT_STATE]==before
        assert self.r(0)==0xffffffff
        if returns_pointer is not None:assert self.m.reg_read(UC_X86_REG_EAX)==returns_pointer

    def snapshot(self):
        return bytes(self.m.mem_read(ARENA,0xe000)),list(self.allocated)

    def invariants(self,expected):
        header=self.r(MAP+4);root=self.r(header+4);seen=set();order=[]
        def visit(node,parent):
            if node==header:return 1
            assert node not in seen;seen.add(node)
            assert not self.byte(node+21) and self.r(node+4)==parent
            left,right=self.r(node),self.r(node+8)
            if not self.byte(node+20):assert self.byte(left+20) and self.byte(right+20)
            a=visit(left,node);order.append(self.r(node+12));b=visit(right,node)
            assert a==b,('black height',node,a,b)
            return a+bool(self.byte(node+20))
        assert self.byte(root+20)==1
        visit(root,header)
        assert order==sorted(expected) and self.r(MAP+8)==len(expected)
        if expected:
            assert self.r(self.r(header)+12)==min(expected)
            assert self.r(self.r(header+8)+12)==max(expected)
        else:assert root==header and self.r(header)==header and self.r(header+8)==header
        return sorted(seen)

def compare_history_tree(original,original_pe,edited,edited_pe,spec,symbols):
    old,new=Runner(original,original_pe,spec),Runner(edited,edited_pe,spec,symbols)
    for key,symbol in [('target_history_insert','bfv_history_insert'),('history_insert_node','bfv_history_insert_node'),
        ('tree_previous','bfv_tree_previous'),('tree_rotate_left','bfv_tree_rotate_left'),
        ('tree_rotate_right','bfv_tree_rotate_right'),('tree_construct','bfv_tree_construct')]:
        addr=spec[key];jump=edited_pe.get_data(addr-0x400000,5)
        assert jump[0]==0xe9 and addr+5+struct.unpack('<i',jump[1:])[0]==symbols[symbol]
    cases=[];rng=random.Random(0x99ef80)
    sequences=list(itertools.permutations([0,1,0x7fffffff,0x80000000,0xffffffff]))
    sequences += [list(range(63)),list(reversed(range(63))),[7]*32]
    sequences += [[rng.getrandbits(32) for _ in range(31)] for _ in range(24)]
    for sequence_index,seq in enumerate(sequences):
        for r in [old,new]:r.reset()
        expected=set()
        for i,key in enumerate(seq):
            inserted=key not in expected
            for r in [old,new]:
                r.w(PAIR,key);r.w(PAIR+4,0xfeed0000+i)
                r.m.mem_write(RESULT,b'\xcc'*8)
                r.invoke(spec['target_history_insert'],MAP,[RESULT,PAIR],RESULT,sequence_index%2)
                assert r.byte(RESULT+4)==inserted and bytes(r.m.mem_read(RESULT+5,3))==b'\xcc'*3
                assert r.r(r.r(RESULT)+12)==key
            expected.add(key)
            assert old.snapshot()==new.snapshot(),('sequence',sequence_index,i,key)
            old.invariants(expected);new.invariants(expected)
            cases.append(dict(kind='insertion',sequence=sequence_index,index=i,key=key,inserted=inserted))
        # Repeat every key to require duplicate detection without allocations.
        for key in seq:
            for r in [old,new]:
                r.w(PAIR,key);r.w(PAIR+4,0xbadcafe)
                r.invoke(spec['target_history_insert'],MAP,[RESULT,PAIR],RESULT)
                assert not r.byte(RESULT+4)
            assert old.snapshot()==new.snapshot()
            cases.append(dict(kind='duplicate',sequence=sequence_index,key=key))
        # Predecessor of every node, the sentinel, and begin.
        for node in [HEADER,*old.invariants(expected)]:
            for r in [old,new]:
                r.w(RESULT,node);r.invoke(spec['tree_previous'],RESULT,[],depth=1)
            assert old.snapshot()==new.snapshot()
            cases.append(dict(kind='previous',sequence=sequence_index,node=node))
    # Exercise the existing timestamp helper with real insertion and native
    # allocator protection, including repeat lookups and unsigned key boundaries.
    for r in [old,new]:r.reset()
    for i,key in enumerate([0,0xffffffff,0x80000000,1,0x7fffffff,2,1,0,0xffffffff]):
        for r in [old,new]:
            r.invoke(spec['target_history'],MAP-0x38,[key],depth=i%2)
            value=r.m.reg_read(UC_X86_REG_EAX);assert r.r(value)==0xc61c4000
        assert old.snapshot()==new.snapshot()
        assert old.m.reg_read(UC_X86_REG_EAX)==new.m.reg_read(UC_X86_REG_EAX)
        cases.append(dict(kind='timestamp_integration',key=key))
    # Full constructor memory effects, including field/output aliasing and the
    # preserved final two padding bytes. Each pair may overlap its destination.
    for offset,color in itertools.product([0,4,8,12,16,24,64],[0,1,255,256,257,0xffffffff]):
        for r in [old,new]:
            r.reset();node=HEAP;r.w(node+offset,0x81234567);r.w(node+offset+4,0xf1234567)
            r.invoke(spec['tree_construct'],node,[0xaaaa0000,0xbbbb0000,0xcccc0000,node+offset,color],node)
        assert old.snapshot()==new.snapshot(),('constructor',offset,color)
        cases.append(dict(kind='constructor',offset=offset,color=color))
    # The capacity limit is unsigned and checked before allocation or mutation.
    for count in [0x1ffffffe,0x1fffffff,0x7fffffff,0x80000000,0xffffffff]:
        for r in [old,new]:
            r.reset();r.mode='error';r.w(MAP+8,count)
            for key,cleanup in [('tree_string_assign',8),('tree_string_copy',12),('tree_exception_construct',0)]:
                r.m.mem_write(spec[key],b'\xc2'+struct.pack('<H',cleanup))
                r.m.ctl_remove_cache(spec[key],spec[key]+16)
            r.invoke(spec['history_insert_node'],MAP,[RESULT,1,HEADER,PAIR])
            assert not r.allocated and r.r(MAP+8)==count
            assert [c[0] for c in r.calls]==['string_assign','exception_construct','string_copy','throw']
        assert old.calls==new.calls
        cases.append(dict(kind='capacity_error',count=count,calls=old.calls))
    return cases

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for t in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[t];w=PROJECT/'build'/t;manifest=json.loads((w/'manifest.json').read_text())
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_history_tree(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
            {k:int(v,16) for k,v in manifest['symbols'].items()})
        (w/'history-tree-verification.json').write_text(json.dumps(dict(target=t,original_sha256=spec['sha'],
            compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2))
        print(f'{t}: {len(cases)} history-tree comparisons passed',flush=True)
