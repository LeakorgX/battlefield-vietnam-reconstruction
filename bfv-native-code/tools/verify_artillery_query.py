"""Instruction comparisons for the first-pass query gate and four helpers.

Virtual methods, protected vector growth and raw free are controlled services.
Original/source field reads, arithmetic, transform and cleanup execute.
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
from unicorn.x86_const import (UC_X86_REG_EAX, UC_X86_REG_EBX, UC_X86_REG_ECX,
    UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP, UC_X86_REG_ESP, UC_X86_REG_FPCW)
from build import PROJECT, GAME, TARGETS
from native_oracle import load_machine, ARENA, STACK
from verify_constant_returns import FLOAT_STATE
from verify_movement_score import bits, state_result


def setup(image, pe, case):
    m = load_machine(image, pe)
    frame, seed = STACK + 0x8000, ARENA + 0x9400
    m.mem_write(ARENA, b'\xa5' * 0x4000)
    m.mem_write(frame, b'\xa5' * 0x220)
    def w(p, v): m.mem_write(p, struct.pack('<I', v & 0xffffffff))
    def r(p): return struct.unpack('<I', m.mem_read(p, 4))[0]
    def words(p, v): m.mem_write(p, struct.pack('<' + 'I' * len(v), *v))
    m.reg_write(UC_X86_REG_FPCW, case.get('cw', 0x37f))
    depth = case.get('depth', 0)
    for i in range(depth): w(seed + 4*i, [bits(1.234), bits(-5.678)][i % 2])
    preload = b''.join(b'\xd9\x05' + struct.pack('<I', seed + 4*i) for i in range(depth))
    return m, frame, w, r, words, preload


def run_helper(image, pe, spec, case):
    m, frame, w, r, words, preload = setup(image, pe, case)
    component, owner, receiver, vt = [ARENA + n for n in (0x100, 0x200, 0x300, 0x400)]
    event, data, alternate, buffer = [ARENA + n for n in (0x500, 0x600, 0x700, 0x1000)]
    convert_obj, converted = ARENA + 0x800, ARENA + 0xa00
    stubs = {name: ARENA + 0x8000 + i*0x100 for i, name in enumerate(['notify', 'object', 'convert', 'data'])}
    for name, address in stubs.items(): m.mem_write(address, b'\xc2\x04\x00' if name in ['notify', 'convert'] else b'\xc3')
    m.mem_write(spec['artillery_query_vector_insert'], b'\xc2\x0c\x00')
    m.mem_write(spec['vector_free'], b'\xc3')
    w(component + 4, owner); w(owner + 0x20, receiver); w(receiver, vt)
    w(vt + 0xa0, stubs['notify']); w(event + 0x14, data); w(data + 4, case.get('value', 0x7f812345))
    w(alternate + 4, 0xdeadbeef)
    w(component, receiver); w(vt + 0x34, stubs['object'])
    w(convert_obj, convert_obj + 0x80); w(convert_obj + 0x80 + 0xc, stubs['convert'])
    w(converted, converted + 0x80); w(converted + 0x80 + 0x3c, stubs['data'])
    w(spec['artillery_query_default_data'], data)
    w(spec['artillery_query_interface_id'], 0x12345678)
    vector, value = ARENA + 0xc00, ARENA + 0xd00
    layout = case.get('layout', 'separate')
    if layout == 'field': value = vector + 8
    elif layout == 'destination': value = buffer + 4
    elif layout == 'unaligned': vector += 1; value += 3; buffer += 1
    kind = case['kind']
    begin = 0 if case.get('empty') else buffer
    end = buffer + case.get('size', 1)*4
    capacity = buffer + case.get('capacity', 4)*4
    if begin == 0: end = capacity = 0
    if case.get('invalid_size'): end = buffer - 4
    words(vector, [0x13579bdf, begin, end, capacity])
    w(value, case.get('value', 0xabcdef01))
    this = vector if kind.startswith('query_vector') else component
    args = [value] if kind == 'query_vector_append' else []
    entry, start, stop = spec[kind], ARENA + 0x9000, ARENA + 0x9200
    words(frame, [stop, *args]); m.mem_write(stop, b'\x90')
    saved = {reg: 0x12340000+i for i, reg in enumerate([UC_X86_REG_EBX, UC_X86_REG_ESI, UC_X86_REG_EDI, UC_X86_REG_EBP])}
    for reg, val in {**saved, UC_X86_REG_ECX: this, UC_X86_REG_ESP: frame}.items(): m.reg_write(reg, val)
    m.mem_write(start, preload + b'\xe9' + struct.pack('<I', (entry-start-len(preload)-5) & 0xffffffff))
    before, calls, returned = [], [], []
    def hook(uc, address, size, unused):
        if address == entry: before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address == stop: returned.append(True); uc.emu_stop(); return
        ecx, sp = uc.reg_read(UC_X86_REG_ECX), uc.reg_read(UC_X86_REG_ESP)
        mutation = case.get('mutation')
        if address == stubs['notify']:
            assert ecx == receiver and r(sp+4) == 2
            calls.append('event_2'); uc.reg_write(UC_X86_REG_EAX, event)
            if mutation == 'data': w(event+0x14, alternate)
            if mutation == 'owner': w(component+4, 0); w(owner+0x20, 0)
        elif address == stubs['object']:
            assert ecx == receiver; calls.append('object')
            uc.reg_write(UC_X86_REG_EAX, 0 if case.get('fallback') else convert_obj)
            if mutation == 'global': w(spec['artillery_query_interface_id'], 0xfedcba98); w(spec['artillery_query_default_data'], alternate)
            if mutation == 'component': w(component, 0)
        elif address == stubs['convert']:
            assert ecx == convert_obj and r(sp+4) == r(spec['artillery_query_interface_id'])
            calls.append(('convert', r(sp+4))); uc.reg_write(UC_X86_REG_EAX, converted)
            if mutation == 'converted': uc.reg_write(UC_X86_REG_EAX, receiver); w(vt+0x3c, stubs['data'])
        elif address == stubs['data']:
            assert ecx in [receiver, converted]; calls.append(('data', ecx))
            uc.reg_write(UC_X86_REG_EAX, alternate if mutation == 'result' else data)
        elif address == spec['artillery_query_vector_insert']:
            assert ecx == vector and [r(sp+4+i*4) for i in range(3)] == [r(vector+8), 1, value]
            calls.append(('insert', r(value), r(vector+8)))
            w(buffer+0x80, r(value)); words(vector+4, [buffer+0x80, buffer+0x84, buffer+0x90])
        elif address == spec['vector_free']:
            assert r(sp+4) == begin; calls.append(('free', begin))
            if mutation == 'free': words(vector+4, [0x11223344, 0x55667788, 0x99aabbcc])
    m.hook_add(UC_HOOK_CODE, hook); m.emu_start(start, 0, timeout=2_000_000, count=100000)
    assert returned and m.reg_read(UC_X86_REG_ESP) == frame + 4 + 4*len(args)
    for reg, val in saved.items(): assert m.reg_read(reg) == val
    result = dict(memory=bytes(m.mem_read(ARENA, 0x4000)).hex(), calls=calls, state=state_result(m, before),
                  globals=[r(spec['artillery_query_interface_id']), r(spec['artillery_query_default_data'])])
    if not kind.startswith('query_vector'): result['return'] = m.reg_read(UC_X86_REG_EAX)
    return result


def run_gate(image, pe, spec, case):
    m, frame, w, r, words, preload = setup(image, pe, case)
    bot, target, driver, movement, component = [ARENA + n for n in (0, 0x200, 0x400, 0x600, 0x800)]
    receiver, alt_receiver, service, alt_service = [ARENA+n for n in (0xa00, 0xc00, 0xe00, 0x1000)]
    data, first, second, matrix, record, buffer, source = [ARENA+n for n in (0x1200, 0x1400, 0x1500, 0x1600, 0x1800, 0x1900, 0x1a00)]
    conversion, converted, event = ARENA+0x1c00, ARENA+0x1e00, ARENA+0x2000
    names = ['position', 'matrix', 'identity', 'component', 'object', 'convert', 'data', 'notify', 'predicate', 'predict', 'query']
    stubs = {name: ARENA+0x7000+i*0x100 for i, name in enumerate(names)}
    for name, address in stubs.items():
        cleanup = {'convert': 4, 'notify': 4, 'predicate': 8, 'predict': 8, 'query': 40}.get(name, 0)
        m.mem_write(address, b'\xc2'+struct.pack('<H', cleanup) if cleanup else b'\xc3')
    def obj(p, slots):
        w(p, p+0x80)
        for offset, name in slots.items(): w(p+0x80+offset, stubs[name])
    obj(bot, {0xb0: 'component'}); obj(target, {0x18: 'position', 0x24: 'matrix', 0x2c: 'identity'})
    obj(movement, {0x28: 'predict'}); obj(receiver, {0x84: 'predicate'}); obj(alt_receiver, {})
    obj(service, {0x50: 'query'}); obj(alt_service, {})
    obj(component, {0x34: 'object'}); w(component, component+0x40); obj(component+0x40, {0x34:'object'})
    obj(conversion, {0xc:'convert'}); obj(converted, {0x3c:'data'})
    w(driver+4, driver+0x40); w(driver+0x40+0x20, driver+0x80); obj(driver+0x80, {0xa0:'notify'})
    w(event+0x14, data); w(data+4, case.get('event_word', 0x7f812345))
    w(source+0x20, 0xabcdef01); w(target+0x20, 0xfedcba98)
    w(spec['artillery_query_default_data'], data); w(spec['artillery_query_interface_id'], 0x2468ace0)
    w(frame+0x1c, case.get('parameter', bits(100))); w(frame+0x28, case.get('distance', bits(50)))
    w(frame+0x1fc, driver if case.get('driver') else 0)
    w(frame+0xf8, receiver); w(frame+0x24, service); w(frame+0x1f8, source); w(frame+0x18, record)
    w(frame+0x58, case.get('reference', bits(1.25)))
    values = case.get('point', [bits(1.2345), bits(-2.3456), bits(.345678)])
    words(first, values); words(second, case.get('second_point', [bits(4), bits(5), bits(6)])); words(record+4, values)
    words(data+0x30, case.get('origin', [bits(.5), bits(1.5), bits(2.5)]))
    matrix_values = [bits(v) for v in [1.1,.2,.3,0,.4,1.5,.6,0,.7,.8,1.9,0,2,3,4,1]]
    words(matrix, case.get('matrix', matrix_values))
    layout = case.get('layout')
    if layout == 'far_pair': first=frame+0x134; second=frame+0x13c; words(first, values); words(second, values)
    if layout == 'origin': data=frame+0x11c; words(data+0x30, case.get('origin', values)); w(spec['artillery_query_default_data'], data)
    if layout == 'record': record=frame+0xdc; w(frame+0x18, record); words(record+4, values)
    if layout == 'actual': first=frame+0xe0
    if layout == 'matrix': matrix=frame+0xb0; words(matrix, matrix_values)
    m.mem_write(spec['artillery_query_vector_insert'], b'\xc2\x0c\x00'); m.mem_write(spec['vector_free'], b'\xc3')
    movement_value = movement if case.get('movement', True) else 0
    saved = {UC_X86_REG_EBX:0x12345678, UC_X86_REG_ESI:bot, UC_X86_REG_EBP:target, UC_X86_REG_ESP:frame}
    for reg, val in {**saved, UC_X86_REG_EDI:movement_value}.items(): m.reg_write(reg, val)
    entry, start = spec['artillery_query_gate'], ARENA+0x9000
    m.mem_write(start, preload+b'\xe9'+struct.pack('<I', (entry-start-len(preload)-5)&0xffffffff))
    before, calls, exits = [], [], []
    def hook(uc, address, size, unused):
        if address == entry: before.extend(uc.reg_read(reg) for reg in FLOAT_STATE)
        if address in [spec['artillery_query_accept'], spec['artillery_filter_reject']]:
            exits.append('accept' if address == spec['artillery_query_accept'] else 'reject'); uc.emu_stop(); return
        ecx, sp = uc.reg_read(UC_X86_REG_ECX), uc.reg_read(UC_X86_REG_ESP)
        mutation = case.get('mutation')
        def args(n): return [r(sp+4+i*4) for i in range(n)]
        if address == stubs['position']:
            assert ecx == target; count=sum(c=='position' for c in calls); calls.append('position')
            uc.reg_write(UC_X86_REG_EAX, second if count else first)
            if mutation == 'position' and count: words(first, [bits(-4), bits(-5), bits(-6)])
            if mutation == 'predicate_receiver': w(frame+0xf8, alt_receiver); w(alt_receiver, receiver+0x80)
        elif address == stubs['component']:
            assert ecx == bot; calls.append('component'); uc.reg_write(UC_X86_REG_EAX, component)
        elif address == stubs['object']:
            assert ecx == component+0x40; calls.append('object'); uc.reg_write(UC_X86_REG_EAX, 0 if case.get('fallback') else conversion)
        elif address == stubs['convert']:
            assert ecx == conversion and args(1)==[0x2468ace0]; calls.append('convert'); uc.reg_write(UC_X86_REG_EAX, converted)
        elif address == stubs['data']:
            assert ecx == converted; calls.append('data'); uc.reg_write(UC_X86_REG_EAX, data)
        elif address == stubs['notify']:
            assert ecx == driver+0x80 and args(1)==[2]; calls.append('event_2'); uc.reg_write(UC_X86_REG_EAX, event)
            if mutation == 'event': w(data+4, 0xdeadbeef); w(frame+0xf8, alt_receiver); w(alt_receiver, alt_receiver+0x80)
        elif address == stubs['predicate']:
            assert ecx == (alt_receiver if mutation in ['event','predicate_receiver'] else receiver)
            assert args(2)==[r(data+4),frame+0x138]; calls.append(('predicate', args(2)))
            uc.reg_write(UC_X86_REG_EAX, case.get('result', 0))
        elif address == spec['artillery_query_vector_insert']:
            assert ecx==frame+0xfc and args(3)==[r(frame+0x104),1,frame+0x30]
            count=sum(isinstance(c,tuple) and c[0]=='insert' for c in calls)
            calls.append(('insert', r(frame+0x30))); w(buffer+4*count,r(frame+0x30))
            words(frame+0x100,[buffer,buffer+4*(count+1),buffer+4 if case.get('grow_twice') else buffer+8])
            if mutation == 'insert': w(target+0x20,0x01020304)
        elif address == stubs['matrix']:
            assert ecx==target; calls.append('matrix'); uc.reg_write(UC_X86_REG_EAX,matrix)
            if mutation == 'record': w(frame+0x18,second); words(record+4,[bits(-1),bits(-2),bits(-3)])
        elif address == stubs['predict']:
            assert ecx==movement and args(2)==[r(frame+0x58),frame+0x198]
            calls.append(('predict',args(2))); words(frame+0x198,case.get('predicted',[bits(10),bits(20),bits(30)]))
            if mutation == 'prediction': words(first,[bits(-1),bits(-2),bits(-3)]); w(frame+0x30,0)
        elif address == stubs['identity']:
            assert ecx==target; calls.append('identity'); uc.reg_write(UC_X86_REG_EAX,case.get('identity',0x13579bdf))
            if mutation == 'service': w(frame+0x24,alt_service)
        elif address == stubs['query']:
            assert ecx==(alt_service if mutation=='service' else service)
            assert args(10)==[frame+0x150,frame+0xe0,frame+0xfc,0,0,1,1,case.get('identity',0x13579bdf),1,0]
            assert r(frame+0x100)==buffer and r(frame+0x104)==buffer+8
            calls.append(('query',args(10))); uc.reg_write(UC_X86_REG_EAX,case.get('result',0))
            if mutation == 'query': words(frame+0xe0,[bits(-4),bits(-5),bits(-6)])
        elif address == spec['vector_free']:
            assert args(1)==[buffer]; calls.append('free')
            if mutation == 'free': words(frame+0x100,[0x12345678,0xabcdef01,0xfedcba98]); uc.reg_write(UC_X86_REG_EAX,~case.get('result',0)&0xffffffff)
    m.hook_add(UC_HOOK_CODE,hook); m.emu_start(start,0,timeout=2_000_000,count=100000)
    assert len(exits)==1
    for reg,val in saved.items(): assert m.reg_read(reg)==val
    if any(isinstance(c,tuple) and c[0]=='query' for c in calls):
        assert calls.count('free')==1 and [r(frame+n) for n in (0x100,0x104,0x108)]==[0,0,0]
    return dict(frame=bytes(m.mem_read(frame,0x220)).hex(),memory=bytes(m.mem_read(ARENA,0x4000)).hex(),
                calls=calls,exit=exits[0],edi=m.reg_read(UC_X86_REG_EDI),state=state_result(m,before))


def helper_cases():
    patterns=[0,1,0x80000000,0xffffffff,0x7f812345,0x12345678]
    cases=[dict(kind='component_event2_word',value=v) for v in patterns]
    cases += [dict(kind='component_event2_word',mutation=x) for x in ['data','owner']]
    cases += [dict(kind='component_query_data',fallback=f,mutation=x) for f,x in itertools.product([False,True],[None,'global','component','converted','result'])]
    cases += [dict(kind='query_vector_append',value=v,layout=l) for v,l in itertools.product(patterns,['separate','field','destination','unaligned'])]
    cases += [dict(kind='query_vector_append',**c) for c in [dict(empty=True),dict(size=4),dict(size=8),dict(invalid_size=True),dict(capacity=0),dict(capacity=-1),dict(size=0)]]
    cases += [dict(kind='query_vector_destroy',empty=e,mutation=x) for e,x in itertools.product([False,True],[None,'free'])]
    selected=[dict(kind='component_event2_word'),dict(kind='component_query_data'),dict(kind='component_query_data',fallback=True),dict(kind='query_vector_append'),dict(kind='query_vector_append',empty=True),dict(kind='query_vector_destroy')]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,3,5])]
    return cases


def gate_cases():
    cases=[{},dict(driver=True),dict(driver=True,distance=bits(100)),dict(distance=bits(100)),dict(movement=False),dict(fallback=True),dict(grow_twice=True)]
    patterns=[0,0x80000000,1,0x80000001,bits(-100),bits(89)-1,bits(89),bits(89)+1,0x7f7fffff,0x7f800000,0xff800000,0x7fc12345,0x7f812345]
    cases += [dict(**{field:v},driver=d) for field,v,d in itertools.product(['parameter','distance'],patterns,[False,True])]
    cases += [dict(c,result=v) for c,v in itertools.product([{},dict(driver=True,distance=bits(100))],[0,1,0x100,0x101,0xff,0xffffffff])]
    cases += [dict(mutation=x) for x in ['insert','record','prediction','service','query','free']]
    cases += [dict(driver=True,distance=bits(100),mutation=x) for x in ['position','predicate_receiver','event']]
    cases += [dict(layout=x) for x in ['origin','record','actual','matrix']]
    cases += [dict(driver=True,distance=bits(100),layout='far_pair')]
    cases += [dict(**{field:[v,0x7fc23456,1]}) for field,v in itertools.product(['point','origin','predicted'],patterns)]
    cases += [dict(driver=True,distance=bits(100),**{field:[v,0x7fc23456,0x7f812346]}) for field,v in itertools.product(['point','second_point'],patterns)]
    selected=[{},dict(movement=False),dict(driver=True),dict(driver=True,distance=bits(100)),dict(driver=True,distance=0x7f812345),dict(point=[0x7f812345,1,0]),dict(predicted=[0x7f812345,1,0]),dict(layout='actual')]
    cases += [dict(c,cw=0x7f|pc|rc,depth=d) for c,pc,rc,d in itertools.product(selected,[0,0x200,0x300],[0,0x400,0x800,0xc00],[0,2,3])]
    rng=random.Random(0x99fe3b)
    cases += [dict(parameter=bits(rng.uniform(-100,200)),distance=bits(rng.uniform(-100,200)),driver=bool(i%2),point=[rng.getrandbits(32) for _ in range(3)],predicted=[rng.getrandbits(32) for _ in range(3)]) for i in range(24)]
    return cases


ENTRIES=[('component_event2_word','bfv_component_event2_word'),('component_query_data','bfv_component_query_data'),
         ('query_vector_append','bfv_query_vector_append'),('query_vector_destroy','bfv_query_vector_destroy'),
         ('artillery_query_gate','bfv_artillery_query_gate_bridge')]


def compare_artillery_query(original,original_pe,edited,edited_pe,spec,symbols):
    for key,symbol in ENTRIES:
        entry=spec[key];jump=edited_pe.get_data(entry-0x400000,5)
        assert jump[0]==0xe9 and entry+5+struct.unpack('<i',jump[1:])[0]==symbols[symbol]
    results=[]
    for group,runner,cases in [('helper',run_helper,helper_cases()),('gate',run_gate,gate_cases())]:
        for index,case in enumerate(cases):
            try:
                old=runner(original,original_pe,spec,case);new=runner(edited,edited_pe,spec,case)
                assert old==new,(case,{k:(old[k],new[k]) for k in old if old[k]!=new[k]})
            except Exception as error: raise RuntimeError(f'query {group} case {index}: {case}') from error
            results.append(dict(group=group,inputs=case,calls=old['calls'],exit=old.get('exit')))
    return results


if __name__=='__main__':
    p=argparse.ArgumentParser();p.add_argument('--game-dir',type=Path,default=GAME)
    p.add_argument('--target',choices=['client','server','both'],default='both');a=p.parse_args()
    for target in ['client','server'] if a.target=='both' else [a.target]:
        spec=TARGETS[target];work=PROJECT/'build'/target;manifest=json.loads((work/'manifest.json').read_text(encoding='utf-8'))
        original=(a.game_dir/spec['file']).read_bytes();edited=Path(manifest['output']).read_bytes()
        assert hashlib.sha256(original).hexdigest()==spec['sha']
        assert hashlib.sha256(edited).hexdigest()==manifest['output_sha256']
        cases=compare_artillery_query(original,pefile.PE(data=original),edited,pefile.PE(data=edited),spec,
             {k:int(v,16) for k,v in manifest['symbols'].items()})
        (work/'artillery-query-verification.json').write_text(json.dumps(dict(target=target,original_sha256=spec['sha'],
            compiled_sha256=manifest['output_sha256'],passed=len(cases),cases=cases),indent=2)+'\n',encoding='utf-8')
        print(f'{target}: {len(cases)} query gate/helper comparisons passed',flush=True)
