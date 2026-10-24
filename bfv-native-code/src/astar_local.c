/* AStarLocalSearch: the local A* search problem the searcher
 * (astar_searcher.c) asks for estimates, goal tests and the end points.
 *
 * A local position is x, y, z and a layer: x and z are ints, y is a float
 * height (the terrain height under x, z for the end points), and the layer
 * (+0x0c) doubles as a cell size: a node with layer n stands for the
 * 2^n x 2^n square its x and z round down to.
 *
 * Arithmetic is 32-bit and wraps as natively (abs via cdq/xor/sub).
 * Functions that natively return a byte through `mov al` or `setle al`
 * return the whole EAX they leave. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))
#define FASTCALL __attribute__((fastcall))

typedef struct LocalSearch {
    uint32_t vtable;            /* +0x00 */
    int32_t reach_radius;       /* +0x04, goal reached within this distance on x and on z */
    uint8_t unknown_08[0x10];
    uint32_t owner;             /* +0x18, its +0xc508 is the default layer */
    int32_t start[3];           /* +0x1c, x, y (float bits), z */
    int32_t goal[3];            /* +0x28, x, y (float bits), z */
    int32_t target[3];          /* +0x34, the exact point a goal-cell node snaps to */
    uint32_t goal_cell_x;       /* +0x40, goal x rounded down to the goal layer's cell */
    uint32_t unknown_44;        /* +0x44, cleared with the goal cell */
    uint32_t goal_cell_z;       /* +0x48 */
    int32_t start_layer;        /* +0x4c, looked up when negative */
    int32_t goal_layer;         /* +0x50, looked up when negative */
    uint8_t unknown_54[8];
    int32_t reach_sum;          /* +0x5c, goal reached when x + z distance is at most this */
    int32_t low[3];             /* +0x60, bounds for goal mode 3 */
    int32_t high[3];            /* +0x6c */
    uint8_t unknown_78[0x24];
    uint32_t goal_mode;         /* +0x9c, how the goal test works (0 to 3) */
    uint8_t use_bounds;         /* +0xa0, successors must lie within low..high */
    uint8_t unknown_a1[3];
    uint32_t out_nodes;         /* +0xa4, the successor nodes the searcher handed in */
    uint32_t out_costs;         /* +0xa8, their step costs (floats) */
    uint32_t out_count;         /* +0xac, successors written so far */
    uint32_t expanding;         /* +0xb0, the node whose successors are being generated */
    uint32_t unknown_b4;
    uint32_t avoid_begin;       /* +0xb8, keep-out areas: x, z, radius, sum (16 bytes each) */
    uint32_t avoid_end;         /* +0xbc */
} LocalSearch;
typedef volatile LocalSearch VLocal;

#define M32(a) (*(volatile uint32_t *)(uintptr_t)(a))
#define OWNER_DEFAULT_LAYER 0xc508

typedef uint32_t (FASTCALL *layer_fn)(uint32_t owner, volatile int32_t *point);
typedef float (TC *height_fn)(uint32_t terrain, float x, float z);
/* The float travels as its bits: a reload through the x87 would flag denormals. */
typedef void (TC *round_fn)(volatile int32_t *out, uint32_t value_bits);
typedef uint32_t (TC *within_fn)(volatile int32_t *point, volatile int32_t *low, volatile int32_t *high);

static uint32_t abs32(uint32_t value)
{
    uint32_t sign = (uint32_t)((int32_t)value >> 31);
    return (value ^ sign) - sign;
}

/* The terrain height at x, z: the terrain object's method 0x4c. */
static uint32_t ground_height(int32_t x, int32_t z)
{
    uint32_t terrain = M32(BFV_LOCAL_TERRAIN);
    volatile union { float value; uint32_t bits; } height;
    height.value = ((height_fn)M32(M32(terrain) + 0x4c))(terrain, (float)x, (float)z);
    return height.bits;
}

/* Natively both |a - b| and |b - a| go on the x87 stack and the smaller
 * stays, |b - a| rounded through a float (always stored, so its precision
 * flag is raised either way). The two only differ in name: they are equal. */
static int32_t axis_distance(int32_t from, int32_t to)
{
    uint32_t a = abs32((uint32_t)from - (uint32_t)to);
    uint32_t b = abs32((uint32_t)to - (uint32_t)from);
    volatile float b_rounded;
    __asm__ volatile("fildl %[b]; fstps %[f]" : [f]"=m"(b_rounded) : [b]"m"(b) : "st");
    if ((int32_t)b >= (int32_t)a)
        return (int32_t)a;
    uint32_t truncated;
    __asm__ volatile("flds %[v]; call *%[ftol]"
        : "=a"(truncated) : [v]"m"(b_rounded), [ftol]"r"(BFV_FTOL2) : "ecx", "edx", "memory", "st");
    return (int32_t)truncated;
}

/* 0x9e5180 / 0x798e10, vf2 (thiscall, ret 4): the estimate from a node to
 * the goal, 3 * |dy| (rounded) + |dx| + |dz|. */
uint32_t TC bfv_astar_local_estimate(VLocal *s, const volatile int32_t *node)
{
    volatile uint32_t dy_bits;
    __asm__ volatile("flds %[goal]; fsubs %[y]; fabs; fstps %[d]"
        : [d]"=m"(dy_bits) : [goal]"m"(s->goal[1]), [y]"m"(node[1]) : "st");
    volatile int32_t dy;
    ((round_fn)BFV_ROUND_TO_INT)(&dy, dy_bits);
    int32_t dx = axis_distance(s->goal[0], node[0]);
    int32_t dz = axis_distance(s->goal[2], node[2]);
    return (uint32_t)dy * 3 + (uint32_t)dz + (uint32_t)dx;
}

/* 0x9e63f0 / 0x79a080, vf4 (thiscall, ret 4): is the node at the goal?
 * Modes 0 to 2: a node whose cell holds the goal snaps to the exact target
 * and counts; otherwise it counts within reach_radius on each axis and
 * reach_sum in total. Mode 3: the node lies within low..high. Other modes
 * always count. */
uint32_t TC bfv_astar_local_reached(VLocal *s, volatile int32_t *node)
{
    uint32_t mode = s->goal_mode;
    if (mode > 3)
        return (mode & ~0xffu) | 1;
    if (mode == 3)
        return ((within_fn)BFV_ASTAR_STRATEGIC_WITHIN)(node, s->low, s->high);
    uint32_t layer = (uint32_t)node[3];
    if (layer) {
        uint32_t cell = ~((1u << (layer & 31)) - 1);
        if (s->goal_cell_x == ((uint32_t)node[0] & cell) && s->goal_cell_z == ((uint32_t)node[2] & cell)) {
            uint32_t x = (uint32_t)s->target[0];
            node[0] = (int32_t)x;
            node[1] = s->target[1];
            node[2] = s->target[2];
            return (x & ~0xffu) | 1;
        }
    }
    int32_t radius = s->reach_radius;
    uint32_t off_x = (uint32_t)node[0] - (uint32_t)s->target[0];
    uint32_t dx = abs32(off_x);
    if ((int32_t)dx > radius)
        return off_x & ~0xffu;
    uint32_t dz = abs32((uint32_t)node[2] - (uint32_t)s->target[2]);
    if ((int32_t)dz > radius)
        return dz & ~0xffu;
    uint32_t sum = dz + dx;
    return (sum & ~0xffu) | ((int32_t)sum <= s->reach_sum);
}

/* 0x9e5da0 / 0x799a30, vf6 (thiscall, ret 4): write the start as a node:
 * its layer (looked up once), x, ground height, z; the height is kept in
 * start[1]. x and z are reread after the height call. EAX is start z. */
uint32_t TC bfv_astar_local_set_start(VLocal *s, volatile int32_t *node)
{
    if (s->start_layer < 0)
        s->start_layer = (int32_t)((layer_fn)BFV_LOCAL_LAYER_AT)(s->owner, s->start);
    node[3] = s->start_layer;
    uint32_t y = ground_height(s->start[0], s->start[2]);
    int32_t x = s->start[0];
    int32_t z = s->start[2];
    node[0] = x;
    node[1] = (int32_t)y;
    node[2] = z;
    s->start[1] = (int32_t)y;
    return (uint32_t)z;
}

/* 0x9e5e10 / 0x799aa0, vf7 (thiscall, ret 4): the same for the goal. The
 * first time, the layer is looked up (the owner's default when that fails
 * with -1) and the goal's cell at that layer is kept for the goal test. */
uint32_t TC bfv_astar_local_set_goal(VLocal *s, volatile int32_t *node)
{
    if (s->goal_layer < 0) {
        int32_t layer = (int32_t)((layer_fn)BFV_LOCAL_LAYER_AT)(s->owner, s->goal);
        s->goal_layer = layer;
        if (layer == -1)
            s->goal_layer = (int32_t)M32(s->owner + OWNER_DEFAULT_LAYER);
        uint32_t cell = ~((1u << ((uint32_t)s->goal_layer & 31)) - 1);
        uint32_t x = (uint32_t)s->goal[0];
        uint32_t z = (uint32_t)s->goal[2];
        s->unknown_44 = 0;
        s->goal_cell_x = x & cell;
        s->goal_cell_z = z & cell;
    }
    node[3] = s->goal_layer;
    uint32_t y = ground_height(s->goal[0], s->goal[2]);
    int32_t x = s->goal[0];
    int32_t z = s->goal[2];
    node[0] = x;
    node[1] = (int32_t)y;
    node[2] = z;
    s->goal[1] = (int32_t)y;
    return (uint32_t)z;
}

/* Successor generation. A successor is the cell dx, dz cells away from the
 * node being expanded, at the node's layer. It is written straight into the
 * searcher's next free node (out_nodes[out_count]) and kept only if it lies
 * inside the world, the layer's cell map does not block it, it is within
 * low..high when use_bounds is set and (for the avoiding variant) outside
 * every keep-out area. A rejected cell is split: the two finer cells on the
 * near side are tried instead, down to the owner's default layer. */
#define OWNER_CELL_MAPS 0x3c        /* object whose +0x50 is a cell map per layer */
#define OWNER_LAYER_COSTS 0x48      /* int per layer, added to every step's cost */
#define WORLD_MASK_X (*(const volatile uint32_t *)BFV_LOCAL_WORLD_MASK_X)
#define WORLD_MASK_Z (*(const volatile uint32_t *)BFV_LOCAL_WORLD_MASK_Z)

typedef uint32_t (TC *blocked_fn)(uint32_t cell_map, volatile int32_t *node);
typedef float (TC *surface_fn)(uint32_t terrain, float x, float z, volatile float *normal);
typedef uint32_t (TC *estimate_fn)(VLocal *s, volatile int32_t *node);

static uint32_t shl32(uint32_t value, uint32_t count)
{
    return value << (count & 31);
}

static uint32_t cell_map(VLocal *s, uint32_t layer)
{
    uint32_t maps = M32(s->owner + OWNER_CELL_MAPS);
    return M32(M32(maps + 0x50) + layer * 4);
}

/* Inside a keep-out area: |dx| and |dz| below its radius and their sum below
 * its limit (all signed). */
static int in_keep_out(VLocal *s, const volatile int32_t *node)
{
    for (uint32_t entry = s->avoid_begin; entry != s->avoid_end; entry += 16) {
        const volatile int32_t *area = (const volatile int32_t *)entry;
        uint32_t dx = abs32((uint32_t)area[0] - (uint32_t)node[0]);
        if ((int32_t)dx >= area[2])
            continue;
        uint32_t dz = abs32((uint32_t)area[1] - (uint32_t)node[2]);
        if ((int32_t)dz < area[2] && (int32_t)(dz + dx) < area[3])
            return 1;
    }
    return 0;
}

/* Accept the cell written to `out` (x and z set) as the next successor of
 * `from` at `layer`: its ground height, its step cost
 * 16 * (3 * climb + 7 * (1 - |normal y|)^2), rounded, plus
 * 16 * (cell size + the layer's cost) as an unsigned float, its estimate
 * times 16 and its layer. */
static void accept_cell(VLocal *s, volatile int32_t *out, const volatile int32_t *from, uint32_t layer)
{
    union { uint32_t bits; float value; } from_y = { .bits = (uint32_t)from[1] };
    uint32_t terrain = M32(BFV_LOCAL_TERRAIN);
    volatile float normal[3];
    volatile union { float value; uint32_t bits; } height;
    height.value = ((surface_fn)M32(M32(terrain) + 0x5c))(terrain, (float)out[0], (float)out[2], normal);
    out[1] = (int32_t)height.bits;
    long double slope = 1.0L - __builtin_fabsl(normal[1]);
    long double climb = __builtin_fabsl((long double)from_y.value - height.value) * 3.0L;
    volatile union { float value; uint32_t bits; } terrain_cost;
    terrain_cost.value = (float)((climb + slope * slope * 7.0L) * 16.0L);
    volatile int32_t rounded;
    ((round_fn)BFV_ROUND_TO_INT)(&rounded, terrain_cost.bits);
    uint32_t layer_cost = M32(M32(s->owner + OWNER_LAYER_COSTS) + layer * 4);
    int32_t cost = (int32_t)(shl32(shl32(1, layer) + layer_cost, 4) + (uint32_t)rounded);
    long double cost_value = cost;
    if (cost < 0)
        cost_value += 4294967296.0L;
    *(volatile float *)(s->out_costs + s->out_count * 4) = (float)cost_value;
    volatile int32_t *added = (volatile int32_t *)M32(s->out_nodes + s->out_count * 4);
    uint32_t estimate = ((estimate_fn)M32(s->vtable + 8))(s, added);
    ((volatile int32_t *)M32(s->out_nodes + s->out_count * 4))[4] = (int32_t)(estimate << 4);
    ((volatile int32_t *)M32(s->out_nodes + s->out_count * 4))[3] = (int32_t)layer;
    s->out_count = s->out_count + 1;
}

static void add_cell(VLocal *s, const volatile int32_t *from, int32_t dx, int32_t dz, int avoiding)
{
    uint32_t layer = (uint32_t)from[3];
    volatile int32_t *out = (volatile int32_t *)M32(s->out_nodes + s->out_count * 4);
    uint32_t from_z = (uint32_t)from[2];
    if (layer == 0) {
        out[0] = (int32_t)((uint32_t)from[0] + (uint32_t)dx);
        out[2] = (int32_t)(from_z + (uint32_t)dz);
    } else {
        /* The centre of the neighbouring cell. */
        uint32_t half = shl32(1, layer - 1);
        uint32_t cell = ~(shl32(1, layer) - 1);
        out[0] = (int32_t)((cell & (uint32_t)from[0]) + half + shl32((uint32_t)dx, layer));
        out[2] = (int32_t)((cell & from_z) + half + shl32((uint32_t)dz, layer));
    }
    out[1] = 0;
    int rejected = ((uint32_t)out[0] & ~WORLD_MASK_X) || ((uint32_t)out[2] & ~WORLD_MASK_Z);
    if (!rejected) {
        uint32_t map = cell_map(s, layer);
        rejected = ((blocked_fn)M32(M32(map) + 0xc))(map, out) != 0
            || ((s->use_bounds & 0xff) && !(((within_fn)BFV_ASTAR_STRATEGIC_WITHIN)(out, s->low, s->high) & 0xff))
            || (avoiding && in_keep_out(s, out));
    }
    if (!rejected) {
        accept_cell(s, out, from, layer);
        return;
    }
    uint32_t owner = s->owner;
    if ((uint32_t)from[3] == M32(owner + OWNER_DEFAULT_LAYER))
        return;
    /* Split: the finer cell towards dx, dz, then the one beside it. */
    uint32_t finer = (uint32_t)from[3] - 1;
    uint32_t cell = ~(shl32(1, (uint32_t)from[3]) - 1);
    int32_t child[4];
    child[3] = (int32_t)finer;
    child[1] = 0;
    child[0] = (int32_t)((cell & (uint32_t)from[0]) + (dx > 0 ? shl32((uint32_t)dx, finer) : 0));
    child[2] = (int32_t)((cell & (uint32_t)from[2]) + (dz > 0 ? shl32((uint32_t)dz, finer) : 0));
    add_cell(s, child, dx, dz, avoiding);
    uint32_t side_x = dx > 0 ? shl32((uint32_t)dx, finer) : dz ? shl32(abs32((uint32_t)dz), finer) : 0;
    uint32_t side_z = dz > 0 ? shl32((uint32_t)dz, finer) : dx ? shl32(abs32((uint32_t)dx), finer) : 0;
    child[0] = (int32_t)((cell & (uint32_t)from[0]) + side_x);
    child[1] = 0;
    child[2] = (int32_t)((cell & (uint32_t)from[2]) + side_z);
    add_cell(s, child, dx, dz, avoiding);
}

/* 0x9e6ee0 / 0x79ab70 (thiscall, ret 0xc): add the successor dx, dz cells
 * from a node, splitting it when rejected. */
void TC bfv_astar_local_add_cell(VLocal *s, const volatile int32_t *from, int32_t dx, int32_t dz)
{
    add_cell(s, from, dx, dz, 0);
}

/* 0x9e7200 / 0x79ae90 (thiscall, ret 0xc): the same, also rejecting cells
 * in a keep-out area. */
void TC bfv_astar_local_add_cell_avoiding(VLocal *s, const volatile int32_t *from, int32_t dx, int32_t dz)
{
    add_cell(s, from, dx, dz, 1);
}

/* ---- Successor generation (vf3) ---- */

#define OWNER_MAX_LAYER 0xc50c      /* coarsest layer a successor may use */

typedef uint32_t (TC *default_map_fn)(uint32_t owner);

/* Native call whose result is not used: the blocked test of the owner's
 * default-layer cell map on a node. */
static void probe_default_map(VLocal *s, volatile int32_t *node)
{
    uint32_t map = ((default_map_fn)BFV_AI_INDEXED_OBJECT)(s->owner);
    ((blocked_fn)M32(M32(map) + 0xc))(map, node);
}

static void probe_last_added(VLocal *s)
{
    uint32_t map = ((default_map_fn)BFV_AI_INDEXED_OBJECT)(s->owner);
    volatile int32_t *last = (volatile int32_t *)M32(s->out_nodes + (s->out_count - 1) * 4);
    ((blocked_fn)M32(M32(map) + 0xc))(map, last);
}

/* One direction: first the coarsest cell, from the owner's maximum layer down
 * to just above the node's, that the node sits on the dx, dz edge of and that
 * is open; otherwise the neighbour at the node's own layer (add_cell). A
 * coarse cell inside a keep-out area stops the coarse search. */
static void expand_direction(VLocal *s, int32_t dx, int32_t dz, int avoiding)
{
    uint32_t before = s->out_count;
    volatile int32_t *out = (volatile int32_t *)M32(s->out_nodes + s->out_count * 4);
    const volatile int32_t *node = (const volatile int32_t *)s->expanding;
    uint32_t layer = (uint32_t)node[3];
    uint32_t fine_cell = ~(shl32(1, layer) - 1);
    uint32_t cell_x = fine_cell & (uint32_t)node[0];
    uint32_t cell_z = fine_cell & (uint32_t)node[2];
    uint32_t coarse = M32(s->owner + OWNER_MAX_LAYER);
    while (coarse > layer) {
        uint32_t cell = ~(shl32(1, coarse) - 1);
        int edge = 1;
        if (dx > 0)
            edge = cell_x == (cell & (uint32_t)node[0]) + shl32((uint32_t)dx, coarse) - shl32(1, layer);
        if (edge && dx < 0)
            edge = cell_x == (cell & (uint32_t)node[0]);
        if (edge && dz > 0)
            edge = cell_z == (cell & (uint32_t)node[2]) + shl32((uint32_t)dz, coarse) - shl32(1, layer);
        if (edge && dz < 0)
            edge = cell_z == (cell & (uint32_t)node[2]);
        if (edge) {
            uint32_t half = shl32(1, coarse - 1);
            uint32_t z = (uint32_t)node[2];
            out[0] = (int32_t)((cell & (uint32_t)node[0]) + half + shl32((uint32_t)dx, coarse));
            out[1] = 0;
            out[2] = (int32_t)((cell & z) + half + shl32((uint32_t)dz, coarse));
            uint32_t map = 0;
            int open = !((uint32_t)out[0] & ~WORLD_MASK_X) && !((uint32_t)out[2] & ~WORLD_MASK_Z)
                && (map = cell_map(s, coarse), ((blocked_fn)M32(M32(map) + 0xc))(map, out) == 0)
                && (!(s->use_bounds & 0xff)
                    || (((within_fn)BFV_ASTAR_STRATEGIC_WITHIN)(out, s->low, s->high) & 0xff));
            if (open) {
                if (avoiding && in_keep_out(s, out))
                    break;
                accept_cell(s, out, (const volatile int32_t *)s->expanding, coarse);
                probe_last_added(s);
                return;
            }
        }
        coarse--;
        node = (const volatile int32_t *)s->expanding;
        layer = (uint32_t)node[3];
    }
    add_cell(s, (const volatile int32_t *)s->expanding, dx, dz, avoiding);
    if (s->out_count != before)
        probe_last_added(s);
}

/* vf3 (0x9e7b80 / 0x79b810, thiscall, ret 0xc): write the successors of
 * `node` into the searcher's arrays and return how many. Goal mode 1 avoids
 * the keep-out areas; modes 0 and 2 do not; other modes add none. */
uint32_t TC bfv_astar_local_successors(VLocal *s, uint32_t node, uint32_t out_nodes, uint32_t out_costs)
{
    s->out_nodes = out_nodes;
    s->out_costs = out_costs;
    s->expanding = node;
    s->out_count = 0;
    probe_default_map(s, (volatile int32_t *)node);
    if (s->start_layer < 0)
        return 0;
    if (s->goal_layer < 0 && s->goal_mode != 2)
        return 0;
    uint32_t mode = s->goal_mode;
    if (mode > 2)
        return s->out_count;
    int avoiding = mode == 1;
    expand_direction(s, 0, 1, avoiding);
    expand_direction(s, 1, 0, avoiding);
    expand_direction(s, 0, -1, avoiding);
    expand_direction(s, -1, 0, avoiding);
    return s->out_count;
}
