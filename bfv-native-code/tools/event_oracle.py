"""Original/reconstructed event effects and lookup return/argument cleanup."""
import itertools
import struct
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_EAX, UC_X86_REG_ECX, UC_X86_REG_ESP
from native_oracle import load_machine, ARENA, STACK
from vector_oracle import install_vector_runtime


def run_constructor(image, pe, entry, spec, case, insert_entry=None):
    m=load_machine(image,pe)
    event,manager,vtable,source,storage= [ARENA+x for x in (0x100,0x400,0x800,0x1000,0x2000)]
    getter,stop=ARENA+0x5000,ARENA+0x5100
    def w32(p,v):m.mem_write(p,struct.pack('<I',v&0xffffffff))
    def r32(p):return struct.unpack('<I',m.mem_read(p,4))[0]
    m.mem_write(event,b'\x55'*56)
    m.mem_write(source,struct.pack('<8I',*[0xabcdef00+i for i in range(8)]))
    w32(spec['collision_event_manager'],manager);w32(manager,vtable);w32(vtable+0x2c,getter)
    m.mem_write(getter,b'\xb8'+struct.pack('<I',case['count']&0xffffffff)+b'\xc3')
    insert_entry=insert_entry or spec['collision_vector_insert']
    if case.get('real_vector',False):install_vector_runtime(m,spec,storage)
    else:m.mem_write(insert_entry,b'\xc2\x0c\x00')
    words=(0x3f800000,0x80000000,0x7fc12345,0x7f812345,0xffc12345,1,
           source,case['kind'],case.get('time',0x3ec00000),case.get('strength',0x40800000),case['flags'])
    esp=STACK+0x8000;m.mem_write(esp,struct.pack('<12I',stop,*words))
    m.reg_write(UC_X86_REG_ESP,esp);m.reg_write(UC_X86_REG_ECX,event)
    trace=[];returned=[]
    def hook(uc,a,size,data):
        if a==stop:returned.append(True);uc.emu_stop();return
        if a==getter:
            assert uc.reg_read(UC_X86_REG_ECX)==manager
            trace.append(dict(name='count',event=bytes(uc.mem_read(event,56)).hex()))
            if case.get('mutate',False):w32(event+20,0xdeadbeef)
        if a==insert_entry:
            sp=uc.reg_read(UC_X86_REG_ESP)
            position,count,values=struct.unpack('<3I',uc.mem_read(sp+4,12))
            assert uc.reg_read(UC_X86_REG_ECX)==event+4 and count==1
            assert position==r32(event+12)
            value=r32(values);old_begin=r32(event+8);old_end=r32(event+12)
            used=(old_end-old_begin)//4 if old_begin else 0
            capacity=used+case.get('reserve',1)
            trace.append(dict(name='insert',position=f'{position:08x}',value=f'{value:08x}',source=f'{values:08x}'))
            if not case.get('real_vector',False):
                if old_begin:assert old_begin==storage
                w32(event+8,storage);w32(storage+used*4,value)
                w32(event+12,storage+(used+1)*4);w32(event+16,storage+capacity*4)
            if case.get('mutate',False):w32(values+4,0x12345678)
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(entry,stop+1,timeout=1_000_000,count=10000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==esp+48
    assert m.reg_read(UC_X86_REG_EAX)==event
    return dict(trace=trace,event=bytes(m.mem_read(event,56)).hex(),storage=bytes(m.mem_read(r32(event+8),32)).hex() if r32(event+8) else None)


def constructor_cases():
    for count,kind,flags,reserve in itertools.product((-2,0,1,2,5),(0,1,256,0x12345601),
                                                      (0,1,256,0xffffffff),(1,8)):
        yield dict(count=count,kind=kind,flags=flags,reserve=reserve)
    for raw in (0,0x80000000,1,0x7f800000,0xff800000,0x7fc12345,0x7f812345,0xffc12345):
        yield dict(count=3,kind=257,flags=0x12345601,time=raw,strength=raw,mutate=True,reserve=1)

    for count,mutate in itertools.product((0,1,3,8),(False,True)):
        yield dict(count=count,kind=257,flags=0x12345601,real_vector=True,mutate=mutate)


def run_lookup(image, pe, entry, kind, case):
    m=load_machine(image,pe);obj,records,stop=ARENA+0x100,ARENA+0x1000,ARENA+0x5000
    def w32(p,v):m.mem_write(p,struct.pack('<I',v))
    if kind=='object':
        w32(obj,records)
        for i in range(4):
            w32(records+i*8,0 if i==2 else 0x12345670+i)
            m.mem_write(records+i*8+6,struct.pack('<H',case['generation']))
        argument=case['handle'];this=obj
    else:
        this=0xffffffdc if case.get('table_null',False) else obj
        argument=case['index']
        w32((obj+0x24+argument*4)&0xffffffff,case['value'])
    esp=STACK+0x8000;m.mem_write(esp,struct.pack('<2I',stop,argument))
    m.reg_write(UC_X86_REG_ESP,esp);m.reg_write(UC_X86_REG_ECX,this);returned=[]
    def hook(uc,a,size,data):
        if a==stop:returned.append(True);uc.emu_stop()
    m.hook_add(UC_HOOK_CODE,hook);m.emu_start(entry,stop+1,timeout=1_000_000,count=1000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==esp+8
    return m.reg_read(UC_X86_REG_EAX)


def lookup_cases():
    for generation,tag,index in itertools.product((0,1,0xffff),(0,1,0xffff),range(5)):
        yield 'object',dict(generation=generation,handle=tag<<16|index)
    for index,value,null in itertools.product((0,1,3,7,0xfffffff8),(0,0x12345678,0xffffffff),(False,True)):
        yield 'interface',dict(index=index,value=value,table_null=null)


def install_event_fixture(m,spec,stub,services,event,event_iface,base,words,insert_entry=None,real_vector=False):
    """Controlled count/growth around real constructor execution in integration cases."""
    manager,vtable,data,storage=base,base+0x100,base+0x200,base+0x300
    def w32(p,v):m.mem_write(p,struct.pack('<I',v))
    w32(spec['collision_event_manager'],manager);w32(manager,vtable)
    w32(vtable+0x2c,stub('event-count',words));w32(event_iface+8,data)
    m.mem_write(data,struct.pack('<8I',*[0xabcdef00+i for i in range(8)]))
    insert_entry=insert_entry or spec['collision_vector_insert']
    if real_vector:
        services[insert_entry]=('vector-insert',12)
        install_vector_runtime(m,spec,base+0x400)
    else:stub('vector-insert',0,12,address=insert_entry)
    def effect(name,this,args):
        if name=='event-count':assert this==manager
        if name=='vector-insert' and not real_vector:
            position,count,source=struct.unpack('<3I',args)
            assert this==event+4 and count==1
            assert position==struct.unpack('<I',m.mem_read(event+12,4))[0]
            begin,end=struct.unpack('<2I',m.mem_read(event+8,8))
            used=(end-begin)//4 if begin else 0
            w32(event+8,storage);m.mem_write(storage+used*4,bytes(m.mem_read(source,4)))
            w32(event+12,storage+(used+1)*4);w32(event+16,storage+8*4)
    return data,storage,effect
