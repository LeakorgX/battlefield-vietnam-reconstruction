/* Recovered four-byte vector insertion. C owns the arithmetic/copying; a small
 * x86 bridge supplies the registration layout required by the retained C++ CRT. */
#include <stdint.h>
#include <stddef.h>
#include "target.h"
#include "word_vector.h"
#define TC __attribute__((thiscall))
#define NI __attribute__((noinline))
static uint32_t read32(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void write32(uint32_t p, uint32_t v) { *(volatile uint32_t *)(uintptr_t)p = v; }
static uint32_t words(uint32_t first, uint32_t last) { return (uint32_t)((int32_t)(last-first) >> 2); }

static void move_words(uint32_t destination, uint32_t source, uint32_t bytes)
{
    if (destination > source && destination - source < bytes) {
        while (bytes) { bytes -= 4; write32(destination + bytes, read32(source + bytes)); }
    } else {
        for (uint32_t offset = 0; offset < bytes; offset += 4)
            write32(destination + offset, read32(source + offset));
    }
}
uint32_t __attribute__((stdcall)) NI bfv_vector_copy(uint32_t first, uint32_t last, uint32_t destination)
{
    uint32_t bytes = words(first,last)*4;
    move_words(destination,first,bytes);
    return destination + bytes;
}
uint32_t __attribute__((stdcall)) NI bfv_vector_fill(uint32_t first, uint32_t count, const uint32_t *value)
{
    uint32_t result = first + count*4;
    for (uint32_t i=0; i<count; ++i) write32(first+i*4, *(const volatile uint32_t *)value);
    return result;
}
void __attribute__((fastcall)) NI bfv_vector_shift(uint32_t first, uint32_t last, uint32_t destination_end)
{ uint32_t bytes=words(first,last)*4; move_words(destination_end-bytes,first,bytes); }
void __attribute__((fastcall)) NI bfv_vector_fill_range(uint32_t first, uint32_t last, const uint32_t *value)
{ while (first != last) { write32(first,*(const volatile uint32_t *)value); first+=4; } }

/* The bridge places this at EBP-36, with state at EBP-4. Reserved words contain
 * the saved stack pointer and FS registration; C never modifies those words. */
struct insert_guard { volatile uint32_t allocated; uint32_t reserved[7]; volatile int32_t state; };
_Static_assert(offsetof(struct insert_guard,state)==32,"Exception state offset");

void NI bfv_vector_insert_body(void *object, uint32_t position, uint32_t count, const void *source, struct insert_guard *guard)
{
    uint32_t vector=(uintptr_t)object;
    volatile uint32_t value=*(const volatile uint32_t *)source;
    uint32_t begin=read32(vector+4);
    uint32_t capacity=begin ? words(begin,read32(vector+12)) : 0;
    if (!count) return;
    uint32_t size=begin ? words(begin,read32(vector+8)) : 0;
    if (0x3fffffffu-size < count) ((void (TC *)(void *))BFV_VECTOR_LENGTH_ERROR)(object);
    size=begin ? words(begin,read32(vector+8)) : 0;
    if (capacity < size+count) {
        uint32_t grown=0x3fffffffu-(capacity>>1) < capacity ? 0 : capacity+(capacity>>1);
        size=begin ? words(begin,read32(vector+8)) : 0;
        if (grown < size+count) grown=begin ? words(begin,read32(vector+8))+count : count;
        uint32_t allocated=((uint32_t (*)(uint32_t))BFV_VECTOR_ALLOCATOR)(grown*4);
        guard->allocated=allocated;
        guard->state=0;
        __asm__ volatile ("fwait" ::: "memory");
        uint32_t next=bfv_vector_copy(read32(vector+4),position,allocated);
        next=bfv_vector_fill(next,count,(const uint32_t *)&value);
        bfv_vector_copy(position,read32(vector+8),next);
        __asm__ volatile ("fwait" ::: "memory");
        guard->state=-1;
        uint32_t old=read32(vector+4);
        size=old ? words(old,read32(vector+8)) : 0;
        if (old) ((void (*)(uint32_t))BFV_VECTOR_FREE)(old);
        write32(vector+12,allocated+grown*4);
        write32(vector+8,allocated+(size+count)*4);
        write32(vector+4,allocated);
    } else {
        uint32_t end=read32(vector+8), bytes=count*4;
        if (words(position,end) < count) {
            bfv_vector_copy(position,end,position+bytes);
            guard->state=2;
            __asm__ volatile ("fwait" ::: "memory");
            end=read32(vector+8);
            bfv_vector_fill(end,count-words(position,end),(const uint32_t *)&value);
            __asm__ volatile ("fwait" ::: "memory");
            guard->state=-1;
            end=read32(vector+8)+bytes;
            write32(vector+8,end);
            bfv_vector_fill_range(position,end-bytes,(const uint32_t *)&value);
        } else {
            uint32_t shifted=end-bytes;
            write32(vector+8,bfv_vector_copy(shifted,end,end));
            bfv_vector_shift(position,shifted,end);
            bfv_vector_fill_range(position,position+bytes,(const uint32_t *)&value);
        }
    }
}

void bfv_vector_handler(void);
void bfv_vector_catch_free(void);
void bfv_vector_catch_rethrow(void);
struct unwind_entry { int32_t previous; uint32_t action; };
struct catch_entry { uint32_t flags, type, displacement; void (*handler)(void); };
struct try_entry { int32_t low, high, catch_high; uint32_t count; const struct catch_entry *handlers; };
struct function_info {
    uint32_t magic, states; const struct unwind_entry *unwind;
    uint32_t tries; const struct try_entry *try_map;
    uint32_t ip_count, ip_map, exception_spec;
};
static const struct unwind_entry unwind_map[4]={{-1,0},{-1,0},{-1,0},{-1,0}};
static const struct catch_entry catch_free={0,0,0,bfv_vector_catch_free};
static const struct catch_entry catch_rethrow={0,0,0,bfv_vector_catch_rethrow};
static const struct try_entry try_map[2]={{0,0,1,1,&catch_free},{2,2,3,1,&catch_rethrow}};
const struct function_info bfv_vector_info={0x19930520,4,unwind_map,2,try_map,0,0,0xffffffffu};
_Static_assert(sizeof(struct function_info)==32,"Inspected x86 CRT function-info layout");

/* These functions are ABI bridges, not translated gameplay instructions. The
 * runtime calls catch funclets with EBP set to this registered frame. */
void __attribute__((naked)) bfv_vector_handler(void)
{
    __asm__ volatile ("movl $_bfv_vector_info, %%eax; jmp %c0" : : "i"(BFV_VECTOR_FRAME_HANDLER));
}
void __attribute__((naked)) bfv_vector_catch_rethrow(void)
{
    __asm__ volatile ("pushl $0; pushl $0; call %c0; ud2" : : "i"(BFV_VECTOR_THROW));
}
void __attribute__((naked)) bfv_vector_catch_free(void)
{
    __asm__ volatile ("pushl -36(%%ebp); call %c0; addl $4, %%esp; jmp _bfv_vector_catch_rethrow"
                      : : "i"(BFV_VECTOR_FREE));
}
void TC __attribute__((naked)) bfv_vector_insert(void *object __attribute__((unused)),
    uint32_t position __attribute__((unused)), uint32_t count __attribute__((unused)), const void *value __attribute__((unused)))
{
    __asm__ volatile (
        "pushl %ebp; movl %esp, %ebp; pushl $-1; pushl $_bfv_vector_handler;"
        "pushl %fs:0; movl %esp, %fs:0; subl $24, %esp;"
        "pushl %ebx; pushl %esi; pushl %edi; movl %esp, -16(%ebp);"
        "leal -36(%ebp), %eax; pushl %eax; pushl 16(%ebp); pushl 12(%ebp); pushl 8(%ebp); pushl %ecx;"
        "call _bfv_vector_insert_body; addl $20, %esp;"
        "movl -12(%ebp), %eax; movl %eax, %fs:0; popl %edi; popl %esi; popl %ebx;"
        "movl %ebp, %esp; popl %ebp; ret $12");
}
