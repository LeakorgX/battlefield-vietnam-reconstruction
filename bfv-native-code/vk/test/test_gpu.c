/* GPU bring-up gate: allocation/upload/readback, native D3D9 clear/state
 * comparisons, textured Present, gamma and Reset. Device draws are unfinished. */
#include "../vkd3d9.h"
#include <stdio.h>
#include <stdlib.h>
#include <d3dx9.h>

#define CHECK(x) do { if (!(x)) { fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); return 1; } } while (0)
#include "scenes.h"
#include "shader_scenes.h"
#include "feature_scenes.h"

static void map_drop(void *value, void *context)
{
    unsigned *count = context;
    if (value == context) ++*count;
}

/* A failed CHECK returns from main with the Vulkan instance alive. Process exit
 * then runs the layers' DLL detach, where a validation layer built on
 * winpthreads waits forever in a static destructor, so end without detach. */
static int device_live, exit_code = 1;
static void skip_detach(void)
{
    if (device_live) TerminateProcess(GetCurrentProcess(), (UINT)exit_code);
}

static void pump(void)
{
    MSG m;
    while (PeekMessageA(&m, 0, 0, 0, PM_REMOVE)) { TranslateMessage(&m); DispatchMessageA(&m); }
}

static int clear_scene(IDirect3DDevice9 *dev, uint32_t *pixels)
{
    IDirect3DSurface9 *bb = 0, *cpu = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev, 0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL,
                                         0xff173b6d, 0.75f, 3)));
    D3DVIEWPORT9 vp = {7, 5, 43, 41, 0, 1};
    CHECK(SUCCEEDED(IDirect3DDevice9_SetViewport(dev, &vp)));
    D3DRECT rects[2] = {{0, 0, 30, 25}, {40, 30, 80, 80}};
    CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev, 2, rects, D3DCLEAR_TARGET, 0xffbd5927, 0, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &bb)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, 64, 64, D3DFMT_A8R8G8B8,
                                                               D3DPOOL_SYSTEMMEM, &cpu, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, bb, cpu)));
    D3DLOCKED_RECT lock;
    CHECK(SUCCEEDED(IDirect3DSurface9_LockRect(cpu, &lock, 0, D3DLOCK_READONLY)));
    for (UINT y = 0; y < 64; y++) memcpy(pixels + y * 64, (uint8_t *)lock.pBits + y * lock.Pitch, 256);
    CHECK(SUCCEEDED(IDirect3DSurface9_UnlockRect(cpu)));
    IDirect3DSurface9_Release(cpu); IDirect3DSurface9_Release(bb);
    vp = (D3DVIEWPORT9){0, 0, 64, 64, 0, 1};
    CHECK(SUCCEEDED(IDirect3DDevice9_SetViewport(dev, &vp)));
    return 0;
}

static int state_scene(IDirect3DDevice9 *dev)
{
    const DWORD before = 0xff123456, after = 0xffabcdef;
    DWORD value;
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev, 7, D3DTSS_CONSTANT, before)));
    IDirect3DStateBlock9 *pixel = 0, *recorded = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_PIXELSTATE, &pixel)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev, 7, D3DTSS_CONSTANT, after)));
    CHECK(SUCCEEDED(IDirect3DStateBlock9_Apply(pixel)));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetTextureStageState(dev, 7, D3DTSS_CONSTANT, &value)));
    CHECK(value == after);
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev, 7, D3DTSS_CONSTANT, before)));
    CHECK(SUCCEEDED(IDirect3DDevice9_BeginStateBlock(dev)));
    CHECK(IDirect3DDevice9_BeginStateBlock(dev) == D3DERR_INVALIDCALL);
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev, 7, D3DTSS_CONSTANT, after)));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetTextureStageState(dev, 7, D3DTSS_CONSTANT, &value)));
    CHECK(value == before);
    float last[4] = {1, 2, 3, 4}, output[4] = {0};
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShaderConstantF(dev, 223, last, 1)));
    CHECK(IDirect3DDevice9_SetPixelShaderConstantF(dev, 223, last, 2) == D3DERR_INVALIDCALL);
    CHECK(SUCCEEDED(IDirect3DDevice9_EndStateBlock(dev, &recorded)));
    CHECK(SUCCEEDED(IDirect3DStateBlock9_Apply(recorded)));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetTextureStageState(dev, 7, D3DTSS_CONSTANT, &value)));
    CHECK(value == after);
    CHECK(SUCCEEDED(IDirect3DDevice9_GetPixelShaderConstantF(dev, 223, output, 1)));
    CHECK(!memcmp(last, output, sizeof last));
    IDirect3DStateBlock9_Release(recorded); IDirect3DStateBlock9_Release(pixel);
    return 0;
}

int main(void)
{
    setvbuf(stdout, 0, _IONBF, 0);  /* keep PASS lines in order with stderr in the piped log */
    struct vkd_map map = {0};
    unsigned key = 71, dropped = 0;
    vkd_map_put(&map, &key, sizeof key, &key);
    vkd_map_put(&map, &key, sizeof key, &dropped);
    CHECK(map.count == 1);
    CHECK(vkd_map_get(&map, &key, sizeof key) == &dropped);
    vkd_map_clear(&map, map_drop, &dropped);
    CHECK(dropped == 1 && !map.count);
    puts("PASS cache replacement: same key overwrites, destructor runs once");
    WNDCLASSA wc = {0};
    wc.lpfnWndProc = DefWindowProcA;
    wc.hInstance = GetModuleHandleA(0);
    wc.lpszClassName = "BFV_Vulkan_GPU_Test";
    CHECK(RegisterClassA(&wc));
    HWND hwnd = CreateWindowA(wc.lpszClassName, "BFV Vulkan bring-up", WS_OVERLAPPEDWINDOW,
                              CW_USEDEFAULT, CW_USEDEFAULT, 352, 279, 0, 0, wc.hInstance, 0);
    CHECK(hwnd);
    ShowWindow(hwnd, SW_SHOWNOACTIVATE);
    pump();
    D3DPRESENT_PARAMETERS pp = {0};
    pp.BackBufferWidth = 64; pp.BackBufferHeight = 64;
    pp.BackBufferFormat = D3DFMT_A8R8G8B8;
    pp.BackBufferCount = 1;
    pp.SwapEffect = D3DSWAPEFFECT_COPY;
    pp.hDeviceWindow = hwnd; pp.Windowed = TRUE;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
    pp.EnableAutoDepthStencil = TRUE; pp.AutoDepthStencilFormat = D3DFMT_D24S8;
    IDirect3D9 *api = vk_Direct3DCreate9(D3D_SDK_VERSION);
    CHECK(api);
    IDirect3DDevice9 *dev = 0;
    CHECK(SUCCEEDED(IDirect3D9_CreateDevice(api, 0, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &dev)));
    struct vkd_device *d = (struct vkd_device *)dev;
    device_live = 1;
    atexit(skip_detach);
    VkFormatProperties vertex_color_props;
    vkGetPhysicalDeviceFormatProperties(vkd.phys, VK_FORMAT_B8G8R8A8_UNORM, &vertex_color_props);
    printf("D3DCOLOR B8G8R8A8 bufferFeatures=%08x vertex support=%u\n", vertex_color_props.bufferFeatures,
        !!(vertex_color_props.bufferFeatures & VK_FORMAT_FEATURE_VERTEX_BUFFER_BIT));
    CHECK(!state_scene(dev));
    CHECK(d->ref == 1);
    CHECK(d->swapchain->swapchain);
    struct vkd_image *img = d->swapchain->back[0]->img;
    uint32_t pixels[64 * 64], readback[64 * 64];
    for (unsigned y = 0; y < 64; y++) for (unsigned x = 0; x < 64; x++)
        pixels[y * 64 + x] = 0xff000000u | x * 4u << 16 | y * 4u << 8 | 0x40u;
    vkd_image_write(d, img, 0, 0, 0, 0, 0, 64, 64, 1, (uint8_t *)pixels, 256, 16384);
    memset(readback, 0, sizeof readback);
    vkd_image_read(d, img, 0, 0, (uint8_t *)readback, 256);
    CHECK(!memcmp(pixels, readback, sizeof pixels));
    printf("PASS upload/readback: 4096 pixels exact\n");
    CHECK(!clear_scene(dev, pixels));
    CHECK(d->ref == 1);
    char dll[MAX_PATH];
    CHECK(GetSystemDirectoryA(dll, MAX_PATH) < MAX_PATH - 10);
    strcat(dll, "\\d3d9.dll");
    HMODULE system = LoadLibraryA(dll);
    CHECK(system);
    IDirect3D9 *(WINAPI *create)(UINT) = (void *)GetProcAddress(system, "Direct3DCreate9");
    CHECK(create);
    IDirect3D9 *native_api = create(D3D_SDK_VERSION);
    CHECK(native_api);
    IDirect3DDevice9 *native = 0;
    CHECK(SUCCEEDED(IDirect3D9_CreateDevice(native_api, 0, D3DDEVTYPE_HAL, hwnd,
        D3DCREATE_SOFTWARE_VERTEXPROCESSING | D3DCREATE_FPU_PRESERVE, &pp, &native)));
    CHECK(!state_scene(native));
    CHECK(!clear_scene(native, readback));
    CHECK(!memcmp(pixels, readback, sizeof pixels));
    CHECK(!lifetime_scene(native));
    printf("PASS native bound texture resurrection\n");
    if (getenv("BFV_NATIVE_ONLY")) {
        CHECK(!IDirect3DDevice9_Release(native));
        puts("PASS native final device release with externally released bound texture");
        exit_code = 0;
        return 0;
    }
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(native, 0, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(native, 0)));
    unsigned scene_failures = 0;
    for (unsigned mode = 0; mode < 10; ++mode) for (unsigned method = 0; method < 4; ++method) {
        char name[80];
        snprintf(name, sizeof name, "geometry method=%u mode=%u", method, mode);
        CHECK(!geometry_scene(native, method, mode, readback));
        CHECK(!geometry_scene(dev, method, mode, pixels));
        scene_failures += compare_pixels(name, pixels, readback, 2);
    }
    for (unsigned version = 0; version < 4; ++version) {
        char name[80]; snprintf(name, sizeof name, "shader model/effect %u", version + 1);
        CHECK(!shader_scene(native, version, readback));
        CHECK(!shader_scene(dev, version, pixels));
        scene_failures += compare_pixels(name, pixels, readback, 2);
    }
    CHECK(!copy_scene(native,readback)); CHECK(!copy_scene(dev,pixels));
    scene_failures+=compare_pixels("UpdateSurface/ColorFill/StretchRect",pixels,readback,0);
    CHECK(!render_texture_scene(native,readback)); CHECK(!render_texture_scene(dev,pixels));
    scene_failures+=compare_pixels("render-to-texture then sampled fullscreen",pixels,readback,2);
    CHECK(!lighting_scene(native,readback)); CHECK(!lighting_scene(dev,pixels));
    scene_failures+=compare_pixels("directional lighting + identity transforms",pixels,readback,2);
    CHECK(!blend_scene(native,readback)); CHECK(!blend_scene(dev,pixels));
    scene_failures+=compare_pixels("X8R8G8B8 destination alpha, BOTH(INV)SRCALPHA blends",pixels,readback,2);
    CHECK(!churn_scene(native,readback)); CHECK(!churn_scene(dev,pixels));
    scene_failures+=compare_pixels("16 draws, one state change each, in one pass",pixels,readback,2);
    for (unsigned samples=0;samples<=8;samples=samples ? samples*2 : 2) {
        char name[80]; snprintf(name,sizeof name,"render target with MSAA %u",samples);
        CHECK(!target_scene(native,(D3DMULTISAMPLE_TYPE)samples,readback));
        CHECK(!target_scene(dev,(D3DMULTISAMPLE_TYPE)samples,pixels));
        scene_failures+=compare_pixels(name,pixels,readback,2);
    }
    const char *bench = getenv("BFV_VK_BENCH");  /* 1: both, d3d9 or vulkan: one */
    if (bench && strcmp(bench, "vulkan")) CHECK(!bench_scene(native,"D3D9"));
    CHECK(!swapchain_lifetime_scene(native,hwnd)); CHECK(!swapchain_lifetime_scene(dev,hwnd));
    CHECK(!clear_scene(dev, pixels));
    CHECK(!IDirect3DDevice9_Release(native));
    IDirect3D9_Release(native_api); FreeLibrary(system);
    printf("PASS D3D9 clear parity: viewport + two clipped rectangles, 4096 exact pixels\n");
    printf("PASS state blocks against D3D9: stage constant bit 32, recording isolation, constant bounds\n");
    for (unsigned frame = 0; frame < 12; frame++) {
        CHECK(SUCCEEDED(IDirect3DDevice9_Present(dev, 0, 0, 0, 0)));
        pump();
    }
    CHECK(d->present_pipes.count == 1);
    CHECK(d->present_vs && d->present_ps);
    printf("PASS textured Present: 12 frames, cached pipeline reused\n");
    D3DGAMMARAMP gamma;
    for (UINT i = 0; i < 256; i++) gamma.red[i] = gamma.green[i] = gamma.blue[i] = (WORD)(65535 - i * 257);
    IDirect3DDevice9_SetGammaRamp(dev, 0, 0, &gamma);
    CHECK(SUCCEEDED(IDirect3DDevice9_Present(dev, 0, 0, 0, 0)));
    CHECK(d->present_pipes.count == 2);
    IDirect3DSurface9 *held = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_GetBackBuffer(dev, 0, 0, D3DBACKBUFFER_TYPE_MONO, &held)));
    CHECK(d->ref == 2);
    CHECK(IDirect3DDevice9_Reset(dev, &pp) == D3DERR_INVALIDCALL);
    IDirect3DSurface9_Release(held);
    CHECK(d->ref == 1);
    CHECK(SUCCEEDED(IDirect3DDevice9_Reset(dev, &pp)));
    CHECK(!clear_scene(dev, readback));
    CHECK(!memcmp(pixels, readback, sizeof pixels));
    CHECK(SUCCEEDED(IDirect3DDevice9_Present(dev, 0, 0, 0, 0)));
    printf("PASS gamma pipeline, implicit references, Reset, clear after Reset\n");
    HWND other = CreateWindowA(wc.lpszClassName, "BFV Vulkan reset window", WS_OVERLAPPEDWINDOW,
                               CW_USEDEFAULT, CW_USEDEFAULT, 224, 167, 0, 0, wc.hInstance, 0);
    CHECK(other);
    ShowWindow(other, SW_SHOWNOACTIVATE); pump();
    pp.hDeviceWindow = other;
    CHECK(SUCCEEDED(IDirect3DDevice9_Reset(dev, &pp)));
    CHECK(!d->swapchain->surface);
    CHECK(!clear_scene(dev, readback));
    CHECK(!memcmp(pixels, readback, sizeof pixels));
    CHECK(SUCCEEDED(IDirect3DDevice9_Present(dev, 0, 0, 0, 0)));
    CHECK(d->swapchain->surface && d->swapchain->hwnd == other);
    printf("PASS Reset to another window: Vulkan surface replaced\n");
    /* Fullscreen changes the desktop display mode, so it only runs on request. */
    if (getenv("BFV_VK_FULLSCREEN")) CHECK(!fullscreen_scene(dev,api,other));
    else puts("SKIP fullscreen scene: it changes the display mode (run_gpu.ps1 -Fullscreen runs it)");
    /* After the last CreateWindow: RTSS's injected RTSSHooks.dll once threw a C++
     * exception out of its window hook in a CreateWindow that followed this loop. */
    if (bench && strcmp(bench, "d3d9")) CHECK(!bench_scene(dev,"Vulkan"));
    CHECK(!lifetime_scene(dev));
    CHECK(!IDirect3DDevice9_Release(dev));
    CHECK(!IDirect3D9_Release(api));
    device_live = 0;
    DestroyWindow(other);
    DestroyWindow(hwnd);
    /* The last IDirect3D9 release destroyed the Vulkan device and instance, so
     * leaked objects were already reported. */
    CHECK(!vkd.dev && !vkd.instance);
    printf("Validation %s: %u errors, %u warnings\n", vkd.validation ? "enabled" : "DISABLED",
           vkd.validation_errors, vkd.validation_warnings);
    CHECK(!vkd.validation_errors);
    CHECK(!vkd.validation_warnings);
    CHECK(!scene_failures);
    return 0;
}
