"""Original/reconstructed insertion with controlled CRT allocation and fault dispatch.
The actual CRT search/unwind implementation is retained, not modeled here.
"""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import *
from native_oracle import load_machine, ARENA, STACK


def function_info(m,handler):
    # Both ABI thunks load the function-info address into EAX before a tail jump.
    assert bytes(m.mem_read(handler,1))==b'\xb8'
    info=struct.unpack('<I',m.mem_read(handler+1,4))[0]
    magic,states,unwind,tries,try_map,ips,ip_map,spec=struct.unpack('<8I',m.mem_read(info,32))
    entries=[]
    for i in range(tries):
        low,high,catch_high,count,handlers=struct.unpack('<5I',m.mem_read(try_map+i*20,20))
        assert count==1
        flags,kind,offset,catch=struct.unpack('<4I',m.mem_read(handlers,16))
        entries.append(dict(low=low,high=high,catch_high=catch_high,flags=flags,type=kind,offset=offset,catch=catch))
    normalized=dict(magic=magic,states=states,unwind=bytes(m.mem_read(unwind,states*8)).hex(),
                    tries=[{k:v for k,v in e.items() if k!='catch'} for e in entries],ips=ips,ip_map=ip_map,spec=spec)
    return normalized,entries,info


def install_vector_runtime(m,spec,base):
    """Fresh emulated FS chain and controlled allocator/memmove for integration."""
    m.mem_map(0,0x1000);m.mem_write(0,struct.pack('<I',0x12345678))
    m.mem_write(spec['vector_allocator'],b'\xc3')
    m.mem_write(spec['vector_free'],b'\xc3')
    m.mem_write(spec['vector_memmove'],b'\x8b\x44\x24\x04\xc3')
    allocations=[]
    def hook(uc,a,size,data):
        sp=uc.reg_read(UC_X86_REG_ESP)
        if a==spec['vector_allocator']:
            amount=struct.unpack('<I',uc.mem_read(sp+4,4))[0]
            address=base+len(allocations)*0x100;allocations.append(amount)
            uc.reg_write(UC_X86_REG_EAX,address)
        if a==spec['vector_memmove']:
            dst,src,n=struct.unpack('<3I',uc.mem_read(sp+4,12))
            if n:uc.mem_write(dst,bytes(uc.mem_read(src,n)))
    m.hook_add(UC_HOOK_CODE,hook)
    return allocations


def run_vector(image,pe,entry,spec,case,symbols=None):
    m=load_machine(image,pe);obj,storage,value,allocated=[ARENA+x for x in (0x100,0x1000,0x2000,0x4000)]
    initial_chain,stop=0x12345678,ARENA+0x7000
    m.mem_map(0,0x1000);m.mem_write(0,struct.pack('<I',initial_chain))
    def r32(p):return struct.unpack('<I',m.mem_read(p,4))[0]
    def w32(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    size,capacity,count,position=case['size'],case['capacity'],case['count'],case['position']
    begin=storage if capacity else 0;end=begin+size*4
    m.mem_write(obj,struct.pack('<4I',0xaabbccdd,begin,end,begin+capacity*4))
    m.mem_write(storage,struct.pack('<32I',*[0x11110000+i for i in range(32)]))
    w32(value,0xfeedbeef)
    source=storage+case['alias']*4 if 'alias' in case else value
    position=begin+position*4
    m.mem_write(spec['vector_allocator'],b'\xb8'+struct.pack('<I',allocated)+b'\xc3')
    m.mem_write(spec['vector_free'],b'\xc3')
    m.mem_write(spec['vector_memmove'],b'\x8b\x44\x24\x04\xc3')
    helpers={spec['vector_'+k]:k for k in ('copy','fill','shift','fill_range')}
    if symbols:helpers={symbols['bfv_vector_'+k]:k for k in ('copy','fill','shift','fill_range')}
    esp=STACK+0x8000;m.mem_write(esp,struct.pack('<4I',stop,position,count,source))
    m.reg_write(UC_X86_REG_ESP,esp);m.reg_write(UC_X86_REG_ECX,obj)
    saved={UC_X86_REG_EBX:0x11111111,UC_X86_REG_ESI:0x22222222,UC_X86_REG_EDI:0x33333333,UC_X86_REG_EBP:0x44444444}
    for reg,v in saved.items():m.reg_write(reg,v)
    trace=[];returned=[];fault=[];calls={};infos=[];throws=[]
    def hook(uc,a,instruction_size,data):
        if a==stop:returned.append(True);uc.emu_stop();return
        sp=uc.reg_read(UC_X86_REG_ESP);frame=r32(0)
        if frame==initial_chain:return
        state=r32(frame+8)
        if a==spec['vector_allocator']:
            trace.append(dict(name='allocate',bytes=r32(sp+4),state=state))
            if case.get('mutate',False):w32(source,0x12345678);w32(storage,0xabcdef01)
        if a==spec['vector_free']:
            trace.append(dict(name='free',pointer=r32(sp+4),state=state))
        if a==spec['vector_length_error']:
            trace.append(dict(name='length-error',this=uc.reg_read(UC_X86_REG_ECX),state=state));fault.append('length');uc.emu_stop()
        if a==spec['vector_memmove']:
            dst,src,n=struct.unpack('<3I',uc.mem_read(sp+4,12))
            if n:uc.mem_write(dst,bytes(uc.mem_read(src,n)))
        if a in helpers:
            name=helpers[a];calls[name]=calls.get(name,0)+1
            if name=='copy':args=list(struct.unpack('<3I',uc.mem_read(sp+4,12)))
            elif name=='fill':
                start,n,p=struct.unpack('<3I',uc.mem_read(sp+4,12));args=[start,n,r32(p)]
            else:
                p=r32(sp+4);args=[uc.reg_read(UC_X86_REG_ECX),uc.reg_read(UC_X86_REG_EDX),r32(p) if name=='fill_range' else p]
            trace.append(dict(name=name,arguments=args,state=state))
            if case.get('fault')==f'{name}:{calls[name]}':fault.append(name);uc.emu_stop()
        if a==spec['vector_throw']:
            throws.append(list(struct.unpack('<2I',uc.mem_read(sp+4,8))));uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(entry,stop+1,timeout=1_000_000,count=30000)
    if returned:
        assert m.reg_read(UC_X86_REG_ESP)==esp+16 and r32(0)==initial_chain
        assert all(m.reg_read(reg)==v for reg,v in saved.items())
    else:
        assert fault
        frame=r32(0);info,catches,_=function_info(m,r32(frame+4));infos.append(info)
        state=r32(frame+8)
        if fault[-1]!='length':
            applicable=[c for c in catches if c['low']<=state<=c['high']]
            if applicable:
                c=applicable[0];w32(frame+8,c['catch_high'])
                m.reg_write(UC_X86_REG_EBP,frame+12)
                m.emu_start(c['catch'],stop+1,timeout=1_000_000,count=1000)
                assert throws==[[0,0]]
    return dict(trace=trace,object=bytes(m.mem_read(obj,16)).hex(),old=bytes(m.mem_read(storage,128)).hex(),
                allocated=bytes(m.mem_read(allocated,128)).hex(),returned=bool(returned),fault=fault,info=infos,rethrow=throws)


def vector_cases():
    for size in range(6):
        for capacity in sorted({size,size+1,size+4}):
            for position,count in itertools.product(range(size+1),(0,1,2,4)):
                yield dict(size=size,capacity=capacity,position=position,count=count)
                if size:yield dict(size=size,capacity=capacity,position=position,count=count,alias=size-1)
    yield dict(size=3,capacity=3,position=0,count=2,mutate=True,alias=0)
    yield dict(size=2,capacity=2,position=0,count=0x3fffffff)
    for size,capacity,position,count in ((2,2,1,2),(2,8,1,3),(5,10,1,2)):
        for stage in ('copy:1','fill:1','copy:2','fill_range:1'):
            yield dict(size=size,capacity=capacity,position=position,count=count,fault=stage)


def run_handler(image,pe,handler,runtime):
    m=load_machine(image,pe);stop=ARENA+0x7000;esp=STACK+0x8000
    arguments=(0x11111111,0x22222222,0x33333333,0x44444444)
    m.mem_write(esp,struct.pack('<5I',stop,*arguments));m.reg_write(UC_X86_REG_ESP,esp)
    normalized,_,info=function_info(m,handler)
    m.mem_write(runtime,b'\xb8\x78\x56\x34\x12\xc3');observed=[];returned=[]
    def hook(uc,a,size,data):
        if a==runtime:
            assert uc.reg_read(UC_X86_REG_EAX)==info
            assert tuple(struct.unpack('<4I',uc.mem_read(uc.reg_read(UC_X86_REG_ESP)+4,16)))==arguments
            observed.append(True)
        if a==stop:returned.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(handler,stop+1,timeout=1_000_000,count=1000)
    assert observed and returned and m.reg_read(UC_X86_REG_ESP)==esp+4 and m.reg_read(UC_X86_REG_EAX)==0x12345678
    return normalized
