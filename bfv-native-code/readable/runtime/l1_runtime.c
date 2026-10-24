/* Entry trampoline, calls and returns shared by all readable bodies.
 *
 * A detoured native entry jumps to a per-function stub. The stub first moves
 * ESP BFV_L1_GAP bytes down (lea keeps EFLAGS), then pushes its resume point
 * (0 = function entry) and C body and jumps here. Native code may read the dead
 * stack just below its ESP (values a caller or callee left there), so nothing
 * at or just below the guest ESP is written on entry.
 *
 * On arrival [esp] is the body, [esp+4] the resume point and the guest ESP is
 * E = esp + 8 + BFV_L1_GAP. The guest registers and EFLAGS are stored in a
 * struct bfv_cpu just below that scratch and then copied to the top of a
 * per-thread shadow stack, where the C body runs; a body runs across its calls
 * and its own guest pushes may go deeper than the gap. So a call made by the body (bfv_call) can run the
 * callee natively on the real game stack and return into the body: C variables
 * survive calls. After the body EFLAGS and the registers are reloaded, ESP
 * becomes the final guest ESP F and execution continues at cpu->eip through a
 * push/ret. The only guest-stack word written on the way out is F-4, which for
 * a returning function is where the native `ret` leaves the return address.
 *
 * Shadow stacks: BFV_THREADS slots {key = the thread's stack base (fs:[4]),
 * top = latest call record}, each with BFV_SHADOW bytes of stack. A thread
 * claims a free slot on its first entry and frees it when its outermost body
 * finishes. A call record {prev, guest ESP at the call, return stub, cpu} is
 * pushed by bfv_call, with 24 bytes after it where the return parks the stub's
 * saved registers (they lie on the guest stack below ESP); a
 * callee's return lands in the call's return stub, which picks the record and
 * returns into the body. Records left behind by a callee
 * that never returned (an exception unwinding past the body) lie below the
 * guest ESP and are dropped at the next entry or return.
 */
#include "l1.h"

#define STR_(x) #x
#define STR(x) STR_(x)
_Static_assert(sizeof(struct bfv_cpu) == 48, "trampoline offsets");
#define BFV_THREADS 16
#define BFV_SHADOW 0x100000

/* edx = this thread's slot (claimed if it has none); clobbers eax, ecx */
#define FIND_SLOT \
    "\tmovl %fs:4, %eax\n" \
    "\tmovl $_bfv_threads, %edx\n" \
    "1:\tcmpl %eax, (%edx)\n\tje 3f\n" \
    "\taddl $16, %edx\n\tcmpl $_bfv_threads+16*" STR(BFV_THREADS) ", %edx\n\tjb 1b\n" \
    "\tmovl %eax, %ecx\n\tmovl $_bfv_threads, %edx\n" \
    "2:\txorl %eax, %eax\n\tlock cmpxchgl %ecx, (%edx)\n\tje 3f\n" \
    "\taddl $16, %edx\n\tcmpl $_bfv_threads+16*" STR(BFV_THREADS) ", %edx\n\tjb 2b\n" \
    "\tint3\n" \
    "3:\n"

__asm__(
    ".section .bfvshadow,\"bw\"\n"
    ".balign 4096\n"
    ".globl _bfv_shadow_stacks\n_bfv_shadow_stacks:\n"
    "\t.space " STR(BFV_THREADS) "*" STR(BFV_SHADOW) "\n"
    ".globl _bfv_threads\n_bfv_threads:\n"     /* slot i's stack ends at _bfv_threads - i*BFV_SHADOW */
    "\t.space 16*" STR(BFV_THREADS) "\n"
    ".balign 4096\n"
    ".globl _bfv_shadow_end\n_bfv_shadow_end:\n"
    ".text\n"
    ".globl _bfv_l1_enter\n"
    "_bfv_l1_enter:\n"                        /* esp = E-GAP-8 */
    "\tpushfl\n"
    "\tpushl %eax\n"                          /* esp = E-GAP-16 */
    "\tleal -48(%esp), %eax\n"                /* eax = cpu */
    "\tmovl %ecx, 4(%eax)\n"
    "\tmovl %edx, 8(%eax)\n"
    "\tmovl %ebx, 12(%eax)\n"
    "\tmovl %ebp, 20(%eax)\n"
    "\tmovl %esi, 24(%eax)\n"
    "\tmovl %edi, 28(%eax)\n"
    "\tpopl %ecx\n\tmovl %ecx, 0(%eax)\n"   /* guest eax */
    "\tpopl %ecx\n\tmovl %ecx, 32(%eax)\n"  /* guest eflags */
    "\tpopl %ecx\n"                           /* body */
    "\tpopl %edx\n\tmovl %edx, 36(%eax)\n"  /* resume point; esp = E-GAP */
    "\tleal " STR(BFV_L1_GAP) "(%esp), %edx\n"
    "\tmovl %edx, 16(%eax)\n"                 /* guest ESP */
    "bfv_l1_enter_tail:\n"                    /* eax = cpu, ecx = body */
    "\tmovl %eax, %esp\n"
    "\tpushl %ecx\n"
    "\tpushl %eax\n"
    FIND_SLOT
    "\tpopl %eax\n"                           /* cpu */
    "\tmovl %edx, 40(%eax)\n"
    "\tmovl 16(%eax), %ecx\n"                 /* E */
    "\tmovl 4(%edx), %ebx\n"                  /* drop records below E */
    "4:\ttestl %ebx, %ebx\n\tjz 5f\n"
    "\tcmpl %ecx, 4(%ebx)\n\tjae 5f\n"
    "\tmovl (%ebx), %ebx\n\tjmp 4b\n"
    "5:\tmovl %ebx, 4(%edx)\n"
    "\tpopl %ecx\n"                           /* body */
    "\ttestl %ebx, %ebx\n\tjnz 6f\n"          /* below the latest record, or the slot's stack end */
    "\tmovl %edx, %ebx\n\tsubl $_bfv_threads, %ebx\n\tshll $16, %ebx\n"
    "\tnegl %ebx\n\taddl $_bfv_threads, %ebx\n"
    /* move *cpu to the shadow stack: the guest stack below ESP may be reused
     * by the body's own pushes (a body runs across calls, so its guest ESP can
     * go deeper than the gap) */
    "6:\tleal -48(%ebx), %edi\n"
    "\tmovl %eax, %esi\n"
    "\tmovl %ecx, %ebx\n"                     /* body */
    "\tmovl $12, %ecx\n"
    "\tcld\n"
    "\trep movsl\n"
    "\tleal -48(%edi), %eax\n"                /* cpu */
    "\tmovl %eax, %esp\n"
    "\tpushl %eax\n"
    "\tpushl %eax\n"
    "\tcall *%ebx\n"
    "\tpopl %eax\n"
    "\tpopl %eax\n"                           /* cpu */
    "\tmovl 40(%eax), %edx\n"
    "\tmovl %eax, %esp\n"
    "\tcmpl $0, 4(%edx)\n\tjne 7f\n"
    "\tmovl $0, (%edx)\n"                     /* outermost body done: free the slot */
    "7:\tpushl 32(%eax)\n"
    "\tpopfl\n"                               /* nothing below changes EFLAGS */
    "\tmovl 16(%eax), %esp\n"                 /* final guest ESP */
    "\tpushl 36(%eax)\n"                      /* continuation */
    "\tmovl 4(%eax), %ecx\n"
    "\tmovl 8(%eax), %edx\n"
    "\tmovl 12(%eax), %ebx\n"
    "\tmovl 20(%eax), %ebp\n"
    "\tmovl 24(%eax), %esi\n"
    "\tmovl 28(%eax), %edi\n"
    "\tmovl 0(%eax), %eax\n"
    "\tret\n"
    /* Deep stubs moved ESP down by the body's own gap G instead: [esp] body,
     * [esp+4] resume point, [esp+8] G, guest ESP E = esp + 12 + G. */
    ".globl _bfv_l1_enter_deep\n"
    "_bfv_l1_enter_deep:\n"                   /* esp = E-G-12 */
    "\tpushfl\n"
    "\tpushl %eax\n"
    "\tleal -48(%esp), %eax\n"                /* eax = cpu */
    "\tmovl %ecx, 4(%eax)\n"
    "\tmovl %edx, 8(%eax)\n"
    "\tmovl %ebx, 12(%eax)\n"
    "\tmovl %ebp, 20(%eax)\n"
    "\tmovl %esi, 24(%eax)\n"
    "\tmovl %edi, 28(%eax)\n"
    "\tpopl %ecx\n\tmovl %ecx, 0(%eax)\n"   /* guest eax */
    "\tpopl %ecx\n\tmovl %ecx, 32(%eax)\n"  /* guest eflags */
    "\tpopl %ecx\n"                           /* body */
    "\tpopl %edx\n\tmovl %edx, 36(%eax)\n"  /* resume point */
    "\tpopl %edx\n"                           /* G; esp = E-G */
    "\taddl %esp, %edx\n"
    "\tmovl %edx, 16(%eax)\n"                 /* guest ESP */
    "\tjmp bfv_l1_enter_tail\n"
    /* bfv_call(cpu, target): the body has pushed the call's return stub at the
     * guest ESP; record the call, load the guest registers and run the target. */
    ".globl _bfv_call\n"
    "_bfv_call:\n"
    "\tmovl 4(%esp), %eax\n"                  /* cpu */
    "\tmovl 8(%esp), %ecx\n"                  /* target */
    "\tpushl %ebp\n\tpushl %ebx\n\tpushl %esi\n\tpushl %edi\n"
    "\tmovl 40(%eax), %edx\n"                 /* slot */
    "\tmovl 16(%eax), %ebx\n"                 /* guest ESP at the call */
    "\tsubl $24, %esp\n"                      /* record +16: room for the return's registers */
    "\tpushl %eax\n"                          /* record: cpu */
    "\tpushl (%ebx)\n"                        /*         return stub */
    "\tpushl %ebx\n"                          /*         guest ESP */
    "\tpushl 4(%edx)\n"                       /*         prev */
    "\tmovl %esp, 4(%edx)\n"
    "\tmovl %ecx, -4(%ebx)\n"                 /* continuation word */
    "\tpushl 32(%eax)\n"
    "\tpopfl\n"
    "\tmovl 4(%eax), %ecx\n"
    "\tmovl 8(%eax), %edx\n"
    "\tmovl 12(%eax), %ebx\n"
    "\tmovl 20(%eax), %ebp\n"
    "\tmovl 24(%eax), %esi\n"
    "\tmovl 28(%eax), %edi\n"
    "\tmovl 16(%eax), %esp\n"
    "\tleal -4(%esp), %esp\n"
    "\tmovl 0(%eax), %eax\n"
    "\tret\n"
    /* A call's return stub moved ESP down by G and pushed G and itself:
     * [esp] stub, [esp+4] G, guest ESP after the return E' = esp + 8 + G. The
     * call's record is the latest one at or above E' - 4 (newer records belong
     * to callees that never returned). */
    ".globl _bfv_l2_return\n"
    "_bfv_l2_return:\n"
    "\tpushfl\n\tpushl %eax\n\tpushl %ecx\n\tpushl %edx\n"
    FIND_SLOT
    "\tpushl %edx\n"                          /* [0] slot [4] edx [8] ecx [12] eax [16] eflags [20] stub [24] G */
    "\tmovl 24(%esp), %edx\n"
    "\tleal 28(%esp,%edx), %edx\n"            /* E' */
    "\tmovl (%esp), %eax\n"
    "\tmovl 4(%eax), %eax\n"                  /* latest record */
    "8:\tmovl (%eax), %ecx\n"
    "\ttestl %ecx, %ecx\n\tjz 9f\n"
    "\tcmpl %edx, 4(%ecx)\n\tjae 9f\n"
    "\tmovl %ecx, %eax\n\tjmp 8b\n"
    "9:\tmovl 20(%esp), %ecx\n"
    "\tcmpl %ecx, 8(%eax)\n\tje 10f\n"
    "\tint3\n"
    /* park the stub's pushes in the record */
    "10:\tmovl 4(%esp), %ecx\n\tmovl %ecx, 16(%eax)\n"   /* edx */
    "\tmovl 8(%esp), %ecx\n\tmovl %ecx, 20(%eax)\n"      /* ecx */
    "\tmovl 12(%esp), %ecx\n\tmovl %ecx, 24(%eax)\n"     /* eax */
    "\tmovl 16(%esp), %ecx\n\tmovl %ecx, 28(%eax)\n"     /* eflags */
    "\tmovl 0(%esp), %ecx\n\tmovl %ecx, 32(%eax)\n"      /* slot */
    "\tmovl 12(%eax), %ecx\n"                 /* cpu */
    "\tmovl %edx, 16(%ecx)\n"
    "\tmovl %ebx, 12(%ecx)\n"
    "\tmovl %ebp, 20(%ecx)\n"
    "\tmovl %esi, 24(%ecx)\n"
    "\tmovl %edi, 28(%ecx)\n"
    "\tmovl 32(%eax), %edx\n\tmovl %edx, 40(%ecx)\n"
    "\tmovl (%eax), %ebx\n\tmovl %ebx, 4(%edx)\n"   /* pop the record */
    "\tmovl 16(%eax), %ebx\n\tmovl %ebx, 8(%ecx)\n"
    "\tmovl 20(%eax), %ebx\n\tmovl %ebx, 4(%ecx)\n"
    "\tmovl 24(%eax), %ebx\n\tmovl %ebx, 0(%ecx)\n"
    "\tmovl 28(%eax), %ebx\n\tmovl %ebx, 32(%ecx)\n"
    "\tleal 40(%eax), %esp\n"
    "\tpopl %edi\n\tpopl %esi\n\tpopl %ebx\n\tpopl %ebp\n"
    "\tcld\n"
    "\tret\n"
    ".globl _bfv_l1_runtime_end\n"
    "_bfv_l1_runtime_end:\n");
