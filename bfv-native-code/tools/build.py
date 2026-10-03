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
        pool=0xe0cd58, trace_index=0xdfa918, trace_mask=0xd1a088, trace_files=0xdfa8f8,
        trace_lines=0xdfa8d8, trace_source=0xbf7e38, context_begin=0x9dce60,
        context_end=0x9dcc10, context_record=0xa64d40, native_bailout=0x984c20,
        native_vehicle=0x9856f0, interpreter=0x9be180,
        bailout_curve=0x9d7200, bailout_score=0x9d6b90,
        bailout_curve_table=0xe0f678, bailout_score_table=0xe0f674, bailout_pattern=0x983280,
        bailout_curve_initializer=0x9d6c20, bailout_score_initializer=0x9d65b0, curve_allocator=0x403610,
        collision=0x9d4ba0, collision_body_view=0x66b190, collision_interface_id=0xb77ee0, collision_handle_field=0x164,
        collision_registry=0xd7d01c, collision_actors=0xdfa8c8, collision_clock=0xe0ef50,
        collision_pool_entry=0x926af0, collision_event_interface=0x92afb0,
        collision_allocate=0x412ee0, collision_allocator=0xe0f7c0, collision_alloc_source=0xb44284,
        collision_construct=0x9dc3b0, collision_notify=0x9d4b60,
        collision_dispatch=0x9d48b0, collision_notify_state_field=0x1ec,
        collision_distance=0x9d46d0, collision_distance_limit=0xb76560,
        patches=[(0xbf7e34,0x9be180,'bfv_interpret'),(0xbf5a18,0x984c20,'bfv_bailout'),(0xbf5ab8,0x9856f0,'bfv_vehicle'),(0xbf8c64,0x9d4ba0,'bfv_collision')]),
    'server': dict(file='bfvietnam_w32ded.exe', output='bfvietnam_w32ded-editable.exe',
        sha='86cb31cd206e337d79009ee53c896895e72e6dad357351fb82f57ba39220ad6d',
        pool=0xc2f7c0, trace_index=0xc1d380, trace_mask=0x934448, trace_files=0xc1d360,
        trace_lines=0xc1d340, trace_source=0x876820, context_begin=0x7b6bd0,
        context_end=0x7b6980, context_record=0x69f0b0, native_bailout=0x72eef0,
        native_vehicle=0x72f9e0, interpreter=0x774ff0,
        bailout_curve=0x78c5f0, bailout_score=0x78bf80,
        bailout_curve_table=0xc31c10, bailout_score_table=0xc31c0c, bailout_pattern=0x72e510,
        bailout_curve_initializer=0x78c010, bailout_score_initializer=0x78b9a0, curve_allocator=0x403d80,
        collision=0x78a500, collision_body_view=0x564460, collision_interface_id=0x820798, collision_handle_field=0x15c,
        collision_registry=0xbecc64, collision_actors=0xc1d334, collision_clock=0xc314b0,
        collision_pool_entry=0x6e8930, collision_event_interface=0x6e88b0,
        collision_allocate=0x404290, collision_allocator=0xc33388, collision_alloc_source=0x807375,
        collision_construct=0x7ae550, collision_notify=0x78a4c0,
        collision_dispatch=0x78a210, collision_notify_state_field=0x1c0,
        collision_distance=0x78a030, collision_distance_limit=0x81facc,
        patches=[(0x87681c,0x774ff0,'bfv_interpret'),(0x873d88,0x72eef0,'bfv_bailout'),(0x873e28,0x72f9e0,'bfv_vehicle'),(0x8775cc,0x78a500,'bfv_collision')]),
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
    (work / 'target.h').write_text(''.join(f'#define BFV_{k} 0x{v:08x}u\n' for k,v in constants.items()))
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
        match = re.match(r'\s*(0x[0-9a-fA-F]+)\s+_?(bfv_[A-Za-z0-9_]+)(?:@\d+)?\s*$',line)
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
    new_pe = pefile.PE(data=bytes(output))
    struct.pack_into('<I',output,pe.OPTIONAL_HEADER.get_field_absolute_offset('CheckSum'),new_pe.generate_checksum())
    destination=GAME/spec['output']
    try:
        destination.write_bytes(output)
    except PermissionError as error:
        raise RuntimeError(f'Close {destination.name} before rebuilding: Windows locks running executable files') from error
    manifest=dict(target=target,input_sha256=spec['sha'],output=str(destination),
        output_sha256=hashlib.sha256(output).hexdigest(),payload_address=f'{address:08x}',
        payload_bytes=len(payload),symbols={k:f'{v:08x}' for k,v in symbols.items()},patches=patches,
        scope='Reconstructed AI interpreter, bailout logic/math and collision callback; remaining engine code/services are retained from the original image')
    (work/'manifest.json').write_text(json.dumps(manifest,indent=2))
    print(f'Compiled {destination.name}: {len(payload)} native payload bytes, {len(patches)} guarded replacements')


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
