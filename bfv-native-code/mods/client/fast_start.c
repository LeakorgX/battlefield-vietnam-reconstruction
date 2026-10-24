/* Fast start (client): no intro movies, straight to the main menu, and a
 * load timer in fast_start.log. No game file is changed.
 *
 * Intros. game/Startup.con (in the game archive) plays the four startup
 * movies with console commands:
 *
 *   renderer.playBink Movies/ea.bik 1000.0
 *   renderer.playBink Movies/Dice.bik 5.0
 *   renderer.playBink Movies/Intro.bik 1.0
 *   renderer.playBink Movies/legal.bik 3.0
 *
 * Renderer.playBink execute (0047f760, ecx = the command, plain ret, eax 0)
 * runs only when both arguments are set ([this+0x1c] == 2): [this+0x20]
 * points to the file name (std::string), [this+0x24] to the float. It then
 * calls Renderer::playBink (004a8050), which plays the movie to its end or a
 * key press. The hook returns before that for files under Movies/, so the
 * menu comes up as soon as Startup.con ends. The main menu's own Play Intro
 * entry (menu_PlayIntro, 004d2840) calls Renderer::playBink directly and still
 * works; so do the menu's background movies (BfBinkNode).
 *
 * Loading. Each load's wall time goes to fast_start.log, from setLoading(1)
 * to setLoading(0), so the next cut can be chosen from measurements. The
 * loading screen redraws every 0.5 s instead of every 0.1 s; each redraw
 * waits for vertical sync. */
#include "bfv_mod.h"

#define STR_DATA 0x04
#define STR_SIZE 0x14
#define STR_CAPACITY 0x18

static uint32_t chars_of(uint32_t s)
{
    return M32(s + STR_CAPACITY) >= 16 ? M32(s + STR_DATA) : s + STR_DATA;
}

struct text { char s[200]; int n; };

static void put(struct text *t, const char *s)
{
    while (*s && t->n < (int)sizeof t->s - 3) t->s[t->n++] = *s++;
}

static void put_str(struct text *t, uint32_t s)
{
    uint32_t n = M32(s + STR_SIZE), c = chars_of(s);
    for (uint32_t i = 0; i < n && i < 64 && t->n < (int)sizeof t->s - 3; i++) t->s[t->n++] = (char)M8(c + i);
}

static void put_uint(struct text *t, uint32_t v)
{
    char d[12];
    int k = 0;
    do { d[k++] = (char)('0' + v % 10); v /= 10; } while (v && k < 11);
    while (k && t->n < (int)sizeof t->s - 3) t->s[t->n++] = d[--k];
}

/* fast_start.log in the game folder, through the game's kernel32 imports */
static void file_log(struct bfv_mod *mod, struct text *t)
{
    static const char path[] = "fast_start.log";
    static uint32_t written;
    t->s[t->n++] = '\r';
    t->s[t->n++] = '\n';
    uint32_t file = GAME_STDCALL(M32(imp_CreateFileA), (uint32_t)(uintptr_t)path, 0x40000000u /* GENERIC_WRITE */,
                                 1u /* FILE_SHARE_READ */, 0u, 4u /* OPEN_ALWAYS */, 0x80u /* NORMAL */, 0u);
    if (file == 0xffffffffu) return;
    GAME_STDCALL(M32(imp_SetFilePointer), file, 0u, 0u, 2u /* FILE_END */);
    GAME_STDCALL(M32(imp_WriteFile), file, (uint32_t)(uintptr_t)t->s, (uint32_t)t->n, (uint32_t)(uintptr_t)&written, 0u);
    GAME_STDCALL(M32(imp_CloseHandle), file);
}

static int is_movie(uint32_t s)
{
    static const char prefix[] = "movies";
    uint32_t c = chars_of(s);
    if (M32(s + STR_SIZE) < 7) return 0;
    for (int i = 0; i < 6; i++) if ((M8(c + i) | 32) != (uint8_t)prefix[i]) return 0;
    return M8(c + 6) == '/' || M8(c + 6) == '\\';
}

MOD_BEFORE(0x0047f760, fast_start_skip_movie)
{
    uint32_t name = M32(MOD_THIS + 0x20);
    if (M32(MOD_THIS + 0x1c) != 2 || !name || !is_movie(name)) return;
    struct text t = { .n = 0 };
    put(&t, "skipped startup movie ");
    put_str(&t, name);
    file_log(mod, &t);
    MOD_RETURN(0);
}

/* setLoading(bool) (004a5ce0, ecx = setup object, ret 4): 1 at load start
 * (reads Level/loadcounter.dat, sets [this+0x134]), 0 at the end (writes the
 * counter back). */
static uint32_t load_start, loading;

MOD_BEFORE(0x004a5ce0, fast_start_set_loading)
{
    uint32_t now = GAME_STDCALL(M32(imp_GetTickCount));
    if (MOD_ARG(0) & 0xff) {
        if (!loading) { loading = 1; load_start = now; }
        return;
    }
    if (!loading) return;
    loading = 0;
    struct text t = { .n = 0 };
    put(&t, "load ");
    put_uint(&t, now - load_start);
    put(&t, " ms");
    file_log(mod, &t);
}

/* The loading screen callback (004a5a10, set by setLoading) redraws when
 * getTimeSeconds() passes [0xd5c914], then sets that to now + 0.1 s. Each
 * redraw ends with a Present that waits for vertical sync (dxvk.conf
 * presentInterval 1), up to 16.7 ms per 100 ms of loading. After a redraw
 * the next one is moved to now + 0.5 s. */
#define LOAD_NEXT_DRAW 0x00d5c914
static float next_before;

MOD_BEFORE(0x004a5a10, fast_start_load_screen_before)
{
    next_before = F32(LOAD_NEXT_DRAW);
}

MOD_AFTER(0x004a5a10, fast_start_load_screen_after)
{
    if (bfv_mod_bits(F32(LOAD_NEXT_DRAW)) != bfv_mod_bits(next_before)) F32(LOAD_NEXT_DRAW) += 0.4f;
}
