/* L2 readable-C runtime: the shorthand used by tools/lift_l2.py.
 *
 * L2 is the same verified translation as L1 (see l1.h), written compactly:
 * flag updates nobody reads are left out, a compare followed by its branch is
 * one `if`, addresses are written inline, and stack and call sequences use the
 * macros below. Values only the function sees are C variables; values a
 * caller or callee sees are named `#define`s of their native register (`this`
 * is ecx in a method, `result` eax). Each body is checked against the original
 * code by tools/verify_lifted.py exactly like L1.
 */
#ifndef BFV_L2_H
#define BFV_L2_H
#include "l1.h"

/* Register variables (v1, v2, ...) are assigned on every path to their uses,
 * but a restart label (`switch (c->eip)`) can hide that from the compiler, and
 * a value the original computes but never reads is kept. */
#pragma GCC diagnostic ignored "-Wmaybe-uninitialized"
#pragma GCC diagnostic ignored "-Wunused-but-set-variable"
#pragma GCC diagnostic ignored "-Wunused-variable"

/* Body prologue and epilogue: guest registers and flags in, then out. */
#define L2_BEGIN \
    uint32_t eax = c->eax, ecx = c->ecx, edx = c->edx, ebx = c->ebx; \
    uint32_t esp = c->esp, ebp = c->ebp, esi = c->esi, edi = c->edi; \
    uint32_t eflags_rest = c->eflags, cf, pf, af, zf, sf, df, of; \
    uint32_t t __attribute__((unused)), a __attribute__((unused)), b __attribute__((unused)); \
    uint32_t r __attribute__((unused)), n __attribute__((unused)), c0 __attribute__((unused)); \
    uint32_t ea __attribute__((unused)), lhs __attribute__((unused)), rhs __attribute__((unused)); \
    int64_t p __attribute__((unused)); uint64_t u __attribute__((unused)); \
    FLAGS_LOAD(eflags_rest)
#define L2_END \
    out: __attribute__((unused)); \
    c->eax = eax; c->ecx = ecx; c->edx = edx; c->ebx = ebx; \
    c->esp = esp; c->ebp = ebp; c->esi = esi; c->edi = edi; \
    c->eflags = (eflags_rest & ~FLAG_MASK) | FLAGS_WORD()

/* Stack slots by name: `#define local_4 M32(frame - 0x4)` reads and writes the
 * slot, ADDR(local_4) is its guest address. */
#define ADDR(slot) ((uint32_t)(uintptr_t)&(slot))

/* Guest stack. */
#define PUSH(v) do { uint32_t v_ = (v); esp -= 4; M32(esp) = v_; } while (0)
#define POP() __extension__({ uint32_t v_ = M32(esp); esp += 4; v_; })

/* Leave the body and continue in native code at an address. */
#define EXIT_TO(target) do { c->eip = (target); goto out; } while (0)
/* ret / ret n */
#define RET(n) do { c->eip = M32(esp); esp += 4 + (n); goto out; } while (0)
/* k-th call of the body (per-function L2_FN): push the call's return stub as
 * the return address and run the callee natively with the guest registers;
 * the body continues with the registers the callee left. */
#define L2_PASTE(p, f, k) p##f##_##k
#define L2_STUB(p, f, k) L2_PASTE(p, f, k)
#define L2_SPILL() do { c->eax = eax; c->ecx = ecx; c->edx = edx; c->ebx = ebx; \
    c->esp = esp; c->ebp = ebp; c->esi = esi; c->edi = edi; \
    c->eflags = (eflags_rest & ~FLAG_MASK) | FLAGS_WORD(); } while (0)
#define L2_RELOAD() do { eax = c->eax; ecx = c->ecx; edx = c->edx; ebx = c->ebx; \
    esp = c->esp; ebp = c->ebp; esi = c->esi; edi = c->edi; \
    eflags_rest = c->eflags; FLAGS_LOAD(eflags_rest); } while (0)
#define CALL(k, target) do { uint32_t t_ = (target); esp -= 4; \
    M32(esp) = (uint32_t)(uintptr_t)L2_STUB(L2_PREFIX, L2_FN, k); \
    L2_SPILL(); bfv_call(c, t_); L2_RELOAD(); } while (0)

/* A call with its stack arguments, first argument first:
 * CALL_ARGS(k, f, a, b) pushes b, then a, and calls f (f pops them, or the
 * caller frees them later);
 * CALL_CDECL(k, f, a, b) does the same and frees them right after the call. */
#define L2_CAT_(x, y) x##y
#define L2_CAT(x, y) L2_CAT_(x, y)
#define L2_NARG(...) L2_NARG_(__VA_ARGS__, 24, 23, 22, 21, 20, 19, 18, 17, 16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1, 0)
#define L2_NARG_(_1, _2, _3, _4, _5, _6, _7, _8, _9, _10, _11, _12, _13, _14, _15, _16, _17, _18, _19, _20, _21, _22, _23, _24, n_, ...) n_
#define L2_PUSHR_1(x_) PUSH(x_)
#define L2_PUSHR_2(x_, ...) L2_PUSHR_1(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_3(x_, ...) L2_PUSHR_2(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_4(x_, ...) L2_PUSHR_3(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_5(x_, ...) L2_PUSHR_4(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_6(x_, ...) L2_PUSHR_5(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_7(x_, ...) L2_PUSHR_6(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_8(x_, ...) L2_PUSHR_7(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_9(x_, ...) L2_PUSHR_8(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_10(x_, ...) L2_PUSHR_9(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_11(x_, ...) L2_PUSHR_10(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_12(x_, ...) L2_PUSHR_11(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_13(x_, ...) L2_PUSHR_12(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_14(x_, ...) L2_PUSHR_13(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_15(x_, ...) L2_PUSHR_14(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_16(x_, ...) L2_PUSHR_15(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_17(x_, ...) L2_PUSHR_16(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_18(x_, ...) L2_PUSHR_17(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_19(x_, ...) L2_PUSHR_18(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_20(x_, ...) L2_PUSHR_19(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_21(x_, ...) L2_PUSHR_20(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_22(x_, ...) L2_PUSHR_21(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_23(x_, ...) L2_PUSHR_22(__VA_ARGS__); PUSH(x_)
#define L2_PUSHR_24(x_, ...) L2_PUSHR_23(__VA_ARGS__); PUSH(x_)
/* pushes the arguments last to first */
#define L2_PUSH_ARGS(...) L2_CAT(L2_PUSHR_, L2_NARG(__VA_ARGS__))(__VA_ARGS__)
#define CALL_ARGS(k, target, ...) L2_PUSH_ARGS(__VA_ARGS__); CALL(k, target)
#define CALL_CDECL(k, target, ...) L2_PUSH_ARGS(__VA_ARGS__); CALL(k, target); esp += 4 * L2_NARG(__VA_ARGS__)

/* Typed calls, where the callee's register inputs and stack arguments are
 * known from its code: the value is eax after the call (`r = STDCALL(...)`).
 * STDCALL(k, f, a, b): f takes no register input and pops its arguments
 * (or has none); THISCALL(k, f, obj, a, b): the same with ecx = obj first;
 * CDECL(k, f, a, b): no register input, the caller frees the arguments.
 * Same stores in the same order as CALL_ARGS / CALL_CDECL. */
#define STDCALL(k, target, ...) __extension__({ __VA_OPT__(L2_PUSH_ARGS(__VA_ARGS__);) CALL(k, target); eax; })
#define THISCALL(k, target, obj, ...) __extension__({ ecx = (obj); __VA_OPT__(L2_PUSH_ARGS(__VA_ARGS__);) CALL(k, target); eax; })
#define CDECL(k, target, ...) __extension__({ CALL_CDECL(k, target, __VA_ARGS__); eax; })

/* Callee-saved registers: SAVE(ebx, esi) pushes ebx then esi,
 * RESTORE(esi, ebx) pops esi then ebx. */
#define L2_SAVE_1(x_) PUSH(x_)
#define L2_SAVE_2(x_, ...) PUSH(x_); L2_SAVE_1(__VA_ARGS__)
#define L2_SAVE_3(x_, ...) PUSH(x_); L2_SAVE_2(__VA_ARGS__)
#define L2_SAVE_4(x_, ...) PUSH(x_); L2_SAVE_3(__VA_ARGS__)
#define L2_RESTORE_1(x_) x_ = POP()
#define L2_RESTORE_2(x_, ...) x_ = POP(); L2_RESTORE_1(__VA_ARGS__)
#define L2_RESTORE_3(x_, ...) x_ = POP(); L2_RESTORE_2(__VA_ARGS__)
#define L2_RESTORE_4(x_, ...) x_ = POP(); L2_RESTORE_3(__VA_ARGS__)
#define SAVE(...) L2_CAT(L2_SAVE_, L2_NARG(__VA_ARGS__))(__VA_ARGS__)
#define RESTORE(...) L2_CAT(L2_RESTORE_, L2_NARG(__VA_ARGS__))(__VA_ARGS__)

/* One native x87/MMX/SSE instruction (Intel syntax). */
#define INTEL(s) ".intel_syntax noprefix\n\t" s "\n\t.att_syntax prefix"
#define FPU(s) __asm__ volatile(INTEL(s))
/* ... with one memory operand at `addr` (written as [%k0] in the text). */
#define FPU_MEM(s, addr) do { ea = (addr); __asm__ volatile(INTEL(s) : : "r"(ea) : "memory", "cc"); } while (0)

#endif
