#define CINTERFACE
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <string.h>
#define CHECK(x) do { if (!(x)) { fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x); return 1; } } while(0)
int main(void)
{
    HWND hwnd = CreateWindowA("STATIC", "D3D9 lifetime", WS_OVERLAPPEDWINDOW, 0,0,128,128,0,0,GetModuleHandleA(0),0);
    char path[MAX_PATH]; GetSystemDirectoryA(path, MAX_PATH); strcat(path,"\\d3d9.dll");
    HMODULE dll = LoadLibraryA(path);
    IDirect3D9 *(WINAPI *create)(UINT) = (void *)GetProcAddress(dll,"Direct3DCreate9");
    IDirect3D9 *api = create(D3D_SDK_VERSION); IDirect3DDevice9 *dev = 0;
    D3DPRESENT_PARAMETERS pp = {0}; pp.BackBufferWidth=64; pp.BackBufferHeight=64;
    pp.BackBufferFormat=D3DFMT_A8R8G8B8; pp.BackBufferCount=1; pp.SwapEffect=D3DSWAPEFFECT_COPY;
    pp.hDeviceWindow=hwnd; pp.Windowed=TRUE;
    CHECK(SUCCEEDED(IDirect3D9_CreateDevice(api,0,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev)));
    IDirect3DTexture9 *tex=0; IDirect3DBaseTexture9 *got=0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateTexture(dev,4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev,0,(IDirect3DBaseTexture9 *)tex)));
    printf("texture release bound external ref=%lu\n",(unsigned long)IDirect3DTexture9_Release(tex));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetTexture(dev,0,&got))); CHECK(got==(IDirect3DBaseTexture9 *)tex);
    printf("resurrected texture release=%lu\n",(unsigned long)IDirect3DBaseTexture9_Release(got));
    DWORD ps[]={0xffff0200,0x02000001,0x800f0800,0xa0e40000,0x0000ffff};
    IDirect3DPixelShader9 *shader=0,*recovered=0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreatePixelShader(dev,ps,&shader)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev,shader)));
    printf("shader release bound external ref=%lu\n",(unsigned long)IDirect3DPixelShader9_Release(shader));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetPixelShader(dev,&recovered))); CHECK(recovered==shader);
    printf("resurrected shader release=%lu\n",(unsigned long)IDirect3DPixelShader9_Release(recovered));
    ULONG final=IDirect3DDevice9_Release(dev); printf("device release with bound ref0 texture+shader=%lu\n",(unsigned long)final);
    CHECK(!final); CHECK(!IDirect3D9_Release(api)); FreeLibrary(dll); DestroyWindow(hwnd);
    return 0;
}
