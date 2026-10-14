"""Differential verifier for L1 lifted functions.

The lifted C (lifted/<target>/lifted.c plus the runtime) is compiled with the
production flags and linked as a standalone blob that is mapped next to the
original image. For every catalogued function, the native body and the lifted
entry stub run on two identical emulated machines from many random states:
registers and stack arguments are arena pointers, small integers or float bits,
and the arena holds a mix of pointers, integers and floats. Optional values on
the x87 stack exercise functions that consume st(0).

Both runs must end at the same place with identical general registers (ESP
included), x87 registers/control/status/tag words, MXCSR and XMM registers, and
an identical ordered list of memory accesses (address, size, written value).
Accesses below the final ESP are dead stack for the native code and are
excluded, as is the lifted code's own frame. EFLAGS are scratch at a native
return and are not compared. A state on which the native code faults or runs
out of budget is skipped; the lifted code must then not complete it. A function
passes when every completed state matches and at least one state completed.

Callees are not run. Each direct call/jump target outside the function becomes
`mov eax, K; ret N` (N from the callee's own `ret N`); random words sometimes
point at 32 such stubs at FUNCS, so calls through random objects are recorded
too; and a call into non-code memory (the arena, the stack, a lazily mapped
page, an unmapped address; every import address table slot holds one) is an
opaque callee that returns K(target) to its caller (from the 17th such call of a
run, varying results that are sometimes 0, so list walks through them end). The
return address a callee pops stays behind as dead stack; on the lifted machine
it is rewritten to the native return address its resume stub stands for. A state whose native run loses the stack or
returns to an address no call returns to is retried once with EBP pointing into
an entry stack of STOP words (code entered mid-function uses its parent's
frame). Up to three times as many states are drawn when they do not complete, the
later ones small (counts and indices near zero, arena words pointing at
themselves). A native run that still runs out of budget or leaves the function
is skipped without running the lifted body; when that leaves a function with no
completed state and some ran out of budget, the states are drawn again with a
native budget of BIG_BUDGET instructions. Only .text is executable, and an
indirect call made by the function into .text is opaque on both machines too
(real code reached through a random pointer may read its return address). The
frame below the entry stub (the default gap, or the body's own gap for
_bfv_l1_enter_deep) is the
lifted code's own and is excluded from the comparison. Runs are bounded by an
instruction count and EVENT_LIMIT events; a run stopped by the RUN_SECONDS
wall-clock limit instead is inconclusive and skipped.
Every call is an event: target, all general registers and 20 bytes of
stack arguments. Pages touched outside the mapped memory are mapped on demand
with contents that depend only on the page and the state (at most LAZY_PAGES).
Before every run the image, the stubs and the blob are restored and stale
translations dropped, so each state of each function starts from the same
machine.

Results carry each function's c_sha256; build.py detours only functions whose
current text and runtime (runtime_sha256) are exactly the verified ones, and
--reuse keeps earlier results for unchanged functions (passes also across
verifier versions).
"""
import argparse
import csv
import hashlib
import json
import random
import re
import struct
import time
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
import pefile
from capstone import Cs,CS_ARCH_X86,CS_MODE_32,CS_OP_IMM,CS_OP_REG
from unicorn import Uc,UcError,UC_ARCH_X86,UC_MODE_32,UC_HOOK_CODE,UC_HOOK_BLOCK,UC_HOOK_MEM_READ,UC_HOOK_MEM_WRITE,UC_MEM_WRITE
from unicorn import UC_HOOK_MEM_READ_UNMAPPED,UC_HOOK_MEM_WRITE_UNMAPPED,UC_PROT_READ,UC_PROT_WRITE
from unicorn import UC_ERR_FETCH_UNMAPPED,UC_ERR_FETCH_PROT
from unicorn import x86_const as U
from build import PROJECT,GAME,TARGETS,COMPILERS,run
from native_oracle import ARENA,STACK

BLOB=0x20000000
FLAGS=['-m32','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-array-bounds','-ffreestanding','-fno-builtin',
       '-frounding-math','-fno-delete-null-pointer-checks','-fno-stack-protector','-fno-asynchronous-unwind-tables','-fno-unwind-tables','-mgeneral-regs-only']
GPRS=[U.UC_X86_REG_EAX,U.UC_X86_REG_ECX,U.UC_X86_REG_EDX,U.UC_X86_REG_EBX,U.UC_X86_REG_ESP,U.UC_X86_REG_EBP,
      U.UC_X86_REG_ESI,U.UC_X86_REG_EDI]
FPU=[U.UC_X86_REG_FPCW,U.UC_X86_REG_FPSW,U.UC_X86_REG_FPTAG,*[getattr(U,f'UC_X86_REG_FP{n}') for n in range(8)]]
SSE=[U.UC_X86_REG_MXCSR,*[getattr(U,f'UC_X86_REG_XMM{n}') for n in range(8)]]
VERIFIER=13  # bump when the states or comparison change; old results are then not reused
ARENA_SIZE=0x10000;LAZY_PAGES=64;
RUN_SECONDS=20;EVENT_LIMIT=20_000
PREFIX_BUDGET=20_000  # native instructions of a prefix comparison
BIG_BUDGET=400_000  # native instructions for the second round of a function that only ran out of budget
# Indirect-call targets: random words sometimes point at one of these `mov eax, K; ret`
# stubs, so virtual calls through random objects reach a recorded callee.
FUNCS=0x1ff00000;FUNC_COUNT=32;IMPORTS=0x1fe00000
# Only the image, the blob, the stubs and CODE (the stop address and the x87
# loader) are executable. A transfer anywhere else (a random pointer used as a
# function) is an opaque call: recorded like a stub call, then it returns
# K(target) in EAX to its caller, identically on both machines.
CODE=0x1ff10000;STOP=CODE;LOADER=CODE+0x100;OPAQUE_CALLS=64
ENTRY_SP=STACK+0x8000
FLOATS=[0.0,1.0,-1.0,0.5,2.0,-3.25,100.0,1e-3,0.75,-0.125]

def link(target,work,lifted_dir):
    """Compile and link the lifted code at BLOB; returns (bytes, {symbol: address})."""
    work.mkdir(parents=True,exist_ok=True)
    sources=[lifted_dir/'runtime/l1_runtime.c',*sorted((lifted_dir/target).glob('lifted*.c'))]
    inputs=[*sources,lifted_dir/'runtime/l1.h']
    stamp=work/'lifted.flags'
    if (work/'lifted.bin').exists() and (work/'lifted.map').exists() and stamp.exists() and \
       stamp.read_text()==' '.join(FLAGS) and (work/'lifted.bin').stat().st_mtime>max(f.stat().st_mtime for f in inputs):
        return (work/'lifted.bin').read_bytes(),map_symbols(work)  # parts share one up-to-date link
    for old in work.glob('*.o'):old.unlink()
    stamp.unlink(missing_ok=True)
    objects=[work/(source.parent.name+'_'+source.stem+'.o') for source in sources]
    with ThreadPoolExecutor(6) as pool:
        list(pool.map(lambda so:run([COMPILERS/'gcc.exe',*FLAGS,'-c',so[0],'-o',so[1]],work),zip(sources,objects)))
    (work/'lifted.ld').write_text(f'SECTIONS {{ . = 0x{BLOB:x}; .payload : {{ *(.text*) *(.rdata*) *(.data*) *(.bss*) BYTE(0) }}'
                                  ' /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.drectve*) } }')
    run([COMPILERS/'ld.exe','-T',work/'lifted.ld','--image-base','0x400000','--entry','_bfv_l1_enter',
         '--disable-dynamicbase','--disable-reloc-section','-Map',work/'lifted.map','-o',work/'lifted.exe',*objects],work)
    run([COMPILERS/'objcopy.exe','-O','binary','--only-section','.payload',work/'lifted.exe',work/'lifted.bin'],work)
    stamp.write_text(' '.join(FLAGS))
    return (work/'lifted.bin').read_bytes(),map_symbols(work)

def trampoline(symbols):
    """[start, end) of the shared trampoline code in the blob (the default and
    the deep entry, which follows it), and the deep entry."""
    start=symbols['bfv_l1_enter'];deep=symbols['bfv_l1_enter_deep']
    return start,min(a for a in symbols.values() if a>max(start,deep)),deep

def runtime_sha256(lifted_dir):
    """Hash of the shared runtime; with a function's own c_sha256 it fixes the compiled code."""
    h=hashlib.sha256()
    for f in [lifted_dir/'runtime/l1.h',lifted_dir/'runtime/l1_runtime.c']:h.update(f.name.encode()+bytes(1)+f.read_bytes())
    return h.hexdigest()

def sources_sha256(target,lifted_dir):
    """Hash of the lifted sources; build.py only detours functions verified from these exact sources."""
    h=hashlib.sha256()
    for f in [lifted_dir/'runtime/l1.h',lifted_dir/'runtime/l1_runtime.c',*sorted((lifted_dir/target).glob('lifted*.c'))]:
        h.update(f.name.encode()+b'\0'+f.read_bytes())
    return h.hexdigest()

def map_symbols(work):
    symbols={}
    for line in (work/'lifted.map').read_text().splitlines():
        m=re.match(r'\s*(0x[0-9a-fA-F]+)\s+_?(bfv_[A-Za-z0-9_]+)\s*$',line)
        if m:symbols[m[2]]=int(m[1],16)
    return symbols

class Machine:
    def __init__(self,image,pe,blob,quiet=(0,0)):
        m=self.uc=Uc(UC_ARCH_X86,UC_MODE_32);base=pe.OPTIONAL_HEADER.ImageBase
        m.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
        m.mem_write(base,image[:pe.OPTIONAL_HEADER.SizeOfHeaders])
        for s in pe.sections:m.mem_write(base+s.VirtualAddress,s.get_data())
        # The file's import address table holds name RVAs, not entry points; each
        # slot gets its own unmapped address so an import call is an opaque callee.
        if not hasattr(pe,'DIRECTORY_ENTRY_IMPORT'):pe.parse_data_directories()
        for n,imp in enumerate(i for d in getattr(pe,'DIRECTORY_ENTRY_IMPORT',[]) for i in d.imports):
            m.mem_write(imp.address,struct.pack('<I',IMPORTS+16*n))
        # only .text is executable: a call through a garbage pointer into data is opaque
        lo,hi=text_range(pe);lo&=~0xfff;hi=(hi+0xfff)&~0xfff;end=base+((pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
        if base<lo:m.mem_protect(base,lo-base,UC_PROT_READ|UC_PROT_WRITE)
        if hi<end:m.mem_protect(hi,end-hi,UC_PROT_READ|UC_PROT_WRITE)
        m.mem_map(ARENA,ARENA_SIZE,UC_PROT_READ|UC_PROT_WRITE);m.mem_map(STACK,0x10000,UC_PROT_READ|UC_PROT_WRITE)
        m.mem_map(CODE,0x1000)
        m.mem_map(BLOB,(len(blob)+0xffff)&~0xffff);m.mem_write(BLOB,blob)
        self.blob_end=BLOB+len(blob);self.events=None
        m.mem_write(STOP,b'\x90')
        m.mem_map(FUNCS,0x1000)
        m.mem_write(FUNCS,b''.join(bytes([0xb8])+struct.pack('<I',ARENA+0x100+n*0x1f4)+bytes([0xc3,0xcc,0xcc])
                                   for n in range(FUNC_COUNT)))
        self.sites={}
        def indirect(uc,address,size,data):
            if self.events is not None and (address-FUNCS)%8==0:
                sp=uc.reg_read(U.UC_X86_REG_ESP)
                self.events.append(('call',address,tuple(uc.reg_read(r) for r in GPRS),bytes(uc.mem_read(sp+4,20))))
                self.returning(uc,ARENA+0x100+(address-FUNCS)//8*0x1f4)
        m.hook_add(UC_HOOK_CODE,indirect,begin=FUNCS,end=FUNCS+8*FUNC_COUNT)
        # A run may write the image, the function stubs or the blob (stray stores,
        # garbage executed through a random function pointer) or reload segment
        # registers; all are undone before the next run so every state, and every
        # function, starts from the same machine.
        self.image_end=base+pe.OPTIONAL_HEADER.SizeOfImage
        self.pristine={base:bytes(m.mem_read(base,self.image_end-base)),FUNCS:bytes(m.mem_read(FUNCS,0x1000)),BLOB:blob}
        self.dirty=[]
        def access(uc,kind,address,size,value,data):
            if kind==UC_MEM_WRITE and (base<=address<self.image_end or FUNCS<=address<FUNCS+0x1000 or BLOB<=address<self.blob_end):
                self.dirty.append((address,size))
            if self.events is None:return
            # A native store into the blob or well below ESP goes through a garbage
            # pointer into what the lifted machine uses for its code or C frame:
            # the state is skipped.
            if not quiet[1] and kind==UC_MEM_WRITE and (BLOB<=address<self.blob_end or
                    STACK<=address<uc.reg_read(U.UC_X86_REG_ESP)-0x20):
                self.left=('garbage store',address);uc.emu_stop();return
            if BLOB<=address<self.blob_end:return
            # the trampoline's continuation word at ESP-4 is dead stack, and the stub
            # pushes, the cpu struct and the C frame lie below the guest ESP at the
            # last (re)entry minus the gap, where guest code never reaches (lifted
            # machine only)
            eip=uc.reg_read(U.UC_X86_REG_EIP)
            if quiet[0]<=eip<quiet[1] or BLOB<=eip<self.blob_end and STACK<=address<max(self.frame_top,uc.reg_read(U.UC_X86_REG_ESP)+16):return
            self.events.append((address,size,value&((1<<(8*size))-1) if kind==UC_MEM_WRITE else None))
        m.hook_add(UC_HOOK_MEM_READ,access);m.hook_add(UC_HOOK_MEM_WRITE,access)
        self.frame_top=0
        def enter(uc,address,size,data):self.frame_top=uc.reg_read(U.UC_X86_REG_ESP)+8  # E-GAP
        def enter_deep(uc,address,size,data):self.frame_top=uc.reg_read(U.UC_X86_REG_ESP)+12  # E-G
        if quiet[1]:
            m.hook_add(UC_HOOK_CODE,enter,begin=quiet[0],end=quiet[0])
            m.hook_add(UC_HOOK_CODE,enter_deep,begin=quiet[2],end=quiet[2])
        # Data reached through random pointers (floats or small integers used as
        # addresses, chains deeper than the arena) would fault and leave the
        # function unverified. Such pages are mapped on first touch with content
        # that depends only on the page and the state, so both machines see the
        # same memory; at most LAZY_PAGES per run, and never for code fetches.
        self.lazy=[];self.state_seed=0
        def unmapped(uc,kind,address,size,value,data):
            pages={address&~0xfff,(address+size-1)&~0xfff}
            if len(self.lazy)+len(pages)>LAZY_PAGES:return False
            for page in sorted(pages):
                try:uc.mem_map(page,0x1000,UC_PROT_READ|UC_PROT_WRITE)
                except UcError:continue
                self.lazy.append(page);uc.mem_write(page,lazy_page(self.state_seed,page))
            return True
        m.hook_add(UC_HOOK_MEM_READ_UNMAPPED|UC_HOOK_MEM_WRITE_UNMAPPED,unmapped)
        # A ret from image code to an address no call instruction returns to means
        # the run has left the function: the state is skipped. An indirect call of
        # the function (on the lifted machine: a call leaving the blob) into code
        # that is no function entry nor stored as a code pointer goes through a
        # garbage pointer; it is treated as an opaque callee, as the garbage could
        # read the return address, which differs between the two machines.
        self.canon={};self.home=None;self.left=False;self.prev=None;self.rets={};self.starts=set();self.text=(0,0);self.targets=set();self.callees=set()
        self.disasm=Cs(CS_ARCH_X86,CS_MODE_32);self.disasm.detail=True
        def block(uc,address,size,data):
            prev=self.prev;self.prev=(address,size)
            if prev is None or self.events is None:return
            if BLOB<=prev[0]<self.blob_end:kind='blob'
            else:
                if prev[0] not in self.rets:
                    last=None
                    for ins in self.disasm.disasm(bytes(uc.mem_read(*prev)),prev[0]):last=ins
                    self.rets[prev[0]]=last and (last.mnemonic if last.mnemonic!='call' or last.operands[0].type!=CS_OP_IMM else 'direct call')
                kind=self.rets[prev[0]]
            # a direct call of the function's own entry (recursion) is opaque on
            # the native machine as well, like the lifted body's call of it
            if (kind in ('blob','call') or kind=='direct call' and address==self.home) and                self.text[0]<=address<self.text[1] and (address not in self.callees or address==self.home):
                sp=uc.reg_read(U.UC_X86_REG_ESP);ret=struct.unpack('<I',bytes(uc.mem_read(sp,4)))[0]
                # every indirect call of the checked function is opaque, like its
                # direct callees are stubs: real code reached through a random
                # pointer may read its return address, which differs between the
                # machines (this includes the function's own entry, a pointer that
                # holds it until the game sets it at run time)
                if ret in self.sites:
                    self.events.append(('call',address,tuple(uc.reg_read(r) for r in GPRS),bytes(uc.mem_read(sp+4,20))))
                    self.returning(uc,opaque_result(address));self.prev=None;return
            if kind=='ret' and not (BLOB<=address<self.blob_end or address==STOP or after_call(uc,address)):
                self.left=(prev[0],address,kind);uc.emu_stop()
        m.hook_add(UC_HOOK_BLOCK,block)
        self.context=m.context_save()  # includes segment descriptor caches

    def returning(self,uc,result,pop=0):
        """A callee stub (or an opaque callee) was reached with [ESP] holding the
        address it returns to; it is emulated in registers only, so the native and
        the lifted machine (which return to different addresses) write no memory
        other than the dead word below the return address.
        For a call site of the checked function (`self.sites`: return address as
        this machine sees it -> (pop, halt)) the callee returns `result` in EAX
        and removes its return address, `pop` and the site's argument bytes; a
        call that does not return within the function ends the run there.
        Anything else is a jump out of the function (a tail call, a shared
        epilogue, a jump through a pointer): the run ends with the state the
        native code hands to that target."""
        if self.events is not None and len(self.events)>EVENT_LIMIT:self.flooded=True;uc.emu_stop();return  # a loop of calls: out of budget
        sp=uc.reg_read(U.UC_X86_REG_ESP);ret=struct.unpack('<I',bytes(uc.mem_read(sp,4)))[0]
        if ret not in self.sites:self.halted=True;uc.emu_stop();return
        args,halt=self.sites[ret]
        # the dead word under the return address: the trampoline leaves the callee
        # address there on the lifted machine, a real callee its own pushes
        uc.mem_write(sp-4,struct.pack('<I',uc.reg_read(U.UC_X86_REG_EIP)))
        # the popped return address stays behind as dead stack that the function
        # may later reuse as an uninitialized local; there the lifted machine's
        # resume stub stands for the native return address (as in as_native)
        if ret in self.canon:uc.mem_write(sp,struct.pack('<I',self.canon[ret]))
        if result is not None:uc.reg_write(U.UC_X86_REG_EAX,result)
        uc.reg_write(U.UC_X86_REG_ESP,sp+4+pop+args)
        if halt:self.halted=True;uc.emu_stop()
        else:uc.reg_write(U.UC_X86_REG_EIP,ret)

    def reset(self):
        m=self.uc
        for address,size in self.dirty:
            start=max(a for a in self.pristine if a<=address)
            old=self.pristine[start][address-start:address-start+size]
            if len(old)==size:m.mem_write(address,old);m.ctl_remove_cache(address,address+size)
        self.dirty=[]
        for page in self.lazy:m.mem_unmap(page,0x1000)
        self.lazy=[]
        m.context_restore(self.context)

    def run(self,state,entry,count,callees=(),sites=None):
        m=self.uc;arena,stack,regs,x87,flags=state
        self.reset();self.sites=sites or {};self.prev=None;self.rets={};self.callees={t for t,_ in callees};self.targets=self.starts|self.callees
        saved=[(t,bytes(m.mem_read(t,len(code)))) for t,code in callees]
        for t,code in callees:m.mem_write(t,code);m.ctl_remove_cache(t,t+len(code))  # drop stale translations
        stubs={t:(struct.unpack('<I',code[1:5])[0] if code[0]==0xb8 else None,
                  struct.unpack('<H',code[-2:])[0] if len(code)>=3 and code[-3]==0xc2 else 0) for t,code in callees}
        def called(uc,address,size,data):  # callee stub reached: arguments are an event
            if self.events is not None:
                sp=uc.reg_read(U.UC_X86_REG_ESP)
                self.events.append(('call',address,tuple(uc.reg_read(r) for r in GPRS),bytes(uc.mem_read(sp+4,20))))
                self.returning(uc,*stubs[address])
        hooks=[m.hook_add(UC_HOOK_CODE,called,begin=t,end=t) for t,_ in callees]
        try:return self._run(state,entry,count)
        finally:
            for h in hooks:m.hook_del(h)
            for t,old in saved:m.mem_write(t,old);m.ctl_remove_cache(t,t+len(old))

    def _run(self,state,entry,count):
        m=self.uc;arena,stack,regs,x87,flags=state
        m.mem_write(ARENA,arena);m.mem_write(STACK,b'\0'*0x10000);m.mem_write(ENTRY_SP,stack)
        self.state_seed=int.from_bytes(hashlib.sha256(arena[:256]).digest()[:8],'little')
        # the whole CODE page: STOP is a frame_state stack word, so stores through it land here
        m.mem_write(CODE,b'\x90'+bytes(0xfff));m.ctl_remove_cache(CODE,CODE+0x1000)
        m.reg_write(U.UC_X86_REG_FPCW,0x37f);m.reg_write(U.UC_X86_REG_FPSW,0);m.reg_write(U.UC_X86_REG_FPTAG,0xffff)
        m.reg_write(U.UC_X86_REG_MXCSR,0x1f80)
        for n in range(8):m.reg_write(getattr(U,f'UC_X86_REG_XMM{n}'),0)
        for reg,value in zip(GPRS,regs):m.reg_write(reg,value)
        m.reg_write(U.UC_X86_REG_ESP,ENTRY_SP);m.reg_write(U.UC_X86_REG_EFLAGS,flags)
        if x87:  # preload the x87 stack through a tiny loader so tags/TOP are real
            loader=b''.join(b'\xd9\x05'+struct.pack('<I',ARENA+ARENA_SIZE-0x100+4*k) for k in range(len(x87)))
            m.mem_write(ARENA+ARENA_SIZE-0x100,struct.pack(f'<{len(x87)}f',*x87))
            m.mem_write(LOADER,loader);m.emu_start(LOADER,LOADER+len(loader))
        self.events=[];self.halted=False;self.left=False;self.frame_top=0;self.flooded=False;began=time.monotonic()
        try:
            start=entry
            for opaque in range(OPAQUE_CALLS+1):
                # the instruction count is the budget; the wall-clock limit only
                # ends runs the count misses, and such a run is inconclusive
                try:m.emu_start(start,STOP,timeout=max(1,int((began+RUN_SECONDS-time.monotonic())*1_000_000)),count=count);break
                except UcError as e:
                    if e.errno not in (UC_ERR_FETCH_UNMAPPED,UC_ERR_FETCH_PROT) or opaque==OPAQUE_CALLS:raise
                    target=m.reg_read(U.UC_X86_REG_EIP);sp=m.reg_read(U.UC_X86_REG_ESP)
                    self.events.append(('call',target,tuple(m.reg_read(r) for r in GPRS),bytes(m.mem_read(sp+4,20))))
                    # the first 16 return K(target); later ones (a list walked through
                    # opaque calls would cycle) vary and are sometimes 0
                    self.returning(m,opaque_result(target) if opaque<16 else opaque_result(target,opaque))
                    if self.halted or self.flooded or time.monotonic()>=began+RUN_SECONDS:break
                    start=m.reg_read(U.UC_X86_REG_EIP)
            done=not self.left and (self.halted or m.reg_read(U.UC_X86_REG_EIP)==STOP);error='left the function' if self.left else None
        except UcError as e:done=False;error=str(e)
        events,self.events=self.events,None
        final_sp=m.reg_read(U.UC_X86_REG_ESP)
        # an ESP outside the stack (`mov esp, ebp` with a random EBP) is a lost run, and a
        # return to STOP below the entry frame popped a copy of STOP from deeper garbage
        if done and not (STACK<=final_sp<=STACK+0x10000 and (self.halted or final_sp>ENTRY_SP)):done=False;error='stack lost'
        if not done:
            error=error or ('timeout' if time.monotonic()-began>=RUN_SECONDS and not self.flooded else 'budget')
            # an unfinished run keeps its trace for the prefix comparison
            return dict(done=False,error=error,events=events)
        return dict(done=True,regs=[m.reg_read(r) for r in GPRS],fpu=[m.reg_read(r) for r in FPU],
            sse=[m.reg_read(r) for r in SSE],
            events=[e for e in events if e[0]=='call' or not (STACK<=e[0]<final_sp)])

def lazy_page(seed,page):
    """Deterministic page content: arena pointers, small integers and float bits."""
    rng=random.Random(seed^(page*0x9e3779b1))
    words=[]
    for _ in range(1024):
        k=rng.random()
        if k<0.12:words.append(FUNCS+8*rng.randrange(FUNC_COUNT))
        elif k<0.5:words.append(ARENA+0x100+rng.randrange(0,ARENA_SIZE-0x400,4))
        elif k<0.8:words.append(rng.choice([0,1,2,3,4,8,16,0xff,0xffffffff,rng.randrange(0,64)]))
        else:words.append(struct.unpack('<I',struct.pack('<f',rng.choice(FLOATS)))[0])
    return struct.pack('<1024I',*words)

def text_range(pe):
    text=next(s for s in pe.sections if s.Name.rstrip(b'\0')==b'.text');lo=pe.OPTIONAL_HEADER.ImageBase+text.VirtualAddress
    return lo,lo+text.Misc_VirtualSize

def call_targets(target,pe):
    """Indexed function entries and code pointers stored in data (vtables, tables)."""
    with (PROJECT.parent/'reports'/target/'all-functions.tsv').open(encoding='utf-8') as f:
        starts={int(row['address'],16) for row in csv.DictReader(f,delimiter='\t')}
    lo,hi=text_range(pe)
    for section in pe.sections:
        if not lo<=pe.OPTIONAL_HEADER.ImageBase+section.VirtualAddress<hi:
            data=section.get_data();starts|={v for (v,) in struct.iter_unpack('<I',data[:len(data)&~3]) if lo<=v<hi}
    return starts

def opaque_result(target,n=None):
    if n is None:return ARENA+0x100+((target*2654435761)&0x7ffc)
    h=(target*2654435761+n*0x85ebca6b)&0xffffffff
    return 0 if (h>>16)%4==0 else ARENA+0x100+(h&0x7ffc)

def frame_state(state):
    """Fallback for a state whose native run lost the stack or returned to an
    address no call returns to: code entered mid-function (an exception
    continuation, a shared epilogue) that uses the frame of the function it
    belongs to. EBP points into the entry stack and every stack word is STOP, so
    `mov esp, ebp; pop ebp; ret N` and `pop ...; ret` return to STOP."""
    arena,stack,regs,x87,flags=state
    regs=list(regs);regs[5]=ENTRY_SP+0x40
    return arena,struct.pack('<32I',*[STOP]*32),regs,x87,flags

def random_state(rng,k=0,small=False):
    # Every third state is pointer-heavy so `this` chains such as [[ecx+8]+4] resolve.
    pointers=0.85 if k%3==2 else 0.45
    def fresh():
        k=rng.random()
        if k<0.1:return FUNCS+8*rng.randrange(FUNC_COUNT)
        if k<pointers:return ARENA+0x100+rng.randrange(0,ARENA_SIZE-0x400,4)
        # extra draws after a skipped state: counts and sizes stay small so loops end
        if small:return rng.choice([0,1,2,3,4,5,8,16,rng.randrange(0,64)]) if rng.random()<0.9 else             struct.unpack('<I',struct.pack('<f',rng.choice(FLOATS)))[0]
        if k<pointers+0.35*(1-pointers)/0.55:return rng.choice([0,1,2,3,4,5,8,16,31,0x7f,0x80,0xff,0x7fffffff,0x80000000,0xffffffff,rng.randrange(0,64)])
        return struct.unpack('<I',struct.pack('<f',rng.choice(FLOATS)))[0]
    # Odd states draw from a small pool so equal operands, equal pointers and
    # boundary comparisons are common; even states use independent values.
    pool=[fresh() for _ in range(rng.choice([2,3,5]))]
    value=(lambda:rng.choice(pool) if rng.random()<0.85 else fresh()) if k%2 else fresh
    words=[value() for _ in range(ARENA_SIZE//4)]
    if small:  # empty circular lists: a head that points to itself
        for n in range(0,len(words),4):
            if rng.random()<0.1:words[n]=ARENA+4*n
    words[-64:]=[0]*64
    arena=struct.pack(f'<{len(words)}I',*words)
    stack=struct.pack('<17I',STOP,*[value() for _ in range(16)])
    regs=[value() for _ in GPRS]
    x87=[rng.choice(FLOATS) for _ in range(rng.choice([0,0,1,2]))]
    flags=rng.choice([0x202,0x246,0x287,0xa93])
    return arena,stack,regs,x87,flags

def stack_cleanup(pe,target,disasm,cache={}):
    """Bytes the callee at `target` pops on return (`ret N`), from its first return."""
    if target not in cache:
        cache[target]=0
        try:code=pe.get_data(target-0x400000,0x2000)
        except Exception:code=b''
        for i in disasm.disasm(code,target):
            if i.mnemonic=='ret':
                cache[target]=i.operands[0].imm if i.operands else 0;break
            if i.mnemonic=='int3':break
    return cache[target]

def callee_stubs(entry,pe,disasm):
    """Direct call/jump targets outside the function (the catalogued exits), each
    replaced on both machines by `mov eax, K; ret N` (K a deterministic arena
    pointer, N the stack bytes the real callee pops), or a bare `ret N` where
    stubs would overlap."""
    address=int(entry['address'],16)
    order=sorted(t for t in map(lambda x:int(x,16),entry['exits']) if 0x400000<=t<0x20000000 and not address<=t<address+entry['bytes']);stubs=[]
    for n,t in enumerate(order):
        pop=stack_cleanup(pe,t,disasm)
        ret=bytes([0xc2])+struct.pack('<H',pop) if pop else bytes([0xc3])
        room=(order[n+1]-t) if n+1<len(order) else 8
        k=ARENA+0x100+((t*2654435761)&0x7ffc)
        code=bytes([0xb8])+struct.pack('<I',k)+ret if room>=5+len(ret) and not (address-5-len(ret)<t<address) else ret
        stubs.append((t,code))
    return stubs

def after_call(uc,address):
    """Whether some call instruction (e8 rel32 or ff /2) ends at address."""
    try:b=bytes(uc.mem_read(address-7,7))
    except UcError:return False
    if b[2]==0xe8:return True
    for k in range(6):
        if b[k]!=0xff or (b[k+1]>>3)&7!=2:continue
        modrm=b[k+1];mod,rm=modrm>>6,modrm&7;n=2
        if mod!=3 and rm==4:
            if k+2>6:continue
            n+=1;base=b[k+2]&7
            if mod==0 and base==5:n+=4
        elif mod==0 and rm==5:n+=4
        n+={1:1,2:4}.get(mod,0)
        if k+n==7:return True
    return False

def call_sites(entry,pe,disasm):
    """{native return address: (pop, halt)} for the function's calls.

    halt: the code after the call is not part of the function (a call that does
    not return, such as a throw), so the run ends when the callee returns.
    pop: for an indirect call, the argument bytes the callee removes (`ret N`),
    estimated from the pushes (and `sub esp, imm`) in the straight-line code
    before the call; 0 when the caller cleans up with `add esp, imm`. Direct
    stubs already pop what their real callee pops."""
    insns=[]
    for r in entry['ranges']:
        lo,hi=(int(x,16) for x in r.split('-'))
        insns+=list(disasm.disasm(pe.get_data(lo-0x400000,hi-lo),lo))
    inside={i.address for i in insns}
    labels={o.imm for i in insns if i.mnemonic.startswith(('j','loop')) for o in i.operands if o.type==CS_OP_IMM}
    reg=lambda i,n:len(i.operands)>n and i.operands[n].type==CS_OP_REG and i.reg_name(i.operands[n].reg)
    sites={}
    for n,i in enumerate(insns):
        if i.mnemonic!='call':continue
        ret=i.address+i.size
        if ret not in inside:sites[ret]=(0,True);continue
        sites[ret]=(0,False)
        if i.operands[0].type==CS_OP_IMM:continue
        after=insns[n+1] if n+1<len(insns) and insns[n+1].address==ret else None
        if after and after.mnemonic=='add' and reg(after,0)=='esp':continue
        pop=0;j=n-1
        # a callee-saved register pushed here and popped after the call is a
        # register save, and arguments are pushed after saves
        restored={reg(q,0) for q in insns[n+1:] if q.mnemonic=='pop'}&{'ebx','esi','edi','ebp'}
        while j>=0 and insns[j].address+insns[j].size==insns[j+1].address and insns[j+1].address not in labels:
            p=insns[j];m=p.mnemonic
            if m=='push' and reg(p,0) in restored:break
            if m=='push':pop+=4
            elif m=='pop':pop-=4
            elif m=='sub' and reg(p,0)=='esp' and p.operands[1].type==CS_OP_IMM and p.operands[1].imm<=16:pop+=p.operands[1].imm  # float arguments; larger is a frame
            elif m=='call' or reg(p,0)=='esp':break
            j-=1
        if pop>0:sites[ret]=(pop,False)
    return sites

def verify(target,game_dir,states,only=None,limit=0,seed=1,lifted_dir=PROJECT/'lifted',part=None,reuse=None):
    spec=TARGETS[target];image=(game_dir/spec['file']).read_bytes()
    assert hashlib.sha256(image).hexdigest()==spec['sha']
    catalog=json.loads((lifted_dir/target/'catalog.json').read_text())
    assert catalog['original_sha256']==spec['sha']
    work=PROJECT/'build'/(f'{target}-lifted' if lifted_dir==PROJECT/'lifted' else f'{target}-lifted-{lifted_dir.name}')
    blob,symbols=link(target,work,lifted_dir)
    disasm=Cs(CS_ARCH_X86,CS_MODE_32);disasm.detail=True
    pe=pefile.PE(data=image);native=Machine(image,pe,blob);lifted=Machine(image,pe,blob,trampoline(symbols))
    native.starts=lifted.starts=call_targets(target,pe);native.text=lifted.text=text_range(pe)
    results=[];started=time.time()
    entries=[e for e in catalog['entries'] if not only or e['address'] in only]
    if limit:entries=entries[:limit]
    # Results of an earlier report stay valid for byte-identical function text,
    # the same runtime and the same states.
    reused=[]
    if reuse:
        old=json.loads(Path(reuse).read_text())
        if (old.get('runtime_sha256'),old['states_per_function'],old['seed'],old['original_sha256'])== \
           (runtime_sha256(lifted_dir),states,seed,spec['sha']):
            # a pass stays a pass under a newer verifier (it only makes more
            # states completable); everything else is checked again
            same=old.get('verifier')==VERIFIER
            known={(r['address'],r.get('c_sha256')):{'verifier':old.get('verifier'),**r} for r in old['results'] if same or r['status']=='passed'}
            keep=[]
            for e in entries:
                r=known.get((e['address'],e.get('c_sha256')))
                if r and e.get('c_sha256'):reused.append(r)
                else:keep.append(e)
            entries=keep
        print(f'  {target}: reusing {len(reused)} earlier results, checking {len(entries)}',flush=True)
    if part:
        entries=entries[part[0]::part[1]]
        if part[0]:reused=[]  # part 0 carries the reused results into the merge
    for index,entry in enumerate(entries):
        address=int(entry['address'],16);rng=random.Random(seed*1_000_003+address)
        assert pe.get_data(address-0x400000,entry['bytes'])==bytes.fromhex(entry['native_body'])
        completed=skipped=0;failure=None;callees=callee_stubs(entry,pe,disasm)
        # the lifted body returns into its resume stubs; native code of the same
        # function (recursion, the rest after a native jump) still returns natively
        sites=call_sites(entry,pe,disasm)
        returns={symbols[name]:int(ret,16) for name,ret in entry['resumes'].items()};lifted.canon=returns
        native.home=lifted.home=address
        lifted_sites={**sites,**{symbols[name]:sites[int(ret,16)] for name,ret in entry['resumes'].items() if int(ret,16) in sites}}
        # draw until `states` complete, at most three times as many; a function
        # none of whose states completed because the native run kept running out
        # of budget gets a second round with BIG_BUDGET
        out_of_budget=0
        for budget,lifted_budget in ((20_000,2_000_000),(BIG_BUDGET,20*BIG_BUDGET)):
            if budget!=20_000:
                if completed or failure or not out_of_budget:break
                rng=random.Random(seed*1_000_003+address);skipped=0
            for k in range(3*states):
                if completed>=states:break
                state=random_state(rng,k,small=k>=states)
                a=native.run(state,address,budget,callees,sites)
                if not a['done'] and a['error'] in ('stack lost','left the function','budget'):
                    state=frame_state(state);a=native.run(state,address,budget,callees,sites)
                if not a['done'] and a['error'] in ('budget','left the function','timeout'):  # lifted run not needed
                    skipped+=1;out_of_budget+=a['error']=='budget';continue
                b=as_native(lifted.run(state,symbols[entry['symbol']],lifted_budget,callees,lifted_sites),returns)
                if not b['done'] and b['error']=='timeout':skipped+=1;continue  # inconclusive
                if not a['done']:
                    skipped+=1
                    if b['done']:failure=dict(state=k,why='native faulted, lifted completed',native=a['error'])
                elif a!=b:failure=dict(state=k,why='mismatch',native=summary(a),lifted=summary(b))
                else:completed+=1
                if failure:break
        # A function no state of which completes even with BIG_BUDGET (a loop over
        # a random count or list) is compared on the trace prefix: the native run
        # stops after PREFIX_BUDGET instructions, and the lifted run must produce
        # the same reads, writes and calls in the same order up to that point.
        prefixed=0
        if not completed and not failure and out_of_budget:
            rng=random.Random(seed*1_000_003+address)
            for k in range(3*states):
                if prefixed>=states:break
                state=random_state(rng,k,small=k>=states)
                a=native.run(state,address,PREFIX_BUDGET,callees,sites)
                if not a['done'] and a['error'] in ('stack lost','left the function','budget'):
                    state=frame_state(state);a=native.run(state,address,PREFIX_BUDGET,callees,sites)
                if a['done'] or a['error']!='budget' or not a['events']:continue
                b=as_native(lifted.run(state,symbols[entry['symbol']],20*PREFIX_BUDGET,callees,lifted_sites),returns)
                n=len(a['events'])
                # a lifted run that completes (native might too, after the prefix)
                # or stops short of the native trace is inconclusive, unless it
                # faulted there while the native run went on
                if b['done'] or b['error']=='timeout':continue
                if len(b['events'])<n:
                    if b['error']=='budget' or a['events'][:len(b['events'])]!=b['events']:continue
                    failure=dict(state=k,why='lifted faulted inside the native trace prefix',lifted=b['error'],event=len(b['events']));break
                if a['events']!=b['events'][:n]:
                    i=next(i for i,(x,y) in enumerate(zip(a['events'],b['events'])) if x!=y)
                    failure=dict(state=k,why='trace prefix mismatch',native=summary(dict(done=True,regs=[],fpu=[],sse=[],events=a['events'][i:i+5])),
                                 lifted=summary(dict(done=True,regs=[],fpu=[],sse=[],events=b['events'][i:i+5])),event=i);break
                prefixed+=1
        status='failed' if failure else 'passed' if completed or prefixed else 'unverified'
        results.append(dict(address=entry['address'],c_sha256=entry.get('c_sha256'),status=status,completed=completed,skipped=skipped,**({'prefixed':prefixed} if prefixed else {}),
                            **({'failure':failure} if failure else {})))
        if (index+1)%250==0:
            print(f'  {target}: {index+1}/{len(entries)} checked, {time.time()-started:.0f}s',flush=True)
    counts={s:sum(r['status']==s for r in results) for s in ('passed','failed','unverified')}
    results=sorted(results+reused,key=lambda r:int(r['address'],16))
    counts={s:sum(r['status']==s for r in results) for s in ('passed','failed','unverified')}
    report=dict(target=target,lifted_dir=str(lifted_dir),original_sha256=spec['sha'],sources_sha256=sources_sha256(target,lifted_dir),
                runtime_sha256=runtime_sha256(lifted_dir),verifier=VERIFIER,states_per_function=states,seed=seed,counts=counts,results=results)
    (PROJECT/'build'/target).mkdir(parents=True,exist_ok=True)
    if not only and not limit and lifted_dir==PROJECT/'lifted':
        name=f'lifted-verification.part{part[0]}of{part[1]}.json' if part else 'lifted-verification.json'
        (PROJECT/'build'/target/name).write_text(json.dumps(report,indent=1))
    print(f'{target}: {counts}')
    return report

def as_native(r,returns):
    """The lifted run with its resume-stub addresses replaced by the native return
    addresses they stand for: a return address can be left in a register or in
    the stack words a call event records."""
    if 'events' not in r or not returns:return r
    fix=lambda v:returns.get(v,v)
    def words(b):
        b=bytearray(b)
        for o in range(0,len(b)-3,4):b[o:o+4]=struct.pack('<I',fix(struct.unpack('<I',b[o:o+4])[0]))
        return bytes(b)
    events=[(e[0],e[1],tuple(map(fix,e[2])),words(e[3])) if e[0]=='call' else
            (e[0],e[1],fix(e[2]) if e[1]==4 and e[2] is not None else e[2]) for e in r['events']]
    return {**r,**({'regs':[fix(v) for v in r['regs']]} if r['done'] else {}),'events':events}

def summary(r):
    if not r['done']:return r
    return dict(regs=[f'{v:08x}' for v in r['regs']],fpu=[str(v) for v in r['fpu']],sse=[f'{v:x}' for v in r['sse']],
                events=[(e[0],f'{e[1]:08x}',[f'{v:08x}' for v in e[2]],e[3].hex()) if e[0]=='call' else
                        (f'{e[0]:08x}',e[1],None if e[2] is None else f'{e[2]:x}') for e in r['events'][:40]])

if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both')
    p.add_argument('--states',type=int,default=12);p.add_argument('--limit',type=int,default=0)
    p.add_argument('--only',nargs='*');p.add_argument('--seed',type=int,default=1)
    p.add_argument('--show-failures',type=int,default=5)
    p.add_argument('--lifted-dir',type=Path,default=PROJECT/'lifted',help='lifter output to check (default: lifted/)')
    p.add_argument('--part',help='K/N: check every N-th function starting at K (run N parts in parallel, then --merge)')
    p.add_argument('--reuse',action='store_true',help='keep results from build/<target>/lifted-verification.json for unchanged functions')
    p.add_argument('--merge',type=int,metavar='N',help='combine N part reports into lifted-verification.json')
    a=p.parse_args()
    if a.merge:
        for t in ['client','server'] if a.target=='both' else [a.target]:
            parts=[json.loads((PROJECT/'build'/t/f'lifted-verification.part{k}of{a.merge}.json').read_text()) for k in range(a.merge)]
            assert len({(r['lifted_dir'],r['original_sha256'],r['sources_sha256'],r.get('runtime_sha256'),r['states_per_function'],r['seed']) for r in parts})==1
            results=sorted((r for q in parts for r in q['results']),key=lambda r:int(r['address'],16))
            counts={s:sum(r['status']==s for r in results) for s in ('passed','failed','unverified')}
            report={**parts[0],'counts':counts,'results':results}
            (PROJECT/'build'/t/'lifted-verification.json').write_text(json.dumps(report,indent=1))
            print(f'{t}: merged {len(results)} results {counts}')
        raise SystemExit
    part=tuple(map(int,a.part.split('/'))) if a.part else None
    for t in ['client','server'] if a.target=='both' else [a.target]:
        old=PROJECT/'build'/t/'lifted-verification.json'
        report=verify(t,a.game_dir,a.states,set(a.only or []),a.limit,a.seed,a.lifted_dir.resolve(),part,
                      old if a.reuse and old.exists() else None)
        for r in [r for r in report['results'] if r['status']=='failed'][:a.show_failures]:
            print(json.dumps(r)[:3000])
