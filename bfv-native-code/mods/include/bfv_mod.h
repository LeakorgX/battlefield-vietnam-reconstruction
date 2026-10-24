/* Mod API: hook game functions from your own C files in mods/client/ and
 * mods/server/ (see docs/MODDING.md).
 *
 *   MOD_BEFORE(Cmd_ObjectTemplate_repairFactor_execute, my_check) { ... }
 *   MOD_AFTER(0x00626890, my_change) { ... }
 *
 * A hook runs when the game calls the function: a before-hook first (it may
 * replace the function with MOD_RETURN), an after-hook when the function has
 * returned to its caller. The first argument names the function by address or
 * by its name in readable/runtime/symbols_<target>.h; the build finds the
 * hooks and wires them in. Mods are ordinary C: float math, variables that keep
 * their value between calls, your own functions. Unlike the generated code,
 * mods are not checked against the original game, and they survive
 * regenerating the lifted C.
 *
 * Inside a hook `mod` is the call: MOD_THIS, MOD_ARG(i), MOD_RESULT, ... read
 * it. GAME_STDCALL / GAME_THISCALL / GAME_CDECL call game functions. M8/M16/M32
 * read and write game memory, F32(addr) a float in game memory. The game's
 * own x87/SSE state is saved around each hook. MOD_LOG / MOD_LOGF write to the
 * debug output (DebugView).
 *
 * Names: game functions and globals (symbols_<target>.h) and field offsets the
 * readable C names (`ObjectTemplate_repairFactor`, build/<target>/mod_fields.h,
 * generated) have each target's own value, so one mod file can build for the
 * client and the server. */
#ifndef BFV_MOD_H
#define BFV_MOD_H
#include <stdint.h>
#include "l1.h"
#if defined(BFV_MOD_CLIENT) && !defined(BFV_MOD_NO_NAMES)
#include "symbols_client.h"
#include "layouts_client.h"
#elif defined(BFV_MOD_SERVER) && !defined(BFV_MOD_NO_NAMES)
#include "symbols_server.h"
#include "layouts_server.h"
#endif
#if !defined(BFV_MOD_NO_NAMES) && __has_include("mod_fields.h")
#include "mod_fields.h"
#endif
#if !defined(BFV_MOD_NO_NAMES) && __has_include("mod_slots.h")
#include "mod_slots.h"
#endif

/* One call of a hooked function. */
struct bfv_mod {
    struct bfv_cpu *cpu;   /* the game's registers now (eax, ecx, ..., esp) */
    uint32_t function;     /* address of the hooked function */
    uint32_t entry_esp;    /* esp when it was called: [entry_esp] is the return address */
    uint32_t entry_ecx;    /* ecx when it was called (`this` of a method) */
    int32_t pops;          /* stack bytes the function pops on return (-1: unknown) */
    uint32_t skip;         /* set by MOD_RETURN: the function does not run */
    uint8_t *fpu;          /* the game's saved x87/SSE state (fxsave image) */
};

typedef void (*bfv_mod_hook)(struct bfv_mod *mod);

#define MOD_BEFORE(function, name) void name(struct bfv_mod *mod)
#define MOD_AFTER(function, name) void name(struct bfv_mod *mod)

/* Arguments, read from the call: valid in before- and after-hooks. */
#define MOD_THIS (mod->entry_ecx)
#define MOD_ARG(i) M32(mod->entry_esp + 4 + 4 * (i))
#define MOD_ARG_F(i) F32(mod->entry_esp + 4 + 4 * (i))
#define MOD_RETURN_ADDRESS M32(mod->entry_esp)

/* Integer and pointer results are in eax, float results in x87 ST0. */
#define MOD_RESULT (mod->cpu->eax)
#define MOD_SET_RESULT(v) (mod->cpu->eax = (uint32_t)(v))
#define MOD_RESULT_F bfv_mod_result_f(mod)
#define MOD_SET_RESULT_F(v) bfv_mod_set_result_f(mod, (v))

/* In a before-hook: return to the caller with this result; the function and
 * the hooks after this one do not run. The stack arguments are popped as the
 * function would (mod->pops). _F returns a float in ST0. */
#define MOD_RETURN(v) do { bfv_mod_return(mod, (uint32_t)(v)); return; } while (0)
#define MOD_RETURN_F(v) do { bfv_mod_return_f(mod, (v)); return; } while (0)

/* Game memory. */
#define F32(a) (*(volatile float *)(uintptr_t)(uint32_t)(a))

/* Call a game function natively: the value is its eax (_F: its float
 * result). Arguments are 32-bit words, first argument first; pass a float as
 * GAME_FLOAT(x). The hooked function's stack is not touched. */
#define BFV_MOD_WORDS(...) (sizeof((const uint32_t[]){0 __VA_OPT__(,) __VA_ARGS__}) / 4 - 1)
#define BFV_MOD_ARGS(...) ((const uint32_t[]){0 __VA_OPT__(,) __VA_ARGS__} + 1)
#define GAME_CALL_REGS(f, ecx, edx, ...) bfv_mod_call(mod, (uint32_t)(f), (uint32_t)(ecx), (uint32_t)(edx), \
    BFV_MOD_WORDS(__VA_ARGS__), BFV_MOD_ARGS(__VA_ARGS__))
#define GAME_STDCALL(f, ...) GAME_CALL_REGS(f, 0, 0 __VA_OPT__(,) __VA_ARGS__)
#define GAME_CDECL(f, ...) GAME_CALL_REGS(f, 0, 0 __VA_OPT__(,) __VA_ARGS__)
#define GAME_THISCALL(f, obj, ...) GAME_CALL_REGS(f, obj, 0 __VA_OPT__(,) __VA_ARGS__)
#define GAME_FASTCALL(f, a, b, ...) GAME_CALL_REGS(f, a, b __VA_OPT__(,) __VA_ARGS__)
#define GAME_CALL_REGS_F(f, ecx, edx, ...) bfv_mod_call_f(mod, (uint32_t)(f), (uint32_t)(ecx), (uint32_t)(edx), \
    BFV_MOD_WORDS(__VA_ARGS__), BFV_MOD_ARGS(__VA_ARGS__))
#define GAME_STDCALL_F(f, ...) GAME_CALL_REGS_F(f, 0, 0 __VA_OPT__(,) __VA_ARGS__)
#define GAME_CDECL_F(f, ...) GAME_CALL_REGS_F(f, 0, 0 __VA_OPT__(,) __VA_ARGS__)
#define GAME_THISCALL_F(f, obj, ...) GAME_CALL_REGS_F(f, obj, 0 __VA_OPT__(,) __VA_ARGS__)
#define GAME_FLOAT(x) bfv_mod_bits((float)(x))
/* A virtual method: slot n of the object's vtable. */
#define GAME_VIRTUAL(obj, n) M32(M32(obj) + 4 * (n))
/* The same by name: GAME_METHOD(obj, Object, update) is slot VF_Object_update
 * (build/<target>/mod_slots.h, generated; `find.py Class --class` lists them). */
#define GAME_METHOD(obj, cls, name) GAME_VIRTUAL(obj, VF_##cls##_##name)

static inline uint32_t bfv_mod_bits(float x) { union { float f; uint32_t u; } v = { .f = x }; return v.u; }
static inline float bfv_mod_float(uint32_t x) { union { uint32_t u; float f; } v = { .u = x }; return v.f; }

/* Small math without a C library (SSE instructions). */
static inline float mod_sqrtf(float x) { return __builtin_sqrtf(x); }
static inline float mod_fabsf(float x) { return __builtin_fabsf(x); }
static inline float mod_minf(float a, float b) { return a < b ? a : b; }
static inline float mod_maxf(float a, float b) { return a > b ? a : b; }
static inline float mod_clampf(float x, float lo, float hi) { return x < lo ? lo : x > hi ? hi : x; }

/* mods/runtime/mod_runtime.c */
uint32_t bfv_mod_call(struct bfv_mod *mod, uint32_t target, uint32_t ecx, uint32_t edx,
                      uint32_t count, const uint32_t *args);
float bfv_mod_call_f(struct bfv_mod *mod, uint32_t target, uint32_t ecx, uint32_t edx,
                     uint32_t count, const uint32_t *args);
void bfv_mod_return(struct bfv_mod *mod, uint32_t value);
void bfv_mod_return_f(struct bfv_mod *mod, float value);
float bfv_mod_result_f(struct bfv_mod *mod);
void bfv_mod_set_result_f(struct bfv_mod *mod, float value);
/* Write text to the debugger output (OutputDebugStringA; DebugView shows it):
 * MOD_LOG("text\n"), or MOD_LOGF("x=%d y=%f\n", x, y) with %d %u %x %s %f (no
 * width or precision; %f prints 3 decimals). */
#define MOD_LOG(text) mod_log(mod, (text))
#define MOD_LOGF(...) mod_logf(mod, __VA_ARGS__)
void mod_log(struct bfv_mod *mod, const char *text);
void mod_logf(struct bfv_mod *mod, const char *format, ...);

/* Used by the generated hook bodies (build/<target>/mod_hooks.c). */
void bfv_mod_run(struct bfv_cpu *c, uint32_t function, int32_t pops, void (*body)(struct bfv_cpu *),
                 const bfv_mod_hook *before, const bfv_mod_hook *after);

#endif
