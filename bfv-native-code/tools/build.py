"""Compile editable native AI and append it to an original, hash-checked PE copy."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import struct
import subprocess
import pefile

PROJECT = Path(__file__).resolve().parents[1]
GAME = Path(os.environ.get('BFV_GAME_DIR', PROJECT.parent)).resolve()
COMPILERS = Path(os.environ.get('MINGW32_BIN', 'C:/msys64/mingw32/bin'))
TARGETS = {
    'client': dict(file='BfVietnam.exe', output='BfVietnam-editable.exe',
        sha='79655e9c2bb92fb24f6daef05566b25633da8cad19d2c95165218a01e17a06a5',
        list_find_word=0x9bf7c0,
        artillery_category_score=0x9a007a,
        artillery_category_alternate=0x9a01f9,
        artillery_category_scale=0xb5d484,
        target_handle_eligible=0x97ede0,
        artillery_alternate_gate=0x9a01f9,
        artillery_alternate_accept=0x9a0264,
        artillery_nested_finish=0x9a03cc,
        artillery_next_target=0x963a20,
        artillery_lookup_target=0x926af0,
        artillery_score_limit=0x956100,
        artillery_limit_scale=0xb44e4c,
        artillery_after_first_pass=0x9a0493,
        artillery_second_filter=0x9a04f0,
        artillery_second_accept=0x9a05d5, artillery_second_reject=0x9a0f5e,
        artillery_second_weight_continue=0x9a062d, artillery_second_weight_scale=0xb92378,
        vector_normalize=0x4a2a10, normalize_tolerance=0xb55588,
        artillery_second_aim_accept=0x9a06ac,
        artillery_second_position_continue=0x9a06f6,
        artillery_second_distance_continue=0x9a078e,
        artillery_second_weapons_accept=0x9a08b8,
        artillery_second_movement_continue=0x9a0981, artillery_second_movement_score=0x9a0bc2, artillery_second_movement_unit_score=0x9a0bba,
        artillery_second_driver_gate=0x9a0bc9, artillery_second_driver_accept=0x9a0c38, artillery_second_region=0x9a0c38, artillery_second_region_continue=0x9a0c7e,
        artillery_second_category_gate=0x9a0c7e, artillery_second_category_accept=0x9a0ca6, artillery_second_category_reject=0x9a0f5e,
        artillery_second_category_prefix=0x9a0ca6, artillery_second_category_factor=0x9a0cf0, artillery_second_category_factor_continue=0x9a0dba,
        artillery_second_category_score=0x9a0dba, artillery_second_category_score_continue=0x9a0f5e,
        artillery_second_iterator=0x9a0f5e, artillery_second_iterator_done=0x9a0f78,
        artillery_final_weights=0x9a0f8c, artillery_final_weights_continue=0x9a1001,
        artillery_parameter_manager=0xe0eb70,
        artillery_final_selection=0x9a1001, artillery_final_selection_bias=0xb95668,
        artillery_final_curve=0x9d6540,
        artillery_final_curve_table=0xe0f670,
        artillery_search_prefix=0x99f3cf, artillery_search_prefix_continue=0x99f6c3,
        artillery_candidate_setup=0x99f7ab, artillery_candidate_setup_continue=0x99f8d7,
        artillery_prefix_refresh_threshold=0xb7628c, artillery_prefix_context=0xe0ef54,
        artillery_prefix_query_manager=0xdfa8cc, artillery_prefix_query_seed=0xe0ee08,
        artillery_prefix_region_registry=0xe0edc0, artillery_prefix_tree_erase=0x5f2dc0,
        artillery_prefix_tree_advance=0x517e10,
        artillery_driver_reload=0x9a0bc2, artillery_allocation_reload=0x9a0f78,
        artillery_second_empty=0x9a0f7f,
        artillery_second_skip=0x9a0f8c,
        point_xz=0x97f070,
        point_in_symmetric_bounds=0x9d5cc0,
        region_contains_point2=0x964ac0,
        region_contains_point3=0x964b70,
        artillery_region_score=0x9a0031,
        artillery_region_continue=0x9a007a,
        artillery_region_scale=0xb55584,
        artillery_query_gate=0x99fe3b,
        artillery_query_accept=0x9a0031,
        artillery_query_scale=0xbf6328,
        component_event2_word=0x92db30,
        component_query_data=0x970440,
        artillery_query_default_data=0xe0ee60,
        artillery_query_interface_id=0xb77da0,
        query_vector_append=0x975ab0,
        query_vector_destroy=0x490610,
        artillery_query_vector_insert=0x972940,
        artillery_movement_gate=0x99fb31, artillery_movement_continue=0x99fc01,
        artillery_movement_scoring=0x99fc01, artillery_movement_delta_limit=0xb5bdf8, artillery_movement_quarter=0xb55318,
        vector_cross_assign=0x6eb030, component_event2_scalar=0x970550,
        artillery_movement_score=0x99fe3b, artillery_movement_unit_score=0x99fe33,
        artillery_movement_scale=0xb44e48, artillery_movement_alt_scale=0xb69400, artillery_movement_threshold=0xb5dc9c,
        artillery_aim_gate=0x99fa61, artillery_aim_gate_accept=0x99fb31, artillery_weight_scale=0xb55580,
        transform_point=0x49b340, vector_difference=0x49b490, aim_world_position=0x9be440, component_position=0x994a80,
        artillery_weapons=0x99f8d7, artillery_weapons_accept=0x99fa61, artillery_weapon_bias=0xb5b7e4,
        tree_previous=0x68ee10, tree_rotate_left=0x996f10, tree_rotate_right=0x66b6c0, tree_construct=0x517c10, history_insert_node=0x99ed10, tree_allocate_node=0x99ebe0, tree_string_assign=0x401ae0, tree_string_copy=0x4019f0, tree_exception_construct=0x7c2027, tree_logic_error_vtable=0xb427dc, tree_length_error_vtable=0xb427f4, tree_length_throw_info=0xc836a4, tree_length_message=0xb44270,
        aim_direction=0x99eae0, aim_within_limits=0x9be880, aim_compose=0x49acd0,
        aim_inverse_sine=0x516f80, aim_negative_one=0xb5456c, aim_pi=0xbf7e90,
        float_minimum=0x422f30, float_maximum=0x422f90, float_clamp=0x4a94a0,
        vector_length=0x49aac0, vector_divide=0x5184c0, math_one=0xb4456c,
        pool=0xe0cd58, trace_index=0xdfa918, trace_mask=0xd1a088, trace_files=0xdfa8f8,
        trace_lines=0xdfa8d8, trace_source=0xbf7e38, context_begin=0x9dce60,
        context_end=0x9dcc10, context_record=0xa64d40, native_bailout=0x984c20,
        native_vehicle=0x9856f0, artillery=0x9a13a0, artillery_score=0x99f2a0, target_history=0x99f040, target_history_insert=0x99ef80, artillery_filter=0x99f6c3, artillery_filter_accept=0x99f7ab, artillery_filter_reject=0x9a0476, artillery_filter_zero=0xb44444, artillery_filter_history_limit=0xbf62e8, artillery_component_view=0x49a980, interpreter=0x9be180,
        bailout_curve=0x9d7200, bailout_score=0x9d6b90,
        bailout_curve_table=0xe0f678, bailout_score_table=0xe0f674, bailout_pattern=0x983280,
        bailout_curve_initializer=0x9d6c20, bailout_score_initializer=0x9d65b0, curve_allocator=0x403610,
        collision=0x9d4ba0, collision_body_view=0x66b190, collision_interface_id=0xb77ee0, collision_handle_field=0x164,
        collision_registry=0xd7d01c, collision_actors=0xdfa8c8, collision_clock=0xe0ef50,
        collision_pool_entry=0x926af0, collision_event_interface=0x92afb0,
        collision_allocate=0x412ee0, collision_allocator=0xe0f7c0, collision_alloc_source=0xb44284,
        vector_allocator=0x403610, vector_free=0x403680, vector_length_error=0xa894e0,
        vector_copy=0xa6ad10, vector_fill=0x4ef090, vector_shift=0xa03770, vector_fill_range=0x6f7000,
        vector_memmove=0x7c16f0, vector_frame_handler=0x7c1b6e, vector_throw=0x7c1fae, vector_original_handler=0xac4a20,
        collision_event_manager=0xe0eb20, collision_vector_insert=0x428200, collision_construct=0x9dc3b0, collision_notify=0x9d4b60,
        collision_dispatch=0x9d48b0, collision_notify_state_field=0x1ec,
        collision_line_distance=0x9d4640, collision_distance=0x9d46d0, collision_distance_limit=0xb76560,
        patches=[(0xbf7e34,0x9be180,'bfv_interpret'),(0xbf5a18,0x984c20,'bfv_bailout'),(0xbf5ab8,0x9856f0,'bfv_vehicle'),(0xbf8c64,0x9d4ba0,'bfv_collision'),(0xbf6378,0x9a13a0,'bfv_artillery')]),
    'server': dict(file='bfvietnam_w32ded.exe', output='bfvietnam_w32ded-editable.exe',
        sha='86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d',
        list_find_word=0x775bf0,
        artillery_category_score=0x74a85a,
        artillery_category_alternate=0x74a9d9,
        artillery_category_scale=0x81df08,
        target_handle_eligible=0x728c90,
        artillery_alternate_gate=0x74a9d9,
        artillery_alternate_accept=0x74aa44,
        artillery_nested_finish=0x74abac,
        artillery_next_target=0x71b110,
        artillery_lookup_target=0x6e8930,
        artillery_score_limit=0x713230,
        artillery_limit_scale=0x873bf0,
        artillery_after_first_pass=0x74ac73,
        artillery_second_filter=0x74acd0,
        artillery_second_accept=0x74adb5, artillery_second_reject=0x74b73e,
        artillery_second_weight_continue=0x74ae0d, artillery_second_weight_scale=0x8372b4,
        vector_normalize=0x438d30, normalize_tolerance=0x80b630,
        artillery_second_aim_accept=0x74ae8c,
        artillery_second_position_continue=0x74aed6,
        artillery_second_distance_continue=0x74af6e,
        artillery_second_weapons_accept=0x74b098,
        artillery_second_movement_continue=0x74b161, artillery_second_movement_score=0x74b3a2, artillery_second_movement_unit_score=0x74b39a,
        artillery_second_driver_gate=0x74b3a9, artillery_second_driver_accept=0x74b418, artillery_second_region=0x74b418, artillery_second_region_continue=0x74b45e,
        artillery_second_category_gate=0x74b45e, artillery_second_category_accept=0x74b486, artillery_second_category_reject=0x74b73e,
        artillery_second_category_prefix=0x74b486, artillery_second_category_factor=0x74b4d0, artillery_second_category_factor_continue=0x74b59a,
        artillery_second_category_score=0x74b59a, artillery_second_category_score_continue=0x74b73e,
        artillery_second_iterator=0x74b73e, artillery_second_iterator_done=0x74b758,
        artillery_final_weights=0x74b76c, artillery_final_weights_continue=0x74b7e1,
        artillery_parameter_manager=0xc31178,
        artillery_final_selection=0x74b7e1, artillery_final_selection_bias=0x839ac0,
        artillery_final_curve=0x78b930,
        artillery_final_curve_table=0xc31c08,
        artillery_search_prefix=0x749baf, artillery_search_prefix_continue=0x749ea3,
        artillery_candidate_setup=0x749f8b, artillery_candidate_setup_continue=0x74a0b7,
        artillery_prefix_refresh_threshold=0x81e9cc, artillery_prefix_context=0xc314b4,
        artillery_prefix_query_manager=0xc1d338, artillery_prefix_query_seed=0xc313d0,
        artillery_prefix_region_registry=0xc31388, artillery_prefix_tree_erase=0x5c7980,
        artillery_prefix_tree_advance=0x718e10,
        artillery_driver_reload=0x74b3a2, artillery_allocation_reload=0x74b758,
        artillery_second_empty=0x74b75f,
        artillery_second_skip=0x74b76c,
        point_xz=0x728f20,
        point_in_symmetric_bounds=0x78cd10,
        region_contains_point2=0x71c280,
        region_contains_point3=0x71c330,
        artillery_region_score=0x74a811,
        artillery_region_continue=0x74a85a,
        artillery_region_scale=0x81fbf4,
        artillery_query_gate=0x74a61b,
        artillery_query_accept=0x74a811,
        artillery_query_scale=0x874728,
        component_event2_word=0x6eb4b0,
        component_query_data=0x727d50,
        artillery_query_default_data=0xc31428,
        artillery_query_interface_id=0x820658,
        query_vector_append=0x72d870,
        query_vector_destroy=0x6ff4f0,
        artillery_query_vector_insert=0x72d640,
        artillery_movement_gate=0x74a311, artillery_movement_continue=0x74a3e1,
        artillery_movement_scoring=0x74a3e1, artillery_movement_delta_limit=0x8220c8, artillery_movement_quarter=0x81fb68,
        vector_cross_assign=0x611430, component_event2_scalar=0x72e7d0,
        artillery_movement_score=0x74a61b, artillery_movement_unit_score=0x74a613,
        artillery_movement_scale=0x8148e0, artillery_movement_alt_scale=0x8377b4, artillery_movement_threshold=0x814d1c,
        artillery_aim_gate=0x74a241, artillery_aim_gate_accept=0x74a311, artillery_weight_scale=0x81fac0,
        transform_point=0x5b25f0, vector_difference=0x48d160, aim_world_position=0x78df90, component_position=0x73f1e0,
        artillery_weapons=0x74a0b7, artillery_weapons_accept=0x74a241, artillery_weapon_bias=0x81fac4,
        tree_previous=0x73f240, tree_rotate_left=0x5c7460, tree_rotate_right=0x55c080, tree_construct=0x7686e0, history_insert_node=0x7494f0, tree_allocate_node=0x7493c0, tree_string_assign=0x402020, tree_string_copy=0x401e20, tree_exception_construct=0x63aa5e, tree_logic_error_vtable=0x80737c, tree_length_error_vtable=0x807388, tree_length_throw_info=0x8ce900, tree_length_message=0x808f70,
        aim_direction=0x7492c0, aim_within_limits=0x78e3d0, aim_compose=0x438590,
        aim_inverse_sine=0x48b570, aim_negative_one=0x817648, aim_pi=0x877770,
        float_minimum=0x5a3340, float_maximum=0x4ac530, float_clamp=0x6f0470,
        vector_length=0x48c2b0, vector_divide=0x48caf0, math_one=0x809264,
        pool=0xc2f7c0, trace_index=0xc1d380, trace_mask=0x934448, trace_files=0xc1d360,
        trace_lines=0xc1d340, trace_source=0x876820, context_begin=0x7b6bd0,
        context_end=0x7b6980, context_record=0x69f0b0, native_bailout=0x72eef0,
        native_vehicle=0x72f9e0, artillery=0x74bb80, artillery_score=0x749a80, target_history=0x749820, target_history_insert=0x749760, artillery_filter=0x749ea3, artillery_filter_accept=0x749f8b, artillery_filter_reject=0x74ac56, artillery_filter_zero=0x851bcc, artillery_filter_history_limit=0x8746e8, artillery_component_view=0x437e80, interpreter=0x774ff0,
        bailout_curve=0x78c5f0, bailout_score=0x78bf80,
        bailout_curve_table=0xc31c10, bailout_score_table=0xc31c0c, bailout_pattern=0x72e510,
        bailout_curve_initializer=0x78c010, bailout_score_initializer=0x78b9a0, curve_allocator=0x403d80,
        collision=0x78a500, collision_body_view=0x564460, collision_interface_id=0x820798, collision_handle_field=0x15c,
        collision_registry=0xbecc64, collision_actors=0xc1d334, collision_clock=0xc314b0,
        collision_pool_entry=0x6e8930, collision_event_interface=0x6e88b0,
        collision_allocate=0x404290, collision_allocator=0xc33388, collision_alloc_source=0x807375,
        vector_allocator=0x403d80, vector_free=0x403df0, vector_length_error=0x48e690,
        vector_copy=0x41fbd0, vector_fill=0x5f83e0, vector_shift=0x7901d0, vector_fill_range=0x705df0,
        vector_memmove=0x63a0d0, vector_frame_handler=0x63a54e, vector_throw=0x63a98e, vector_original_handler=0x7e1080,
        collision_event_manager=0xc3117c, collision_vector_insert=0x4297c0, collision_construct=0x7ae550, collision_notify=0x78a4c0,
        collision_dispatch=0x78a210, collision_notify_state_field=0x1c0,
        collision_line_distance=0x789fa0, collision_distance=0x78a030, collision_distance_limit=0x81facc,
        patches=[(0x87681c,0x774ff0,'bfv_interpret'),(0x873d88,0x72eef0,'bfv_bailout'),(0x873e28,0x72f9e0,'bfv_vehicle'),(0x8775cc,0x78a500,'bfv_collision'),(0x874778,0x74bb80,'bfv_artillery')]),
}


def align(n, size): return (n + size - 1) // size * size


def run(args, cwd):
    result = subprocess.run([str(x) for x in args], cwd=cwd, text=True, capture_output=True)
    if result.returncode:
        raise RuntimeError(f'Command failed: {args}\n{result.stdout}\n{result.stderr}')
    return result.stdout


LIFTED_FLAGS=['-m32','-std=c11','-O2','-Wall','-Wextra','-Werror','-Wno-array-bounds','-ffreestanding','-fno-builtin',
    '-frounding-math','-fno-delete-null-pointer-checks','-fno-stack-protector','-fno-asynchronous-unwind-tables','-fno-unwind-tables','-mgeneral-regs-only']

def lifted_sources(target):
    lifted=PROJECT/'lifted'
    return [lifted/'runtime/l1_runtime.c',*sorted((lifted/target).glob('lifted*.c'))]

def compile_lifted(target,work):
    """Compile the L1 lifted C (all of it; only verified entries get detoured)."""
    from concurrent.futures import ThreadPoolExecutor
    sources=lifted_sources(target);objects=[work/f'l1_{s.stem}.o' for s in sources]
    with ThreadPoolExecutor(6) as pool:
        list(pool.map(lambda so:run([COMPILERS/'gcc.exe',*LIFTED_FLAGS,'-c',so[0],'-o',so[1]],work),zip(sources,objects)))
    return objects

def lifted_entries(target,spec,pe,original,hand_entries):
    """Detours for lifted functions that passed verify_lifted.py on exactly these sources.

    An entry is skipped when hand-written C already owns it, when it is not an
    instruction start of the linear .text decode that static_recomp reassembles
    byte-exact (a misaligned index entry would cut a native instruction in half),
    when anything may enter its first five bytes other than at the entry (a
    labelled branch target, an image dword pointing there, or another lifted
    function's exit), or when its detour would overlap another patch."""
    lifted=PROJECT/'lifted'/target
    if not (lifted/'catalog.json').exists():return [],{}
    report_path=PROJECT/'build'/target/'lifted-verification.json'
    if not report_path.exists():return [],dict(not_verified=1)
    catalog=json.loads((lifted/'catalog.json').read_text())
    report=json.loads(report_path.read_text())
    if catalog['original_sha256']!=spec['sha'] or report['original_sha256']!=spec['sha']:
        raise RuntimeError('Lifted catalog or verification hash differs from the ABI target')
    runtime=hashlib.sha256()  # same hash as verify_lifted.runtime_sha256
    for f in [PROJECT/'lifted/runtime/l1.h',PROJECT/'lifted/runtime/l1_runtime.c']:runtime.update(f.name.encode()+bytes(1)+f.read_bytes())
    if report.get('runtime_sha256')!=runtime.hexdigest():
        raise RuntimeError(f'{target}: lifted runtime changed since verify_lifted.py; rerun it (or remove lifted-verification.json)')
    # only functions whose current text is exactly the verified text
    passed={(r['address'],r['c_sha256']) for r in report['results'] if r['status']=='passed' and r.get('c_sha256')}
    entries=[e for e in catalog['entries'] if (e['address'],e.get('c_sha256')) in passed]
    interior={}
    for e in entries:
        a=int(e['address'],16)
        for x in range(a+1,a+5):interior[x]=e
    blocked=set()
    from capstone import Cs,CS_ARCH_X86,CS_MODE_32  # same linear sweep as static_recomp.TextEmitter.plan
    text=next(x for x in pe.sections if x.Name.rstrip(b'\0')==b'.text')
    code=text.get_data()[:text.Misc_VirtualSize]
    base=pe.OPTIONAL_HEADER.ImageBase+text.VirtualAddress
    # Image pointers: an absolute immediate or displacement field of a .text
    # instruction, a 4-aligned .text dword next to another .text address (jump
    # table), or any 4-aligned dword of another section. Other byte windows that
    # happen to spell an interior address are not pointers.
    starts=set();offset=0;md=Cs(CS_ARCH_X86,CS_MODE_32);md.detail=True
    sources={}  # direct branch target -> source addresses
    # Code pointers land on instruction starts: a pointer-like dword hits an
    # entry only when it names one of the instructions inside its first five
    # bytes (branches and exits are checked against every interior byte).
    pointed={}
    for e in entries:
        a=int(e['address'],16)
        for i in md.disasm(pe.get_data(a-pe.OPTIONAL_HEADER.ImageBase,20),a):
            if i.address>=a+5:break
            if i.address>a:pointed[i.address]=e
    def hit(v):
        if v in pointed:blocked.add(pointed[v]['address'])
    while offset<len(code):
        last=offset
        for i in md.disasm(code[offset:],base+offset):
            starts.add(i.address);last=i.address-base+i.size
            branch=(i.group(1) or i.group(2)) and i.op_str.startswith('0x') and ',' not in i.op_str and ':' not in i.op_str  # direct jump, jcc, loop or call
            if branch:
                sources.setdefault(int(i.op_str,16),[]).append(i.address)
                continue  # its immediate is relative
            for o in (i.disp_offset,i.imm_offset):
                if o and o+4<=i.size:hit(int.from_bytes(i.bytes[o:o+4],'little'))
        offset=last if last>offset else offset+1
    # An entry the linear sweep decodes through (usually right after a jump table)
    # is still a real entry when another indexed function calls or jumps to it
    # directly and nothing branches to the sweep instruction that covers it.
    import bisect
    spans=sorted(tuple(int(y,16) for y in r.split('-')) for e in catalog['entries'] for r in e['ranges']);span_starts=[lo for lo,_ in spans]
    def indexed(x):
        k=bisect.bisect_right(span_starts,x)-1
        return k>=0 and spans[k][0]<=x<spans[k][1]
    def real_entry(e):
        a=int(e['address'],16);own=[tuple(int(y,16) for y in r.split('-')) for r in e['ranges']]
        if any(p in sources for p in range(a-15,a) if p in starts):return False
        return any(indexed(s) and not any(lo<=s<hi for lo,hi in own) for s in sources.get(a,()))
    misaligned={e['address'] for e in entries if int(e['address'],16) not in starts and not real_entry(e)}
    stats_misaligned_entries=sum(1 for e in entries if int(e['address'],16) not in starts)-len(misaligned)
    blocked|=misaligned
    from array import array
    words=array('I',code[:len(code)//4*4]);assert base%4==0
    for k,v in enumerate(words):
        if v in interior and ((k and base<=words[k-1]<base+len(code)) or (k+1<len(words) and base<=words[k+1]<base+len(code))):hit(v)
    for section in pe.sections:
        if section is text:continue
        data=section.get_data()
        for v in set(array('I',data[:len(data)//4*4])).intersection(interior):hit(v)
    # A labelled interior address is harmless when every branch to it is a direct
    # branch inside the same lifted function (the C has its own goto for it) and
    # that function has no indirect jump that could land there natively.
    def own(e,x):
        spans=[tuple(int(y,16) for y in r.split('-')) for r in e['ranges']]
        if not sources.get(x) or not all(any(lo<=s<hi for lo,hi in spans) for s in sources[x]):return False
        for lo,hi in spans:
            for i in md.disasm(code[lo-base:hi-base],lo):
                if i.mnemonic=='jmp' and not i.op_str.startswith('0x'):return False
        return True
    labels_file=PROJECT/'build'/f'{target}-recomp'/'text.s'
    if not labels_file.exists():raise RuntimeError(f'{target}: run recomp.ps1 once first (branch labels are needed for the detour audit)')
    with labels_file.open(encoding='utf-8') as f:
        for line in f:
            if line[:4] in ('loc_','sub_') and line.rstrip().endswith(':'):
                x=int(line[4:12],16)
                if x in interior and not own(interior[x],x):blocked.add(interior[x]['address'])
    for x,e in interior.items():
        if x in sources and not own(e,x):blocked.add(e['address'])
    for e in catalog['entries']:
        for t in e['exits']:
            x=int(t,16)
            if x in interior:blocked.add(interior[x]['address'])
    stats=dict(passed=len(entries),misaligned=len(misaligned),resynced=stats_misaligned_entries,interior_target=len(blocked-misaligned))
    # verify_live_exceptions.py patches the call in the original vector-insert catch
    # funclet, so that funclet stays native
    u32=lambda a:int.from_bytes(pe.get_data(a-pe.OPTIONAL_HEADER.ImageBase,4),'little')
    funclet=u32(u32(u32(u32(spec['vector_original_handler']+1)+16)+16)+12)
    instrumented={e['address'] for e in entries if any(int(r.split('-')[0],16)<=funclet<int(r.split('-')[1],16) for r in e['ranges'])}
    blocked|=instrumented;stats.update(instrumented=len(instrumented))
    # hand-written C wins: skip lifted functions that contain any of its patches
    hand=sorted(location for location,_,_ in hand_entries)
    import bisect
    result=[];skipped_taken=0
    for e in entries:
        a=int(e['address'],16)
        if e['address'] in blocked:continue
        spans=[tuple(int(x,16) for x in r.split('-')) for r in e['ranges']]
        if any(bisect.bisect_left(hand,lo-15)<bisect.bisect_left(hand,hi) for lo,hi in spans):skipped_taken+=1;continue
        prefix=bytes.fromhex(e['guarded_prefix'])
        result.append((a,prefix,e['symbol']))
    stats.update(hand_written=skipped_taken,detoured=len(result))
    return result,stats

def build(target,lifted=True):
    spec = TARGETS[target]
    original = (GAME / spec['file']).read_bytes()
    if hashlib.sha256(original).hexdigest() != spec['sha']:
        raise RuntimeError('Original image hash differs from the verified ABI target')
    pe = pefile.PE(data=original)
    if pe.OPTIONAL_HEADER.ImageBase != 0x400000 or pe.OPTIONAL_HEADER.DllCharacteristics & 0x40:
        raise RuntimeError('Only the inspected fixed-base executable layout is supported')
    work = PROJECT / 'build' / target
    work.mkdir(parents=True, exist_ok=True)
    constants = {('OBJECT_POOL' if key == 'pool' else key.upper()): value
        for key, value in spec.items() if isinstance(value, int)}
    target_flags=f'#define BFV_BUILD_CLIENT {int(target=="client")}\n#define BFV_BUILD_SERVER {int(target=="server")}\n'
    (work / 'target.h').write_text(target_flags+''.join(f'#define BFV_{k} 0x{v:08x}u\n' for k,v in constants.items()))
    os.environ['PATH'] = str(COMPILERS) + os.pathsep + os.environ['PATH']
    priority={'native_ai':0,'mod_rules':1}
    sources = sorted((PROJECT/'src').glob('*.c'),key=lambda p:(priority.get(p.stem,2),p.name))
    objects = []
    for source_path in sources:
        source = source_path.stem
        run([COMPILERS/'gcc.exe','-m32','-std=c11','-O2','-Wall','-Wextra','-Werror',
             '-ffreestanding','-fno-builtin','-frounding-math','-fno-stack-protector','-fno-asynchronous-unwind-tables',
             '-fno-unwind-tables','-I',work,'-c',source_path,'-o',work/f'{source}.o'], work)
        objects.append(work/f'{source}.o')
    if lifted and (PROJECT/'lifted'/target/'catalog.json').exists():objects+=compile_lifted(target,work)
    rva = align(pe.OPTIONAL_HEADER.SizeOfImage, pe.OPTIONAL_HEADER.SectionAlignment)
    address = pe.OPTIONAL_HEADER.ImageBase + rva
    (work/'payload.ld').write_text(f'SECTIONS {{ . = 0x{address:x}; .payload : {{ *(.text*) *(.rdata*) *(.data*) *(.bss*) BYTE(0) }} /DISCARD/ : {{ *(.eh_frame*) *(.comment*) *(.drectve*) }} }}')
    run([COMPILERS/'ld.exe','-T',work/'payload.ld','--image-base','0x400000',
         '--entry','_bfv_interpret','--disable-dynamicbase','--disable-reloc-section',
         '-Map',work/'payload.map','-o',work/'payload.exe',*objects],work)
    run([COMPILERS/'objcopy.exe','-O','binary','--only-section','.payload',
         work/'payload.exe',work/'payload.bin'],work)
    # The map retains final virtual addresses even with a raw binary output.
    import re
    symbols = {}
    for line in (work/'payload.map').read_text().splitlines():
        match = re.match(r'\s*(0x[0-9a-fA-F]+)\s+[_@]?(bfv_[A-Za-z0-9_]+)(?:@\d+)?\s*$',line)
        if match: symbols[match[2]] = int(match[1],16)
    payload = (work/'payload.bin').read_bytes()
    linked = pefile.PE(str(work/'payload.exe'))
    linked_section = next(s for s in linked.sections if s.Name.rstrip(b'\0') == b'.payload')
    if linked.OPTIONAL_HEADER.ImageBase + linked_section.VirtualAddress != address:
        raise RuntimeError('Linked payload address differs from the appended PE section')
    raw = align(len(original), pe.OPTIONAL_HEADER.FileAlignment)
    header_offset = pe.sections[-1].get_file_offset() + 40
    if header_offset + 40 > min(s.PointerToRawData for s in pe.sections):
        raise RuntimeError('No unused PE section-header slot')
    output = bytearray(original)
    output.extend(b'\0'*(raw-len(output)))
    output.extend(payload)
    raw_size = align(len(payload),pe.OPTIONAL_HEADER.FileAlignment)
    output.extend(b'\0'*(raw_size-len(payload)))
    section_header = struct.pack('<8sIIIIIIHHI',b'.bfvmod\0',len(payload),rva,raw_size,raw,0,0,0,0,0xe0000060)
    output[header_offset:header_offset+40] = section_header
    struct.pack_into('<H',output,pe.FILE_HEADER.get_field_absolute_offset('NumberOfSections'),len(pe.sections)+1)
    struct.pack_into('<I',output,pe.OPTIONAL_HEADER.get_field_absolute_offset('SizeOfImage'),align(rva+len(payload),pe.OPTIONAL_HEADER.SectionAlignment))
    struct.pack_into('<I',output,pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),0)
    patches=[]
    extra = json.loads((PROJECT/'extra-patches.json').read_text()).get(target, [])
    added_patches = [(int(p['vtable_slot'],16),int(p['expected_function'],16),p['replacement']) for p in extra]
    for location, expected, name in [*spec['patches'], *added_patches]:
        offset = pe.get_offset_from_rva(location-pe.OPTIONAL_HEADER.ImageBase)
        actual = struct.unpack_from('<I', original, offset)[0]
        if actual != expected: raise RuntimeError(f'Vtable guard failed at {location:x}')
        struct.pack_into('<I',output,offset,symbols[name])
        patches.append(dict(vtable_slot=f'{location:08x}',original=f'{expected:08x}',replacement=f'{symbols[name]:08x}',source=name))
    entry_patches=[]
    constant_catalog=json.loads((PROJECT/'constant-returns.json').read_text())['targets'][target]
    if constant_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Constant catalog hash differs from the ABI target')
    constant_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in constant_catalog['entries']]
    getter_catalog=json.loads((PROJECT/'word-getters.json').read_text())['targets'][target]
    if getter_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Getter catalog hash differs from the ABI target')
    getter_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in getter_catalog['entries']]
    accessor_catalog=json.loads((PROJECT/'field-accessors.json').read_text())['targets'][target]
    if accessor_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Accessor catalog hash differs from the ABI target')
    accessor_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in accessor_catalog['entries']]
    init_catalog=json.loads((PROJECT/'static-initializers.json').read_text())['targets'][target]
    if init_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Initializer catalog hash differs from the ABI target')
    init_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in init_catalog['entries']]
    deleting_catalog=json.loads((PROJECT/'deleting-destructors.json').read_text())['targets'][target]
    if deleting_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Deleting-destructor catalog hash differs from the ABI target')
    deleting_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in deleting_catalog['entries']]
    construct_catalog=json.loads((PROJECT/'constructor-calls.json').read_text())['targets'][target]
    if construct_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Constructor-call catalog hash differs from the ABI target')
    construct_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in construct_catalog['entries']]
    cast_catalog=json.loads((PROJECT/'dynamic-casts.json').read_text())['targets'][target]
    if cast_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Dynamic-cast catalog hash differs from the ABI target')
    cast_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in cast_catalog['entries']]
    clone_catalog=json.loads((PROJECT/'clones.json').read_text())['targets'][target]
    if clone_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Clone catalog hash differs from the ABI target')
    clone_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in clone_catalog['entries']]
    store_catalog=json.loads((PROJECT/'global-stores.json').read_text())['targets'][target]
    if store_catalog['original_sha256']!=spec['sha']:raise RuntimeError('Global-store catalog hash differs from the ABI target')
    store_entries=[(int(e['address'],16),bytes.fromhex(e['guarded_prefix']),e['symbol']) for e in store_catalog['entries']]
    hand_entries=[
        (spec['target_history'],bytes.fromhex('83ec105657'),'bfv_target_history'),
        (spec['target_history_insert'],bytes.fromhex('51558b6c2410'),'bfv_history_insert'),
        (spec['history_insert_node'],bytes.fromhex('83ec445657'),'bfv_history_insert_node'),
        (spec['tree_previous'],bytes.fromhex('8b018a5015'),'bfv_tree_previous'),
        (spec['tree_rotate_left'],bytes.fromhex('8b5424048b4208'),'bfv_tree_rotate_left'),
        (spec['tree_rotate_right'],bytes.fromhex('8b5424048b02'),'bfv_tree_rotate_right'),
        (spec['tree_construct'],bytes.fromhex('8b5424088bc1'),'bfv_tree_construct'),
        (spec['artillery_filter'],bytes.fromhex('8b4424408b4008'),'bfv_artillery_filter_bridge'),
        (spec['artillery_weapons'],bytes.fromhex('8b4c24508b11'),'bfv_artillery_weapons_bridge'),
        (spec['artillery_aim_gate'],bytes.fromhex('8b4424188a4814'),'bfv_artillery_aim_gate_bridge'),
        (spec['artillery_movement_gate'],bytes.fromhex('8b7c243085ff'),'bfv_artillery_movement_gate_bridge'),
        (spec['artillery_movement_scoring'],bytes.fromhex('85ff0f842a020000'),'bfv_artillery_movement_score_bridge'),
        (spec['vector_cross_assign'],bytes.fromhex('83ec0c8bc1'),'bfv_vector_cross_assign'),
        (spec['component_event2_scalar'],bytes.fromhex('8b41048b4820'),'bfv_component_event2_scalar'),
        (spec['point_xz'],bytes.fromhex('8bc28b118910'),'bfv_point_xz'),
        (spec['point_in_symmetric_bounds'],bytes.fromhex('d90183ec08'),'bfv_point_in_symmetric_bounds'),
        (spec['region_contains_point2'],bytes.fromhex('8a81c4010000'),'bfv_region_contains_point2'),
        (spec['region_contains_point3'],bytes.fromhex('83ec08568bf1'),'bfv_region_contains_point3'),
        (spec['artillery_region_score'],bytes.fromhex('8b84249c000000'),'bfv_artillery_region_score_bridge'),
        (spec['list_find_word'],bytes.fromhex('568b742408'),'bfv_find_word_in_nodes'),
        (spec['artillery_category_score'],bytes.fromhex('f64507010f8475010000'),'bfv_artillery_category_score_bridge'),
        (spec['target_handle_eligible'],bytes.fromhex('51568bc125ffff0000'),'bfv_target_handle_eligible'),
        (spec['artillery_alternate_gate'],bytes.fromhex('8b4d10c1e903'),'bfv_artillery_alternate_gate_bridge'),
        (spec['artillery_alternate_accept'],bytes.fromhex('8d472485c0'),'bfv_artillery_nested_score_bridge'),
        (spec['artillery_nested_finish'],bytes.fromhex('d944242851'),'bfv_artillery_alternate_finish_bridge'),
        (spec['artillery_next_target'],bytes.fromhex('8b49208b542404'),'bfv_next_target_handle'),
        (spec['artillery_filter_reject'],bytes.fromhex('8b5424408b3a'),'bfv_artillery_next_candidate_bridge'),
        (spec['artillery_after_first_pass'],bytes.fromhex('8b4c24508b11'),'bfv_artillery_second_setup_bridge'),
        (spec['artillery_second_filter'],bytes.fromhex('8b44241c8b00'),'bfv_artillery_second_filter_bridge'),
        (spec['artillery_second_accept'],bytes.fromhex('8b4c241c8b16'),'bfv_artillery_second_weight_bridge'),
        (spec['vector_normalize'],bytes.fromhex('51d94108d94104'),'bfv_vector_normalize'),
        (spec['artillery_second_weight_continue'],bytes.fromhex('8b8424fc010000'),'bfv_artillery_second_aim_bridge'),
        (spec['artillery_second_aim_accept'],bytes.fromhex('8b7c24186a02'),'bfv_artillery_second_position_bridge'),
        (spec['artillery_second_position_continue'],bytes.fromhex('d98424c4000000'),'bfv_artillery_second_distance_bridge'),
        (spec['artillery_second_distance_continue'],bytes.fromhex('8b4c24508b01'),'bfv_artillery_second_weapons_bridge'),
        (spec['artillery_second_weapons_accept'],bytes.fromhex('8b7c242485ff'),'bfv_artillery_second_movement_gate_bridge'),
        (spec['artillery_second_movement_continue'],bytes.fromhex('85ff0f8431020000'),'bfv_artillery_second_movement_score_bridge'),
        (spec['artillery_second_driver_gate'],bytes.fromhex('d9442420d80d')+struct.pack('<I',spec['artillery_query_scale']),'bfv_artillery_second_driver_gate_bridge'),
        (spec['artillery_second_region'],bytes.fromhex('85ff8b8c249c0000008b'),'bfv_artillery_second_region_modifier_bridge'),
        (spec['artillery_second_category_gate'],bytes.fromhex('8b4310c1e803a801'),'bfv_artillery_second_category_gate_bridge'),
        (spec['artillery_second_category_prefix'],bytes.fromhex('8b0f894c24248b4b20'),'bfv_artillery_second_category_prefix_bridge'),
        (spec['artillery_second_category_factor'],bytes.fromhex('8d432485c074058b4330eb02'),'bfv_artillery_second_category_factor_bridge'),
        (spec['artillery_second_category_score'],bytes.fromhex('8b8c24d80000008b51088b44243c'),'bfv_artillery_second_category_score_bridge'),
        (spec['artillery_second_iterator'],bytes.fromhex('8b44241c8b8c2404010000'),'bfv_artillery_second_iterator_bridge'),
        (spec['artillery_final_weights'],bytes.fromhex('8b168b3d')+struct.pack('<I',spec['artillery_parameter_manager'])+bytes.fromhex('8bceff92dc000000'),'bfv_artillery_final_weights_bridge'),
        (spec['artillery_final_selection'],bytes.fromhex('d9842480000000'),'bfv_artillery_final_selection_bridge'),
        (spec['artillery_search_prefix'],b'\x8b\x0d'+struct.pack('<I',spec['collision_clock']),'bfv_artillery_search_prefix_bridge'),
        (spec['artillery_candidate_setup'],bytes.fromhex('8b7c24188b0f'),'bfv_artillery_candidate_setup_bridge'),
        (spec['artillery_score'],bytes.fromhex('81ecd8010000'),'bfv_artillery_entry_bridge'),
        (spec['artillery_second_movement_unit_score'],bytes.fromhex('c74424340000803f'),'bfv_artillery_unit_score_bridge'),
        (spec['artillery_driver_reload'],bytes.fromhex('8bbc24fc010000'),'bfv_artillery_driver_reload_bridge'),
        (spec['artillery_allocation_reload'],bytes.fromhex('8b842400010000'),'bfv_artillery_allocation_reload_bridge'),
        (spec['artillery_second_empty'],bytes.fromhex('85c0740950'),'bfv_artillery_query_cleanup_bridge'),
        (spec['artillery_query_gate'],b'\xd9\x44\x24\x1c\xd8\x0d'+struct.pack('<I',spec['artillery_query_scale']),'bfv_artillery_query_gate_bridge'),
        (spec['component_event2_word'],bytes.fromhex('8b41048b4820'),'bfv_component_event2_word'),
        (spec['component_query_data'],bytes.fromhex('8b098b01ff5034'),'bfv_component_query_data'),
        (spec['query_vector_append'],bytes.fromhex('568b710485f6'),'bfv_query_vector_append'),
        (spec['query_vector_destroy'],bytes.fromhex('568bf18b4604'),'bfv_query_vector_destroy'),
        (spec['transform_point'],bytes.fromhex('8b442404d94108'),'bfv_transform_point'),
        (spec['vector_difference'],bytes.fromhex('8b442408d900'),'bfv_vector_difference'),
        (spec['aim_world_position'],bytes.fromhex('568bf18b4e04'),'bfv_aim_world_position'),
        (spec['component_position'],bytes.fromhex('8b41048b542404'),'bfv_component_position'),
        *[(spec[key],bytes.fromhex('d9442404d85c2408'),'bfv_'+key)
          for key in ['float_minimum','float_maximum','float_clamp']],
        (spec['vector_length'],bytes.fromhex('d94108d94104'),'bfv_vector_length'),
        (spec['aim_direction'],bytes.fromhex('8b41048b542404'),'bfv_aim_direction'),
        (spec['aim_within_limits'],bytes.fromhex('83ec405657'),'bfv_aim_within_limits'),
        (spec['aim_compose'],bytes.fromhex('8b54240856'),'bfv_affine_compose'),
        (spec['vector_divide'],b'\xd9\x05'+struct.pack('<I',spec['math_one']),'bfv_vector_divide'),
        *constant_entries,*getter_entries,*accessor_entries,*init_entries,*deleting_entries,*construct_entries,*cast_entries,*clone_entries,*store_entries]
    lifted_patch_entries,lifted_stats=lifted_entries(target,spec,pe,original,hand_entries) if lifted else ([],{})
    for location,expected,name in [*hand_entries,*lifted_patch_entries]:
        offset=pe.get_offset_from_rva(location-pe.OPTIONAL_HEADER.ImageBase)
        if original[offset:offset+len(expected)]!=expected:raise RuntimeError(f'Entry guard failed at {location:x}')
        replacement=symbols[name]
        jump=b'\xe9'+struct.pack('<I',(replacement-location-5)&0xffffffff)+b'\x90'*(len(expected)-5)
        output[offset:offset+len(expected)]=jump
        entry_patches.append(dict(entry=f'{location:08x}',original=expected.hex(),patched=jump.hex(),
                                  replacement=f'{replacement:08x}',source=name))
    new_pe = pefile.PE(data=bytes(output))
    struct.pack_into('<I',output,pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),new_pe.generate_checksum())
    destination=GAME/spec['output']
    try:
        destination.write_bytes(output)
    except PermissionError as error:
        raise RuntimeError(f'Close {destination.name} before rebuilding: Windows locks running executable files') from error
    manifest=dict(target=target,input_sha256=spec['sha'],output=str(destination),
        output_sha256=hashlib.sha256(output).hexdigest(),payload_address=f'{address:08x}',
        payload_bytes=len(payload),symbols={k:f'{v:08x}' for k,v in symbols.items()},patches=patches,entry_patches=entry_patches,lifted=lifted_stats,
        scope='Reconstructed AI interpreter, bailout logic/math, collision callback/dispatcher and distance geometry, event construction, object/interface lookups and word-vector insertion/exception bridge; artillery driver/cache validation, target-history lookup/insertion and red-black balancing, and first-pass candidate filter/weapon selection; shared scalar selectors and vector length/division, direction aiming-limit predicate/event wrapper and affine matrix composition; audited constant-return functions, object-word getters, field/global accessors, C++ static-object initializers (with and without constructor arguments) scalar deleting destructors, dynamic_cast wrappers, virtual clones and constant global stores; remaining engine code/services are retained from the original image')
    (work/'manifest.json').write_text(json.dumps(manifest,indent=2))
    print(f'Compiled {destination.name}: {len(payload)} native payload bytes, {len(patches)} guarded vtable replacements and {len(entry_patches)} guarded function entries'
          +(f' ({len(lifted_patch_entries)} of them L1 lifted C; {lifted_stats})' if lifted_stats else ''))


def main():
    global GAME, COMPILERS
    parser=argparse.ArgumentParser()
    parser.add_argument('--target',choices=('client','server','both'),default='both')
    parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument('--compiler-bin',type=Path,default=COMPILERS)
    parser.add_argument('--no-lifted',action='store_true',help='leave out the L1 lifted C')
    args=parser.parse_args()
    GAME=args.game_dir.resolve()
    COMPILERS=args.compiler_bin.resolve()
    for target in ('client','server') if args.target=='both' else (args.target,): build(target,not args.no_lifted)


if __name__=='__main__': main()
