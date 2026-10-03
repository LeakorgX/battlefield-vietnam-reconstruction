"""Opt-in Windows probe of ORIGINAL and reconstructed vector exception propagation.
Only starts/patches its own temporary, hash-verified game process. Game threads are
paused during probe execution; patches are restored before the child is stopped.
"""
import argparse
import ctypes as C
from ctypes import wintypes as W
import hashlib
import json
import os
import re
import struct
import subprocess
import time
from pathlib import Path
import capstone
import pefile
from build import PROJECT,GAME,TARGETS,COMPILERS


class Probe(C.Structure):
    _fields_=[(name,C.c_uint32) for name in ('status','caught','exception_code','handler_calls','fs_before',
       'fs_after','phase','mode','entry','size','count','capacity','position','value')]+[
       ('vector',C.c_uint32*4),('old_buffer',C.c_uint32),('freed_buffer',C.c_uint32),
       ('free_calls',C.c_uint32),('allocation_offset',C.c_uint32),('expected_allocation',C.c_uint32),('result',C.c_uint32*16)]


def api():
    k=C.WinDLL('kernel32',use_last_error=True);n=C.WinDLL('ntdll')
    declarations={
       'OpenProcess':([W.DWORD,W.BOOL,W.DWORD],W.HANDLE),
       'ReadProcessMemory':([W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)],W.BOOL),
       'WriteProcessMemory':([W.HANDLE,C.c_void_p,C.c_void_p,C.c_size_t,C.POINTER(C.c_size_t)],W.BOOL),
       'VirtualAllocEx':([W.HANDLE,C.c_void_p,C.c_size_t,W.DWORD,W.DWORD],C.c_void_p),
       'VirtualProtectEx':([W.HANDLE,C.c_void_p,C.c_size_t,W.DWORD,C.POINTER(W.DWORD)],W.BOOL),
       'FlushInstructionCache':([W.HANDLE,C.c_void_p,C.c_size_t],W.BOOL),
       'CreateRemoteThread':([W.HANDLE,C.c_void_p,C.c_size_t,C.c_void_p,C.c_void_p,W.DWORD,C.POINTER(W.DWORD)],W.HANDLE),
       'ResumeThread':([W.HANDLE],W.DWORD),'WaitForSingleObject':([W.HANDLE,W.DWORD],W.DWORD),
       'GetExitCodeThread':([W.HANDLE,C.POINTER(W.DWORD)],W.BOOL),
       'CloseHandle':([W.HANDLE],W.BOOL)}
    for name,(args,result) in declarations.items():getattr(k,name).argtypes=args;getattr(k,name).restype=result
    for name in ('NtSuspendProcess','NtResumeProcess'):
        getattr(n,name).argtypes=[W.HANDLE];getattr(n,name).restype=C.c_long
    return k,n


class Remote:
    def __init__(self,pid):
        self.k,self.n=api();self.handle=self.k.OpenProcess(0x1f0fff,False,pid)
        if not self.handle:raise C.WinError(C.get_last_error())
    def read(self,address,size):
        out=C.create_string_buffer(size);got=C.c_size_t()
        if not self.k.ReadProcessMemory(self.handle,address,out,size,C.byref(got)) or got.value!=size:
            raise C.WinError(C.get_last_error())
        return out.raw
    def write(self,address,data):
        old=W.DWORD();done=C.c_size_t()
        if not self.k.VirtualProtectEx(self.handle,address,len(data),0x40,C.byref(old)):raise C.WinError(C.get_last_error())
        try:
            if not self.k.WriteProcessMemory(self.handle,address,data,len(data),C.byref(done)) or done.value!=len(data):raise C.WinError(C.get_last_error())
            if not self.k.FlushInstructionCache(self.handle,address,len(data)):raise C.WinError(C.get_last_error())
        finally:
            previous=W.DWORD();self.k.VirtualProtectEx(self.handle,address,len(data),old.value,C.byref(previous))
    def u32(self,address):return struct.unpack('<I',self.read(address,4))[0]
    def suspend(self):
        status=self.n.NtSuspendProcess(self.handle)
        if status<0:raise RuntimeError(f'NtSuspendProcess: {status:08x}')
    def resume(self):
        status=self.n.NtResumeProcess(self.handle)
        if status<0:raise RuntimeError(f'NtResumeProcess: {status:08x}')
    def allocate(self):
        for address in (0x20000000,0x24000000,0x28000000):
            result=self.k.VirtualAllocEx(self.handle,address,0x10000,0x3000,0x40)
            if result==address:return address
        raise C.WinError(C.get_last_error())
    def thread(self,entry,argument):
        ident=W.DWORD();h=self.k.CreateRemoteThread(self.handle,None,0,entry,argument,4,C.byref(ident))
        if not h:raise C.WinError(C.get_last_error())
        try:
            if self.k.ResumeThread(h)==0xffffffff:raise C.WinError(C.get_last_error())
            wait=self.k.WaitForSingleObject(h,15000)
            if wait!=0:raise RuntimeError(f'Probe thread did not finish: wait={wait:08x}')
            code=W.DWORD()
            if not self.k.GetExitCodeThread(h,C.byref(code)):raise C.WinError(C.get_last_error())
            return code.value
        finally:self.k.CloseHandle(h)
    def close(self):self.k.CloseHandle(self.handle)


def build_probe(target,address,work):
    source=PROJECT/'tools/native-probe/exception_probe.c'
    os.environ['PATH']=str(COMPILERS)+os.pathsep+os.environ['PATH']
    def run(args):
        result=subprocess.run([str(x) for x in args],cwd=work,capture_output=True,text=True)
        if result.returncode:raise RuntimeError(result.stdout+result.stderr)
    run([COMPILERS/'gcc.exe','-m32','-std=c11','-O2','-Wall','-Wextra','-Werror','-ffreestanding',
         '-fno-builtin','-fno-stack-protector','-fno-asynchronous-unwind-tables','-fno-unwind-tables',
         '-I',PROJECT/'build'/target,'-c',source,'-o',work/'probe.o'])
    (work/'probe.ld').write_text(f'SECTIONS {{ . = 0x{address:x}; .probe : {{ *(.text*) *(.rdata*) *(.data*) *(.bss*) BYTE(0) }} /DISCARD/ : {{ *(.eh_frame*) *(.comment*) *(.drectve*) }} }}')
    run([COMPILERS/'ld.exe','-T',work/'probe.ld','--image-base','0x400000','--entry','_bfv_probe_entry@4',
         '--disable-dynamicbase','--disable-reloc-section','-Map',work/'probe.map','-o',work/'probe.exe',work/'probe.o'])
    run([COMPILERS/'objcopy.exe','-O','binary','--only-section','.probe',work/'probe.exe',work/'probe.bin'])
    symbols={}
    for line in (work/'probe.map').read_text().splitlines():
        match=re.match(r'\s*(0x[0-9a-fA-F]+)\s+[_@]?(bfv_probe_[A-Za-z0-9_]+)(?:@\d+)?\s*$',line)
        if match:symbols[match[2]]=int(match[1],16)
    data=(work/'probe.bin').read_bytes();pe=pefile.PE(str(work/'probe.exe'))
    section=next(s for s in pe.sections if s.Name.rstrip(b'\0')==b'.probe')
    assert pe.OPTIONAL_HEADER.ImageBase+section.VirtualAddress==address and len(data)<0x8000
    return data,symbols


def calls(remote,start,size,target):
    md=capstone.Cs(capstone.CS_ARCH_X86,capstone.CS_MODE_32);md.detail=True
    return [i.address for i in md.disasm(remote.read(start,size),start)
            if i.mnemonic=='call' and i.size==5 and i.bytes[0]==0xe8 and i.operands[0].imm==target]


def catch_free(remote,handler):
    assert remote.read(handler,1)==b'\xb8'
    info=remote.u32(handler+1);try_map=remote.u32(info+16)
    assert remote.u32(try_map)==0 and remote.u32(try_map+4)==0
    handlers=remote.u32(try_map+16)
    return remote.u32(handlers+12)


def verify(target,game_dir):
    spec=TARGETS[target];work=PROJECT/'build'/target/'live-exceptions';work.mkdir(parents=True,exist_ok=True)
    manifest=json.loads((PROJECT/'build'/target/'manifest.json').read_text())
    exe=Path(manifest['output']).resolve()
    assert exe==game_dir/spec['output']
    assert hashlib.sha256(exe.read_bytes()).hexdigest()==manifest['output_sha256']
    assert hashlib.sha256((game_dir/spec['file']).read_bytes()).hexdigest()==spec['sha']
    settings=(game_dir/'Mods/BfVietnam/settings/ServerSettings.con').read_text(errors='replace')
    assert re.search(r'^game.serverInternet\s+0\s*$',settings,re.M),'Private server configuration required'
    syms={k:int(v,16) for k,v in manifest['symbols'].items()}
    argv=[str(exe),'+game','BFVietnam','+restart','1','+hostServer','1'] if target=='server' else [str(exe),'+game','BFVietnam','+window','1','+szx','800','+szy','600']
    startup=subprocess.STARTUPINFO();startup.dwFlags|=subprocess.STARTF_USESHOWWINDOW;startup.wShowWindow=0
    output=(work/'process.log').open('wb')
    process=subprocess.Popen(argv,cwd=game_dir,stdout=output,stderr=output,startupinfo=startup,creationflags=0x08000000)
    remote=None;suspended=False;patches=[];results=[]
    try:
        remote=Remote(process.pid);print(f'{target}: temporary process started; waiting for engine initialization',flush=True)
        idle=C.WinDLL('user32').WaitForInputIdle
        idle.argtypes=[W.HANDLE,W.DWORD];idle.restype=W.DWORD
        deadline=time.monotonic()+90
        while time.monotonic()<deadline:
            if process.poll() is not None:raise RuntimeError(f'Game closed during initialization: {process.returncode}')
            if remote.u32(spec['collision_event_manager']) and remote.u32(spec['collision_actors']):break
            if target=='client' and idle(remote.handle,0)==0:break
            time.sleep(0.25)
        else:raise RuntimeError('Engine initialization timeout')
        # Verify the native insertion/catch block too, not just the added payload.
        disk=pefile.PE(str(exe))
        native_start=spec['collision_vector_insert']
        assert remote.read(native_start,0x230)==disk.get_data(native_start-disk.OPTIONAL_HEADER.ImageBase,0x230)
        # Verify immutable payload and every redirected slot in this exact child.
        payload=(PROJECT/'build'/target/'payload.bin').read_bytes()
        payload_address=int(manifest['payload_address'],16)
        live=bytearray(remote.read(payload_address,len(payload)))
        # These documented invocation counters are mutable during normal startup.
        for name,address in syms.items():
            if name.endswith('_calls'):
                offset=address-payload_address
                assert 0<=offset<=len(payload)-4
                live[offset:offset+4]=payload[offset:offset+4]
        assert bytes(live)==payload
        for patch in manifest['patches']:assert remote.u32(int(patch['vtable_slot'],16))==int(patch['replacement'],16)
        for patch in manifest.get('entry_patches',[]):
            assert remote.read(int(patch['entry'],16),5)==bytes.fromhex(patch['patched'])
        remote.suspend();suspended=True
        base=remote.allocate();data,probe=build_probe(target,base,work);remote.write(base,data)
        context=base+0x8000
        for kind in ('original','reconstructed'):
            entry=spec['collision_vector_insert'] if kind=='original' else syms['bfv_vector_insert']
            body=entry if kind=='original' else syms['bfv_vector_insert_body']
            body_end=entry+0x230 if kind=='original' else min(v for v in syms.values() if v>body)
            handler=spec['vector_original_handler'] if kind=='original' else syms['bfv_vector_handler']
            cleanup=catch_free(remote,handler)
            cleanup_calls=calls(remote,cleanup,32,spec['vector_free']);assert len(cleanup_calls)==1
            for mode in ('normal','length','growth-fault','inplace-fault'):
                p=Probe();p.entry=entry;p.value=0xfeedbeef;p.allocation_offset=24
                if mode=='length':p.count=0x40000000
                else:p.size=2;p.capacity=8 if mode=='inplace-fault' else 2;p.count=3 if mode=='inplace-fault' else 2;p.position=1
                remote.write(context,bytes(p));patches=[]
                def patch_call(address,destination):
                    old=remote.read(address,5);assert old[0]==0xe8
                    patched=b'\xe8'+struct.pack('<I',(destination-address-5)&0xffffffff)
                    remote.write(address,patched);patches.append((address,old,patched))
                try:
                    patch_call(cleanup_calls[0],probe['bfv_probe_free'])
                    if mode.endswith('fault'):
                        name='fill' if mode=='inplace-fault' else 'copy'
                        target_entry=spec['vector_'+name] if kind=='original' else syms['bfv_vector_'+name]
                        sites=calls(remote,body,body_end-body,target_entry);assert sites,(kind,name)
                        for site in sites:patch_call(site,probe['bfv_probe_fault'])
                    code=remote.thread(probe['bfv_probe_entry'],context)
                    p=Probe.from_buffer_copy(remote.read(context,C.sizeof(Probe)))
                    row=dict(implementation=kind,case=mode,thread_exit=f'{code:08x}',status=p.status,caught=bool(p.caught),
                         exception_code=f'{p.exception_code:08x}',handler_calls=p.handler_calls,phase=p.phase,
                         fs_restored=p.fs_before==p.fs_after,cleanup_calls=p.free_calls,
                         released_new_allocation=bool(p.expected_allocation and p.freed_buffer==p.expected_allocation and p.freed_buffer!=p.old_buffer),
                         size=(p.vector[2]-p.vector[1])//4,capacity=(p.vector[3]-p.vector[1])//4,
                         words=[f'{x:08x}' for x in p.result[:min(16,(p.vector[2]-p.vector[1])//4)]])
                    results.append(row);print(f'{target}/{kind}/{mode}: {json.dumps(row)}',flush=True)
                    assert row['fs_restored'] and code==row['status'],row
                    if mode=='normal':assert code==1 and not p.caught and row['words']==['00000100','feedbeef','feedbeef','00000101'],row
                    else:
                        assert code==2 and p.caught and p.exception_code==0xe06d7363,row
                        assert p.free_calls==(1 if mode=='growth-fault' else 0),row
                        if mode=='growth-fault':assert row['released_new_allocation'],row
                finally:
                    for address,old,patched in reversed(patches):
                        assert remote.read(address,5)==patched
                        remote.write(address,old)
                    patches=[]
        for case in ('normal','length','growth-fault','inplace-fault'):
            a,b=[r for r in results if r['case']==case]
            assert {k:v for k,v in a.items() if k not in ('implementation','handler_calls')}=={k:v for k,v in b.items() if k not in ('implementation','handler_calls')},(a,b)
        report=dict(target=target,compiled_sha256=manifest['output_sha256'],original_sha256=spec['sha'],passed=len(results),
           scope='Temporary private Windows process with other game threads paused. Real game C++ runtime, allocation/deallocation, exception dispatch, catch/rethrow and FS restoration. Fault calls redirected only inside the tested insertion function. This does not verify every exception type, asynchronous fault, heap internals or live-match event lifetime.',cases=results)
        (work/'verification.json').write_text(json.dumps(report,indent=2)+'\n',encoding='utf-8')
        return report
    finally:
        if remote and suspended:
            try:remote.resume()
            except (OSError,RuntimeError):pass
        if process.poll() is None:
            process.terminate()
            try:process.wait(timeout=10)
            except subprocess.TimeoutExpired:process.kill();process.wait(timeout=10)
        if remote:remote.close()
        output.close()


if __name__=='__main__':
    parser=argparse.ArgumentParser();parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument('--target',choices=('client','server','both'),default='server');args=parser.parse_args()
    for target in ('client','server') if args.target=='both' else (args.target,):verify(target,args.game_dir.resolve())
