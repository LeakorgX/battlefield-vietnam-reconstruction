/* Helpers used by the candidate query gate. Native dependencies are explicit:
 * object/interface services, protected growth insertion and the raw heap. */
#include "artillery_query_helpers.h"
#include "target.h"
typedef uint32_t (BFV_QUERY_TC *get_word)(void *);
typedef uint32_t (BFV_QUERY_TC *get_argument)(void *,uint32_t);
typedef void (BFV_QUERY_TC *insert_words)(void *,uint32_t,uint32_t,const uint32_t *);
static uint32_t word(uintptr_t p) { return *(volatile uint32_t *)p; }
static void store(uintptr_t p,uint32_t value) { *(volatile uint32_t *)p=value; }
static uint32_t get(uint32_t object,uint32_t slot)
{ return ((get_word)(uintptr_t)word(word(object)+slot))((void *)(uintptr_t)object); }

uint32_t BFV_QUERY_TC bfv_component_event2_word(void *component)
{
    uint32_t owner=word((uintptr_t)component+4),receiver=word(owner+0x20);
    uint32_t event=((get_argument)(uintptr_t)word(word(receiver)+0xa0))((void *)(uintptr_t)receiver,2);
    return word(word(event+0x14)+4);
}

void *BFV_QUERY_TC bfv_component_query_data(void *component)
{
    uint32_t object=get(word((uintptr_t)component),0x34);
    if(!object) return (void *)(uintptr_t)word(BFV_ARTILLERY_QUERY_DEFAULT_DATA);
    uint32_t interface_id=word(BFV_ARTILLERY_QUERY_INTERFACE_ID),table=word(object);
    uint32_t converted=((get_argument)(uintptr_t)word(table+0x0c))((void *)(uintptr_t)object,interface_id);
    return (void *)(uintptr_t)get(converted,0x3c);
}

void BFV_QUERY_TC bfv_query_vector_append(void *object,const uint32_t *value)
{
    uint32_t vector=(uintptr_t)object,begin=word(vector+4);
    uint32_t size=begin?(uint32_t)((int32_t)(word(vector+8)-begin)>>2):0;
    if(begin) {
        uint32_t capacity=(uint32_t)((int32_t)(word(vector+12)-begin)>>2);
        if(size<capacity) {
            uint32_t end=word(vector+8),bits=word((uintptr_t)value);
            store(end,bits);store(vector+8,end+4);return;
        }
    }
    uint32_t end=word(vector+8);
    ((insert_words)BFV_ARTILLERY_QUERY_VECTOR_INSERT)(object,end,1,value);
}

void BFV_QUERY_TC bfv_query_vector_destroy(void *object)
{
    uint32_t vector=(uintptr_t)object,begin=word(vector+4);
    if(begin) ((void (*)(uint32_t))BFV_VECTOR_FREE)(begin);
    store(vector+4,0);store(vector+8,0);store(vector+12,0);
}
