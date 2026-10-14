/* L1 lifted-C runtime: helpers shared by every mechanically lifted function.
 *
 * L1 code is a faithful instruction-by-instruction translation. Integer
 * registers and flags are C locals (GCC removes flag computations that are never
 * read). The x87, MMX and SSE state stays in the real hardware registers: lifted
 * files are compiled with -mgeneral-regs-only, so compiler code never touches
 * them, and every floating-point/SIMD instruction is executed as itself through
 * a one-instruction asm statement. Guest memory accesses are volatile so they
 * keep the native order, width and count.
 *
 * Guest pushes and pops use the real game stack through the guest ESP, exactly
 * like the original. The C body runs on the same stack, BFV_L1_GAP bytes below
 * the entry ESP (see l1_runtime.c), which is dead space for the native code.
 */
#ifndef BFV_L1_H
#define BFV_L1_H
#include <stdint.h>

struct bfv_cpu {
    uint32_t eax, ecx, edx, ebx, esp, ebp, esi, edi; /* offsets 0..28 */
    uint32_t eflags;                                 /* 32 */
    uint32_t eip;                                    /* 36: where to continue */
};

typedef uint32_t __attribute__((aligned(1))) bfv_u32u;
typedef uint16_t __attribute__((aligned(1))) bfv_u16u;
#define M8(a)  (*(volatile uint8_t *)(uintptr_t)(uint32_t)(a))
#define M16(a) (*(volatile bfv_u16u *)(uintptr_t)(uint32_t)(a))
#define M32(a) (*(volatile bfv_u32u *)(uintptr_t)(uint32_t)(a))

/* Sub-register views and updates. */
#define LO8(r)  ((uint8_t)(r))
#define HI8(r)  ((uint8_t)((r) >> 8))
#define LO16(r) ((uint16_t)(r))
#define SET8L(r, v)  ((r) = ((r) & 0xffffff00u) | (uint8_t)(v))
#define SET8H(r, v)  ((r) = ((r) & 0xffff00ffu) | ((uint32_t)(uint8_t)(v) << 8))
#define SET16(r, v)  ((r) = ((r) & 0xffff0000u) | (uint16_t)(v))

static inline uint32_t bfv_parity(uint32_t v)
{
    v &= 0xffu; v ^= v >> 4; v ^= v >> 2; v ^= v >> 1;
    return (~v) & 1u;
}

/* Flag results. W is the operand width in bits; R, A, B are already truncated. */
#define SIGN(W, v) (((v) >> ((W) - 1)) & 1u)
#define F_LOGIC(W, r) do { cf = 0; of = 0; zf = (r) == 0; sf = SIGN(W, r); pf = bfv_parity(r); af = 0; } while (0)
#define F_ADD(W, a, b, r) do { cf = (r) < (a); of = SIGN(W, ((a) ^ (r)) & ((b) ^ (r))); \
    zf = (r) == 0; sf = SIGN(W, r); pf = bfv_parity(r); af = (((a) ^ (b) ^ (r)) >> 4) & 1u; } while (0)
#define F_ADC(W, a, b, c0, r) do { cf = (c0) ? (r) <= (a) : (r) < (a); of = SIGN(W, ((a) ^ (r)) & ((b) ^ (r))); \
    zf = (r) == 0; sf = SIGN(W, r); pf = bfv_parity(r); af = (((a) ^ (b) ^ (r)) >> 4) & 1u; } while (0)
#define F_SUB(W, a, b, r) do { cf = (a) < (b); of = SIGN(W, ((a) ^ (b)) & ((a) ^ (r))); \
    zf = (r) == 0; sf = SIGN(W, r); pf = bfv_parity(r); af = (((a) ^ (b) ^ (r)) >> 4) & 1u; } while (0)
#define F_SBB(W, a, b, c0, r) do { cf = (c0) ? (a) <= (b) : (a) < (b); of = SIGN(W, ((a) ^ (b)) & ((a) ^ (r))); \
    zf = (r) == 0; sf = SIGN(W, r); pf = bfv_parity(r); af = (((a) ^ (b) ^ (r)) >> 4) & 1u; } while (0)
/* inc/dec leave CF unchanged. */
#define F_INC(W, a, r) do { of = (r) == (1u << ((W) - 1)); zf = (r) == 0; sf = SIGN(W, r); \
    pf = bfv_parity(r); af = (((a) ^ 1u ^ (r)) >> 4) & 1u; } while (0)
#define F_DEC(W, a, r) do { of = (a) == (1u << ((W) - 1)); zf = (r) == 0; sf = SIGN(W, r); \
    pf = bfv_parity(r); af = (((a) ^ 1u ^ (r)) >> 4) & 1u; } while (0)
#define F_NEG(W, a, r) do { cf = (a) != 0; of = (a) == (1u << ((W) - 1)); zf = (r) == 0; sf = SIGN(W, r); \
    pf = bfv_parity(r); af = (((a) ^ (r)) >> 4) & 1u; } while (0)
/* Result flags of a shift by a non-zero count (count already masked). */
#define F_SZP(W, r) do { zf = (r) == 0; sf = SIGN(W, r); pf = bfv_parity(r); } while (0)

/* Pack/unpack EFLAGS (pushfd/popfd, x87/SSE compares, sahf/lahf). */
#define FLAG_MASK 0xcd5u /* CF PF AF ZF SF DF OF */
#define FLAGS_WORD() ((uint32_t)(cf | 2u | (pf << 2) | (af << 4) | (zf << 6) | (sf << 7) | (df << 10) | (of << 11)))
#define FLAGS_LOAD(w) do { uint32_t w_ = (w); cf = w_ & 1u; pf = (w_ >> 2) & 1u; af = (w_ >> 4) & 1u;     zf = (w_ >> 6) & 1u; sf = (w_ >> 7) & 1u; df = (w_ >> 10) & 1u; of = (w_ >> 11) & 1u; } while (0)
#define SAHF_LOAD(ah) do { uint32_t w_ = (ah); cf = w_ & 1u; pf = (w_ >> 2) & 1u; af = (w_ >> 4) & 1u;     zf = (w_ >> 6) & 1u; sf = (w_ >> 7) & 1u; } while (0)

/* Bytes of guest stack below the entry ESP that a lifted body may use; the C
 * frame lives below that (same thread, no shared state, so it is re-entrant). */
#define BFV_L1_GAP 0x400

/* Native-ABI entry stub for a lifted body: the detour target. It passes resume
 * point 0 (function entry) to the shared trampoline, pushing only below the gap
 * so the dead stack just under the guest ESP keeps its native contents. */
#define BFV_L1_ENTRY(stub, body) \
    void body(struct bfv_cpu *c); \
    __asm__(".text\n.globl _" #stub "\n_" #stub ":\n\tleal -0x400(%esp), %esp\n\tpushl $0\n\tpushl $_" #body \
            "\n\tjmp _bfv_l1_enter\n")

/* Return address pushed for the k-th call of a body: re-enters it after the call. */
#define BFV_L1_RESUME(stub, body, k) \
    extern const char stub[]; \
    __asm__(".text\n.globl _" #stub "\n_" #stub ":\n\tleal -0x400(%esp), %esp\n\tpushl $" #k "\n\tpushl $_" #body \
            "\n\tjmp _bfv_l1_enter\n")
_Static_assert(BFV_L1_GAP == 0x400, "stub lea above uses the gap");

/* Bodies whose guest pushes go deeper than the default gap allows use their own
 * gap G (at most 0xf00, so the stub never skips a whole stack page) and pass it
 * to _bfv_l1_enter_deep. */
#define BFV_L1_ENTRY_DEEP(stub, body, gap) \
    void body(struct bfv_cpu *c); \
    __asm__(".text\n.globl _" #stub "\n_" #stub ":\n\tleal -" #gap "(%esp), %esp\n\tpushl $" #gap "\n\tpushl $0\n\tpushl $_" #body \
            "\n\tjmp _bfv_l1_enter_deep\n")
#define BFV_L1_RESUME_DEEP(stub, body, k, gap) \
    extern const char stub[]; \
    __asm__(".text\n.globl _" #stub "\n_" #stub ":\n\tleal -" #gap "(%esp), %esp\n\tpushl $" #gap "\n\tpushl $" #k "\n\tpushl $_" #body \
            "\n\tjmp _bfv_l1_enter_deep\n")

/* fs:-relative thread data (SEH chain, TEB). */
static inline uint32_t bfv_fs_read32(uint32_t offset)
{
    uint32_t v;
    __asm__ volatile("movl %%fs:(%1), %0" : "=r"(v) : "r"(offset) : "memory");
    return v;
}
static inline void bfv_fs_write32(uint32_t offset, uint32_t v)
{
    __asm__ volatile("movl %0, %%fs:(%1)" : : "r"(v), "r"(offset) : "memory");
}

#endif
