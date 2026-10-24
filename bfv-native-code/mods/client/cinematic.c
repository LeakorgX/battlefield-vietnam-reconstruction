/* Engine-integrated D3D9 colour finish. No proxy DLL or runtime compiler.
 * In the supported original EXE, 004a8c9d calls renderList_end after world
 * and first-person draws, before 004a04d0 draws HUD icons and 00490e10 menus.
 * Only that call site is graded. Build with --tree readable --mods-only:
 * the parent 004a8af0 must stay native so its return address is 004a8ca2.
 * Evidence: lifted/client/unsorted/unsorted_0048e0f0.c; docs/CINEMATIC.md.
 */
#include "bfv_mod.h"
#include "../shaders/cinematic_shader.h"

#define PTR(p) ((uint32_t)(uintptr_t)(p))
#define DEV(slot, ...) GAME_STDCALL(GAME_VIRTUAL(dev, slot), dev __VA_OPT__(,) __VA_ARGS__)
static uint32_t scene_texture, scene_surface, pixel_shader;
static uint32_t cached_w, cached_h, cached_format, owner, resource_failed;
static uint32_t frames;

static void release(struct bfv_mod *mod, uint32_t obj)
{
    if (obj) GAME_STDCALL(GAME_VIRTUAL(obj, 2), obj);
}

static void drop(struct bfv_mod *mod)
{
    release(mod, scene_surface); release(mod, scene_texture); release(mod, pixel_shader);
    scene_surface = scene_texture = pixel_shader = 0;
    cached_w = cached_h = cached_format = owner = resource_failed = 0;
}

static void log_status(struct bfv_mod *mod, const char *message, uint32_t hr)
{
#ifdef BFV_CINEMATIC_TEST
    mod_log(mod, message);
#else
    // Infrequent append-only diagnostics, no per-frame disk traffic.
    static const char path[] = "cinematic.log";
    char line[200]; uint32_t n = 0, written = 0;
    while (*message && n < 170) line[n++] = *message++;
    line[n++] = ' ';
    for (int i = 7; i >= 0; i--) line[n++] = "0123456789abcdef"[(hr >> (4 * i)) & 15];
    line[n++] = '\r'; line[n++] = '\n'; line[n] = 0;
    mod_log(mod, line);
    uint32_t f = GAME_STDCALL(M32(imp_CreateFileA), PTR(path), 0x40000000u, 1, 0, 4, 0x80, 0);
    if (f == 0xffffffffu) return;
    GAME_STDCALL(M32(imp_SetFilePointer), f, 0, 0, 2);
    GAME_STDCALL(M32(imp_WriteFile), f, PTR(line), n, PTR(&written), 0);
    GAME_STDCALL(M32(imp_CloseHandle), f);
#endif
}

static int prepare(struct bfv_mod *mod, uint32_t dev, const uint32_t *desc)
{
    if (owner != dev || cached_w != desc[6] || cached_h != desc[7] || cached_format != desc[0]) {
        drop(mod);
        owner = dev; cached_w = desc[6]; cached_h = desc[7]; cached_format = desc[0];
    }
    if (resource_failed) return 0;
    if (scene_surface && pixel_shader) return 1;
    uint32_t hr = DEV(23, cached_w, cached_h, 1, 1 /* RENDERTARGET */, cached_format,
                      0 /* DEFAULT */, PTR(&scene_texture), 0);
    if (!hr) hr = GAME_STDCALL(GAME_VIRTUAL(scene_texture, 18), scene_texture, 0, PTR(&scene_surface));
    if (!hr) hr = DEV(106, PTR(cinematic_shader_code), PTR(&pixel_shader));
    if (hr || !scene_surface || !pixel_shader) {
        drop(mod); owner = dev; cached_w = desc[6]; cached_h = desc[7]; cached_format = desc[0]; resource_failed = 1;
        log_status(mod, "cinematic: resources unavailable", hr); return 0;
    }
    log_status(mod, "cinematic: ps_2_0 ready", 0);
    return 1;
}

static uint32_t grade(struct bfv_mod *mod, uint32_t dev)
{
    uint32_t rt = 0, bb = 0, saved_state = 0, desc[8] = {0};
    uint32_t hr = DEV(38, 0, PTR(&rt));
    if (hr || !rt) return hr;
    // Never grade a shadow, reflection or other texture render target.
    hr = DEV(18, 0, 0, 0, PTR(&bb));
    if (hr || rt != bb) { release(mod, bb); release(mod, rt); return hr; }
    release(mod, bb);
    hr = GAME_STDCALL(GAME_VIRTUAL(rt, 12), rt, PTR(desc));
    if (hr || !prepare(mod, dev, desc)) { release(mod, rt); return hr ? hr : 1; }
    hr = DEV(59, 1 /* D3DSBT_ALL */, PTR(&saved_state));
    if (hr || !saved_state) { release(mod, rt); return hr ? hr : 1; }
    // Resolve outside a scene; reopen it for the grade and following HUD.
    hr = DEV(42);
    if (!hr) {
        uint32_t copy_hr = DEV(34, rt, 0, scene_surface, 0, 0);
        hr = DEV(41);
        if (!hr && !copy_hr) {
            struct vertex { float x, y, z, rhw, u, v; };
            float w = (float)desc[6], h = (float)desc[7];
            const struct vertex quad[4] = {
                {-0.5f,-0.5f,0,1,0,0}, {w-0.5f,-0.5f,0,1,1,0},
                {-0.5f,h-0.5f,0,1,0,1}, {w-0.5f,h-0.5f,0,1,1,1}
            };
            uint32_t viewport[6] = {0,0,desc[6],desc[7],0,0x3f800000};
            DEV(47, PTR(viewport));
            DEV(92, 0); DEV(89, 0x104 /* XYZRHW | TEX1 */);
            DEV(107, pixel_shader); DEV(65, 0, scene_texture);
            DEV(57, 7, 0);    // ZENABLE
            DEV(57, 14, 0);   // ZWRITEENABLE
            DEV(57, 15, 0);   // ALPHATESTENABLE
            DEV(57, 27, 0);   // ALPHABLENDENABLE
            DEV(57, 28, 0);   // FOGENABLE
            DEV(57, 22, 1);   // CULLMODE NONE
            DEV(57, 8, 3);    // FILLMODE SOLID
            DEV(57, 52, 0);   // STENCILENABLE
            DEV(57, 174, 0);  // SCISSORTESTENABLE
            DEV(57, 168, 15); // COLORWRITEENABLE RGBA
            DEV(57, 194, 0);  // SRGBWRITEENABLE: LDR source already encoded
            DEV(57, 176, 0xffffffffu); // MULTISAMPLEMASK
            DEV(69, 0, 1, 3); DEV(69, 0, 2, 3); // clamp UV
            DEV(69, 0, 5, 1); DEV(69, 0, 6, 1); // point, exact pixel centres
            DEV(69, 0, 7, 0); DEV(69, 0, 11, 0); // no mips, no sRGB decode
            DEV(67, 0, 11, 0); DEV(67, 0, 24, 0); // TEXCOORDINDEX, TEXTURETRANSFORMFLAGS
            hr = DEV(83, 5 /* TRIANGLESTRIP */, 2, PTR(quad), sizeof(struct vertex));
            if (!hr && !frames++) log_status(mod, "cinematic: world graded before HUD", 0);
        } else if (!hr) hr = copy_hr;
    }
    // Restore actual device state to match the engine's unchanged state cache.
    GAME_STDCALL(GAME_VIRTUAL(saved_state, 5), saved_state);
    release(mod, saved_state); release(mod, rt);
    return hr;
}

MOD_AFTER(0x00a401b0, cinematic_world)
{
    if (MOD_RETURN_ADDRESS != 0x004a8ca2u || !M8(0x00e20bb4u)) return;
    uint32_t dev = M32(g_pD3DDevice);
    if (dev) {
        uint32_t hr = grade(mod, dev);
        static uint32_t errors;
        if (hr && errors++ < 3) log_status(mod, "cinematic: pass skipped/failed", hr);
    }
}
MOD_BEFORE(0x00a5fbc0, cinematic_reset) { drop(mod); }
MOD_BEFORE(0x00a5f650, cinematic_release) { drop(mod); }
MOD_BEFORE(0x00a603e0, cinematic_create) { drop(mod); }

#ifdef BFV_CINEMATIC_TEST
uint32_t cinematic_test_grade(struct bfv_mod *mod, uint32_t dev) { return grade(mod, dev); }
void cinematic_test_drop(struct bfv_mod *mod) { drop(mod); }
#endif
