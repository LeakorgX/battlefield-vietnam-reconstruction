/* AICollisionHandler slot 1, reconstructed from instructions after the raw
 * decompiler failed. Registry/event services are retained native dependencies.
 * Field/method names describe observed use, not restored original type names.
 */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))

typedef struct { uint32_t x, y, z; } vector_bits;
_Static_assert(sizeof(vector_bits) == 12, "Native vectors contain three words");
typedef uint32_t (TC *getter)(void *);
typedef uint32_t (TC *lookup)(void *, uint32_t);
typedef void (TC *set_float)(void *, float);
typedef void (TC *attach_event)(void *, void *, uint32_t);
typedef void (TC *collision_event)(void *, uint32_t, void *, uint32_t);
typedef long double (TC *clock_value)(void *);
typedef void *(TC *allocate_event)(void *, uint32_t, uint32_t, uint32_t);
typedef void *(TC *construct_event)(void *, vector_bits, vector_bits, void *, uint32_t, float, float, uint32_t);
typedef long double (__attribute__((stdcall)) *collision_distance)(vector_bits, vector_bits, vector_bits);

volatile uint32_t bfv_collision_calls;

static uint32_t read32(uintptr_t address) { return *(volatile uint32_t *)address; }
static uintptr_t method(void *object, uint32_t offset)
{ return read32(read32((uintptr_t)object) + offset); }
static uint32_t get(void *object, uint32_t offset)
{ return ((getter)method(object, offset))(object); }
static uint32_t find(void *object, uint32_t offset, uint32_t value)
{ return ((lookup)method(object, offset))(object, value); }
static void *global_object(uintptr_t address) { return (void *)(uintptr_t)read32(address); }
static void *body_component(void *object)
{
    uintptr_t view = ((getter)BFV_COLLISION_BODY_VIEW)(object);
    return (void *)(uintptr_t)read32(view + 0x5c);
}
static vector_bits copy_vector(uintptr_t address)
{
    vector_bits result = {read32(address), read32(address + 4), read32(address + 8)};
    return result;
}

/* Keep this boundary visible for ABI traces and debugger inspection. */
void TC __attribute__((noinline)) bfv_collision_dispatch(void *handler, void *member,
                          void *component, uint32_t position, uint32_t selector, uint32_t flags)
{
    (void)handler;
    vector_bits origin = copy_vector(get(component, 0x18));
    void *actors = global_object(BFV_COLLISION_ACTORS);
    uint32_t index = 0;
    if (!get(actors, 4)) return;
    do {
        void *actor = (void *)(uintptr_t)find(actors, 0x14, index);
        if (actor) {
            uint32_t member_id = get(member, 0x44);
            if (member_id != get(actor, 0xa8) && selector != get(actor, 0xa8) &&
                get(actor, 0xa8) != 0xffffffffu && !(uint8_t)get(actor, 0xc4)) {
                uint32_t handle = get(actor, 0xcc);
                uint32_t slot = handle & 0xffffu;
                uintptr_t entity = 0;
                if (slot) {
                    uintptr_t record = read32(read32(BFV_OBJECT_POOL)) + slot * 8 - 8;
                    if (*(volatile uint16_t *)(record + 6) == (uint16_t)(handle >> 16))
                        entity = read32(record);
                }
                if (entity) {
                    void *parent = (void *)(uintptr_t)read32(entity + 0x20);
                    void *actor_object = (void *)(uintptr_t)get(parent, 0x14);
                    uintptr_t actor_position = get(actor_object, 0x34);
                    vector_bits contact = copy_vector(position);
                    vector_bits candidate = copy_vector(actor_position);
                    long double distance = ((collision_distance)BFV_COLLISION_DISTANCE)(candidate, origin, contact);
                    if (distance < *(volatile float *)BFV_COLLISION_DISTANCE_LIMIT) {
                        uintptr_t component_vtable = read32((uintptr_t)component);
                        uint32_t actor_selector = get(actor, 0xd4);
                        uint32_t resolved = ((lookup)read32(component_vtable + 0x5c))(component, actor_selector);
                        void *pool_entry = (void *)(uintptr_t)((lookup)BFV_COLLISION_POOL_ENTRY)(global_object(BFV_OBJECT_POOL), resolved);
                        void *clock = global_object(BFV_COLLISION_CLOCK);
                        uintptr_t pool_vtable = read32((uintptr_t)pool_entry);
                        volatile float time = ((clock_value)method(clock, 4))(clock);
                        ((set_float)read32(pool_vtable + 0x0c))(pool_entry, time);
                        void *event_interface = (void *)(uintptr_t)((lookup)BFV_COLLISION_EVENT_INTERFACE)(pool_entry, 3);
                        if (event_interface) {
                            void *buffer = ((allocate_event)BFV_COLLISION_ALLOCATE)((void *)BFV_COLLISION_ALLOCATOR, 0x38, BFV_COLLISION_ALLOC_SOURCE, 0);
                            void *event = 0;
                            if (buffer) {
                                float strength = (uint8_t)flags ? 4.0f : 1.0f;
                                clock = global_object(BFV_COLLISION_CLOCK);
                                void *event_data = (void *)(uintptr_t)read32((uintptr_t)event_interface + 8);
                                uintptr_t current_origin = get(component, 0x18);
                                volatile float timestamp = ((clock_value)method(clock, 4))(clock);
                                vector_bits contact_vector = copy_vector(position);
                                vector_bits origin_vector = copy_vector(current_origin);
                                event = ((construct_event)BFV_COLLISION_CONSTRUCT)(buffer, origin_vector, contact_vector, event_data, 0, timestamp, strength, flags);
                            }
                            ((attach_event)method(actor, 0x144))(actor, event, 0xffffffffu);
                        }
                    }
                }
            }
        }
        ++index;
    } while (index < get(actors, 4));
}

/* Native notification helper, previously 009d4b60 / 0078a4c0. The position
 * getter takes no arguments: selector/flag pushes prepare the later dispatcher.
 * SETE DL changes only the flag word's low byte; retain the remaining bits. */
void TC bfv_collision_notify(void *handler, void *member, void *component,
                             void *source, uint32_t selector)
{
    uintptr_t interface = read32((uintptr_t)source + 0x4c);
    uint32_t state = read32(interface + BFV_COLLISION_NOTIFY_STATE_FIELD);
    uint32_t flag_word = (state & 0xffffff00u) | (state == 1);
    uint32_t position = get(source, 0x34);
    bfv_collision_dispatch(handler, member, component, position, selector, flag_word);
}

void TC bfv_collision(void *handler, void *source, void *other, uint32_t payload)
{
    ++bfv_collision_calls;
    if (other && !source) return;
    void *source_interface = (void *)(uintptr_t)read32((uintptr_t)source + 0x4c);
    if (!find(source_interface, 0x2c, read32(BFV_COLLISION_INTERFACE_ID))) {
        if (other) {
            void *other_interface = (void *)(uintptr_t)read32((uintptr_t)other + 0x4c);
            find(other_interface, 0x2c, read32(BFV_COLLISION_INTERFACE_ID));
        }
        return;
    }
    uint32_t source_handle = read32((uintptr_t)source + BFV_COLLISION_HANDLE_FIELD);
    void *member = (void *)(uintptr_t)find(global_object(BFV_COLLISION_REGISTRY), 0x1c, source_handle);
    if (!member) return;
    void *member_body = (void *)(uintptr_t)get(member, 0x38);
    if (!member_body) return;
    void *source_component = body_component(member_body);
    if (!source_component) return;
    uint32_t event_selector = 0xffffffffu;
    if (!other) {
        bfv_collision_notify(handler, member, source_component, source, event_selector);
        return;
    }

    void *other_component = body_component(other);
    void *source_actor = (void *)(uintptr_t)find(global_object(BFV_COLLISION_ACTORS), 0x1c, source_handle);
    uint8_t active_source = source_actor && !(uint8_t)get(source_actor, 0xc4);
    if (other_component) {
        void *other_actor = (void *)(uintptr_t)find(global_object(BFV_COLLISION_ACTORS), 0x10, (uint32_t)(uintptr_t)other_component);
        if (other_actor && !(uint8_t)get(other_actor, 0xc4)) {
            /* Capture the vtable pointer before the callback, then read the
             * method slot afterward, matching native mutation visibility. */
            uintptr_t source_vtable = read32((uintptr_t)source_component);
            uint32_t actor_selector = get(other_actor, 0xd4);
            lookup resolve_source = (lookup)read32(source_vtable + 0x5c);
            uint32_t handle = resolve_source(source_component, actor_selector);
            void *pool_entry = (void *)(uintptr_t)((lookup)BFV_COLLISION_POOL_ENTRY)(global_object(BFV_OBJECT_POOL), handle);
            void *clock = global_object(BFV_COLLISION_CLOCK);
            uintptr_t pool_vtable = read32((uintptr_t)pool_entry);
            volatile float time = ((clock_value)method(clock, 4))(clock);
            set_float update_time = (set_float)read32(pool_vtable + 0x0c);
            update_time(pool_entry, time);
            void *event_interface = (void *)(uintptr_t)((lookup)BFV_COLLISION_EVENT_INTERFACE)(pool_entry, 3);
            if (event_interface) {
                void *buffer = ((allocate_event)BFV_COLLISION_ALLOCATE)((void *)BFV_COLLISION_ALLOCATOR, 0x38, BFV_COLLISION_ALLOC_SOURCE, 0);
                void *event = 0;
                if (buffer) {
                    void *event_data = (void *)(uintptr_t)read32((uintptr_t)event_interface + 8);
                    clock = global_object(BFV_COLLISION_CLOCK);
                    uintptr_t source_position = get(source, 0x34);
                    uintptr_t member_position = get(member_body, 0x34);
                    volatile float timestamp = ((clock_value)method(clock, 4))(clock);
                    vector_bits source_vector = copy_vector(source_position);
                    vector_bits member_vector = copy_vector(member_position);
                    event = ((construct_event)BFV_COLLISION_CONSTRUCT)(buffer, member_vector, source_vector, event_data, 1, timestamp, 1.0f, 0);
                }
                ((attach_event)method(other_actor, 0x144))(other_actor, event, handle);
                event_selector = get(other_actor, 0xa8);
            }
        }
        if (active_source) {
            uintptr_t other_vtable = read32((uintptr_t)other_component);
            uint32_t actor_selector = get(source_actor, 0xd4);
            lookup resolve_other = (lookup)read32(other_vtable + 0x5c);
            uint32_t handle = resolve_other(other_component, actor_selector);
            uintptr_t record = find(source_actor, 0x88, handle);
            if (record) ++*(volatile uint32_t *)(record + 0x30);
        }
    }
    if (active_source) {
        uint32_t field = read32((uintptr_t)source + 0x48);
        ((collision_event)method(source_actor, 0x148))(source_actor, field, other, payload);
    }
    bfv_collision_notify(handler, member, source_component, source, event_selector);
}
