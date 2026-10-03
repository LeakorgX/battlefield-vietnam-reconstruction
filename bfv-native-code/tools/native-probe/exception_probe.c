/* Isolated live Windows probe, linked separately from the shipping game payload.
 * Uses the inspected game's initialized C++ runtime and an outer catch frame. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
struct probe {
    uint32_t status,caught,exception_code,handler_calls,fs_before,fs_after,phase,mode,entry;
    uint32_t size,count,capacity,position,value,vector[4],old_buffer,freed_buffer,free_calls,allocation_offset,expected_allocation,result[16];
};
struct probe *bfv_probe_active;
void bfv_probe_handler(void);
void bfv_probe_catch(void);
struct unwind_entry { int32_t previous; uint32_t action; };
struct catch_entry { uint32_t flags,type,displacement; void (*handler)(void); };
struct try_entry { int32_t low,high,catch_high; uint32_t count; const struct catch_entry *handlers; };
struct info { uint32_t magic,states; const struct unwind_entry *unwind; uint32_t tries; const struct try_entry *try_map; uint32_t ips,ip_map,spec; };
static const struct unwind_entry unwind_map[2]={{-1,0},{-1,0}};
static const struct catch_entry catch_all={0,0,0,bfv_probe_catch};
static const struct try_entry try_map={0,0,1,1,&catch_all};
const struct info bfv_probe_info={0x19930520,2,unwind_map,1,&try_map,0,0,0xffffffffu};

void bfv_probe_body(struct probe *p)
{
    bfv_probe_active=p;
    p->phase=1;
    uint32_t storage=p->capacity ? ((uint32_t (*)(uint32_t))BFV_VECTOR_ALLOCATOR)(p->capacity*4) : 0;
    p->old_buffer=storage;
    for (uint32_t i=0;i<p->size;++i) ((uint32_t *)(uintptr_t)storage)[i]=0x100+i;
    p->vector[0]=0xaabbccdd;p->vector[1]=storage;p->vector[2]=storage+p->size*4;p->vector[3]=storage+p->capacity*4;
    ((void (TC *)(void *,uint32_t,uint32_t,const void *))(uintptr_t)p->entry)(
        p->vector,storage+p->position*4,p->count,&p->value);
    p->phase=3;
}
void bfv_probe_finish(struct probe *p)
{
    uint32_t begin=p->vector[1],size=(p->vector[2]-begin)/4;
    for (uint32_t i=0;i<size && i<16;++i) p->result[i]=((uint32_t *)(uintptr_t)begin)[i];
    if (begin) ((void (*)(uint32_t))BFV_VECTOR_FREE)(begin);
    bfv_probe_active=0;
}
void bfv_probe_free(uint32_t allocation)
{
    if (bfv_probe_active) {
        bfv_probe_active->freed_buffer=allocation;
        ++bfv_probe_active->free_calls;
    }
    ((void (*)(uint32_t))BFV_VECTOR_FREE)(allocation);
}
uint32_t __attribute__((stdcall)) bfv_probe_fault(uint32_t a __attribute__((unused)),
    uint32_t b __attribute__((unused)),uint32_t c __attribute__((unused)))
{
    bfv_probe_active->phase=2;
    uint32_t frame;
    __asm__ volatile ("movl %%fs:0, %0" : "=r"(frame));
    if (*(volatile uint32_t *)(uintptr_t)(frame+8)==0)
        bfv_probe_active->expected_allocation=*(volatile uint32_t *)(uintptr_t)(frame-bfv_probe_active->allocation_offset);
    ((void (TC *)(void *))BFV_VECTOR_LENGTH_ERROR)(0);
    return 0;
}
void __attribute__((naked)) bfv_probe_handler(void)
{
    __asm__ volatile (
        "movl 8(%%esp), %%eax; movl 20(%%eax), %%edx; incl 12(%%edx);"
        "movl 4(%%esp), %%eax; movl (%%eax), %%ecx; movl %%ecx, 8(%%edx);"
        "movl $_bfv_probe_info, %%eax; jmp %c0" : : "i"(BFV_VECTOR_FRAME_HANDLER));
}
void __attribute__((naked)) bfv_probe_catch(void)
{
    __asm__ volatile (
        "movl 8(%ebp), %eax; movl $1, 4(%eax); movl $_bfv_probe_after_catch, %eax; ret");
}
uint32_t __attribute__((stdcall,naked)) bfv_probe_entry(struct probe *p __attribute__((unused)))
{
    __asm__ volatile (
        "pushl %ebp; movl %esp, %ebp; pushl $-1; pushl $_bfv_probe_handler;"
        "pushl %fs:0; movl %esp, %fs:0; subl $24, %esp;"
        "pushl %ebx; pushl %esi; pushl %edi; movl %esp, -16(%ebp);"
        "movl 8(%ebp), %eax; movl -12(%ebp), %edx; movl %edx, 16(%eax);"
        "movl $0, -4(%ebp); pushl %eax; call _bfv_probe_body; addl $4, %esp;"
        "movl 8(%ebp), %eax; movl $1, (%eax); jmp bfv_probe_exit;"
        ".globl _bfv_probe_after_catch; _bfv_probe_after_catch:"
        "movl 8(%ebp), %eax; movl $2, (%eax);"
        "bfv_probe_exit: movl $-1, -4(%ebp); pushl 8(%ebp); call _bfv_probe_finish; addl $4, %esp;"
        "movl -12(%ebp), %eax; movl %eax, %fs:0; movl 8(%ebp), %edx; movl %eax, 20(%edx);"
        "movl (%edx), %eax; popl %edi; popl %esi; popl %ebx; movl %ebp, %esp; popl %ebp; ret $4");
}
