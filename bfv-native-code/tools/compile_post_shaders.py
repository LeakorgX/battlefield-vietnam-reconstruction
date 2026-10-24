"""Compile the world post shaders (mods/shaders/world_post.hlsl) to C headers.

Each entry point becomes mods/shaders/world_post_<entry>.h holding the
authored shader's bytecode as uint32_t words. Uses the system
d3dcompiler_47.dll (Windows 8.1 and later ship it). The fullscreen vertex
shader is vs_3_0 so the pixel passes can be ps_3_0 (loops, tex2Dlod, VPOS).
"""
import ctypes
import sys
from pathlib import Path

PROJECT = Path(__file__).resolve().parents[1]
SOURCE = PROJECT / 'mods' / 'shaders' / 'world_post.hlsl'
ENTRIES = [('post_vs', 'vs_3_0')] + [(e, 'ps_3_0') for e in
    ('depth_lin', 'ssao', 'composite', 'aa', 'adapt', 'bloom_down', 'bloom_up', 'finish')]


class Blob(ctypes.Structure):
    pass


def blob_bytes(blob):
    vt = ctypes.cast(blob, ctypes.POINTER(ctypes.POINTER(ctypes.c_void_p))).contents
    get_ptr = ctypes.WINFUNCTYPE(ctypes.c_void_p, ctypes.c_void_p)(vt[3])
    get_size = ctypes.WINFUNCTYPE(ctypes.c_size_t, ctypes.c_void_p)(vt[4])
    release = ctypes.WINFUNCTYPE(ctypes.c_ulong, ctypes.c_void_p)(vt[2])
    data = ctypes.string_at(get_ptr(blob), get_size(blob))
    release(blob)
    return data


def compile_entry(d3dc, text, entry, profile):
    code = ctypes.c_void_p()
    errors = ctypes.c_void_p()
    hr = d3dc.D3DCompile(text, len(text), str(SOURCE).encode(), None, None, entry.encode(), profile.encode(),
                         0, 0, ctypes.byref(code), ctypes.byref(errors))
    message = blob_bytes(errors).decode(errors='replace') if errors.value else ''
    if hr != 0 or not code.value:
        sys.exit(f'{entry}: D3DCompile failed {hr & 0xffffffff:08x}\n{message}')
    if message:
        print(message)
    return blob_bytes(code)


def main():
    d3dc = ctypes.WinDLL('d3dcompiler_47.dll')
    d3dc.D3DCompile.restype = ctypes.c_long
    d3dc.D3DCompile.argtypes = [ctypes.c_char_p, ctypes.c_size_t, ctypes.c_char_p, ctypes.c_void_p, ctypes.c_void_p,
                                ctypes.c_char_p, ctypes.c_char_p, ctypes.c_uint, ctypes.c_uint,
                                ctypes.POINTER(ctypes.c_void_p), ctypes.POINTER(ctypes.c_void_p)]
    text = SOURCE.read_bytes()
    for entry, profile in ENTRIES:
        code = compile_entry(d3dc, text, entry, profile)
        assert len(code) % 4 == 0
        words = [int.from_bytes(code[i:i + 4], 'little') for i in range(0, len(code), 4)]
        lines = [', '.join(f'0x{w:08x}u' for w in words[i:i + 6]) for i in range(0, len(words), 6)]
        out = PROJECT / 'mods' / 'shaders' / f'world_post_{entry}.h'
        out.write_text(f'/* Authored bfv-native-code/mods/shaders/world_post.hlsl:{entry}, compiled as '
                       f'{profile}. */\nstatic const uint32_t world_post_{entry}[] = {{\n'
                       + ',\n'.join(lines) + '\n};\n', newline='\n')
        print(f'{out.name}: {len(code)} bytes')


if __name__ == '__main__':
    main()
