/* AStarStrategicSearch: setup, the run that builds the disc mask, neighbour
 * expansion over the strategic grid used by bot pathfinding, and the
 * strategic-grid queries it needs.
 *
 * Positions are three ints: x, layer, z. Each grid cell covers 64 x 64
 * position units (x >> 6, z >> 6) and is 16 bytes:
 *
 *   +0x00 word  links in +z: 4 bits per layer, bit n = link to layer n
 *   +0x02 word  links in +x, same format
 *   +0x04 + 2*layer  byte: low 6 bits of that layer's x inside the cell
 *   +0x05 + 2*layer  byte: low 6 bits of that layer's z inside the cell
 *   +0x0c byte  high nibble: bit 4 + layer is set when the layer exists
 *
 * A link in -z or -x is stored in the neighbouring cell, from that cell's
 * layer back to ours, so those tests read the neighbour with the two layers
 * swapped and first check that the neighbour lies inside the grid masks.
 *
 * Names describe how these routines use each field. Arithmetic is 32-bit
 * and wraps as natively (cell index, abs via cdq/xor/sub). Functions that
 * natively return a byte through `xor al, al` return the whole eax they
 * leave, so callers that read eax see the same value. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))

#define M8(a)  (*(volatile uint8_t *)(uintptr_t)(a))
#define M16(a) (*(volatile uint16_t *)(uintptr_t)(a))
#define M32(a) (*(volatile uint32_t *)(uintptr_t)(a))

typedef struct StrategicGrid {
    uint8_t unknown_00[0x1c];
    uint32_t cells;             /* +0x1c, 16-byte cells */
    uint8_t unknown_20[8];
    uint32_t row_shift;         /* +0x28, cell index = (z >> 6 << row_shift) + (x >> 6) */
} StrategicGrid;

typedef struct StrategicSearch {
    uint32_t vtable;            /* +0x00 */
    int32_t reach_distance;     /* +0x04, goal reached within this x + z distance */
    uint32_t blocked_cost;      /* +0x08, returned for a neighbour that can't be entered */
    uint32_t unknown_0c;        /* +0x0c */
    uint32_t unknown_10;        /* +0x10, setup stores 0x20 */
    uint32_t unknown_14;        /* +0x14, setup stores 0x41fc */
    uint32_t owner;             /* +0x18, its +0x40 is the strategic grid */
    int32_t low[3];             /* +0x1c, search bounds; ignored on an axis where low == high */
    int32_t high[3];            /* +0x28 */
    uint32_t start[3];          /* +0x34 */
    int32_t goal[3];            /* +0x40 */
} StrategicSearch;

static uint32_t cell_address(volatile StrategicGrid *grid, int32_t x, int32_t z)
{
    uint32_t index = ((uint32_t)(z >> 6) << (grid->row_shift & 31)) + (uint32_t)(x >> 6);
    return index * 16 + grid->cells;
}

/* (1 << layer) << 4 as the byte the native test uses. */
static uint8_t layer_flag(uint32_t layer) { return (uint8_t)((1u << (layer & 31)) << 4); }

static uint32_t abs32(uint32_t value)
{
    uint32_t sign = (uint32_t)((int32_t)value >> 31);
    return (value ^ sign) - sign;
}

/* Native 0x9cf470 / 0x784e60: a and b are in the same cell, a's layer
 * exists there and both are on the same layer. Different cells return the
 * cell address of a with the low byte cleared. */
uint32_t TC bfv_strategic_same_cell(volatile StrategicGrid *grid, const volatile int32_t *a,
                                    const volatile int32_t *b)
{
    uint32_t cell_a = cell_address(grid, a[0], a[2]);
    uint32_t cell_b = cell_address(grid, b[0], b[2]);
    if (cell_a != cell_b) return cell_a & ~0xffu;
    uint32_t layer = (uint32_t)a[1];
    if ((M8(cell_a + 0xc) & layer_flag(layer)) && layer == (uint32_t)b[1]) return 1;
    return 0;
}

/* Native 0x9d1330 / 0x786b30: the point of `layer` in p's cell. x and z keep
 * p's cell bits and take their low 6 bits from the cell. */
volatile int32_t *TC bfv_strategic_cell_point(volatile StrategicGrid *grid, volatile int32_t *out,
                                              const volatile int32_t *p, uint32_t layer)
{
    uint32_t z = (uint32_t)p[2], x = (uint32_t)p[0];
    uint32_t cell = cell_address(grid, (int32_t)x, (int32_t)z);
    uint32_t point_x = x ^ ((M8(cell + layer * 2 + 4) ^ x) & 0x3f);
    uint32_t point_z = ((M8(cell + layer * 2 + 5) ^ z) & 0x3f) ^ z;
    out[0] = (int32_t)point_x;
    out[1] = (int32_t)layer;
    out[2] = (int32_t)point_z;
    return out;
}

/* Native 0x9de9c0 / 0x792620: p's cell links in +z from `from` to `to`. */
uint32_t TC bfv_strategic_link_z(volatile StrategicGrid *grid, const volatile int32_t *p,
                                 uint32_t from, uint32_t to)
{
    uint32_t links = M32(cell_address(grid, p[0], p[2]));
    return ((links >> ((from << 2) & 31)) & (1u << (to & 31))) != 0;
}

/* Native 0x9dea10 / 0x792670: p's cell links in +x from `from` to `to`. */
uint32_t TC bfv_strategic_link_x(volatile StrategicGrid *grid, const volatile int32_t *p,
                                 uint32_t from, uint32_t to)
{
    uint32_t links = M16(cell_address(grid, p[0], p[2]) + 2);
    return ((links >> ((from << 2) & 31)) & (1u << (to & 31))) != 0;
}

#define X_MASK (*(const volatile uint32_t *)BFV_STRATEGIC_X_MASK)
#define Z_MASK (*(const volatile uint32_t *)BFV_STRATEGIC_Z_MASK)

/* Native 0x9dea60 / 0x7926c0: the cell at z - 64 exists, has layer `to`
 * and links in +z from `to` back to `from`. p is passed by value. */
uint32_t TC bfv_strategic_link_back_z(volatile StrategicGrid *grid, int32_t x, int32_t layer,
                                      int32_t z, uint32_t from, uint32_t to)
{
    (void)layer;
    uint32_t below = (uint32_t)z - 0x40;
    if ((uint32_t)x & ~X_MASK) return below & ~0xffu;
    if (below & ~Z_MASK) return below & ~0xffu;
    uint32_t cell = cell_address(grid, x, (int32_t)below);
    if (!(layer_flag(to) & M8(cell + 0xc))) return 0;
    return ((M32(cell) >> ((to << 2) & 31)) & (1u << (from & 31))) ? 1 : 0;
}

/* Native 0x9deaf0 / 0x792750: the same for the cell at x - 64 and +x links. */
uint32_t TC bfv_strategic_link_back_x(volatile StrategicGrid *grid, int32_t x, int32_t layer,
                                      int32_t z, uint32_t from, uint32_t to)
{
    (void)layer;
    uint32_t left = (uint32_t)x - 0x40;
    uint32_t outside_x = ~X_MASK;
    if (left & outside_x) return outside_x & ~0xffu;
    if ((uint32_t)z & ~Z_MASK) return (uint32_t)z & ~0xffu;
    uint32_t cell = cell_address(grid, (int32_t)left, z);
    if (!(layer_flag(to) & M8(cell + 0xc))) return 0;
    return ((M16(cell + 2) >> ((to << 2) & 31)) & (1u << (from & 31))) ? 1 : 0;
}

static volatile StrategicGrid *search_grid(volatile StrategicSearch *search)
{
    return (volatile StrategicGrid *)(uintptr_t)M32(search->owner + 0x40);
}

/* vf6 (0x9de950 / 0x7925b0) and vf7 (0x9de970 / 0x7925d0): copy the start or goal. */
volatile uint32_t *TC bfv_astar_strategic_start(volatile StrategicSearch *search, volatile uint32_t *out)
{
    out[0] = search->start[0];
    out[1] = search->start[1];
    out[2] = search->start[2];
    return out;
}
volatile uint32_t *TC bfv_astar_strategic_goal(volatile StrategicSearch *search, volatile uint32_t *out)
{
    out[0] = (uint32_t)search->goal[0];
    out[1] = (uint32_t)search->goal[1];
    out[2] = (uint32_t)search->goal[2];
    return out;
}

/* vf2 (0x9de990 / 0x7925f0): x + z distance from p to the goal. */
uint32_t TC bfv_astar_strategic_estimate(volatile StrategicSearch *search, const volatile int32_t *p)
{
    uint32_t dz = abs32((uint32_t)p[2] - (uint32_t)search->goal[2]);
    uint32_t dx = abs32((uint32_t)p[0] - (uint32_t)search->goal[0]);
    return dx + dz;
}

/* vf4 (0x9deb80 / 0x7927e0): p is in the goal's cell on the goal's layer
 * and within reach_distance (x + z) of it. */
uint32_t TC bfv_astar_strategic_reached(volatile StrategicSearch *search, const volatile int32_t *p)
{
    uint32_t same = bfv_strategic_same_cell(search_grid(search), p, search->goal);
    if (!(uint8_t)same) return same;
    uint32_t dz = abs32((uint32_t)p[2] - (uint32_t)search->goal[2]);
    uint32_t dx = abs32((uint32_t)p[0] - (uint32_t)search->goal[0]);
    return (int32_t)(dz + dx) <= search->reach_distance;
}

/* Native 0x9debd0 / 0x792830 (ecx = p): p lies in [low, high) on each axis
 * where low != high. */
uint32_t TC bfv_astar_strategic_within(const volatile int32_t *p, const volatile int32_t *low,
                                       const volatile int32_t *high)
{
    int32_t x = p[0];
    int32_t high_x = high[0], low_x = low[0];
    if (low_x != high_x && (x < low_x || x >= high_x)) return 0;
    int32_t high_z = high[2], low_z = low[2];
    int32_t z = p[2];
    if (low_z != high_z && (z < low_z || z >= high_z)) return 0;
    return 1;
}

typedef uint32_t (TC *object_fn)(uint32_t owner);
typedef uint32_t (TC *visit_fn)(uint32_t object, volatile int32_t *point);

/* vf3 helper (0x9decf0 / 0x792950): neighbour `index` of `node`. index >> 2
 * picks the direction (+z, +x, -z, -x) and index & 3 the layer to enter.
 * Writes the neighbour point to `out` and its estimate to `*estimate`, and
 * returns the step cost, or blocked_cost when the link is missing or the
 * point is outside the bounds. index >> 2 above 3 keeps node's position
 * and goes on when index's low byte is nonzero, as natively. */
uint32_t TC bfv_astar_strategic_step(volatile StrategicSearch *search, const volatile int32_t *node,
                                     int32_t index, volatile int32_t *out, volatile uint32_t *estimate)
{
    uint32_t to = (uint32_t)index & 3;
    int32_t dx = 0, dz = 0;
    uint8_t linked;
    switch ((uint32_t)(index >> 2)) {
    case 0:
        linked = (uint8_t)bfv_strategic_link_z(search_grid(search), node, (uint32_t)node[1], to);
        dz = 0x40;
        break;
    case 1:
        linked = (uint8_t)bfv_strategic_link_x(search_grid(search), node, (uint32_t)node[1], to);
        dx = 0x40;
        break;
    case 2:
        linked = (uint8_t)bfv_strategic_link_back_z(search_grid(search), node[0], node[1], node[2],
                                                    (uint32_t)node[1], to);
        dz = -0x40;
        break;
    case 3:
        linked = (uint8_t)bfv_strategic_link_back_x(search_grid(search), node[0], node[1], node[2],
                                                    (uint32_t)node[1], to);
        dx = -0x40;
        break;
    default:
        linked = (uint8_t)index;
        break;
    }
    if (!linked) return search->blocked_cost;

    out[0] = node[0];
    out[1] = node[1];
    out[2] = node[2];
    int32_t x = out[0], z = out[2];
    out[0] = (int32_t)((uint32_t)x + (uint32_t)dx);
    out[2] = (int32_t)((uint32_t)z + (uint32_t)dz);
    int32_t point[3];
    volatile int32_t *at = bfv_strategic_cell_point(search_grid(search), point, out, to);
    out[0] = at[0];
    out[1] = at[1];
    out[2] = at[2];
    out[1] = (int32_t)to;

    /* Native call whose result is not used: owner's indexed object, slot 3. */
    uint32_t object = ((object_fn)BFV_AI_INDEXED_OBJECT)(search->owner);
    ((visit_fn)M32(M32(object) + 0xc))(object, out);

    if (!bfv_astar_strategic_within(out, search->low, search->high)) return search->blocked_cost;
    uint32_t to_goal_z = abs32((uint32_t)out[2] - (uint32_t)search->goal[2]);
    uint32_t to_goal_x = abs32((uint32_t)out[0] - (uint32_t)search->goal[0]);
    *estimate = to_goal_z + to_goal_x;
    uint32_t step_z = abs32((uint32_t)out[2] - (uint32_t)node[2]);
    uint32_t step_x = abs32((uint32_t)out[0] - (uint32_t)node[0]);
    return step_x + step_z;
}

/* vf3 (0x9deed0 / 0x792b30): try all 16 neighbours of `node`. nodes[i] are
 * the output points (estimate at +0x10) and costs[i] their step costs as
 * floats. Each one that isn't blocked copies its layer to +0x0c and takes
 * the next slot. Returns how many were kept. */
uint32_t TC bfv_astar_strategic_expand(volatile StrategicSearch *search, const volatile int32_t *node,
                                       volatile uint32_t *nodes, volatile float *costs)
{
    uint32_t kept = 0;
    for (int32_t index = 0; index < 16; index++) {
        uint32_t point = nodes[kept];
        uint32_t cost = bfv_astar_strategic_step(search, node, index, (volatile int32_t *)(uintptr_t)point,
                                                 (volatile uint32_t *)(uintptr_t)(point + 0x10));
        costs[kept] = (float)(int32_t)cost;
        if (cost != search->blocked_cost) {
            uint32_t kept_point = nodes[kept];
            M32(kept_point + 0xc) = M32(kept_point + 4);
            kept++;
        }
    }
    return kept;
}

/* Native 0xa07350 / 0x7bb930, called for start and goal with its result
 * unused: a grid query on the point. */
typedef uint32_t (TC *point_query_fn)(uint32_t grid, const volatile int32_t *point);

#define REACH_SCALE (*(const volatile float *)BFV_ASTAR_REACH_SCALE)

/* _ftol2(reach * 5): the retained runtime truncation of the extended product. */
static uint32_t reach_distance(const volatile float *reach)
{
    uint32_t distance;
    __asm__ volatile("flds %[v]; fmuls %[scale]; call *%[ftol]"
        : "=a"(distance)
        : [v]"m"(*reach), [scale]"m"(REACH_SCALE), [ftol]"r"(BFV_FTOL2)
        : "ecx", "edx", "memory", "st");
    return distance;
}

/* 0x9dec20 / 0x792880: prepare a search from start to goal inside the
 * bounds low..high. Start and goal are queried on the owner's grid and
 * snapped to their cells' layer points; the goal counts as reached within
 * reach * 5 (x + z). Returns that distance. */
uint32_t TC bfv_astar_strategic_setup(volatile StrategicSearch *search, uint32_t owner,
                                      const volatile int32_t *start, const volatile int32_t *goal,
                                      float reach, const volatile int32_t *low,
                                      const volatile int32_t *high)
{
    search->unknown_10 = 0x20;
    search->unknown_14 = 0x41fc;
    search->low[0] = low[0];
    search->low[1] = low[1];
    search->low[2] = low[2];
    search->high[0] = high[0];
    search->high[1] = high[1];
    search->high[2] = high[2];
    search->owner = owner;
    ((point_query_fn)BFV_STRATEGIC_POINT_QUERY)(M32(owner + 0x40), start);
    ((point_query_fn)BFV_STRATEGIC_POINT_QUERY)(M32(search->owner + 0x40), goal);

    volatile int32_t snapped[3];
    volatile int32_t *at = bfv_strategic_cell_point(search_grid(search), snapped, start, (uint32_t)start[1]);
    search->start[0] = (uint32_t)at[0];
    search->start[1] = (uint32_t)at[1];
    search->start[2] = (uint32_t)at[2];
    at = bfv_strategic_cell_point(search_grid(search), snapped, goal, (uint32_t)goal[1]);
    search->goal[0] = at[0];
    search->goal[1] = at[1];
    search->goal[2] = at[2];
    uint32_t distance = reach_distance(&reach);
    search->reach_distance = (int32_t)distance;
    return distance;
}

/* A bit set: `count` bits in 32-bit words. */
typedef struct BitSet {
    uint32_t count;             /* +0x00 */
    uint32_t words;             /* +0x04 */
} BitSet;

typedef uint32_t (*allocate_fn)(uint32_t size);

static uint32_t bit_words(uint32_t count) { return (count >> 5) + ((count & 31) != 0); }

/* 0x9ceda0 / 0x784590: allocate `count` cleared bits (operator new). */
volatile BitSet *TC bfv_bitset_init(volatile BitSet *bits, uint32_t count)
{
    bits->count = count;
    bits->words = ((allocate_fn)BFV_VECTOR_ALLOCATOR)(bit_words(count) * 4);
    uint32_t words = bit_words(bits->count);
    for (uint32_t i = 0; i < words; i++) M32(bits->words + i * 4) = 0;
    return bits;
}

/* A search request: the strategic search engine (+0x12c) and the radius in
 * cells that the search may cover around its start. */
typedef struct StrategicRequest {
    float radius;               /* +0x000 */
    uint8_t flag;               /* +0x004, passed to the engine as its last argument */
    uint8_t unknown_005[0x113];
    uint8_t engine_arg_118[8];  /* +0x118, passed by address */
    uint8_t engine_arg_120[0xc];/* +0x120, passed by address */
    uint8_t engine[4];          /* +0x12c */
} StrategicRequest;

/* A width x height grid of bits; bit y * width + x. */
typedef struct DiscMask {
    int32_t width;
    int32_t height;
    BitSet bits;
} DiscMask;

/* The engine's listeners are told before (0x9def90 / 0x792bf0) and after
 * (0x9defd0 / 0x792c30, 0x9df220 / 0x792e80) a run. */
typedef uint32_t (TC *notify_fn)(uint32_t engine, uint32_t arg);
/* 0x9e4620 / 0x798280: the A* engine. */
typedef uint32_t (TC *engine_fn)(uint32_t engine, uint32_t a, uint32_t b, volatile DiscMask *mask,
                                 uint32_t arg_120, uint32_t arg_118, uint32_t two,
                                 uint32_t c, uint32_t d, uint32_t flag);
typedef uint32_t (*release_fn)(uint32_t memory);

#define HALF (*(const volatile float *)BFV_MATH_HALF)
#define UNSIGNED_BIAS (*(const volatile float *)BFV_UNSIGNED_BIAS)

/* _ftol2(|radius|), keeping |radius| as a float. */
static uint32_t radius_cells(const volatile float *radius, volatile float *kept)
{
    uint32_t cells;
    __asm__ volatile("flds %[v]; fabs; fsts %[kept]; call *%[ftol]"
        : "=a"(cells), [kept]"=m"(*kept)
        : [v]"m"(*radius), [ftol]"r"(BFV_FTOL2)
        : "ecx", "edx", "memory", "st");
    return cells;
}

/* An unsigned int as the native code converts it: signed, plus 2^32 when
 * the sign bit is set. */
static long double unsigned_value(uint32_t value)
{
    long double result = (long double)(int32_t)value;
    if ((int32_t)value < 0) result += UNSIGNED_BIAS;
    return result;
}

/* sum <= limit through FCOMP (unordered is false and raises invalid). */
static int at_most(long double sum, long double limit)
{
    volatile long double a = sum, b = limit;
    uint16_t status;
    __asm__ volatile("fldt %[b]; fldt %[a]; fcompp; fnstsw %%ax"
        : "=a"(status) : [a]"m"(a), [b]"m"(b) : "st", "st(1)");
    uint32_t c0c3 = (status >> 8) & 0x41;
    return c0c3 == 0x01 || c0c3 == 0x40;
}

/* 0x9cf5c0 / 0x784fb0: run the engine inside a disc of |radius| cells.
 * The mask is (2n + 1) squared bits, n = |radius| truncated; a cell is set
 * when its centre lies within |radius| of the mask centre. Returns what
 * operator delete leaves in EAX. */
uint32_t TC bfv_astar_strategic_run(volatile StrategicRequest *request, uint32_t notify_arg,
                                    uint32_t a, uint32_t b, uint32_t c, uint32_t d)
{
    volatile float radius;
    int32_t size = (int32_t)(radius_cells(&request->radius, &radius) * 2 + 1);
    int32_t width = size < 1 ? 1 : size;
    volatile DiscMask mask;
    mask.width = width;
    mask.height = width;
    bfv_bitset_init(&mask.bits, (uint32_t)width * (uint32_t)width);
    if (size <= 1) {
        M32(mask.bits.words) |= 1;
    } else {
        volatile float centre;
        centre = (float)((long double)(size / 2) + HALF);
        for (uint32_t row = 0; row < (uint32_t)size; row++) {
            long double dy = unsigned_value(row) + HALF - centre;
            dy = dy * dy;
            long double limit = (long double)radius * radius;
            for (uint32_t column = 0; column < (uint32_t)size; column++) {
                long double dx = unsigned_value(column) + HALF - centre;
                if (at_most(dx * dx + dy, limit)) {
                    uint32_t bit = row * (uint32_t)mask.width + column;
                    M32(mask.bits.words + (bit >> 5) * 4) |= 1u << (bit & 31);
                }
            }
        }
    }
    uint32_t engine = (uint32_t)(uintptr_t)request->engine;
    ((notify_fn)BFV_SEARCH_NOTIFY_BEGIN)(engine, notify_arg);
    ((engine_fn)BFV_ASTAR_ENGINE)(engine, a, b, &mask, (uint32_t)(uintptr_t)request->engine_arg_120,
                                  (uint32_t)(uintptr_t)request->engine_arg_118, 2, c, d, request->flag);
    ((notify_fn)BFV_SEARCH_NOTIFY_END)(engine, notify_arg);
    ((notify_fn)BFV_SEARCH_NOTIFY_FINISH)(engine, notify_arg);
    return ((release_fn)BFV_VECTOR_FREE)(mask.bits.words);
}
