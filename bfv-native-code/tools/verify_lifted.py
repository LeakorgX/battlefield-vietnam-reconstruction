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
native budget of BIG_BUDGET instructions. Only the code sections are executable
(.text and the patch code in section slack and other sections: the .tls slack,
.ctfsnd, the client's patch sections and entry stub), and an
indirect call made by the function into code is opaque on both machines too
(real code reached through a random pointer may read its return address). The
frame below the entry stub (the default gap, or the body's own gap for
_bfv_l1_enter_deep) is the
lifted code's own and is excluded from the comparison. Runs are bounded by an
instruction count, EVENT_LIMIT events and OPAQUE_CALLS opaque calls (a loop
whose exit depends on opaque results is out of budget); a run stopped by the RUN_SECONDS
wall-clock limit instead is inconclusive and skipped.
Every call is an event: target, all general registers and 20 bytes of
stack arguments. Pages touched outside the mapped memory are mapped on demand
with contents that depend only on the page and the state (at most LAZY_PAGES).
Before every run the image, the stubs and the blob are restored and stale
translations dropped, so each state of each function starts from the same
machine.

A function with no completed state is compared on trace prefixes: the native
run stopped at PREFIX_BUDGET instructions must be a prefix of the lifted run of
the same state. Direct self-calls are opaque on both machines.

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
from unicorn import UC_HOOK_MEM_READ_UNMAPPED,UC_HOOK_MEM_WRITE_UNMAPPED,UC_PROT_READ,UC_PROT_WRITE,UC_PROT_ALL
from unicorn import UC_ERR_FETCH_UNMAPPED,UC_ERR_FETCH_PROT
from unicorn import x86_const as U
from build import PROJECT,GAME,TARGETS,COMPILERS,run,response,rebase,changed_runs,overlaps
from native_oracle import ARENA,STACK

BLOB=0x20000000
FLAGS=['-m32','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-array-bounds','-ffreestanding','-fno-builtin',
       '-frounding-math','-fno-delete-null-pointer-checks','-fno-stack-protector','-fno-asynchronous-unwind-tables','-fno-unwind-tables','-mgeneral-regs-only']
GPRS=[U.UC_X86_REG_EAX,U.UC_X86_REG_ECX,U.UC_X86_REG_EDX,U.UC_X86_REG_EBX,U.UC_X86_REG_ESP,U.UC_X86_REG_EBP,
      U.UC_X86_REG_ESI,U.UC_X86_REG_EDI]
FPU=[U.UC_X86_REG_FPCW,U.UC_X86_REG_FPSW,U.UC_X86_REG_FPTAG,*[getattr(U,f'UC_X86_REG_FP{n}') for n in range(8)]]
SSE=[U.UC_X86_REG_MXCSR,*[getattr(U,f'UC_X86_REG_XMM{n}') for n in range(8)]]
VERIFIER=24  # bump when the states or comparison change; old results are then not reused
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
ESP_BIT=16;POISON=0x5a5a5a5b
CODE=0x1ff10000;STOP=CODE;LOADER=CODE+0x100;OPAQUE_CALLS=64
ENTRY_SP=STACK+0x8000
# Thread data: fs points at a TEB page (SEH chain end, stack base and limit),
# loaded through a GDT, on both machines.
TEB=0x1fd00000;GDT=TEB+0x1000;FS_SELECTOR=3*8|3;SS_SELECTOR=4*8  # SS: flat 32-bit, so pushes use ESP
# The readable runtime's shadow stacks (l1_runtime.c): only the top SHADOW_USED
# bytes of slot 0 (the one thread) and the slot table are mapped.
SHADOW_USED=0x40000
FLOATS=[0.0,1.0,-1.0,0.5,2.0,-3.25,100.0,1e-3,0.75,-0.125]

def link(target,work,lifted_dir):
    """Compile and link the lifted code at BLOB; returns (bytes, {symbol: address})."""
    work.mkdir(parents=True,exist_ok=True)
    sources=[lifted_dir/'runtime/l1_runtime.c',*sorted((lifted_dir/target).rglob('*.c'))]
    inputs=[*sources,*runtime_headers(lifted_dir)]
    stamp=work/'lifted.flags'
    if (work/'lifted.bin').exists() and (work/'lifted.map').exists() and stamp.exists() and \
       stamp.read_text()==' '.join(FLAGS) and (work/'lifted.bin').stat().st_mtime>max(f.stat().st_mtime for f in inputs):
        return (work/'lifted.bin').read_bytes(),map_symbols(work)  # parts share one up-to-date link
    for old in work.glob('*.o'):old.unlink()
    stamp.unlink(missing_ok=True)
    objects=[work/('_'.join(source.relative_to(lifted_dir).with_suffix('.o').parts)) for source in sources]
    with ThreadPoolExecutor(6) as pool:
        list(pool.map(lambda so:run([COMPILERS/'gcc.exe',*FLAGS,'-I',lifted_dir/'runtime','-c',so[0],'-o',so[1]],work),zip(sources,objects)))
    (work/'lifted.ld').write_text(f'SECTIONS {{ . = 0x{BLOB:x}; .payload : {{ *(.text*) *(.rdata*) *(.data*) *(.bss*) BYTE(0) }}'
                                  ' .bfvshadow ALIGN(4096) (NOLOAD) : { *(.bfvshadow) }'
                                  ' /DISCARD/ : { *(.eh_frame*) *(.comment*) *(.drectve*) } }')
    run([COMPILERS/'ld.exe','-T',work/'lifted.ld','--image-base','0x400000','--entry','_bfv_l1_enter',
         '--disable-dynamicbase','--disable-reloc-section','-Map',work/'lifted.map','-o',work/'lifted.exe',response(work/'objects.rsp',objects)],work)
    run([COMPILERS/'objcopy.exe','-O','binary','--only-section','.payload',work/'lifted.exe',work/'lifted.bin'],work)
    stamp.write_text(' '.join(FLAGS))
    return (work/'lifted.bin').read_bytes(),map_symbols(work)

def runtime_headers(lifted_dir):
    """l1.h, and l2.h and the address names for the readable tree."""
    return [h for h in (lifted_dir/'runtime/l1.h',lifted_dir/'runtime/l2.h',lifted_dir/'runtime/symbols_client.h',
                        lifted_dir/'runtime/symbols_server.h') if h.name=='l1.h' or h.exists()]

def trampoline(symbols):
    """[start, end) of the shared trampoline code in the blob (the default and
    the deep entry, which follows it, and in the readable runtime bfv_call and
    the return path up to bfv_l1_runtime_end), and the deep entry."""
    start=symbols['bfv_l1_enter'];deep=symbols['bfv_l1_enter_deep']
    if 'bfv_l1_runtime_end' in symbols:return start,symbols['bfv_l1_runtime_end'],deep
    return start,min(a for a in symbols.values() if a>max(start,deep)),deep

def shadow_range(symbols):
    """[lo, hi) of the mapped shadow stack and slot table, or None (L1 runtime)."""
    if 'bfv_threads' not in symbols:return None
    return symbols['bfv_threads']-SHADOW_USED,symbols['bfv_shadow_end']

def teb_page():
    """TEB contents: no SEH frame, the stack's base and limit, the TEB itself."""
    page=bytearray(0x1000)
    struct.pack_into('<III',page,0,0xffffffff,STACK+0x10000,STACK);struct.pack_into('<I',page,0x18,TEB)
    return bytes(page)

def runtime_sha256(lifted_dir):
    """Hash of the shared runtime; with a function's own c_sha256 it fixes the compiled code."""
    h=hashlib.sha256()
    for f in [*runtime_headers(lifted_dir),lifted_dir/'runtime/l1_runtime.c']:h.update(f.name.encode()+bytes(1)+f.read_bytes())
    return h.hexdigest()

def sources_sha256(target,lifted_dir):
    """Hash of the lifted sources; build.py only detours functions verified from these exact sources."""
    h=hashlib.sha256()
    for f in [*runtime_headers(lifted_dir),lifted_dir/'runtime/l1_runtime.c',*sorted((lifted_dir/target).rglob('*.c'))]:
        h.update(f.relative_to(lifted_dir).as_posix().encode()+b'\0'+f.read_bytes())
    return h.hexdigest()

def map_symbols(work):
    symbols={}
    for line in (work/'lifted.map').read_text().splitlines():
        m=re.match(r'\s*(0x[0-9a-fA-F]+)\s+_?(bfv_[A-Za-z0-9_]+)\s*$',line)
        if m:symbols[m[2]]=int(m[1],16)
    return symbols

class Machine:
    def __init__(self,image,pe,blob,quiet=(0,0),shadow=None):
        m=self.uc=Uc(UC_ARCH_X86,UC_MODE_32);base=pe.OPTIONAL_HEADER.ImageBase
        # A Phenom has 3DNow! (the d3dx and 0x0081/0x0085 vector code); CR4.OSFXSR enables SSE on it.
        m.ctl_set_cpu_model(U.UC_CPU_X86_PHENOM);m.reg_write(U.UC_X86_REG_CR4,0x200)
        m.mem_map(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095)
        m.mem_write(base,image[:pe.OPTIONAL_HEADER.SizeOfHeaders])
        for s in pe.sections:m.mem_write(base+s.VirtualAddress,s.get_data())
        # The file's import address table holds name RVAs, not entry points; each
        # slot gets its own unmapped address so an import call is an opaque callee.
        if not hasattr(pe,'DIRECTORY_ENTRY_IMPORT'):pe.parse_data_directories()
        for n,imp in enumerate(i for d in getattr(pe,'DIRECTORY_ENTRY_IMPORT',[]) for i in d.imports):
            m.mem_write(imp.address,struct.pack('<I',IMPORTS+16*n))
        # only code is executable: a call through a garbage pointer into data is opaque
        m.mem_protect(base,(pe.OPTIONAL_HEADER.SizeOfImage+4095)&~4095,UC_PROT_READ|UC_PROT_WRITE)
        for lo,hi in code_ranges(pe):m.mem_protect(lo&~0xfff,((hi+0xfff)&~0xfff)-(lo&~0xfff),UC_PROT_ALL)
        m.mem_map(ARENA,ARENA_SIZE,UC_PROT_READ|UC_PROT_WRITE);m.mem_map(STACK,0x10000,UC_PROT_READ|UC_PROT_WRITE)
        m.mem_map(CODE,0x1000)
        # fs: a data segment over the TEB page
        m.mem_map(TEB,0x2000,UC_PROT_READ|UC_PROT_WRITE);m.mem_write(TEB,teb_page())
        m.mem_write(GDT+8*(FS_SELECTOR>>3),struct.pack('<HHBBBB',0xfff,TEB&0xffff,(TEB>>16)&0xff,0xf3,0x40,TEB>>24))
        m.mem_write(GDT+SS_SELECTOR,struct.pack('<HHBBBB',0xffff,0,0,0x93,0xcf,0))
        m.reg_write(U.UC_X86_REG_GDTR,(0,GDT,0x1000,0));m.reg_write(U.UC_X86_REG_SS,SS_SELECTOR);m.reg_write(U.UC_X86_REG_FS,FS_SELECTOR)
        self.shadow=shadow or (0,0)
        if shadow:m.mem_map(shadow[0],shadow[1]-shadow[0],UC_PROT_READ|UC_PROT_WRITE)
        m.mem_map(BLOB,(len(blob)+0xffff)&~0xffff);m.mem_write(BLOB,blob)
        self.blob_end=BLOB+len(blob);self.events=None
        m.mem_write(STOP,b'\x90')
        m.mem_map(FUNCS,0x1000)
        m.mem_write(FUNCS,b''.join(bytes([0xb8])+struct.pack('<I',ARENA+0x100+n*0x1f4)+bytes([0xc3,0xcc,0xcc])
                                   for n in range(FUNC_COUNT)))
        self.sites={};self.abi_calls={};self.ret_mask=None;self.ret_calls=set();self.ncalls=0;self.quiet=bool(quiet[1]);self.last_from=None
        def indirect(uc,address,size,data):
            if self.events is not None and (address-FUNCS)%8==0:
                sp=uc.reg_read(U.UC_X86_REG_ESP)
                self.call_event(uc,address,sp)
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
            if not quiet[1] and kind==UC_MEM_WRITE and (BLOB<=address<self.blob_end or self.shadow[0]<=address<self.shadow[1] or
                    STACK<=address<uc.reg_read(U.UC_X86_REG_ESP)-0x20):
                self.left=('garbage store',address);uc.emu_stop();return
            # the blob and the shadow stack (the lifted body's C frame) are the lifted code's own
            if BLOB<=address<self.blob_end or self.shadow[0]<=address<self.shadow[1]:return
            # the trampoline's continuation word at ESP-4 is dead stack, and the stub
            # pushes, the cpu struct and the C frame lie below the guest ESP at the
            # last (re)entry minus the gap, where guest code never reaches (lifted
            # machine only)
            eip=uc.reg_read(U.UC_X86_REG_EIP);sp=uc.reg_read(U.UC_X86_REG_ESP)
            if quiet[0]<=eip<quiet[1] or BLOB<=eip<self.blob_end and STACK<=address<max(self.frame_top,sp+16 if STACK<=sp<STACK+0x10000 else 0):return
            event=(address,size,value&((1<<(8*size))-1) if kind==UC_MEM_WRITE else None)
            # unicorn's pushad stores EDI first, at the lowest address; the CPU's
            # order (and the lifted body's) is EAX first, at the highest
            if kind==UC_MEM_WRITE and base<=eip<self.image_end and uc.mem_read(eip,1)==b'\x60':
                if not(self.pushad and self.pushad[0]==eip and self.pushad[2]<8 and self.pushad[1]+self.pushad[2]==len(self.events)):
                    self.pushad=(eip,len(self.events),0)
                _,at,n=self.pushad;self.events.insert(at,event);self.pushad=(eip,at,n+1);return
            self.events.append(event)
        m.hook_add(UC_HOOK_MEM_READ,access);m.hook_add(UC_HOOK_MEM_WRITE,access)
        self.frame_top=0
        def enter(uc,address,size,data):self.frame_top=uc.reg_read(U.UC_X86_REG_ESP)+8  # E-GAP
        def enter_deep(uc,address,size,data):self.frame_top=uc.reg_read(U.UC_X86_REG_ESP)+12  # E-G
        if quiet[1] and not self.shadow[1]:  # the readable runtime keeps *cpu on the shadow stack: no guest frame to leave out
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
        self.canon={};self.home=None;self.left=False;self.prev=None;self.rets={};self.starts=set();self.code=[];self.targets=set();self.callees=set()
        self.disasm=Cs(CS_ARCH_X86,CS_MODE_32);self.disasm.detail=True;counter=Cs(CS_ARCH_X86,CS_MODE_32)
        self.ninsn={};self.budget=0;self.executed=0
        def block(uc,address,size,data):
            # the instruction budget: emu_start's count would add a code hook on every
            # instruction, and with one unicorn loses the lazy flags of an instruction
            # before a rep string instruction whose first access faults into a lazy page
            if self.budget:
                n=self.ninsn.get((address,size))
                if n is None:n=self.ninsn[(address,size)]=max(1,sum(1 for _ in counter.disasm_lite(bytes(uc.mem_read(address,size)),address)))
                self.executed+=n
                if self.executed>self.budget:uc.emu_stop();return
            prev=self.prev;self.prev=(address,size);self.last_from=prev
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
            if (kind in ('blob','call') or kind=='direct call' and address==self.home) and                any(lo<=address<hi for lo,hi in self.code) and (address not in self.callees or address==self.home):
                sp=uc.reg_read(U.UC_X86_REG_ESP);ret=struct.unpack('<I',bytes(uc.mem_read(sp,4)))[0]
                # every indirect call of the checked function is opaque, like its
                # direct callees are stubs: real code reached through a random
                # pointer may read its return address, which differs between the
                # machines (this includes the function's own entry, a pointer that
                # holds it until the game sets it at run time)
                if ret in self.sites:
                    self.call_event(uc,address,sp)
                    self.returning(uc,opaque_result(address));self.prev=None;return
            if kind=='ret' and not (BLOB<=address<self.blob_end or address==STOP or after_call(uc,address)):
                self.left=(prev[0],address,kind);uc.emu_stop()
        m.hook_add(UC_HOOK_BLOCK,block)
        self.context=m.context_save()  # includes segment descriptor caches

    def call_event(self,uc,address,sp):
        """A call: its target, registers and the first stack words. Registers the
        callee can't see (L2b register variables, the call's ABI in the catalog)
        are left out."""
        regs=tuple(uc.reg_read(r) for r in GPRS)
        m=self.abi_calls.get(struct.unpack('<I',bytes(uc.mem_read(sp,4)))[0])
        # native `ret` to a random address: the event is masked like the final
        # registers, on both machines (verify() applies ret_calls to both)
        if m is None and self.ret_mask is not None and not self.quiet:
            src=self.last_from if self.prev and self.prev[0]==address else self.prev
            last=None
            if src:
                for ins in self.disasm.disasm(bytes(uc.mem_read(*src)),src[0]):last=ins
            if last is not None and last.mnemonic=='ret':self.ret_calls.add(self.ncalls)
        self.ncalls+=1
        if m is not None:regs=tuple(v if (m|ESP_BIT)>>k&1 else None for k,v in enumerate(regs))
        self.events.append(('call',address,regs,bytes(uc.mem_read(sp+4,20))))

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
        # ecx/edx the call didn't hand over: a real callee leaves something there
        m=self.abi_calls.get(ret)
        if m is not None:
            for bit,reg in ((2,U.UC_X86_REG_ECX),(4,U.UC_X86_REG_EDX)):
                if not m&bit:uc.reg_write(reg,POISON)
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
        self.reset();self.sites=sites or {};self.prev=None;self.rets={};self.ninsn={};self.callees={t for t,_ in callees};self.targets=self.starts|self.callees
        saved=[(t,bytes(m.mem_read(t,len(code)))) for t,code in callees]
        for t,code in callees:m.mem_write(t,code);m.ctl_remove_cache(t,t+len(code))  # drop stale translations
        stubs={t:(struct.unpack('<I',code[1:5])[0] if code[0]==0xb8 else None,
                  struct.unpack('<H',code[-2:])[0] if len(code)>=3 and code[-3]==0xc2 else 0) for t,code in callees}
        def called(uc,address,size,data):  # callee stub reached: arguments are an event
            if self.events is not None:
                sp=uc.reg_read(U.UC_X86_REG_ESP)
                self.call_event(uc,address,sp)
                self.returning(uc,*stubs[address])
        hooks=[m.hook_add(UC_HOOK_CODE,called,begin=t,end=t) for t,_ in callees]
        try:return self._run(state,entry,count)
        finally:
            for h in hooks:m.hook_del(h)
            for t,old in saved:m.mem_write(t,old);m.ctl_remove_cache(t,t+len(old))

    def _run(self,state,entry,count):
        m=self.uc;arena,stack,regs,x87,flags=state;self.budget=0
        m.mem_write(ARENA,arena);m.mem_write(STACK,b'\0'*0x10000);m.mem_write(ENTRY_SP,stack);m.mem_write(TEB,teb_page())
        if self.shadow[1]:m.mem_write(self.shadow[0],bytes(self.shadow[1]-self.shadow[0]))
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
        self.events=[];self.pushad=None;self.halted=False;self.ret_calls=set();self.ncalls=0;self.left=False;self.frame_top=0;self.flooded=False;began=time.monotonic()
        try:
            start=entry
            for opaque in range(OPAQUE_CALLS+1):
                # the instruction count is the budget; the wall-clock limit only
                # ends runs the count misses, and such a run is inconclusive
                self.executed=0;self.budget=count  # per start, as emu_start's count
                try:m.emu_start(start,STOP,timeout=max(1,int((began+RUN_SECONDS-time.monotonic())*1_000_000)));break
                except UcError as e:
                    if e.errno not in (UC_ERR_FETCH_UNMAPPED,UC_ERR_FETCH_PROT):raise
                    if opaque==OPAQUE_CALLS:break  # out of opaque calls: out of budget
                    target=m.reg_read(U.UC_X86_REG_EIP);sp=m.reg_read(U.UC_X86_REG_ESP)
                    self.call_event(m,target,sp)
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
        regs=[m.reg_read(r) for r in GPRS]
        # returned with `ret`: only what the callers read (L2b register variables)
        if self.ret_mask is not None and not self.halted:regs=[v if (self.ret_mask|ESP_BIT)>>k&1 else None for k,v in enumerate(regs)]
        return dict(done=True,regs=regs,fpu=[m.reg_read(r) for r in FPU],
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

def code_ranges(pe):
    """Executable sections, .text first. The 1.21 patch put code in the raw
    slack of the 9-byte .tls section, which the loader maps with its page; the
    newer client build has its entry stub in the raw slack after .text."""
    return [(pe.OPTIONAL_HEADER.ImageBase+s.VirtualAddress,pe.OPTIONAL_HEADER.ImageBase+s.VirtualAddress+
             max(s.Misc_VirtualSize,min(s.SizeOfRawData,(s.Misc_VirtualSize+0xfff)&~0xfff)))
            for s in pe.sections if s.Characteristics&0x20000000]

def call_targets(target,pe):
    """Indexed function entries and code pointers stored in data (vtables, tables)."""
    with (PROJECT.parent/'reports'/target/'all-functions.tsv').open(encoding='utf-8') as f:
        starts={int(row['address'],16) for row in csv.DictReader(f,delimiter='\t')}
    code=code_ranges(pe)
    for section in pe.sections:
        if not section.Characteristics&0x20000000:
            data=section.get_data();starts|={v for (v,) in struct.iter_unpack('<I',data[:len(data)&~3]) if any(lo<=v<hi for lo,hi in code)}
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

def cookie_check(pe,t,disasm):
    """__security_check_cookie (`cmp ecx, [cookie]; jne fail; ret`) returns
    with every register unchanged; its stub is a bare `ret`, so a function whose
    result is in eax across it is checked for keeping it there."""
    try:i=list(disasm.disasm(pe.get_data(t-0x400000,16),t,3))
    except Exception:return False
    return len(i)==3 and i[0].mnemonic=='cmp' and i[0].op_str.startswith('ecx, dword ptr [0x') and i[1].mnemonic=='jne' and i[2].mnemonic=='ret' and not i[2].op_str

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
        code=bytes([0xb8])+struct.pack('<I',k)+ret if room>=5+len(ret) and not (address-5-len(ret)<t<address) and not cookie_check(pe,t,disasm) else ret
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

def report_name(lifted_dir):
    """Report file stem: lifted-verification for lifted/, <dir>-verification otherwise (readable/)."""
    return 'lifted' if Path(lifted_dir).resolve()==(PROJECT/'lifted').resolve() else Path(lifted_dir).name

VCORE=PROJECT/'build'/'vcore'/'vcore.exe'

def verify_cpp(target,work,entries,pe,disasm,native,symbols,states,seed,threads):
    """Run the emulation in tools/vcore (C++, same machines, states and
    comparison as below): this side writes each function's stubs, sites and
    masks to a job file and reads back one result line per function."""
    assert VCORE.exists(),f'build {VCORE} first (tools/vcore/build.ps1)'
    base=pe.OPTIONAL_HEADER.ImageBase
    (work/'image.mem').write_bytes(bytes(native.uc.mem_read(base,native.image_end-base)))
    start,end,deep=trampoline(symbols)
    hx=lambda v:f'{v:x}'
    lines=[f'image {base:x} {pe.OPTIONAL_HEADER.SizeOfImage:x} {work/"image.mem"}',f'blob {work/"lifted.bin"}',
           *(f'code {lo:x} {hi:x}' for lo,hi in code_ranges(pe)),f'quiet {start:x} {end:x} {deep:x}',f'states {states} {seed}']
    if shadow_range(symbols):lines.append('shadow {:x} {:x}'.format(*shadow_range(symbols)))
    for entry in entries:
        address=int(entry['address'],16)
        assert pe.get_data(address-0x400000,entry['bytes'])==bytes.fromhex(entry['native_body'])
        sites=call_sites(entry,pe,disasm);abi=entry.get('abi') or {}
        calls={int(r,16):m for r,m in abi.get('calls',{}).items()}
        lines.append(f'E {entry["address"]} {hx(symbols[entry["symbol"]])} {hx(address)}')
        lines+=[f'C {hx(t)} {code.hex()}' for t,code in callee_stubs(entry,pe,disasm)]
        lines+=[f'S {hx(r)} {p} {int(h)}' for r,(p,h) in sites.items()]
        lifted_sites={**sites,**{symbols[n]:sites[int(r,16)] for n,r in entry['resumes'].items() if int(r,16) in sites}}
        lines+=[f'L {hx(r)} {p} {int(h)}' for r,(p,h) in lifted_sites.items()]
        lines+=[f'R {hx(symbols[n])} {r}' for n,r in entry['resumes'].items()]
        lines+=[f'A {hx(r)} {m}' for r,m in calls.items()]
        lines+=[f'B {hx(r)} {m}' for r,m in calls.items()]
        lines+=[f'B {hx(symbols[n])} {calls[int(r,16)]}' for n,r in entry['resumes'].items() if int(r,16) in calls]
        if abi.get('ret') is not None:lines.append(f'M {abi["ret"]}')
    (work/'vcore.job').write_text('\n'.join(lines)+'\n')
    run([VCORE,work/'vcore.job',work/'vcore.out',str(threads)],work)
    by={e['address']:e for e in entries};results=[]
    for line in (work/'vcore.out').read_text().splitlines():
        address,status,completed,skipped,prefixed,failure=line.split(' ',5)
        results.append(dict(address=address,c_sha256=by[address].get('c_sha256'),status=status,completed=int(completed),skipped=int(skipped),
                            **({'prefixed':int(prefixed)} if int(prefixed) else {}),**({'failure':json.loads(failure)} if failure!='-' else {})))
    return results

def verify(target,game_dir,states,only=None,limit=0,seed=1,lifted_dir=PROJECT/'lifted',part=None,reuse=None,engine='py',threads=8):
    spec=TARGETS[target];image=(game_dir/spec['file']).read_bytes()
    assert hashlib.sha256(image).hexdigest()==spec['sha']
    catalog=json.loads((lifted_dir/target/'catalog.json').read_text())
    assert catalog['original_sha256']==spec['sha']
    work=PROJECT/'build'/(f'{target}-lifted' if lifted_dir==PROJECT/'lifted' else f'{target}-lifted-{lifted_dir.name}')
    blob,symbols=link(target,work,lifted_dir)
    disasm=Cs(CS_ARCH_X86,CS_MODE_32);disasm.detail=True
    pe=pefile.PE(data=image);shadow=shadow_range(symbols)
    native=Machine(image,pe,blob,shadow=shadow);lifted=Machine(image,pe,blob,trampoline(symbols),shadow)
    native.starts=lifted.starts=call_targets(target,pe);native.code=lifted.code=code_ranges(pe)
    results=[];started=time.time()
    entries=[e for e in catalog['entries'] if not only or e['address'] in only]
    # Functions the executable's own patches changed since the reference image
    # (rebase.json) were lifted from code this image no longer has. They are
    # reported as native, not run, and build.py keeps them native.
    record=rebase(target);touches=overlaps(changed_runs(record))
    changed={e['address'] for e in entries if any(touches(*(int(x,16) for x in r.split('-'))) for r in e['ranges'])}
    if record and not only and sorted(changed)!=record['native_lifted'].get(report_name(lifted_dir)):
        raise RuntimeError(f'{target}: rebase.json native_lifted differs from the catalog; rerun tools/rebase.py')
    native_results=[dict(address=e['address'],c_sha256=e.get('c_sha256'),status='native') for e in entries if e['address'] in changed]
    entries=[e for e in entries if e['address'] not in changed]
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
        if part[0]:reused=native_results=[]  # part 0 carries the reused and native results into the merge
    if engine=='cpp':
        results=verify_cpp(target,work,entries,pe,disasm,native,symbols,states,seed,threads)
        entries=[]
    for index,entry in enumerate(entries):
        address=int(entry['address'],16);rng=random.Random(seed*1_000_003+address)
        assert pe.get_data(address-0x400000,entry['bytes'])==bytes.fromhex(entry['native_body'])
        completed=skipped=0;failure=None;callees=callee_stubs(entry,pe,disasm)
        # the lifted body returns into its resume stubs; native code of the same
        # function (recursion, the rest after a native jump) still returns natively
        sites=call_sites(entry,pe,disasm)
        returns={symbols[name]:int(ret,16) for name,ret in entry['resumes'].items()};lifted.canon=returns
        native.home=lifted.home=address
        abi=entry.get('abi') or {}
        native.abi_calls={int(r,16):m for r,m in abi.get('calls',{}).items()}
        lifted.abi_calls={**native.abi_calls,**{symbols[name]:native.abi_calls[int(r,16)] for name,r in entry['resumes'].items() if int(r,16) in native.abi_calls}}
        native.ret_mask=lifted.ret_mask=abi.get('ret')
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
                a,b=ret_view(a,native),ret_view(b,native)
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
                b=as_native(lifted.run(state,symbols[entry['symbol']],100*PREFIX_BUDGET,callees,lifted_sites),returns)
                a,b=ret_view(a,native),ret_view(b,native)
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
    results=sorted(results+reused+native_results,key=lambda r:int(r['address'],16))
    counts={s:sum(r['status']==s for r in results) for s in ('passed','failed','unverified','native')}
    report=dict(target=target,lifted_dir=str(lifted_dir),original_sha256=spec['sha'],sources_sha256=sources_sha256(target,lifted_dir),
                runtime_sha256=runtime_sha256(lifted_dir),verifier=VERIFIER,states_per_function=states,seed=seed,counts=counts,results=results)
    (PROJECT/'build'/target).mkdir(parents=True,exist_ok=True)
    if not only and not limit:
        stem=report_name(lifted_dir)+'-verification'
        name=f'{stem}.part{part[0]}of{part[1]}.json' if part else f'{stem}.json'
        (PROJECT/'build'/target/name).write_text(json.dumps(report,indent=1))
    print(f'{target}: {counts}')
    return report

def ret_view(r,native):
    """Calls the native run made with `ret` (a return to a random address) and,
    when the run ended there, its final registers: only what callers see."""
    if native.ret_mask is None or not native.ret_calls or 'events' not in r:return r
    keep=lambda regs:[v if (native.ret_mask|ESP_BIT)>>k&1 else None for k,v in enumerate(regs)]
    events=[];n=0
    for e in r['events']:
        if e[0]=='call':
            if n in native.ret_calls:e=(e[0],e[1],tuple(keep(e[2])),e[3])
            n+=1
        events.append(e)
    out={**r,'events':events}
    if r.get('done') and native.halted and native.ncalls-1 in native.ret_calls:out['regs']=keep(r['regs'])
    return out

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
    hx=lambda v:'-' if v is None else f'{v:08x}'
    events=[(e[0],f'{e[1]:08x}',[hx(v) for v in e[2]],e[3].hex()) if e[0]=='call' else
            (f'{e[0]:08x}',e[1],None if e[2] is None else f'{e[2]:x}') for e in r.get('events',[])[:40]]
    if not r['done']:return dict(error=r.get('error'),events=events)  # unfinished runs keep their trace
    return dict(regs=[hx(v) for v in r['regs']],fpu=[str(v) for v in r['fpu']],sse=[f'{v:x}' for v in r['sse']],events=events)

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
    p.add_argument('--engine',choices=['py','cpp'],default='py',help='cpp: run the emulation in tools/vcore (much faster)')
    p.add_argument('--threads',type=int,default=8,help='vcore worker threads')
    a=p.parse_args()
    if a.merge:
        for t in ['client','server'] if a.target=='both' else [a.target]:
            stem=report_name(a.lifted_dir)+'-verification'
            parts=[json.loads((PROJECT/'build'/t/f'{stem}.part{k}of{a.merge}.json').read_text()) for k in range(a.merge)]
            assert len({(r['lifted_dir'],r['original_sha256'],r['sources_sha256'],r.get('runtime_sha256'),r['states_per_function'],r['seed']) for r in parts})==1
            results=sorted((r for q in parts for r in q['results']),key=lambda r:int(r['address'],16))
            counts={s:sum(r['status']==s for r in results) for s in ('passed','failed','unverified','native')}
            report={**parts[0],'counts':counts,'results':results}
            (PROJECT/'build'/t/f'{stem}.json').write_text(json.dumps(report,indent=1))
            print(f'{t}: merged {len(results)} results {counts}')
        raise SystemExit
    part=tuple(map(int,a.part.split('/'))) if a.part else None
    for t in ['client','server'] if a.target=='both' else [a.target]:
        old=PROJECT/'build'/t/f'{report_name(a.lifted_dir)}-verification.json'
        report=verify(t,a.game_dir,a.states,set(a.only or []),a.limit,a.seed,a.lifted_dir.resolve(),part,
                      old if a.reuse and old.exists() else None,a.engine,a.threads)
        for r in [r for r in report['results'] if r['status']=='failed'][:a.show_failures]:
            print(json.dumps(r)[:30000])
