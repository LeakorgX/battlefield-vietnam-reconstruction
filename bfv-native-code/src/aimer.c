/* AIBotActionPlan Aimer: target track, linear prediction, projectile offset
 * and the launch-angle search used by bot aiming.
 *
 * Field and parameter names describe how these four routines use them:
 * bfv_aimer_predict moves position by velocity over elapsed time, and the
 * angle search treats speed as launch speed and gravity as the constant term
 * of the height formula. Drag is the coefficient k of the k != 0 formulas.
 *
 * Arithmetic follows the native x87 order. With -std=c11, float expressions
 * are evaluated in long double (FLT_EVAL_METHOD 2) and every float variable
 * rounds where the native code stores a float. Comparisons, trigonometry and
 * the _ftol2 call stay in small x87 helpers so the status word matches. */
#include <stdint.h>
#include "target.h"
#define TC __attribute__((thiscall))

/* Vectors are read as floats and copied as words. */
typedef union Vec3 { float f[3]; uint32_t w[3]; } Vec3;

typedef struct Aimer {
    float drag;                 /* +0x00 */
    uint32_t unused_04;         /* +0x04, not read by these routines */
    float gravity;              /* +0x08 */
    float speed;                /* +0x0c */
    float start_time;           /* +0x10 */
    float last_time;            /* +0x14 */
    Vec3 position;          /* +0x18 */
    Vec3 velocity;          /* +0x24 */
    Vec3 position_30;       /* +0x30, second copy of the tracked position */
    Vec3 velocity_3c;       /* +0x3c, second copy of the tracked velocity */
    Vec3 position_48;       /* +0x48, copied from +0x18 */
    Vec3 velocity_54;       /* +0x54, copied from +0x24 */
    Vec3 zero_60;           /* +0x60, cleared on every accepted sample */
    Vec3 zero_6c;           /* +0x6c, cleared on every accepted sample */
    uint32_t samples;           /* +0x78 */
    float half_flight_time;     /* +0x7c, written by the angle search */
} Aimer;

#define K(name) (*(const volatile float *)BFV_##name)

typedef int (*finite_fn)(double);

/* Native trace ring: index = (index + 1) & mask, then file and line. */
static void trace(uint32_t line)
{
    uint32_t index = (*(volatile uint32_t *)BFV_TRACE_INDEX + 1) & *(volatile uint32_t *)BFV_TRACE_MASK;
    *(volatile uint32_t *)BFV_TRACE_INDEX = index;
    *(volatile uint32_t *)(BFV_TRACE_FILES + index * 4) = BFV_AIMER_TRACE_SOURCE;
    *(volatile uint32_t *)(BFV_TRACE_LINES + index * 4) = line;
}

/* Raw float bits, without an x87 load that would quiet a signaling NaN. */
static uint32_t word_at(const volatile void *address)
{ return *(const volatile uint32_t *)(uintptr_t)address; }
static void put_word(volatile void *address, uint32_t value)
{ *(volatile uint32_t *)(uintptr_t)address = value; }

/* Word copies keep NaN payloads; each word is read just before it is written,
 * as natively, so overlapping source and destination behave the same. */
static void copy3(volatile uint32_t *to, const volatile uint32_t *from)
{
    to[0] = from[0];
    to[1] = from[1];
    to[2] = from[2];
}

/* FCOMP against a float or double in memory, returning FNSTSW. */
static uint16_t compare(long double value, const volatile float *bound)
{
    uint16_t status;
    __asm__ volatile("fldt %[value]; fcomps %[bound]; fnstsw %%ax"
        : "=a"(status) : [value]"m"(value), [bound]"m"(*bound) : "st");
    return status;
}
static uint16_t compare_double(long double value, const volatile double *bound)
{
    uint16_t status;
    __asm__ volatile("fldt %[value]; fcompl %[bound]; fnstsw %%ax"
        : "=a"(status) : [value]"m"(value), [bound]"m"(*bound) : "st");
    return status;
}
/* FUCOMPP of a float against the native 0.0f constant. */
static uint16_t compare_zero_unordered(const volatile float *value)
{
    uint16_t status;
    __asm__ volatile("flds %[zero]; flds %[value]; fucompp; fnstsw %%ax"
        : "=a"(status) : [value]"m"(*value), [zero]"m"(K(MATH_ZERO)) : "st");
    return status;
}
/* Status tests used by the native branches. */
static int less(uint16_t status)            /* TEST AH,5 with odd parity */
{ status &= 0x0500; return status == 0x0100 || status == 0x0400; }
static int greater(uint16_t status)         /* TEST AH,41h is zero */
{ return (status & 0x4100) == 0; }
static int less_or_equal(uint16_t status)   /* TEST AH,41h with odd parity */
{ status &= 0x4100; return status == 0x0100 || status == 0x4000; }
static int equal(uint16_t status)           /* TEST AH,44h with odd parity */
{ return (status & 0x4400) == 0x4000; }

static long double cosine(long double x)
{ __asm__("fldt %1; fcos; fstpt %0" : "=m"(x) : "m"(x) : "st"); return x; }
static long double sine(long double x)
{ __asm__("fldt %1; fsin; fstpt %0" : "=m"(x) : "m"(x) : "st"); return x; }
static long double square_root(long double x)
{ __asm__("fldt %1; fsqrt; fstpt %0" : "=m"(x) : "m"(x) : "st"); return x; }
/* FPATAN: atan(y / x) with the quadrant from both signs. */
static long double arc_tangent(long double y, long double x)
{
    long double result;
    __asm__("fldt %1; fldt %2; fpatan; fstpt %0" : "=m"(result) : "m"(y), "m"(x) : "st", "st(1)");
    return result;
}

/* Accept a new target sample unless it is not newer than the last one by
 * more than the native epsilon. Returns 1 when stored. */
uint8_t TC bfv_aimer_track(volatile Aimer *aimer, const volatile uint32_t *position,
                           const volatile uint32_t *velocity, float time)
{
    trace(0x4b);
    if (aimer->samples != 0) {
        long double deadline = aimer->last_time + K(AIMER_TIME_EPSILON);
        /* Equal, older and unordered times are all rejected. */
        if (!less(compare(deadline, &time))) return 0;
    }
    copy3(aimer->position.w, position);
    copy3(aimer->position_30.w, position);
    copy3(aimer->velocity.w, velocity);
    copy3(aimer->velocity_3c.w, velocity);
    uint32_t first = aimer->position.w[0];
    put_word(&aimer->last_time, word_at(&time));
    put_word(&aimer->start_time, word_at(&time));
    aimer->position_48.w[0] = first;
    aimer->position_48.w[1] = aimer->position.w[1];
    aimer->position_48.w[2] = aimer->position.w[2];
    copy3(aimer->velocity_54.w, aimer->velocity.w);
    for (int i = 0; i < 3; i++) aimer->zero_60.w[i] = 0;
    for (int i = 0; i < 3; i++) aimer->zero_6c.w[i] = 0;
    aimer->samples = aimer->samples + 1;
    return 1;
}

/* out = position + velocity * (time - start_time). The x product stays
 * extended; y and z products round to float before the add, as natively. */
void TC bfv_aimer_predict(volatile Aimer *aimer, float time, volatile float *out)
{
    trace(0x105);
    long double elapsed = time - aimer->start_time;
    long double x = elapsed * aimer->velocity.f[0];
    float y = elapsed * aimer->velocity.f[1];
    float z = elapsed * aimer->velocity.f[2];
    long double px = x + aimer->position.f[0];
    long double py = y + aimer->position.f[1];
    long double pz = z + aimer->position.f[2];
    out[0] = px;
    out[1] = py;
    out[2] = pz;
}

/* Projectile displacement after time for launch direction `direction`:
 * without drag, direction*speed*time plus gravity*time^2/2 on y; with drag,
 * the native k-weighted variant. Out is rewritten in place and reread,
 * so it may alias the direction or the aimer. */
void TC bfv_aimer_projectile_offset(volatile Aimer *aimer, volatile Vec3 *out,
                                    const volatile Vec3 *direction, float time)
{
    float gravity = aimer->gravity;
    if (equal(compare_zero_unordered(&aimer->drag))) {
        copy3(out->w, direction->w);
        long double travel = time * aimer->speed;
        long double y = travel * out->f[1];
        out->f[1] = y;
        long double half_square = time * time * K(MATH_HALF);
        float half = half_square;
        /* The x and z terms add half_square * 0.0: zero, but NaN and
         * infinity propagate exactly as natively. */
        long double flat = half_square * K(MATH_ZERO);
        float lift = gravity * half;
        float flat_z = flat;
        out->f[0] = travel * out->f[0] + flat;
        out->f[1] = y + lift;
        out->f[2] = travel * out->f[2] + flat_z;
        return;
    }
    float dx = direction->f[0], dy = direction->f[1], dz = direction->f[2];
    long double speed = aimer->speed;
    float vx = dx * speed;
    float vy = dy * speed;
    long double vz = speed * dz;
    long double drag = aimer->drag;
    float ax = vx * drag;
    out->f[0] = ax;
    long double ay = vy * drag + gravity;
    out->f[1] = ay;
    float az = vz * drag;
    out->f[2] = az;
    float scale = K(MATH_HALF) / (K(MATH_ONE) - aimer->drag);
    ax = ax * scale;
    out->f[0] = ax;
    ay = ay * scale;
    out->f[1] = ay;
    long double square = time * time;
    out->f[2] = az * square * scale;
    long double px = vx * time;
    float py = vy * time;
    float pz = vz * time;
    out->f[0] = ax * square + px;
    out->f[1] = ay * square + py;
    out->f[2] = pz + out->f[2];
}

/* Native angle clamp to [-1000, 1000]; NaN passes through. */
static long double clamp_angle(const volatile float *angle)
{
    if (greater(compare(*angle, &K(AIMER_ANGLE_LIMIT)))) return K(AIMER_ANGLE_LIMIT);
    if (less(compare(*angle, &K(AIMER_NEGATIVE_ANGLE_LIMIT)))) return K(AIMER_NEGATIVE_ANGLE_LIMIT);
    return *angle;
}

/* Horizontal distance sqrt(z*z + x*x) of a predicted position. */
static long double horizontal_distance(const volatile float *point)
{
    long double x = point[0], z = point[2];
    return square_root(z * z + x * x);
}

/* _ftol2(search_scale * 8 + 2): the retained runtime truncation, called
 * with the extended value in ST0. */
static uint32_t iteration_count(const volatile float *search_scale)
{
    uint32_t count;
    __asm__ volatile("flds %[v]; fmuls %[scale]; fadds %[bias]; call *%[ftol]"
        : "=a"(count)
        : [v]"m"(*search_scale), [scale]"m"(K(AIMER_ITERATION_SCALE)),
          [bias]"m"(K(AIMER_ITERATION_BIAS)), [ftol]"r"(BFV_FTOL2)
        : "ecx", "edx", "memory", "st");
    return count;
}

/* Search the launch angle that brings the projectile to the target's
 * predicted position. The step halves after the height error changes sign;
 * the search ends after `search_scale * 8 + 2` refined steps or 1025 passes.
 * On success writes the unit launch direction, optionally the aim time, and
 * returns 1. `high_arc` starts halfway to 1.5079645 rad. */
uint8_t TC bfv_aimer_solve(volatile Aimer *aimer, float time, float search_scale, uint8_t high_arc,
                           volatile float *direction, volatile float *target, volatile uint32_t *aim_time_out)
{
    trace(0x6f);
    bfv_aimer_predict(aimer, time, target);
    float distance = horizontal_distance(target);
    volatile float angle = arc_tangent(target[1], distance);
    if (high_arc) angle = (angle + K(AIMER_HIGH_ARC_ANGLE)) * K(MATH_HALF);
    int32_t direction_sign = 0;
    uint8_t refining = 0;
    volatile float step = K(AIMER_PI) / (search_scale * K(AIMER_STEP_SCALE) + K(AIMER_STEP_BIAS));
    uint8_t has_drag = !equal(compare_zero_unordered(&aimer->drag));
    uint32_t count = iteration_count(&search_scale);
    trace(0x83);
    volatile float aim_time = 0;
    if (count != 0) {
        int32_t passes = 0;
        uint32_t steps = 0;
        do {
            long double flight;
            float height;
            if (!has_drag) {
                flight = distance / (cosine(clamp_angle(&angle)) * aimer->speed);
                long double rise = sine(clamp_angle(&angle)) * aimer->speed;
                height = (rise + flight * aimer->gravity * K(MATH_HALF)) * flight;
            } else {
                long double cos_angle = cosine(clamp_angle(&angle));
                float retained = K(MATH_ONE) - aimer->drag;
                flight = distance / (cos_angle * aimer->speed) * retained;
                long double rise = sine(clamp_angle(&angle)) * aimer->speed;
                height = ((rise * aimer->drag + aimer->gravity) * K(MATH_HALF) / retained * flight + rise) * flight;
            }
            aimer->half_flight_time = K(MATH_HALF) * flight;
            aim_time = flight + time;
            bfv_aimer_predict(aimer, aim_time, target);
            float target_y = target[1];
            distance = horizontal_distance(target);
            if (!((finite_fn)BFV_FINITE)(distance)) return 0;
            if (!((finite_fn)BFV_FINITE)(target_y)) return 0;
            long double error = height - target_y;
            trace(0xb7);
            if (less(compare(error, &K(MATH_ZERO)))) {
                /* Too low: raise the angle, up to 1.5393804. */
                if (!(compare_double(angle, (const volatile double *)BFV_AIMER_UPPER_ANGLE_DOUBLE) & 0x0100)) return 0;
                if (direction_sign == 0) direction_sign = 1;
                else if (direction_sign < 0) refining = 1;
                if (refining) step = step * K(MATH_HALF);
                long double raised = step + angle;
                angle = raised;
                if (greater(compare(raised, &K(AIMER_UPPER_ANGLE)))) put_word(&angle, 0x3fc50a6b);
            } else {
                /* Not too low: lower the angle, down to -1.5393804. */
                if (less_or_equal(compare_double(angle, (const volatile double *)BFV_AIMER_LOWER_ANGLE_DOUBLE))) return 0;
                if (direction_sign == 0) direction_sign = -1;
                else if (direction_sign > 0) refining = 1;
                if (refining) step = step * K(MATH_HALF);
                long double lowered = angle - step;
                angle = lowered;
                if (less(compare(lowered, &K(AIMER_LOWER_ANGLE)))) put_word(&angle, 0xbfc50a6b);
            }
            if (++passes > 0x400) break;
            steps += refining != 0;
        } while (steps < count);
    }
    trace(0xe5);
    if (!refining) {
        trace(0xef);
        return 0;
    }
    long double horizontal = cosine(clamp_angle(&angle)) / distance;
    long double elevation = clamp_angle(&angle);
    float x = horizontal * target[0];
    float y = sine(elevation);
    float z = horizontal * target[2];
    direction[0] = x;
    direction[1] = y;
    direction[2] = z;
    long double dx = direction[0], dy = direction[1], dz = direction[2];
    if (less(compare(dx * dx + dy * dy + dz * dz, &K(AIMER_MINIMUM_LENGTH_SQUARED)))) return 0;
    if (aim_time_out) *aim_time_out = word_at(&aim_time);
    return 1;
}
