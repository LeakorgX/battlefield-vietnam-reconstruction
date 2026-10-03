"""Compare collision callback instructions with controlled native services.

No game collision/event service implementation is fabricated by this fixture.
Stubs record their exact this pointer/stack arguments and selected side effects.
"""
import struct
import itertools
from unicorn import UC_HOOK_CODE
from unicorn.x86_const import UC_X86_REG_ECX, UC_X86_REG_ESP
from native_oracle import load_machine, ARENA, STACK
from event_oracle import install_event_fixture


def run_collision(image, pe, entry, spec, case, dispatch_entry=None, helpers=None):
    helpers=helpers or {}
    real=case.get("real_events",False)
    m = load_machine(image, pe)
    def a(offset): return ARENA+offset
    source, other, handler = a(0x100), a(0x400), a(0x700)
    source_iface, other_iface, member, member_body = a(0xa00), a(0xd00), a(0x1000), a(0x1300)
    registry, actors, source_actor, other_actor = a(0x1600), a(0x1900), a(0x1c00), a(0x1f00)
    source_comp, other_comp, pool_entry, clock = a(0x2200), a(0x2500), a(0x2800), a(0x2b00)
    event_iface, buffer, record, source_view, other_view = a(0x2e00), a(0x3100), a(0x3400), a(0x3700), a(0x3a00)
    source_position, member_position = a(0x3d00), a(0x3e00)
    return_cell, float_cell, stop = a(0x3f00), a(0x3f10), a(0x3f20)
    vtables = {}
    for i,obj in enumerate((source,other,source_iface,other_iface,member,member_body,registry,actors,
                             source_actor,other_actor,source_comp,other_comp,pool_entry,clock)):
        vtables[obj] = a(0x4000+i*0x300)
    def w32(address, value): m.mem_write(address, struct.pack('<I', value))
    for obj,vt in vtables.items(): w32(obj, vt)
    w32(source+0x4c, source_iface); w32(other+0x4c, other_iface)
    w32(source+0x164, 0x76543210); w32(source+0x15c, 0x76543210)
    w32(source+spec['collision_handle_field'], 0x1234); w32(source+0x48, 0x11223344)
    w32(source_iface+0x1ec,0x76543210); w32(source_iface+0x1c0,0x76543210)
    w32(source_iface+spec['collision_notify_state_field'],case.get('notify_state',1))
    w32(source_view+0x5c, source_comp if case.get('source_component',True) else 0)
    w32(other_view+0x5c, other_comp if case.get('other_component',True) else 0)
    w32(spec['collision_interface_id'], 0x55667788)
    w32(spec['collision_registry'], registry); w32(spec['collision_actors'], actors)
    w32(spec['collision_clock'], clock); w32(spec['pool'], a(0x3f80))
    w32(event_iface+8, 0xabcdef01); w32(record+0x30, 0xffffffff)
    m.mem_write(source_position, struct.pack('<3I', 0x80000000, 0x7fc12345, 0x3f800000))
    m.mem_write(member_position, struct.pack('<3I', 0x40000000, 0xbf800000, 0x7f800000))
    cursor = a(0x8000)
    services = {}
    trace = []
    def stub(name, value=0, pop=0, address=None, floating=False, dynamic=False):
        nonlocal cursor
        target = address if address is not None else cursor
        if address is None: cursor += 0x40
        code = b'\xd9\x05'+struct.pack('<I',float_cell) if floating else (
            b'\xa1'+struct.pack('<I',return_cell) if dynamic else b'\xb8'+struct.pack('<I',value))
        code += b'\xc2'+struct.pack('<H',pop) if pop else b'\xc3'
        m.mem_write(target, code); services[target] = (name,pop)
        return target
    def method(obj, offset, name, value=0, pop=0, **options):
        target = stub(name,value,pop,**options); w32(vtables[obj]+offset,target); return target
    method(source_iface,0x2c,'query-source',int(case.get('query',True)),4)
    method(other_iface,0x2c,'query-other',1,4)
    method(registry,0x1c,'registry-member',member if case.get('member',True) else 0,4)
    method(member,0x38,'member-body',member_body if case.get('member_body',True) else 0)
    method(actors,0x1c,'source-actor',source_actor if case.get('source_actor',0)>=0 else 0,4)
    method(actors,0x10,'other-actor',other_actor if case.get('other_actor',0)>=0 else 0,4)
    method(actors,4,'actor-count',0)
    method(source_actor,0xc4,'source-state',max(case.get('source_actor',0),0))
    method(other_actor,0xc4,'other-state',max(case.get('other_actor',0),0))
    method(source_actor,0xd4,'source-selector',0x20)
    method(other_actor,0xd4,'other-selector',0x30)
    method(source_comp,0x5c,'resolve-source',0x20002 if real else 0x4444,4)
    method(other_comp,0x5c,'resolve-other',0x5555,4)
    origin_stub=method(source_comp,0x18,'component-origin',member_position)
    w32(a(0x7a00)+0x18,origin_stub)
    method(source_actor,0x88,'record',record if case.get('record',True) else 0,4)
    method(source_actor,0x148,'source-event',0,12)
    method(other_actor,0x144,'attach-event',0,8)
    method(other_actor,0xa8,'event-selector',0x77)
    method(pool_entry,0xc,'update-time',0,4)
    method(clock,4,'clock',floating=True)
    method(source,0x34,'source-position',source_position)
    method(member_body,0x34,'member-position',member_position)
    stub('body-view',address=spec['collision_body_view'],dynamic=True)
    pool_helper=helpers.get('pool',spec['collision_pool_entry'])
    interface_helper=helpers.get('interface',spec['collision_event_interface'])
    if real:
        w32(a(0x3f80),a(0x9000));w32(a(0x9008),pool_entry)
        m.mem_write(a(0x900e),struct.pack('<H',2));w32(pool_entry+0x30,event_iface)
        services[pool_helper]=('pool-entry',4);services[interface_helper]=('event-interface',4)
    else:
        stub('pool-entry',pool_entry,4,address=pool_helper)
        stub('event-interface',event_iface if case.get('event_interface',True) else 0,4,address=interface_helper)
    stub('allocate',buffer if case.get('allocate',True) else 0,12,address=spec['collision_allocate'])
    construct_helper=helpers.get('construct',spec['collision_construct'])
    if real:
        services[construct_helper]=('construct',44)
        event_data,event_storage,event_effect=install_event_fixture(m,spec,stub,services,buffer,event_iface,a(0x9500),case['event_words'],
            insert_entry=helpers.get('insert'),real_vector=case.get('real_vector',False))
    else:
        event_data=0xabcdef01
        stub('construct',buffer,44,address=construct_helper)
    # Execute the actual dispatcher with an empty actor list in integration cases.
    services[dispatch_entry or spec['collision_dispatch']]=('dispatch',20)
    # Mutation cases distinguish saved vtable pointers from early method reads.
    alternate_source = stub('resolve-source-updated',0x20002 if real else 0x4444,4)
    alternate_other = stub('resolve-other-updated',0x5555,4)
    alternate_time = stub('update-time-updated',0,4)
    m.mem_write(stop,b'\x90')
    esp = STACK+0x8000
    if case.get('notify_only',False):
        frame=struct.pack('<5I',stop,member,source_comp,source,case['selector'])
    else:
        frame=struct.pack('<4I',stop,source if case.get('source',True) else 0,
                          other if case.get('other',True) else 0,0x55aa77ff)
    m.mem_write(esp,frame)
    m.reg_write(UC_X86_REG_ESP,esp); m.reg_write(UC_X86_REG_ECX,handler)
    returned, clock_calls = [], []
    def hook(uc,address,size,data):
        if address == stop: returned.append(True); uc.emu_stop(); return
        if address not in services: return
        name,pop = services[address]
        sp,this = uc.reg_read(UC_X86_REG_ESP),uc.reg_read(UC_X86_REG_ECX)
        args = bytes(uc.mem_read(sp+4,pop))
        trace.append(dict(name=name,this=f'{this:08x}',arguments=args.hex()))
        if name == 'body-view':
            assert this in (member_body,other)
            w32(return_cell,source_view if this == member_body else other_view)
        if name == 'clock':
            clock_calls.append(True)
            m.mem_write(float_cell,struct.pack('<f',0.125 if len(clock_calls)==1 else 0.375))
            if len(clock_calls)==2: w32(source_position,0xdeadbeef)
            if case.get('mutate',False): w32(vtables[pool_entry]+0xc,alternate_time)
        if name == 'source-position' and case.get('notify_only',False) and case.get('mutate',False):
            w32(source_iface+spec['collision_notify_state_field'],0x98765432)
        if case.get('mutate',False):
            if name == 'other-selector':
                w32(vtables[source_comp]+0x5c,alternate_source)
                w32(source_comp,a(0x7a00))
            if name == 'source-selector':
                w32(vtables[other_comp]+0x5c,alternate_other)
                w32(other_comp,a(0x7a00))
        if name == 'allocate':
            assert this == spec['collision_allocator']
            assert struct.unpack('<3I',args) == (0x38,spec['collision_alloc_source'],0)
        if name == 'construct':
            assert this == buffer and struct.unpack('<I',args[24:28])[0] == event_data, (hex(this),args.hex())
            assert args[:12] == bytes(m.mem_read(member_position,12))
            assert args[12:24] == bytes(m.mem_read(source_position,12))
            assert struct.unpack('<4I',args[28:]) == (1,0x3ec00000,0x3f800000,0)
            if not real: m.mem_write(buffer,b'\xa5'*0x38)
        if real: event_effect(name,this,args)
        if name == 'dispatch':
            assert this == handler
            state=case.get('notify_state',1)
            member_arg,component_arg,position_arg,selector,flag_word=struct.unpack('<5I',args)
            assert (member_arg,component_arg,position_arg)==(member,source_comp,source_position)
            assert flag_word==(state & 0xffffff00)|(state==1)
            if case.get('notify_only',False): assert selector==case['selector']
    m.hook_add(UC_HOOK_CODE,hook)
    m.emu_start(entry,stop+1,timeout=1_000_000,count=10000)
    assert returned and m.reg_read(UC_X86_REG_ESP)==esp+len(frame)
    return dict(trace=trace,counter=struct.unpack('<I',m.mem_read(record+0x30,4))[0],
                event=bytes(m.mem_read(buffer,0x38)).hex(),
                event_words=bytes(m.mem_read(struct.unpack('<I',m.mem_read(buffer+8,4))[0],32)).hex() if real and struct.unpack('<I',m.mem_read(buffer+8,4))[0] else None)


def collision_cases():
    yield {'other':False}
    for gate in ('query','member','member_body','source_component'):
        yield {'other':False,gate:False}
        yield {gate:False}
    yield {'source':False}
    for source_state in (-1,0,1,256,257):
        for other_state in (-1,0,1,256,257):
            for other_component in (False,True):
                for record in (False,True):
                    yield dict(source_actor=source_state,other_actor=other_state,
                               other_component=other_component,record=record)
    for event_interface,allocate,mutate in itertools.product((False,True),repeat=3):
        yield dict(event_interface=event_interface,allocate=allocate,mutate=mutate)
    for state in (0,1,2,255,256,257,0x12345601,0xffffffff):
        yield {'other':False,'notify_state':state}
    for state,selector,mutate in itertools.product((0,1,2,255,256,257,0x12345601,0xffffffff),
                                                   (0,1,0x12345678,0xffffffff),(False,True)):
        yield dict(notify_only=True,notify_state=state,selector=selector,mutate=mutate)

    for words in (0,3):
        yield dict(real_events=True,event_words=words)

    yield dict(real_events=True,event_words=3,real_vector=True)
