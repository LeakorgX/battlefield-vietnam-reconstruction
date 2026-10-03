"""Read only the guarded patch slots/counters in our own offline test process."""
import argparse
import ctypes as c
from ctypes import wintypes as w
import json
from pathlib import Path
from datetime import datetime, timezone

PROJECT=Path(__file__).resolve().parents[1]


def main():
    parser=argparse.ArgumentParser()
    parser.add_argument('--pid',type=int,required=True)
    parser.add_argument('--target',choices=('client','server'),required=True)
    args=parser.parse_args()
    manifest=json.loads((PROJECT/'build'/args.target/'manifest.json').read_text())
    k=c.WinDLL('kernel32',use_last_error=True)
    k.OpenProcess.argtypes=[w.DWORD,w.BOOL,w.DWORD]; k.OpenProcess.restype=w.HANDLE
    k.CloseHandle.argtypes=[w.HANDLE]
    k.ReadProcessMemory.argtypes=[w.HANDLE,c.c_void_p,c.c_void_p,c.c_size_t,c.POINTER(c.c_size_t)]
    k.QueryFullProcessImageNameW.argtypes=[w.HANDLE,w.DWORD,w.LPWSTR,c.POINTER(w.DWORD)]
    k.GetExitCodeProcess.argtypes=[w.HANDLE,c.POINTER(w.DWORD)]
    handle=k.OpenProcess(0x410,False,args.pid)
    if not handle: raise c.WinError(c.get_last_error())
    try:
        image=c.create_unicode_buffer(32768); size=w.DWORD(len(image))
        if not k.QueryFullProcessImageNameW(handle,0,image,c.byref(size)): raise c.WinError(c.get_last_error())
        if Path(image.value).resolve()!=Path(manifest['output']).resolve():
            raise RuntimeError('Process is not the generated editable game executable')
        def read(address,size):
            data=c.create_string_buffer(size); got=c.c_size_t()
            if not k.ReadProcessMemory(handle,c.c_void_p(address),data,size,c.byref(got)) or got.value!=size:
                raise c.WinError(c.get_last_error())
            return data.raw
        status=w.DWORD()
        if not k.GetExitCodeProcess(handle,c.byref(status)) or status.value!=259:
            raise RuntimeError('Generated executable exited')
        assert read(0x400000,2)==b'MZ'
        for patch in manifest['patches']:
            loaded=int.from_bytes(read(int(patch['vtable_slot'],16),4),'little')
            assert loaded==int(patch['replacement'],16),patch
        payload=int(manifest['payload_address'],16)
        on_disk=(PROJECT/'build'/args.target/'payload.bin').read_bytes()
        immutable_end=min(int(address,16) for name,address in manifest['symbols'].items() if name.endswith('_calls'))-payload
        assert read(payload,immutable_end)==on_disk[:immutable_end]
        counters={name:int.from_bytes(read(int(address,16),4),'little')
            for name,address in manifest['symbols'].items() if name.endswith('_calls')}
        report=dict(pid=args.pid,target=args.target,image=image.value,
                    checked_at=datetime.now(timezone.utc).isoformat(),alive=True,
                    loaded_patch_slots=len(manifest['patches']),compiled_body_bytes_verified=immutable_end,counters=counters)
        (PROJECT/'build'/args.target/'startup-smoke.json').write_text(json.dumps(report,indent=2))
        print(json.dumps(report,indent=2))
    finally: k.CloseHandle(handle)


if __name__=='__main__': main()
