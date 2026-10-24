/* The A* searcher's step: the time-sliced A* loop the local and strategic
 * search tasks run (client 0x00a09340, server 0x007bd990).
 *
 * The searcher keeps its nodes in a binary heap ordered by total cost (the
 * open set, 1-based, slot 0 unused), two hash tables chaining nodes by grid
 * position (open and closed), a doubly linked list of closed nodes, and a
 * scratch array of successor nodes and their float step costs. The problem
 * object (an AStarLocalSearch or AStarStrategicSearch) supplies the start and
 * goal, the estimate, the successors and the goal test through its vtable.
 *
 * A node is identified by x, z and id; the hash is (z * 31 + x * 17) modulo
 * the bucket count, made non-negative as cdq/xor/sub does. Arithmetic wraps
 * as natively. The step runs until the scheduler time it was given has
 * passed, the heap is empty, the step budget is used up, the goal is popped
 * or the node pool runs out, and returns the EAX the native code leaves: AL
 * is 1 while searching or when the goal was found, 0 when the search failed.
 *
 * The open heap's helpers (push, update, pop and its sift-down) and the
 * closed-list removal are reconstructed below. Equal totals are ordered by a
 * coin flip from the game's random generator, as natively. So are node
 * allocation, reset and the routines that hand the result out: the path
 * copy, the path into a ring of points (and the ring's extension) and the
 * closed-list points. The pool allocator and the vectors' growth stay
 * native. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))

typedef struct SearchNode SearchNode;
typedef volatile SearchNode VNode;
struct SearchNode {
    int32_t x;                  /* +0x00 */
    int32_t y;                  /* +0x04 */
    int32_t z;                  /* +0x08 */
    int32_t id;                 /* +0x0c, layer or cell, part of the identity */
    int32_t estimate;           /* +0x10, to the goal */
    uint32_t heap_index;        /* +0x14 */
    int32_t cost;               /* +0x18, from the start */
    int32_t total;              /* +0x1c, cost + estimate: the heap key */
    VNode *closed_prev;         /* +0x20 */
    VNode *closed_next;         /* +0x24 */
    VNode *hash_next;           /* +0x28 */
    VNode *parent;              /* +0x2c */
    VNode *child;               /* +0x30 */
};

typedef struct Searcher {
    uint32_t unknown_00;
    VNode *volatile *heap;      /* +0x04, begin; heap[1] is the cheapest */
    VNode *volatile *heap_end;  /* +0x08 */
    uint32_t unknown_0c;
    uint32_t pool[3];           /* +0x10, allocated nodes (vector) */
    uint32_t unknown_1c;
    uint32_t unknown_20;        /* +0x20 */
    VNode *volatile *open_hash; /* +0x24 */
    VNode *closed;              /* +0x28, most recently closed node */
    VNode *volatile *closed_hash; /* +0x2c */
    uint32_t allocated;         /* +0x30 */
    uint8_t unknown_34[0x24];
    int32_t buckets;            /* +0x58 */
    int32_t node_limit;         /* +0x5c */
    VNode *last;                /* +0x60, the node the step stopped at */
    VNode *start;               /* +0x64 */
    VNode *goal;                /* +0x68 */
    int32_t steps_left;         /* +0x6c */
    int32_t successors;         /* +0x70 */
    uint8_t started;            /* +0x74 */
    uint8_t unknown_75[3];
    int32_t cost_total;         /* +0x78, all step costs seen */
    int32_t successor_total;    /* +0x7c, all successors seen */
    VNode *successor[0x100];    /* +0x80 */
    float step_cost[0x100];     /* +0x480 */
    uint32_t problem;           /* +0x880 */
} Searcher;
typedef volatile Searcher VSearcher;
_Static_assert(__builtin_offsetof(Searcher, problem) == 0x880, "searcher layout");

typedef uint32_t (TC *object_fn)(uint32_t object);
typedef uint32_t (TC *object_arg_fn)(uint32_t object, uint32_t arg);
typedef int32_t (TC *successors_fn)(uint32_t problem, VNode *node, VNode *volatile *nodes,
                                    volatile float *costs);
typedef uint32_t (TC *searcher_fn)(VSearcher *searcher);
typedef VNode *(TC *alloc_fn)(VSearcher *searcher);
typedef uint32_t (TC *searcher_node_fn)(VSearcher *searcher, VNode *node);

static uint32_t method(uint32_t object, uint32_t slot)
{
    return (*(volatile uint32_t *volatile *)(uintptr_t)object)[slot];
}

/* The problem's methods by vtable slot. */
enum { PROBLEM_DELETE = 0, PROBLEM_ESTIMATE = 2, PROBLEM_SUCCESSORS = 3, PROBLEM_IS_GOAL = 4,
       PROBLEM_SET_START = 6, PROBLEM_SET_GOAL = 7 };

static uint32_t call_problem(VSearcher *s, uint32_t slot, VNode *node)
{
    uint32_t problem = s->problem;
    return ((object_arg_fn)method(problem, slot))(problem, (uint32_t)(uintptr_t)node);
}

/* The global log object's method 0xcc, called when a search fails. */
static uint32_t log_failure(void)
{
    uint32_t log = *(volatile uint32_t *)BFV_SEARCH_LOG;
    return ((object_fn)method(log, 0xcc / 4))(log);
}

static uint32_t bucket(VSearcher *s, int32_t x, int32_t z)
{
    int32_t mixed = (int32_t)((uint32_t)z * 31u + (uint32_t)x * 17u);
    uint32_t rest = (uint32_t)(mixed % s->buckets);
    uint32_t sign = (int32_t)rest < 0 ? ~0u : 0;
    return (rest ^ sign) - sign;
}

/* A node's bucket, reading z before x as natively. */
static uint32_t node_bucket(VSearcher *s, VNode *node)
{
    int32_t z = node->z;
    int32_t x = node->x;
    return bucket(s, x, z);
}

static void push_hash(VNode *volatile *volatile *table, uint32_t index, VNode *node)
{
    node->hash_next = (*table)[index];
    (*table)[index] = node;
}

/* The timer's method 3 leaves the time in ST0; the deadline is that plus the
 * time given, stored as a double. */
static void deadline_after(volatile double *deadline, uint32_t timer, const volatile double *time_left)
{
    uint32_t fn = method(timer, 3);
    __asm__ volatile("call *%[fn]; faddl %[time]; fstpl %[deadline]"
        : [deadline]"=m"(*deadline), "+c"(timer)
        : [fn]"r"(fn), [time]"m"(*time_left)
        : "eax", "edx", "memory", "st");
}

/* Time left: the timer's time compares below the deadline (FCOMP, then the
 * parity test of C0 and C2, so an unordered compare counts as no time). */
static uint8_t time_remains(uint32_t timer, const volatile double *deadline)
{
    uint32_t fn = method(timer, 3);
    uint16_t status;
    __asm__ volatile("call *%[fn]; fcompl %[deadline]; fnstsw %%ax"
        : "=a"(status), "+c"(timer)
        : [fn]"r"(fn), [deadline]"m"(*deadline)
        : "edx", "memory", "st");
    uint32_t flags = (status >> 8) & 5;
    return flags == 1 || flags == 4;
}

/* _ftol2 of a successor's float step cost. */
static int32_t step_cost(const volatile float *cost)
{
    int32_t value;
    __asm__ volatile("flds %[v]; call *%[ftol]"
        : "=a"(value) : [v]"m"(*cost), [ftol]"r"(BFV_FTOL2) : "ecx", "edx", "memory", "st");
    return value;
}

/* Heap entries: (end - begin) >> 2 (signed), or 0 without a vector. The heap
 * is 1-based, so the last entry is this minus one (unsigned). */
static uint32_t heap_size(VSearcher *s)
{
    VNode *volatile *heap = s->heap;
    if (!heap)
        return 0;
    return (uint32_t)((int32_t)((uint32_t)(uintptr_t)s->heap_end - (uint32_t)(uintptr_t)heap) >> 2);
}

/* The tie-break for equal totals: draw from the game's random generator
 * (thiscall on its state), v = ((r | 0x80) >> 7) + 1, and compare v * 2^-24
 * with 0.5 (FILD, FMUL, FCOMP). True when below, as the heap's other
 * comparisons read the status flags (C0 or C3 alone). */
static uint8_t tie_less(void)
{
    uint32_t state = BFV_SEARCH_RANDOM_STATE;
    volatile int32_t draw;
    uint16_t status;
    __asm__ volatile("call *%[fn]; orl $0x80, %%eax; sarl $7, %%eax; incl %%eax; movl %%eax, %[draw];"
                     "fildl %[draw]; fmuls %[scale]; fcomps %[half]; fnstsw %%ax"
        : "=&a"(status), [draw]"=m"(draw), "+c"(state)
        : [fn]"r"(BFV_SEARCH_RANDOM), [scale]"m"(*(const volatile float *)BFV_SEARCH_TIE_SCALE),
          [half]"m"(*(const volatile float *)BFV_SEARCH_TIE_HALF)
        : "edx", "memory", "st");
    uint32_t flags = (status >> 8) & 5;
    return flags == 1 || flags == 4;
}

/* Whether a total `a` sorts before `b`: signed less, or the coin flip when
 * they are equal. */
static uint8_t sorts_before(int32_t a, int32_t b)
{
    if (a == b)
        return tie_less();
    return a < b;
}

/* Take `node` out of its hash chain (no null checks: the node is in it). */
static void unhash(VNode *volatile *table, uint32_t index, VNode *node)
{
    VNode *volatile *slot = &table[index];
    VNode *n = *slot;
    if (n != node) {
        VNode *prev;
        do {
            prev = n;
            n = n->hash_next;
        } while (n != node);
        if (prev) {
            prev->hash_next = n->hash_next;
            n->hash_next = 0;
            return;
        }
    }
    *slot = n->hash_next;
    n->hash_next = 0;
}

typedef uint32_t (TC *push_back_fn)(VSearcher *searcher, const volatile uint32_t *value);

/* 0xa08e70 / 0x7bd540 (thiscall, ret 4): move an open node whose total fell
 * up the heap. */
void TC bfv_searcher_heap_update(VSearcher *s, VNode *node)
{
    uint32_t i = node->heap_index;
    if (i == 1)
        return;
    do {
        uint32_t up = i >> 1;
        if (!sorts_before(node->total, s->heap[up]->total))
            return;
        s->heap[i] = s->heap[up];
        s->heap[i]->heap_index = i;
        i = up;
        s->heap[i] = node;
        node->heap_index = i;
    } while (i != 1);
}

/* 0xa09090 / 0x7bd6e0 (thiscall, ret 4): open a node in the heap. A node
 * already at its heap index is only moved up; otherwise a slot is appended
 * (the native vector push_back of a zero) and the node rises from it. */
void TC bfv_searcher_heap_push(VSearcher *s, VNode *node)
{
    uint32_t index = node->heap_index;
    if (index != 0) {
        VNode *volatile *heap = s->heap;
        if (heap && index < heap_size(s) && heap[index] == node) {
            ((searcher_node_fn)BFV_SEARCHER_HEAP_UPDATE)(s, node);
            return;
        }
    }
    volatile uint32_t empty = 0;
    ((push_back_fn)BFV_SEARCHER_PUSH_BACK)(s, &empty);
    uint32_t i = heap_size(s) - 1;
    while (i != 1) {
        uint32_t up = i >> 1;
        if (!sorts_before(node->total, s->heap[up]->total))
            break;
        s->heap[i] = s->heap[up];
        s->heap[i]->heap_index = i;
        i = up;
    }
    s->heap[i] = node;
    node->heap_index = i;
}

/* 0xa08870 / 0x7bcde0 (thiscall, plain ret): drop heap[1] and refill the
 * heap from its last entry. Natively the walk never stops early: at each
 * level the cheaper child (ties by coin flip) either moves up, or takes the
 * moving entry's place and becomes the moving entry. */
void TC bfv_searcher_sift_down(VSearcher *s)
{
    s->heap[1]->heap_index = 0;
    VNode *volatile *begin = s->heap;
    VNode *volatile *end = s->heap_end;
    VNode *moving = end[-1];
    if (begin && (int32_t)((uint32_t)(uintptr_t)end - (uint32_t)(uintptr_t)begin) >> 2 != 0)
        s->heap_end = end - 1;
    moving->heap_index = 0;
    uint32_t i = 1;
    for (;;) {
        uint32_t child = i + i;
        if (child > heap_size(s) - 1)
            break;
        i = child;
        if (i + 1 <= heap_size(s) - 1) {
            VNode *left = s->heap[i];
            VNode *right = s->heap[i + 1];
            if (sorts_before(right->total, left->total))
                i = i + 1;
        }
        uint32_t up = i >> 1;
        if (sorts_before(s->heap[i]->total, moving->total)) {
            s->heap[up] = s->heap[i];
            s->heap[i]->heap_index = up;
        } else {
            s->heap[up] = moving;
            moving->heap_index = up;
            moving = s->heap[i];
        }
    }
    if (i <= heap_size(s) - 1) {
        s->heap[i] = moving;
        moving->heap_index = i;
    }
}

/* 0xa08a60 / 0x7bcfd0 (thiscall, plain ret): remove the cheapest node from
 * the heap and the open hash. */
void TC bfv_searcher_heap_pop(VSearcher *s)
{
    VNode *first = s->heap[1];
    ((searcher_fn)BFV_SEARCHER_SIFT_DOWN)(s);
    uint32_t index = node_bucket(s, first);
    unhash(s->open_hash, index, first);
}

/* 0xa089e0 / 0x7bcf50 (thiscall, ret 4): take a closed node out of the
 * closed hash and list, to reopen it. Without a previous node the list head
 * advances (natively the head's own successor, not the node's). */
void TC bfv_searcher_unclose(VSearcher *s, VNode *node)
{
    uint32_t index = node_bucket(s, node);
    unhash(s->closed_hash, index, node);
    VNode *prev = node->closed_prev;
    if (prev)
        prev->closed_next = node->closed_next;
    else
        s->closed = s->closed->closed_next;
    VNode *next = node->closed_next;
    if (next)
        next->closed_prev = node->closed_prev;
}

typedef uint32_t (TC *pool_free_fn)(uint32_t pool, uint32_t node);
typedef uint32_t (TC *resize_fn)(volatile void *vector, uint32_t count, uint32_t value);

/* 0xa09190 / 0x7bd7e0 (thiscall, plain ret; the naked entry below): forget
 * every node. Without allocated nodes nothing happens and EAX is what the
 * caller left (0 when the node vector is empty but present). Otherwise the
 * list at +0x20 and the closed list are emptied, both hashes cleared (the
 * bucket count reread each step), every node handed back to the pool, the
 * node vector resized to 0, the allocation count cleared and the heap
 * resized to its unused slot 0. EAX is the last resize's; the step's
 * failure path keeps its upper bytes. */
uint32_t bfv_searcher_reset_body(VSearcher *s, uint32_t eax)
{
    volatile uint32_t *pool = s->pool;
    uint32_t begin = pool[1];
    if (!begin)
        return eax;
    int32_t count = (int32_t)(pool[2] - begin) >> 2;
    if (count == 0)
        return 0;
    int32_t buckets = s->buckets;
    s->unknown_20 = 0;
    s->closed = 0;
    if (buckets > 0) {
        int32_t i = 0;
        do
            s->open_hash[i++] = 0;
        while (i < s->buckets);
    }
    if (s->buckets > 0) {
        int32_t i = 0;
        do
            s->closed_hash[i++] = 0;
        while (i < s->buckets);
    }
    uint32_t end = pool[2];
    for (uint32_t p = pool[1]; p != end; p += 4) {
        ((pool_free_fn)BFV_SEARCHER_POOL_FREE)(BFV_SEARCHER_NODE_POOL, *(volatile uint32_t *)(uintptr_t)p);
        end = pool[2];
    }
    ((resize_fn)BFV_SEARCHER_RESIZE)(pool, 0, 0);
    s->allocated = 0;
    return ((resize_fn)BFV_SEARCHER_RESIZE)(s, 1, 0);
}

/* The native entry: EAX passes through when nothing is allocated. */
__attribute__((naked)) void bfv_searcher_reset(void)
{
    __asm__ volatile("push %%eax; push %%ecx; call _bfv_searcher_reset_body; add $8, %%esp; ret" : : : "memory");
}

typedef void *(TC *pool_alloc_fn)(uint32_t pool, uint32_t size, uint32_t source, uint32_t flags);

/* 0xa092c0 / 0x7bd910 (thiscall, plain ret): a new node from the game's
 * pool allocator, or 0 once the searcher holds node_limit nodes (unsigned
 * compare) or the pool fails. The node is recorded in the searcher's node
 * vector (+0x10, the native push_back) and counted; its list links, parent
 * and id are cleared, the rest is left as the pool hands it out. */
VNode *TC bfv_searcher_alloc(VSearcher *s)
{
    volatile uint32_t *pool = s->pool;
    uint32_t begin = pool[1];
    uint32_t count = begin ? (uint32_t)((int32_t)(pool[2] - begin) >> 2) : 0;
    if (count >= (uint32_t)s->node_limit)
        return 0;
    VNode *node = ((pool_alloc_fn)BFV_COLLISION_ALLOCATE)(BFV_SEARCHER_NODE_POOL, sizeof(SearchNode),
                                                          BFV_COLLISION_ALLOC_SOURCE, 0);
    if (!node)
        return 0;
    uint32_t allocated = s->allocated;
    volatile uint32_t entry = (uint32_t)(uintptr_t)node;
    s->allocated = allocated + 1;
    ((push_back_fn)BFV_SEARCHER_PUSH_BACK)((VSearcher *)pool, &entry);
    node->closed_prev = 0;
    node->closed_next = 0;
    node->hash_next = 0;
    node->parent = 0;
    node->id = 0;
    return node;
}

/* A path point: a node's x, y, z. */
typedef struct PathPoint {
    int32_t x, y, z;
} PathPoint;
typedef volatile PathPoint VPoint;

static void copy_point(VPoint *to, VNode *node)
{
    to->x = node->x;
    to->y = node->y;
    to->z = node->z;
}

/* The nodes from `node` back to the start (parent links). */
static int32_t chain_length(VNode *node)
{
    int32_t count = 0;
    for (; node; node = node->parent)
        count++;
    return count;
}

/* 0xa08ad0 / 0x7bd040 (thiscall, ret 0Ch): copy the path to the node the
 * search stopped at into `points` (and its ids into `ids`, if given), start
 * first. A path longer than `max` loses its last nodes: the copy starts that
 * many parents up. Returns the points written. Natively a negative `max`
 * walks past the start. */
int32_t TC bfv_searcher_path(VSearcher *s, VPoint *points, int32_t max, volatile int32_t *ids)
{
    VNode *node = s->last;
    int32_t count = chain_length(node);
    if (count > max) {
        int32_t skip = count - max;
        count -= skip;
        do
            node = node->parent;
        while (--skip);
    }
    if (!node)
        return count;
    int32_t i = count;
    do {
        i--;
        copy_point(&points[i], node);
        if (ids)
            ids[i] = node->id;
        node = node->parent;
    } while (node);
    return count;
}

/* A ring of path points: capacity, head and tail indexes (the tail one past
 * the last point), and the point array. */
typedef struct PathRing {
    int32_t capacity;           /* +0x00 */
    int32_t head;               /* +0x04 */
    int32_t tail;               /* +0x08 */
    VPoint *points;             /* +0x0c */
} PathRing;
typedef volatile PathRing VRing;

/* An index taken modulo the capacity (signed idiv), made non-negative. */
static int32_t ring_wrap(int32_t index, int32_t capacity)
{
    int32_t rest = index % capacity;
    if (rest < 0)
        rest += capacity;
    return rest;
}

static int32_t ring_used(int32_t tail, int32_t head, int32_t capacity)
{
    return ring_wrap((int32_t)((uint32_t)tail - (uint32_t)head), capacity);
}

/* 0xa08440 / 0x7bca20 (thiscall, ret 4): extend the ring by `count` points,
 * at most up to the free space, and return how many. The tail is stored
 * once unwrapped and then wrapped. */
int32_t TC bfv_path_ring_extend(VRing *ring, int32_t count)
{
    int32_t head = ring->head;
    int32_t tail = ring->tail;
    int32_t capacity = ring->capacity;
    int32_t used = ring_used(tail, head, capacity);
    int32_t short_by = (int32_t)((uint32_t)capacity - (uint32_t)used - (uint32_t)count);
    if (short_by > 0)
        short_by = 0;
    int32_t added = (int32_t)((uint32_t)short_by + (uint32_t)count);
    int32_t moved = (int32_t)((uint32_t)ring->tail + (uint32_t)added);
    ring->tail = moved;
    ring->tail = ring_wrap(moved, capacity);
    return added;
}

typedef int32_t (TC *ring_extend_fn)(VRing *ring, int32_t count);

/* 0xa08b50 / 0x7bd0c0 (thiscall, ret 0Ch; the second argument is unused):
 * append the path to the node the search stopped at to a ring. The ring is
 * extended by the path length first; then each node, from the end of the
 * path back to the start, goes to the slot that many points before the
 * tail, if that slot lies between head and tail. Ids, if given, are stored
 * 1-based (ids[1] is the last node). Returns the path length. */
int32_t TC bfv_searcher_path_to_ring(VSearcher *s, VRing *ring, uint32_t unused, volatile int32_t *ids)
{
    (void)unused;
    VNode *node = s->last;
    int32_t count = chain_length(node);
    ((ring_extend_fn)BFV_PATH_RING_EXTEND)(ring, count);
    int32_t i = 0;
    for (; node; node = node->parent) {
        i++;
        if (ids)
            ids[i] = node->id;
        int32_t tail = ring->tail;
        int32_t head = ring->head;
        int32_t capacity = ring->capacity;
        int32_t back = (int32_t)((uint32_t)ring_used(tail, head, capacity) - (uint32_t)i);
        if (back >= capacity)
            continue;
        int32_t slot = ring_wrap((int32_t)((uint32_t)head + (uint32_t)back), capacity);
        if (head <= tail) {
            if (slot < head || slot >= tail)
                continue;
        } else if (slot < head && slot >= tail) {
            continue;
        }
        copy_point(&ring->points[slot], node);
    }
    return i;
}

/* 0xa08c60 / 0x7bd1d0 (thiscall, ret 0Ch): copy up to `max` points (and
 * ids, if given) from the closed list, most recently closed first, then from
 * the list at +0x20 (only ever cleared, by reset). Returns the count. */
int32_t TC bfv_searcher_closed_points(VSearcher *s, VPoint *points, int32_t max, volatile int32_t *ids)
{
    int32_t i = 0;
    for (VNode *node = s->closed; node && i < max; node = node->closed_next, i++) {
        copy_point(&points[i], node);
        if (ids)
            ids[i] = node->id;
    }
    for (VNode *node = (VNode *)(uintptr_t)s->unknown_20; node && i < max; node = node->closed_next, i++) {
        copy_point(&points[i], node);
        if (ids)
            ids[i] = node->id;
    }
    return i;
}

/* Start a search: take the problem from the job, reset, then allocate the
 * goal and start nodes, have the problem fill them, estimate the start and
 * put it into the open set. */
static void begin(VSearcher *s, uint32_t job)
{
    s->problem = ((object_fn)method(job, 1))(job);
    ((searcher_fn)BFV_SEARCHER_RESET)(s);
    s->started = 1;
    s->cost_total = 0;
    s->successor_total = 0;
    VNode *goal = ((alloc_fn)BFV_SEARCHER_ALLOC)(s);
    s->goal = goal;
    goal->id = 0;
    call_problem(s, PROBLEM_SET_GOAL, s->goal);
    s->goal->cost = 0;
    s->goal->estimate = 0;
    s->goal->total = 0;
    s->goal->parent = 0;
    VNode *start = ((alloc_fn)BFV_SEARCHER_ALLOC)(s);
    s->start = start;
    start->id = 0;
    call_problem(s, PROBLEM_SET_START, s->start);
    s->start->cost = 0;
    int32_t estimate = (int32_t)call_problem(s, PROBLEM_ESTIMATE, s->start);
    s->start->estimate = estimate;
    start = s->start;
    int32_t start_estimate = start->estimate;
    start->total = start->cost + start_estimate;
    s->start->parent = 0;

    /* The problem's step budget (+0x14), capped by the node limit, and its
     * successor array size (+0x10), whose slots start empty. */
    uint32_t problem = s->problem;
    int32_t steps = *(volatile int32_t *)(uintptr_t)(problem + 0x14);
    int32_t limit = s->node_limit;
    s->steps_left = steps;
    if (steps > limit)
        s->steps_left = limit;
    int32_t slots = *(volatile int32_t *)(uintptr_t)(problem + 0x10);
    s->successors = slots;
    if (slots > 0) {
        int32_t i = 0;
        do
            s->successor[i++] = 0;
        while (i < s->successors);
    }
    start = s->start;
    push_hash(&s->open_hash, node_bucket(s, start), start);
    ((searcher_node_fn)BFV_SEARCHER_HEAP_PUSH)(s, start);
}

/* Relax one successor of `current`: find it in the open or closed set, and
 * unless the known node is already cheaper take the new cost and parent,
 * then re-sort it in the heap, reopen it, or open the new node (whose slot
 * the searcher gives up). */
static void relax(VSearcher *s, VNode *current, VNode *volatile *slot, const volatile float *cost_slot)
{
    int32_t cost = step_cost(cost_slot);
    VNode *successor = *slot;
    int32_t z = successor->z, x = successor->x;
    uint32_t index = bucket(s, x, z);
    VNode *node = 0;
    uint8_t in_open = 0, in_closed = 0;
    for (VNode *n = s->open_hash[index]; n; n = n->hash_next)
        if (n->x == x && n->z == z && n->id == successor->id) {
            node = n;
            in_open = 1;
            break;
        }
    if (!node)
        for (VNode *n = s->closed_hash[index]; n; n = n->hash_next)
            if (n->x == x && n->z == z && n->id == successor->id) {
                node = n;
                in_closed = 1;
                break;
            }
    if (!node)
        node = successor;

    int32_t total = s->cost_total;
    int32_t seen = s->successor_total;
    s->successor_total = seen + 1;
    s->cost_total = (int32_t)((uint32_t)total + (uint32_t)cost);
    int32_t reached = (int32_t)((uint32_t)current->cost + (uint32_t)cost);
    if ((in_open || in_closed) && node->cost < reached)
        return;
    node->parent = current;
    current->child = node;
    node->cost = reached;
    int32_t estimate = (*slot)->estimate;
    node->estimate = estimate;
    node->total = (int32_t)((uint32_t)estimate + (uint32_t)reached);
    node->id = (*slot)->id;
    if (in_open) {
        ((searcher_node_fn)BFV_SEARCHER_HEAP_UPDATE)(s, node);
        return;
    }
    if (in_closed)
        ((searcher_node_fn)BFV_SEARCHER_UNCLOSE)(s, node);
    else
        *slot = 0;
    push_hash(&s->open_hash, node_bucket(s, node), node);
    ((searcher_node_fn)BFV_SEARCHER_HEAP_PUSH)(s, node);
}

/* Move an expanded node to the closed list and the closed hash. */
static void close_node(VSearcher *s, VNode *node)
{
    node->closed_next = s->closed;
    VNode *head = s->closed;
    if (head)
        head->closed_prev = node;
    s->closed = node;
    push_hash(&s->closed_hash, node_bucket(s, node), node);
}

/* 0xa09340 / 0x7bd990 (thiscall, ret 0x10). `running` ends 1 while the
 * search still needs time. */
uint32_t TC bfv_searcher_step(VSearcher *s, uint32_t job, uint8_t in_progress, volatile uint8_t *running,
                              const volatile double *time_left)
{
    uint32_t timer = *(volatile uint32_t *)BFV_SEARCH_TIMER;
    *running = 0;
    uint8_t goal_found = 0;
    volatile double deadline;
    deadline_after(&deadline, timer, time_left);
    /* Natively this flag lives in the time pointer's argument slot and is
     * only ever written as a byte, so before the first expansion it is the
     * pointer's low byte. */
    uint8_t out_of_nodes = (uint8_t)(uintptr_t)time_left;
    if (!in_progress)
        begin(s, job);

    VNode *current = s->start;
    for (;;) {
        uint8_t remains = time_remains(timer, &deadline);
        *running = remains;
        if (!remains)
            break;
        VNode *volatile *heap = s->heap;
        if (!heap)
            break;
        if ((uint32_t)((int32_t)((uint32_t)(uintptr_t)s->heap_end - (uint32_t)(uintptr_t)heap) >> 2) < 2)
            break;
        int32_t steps = s->steps_left - 1;
        s->steps_left = steps;
        if (steps <= 0)
            break;
        VNode *best = heap[1];
        current = best;
        ((searcher_fn)BFV_SEARCHER_HEAP_POP)(s);
        goal_found = (uint8_t)call_problem(s, PROBLEM_IS_GOAL, best);
        if (goal_found)
            break;

        /* Refill the successor slots the last expansion gave up. */
        out_of_nodes = 0;
        for (int32_t i = 0; i < s->successors; i++) {
            if (!s->successor[i]) {
                VNode *node = ((alloc_fn)BFV_SEARCHER_ALLOC)(s);
                s->successor[i] = node;
                if (!node)
                    out_of_nodes = 1;
            }
            if (out_of_nodes)
                goto stopped;
        }

        uint32_t problem = s->problem;
        int32_t count = ((successors_fn)method(problem, PROBLEM_SUCCESSORS))(problem, best, s->successor,
                                                                              s->step_cost);
        s->successors = count;
        if (count > 0)
            for (int32_t i = 0; i < s->successors; i++)
                relax(s, best, &s->successor[i], &s->step_cost[i]);
        close_node(s, best);
    }
stopped:;
    uint8_t was_running = *running;
    *running = was_running == 0;
    s->last = current;
    if (*running)
        return ((uint32_t)(uintptr_t)current & ~0xffu) | 1;

    uint32_t problem = s->problem;
    if (problem)
        ((object_arg_fn)method(problem, PROBLEM_DELETE))(problem, 1);
    s->problem = 0;
    uint32_t eax = log_failure();
    if (goal_found)
        return (eax & ~0xffu) | 1;
    if (s->allocated == (uint32_t)s->node_limit)
        log_failure();
    if (s->steps_left == 0)
        log_failure();
    if (out_of_nodes)
        log_failure();
    return ((searcher_fn)BFV_SEARCHER_RESET)(s) & ~0xffu;
}

/* 0xa09880 / 0x7bded0 (thiscall, ret 4): run a search job to completion.
 * Readable form:
 *
 *     double time_left = SEARCH_UNLIMITED_TIME;   // 10000000.0 seconds
 *     return bfv_searcher_step(s, job, 0, (uint8_t *)&job, &time_left);
 *
 * The step's `running` flag lands in the job's own argument slot. The frame
 * is spelled out because the step seeds its out-of-nodes flag from the low
 * byte of the time pointer, so `time_left` must sit at the same stack address
 * as natively: 8 bytes below the return address. */
__attribute__((naked)) void bfv_searcher_run(void)
{
    __asm__ volatile(
        "sub $8, %%esp;"
        "fldl %c0;"
        "lea (%%esp), %%eax; push %%eax;"     /* &time_left */
        "fstpl 4(%%esp);"                     /* time_left = unlimited */
        "mov 0x10(%%esp), %%eax;"             /* job */
        "lea 0x10(%%esp), %%edx; push %%edx;" /* running -> job's slot */
        "push $0; push %%eax;"                /* in_progress = 0, job */
        "call %c1;"                           /* step(s in ecx, ...) */
        "add $8, %%esp; ret $4;"
        : : "i"(BFV_SEARCH_UNLIMITED_TIME), "i"(BFV_SEARCHER_STEP) : "memory");
}

/* A std::deque of node pointers, four per block (the path is gathered from
 * the goal back, then taken from the back). Its push_back and destructor
 * stay native. */
typedef struct NodeDeque {
    uint32_t allocator;         /* +0x00, empty; never written natively */
    uint32_t *volatile *map;    /* +0x04, block pointers */
    uint32_t map_size;          /* +0x08 */
    uint32_t offset;            /* +0x0c, index of the front element */
    uint32_t size;              /* +0x10 */
} NodeDeque;
typedef volatile NodeDeque VDeque;
typedef uint32_t (TC *deque_push_fn)(VDeque *deque, const volatile uint32_t *value);
typedef uint32_t (TC *deque_fn)(VDeque *deque);

/* What the delivery callback receives: the node's x, id and z. */
typedef struct PathStep {
    int32_t x, id, z;
} PathStep;

static VNode *deque_back(VDeque *path, uint32_t size)
{
    uint32_t index = path->offset + size - 1;
    uint32_t block = index >> 2;
    if (path->map_size <= block)
        block -= path->map_size;
    return (VNode *)path->map[block][index & 3];
}

/* 0xa098b0 / 0x7bdf00 (thiscall, ret 4): hand the found path to a callback,
 * start first (the callback object's method 0 gets a PathStep), then reset
 * the searcher. Returns the path length. */
uint32_t TC bfv_searcher_deliver(VSearcher *s, uint32_t callback)
{
    VDeque path;
    path.map = 0;
    path.map_size = 0;
    path.offset = 0;
    path.size = 0;
    volatile uint32_t node = (uint32_t)(uintptr_t)s->last;
    while (node) {
        ((deque_push_fn)BFV_PATH_DEQUE_PUSH_BACK)(&path, &node);
        node = (uint32_t)(uintptr_t)((VNode *)(uintptr_t)node)->parent;
    }
    uint32_t count = path.size;
    if (count) {
        uint32_t size = count;
        for (;;) {
            VNode *back = deque_back(&path, size);
            int32_t z = back->z;
            volatile PathStep step;
            step.x = back->x;
            step.id = back->id;
            step.z = z;
            ((object_arg_fn)method(callback, 0))(callback, (uint32_t)(uintptr_t)&step);
            /* pop_back */
            size = path.size;
            if (!size)
                break;
            path.size = --size;
            if (size)
                continue;
            path.offset = 0;
            break;
        }
    }
    ((searcher_fn)BFV_SEARCHER_RESET)(s);
    ((deque_fn)BFV_PATH_DEQUE_DESTROY)(&path);
    return count;
}
