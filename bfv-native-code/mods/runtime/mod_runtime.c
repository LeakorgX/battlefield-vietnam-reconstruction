/* Mod runtime: runs the hooks of one call of a hooked function and lets hooks
 * call game code (see mods/include/bfv_mod.h).
 *
 * A hooked function's detour enters bfv_mod_<target>_body_<address>
 * (generated, build/<target>/mod_hooks.c) through the same entry trampoline as
 * its lifted body, so the hook sees the guest registers in *c exactly as the
 * body would. bfv_mod_run saves the game's x87/SSE state (fxsave) before mod
 * code runs and restores it (fxrstor) before the original body runs and before
 * returning: mods are compiled with SSE math and never disturb the game's FPU.
 *
 * Calls into game code run natively on the real game stack through bfv_call,
 * starting BFV_L1_GAP bytes below the guest ESP so the dead stack just below it
 * keeps the native values a caller may still read. */
#define BFV_MOD_NO_NAMES /* this file defines memcpy etc., which are game names too */
#include "bfv_mod.h"
/* BFV_MOD_LOG_IMPORT: the game's import slot of OutputDebugStringA (tools/mods.py) */

BFV_L2_RET(bfv_mod_call_return, 0x400);

static void fx_save(uint8_t *image) { __asm__ volatile("fxsave (%0)" : : "r"(image) : "memory"); }
static void fx_restore(uint8_t *image) { __asm__ volatile("fxrstor (%0)" : : "r"(image) : "memory"); }

void bfv_mod_run(struct bfv_cpu *c, uint32_t function, int32_t pops, void (*body)(struct bfv_cpu *),
                 const bfv_mod_hook *before, const bfv_mod_hook *after)
{
    uint8_t raw[512 + 15];
    uint8_t *image = (uint8_t *)(((uintptr_t)raw + 15) & ~(uintptr_t)15);
    struct bfv_mod mod = { c, function, c->esp, c->ecx, pops, 0, image };
    uint32_t return_address = M32(c->esp);
    if (*before) {
        fx_save(image);
        for (; *before && !mod.skip; before++) (*before)(&mod);
        fx_restore(image);
        if (mod.skip) return;
    }
    body(c);
    /* After-hooks run only when the function returned to its caller (not when
     * the body left through a jump into native code that finishes it). */
    if (*after && c->eip == return_address) {
        fx_save(image);
        for (; *after; after++) (*after)(&mod);
        fx_restore(image);
    }
}

void bfv_mod_return(struct bfv_mod *mod, uint32_t value)
{
    struct bfv_cpu *c = mod->cpu;
    if (mod->pops < 0) {
        mod_log(mod, "bfv_mod: MOD_RETURN on a function whose stack cleanup is unknown\n");
        __asm__ volatile("int3");
    }
    c->eax = value;
    c->eip = M32(mod->entry_esp);
    c->esp = mod->entry_esp + 4 + (uint32_t)mod->pops;
    mod->skip = 1;
}

/* fxsave image: FSW at 2 (TOP in bits 11-13), abridged tag byte at 4 (bit i:
 * physical register i in use), ST0..ST7 in 16-byte slots from 32. */
static void st0_store(uint8_t *slot, float value)
{
    __asm__ volatile("flds %1\n\tfstpt (%0)" : : "r"(slot), "m"(value) : "memory");
}

void bfv_mod_return_f(struct bfv_mod *mod, float value)
{
    uint8_t *f = mod->fpu;
    uint16_t fsw = (uint16_t)(f[2] | f[3] << 8);
    uint32_t top = ((fsw >> 11) - 1) & 7u;
    for (int i = 7; i > 0; i--)
        for (int k = 0; k < 16; k++) f[32 + 16 * i + k] = f[32 + 16 * (i - 1) + k];
    st0_store(f + 32, value);
    fsw = (uint16_t)((fsw & ~0x3800u) | top << 11);
    f[2] = (uint8_t)fsw; f[3] = (uint8_t)(fsw >> 8);
    f[4] |= (uint8_t)(1u << top);
    bfv_mod_return(mod, mod->cpu->eax);
}

float bfv_mod_result_f(struct bfv_mod *mod)
{
    float v;
    __asm__ volatile("fldt (%1)\n\tfstps %0" : "=m"(v) : "r"(mod->fpu + 32) : "memory");
    return v;
}

void bfv_mod_set_result_f(struct bfv_mod *mod, float value) { st0_store(mod->fpu + 32, value); }

static void native_call(struct bfv_mod *mod, struct bfv_cpu *t, uint32_t target, uint32_t ecx, uint32_t edx,
                        uint32_t count, const uint32_t *args)
{
    *t = *mod->cpu;
    t->ecx = ecx;
    t->edx = edx;
    t->esp -= BFV_L1_GAP;
    t->eflags &= ~0x400u; /* DF clear, as the native ABI requires at a call */
    for (uint32_t i = count; i > 0; i--) { t->esp -= 4; M32(t->esp) = args[i - 1]; }
    t->esp -= 4;
    M32(t->esp) = (uint32_t)(uintptr_t)bfv_mod_call_return;
    bfv_call(t, target);
}

uint32_t bfv_mod_call(struct bfv_mod *mod, uint32_t target, uint32_t ecx, uint32_t edx,
                      uint32_t count, const uint32_t *args)
{
    struct bfv_cpu t;
    native_call(mod, &t, target, ecx, edx, count, args);
    return t.eax;
}

float bfv_mod_call_f(struct bfv_mod *mod, uint32_t target, uint32_t ecx, uint32_t edx,
                     uint32_t count, const uint32_t *args)
{
    struct bfv_cpu t;
    float v;
    native_call(mod, &t, target, ecx, edx, count, args);
    __asm__ volatile("fstps %0" : "=m"(v) : : "memory");
    return v;
}

void mod_log(struct bfv_mod *mod, const char *text)
{
    GAME_STDCALL(M32(BFV_MOD_LOG_IMPORT), (uint32_t)(uintptr_t)text);
}

/* Freestanding helpers GCC may call for struct copies and clears. */
void *memcpy(void *d, const void *s, unsigned n)
{
    uint8_t *a = d; const uint8_t *b = s;
    while (n--) *a++ = *b++;
    return d;
}
void *memmove(void *d, const void *s, unsigned n)
{
    uint8_t *a = d; const uint8_t *b = s;
    if (a < b) while (n--) *a++ = *b++;
    else while (n--) a[n] = b[n];
    return d;
}
void *memset(void *d, int v, unsigned n)
{
    uint8_t *a = d;
    while (n--) *a++ = (uint8_t)v;
    return d;
}
int memcmp(const void *x, const void *y, unsigned n)
{
    const uint8_t *a = x, *b = y;
    for (; n; n--, a++, b++) if (*a != *b) return *a < *b ? -1 : 1;
    return 0;
}

static char *put_text(char *o, char *end, const char *s)
{
    while (*s && o < end) *o++ = *s++;
    return o;
}
static char *put_unsigned(char *o, char *end, uint32_t v, uint32_t base)
{
    char digits[12]; int n = 0;
    do { digits[n++] = "0123456789abcdef"[v % base]; v /= base; } while (v);
    while (n && o < end) *o++ = digits[--n];
    return o;
}

void mod_logf(struct bfv_mod *mod, const char *format, ...)
{
    char text[512], *o = text, *end = text + sizeof text - 1;
    __builtin_va_list list;
    __builtin_va_start(list, format);
    for (const char *p = format; *p && o < end; p++) {
        if (*p != '%' || !p[1]) { *o++ = *p; continue; }
        switch (*++p) {
        case 'd': { int32_t v = __builtin_va_arg(list, int32_t);
            if (v < 0 && o < end) *o++ = '-';
            o = put_unsigned(o, end, v < 0 ? 0u - (uint32_t)v : (uint32_t)v, 10); break; }
        case 'u': o = put_unsigned(o, end, __builtin_va_arg(list, uint32_t), 10); break;
        case 'x': o = put_unsigned(o, end, __builtin_va_arg(list, uint32_t), 16); break;
        case 's': o = put_text(o, end, __builtin_va_arg(list, const char *)); break;
        case 'f': { double v = __builtin_va_arg(list, double);
            if (v < 0 && o < end) { *o++ = '-'; v = -v; }
            if (v >= 4294967295.0) { o = put_text(o, end, "big"); break; }
            uint32_t whole = (uint32_t)v, milli = (uint32_t)((v - whole) * 1000.0 + 0.5);
            if (milli == 1000) { whole++; milli = 0; }
            o = put_unsigned(o, end, whole, 10);
            if (o < end) *o++ = '.';
            if (milli < 100 && o < end) *o++ = '0';
            if (milli < 10 && o < end) *o++ = '0';
            o = put_unsigned(o, end, milli, 10); break; }
        default: *o++ = *p;
        }
    }
    __builtin_va_end(list);
    *o = 0;
    mod_log(mod, text);
}
