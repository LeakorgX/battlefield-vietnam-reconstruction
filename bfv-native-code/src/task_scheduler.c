/* AI task scheduler: the tree of bot tasks (bf/ai/Task.c and the scheduler
 * routines lifted into bf/ai/misc_3.c).
 *
 * A task (pathfinding search, strategic query, ...) is linked three ways:
 * its children in a singly linked list (first_child, next), and a sequel,
 * the task queued to run after it. The scheduler keeps the root task, the
 * task running now and the task added last; an added task either becomes a
 * child of the running task or the sequel of the last added one.
 *
 * Names describe how these routines use each field. Doubles copied through
 * the x87 stack stay x87 copies (a signalling NaN becomes quiet and sets the
 * invalid flag, as natively). Routines that natively return a byte through
 * `mov al, 1` / `xor al, al` return the whole eax they leave. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))

typedef struct Task Task;
struct Task {
    uint32_t vtable;            /* +0x00 */
    uint32_t unknown_04;        /* +0x04 */
    double wake_time;           /* +0x08, scheduler time at which the task runs */
    const char *name;           /* +0x10, e.g. "TaskAStarLocalSearch" */
    volatile struct TaskScheduler *scheduler; /* +0x14 */
    volatile Task *sequel;      /* +0x18, queued to run after this task */
    volatile Task *next;        /* +0x1c, next child of the same parent */
    volatile Task *first_child; /* +0x20 */
    uint32_t unknown_24;        /* +0x24 */
    double delay;               /* +0x28, from its parent's wake time */
    uint8_t sleeps_until_woken; /* +0x30, set by a delay of -1 */
    uint8_t unknown_31;         /* +0x31, cleared on sleep and add */
};

typedef struct TaskScheduler {
    volatile Task *root;        /* +0x00 */
    uint32_t unknown_04;        /* +0x04 */
    double slice_start;         /* +0x08 */
    double slice_length;        /* +0x10 */
    volatile Task *running;     /* +0x18 */
    volatile Task *last_added;  /* +0x1c */
} TaskScheduler;

typedef volatile Task VTask;
typedef volatile TaskScheduler VScheduler;
/* getTimeSeconds returns st0; long double keeps it and the result unrounded. */
typedef long double (*time_fn)(void);

/* fld qword / fstp qword. */
static void x87_copy(volatile double *to, const volatile double *from)
{
    __asm__ volatile("fldl %1; fstpl %0" : "=m"(*to) : "m"(*from));
}

static uint32_t al_one(uint32_t eax) { return (eax & ~0xffu) | 1; }

/* Task::sleep (vf): -1 sleeps until woken, any other delay (NaN too) is kept. */
void TC bfv_task_sleep(VTask *task, double delay)
{
    if (delay == -1.0) {
        task->sleeps_until_woken = 1;
        task->delay = 0.0;
    } else {
        x87_copy(&task->delay, &delay);
        task->sleeps_until_woken = 0;
    }
    task->unknown_31 = 0;
}

/* Remove task from the subtree under root. Each child is searched depth
 * first; on the child equal to task (or every child, when remove_all is set)
 * the call recurses with remove_all set, then unlinks task from root's
 * children: its sequel takes its place and inherits its wake time plus the
 * sequel's delay; without a sequel the previous child (or root) takes task's
 * next link and wake time. Returns 1 in al once something was unlinked. */
uint32_t TC bfv_task_tree_remove(VScheduler *scheduler, VTask *root, VTask *task, uint32_t remove_all)
{
    VTask *child = root->first_child;
    VTask *previous = 0;
    uint32_t eax = (uint32_t)(uintptr_t)root;

    while (child) {
        if (child == task || (uint8_t)remove_all) {
            bfv_task_tree_remove(scheduler, child, task, 1);
            VTask *sequel = task->sequel;
            if (sequel) {
                if (previous)
                    previous->next = sequel;
                else
                    root->first_child = sequel;
                VTask *next = task->next;
                task->sequel->next = next;
                sequel = task->sequel;
                sequel->wake_time = sequel->delay + task->wake_time;
                return al_one((uint32_t)(uintptr_t)sequel);
            }
            if (previous) {
                previous->next = task->next;
                x87_copy(&previous->wake_time, &task->wake_time);
                return 1;
            }
            root->first_child = task->next;
            x87_copy(&root->wake_time, &task->wake_time);
            return al_one((uint32_t)(uintptr_t)root);
        }
        eax = bfv_task_tree_remove(scheduler, child, task, 0);
        if ((uint8_t)eax)
            return al_one(eax);
        previous = child;
        child = child->next;
    }
    return eax & ~0xffu;
}

/* Seconds left in the scheduler's current slice. The end is rounded to a
 * double (stored across the call, as natively); the difference is not. */
long double TC bfv_scheduler_time_left(VScheduler *scheduler)
{
    double slice_end = scheduler->slice_length + scheduler->slice_start;
    return slice_end - ((time_fn)BFV_GET_TIME_SECONDS)();
}

/* Add a task with a delay (-1 = sleep until woken). It becomes a child of the
 * running task, due at the running task's wake time plus the delay, when
 * as_child is set or nothing was added before; otherwise it is queued as the
 * sequel of the last added task. */
uint32_t TC bfv_scheduler_add(VScheduler *scheduler, VTask *task, double delay, uint32_t as_child)
{
    if (delay == -1.0) {
        task->sleeps_until_woken = 1;
        task->delay = 0.0;
    } else {
        task->sleeps_until_woken = 0;
        x87_copy(&task->delay, &delay);
    }
    task->scheduler = scheduler;
    task->unknown_31 = 0;
    int child = (uint8_t)as_child || !scheduler->last_added;
    task->sequel = 0;
    task->next = 0;
    task->first_child = 0;
    if (child) {
        task->wake_time = scheduler->running->wake_time + task->delay;
        task->next = scheduler->running->first_child;
        scheduler->running->first_child = task;
    } else {
        scheduler->last_added->sequel = task;
    }
    scheduler->last_added = task;
    return al_one((uint32_t)(uintptr_t)task);
}

/* Remove a task anywhere under the root. */
uint32_t TC bfv_scheduler_remove(VScheduler *scheduler, VTask *task)
{
    return bfv_task_tree_remove(scheduler, scheduler->root, task, 0);
}

/* TaskAStarLocalSearch::execute and TaskAStarStrategicSearch::execute (vf).
 * Each kind of search has one searcher object and a slot naming the task
 * whose search it is running; a task that finds the slot taken by another
 * task sleeps (delay 0) and tries again next time. Otherwise it claims the
 * slot and lets the searcher run until the slice's time is used up. While the
 * searcher reports the search still in progress the task keeps the slot and
 * sleeps; once finished it frees the slot and stores into *result -1 (search
 * failed), the path the searcher copies out (when it wants at least one
 * point) or 0. The two methods are the same code over different globals.
 * Returns the eax the native code leaves. */
typedef struct SearchTask {
    uint8_t task[0x34];         /* Task, +0x00 */
    uint32_t state;             /* +0x34, 2 when queued, 1 while the search runs on */
    uint8_t in_progress;        /* +0x38, written by the searcher each slice */
    uint8_t unknown_39[3];
    uint32_t search;            /* +0x3c, handed to the searcher */
    volatile int32_t *result;   /* +0x40 */
    uint32_t path_out;          /* +0x44 */
    uint32_t path_extra;        /* +0x48 */
    int32_t path_points;        /* +0x4c */
} SearchTask;
typedef volatile SearchTask VSearchTask;

typedef long double (TC *time_left_fn)(VScheduler *);
typedef uint32_t (TC *sleep_fn)(VSearchTask *, double);
typedef uint32_t (TC *step_fn)(uint32_t searcher, uint32_t search, uint32_t in_progress,
                               volatile uint8_t *still_running, volatile double *time_left);
typedef uint32_t (TC *path_fn)(uint32_t searcher, uint32_t out, int32_t points, uint32_t extra);

static uint32_t search_task_execute(VSearchTask *task, volatile uint32_t *slot, uint32_t searcher)
{
    volatile double time_left;
    volatile uint8_t still_running;
    time_left = ((time_left_fn)BFV_SCHEDULER_TIME_LEFT)(((VTask *)task)->scheduler);
    uint32_t current = *slot;
    if (current == 0 || current == (uint32_t)(uintptr_t)task) {
        uint32_t in_progress = task->in_progress;
        uint32_t search = task->search;
        *slot = (uint32_t)(uintptr_t)task;
        uint32_t eax = ((step_fn)BFV_SEARCHER_STEP)(searcher, search, in_progress, &still_running, &time_left);
        uint8_t running = still_running;
        task->in_progress = running;
        if (!running) {
            if (!(uint8_t)eax) {
                *task->result = -1;
            } else {
                int32_t points = task->path_points;
                if (points > 0) {
                    eax = ((path_fn)BFV_SEARCHER_PATH)(searcher, task->path_out, points, task->path_extra);
                    *task->result = (int32_t)eax;
                } else {
                    eax = (uint32_t)points;
                    *task->result = 0;
                }
            }
            *slot = 0;
            return eax;
        }
        task->state = 1;
    }
    return ((sleep_fn)BFV_TASK_SLEEP)(task, *(const volatile double *)BFV_TASK_ZERO);
}

uint32_t TC bfv_task_local_search_execute(VSearchTask *task)
{
    return search_task_execute(task, (volatile uint32_t *)BFV_LOCAL_SEARCH_CURRENT, BFV_LOCAL_SEARCHER);
}

uint32_t TC bfv_task_strategic_search_execute(VSearchTask *task)
{
    return search_task_execute(task, (volatile uint32_t *)BFV_STRATEGIC_SEARCH_CURRENT, BFV_STRATEGIC_SEARCHER);
}

/* Task::vf3: take the task out of its scheduler, if it has one. */
typedef uint32_t (TC *remove_fn)(VScheduler *, VTask *);
uint32_t TC bfv_task_unschedule(VTask *task)
{
    VScheduler *scheduler = task->scheduler;
    if (!scheduler)
        return (uint32_t)(uintptr_t)task;
    return ((remove_fn)BFV_SCHEDULER_REMOVE)(scheduler, task);
}

/* Queue child on this task's scheduler (scheduler add). The delay is copied
 * through the x87 stack, as natively. */
typedef uint32_t (TC *add_fn)(VScheduler *, volatile void *, double, uint32_t);
uint32_t TC bfv_task_queue(VTask *task, volatile void *child, double delay, uint32_t as_child)
{
    double copy;
    __asm__ volatile("fldl %1; fstpl %0" : "=m"(copy) : "m"(delay));
    return ((add_fn)BFV_SCHEDULER_ADD)(task->scheduler, child, copy, as_child);
}

/* TaskPlayerMapSearch::execute and TaskStrategicPlayerMapSearch::execute (vf).
 * Both prepare their A* search (an AStarLocalSearch at +0xa8) from the start
 * and goal at +0x38 / +0x44 on the map at +0x50, then queue the A* search
 * task they embed at +0x58 with delay 0: named, pointed at the search and the
 * caller's result and path, queued state (2), not in progress. */
typedef struct PlayerMapSearchTask {
    uint8_t task[0x38];         /* Task, +0x00 */
    uint8_t start[0x0c];        /* +0x38 */
    uint8_t goal[0x0c];         /* +0x44 */
    uint32_t map;               /* +0x50 */
    uint32_t unknown_54;        /* +0x54 */
    SearchTask child;           /* +0x58 */
    uint8_t search[0xc4];       /* +0xa8, AStarLocalSearch */
    volatile int32_t *result;   /* +0x16c */
    uint32_t path_out;          /* +0x170 */
    uint32_t value_174;         /* +0x174, path points (strategic) */
    uint32_t value_178;         /* +0x178, path points (local); passed by address (strategic) */
    uint32_t limit;             /* +0x17c, local */
    uint32_t unknown_180;       /* +0x180 */
    uint32_t value_184;         /* +0x184, passed by address (strategic) */
} PlayerMapSearchTask;
typedef volatile PlayerMapSearchTask VPlayerMapTask;

typedef uint32_t (TC *local_prepare_fn)(volatile void *search, uint32_t map, volatile void *start,
                                        volatile void *goal, uint32_t limit, float range);
typedef uint32_t (TC *local_finish_fn)(volatile void *search, uint32_t map, volatile void *start, volatile void *goal);
typedef uint32_t (TC *strategic_prepare_fn)(volatile void *search, uint32_t map, volatile void *start,
                                            volatile void *goal, volatile void *a, volatile void *b);
typedef uint32_t (TC *queue_fn)(volatile void *, volatile void *, double, uint32_t);

/* Fill the A* search task child and queue it on task with delay 0. */
static uint32_t queue_child_search(volatile void *task, VSearchTask *child, volatile void *search,
                                   uint32_t name, volatile int32_t *result, uint32_t path_out,
                                   int32_t points)
{
    child->result = result;
    child->in_progress = 0;
    child->path_extra = 0;
    ((VTask *)child)->name = (const char *)(uintptr_t)name;
    child->search = (uint32_t)(uintptr_t)search;
    child->path_out = path_out;
    child->path_points = points;
    child->state = 2;
    return ((queue_fn)BFV_TASK_QUEUE)(task, child, *(const volatile double *)BFV_TASK_ZERO, 0);
}

static uint32_t queue_search(VPlayerMapTask *task, uint32_t name, volatile int32_t *result,
                             uint32_t path_out, int32_t points)
{
    return queue_child_search(task, &task->child, task->search, name, result, path_out, points);
}

uint32_t TC bfv_task_player_map_execute(VPlayerMapTask *task)
{
    ((local_prepare_fn)BFV_LOCAL_SEARCH_PREPARE)(task->search, task->map, task->start, task->goal, task->limit, -1.0f);
    ((local_finish_fn)BFV_LOCAL_SEARCH_FINISH)(task->search, task->map, task->start, task->goal);
    volatile int32_t *result = task->result;
    int32_t points = (int32_t)task->value_178;
    uint32_t path_out = task->path_out;
    return queue_search(task, BFV_LOCAL_SEARCH_TASK_NAME, result, path_out, points);
}

uint32_t TC bfv_task_strategic_player_map_execute(VPlayerMapTask *task)
{
    ((strategic_prepare_fn)BFV_STRATEGIC_MAP_PREPARE)(task->search, task->map, task->start, task->goal,
                                                     &task->value_178, &task->value_184);
    volatile int32_t *result = task->result;
    int32_t points = (int32_t)task->value_174;
    uint32_t path_out = task->path_out;
    return queue_search(task, BFV_STRATEGIC_SEARCH_TASK_NAME, result, path_out, points);
}

/* TaskStrategicSearch::execute (vf), one step per turn on the state at
 * +0x300:
 *   0  queue the cell query child (+0xa8) for the start cell, unless known;
 *   1  give up (result -1) without a start cell; with no goal cell yet, a
 *      reach above 64 takes cell 0, otherwise queue the cell query for the
 *      goal with the reach;
 *   2  give up (result -1) without a goal cell; otherwise store both cells
 *      in the points' y, set up the strategic search (+0x2a0) with zero
 *      bounds and queue the A* search task child (+0x58) on it.
 * States 0 and 1 then advance the state and sleep with delay 0. */
typedef struct StrategicSearchTask {
    uint8_t task[0x38];         /* Task, +0x00 */
    int32_t start[3];           /* +0x38 */
    int32_t goal[3];            /* +0x44 */
    uint32_t map;               /* +0x50 */
    uint32_t unknown_54;        /* +0x54 */
    SearchTask child;           /* +0x58 */
    uint8_t cell_query[0x1f8];  /* +0xa8, TaskGetStrategicCellInfoNo */
    uint8_t search[0x4c];       /* +0x2a0, AStarStrategicSearch */
    int32_t start_cell;         /* +0x2ec, -1 until found */
    int32_t goal_cell;          /* +0x2f0, -1 until found */
    volatile int32_t *result;   /* +0x2f4 */
    uint32_t path_out;          /* +0x2f8 */
    int32_t path_points;        /* +0x2fc */
    uint32_t state;             /* +0x300 */
    union { float value; uint32_t bits; } reach; /* +0x304 */
} StrategicSearchTask;
typedef volatile StrategicSearchTask VStrategicSearchTask;

typedef uint32_t (TC *cell_query_init_fn)(volatile void *query, uint32_t map, volatile int32_t *point,
                                          uint32_t reach_bits, volatile int32_t *cell);
typedef uint32_t (TC *strategic_setup_fn)(volatile void *search, uint32_t map, volatile int32_t *start,
                                          volatile int32_t *goal, uint32_t reach_bits,
                                          volatile int32_t *low, volatile int32_t *high);
typedef uint32_t (TC *task_sleep_fn)(volatile void *, double);

/* FCOMP of the reach against the float bound: true when above (ordered). */
static int reach_above(const volatile float *reach, const volatile float *bound)
{
    uint16_t status;
    __asm__ volatile("flds %[reach]; fcomps %[bound]; fnstsw %%ax"
        : "=a"(status) : [reach]"m"(*reach), [bound]"m"(*bound) : "st");
    return !(status & 0x4100);
}

static uint32_t query_cell(VStrategicSearchTask *task, volatile int32_t *point, uint32_t reach_bits,
                           volatile int32_t *cell)
{
    ((cell_query_init_fn)BFV_CELL_QUERY_INIT)(task->cell_query, task->map, point, reach_bits, cell);
    return ((queue_fn)BFV_TASK_QUEUE)(task, task->cell_query, *(const volatile double *)BFV_TASK_ZERO, 0);
}

uint32_t TC bfv_task_strategic_search_run(VStrategicSearchTask *task)
{
    uint32_t state = task->state;
    if (state == 2) {
        int32_t goal_cell = task->goal_cell;
        if (goal_cell == -1) {
            volatile int32_t *result = task->result;
            *result = -1;
            return (uint32_t)(uintptr_t)result;
        }
        int32_t start_cell = task->start_cell;
        task->goal[1] = goal_cell;
        task->start[1] = start_cell;
        volatile int32_t high[3] = {0, 0, 0}, low[3] = {0, 0, 0};
        ((strategic_setup_fn)BFV_ASTAR_STRATEGIC_SETUP)(task->search, task->map, task->start, task->goal,
                                                        task->reach.bits, low, high);
        uint32_t path_out = task->path_out;
        volatile int32_t *result = task->result;
        int32_t points = task->path_points;
        return queue_child_search(task, &task->child, task->search, BFV_STRATEGIC_SEARCH_TASK_NAME,
                                  result, path_out, points);
    }
    if (state == 1) {
        if (task->start_cell == -1) {
            *task->result = -1;
            return 0;
        }
        if (task->goal_cell == -1) {
            if (reach_above(&task->reach.value, (const volatile float *)BFV_STRATEGIC_REACH_LIMIT))
                task->goal_cell = 0;
            if (task->goal_cell == -1)
                query_cell(task, task->goal, task->reach.bits, &task->goal_cell);
        }
    } else if (state == 0) {
        if (task->start_cell == -1)
            query_cell(task, task->start, 0, &task->start_cell);
    } else {
        return state - 2;
    }
    task->state = task->state + 1;
    return ((task_sleep_fn)BFV_TASK_SLEEP)(task, *(const volatile double *)BFV_TASK_ZERO);
}

/* TaskGetStrategicCellInfoNo::execute (vf). Finds the strategic grid layer
 * (0-3) of the cell under the point whose layer point lies nearest the
 * point, among layers flagged in the cell and not tried yet, marks it tried
 * and queues the player map search it embeds (+0x48) from the point to that
 * layer point, inside the 64-unit cell around the point. The search writes
 * +0x1f0; once that and the layer are known, the layer goes to *out. */
typedef struct CellQueryTask {
    uint8_t task[0x38];         /* Task, +0x00 */
    int32_t point[3];           /* +0x38 */
    uint32_t map;               /* +0x44, its +0x40 is the strategic grid */
    uint8_t search[0x198];      /* +0x48, TaskPlayerMapSearch */
    volatile int32_t *out;      /* +0x1e0 */
    int32_t layer;              /* +0x1e4, -1 until found */
    uint32_t tried;             /* +0x1e8, bit per layer */
    uint32_t reach_bits;        /* +0x1ec */
    int32_t search_result;      /* +0x1f0 */
} CellQueryTask;
typedef volatile CellQueryTask VCellQueryTask;

typedef volatile int32_t *(TC *layer_point_fn)(uint32_t grid, volatile int32_t *out,
                                               volatile int32_t *point, int32_t layer);
typedef uint32_t (__attribute__((fastcall)) *ground_height_fn)(uint32_t map, volatile int32_t *point,
                                                               uint32_t walkable);
typedef int32_t (TC *cell_distance_fn)(volatile int32_t *a, volatile int32_t *b);
typedef uint32_t (TC *player_map_init_fn)(volatile void *search, uint32_t map, volatile int32_t *start,
                                          volatile int32_t *goal, volatile int32_t *path_goal,
                                          uint32_t zero, volatile int32_t *result,
                                          volatile int32_t *low, volatile int32_t *high, uint32_t reach_bits);

/* The TaskStrategicPlayerMapSearch the cell query embeds at +0x48: a
 * player map search task whose prepare also gets the bounds by address. */
typedef struct StrategicPlayerMapTask {
    uint8_t task[0x38];         /* Task, +0x00 */
    int32_t start[3];           /* +0x38 */
    int32_t goal[3];            /* +0x44 */
    uint32_t map;               /* +0x50 */
    uint32_t unknown_54;        /* +0x54 */
    SearchTask child;           /* +0x58 */
    uint8_t search[0xc4];       /* +0xa8, AStarLocalSearch */
    volatile int32_t *result;   /* +0x16c */
    volatile int32_t *path_goal; /* +0x170 */
    uint32_t path_points;       /* +0x174 */
    int32_t low[3];             /* +0x178 */
    int32_t high[3];            /* +0x184 */
    uint32_t reach_bits;        /* +0x190 */
} StrategicPlayerMapTask;
_Static_assert(__builtin_offsetof(StrategicPlayerMapTask, reach_bits) == 0x190, "strategic player map layout");

/* 0x9de100 / 0x791d60: set up the cell query for the point on the map:
 * named, *out = -1 until a layer is found, nothing tried, layer -1. Returns
 * out (what EAX holds natively). Native callers rely on ECX still being the
 * query afterwards (the C task executes here pass it explicitly anyway), so
 * the entry below saves ECX around the C body. */
volatile int32_t *TC bfv_cell_query_init_body(VCellQueryTask *query, uint32_t map, const volatile int32_t *point,
                                         uint32_t reach_bits, volatile int32_t *out)
{
    volatile Task *task = (volatile Task *)query;
    task->name = (const char *)BFV_CELL_QUERY_TASK_NAME;
    query->point[0] = point[0];
    query->point[1] = point[1];
    query->point[2] = point[2];
    query->map = map;
    query->out = out;
    *out = -1;
    query->reach_bits = reach_bits;
    query->tried = 0;
    query->layer = -1;
    return out;
}

/* 0x9de670 / 0x7922d0: set up a strategic player map search from start to
 * goal inside low..high. Returns the reach bits (what EAX holds natively);
 * ECX stays the search as for the cell query init. */
uint32_t TC bfv_player_map_search_init_body(volatile StrategicPlayerMapTask *search, uint32_t map,
                                          const volatile int32_t *start, const volatile int32_t *goal,
                                          volatile int32_t *path_goal, uint32_t path_points,
                                          volatile int32_t *result, const volatile int32_t *low,
                                          const volatile int32_t *high, uint32_t reach_bits)
{
    search->map = map;
    ((volatile Task *)search)->name = (const char *)BFV_STRATEGIC_PLAYER_MAP_TASK_NAME;
    for (int i = 0; i < 3; i++)
        search->start[i] = start[i];
    for (int i = 0; i < 3; i++)
        search->goal[i] = goal[i];
    search->path_goal = path_goal;
    search->path_points = path_points;
    search->result = result;
    for (int i = 0; i < 3; i++)
        search->low[i] = low[i];
    for (int i = 0; i < 3; i++)
        search->high[i] = high[i];
    search->reach_bits = reach_bits;
    return reach_bits;
}

/* The native entries: copy the stack arguments, run the C body and return
 * with ECX restored and the callee's pop. */
__attribute__((naked)) void bfv_cell_query_init(void)
{
    __asm__ volatile("push %%ecx;"
                     "pushl 0x14(%%esp); pushl 0x14(%%esp); pushl 0x14(%%esp); pushl 0x14(%%esp);"
                     "call _bfv_cell_query_init_body; pop %%ecx; ret $0x10" : : : "memory");
}

__attribute__((naked)) void bfv_player_map_search_init(void)
{
    __asm__ volatile("push %%ecx;"
                     "pushl 0x28(%%esp); pushl 0x28(%%esp); pushl 0x28(%%esp); pushl 0x28(%%esp);"
                     "pushl 0x28(%%esp); pushl 0x28(%%esp); pushl 0x28(%%esp); pushl 0x28(%%esp);"
                     "pushl 0x28(%%esp);"
                     "call _bfv_player_map_search_init_body; pop %%ecx; ret $0x24" : : : "memory");
}

static uint32_t strategic_grid(uint32_t map)
{
    return *(volatile uint32_t *)(uintptr_t)(map + 0x40);
}

/* The flag byte of the grid cell under the point: bits 0-3 say a layer
 * exists, bits 4-7 something per layer that the selection needs too. */
static uint8_t cell_flags(uint32_t grid, VCellQueryTask *task)
{
    uint32_t shift = *(volatile uint32_t *)(uintptr_t)(grid + 0x28);
    uint32_t cells = *(volatile uint32_t *)(uintptr_t)(grid + 0x1c);
    uint32_t row = (uint32_t)(task->point[2] >> 6) << (shift & 31);
    uint32_t index = (row + (uint32_t)(task->point[0] >> 6)) << 4;
    return *(volatile uint8_t *)(uintptr_t)(cells + index + 0xc);
}

static uint32_t absolute_difference(int32_t a, int32_t b)
{
    uint32_t difference = (uint32_t)a - (uint32_t)b;
    uint32_t sign = (int32_t)difference < 0 ? ~0u : 0;
    return (difference ^ sign) - sign;
}

/* _ftol2 of the smaller of |b - a| and |a - b| as x87 sees them: both
 * loaded with fild, the second also stored as a float. When the second
 * compares below the first, the float copy is truncated instead. */
static int32_t axis_distance(int32_t a, int32_t b)
{
    int32_t first = (int32_t)absolute_difference(b, a);
    int32_t second = (int32_t)absolute_difference(a, b);
    float second_float;
    int32_t distance;
    __asm__ volatile("fildl %[first]; fildl %[second]; fsts %[copy]; fcomp %%st(1);"
                     "fnstsw %%ax; testb $5, %%ah; jp 1f; fstp %%st(0); flds %[copy];"
                     "1: call *%[ftol]"
        : "=&a"(distance), [copy]"=m"(second_float)
        : [first]"m"(first), [second]"m"(second), [ftol]"r"(BFV_FTOL2)
        : "ecx", "edx", "memory", "st");
    return distance;
}

/* 0x9de5c0 / 0x792220: octile-style distance between two grid points on
 * x and z: max + min * 23 - min, the 23 being 0x170000 in 16.16 fixed
 * point with the 32-bit product wrapping. */
int32_t TC bfv_strategic_cell_distance(volatile int32_t *a, volatile int32_t *b)
{
    int32_t x = axis_distance(a[0], b[0]);
    int32_t z = axis_distance(a[2], b[2]);
    int32_t low = x < z ? x : z;
    int32_t high = x < z ? z : x;
    int32_t scaled = (int32_t)((uint32_t)low * 0x170000u) >> 16;
    return (int32_t)((uint32_t)scaled - (uint32_t)low + (uint32_t)high);
}

uint32_t TC bfv_task_cell_query_execute(VCellQueryTask *task)
{
    if (task->layer >= 0) {
        int32_t result = task->search_result;
        if (result >= 0) {
            *task->out = task->layer;
            return (uint32_t)result;
        }
    }
    task->layer = -1;
    int32_t best = 0x100000;
    volatile int32_t point[3] = {0, 0, 0};
    for (int32_t layer = 0; layer < 4; layer++) {
        uint32_t bit = 1u << layer;
        if (!(cell_flags(strategic_grid(task->map), task) & (uint8_t)(bit << 4)))
            continue;
        if (task->tried & bit)
            continue;
        volatile int32_t found[3];
        volatile int32_t *p = ((layer_point_fn)BFV_STRATEGIC_LAYER_POINT)(strategic_grid(task->map), found,
                                                                         task->point, layer);
        point[0] = p[0];
        point[1] = p[1];
        point[2] = p[2];
        uint32_t map = task->map;
        uint32_t walkable = (cell_flags(strategic_grid(map), task) & (uint8_t)bit) != 0;
        ((ground_height_fn)BFV_STRATEGIC_GROUND_HEIGHT)(map, point, walkable);
        if (((cell_distance_fn)BFV_STRATEGIC_CELL_DISTANCE)(point, task->point) < best) {
            best = ((cell_distance_fn)BFV_STRATEGIC_CELL_DISTANCE)(point, task->point);
            task->layer = layer;
        }
    }
    int32_t layer = task->layer;
    if (layer < 0) {
        *task->out = layer;
        return 4;
    }
    uint32_t bit = 1u << (layer & 31);
    uint32_t tried = task->tried;
    uint32_t map = task->map;
    task->tried = tried | bit;
    uint32_t walkable = (cell_flags(strategic_grid(map), task) & (uint8_t)bit) != 0;
    ((ground_height_fn)BFV_STRATEGIC_GROUND_HEIGHT)(map, point, walkable);
    volatile int32_t low[3], high[3];
    low[0] = task->point[0] & ~0x3f;
    low[1] = 0;
    high[1] = 0;
    low[2] = task->point[2] & ~0x3f;
    high[0] = low[0] + 0x40;
    high[2] = low[2] + 0x40;
    uint32_t reach_bits = task->reach_bits;
    ((player_map_init_fn)BFV_PLAYER_MAP_SEARCH_INIT)(task->search, task->map, task->point, point, task->point,
                                                     0, &task->search_result, low, high, reach_bits);
    ((queue_fn)BFV_TASK_QUEUE)(task, task->search, *(const volatile double *)BFV_TASK_ZERO, 0);
    return ((task_sleep_fn)BFV_TASK_SLEEP)(task, *(const volatile double *)BFV_TASK_ZERO);
}
