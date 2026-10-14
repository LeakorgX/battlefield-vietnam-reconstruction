/* L1 entry trampoline shared by all lifted functions.
 *
 * A detoured native entry jumps to a per-function stub; a call's resume stub
 * is the return address the callee returns into. Both first move ESP
 * BFV_L1_GAP bytes down (lea keeps EFLAGS), then push their resume point
 * (0 = function entry) and C body and jump here. Native code may read the dead
 * stack just below its ESP (values a caller or callee left there), so nothing
 * at or just below the guest ESP is written on entry.
 *
 * On arrival [esp] is the body, [esp+4] the resume point and the guest ESP is
 * E = esp + 8 + BFV_L1_GAP. The guest registers and EFLAGS are stored in a
 * struct bfv_cpu just below that scratch, body(cpu) runs on the stack below
 * the struct, then EFLAGS and the registers are reloaded, ESP becomes the final
 * guest ESP F and execution continues at cpu->eip through a push/ret. The only
 * guest-stack word written on the way out is F-4, which for a returning
 * function is where the native `ret` leaves the return address too.
 */
#include "l1.h"

#define STR_(x) #x
#define STR(x) STR_(x)
_Static_assert(sizeof(struct bfv_cpu) == 40, "trampoline offsets");

__asm__(
    ".text\n"
    ".globl _bfv_l1_enter\n"
    "_bfv_l1_enter:\n"                        /* esp = E-GAP-8 */
    "\tpushfl\n"
    "\tpushl %eax\n"                          /* esp = E-GAP-16 */
    "\tleal -40(%esp), %eax\n"                /* eax = cpu */
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
    "\tcld\n"
    "\tpushl %eax\n"
    "\tcall *%ecx\n"
    "\tleal 4(%esp), %eax\n"
    "\tpushl 32(%eax)\n"
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
    "\tleal -40(%esp), %eax\n"                /* eax = cpu */
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
    "\tjmp bfv_l1_enter_tail\n");
