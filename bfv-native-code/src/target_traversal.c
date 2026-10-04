/* Object traversal wrapper: owner slot +0x84, then returned object slot +0x5c.
 * The second stack argument is read after the first callback, as in the native
 * body. A scratch output may alias that argument; do not capture it early. */
#include "target_traversal.h"
#define TC __attribute__((thiscall))
typedef uint32_t (TC *get_pointer)(void *,void *);
typedef uint32_t (TC *get_argument)(void *,uint32_t);
static uint32_t word(uintptr_t p) { return *(volatile uint32_t *)p; }
uint32_t TC bfv_next_target_handle(void *object,void *scratch,volatile uint32_t argument)
{
    uintptr_t receiver=word((uintptr_t)object+0x20),table=word(receiver);
    uintptr_t next=((get_pointer)(uintptr_t)word(table+0x84))((void *)receiver,scratch);
    if(!next) return 0xffffffffu;
    return ((get_argument)(uintptr_t)word(word(next)+0x5c))((void *)next,argument);
}
