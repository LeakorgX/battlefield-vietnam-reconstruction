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


def build(target):
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
    for location,expected,name in [
        (spec['target_history'],bytes.fromhex('83ec105657'),'bfv_target_history'),
        (spec['target_history_insert'],bytes.fromhex('51558b6c2410'),'bfv_history_insert'),
        (spec['history_insert_node'],bytes.fromhex('83ec445657'),'bfv_history_insert_node'),
        (spec['tree_previous'],bytes.fromhex('8b018a5015'),'bfv_tree_previous'),
        (spec['tree_rotate_left'],bytes.fromhex('8b5424048b4208'),'bfv_tree_rotate_left'),
        (spec['tree_rotate_right'],bytes.fromhex('8b5424048b02'),'bfv_tree_rotate_right'),
        (spec['tree_construct'],bytes.fromhex('8b5424088bc1'),'bfv_tree_construct'),
        (spec['artillery_filter'],bytes.fromhex('8b4424408b4008'),'bfv_artillery_filter_bridge'),
        (spec['artillery_weapons'],bytes.fromhex('8b4c24508b11'),'bfv_artillery_weapons_bridge'),
        *[(spec[key],bytes.fromhex('d9442404d85c2408'),'bfv_'+key)
          for key in ['float_minimum','float_maximum','float_clamp']],
        (spec['vector_length'],bytes.fromhex('d94108d94104'),'bfv_vector_length'),
        (spec['aim_direction'],bytes.fromhex('8b41048b542404'),'bfv_aim_direction'),
        (spec['aim_within_limits'],bytes.fromhex('83ec405657'),'bfv_aim_within_limits'),
        (spec['aim_compose'],bytes.fromhex('8b54240856'),'bfv_affine_compose'),
        (spec['vector_divide'],b'\xd9\x05'+struct.pack('<I',spec['math_one']),'bfv_vector_divide'),
        *constant_entries,*getter_entries]:
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
        payload_bytes=len(payload),symbols={k:f'{v:08x}' for k,v in symbols.items()},patches=patches,entry_patches=entry_patches,
        scope='Reconstructed AI interpreter, bailout logic/math, collision callback/dispatcher and distance geometry, event construction, object/interface lookups and word-vector insertion/exception bridge; artillery driver/cache validation, target-history lookup/insertion and red-black balancing, and first-pass candidate filter/weapon selection; shared scalar selectors and vector length/division, direction aiming-limit predicate/event wrapper and affine matrix composition; audited constant-return functions and object-word getters; remaining engine code/services are retained from the original image')
    (work/'manifest.json').write_text(json.dumps(manifest,indent=2))
    print(f'Compiled {destination.name}: {len(payload)} native payload bytes, {len(patches)} guarded vtable replacements and {len(entry_patches)} guarded function entries')


def main():
    global GAME, COMPILERS
    parser=argparse.ArgumentParser()
    parser.add_argument('--target',choices=('client','server','both'),default='both')
    parser.add_argument('--game-dir',type=Path,default=GAME)
    parser.add_argument('--compiler-bin',type=Path,default=COMPILERS)
    args=parser.parse_args()
    GAME=args.game_dir.resolve()
    COMPILERS=args.compiler_bin.resolve()
    for target in ('client','server') if args.target=='both' else (args.target,): build(target)


if __name__=='__main__': main()
