/* GPU integration test for mods/client/cinematic.c; see docs/CINEMATIC.md. */
#define COBJMACROS
#include <windows.h>
#include <d3d9.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
uint32_t cinematic_test_grade(void *, uint32_t);
void cinematic_test_drop(void *);
void mod_log(void *m, const char *s) { puts(s); }
uint32_t bfv_mod_call(void *m, uint32_t target, uint32_t ecx, uint32_t edx, uint32_t count, const uint32_t *a) {
 switch(count) {
case 0: return ((uint32_t (__stdcall *)())(uintptr_t)target)();
case 1: return ((uint32_t (__stdcall *)(uint32_t))(uintptr_t)target)(a[0]);
case 2: return ((uint32_t (__stdcall *)(uint32_t,uint32_t))(uintptr_t)target)(a[0],a[1]);
case 3: return ((uint32_t (__stdcall *)(uint32_t,uint32_t,uint32_t))(uintptr_t)target)(a[0],a[1],a[2]);
case 4: return ((uint32_t (__stdcall *)(uint32_t,uint32_t,uint32_t,uint32_t))(uintptr_t)target)(a[0],a[1],a[2],a[3]);
case 5: return ((uint32_t (__stdcall *)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t))(uintptr_t)target)(a[0],a[1],a[2],a[3],a[4]);
case 6: return ((uint32_t (__stdcall *)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t))(uintptr_t)target)(a[0],a[1],a[2],a[3],a[4],a[5]);
case 7: return ((uint32_t (__stdcall *)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t))(uintptr_t)target)(a[0],a[1],a[2],a[3],a[4],a[5],a[6]);
case 8: return ((uint32_t (__stdcall *)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t))(uintptr_t)target)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7]);
case 9: return ((uint32_t (__stdcall *)(uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t,uint32_t))(uintptr_t)target)(a[0],a[1],a[2],a[3],a[4],a[5],a[6],a[7],a[8]);
default: abort(); }}
#define CHECK(x) do { HRESULT hr=(x); if(FAILED(hr)) {printf("FAIL %s: %08lx line %d\n",#x,hr,__LINE__);return 1;} }while(0)
int main(void) {
 HWND hwnd=CreateWindowA("STATIC","Cinematic GPU test",WS_POPUP,0,0,128,64,0,0,GetModuleHandleA(0),0);
 IDirect3D9 *d=Direct3DCreate9(D3D_SDK_VERSION); IDirect3DDevice9 *dev=0;
 D3DPRESENT_PARAMETERS pp={0}; pp.Windowed=TRUE; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
 pp.hDeviceWindow=hwnd; pp.BackBufferWidth=128;pp.BackBufferHeight=64;pp.BackBufferFormat=D3DFMT_X8R8G8B8;
 int ran=0;
 int modes[]={0,2,4,8};
 for(int mode=0;mode<4;mode++) {
  int samples=modes[mode];
  if(samples && FAILED(IDirect3D9_CheckDeviceMultiSampleType(d,0,D3DDEVTYPE_HAL,pp.BackBufferFormat,TRUE,samples,0))) continue;
  pp.MultiSampleType=samples;
  CHECK(IDirect3D9_CreateDevice(d,0,D3DDEVTYPE_HAL,hwnd,D3DCREATE_SOFTWARE_VERTEXPROCESSING,&pp,&dev));
  for(int iteration=0;iteration<3;iteration++) {
   if(iteration==2) { cinematic_test_drop(0); CHECK(IDirect3DDevice9_Reset(dev,&pp)); }
   IDirect3DSurface9 *bb=0,*sys=0,*resolved=0; D3DLOCKED_RECT lr;
   CHECK(IDirect3DDevice9_GetBackBuffer(dev,0,0,D3DBACKBUFFER_TYPE_MONO,&bb));
   for(int i=0;i<8;i++) { RECT rect={i*16,0,(i+1)*16,64};
    DWORD colours[]={0xff000000,0xff202020,0xff808080,0xffc0c0c0,0xffffffff,0xff408030,0xff3050a0,0xff804020};
    CHECK(IDirect3DDevice9_ColorFill(dev,bb,&rect,colours[i])); }
   D3DVIEWPORT9 vp={3,4,100,50,0.2f,0.8f},after;
   CHECK(IDirect3DDevice9_SetViewport(dev,&vp));
   CHECK(IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHABLENDENABLE,TRUE));
   CHECK(IDirect3DDevice9_SetRenderState(dev,D3DRS_FILLMODE,D3DFILL_WIREFRAME));
   CHECK(IDirect3DDevice9_BeginScene(dev));
   if(cinematic_test_grade(0,(uint32_t)(uintptr_t)dev)) return 2;
   CHECK(IDirect3DDevice9_GetViewport(dev,&after));
   DWORD blend,fill; IDirect3DDevice9_GetRenderState(dev,D3DRS_ALPHABLENDENABLE,&blend);IDirect3DDevice9_GetRenderState(dev,D3DRS_FILLMODE,&fill);
   if(memcmp(&vp,&after,sizeof(vp))||!blend||fill!=D3DFILL_WIREFRAME) { puts("FAIL state restore");return 3; }
   CHECK(IDirect3DDevice9_EndScene(dev));
   CHECK(IDirect3DDevice9_CreateRenderTarget(dev,128,64,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&resolved,0));
   CHECK(IDirect3DDevice9_StretchRect(dev,bb,0,resolved,0,D3DTEXF_NONE));
   CHECK(IDirect3DDevice9_CreateOffscreenPlainSurface(dev,128,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&sys,0));
   CHECK(IDirect3DDevice9_GetRenderTargetData(dev,resolved,sys));
   CHECK(IDirect3DSurface9_LockRect(sys,&lr,0,D3DLOCK_READONLY));
   DWORD *row=(DWORD *)((char *)lr.pBits+32*lr.Pitch);
   printf("samples=%d cycle=%d pixels:",samples,iteration);
   for(int i=0;i<8;i++) printf(" %06lx",row[i*16+8]&0xffffff);
   puts("");
   if((row[8]&0xffffff)!=0 || (row[40]&0xffffff)==0x808080 || (row[88]&0xffffff)==0x408030) {puts("FAIL shader output");return 4;}
   CHECK(IDirect3DSurface9_UnlockRect(sys));
   IDirect3DSurface9_Release(sys);IDirect3DSurface9_Release(resolved);IDirect3DSurface9_Release(bb);
  }
  cinematic_test_drop(0);
  ULONG refs=IDirect3DDevice9_Release(dev); if(refs) {printf("FAIL leaked device refs %lu\n",refs);return 5;}
  ran++;
 }
 IDirect3D9_Release(d);DestroyWindow(hwnd);
 printf("PASS: %d device modes, 3 cycles each, state restoration, shader output, Reset and reference cleanup\n",ran);
 return ran?0:6;
}
