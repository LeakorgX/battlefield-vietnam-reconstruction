/* Artillery target-history insertion. Node allocation/exception protection and
 * native string/exception runtime services remain dependencies; traversal,
 * duplicate selection, link updates and red/black balancing are source-owned. */
#include "history_tree.h"
#include "target.h"
#include <stddef.h>

enum { LEFT=0, PARENT=4, RIGHT=8, KEY=12, VALUE=16, COLOR=20, SENTINEL=21 };
static uint32_t read_word(uint32_t p) { return *(volatile uint32_t *)(uintptr_t)p; }
static void write_word(uint32_t p,uint32_t v) { *(volatile uint32_t *)(uintptr_t)p=v; }
static uint8_t read_byte(uint32_t p) { return *(volatile uint8_t *)(uintptr_t)p; }
static void write_byte(uint32_t p,uint8_t v) { *(volatile uint8_t *)(uintptr_t)p=v; }

void BFV_TC bfv_tree_previous(uint32_t *iterator)
{
    uint32_t slot=(uintptr_t)iterator, node=read_word(slot);
    if (read_byte(node+SENTINEL)) { write_word(slot,read_word(node+RIGHT)); return; }
    uint32_t child=read_word(node+LEFT);
    if (!read_byte(child+SENTINEL)) {
        uint32_t next=read_word(child+RIGHT);
        while (!read_byte(next+SENTINEL)) { child=next; next=read_word(child+RIGHT); }
        write_word(slot,child);
        return;
    }
    uint32_t parent=read_word(node+PARENT);
    while (!read_byte(parent+SENTINEL)) {
        if (read_word(slot)!=read_word(parent+LEFT)) break;
        write_word(slot,parent);
        parent=read_word(parent+PARENT);
    }
    /* Decrementing begin retains the highest visited node, as the binary does. */
    if (!read_byte(parent+SENTINEL)) write_word(slot,parent);
}

void BFV_TC bfv_tree_rotate_left(void *object,uint32_t pivot)
{
    uint32_t map=(uintptr_t)object, promoted=read_word(pivot+RIGHT);
    write_word(pivot+RIGHT,read_word(promoted+LEFT));
    uint32_t child=read_word(promoted+LEFT);
    if (!read_byte(child+SENTINEL)) write_word(child+PARENT,pivot);
    write_word(promoted+PARENT,read_word(pivot+PARENT));
    uint32_t header=read_word(map+4);
    if (pivot==read_word(header+PARENT)) write_word(header+PARENT,promoted);
    else {
        uint32_t parent=read_word(pivot+PARENT);
        if (pivot==read_word(parent+LEFT)) write_word(parent+LEFT,promoted);
        else write_word(parent+RIGHT,promoted);
    }
    write_word(promoted+LEFT,pivot);
    write_word(pivot+PARENT,promoted);
}

void BFV_TC bfv_tree_rotate_right(void *object,uint32_t pivot)
{
    uint32_t map=(uintptr_t)object, promoted=read_word(pivot+LEFT);
    write_word(pivot+LEFT,read_word(promoted+RIGHT));
    uint32_t child=read_word(promoted+RIGHT);
    if (!read_byte(child+SENTINEL)) write_word(child+PARENT,pivot);
    write_word(promoted+PARENT,read_word(pivot+PARENT));
    uint32_t header=read_word(map+4);
    if (pivot==read_word(header+PARENT)) write_word(header+PARENT,promoted);
    else {
        uint32_t parent=read_word(pivot+PARENT);
        if (pivot==read_word(parent+RIGHT)) write_word(parent+RIGHT,promoted);
        else write_word(parent+LEFT,promoted);
    }
    write_word(promoted+RIGHT,pivot);
    write_word(pivot+PARENT,promoted);
}

void *BFV_TC bfv_tree_construct(void *object,uint32_t left,uint32_t parent,
                              uint32_t right,const bfv_history_value *value,uint32_t color)
{
    uint32_t node=(uintptr_t)object, pair=(uintptr_t)value;
    write_word(node+LEFT,left);
    write_word(node+RIGHT,right);
    write_word(node+PARENT,parent);
    write_word(node+KEY,read_word(pair));
    write_word(node+VALUE,read_word(pair+4));
    write_byte(node+COLOR,(uint8_t)color);
    write_byte(node+SENTINEL,0);
    return object;
}

/* The inspected CRT string layout is 28 bytes: allocator byte, inline buffer,
 * size at 20, capacity at 24. Only the fields initialized by the native error
 * branch are written; padding is not made part of the recovered contract. */
static __attribute__((noreturn,noinline)) void too_many_nodes(void)
{
    uint32_t text[7], exception[10];
    write_word((uintptr_t)text+24,15);
    write_word((uintptr_t)text+20,0);
    write_byte((uintptr_t)text+4,0);
    ((void *(BFV_TC *)(void *,const char *,uint32_t))BFV_TREE_STRING_ASSIGN)
        (text,(const char *)BFV_TREE_LENGTH_MESSAGE,19);
    ((void *(BFV_TC *)(void *))BFV_TREE_EXCEPTION_CONSTRUCT)(exception);
    write_word((uintptr_t)exception,BFV_TREE_LOGIC_ERROR_VTABLE);
    write_word((uintptr_t)exception+36,15);
    write_word((uintptr_t)exception+32,0);
    write_byte((uintptr_t)exception+16,0);
    ((void *(BFV_TC *)(void *,const void *,uint32_t,uint32_t))BFV_TREE_STRING_COPY)
        ((uint8_t *)exception+12,text,0,UINT32_MAX);
    write_word((uintptr_t)exception,BFV_TREE_LENGTH_ERROR_VTABLE);
    ((void (__attribute__((stdcall)) *)(void *,const void *))BFV_VECTOR_THROW)
        (exception,(const void *)BFV_TREE_LENGTH_THROW_INFO);
    __builtin_unreachable();
}

uint32_t *BFV_TC bfv_history_insert_node(void *object,uint32_t *result,
    uint32_t left,uint32_t parent,const bfv_history_value *value)
{
    uint32_t map=(uintptr_t)object;
    if (read_word(map+8)>=0x1ffffffeu) too_many_nodes();
    uint32_t header=read_word(map+4);
    uint32_t node=((uint32_t (__attribute__((stdcall)) *)(uint32_t,uint32_t,uint32_t,
        const bfv_history_value *,uint32_t))BFV_TREE_ALLOCATE_NODE)(header,parent,header,value,0);
    write_word(map+8,read_word(map+8)+1);
    header=read_word(map+4); /* The allocation service may have changed the header. */
    if (parent==header) {
        write_word(header+PARENT,node);
        write_word(read_word(map+4)+LEFT,node);
        write_word(read_word(map+4)+RIGHT,node);
    } else if ((uint8_t)left) {
        write_word(parent+LEFT,node);
        header=read_word(map+4);
        if (parent==read_word(header+LEFT)) write_word(header+LEFT,node);
    } else {
        write_word(parent+RIGHT,node);
        header=read_word(map+4);
        if (parent==read_word(header+RIGHT)) write_word(header+RIGHT,node);
    }
    uint32_t current=node;
    while (!read_byte(read_word(current+PARENT)+COLOR)) {
        uint32_t father=read_word(current+PARENT), grand=read_word(father+PARENT);
        uint32_t uncle=read_word(grand+LEFT);
        if (father==uncle) {
            uncle=read_word(grand+RIGHT);
            if (read_byte(uncle+COLOR)) {
                if (current==read_word(father+RIGHT)) {
                    current=father; bfv_tree_rotate_left(object,current);
                }
                write_byte(read_word(current+PARENT)+COLOR,1);
                write_byte(read_word(read_word(current+PARENT)+PARENT)+COLOR,0);
                bfv_tree_rotate_right(object,read_word(read_word(current+PARENT)+PARENT));
            } else {
                write_byte(read_word(current+PARENT)+COLOR,1);
                write_byte(uncle+COLOR,1);
                write_byte(read_word(read_word(current+PARENT)+PARENT)+COLOR,0);
                current=read_word(read_word(current+PARENT)+PARENT);
            }
        } else if (read_byte(uncle+COLOR)) {
            if (current==read_word(father+LEFT)) {
                current=father; bfv_tree_rotate_right(object,current);
            }
            write_byte(read_word(current+PARENT)+COLOR,1);
            write_byte(read_word(read_word(current+PARENT)+PARENT)+COLOR,0);
            bfv_tree_rotate_left(object,read_word(read_word(current+PARENT)+PARENT));
        } else {
            write_byte(read_word(current+PARENT)+COLOR,1);
            write_byte(uncle+COLOR,1);
            write_byte(read_word(read_word(current+PARENT)+PARENT)+COLOR,0);
            current=read_word(read_word(current+PARENT)+PARENT);
        }
    }
    write_byte(read_word(read_word(map+4)+PARENT)+COLOR,1);
    write_word((uintptr_t)result,node);
    return result;
}

bfv_history_result *BFV_TC bfv_history_insert(void *object,bfv_history_result *result,
                                            const bfv_history_value *value)
{
    uint32_t map=(uintptr_t)object, pair=(uintptr_t)value;
    uint32_t parent=read_word(map+4), node=read_word(parent+PARENT), left=1;
    /* The key is captured once while descending; the native helper rereads it
     * after predecessor lookup when deciding whether this is a duplicate. */
    if (!read_byte(node+SENTINEL)) {
        uint32_t key=read_word(pair);
        do {
            parent=node;
            left=key<read_word(node+KEY);
            node=read_word(node+(left ? LEFT : RIGHT));
        } while (!read_byte(node+SENTINEL));
    }
    uint32_t candidate=parent;
    if (left) {
        if (parent==read_word(read_word(map+4)+LEFT)) {
            uint32_t *inserted=bfv_history_insert_node(object,&candidate,1,parent,value);
            node=read_word((uintptr_t)inserted);
            write_byte((uintptr_t)result+4,1);
            write_word((uintptr_t)result,node);
            return result;
        }
        bfv_tree_previous(&candidate);
    }
    if (read_word(candidate+KEY)<read_word(pair)) {
        uint32_t *inserted=bfv_history_insert_node(object,&candidate,left,parent,value);
        write_word((uintptr_t)result,read_word((uintptr_t)inserted));
        write_byte((uintptr_t)result+4,1);
    } else {
        write_byte((uintptr_t)result+4,0);
        write_word((uintptr_t)result,candidate);
    }
    return result;
}
