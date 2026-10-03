"""Original/reconstructed actor scanning; geometry/event services are controlled."""
import struct
import itertools
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP
from native_oracle import load_machine, ARENA, STACK


def run_dispatcher(image, pe, entry, spec, case):
    m=load_machine(image,pe)
    objs={name:ARENA+0x100+i*0x300 for i,name in enumerate(
        ('member','component','manager','actor','entity','parent','body','pool_entry','clock'))}
    vts={name:ARENA+0x4000+i*0x300 for i,name in enumerate(objs)}
    origin,contact,candidate,buffer,event_iface,pool,records=[ARENA+x for x in (0x2000,0x2100,0x2200,0x2400,0x2500,0x2600,0x2700)]
    selector_cell,count_cell,distance_cell,time_cell= [ARENA+x for x in (0x2800,0x2810,0x2820,0x2830)]
    stop=ARENA+0x3000
    def w32(a,v): m.mem_write(a,struct.pack('<I',v))
    for name,p in objs.items(): w32(p,vts[name])
    w32(spec['collision_clock'],objs['clock']); w32(spec['collision_actors'],objs['manager']); w32(spec['pool'],pool); w32(pool,records)
    w32(records,0 if case.get('entity_null',False) else objs['entity'])
    m.mem_write(records+6,struct.pack('<H',2))
    w32(objs['entity']+0x20,objs['parent']); w32(event_iface+8,0xabcdef01)
    for address,words in [(origin,(0x3f800000,0x80000000,0x7fc12345)),
                          (contact,(0x40000000,0xbf800000,0x7f800000)),
                          (candidate,(0x3f000000,0x40400000,0x40800000))]:
        m.mem_write(address,struct.pack('<3I',*words))
    threshold=bytes(m.mem_read(spec['collision_distance_limit'],4))
    raw=struct.unpack('<I',threshold)[0]
    assert struct.unpack('<f',threshold)[0]>0
    raw={'below':raw-1,'equal':raw,'above':raw+1,'nan':0x7fc12345,'infinity':0x7f800000}[case.get('distance','below')]
    w32(distance_cell,raw)
    cursor=ARENA+0x8000; services={}; trace=[]
    def stub(name,value=0,pop=0,address=None,cell=None,floating=False):
        nonlocal cursor
        a=cursor if address is None else address
        if address is None: cursor+=0x40
        code=(b'\xd9\x05'+struct.pack('<I',cell) if floating else
              b'\xa1'+struct.pack('<I',cell) if cell is not None else b'\xb8'+struct.pack('<I',value))
        code+=b'\xc2'+struct.pack('<H',pop) if pop else b'\xc3'
        m.mem_write(a,code);services[a]=(name,pop);return a
    def method(obj,offset,name,value=0,pop=0,**kw):
        a=stub(name,value,pop,**kw);w32(vts[obj]+offset,a);return a
    method('component',0x18,'origin',origin)
    method('component',0x5c,'resolve',0x8888,4)
    method('manager',4,'count',cell=count_cell)
    method('manager',0x14,'actor',0 if case.get('actor_null',False) else objs['actor'],4)
    method('member',0x44,'member-id',0x1111)
    method('actor',0xa8,'actor-id',cell=selector_cell)
    method('actor',0xc4,'state',case.get('state',0))
    method('actor',0xcc,'handle',case.get('handle',0x20001))
    method('actor',0xd4,'actor-selector',0x77)
    method('actor',0x144,'attach',0,8)
    method('parent',0x14,'body',objs['body'])
    method('body',0x34,'position',candidate)
    method('clock',4,'clock',cell=time_cell,floating=True)
    method('pool_entry',0xc,'time',0,4)
    stub('distance',pop=36,address=spec['collision_distance'],cell=distance_cell,floating=True)
    stub('pool-entry',objs['pool_entry'],4,address=spec['collision_pool_entry'])
    stub('event-interface',event_iface if case.get('interface',True) else 0,4,address=spec['collision_event_interface'])
    stub('allocate',buffer if case.get('allocate',True) else 0,12,address=spec['collision_allocate'])
    stub('construct',buffer,44,address=spec['collision_construct'])
    esp=STACK+0x8000
    flags=case.get('flags',1); selector=case.get('selector',0x2222)
    # Five dispatcher arguments after the return address (handler is in ECX).
    m.mem_write(esp,struct.pack('<6I',stop,objs['member'],objs['component'],contact,selector,flags))
    m.reg_write(UC_X86_REG_ESP,esp);m.reg_write(UC_X86_REG_ECX,ARENA+0x100)
    returned=[]; counts=[]; ids=[]; clocks=[]; origins=[]; indices=[]
    def hook(uc,a,size,data):
        if a==stop: returned.append(True);uc.emu_stop();return
        if a not in services:return
        name,pop=services[a]; sp=uc.reg_read(UC_X86_REG_ESP)
        args=bytes(uc.mem_read(sp+4,pop));this=uc.reg_read(UC_X86_REG_ECX)
        # Geometry is stdcall; ECX is scratch, not a receiver.
        trace.append(dict(name=name,this=None if name=='distance' else f'{this:08x}',arguments=args.hex()))
        if name=='count':
            seq=case.get('counts',(case.get('count',1),)*8)
            value=seq[min(len(counts),len(seq)-1)];counts.append(value);w32(count_cell,value)
        if name=='actor': indices.append(struct.unpack('<I',args)[0])
        if name=='actor-id':
            seq=case.get('ids',(0x3333,0x3333,0x3333))
            value=seq[len(ids)%len(seq)];ids.append(value);w32(selector_cell,value)
        if name=='origin':
            origins.append(True)
            if len(origins)>1 and case.get('mutate',False): w32(origin,0xdeadbeef)
        if name=='clock':
            clocks.append(True);m.mem_write(time_cell,struct.pack('<f',0.125 if len(clocks)%2 else 0.375))
            if len(clocks)%2==0 and case.get('mutate',False):w32(contact,0x12345678)
        if name=='distance':
            assert args[:12]==bytes(m.mem_read(candidate,12))
            assert args[12:24]==struct.pack('<3I',0x3f800000,0x80000000,0x7fc12345)
            assert args[24:]==bytes(m.mem_read(contact,12))
        if name=='allocate':
            assert this==spec['collision_allocator']
            assert struct.unpack('<3I',args)==(0x38,spec['collision_alloc_source'],0)
        if name=='construct':
            assert this==buffer
            assert args[:12]==bytes(m.mem_read(origin,12)) and args[12:24]==bytes(m.mem_read(contact,12))
            strength=0x40800000 if flags &255 else 0x3f800000
            assert struct.unpack('<5I',args[24:])==(0xabcdef01,0,0x3ec00000,strength,flags)
            m.mem_write(buffer,b'\xa5'*0x38)
        if name=='attach':
            assert struct.unpack('<2I',args)==(buffer if case.get('allocate',True) else 0,0xffffffff)
    m.hook_add(UC_HOOK_CODE,hook)
    try:
        m.emu_start(entry,stop+1,timeout=1_000_000,count=20000)
    except Exception as error:
        raise RuntimeError((case, trace)) from error
    assert returned and m.reg_read(UC_X86_REG_ESP)==esp+24
    assert indices==list(range(len(indices)))
    return dict(trace=trace,event=bytes(m.mem_read(buffer,56)).hex())


def dispatcher_cases():
    yield {'count':0}
    yield {'count':3,'actor_null':True}
    for ids in ((0x1111,),(0x3333,0x2222),(0x3333,0x3333,0xffffffff),(0x3333,0x3333,0x3333)):
        for state,handle in itertools.product((0,1,256,257),(0,0x20001,0x30001)):
            yield dict(ids=ids,state=state,handle=handle)
    yield {'entity_null':True}
    for distance,interface,allocate,flags in itertools.product(('below','equal','above','nan','infinity'),
                                                              (False,True),(False,True),(0,1,256,0x12345601)):
        yield dict(distance=distance,interface=interface,allocate=allocate,flags=flags)
    yield {'count':3}
    yield {'counts':(1,3,3,3)}
    yield {'counts':(3,0)}
    yield {'mutate':True}
