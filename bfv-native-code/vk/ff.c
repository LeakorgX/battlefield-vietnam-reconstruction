/* Fixed-function vertex and pixel pipelines generated as vs_3_0 / ps_3_0
 * tokens (minor 0xff), following the D3D9 fixed-function equations. */
#include "ff.h"
#include "vkp.h"

enum { R_TEMP = 0, R_INPUT = 1, R_CONST = 2, R_TEX = 3, R_OUTPUT = 6, R_COLOROUT = 8, R_SAMPLER = 10 };
enum { O_MOV = 1, O_ADD, O_SUB, O_MAD, O_MUL, O_RCP, O_RSQ, O_DP3, O_DP4, O_MIN, O_MAX, O_SLT, O_SGE, O_EXP,
       O_M4x4 = 20, O_M3x3 = 23, O_DCL = 31, O_POW = 32, O_NRM = 36, O_LRP = 18, O_DEF = 81, O_TEX = 66 };
enum { MOD_NONE = 0, MOD_NEG = 1, MOD_BX2 = 4, MOD_COMP = 6, MOD_ABS = 11 };
#define XYZW 0xe4u
#define XXXX 0x00u
#define YYYY 0x55u
#define ZZZZ 0xaau
#define WWWW 0xffu
#define SWZ(x, y, z, w) ((uint32_t)((x) | (y) << 2 | (z) << 4 | (w) << 6))
#define MX 1u
#define MY 2u
#define MZ 4u
#define MW 8u
#define MXYZ 7u
#define MALL 15u

struct gen { uint32_t *w; uint32_t n, cap; int fail; };

static void put(struct gen *g, uint32_t t)
{
    if (g->n == g->cap) {
        uint32_t cap = g->cap ? g->cap * 2 : 1024;
        uint32_t *w = vkp_realloc(g->w, cap * 4);
        if (!w) { g->fail = 1; return; }
        g->w = w; g->cap = cap;
    }
    g->w[g->n++] = t;
}

static uint32_t rt(uint32_t type) { return (type & 7u) << 28 | (type & 0x18u) << 8; }
/* destination: mask, saturate, shift (signed) */
static uint32_t dst(uint32_t type, uint32_t num, uint32_t mask) { return 0x80000000u | rt(type) | mask << 16 | num; }
static uint32_t dsat(uint32_t d) { return d | 1u << 20; }
static uint32_t dshift(uint32_t d, int s) { return d | ((uint32_t)s & 15u) << 24; }
static uint32_t src(uint32_t type, uint32_t num, uint32_t swz, uint32_t mod) { return 0x80000000u | rt(type) | swz << 16 | mod << 24 | num; }

static void op1(struct gen *g, uint32_t op, uint32_t d, uint32_t a) { put(g, op | 2u << 24); put(g, d); put(g, a); }
static void op2(struct gen *g, uint32_t op, uint32_t d, uint32_t a, uint32_t b) { put(g, op | 3u << 24); put(g, d); put(g, a); put(g, b); }
static void op3(struct gen *g, uint32_t op, uint32_t d, uint32_t a, uint32_t b, uint32_t c)
{
    put(g, op | 4u << 24); put(g, d); put(g, a); put(g, b); put(g, c);
}
static void dcl(struct gen *g, uint32_t usage, uint32_t index, uint32_t d)
{
    put(g, O_DCL | 2u << 24); put(g, 0x80000000u | usage | index << 16); put(g, d);
}
static void dcl_sampler(struct gen *g, uint32_t kind, uint32_t n)
{
    put(g, O_DCL | 2u << 24); put(g, 0x80000000u | kind << 27); put(g, dst(R_SAMPLER, n, MALL));
}
static void def(struct gen *g, uint32_t n, float x, float y, float z, float w)
{
    union { float f; uint32_t u; } v[4] = {{x}, {y}, {z}, {w}};
    put(g, O_DEF | 5u << 24); put(g, dst(R_CONST, n, MALL));
    for (int i = 0; i < 4; i++) put(g, v[i].u);
}

static uint32_t *finish(struct gen *g, uint32_t *words)
{
    put(g, 0x0000ffffu);
    if (g->fail) { vkp_free(g->w); return 0; }
    *words = g->n;
    return g->w;
}

/* shorthand */
#define T(n) src(R_TEMP, n, XYZW, 0)
#define TS(n, s) src(R_TEMP, n, s, 0)
#define TD(n, m) dst(R_TEMP, n, m)
#define C(n) src(R_CONST, n, XYZW, 0)
#define CS(n, s) src(R_CONST, n, s, 0)
#define V(n) src(R_INPUT, n, XYZW, 0)
#define VS_(n, s) src(R_INPUT, n, s, 0)
#define OD(n, m) dst(R_OUTPUT, n, m)

/* ---- vertex ---- */

enum { VO_POS, VO_COLOR0, VO_COLOR1, VO_FOG, VO_PSIZE, VO_TEX };
enum { D3DUSAGE_POSITION = 0, D3DUSAGE_BLENDWEIGHT = 1, D3DUSAGE_NORMAL = 3, D3DUSAGE_PSIZE = 4, D3DUSAGE_TEXCOORD = 5,
       D3DUSAGE_COLOR = 10, D3DUSAGE_FOG = 11 };
#define K FF_VS_CONSTS   /* (0, 1, -1, 0.5) */
#define K2 (FF_VS_CONSTS + 1) /* (2, 1.442695, 0, 0) */

static uint32_t mat_color(const struct ff_vs_key *k, int which)
{
    int s = k->color_vertex ? k->mat_src[which] : 0;
    static const uint32_t mat[4] = {FF_VS_MAT_DIFFUSE, FF_VS_MAT_AMBIENT, FF_VS_MAT_SPECULAR, FF_VS_MAT_EMISSIVE};
    if (s == 1 && k->has_color[0]) return V(FF_IN_COLOR0);
    if (s == 2 && k->has_color[1]) return V(FF_IN_COLOR1);
    return C(mat[which]);
}

/* r0 view-space position, r1 view-space normal, r2 diffuse sum, r3 specular sum, r4 ambient sum,
 * r5-r9 scratch, r10 eye vector, r11 texture coordinate scratch */
static void vs_lighting(struct gen *g, const struct ff_vs_key *k)
{
    op1(g, O_MOV, TD(2, MALL), CS(K, XXXX));
    op1(g, O_MOV, TD(3, MALL), CS(K, XXXX));
    op1(g, O_MOV, TD(4, MALL), CS(K, XXXX));
    if (k->specular) {
        if (k->local_viewer) {
            op1(g, O_NRM, TD(10, MXYZ), T(0));
            op1(g, O_MOV, TD(10, MXYZ), src(R_TEMP, 10, XYZW, MOD_NEG));
        } else op1(g, O_MOV, TD(10, MXYZ), CS(K, SWZ(0, 0, 2, 0)));  /* (0, 0, -1) */
    }
    for (int i = 0; i < 8 && k->light[i]; i++) {
        uint32_t L = FF_VS_LIGHT + (uint32_t)i * FF_LIGHT_STRIDE;
        int type = k->light[i];
        if (type == FF_LIGHT_DIRECTIONAL_T) {
            op1(g, O_MOV, TD(5, MXYZ), C(L + FF_LIGHT_DIRECTION));
            op1(g, O_MOV, TD(6, MX), CS(K, YYYY));
        } else {
            op2(g, O_ADD, TD(5, MXYZ), C(L + FF_LIGHT_POSITION), src(R_TEMP, 0, XYZW, MOD_NEG));
            op2(g, O_DP3, TD(6, MY), T(5), T(5));                     /* d^2 */
            op1(g, O_RSQ, TD(6, MZ), TS(6, YYYY));                    /* 1/d */
            op2(g, O_MUL, TD(5, MXYZ), T(5), TS(6, ZZZZ));            /* L */
            op2(g, O_MUL, TD(6, MW), TS(6, YYYY), TS(6, ZZZZ));       /* d */
            op3(g, O_MAD, TD(6, MX), CS(L + FF_LIGHT_ATTEN, WWWW), TS(6, YYYY), CS(L + FF_LIGHT_ATTEN, YYYY));
            op3(g, O_MAD, TD(6, MX), CS(L + FF_LIGHT_ATTEN, ZZZZ), TS(6, WWWW), TS(6, XXXX));
            op1(g, O_RCP, TD(6, MX), TS(6, XXXX));
            op2(g, O_SGE, TD(7, MX), CS(L + FF_LIGHT_ATTEN, XXXX), TS(6, WWWW));  /* d <= range */
            op2(g, O_MUL, TD(6, MX), TS(6, XXXX), TS(7, XXXX));
            if (type == FF_LIGHT_SPOT_T) {
                op2(g, O_DP3, TD(8, MX), T(5), C(L + FF_LIGHT_DIRECTION));          /* rho */
                op2(g, O_ADD, TD(8, MY), TS(8, XXXX), src(R_CONST, L + FF_LIGHT_SPOT, XXXX, MOD_NEG));
                op2(g, O_MUL, dsat(TD(8, MY)), TS(8, YYYY), CS(L + FF_LIGHT_SPOT, YYYY));
                op2(g, O_POW, TD(8, MZ), TS(8, YYYY), CS(L + FF_LIGHT_SPOT, ZZZZ));
                op2(g, O_SLT, TD(8, MW), CS(L + FF_LIGHT_SPOT, XXXX), TS(8, XXXX));  /* rho > cos(phi/2) */
                op2(g, O_MUL, TD(8, MZ), TS(8, ZZZZ), TS(8, WWWW));
                op2(g, O_MUL, TD(6, MX), TS(6, XXXX), TS(8, ZZZZ));
            }
        }
        /* r6.x attenuation * spot, r5 direction to the light */
        op2(g, O_DP3, TD(7, MX), T(1), T(5));
        op2(g, O_MAX, TD(7, MY), TS(7, XXXX), CS(K, XXXX));
        op2(g, O_MUL, TD(7, MY), TS(7, YYYY), TS(6, XXXX));
        op3(g, O_MAD, TD(2, MALL), C(L + FF_LIGHT_DIFFUSE), TS(7, YYYY), T(2));
        op3(g, O_MAD, TD(4, MALL), C(L + FF_LIGHT_AMBIENT), TS(6, XXXX), T(4));
        if (k->specular) {
            op2(g, O_ADD, TD(8, MXYZ), T(5), T(10));
            op1(g, O_NRM, TD(8, MXYZ), T(8));
            op2(g, O_DP3, TD(7, MZ), T(1), T(8));
            op2(g, O_MAX, TD(7, MZ), TS(7, ZZZZ), CS(K, XXXX));
            op2(g, O_POW, TD(7, MW), TS(7, ZZZZ), CS(FF_VS_MAT_POWER, XXXX));
            op2(g, O_SLT, TD(9, MX), CS(K, XXXX), TS(7, XXXX));                       /* N.L > 0 */
            op2(g, O_MUL, TD(7, MW), TS(7, WWWW), TS(9, XXXX));
            op2(g, O_MUL, TD(7, MW), TS(7, WWWW), TS(6, XXXX));
            op3(g, O_MAD, TD(3, MALL), C(L + FF_LIGHT_SPECULAR), TS(7, WWWW), T(3));
        }
    }
    uint32_t cd = mat_color(k, 0), ca = mat_color(k, 1), cs = mat_color(k, 2), ce = mat_color(k, 3);
    op2(g, O_ADD, TD(4, MXYZ), T(4), C(FF_VS_AMBIENT));
    op2(g, O_MUL, TD(5, MXYZ), T(4), ca);
    op2(g, O_ADD, TD(5, MXYZ), T(5), ce);
    op3(g, O_MAD, TD(5, MXYZ), T(2), cd, T(5));
    op1(g, O_MOV, TD(5, MW), cd);
    op1(g, O_MOV, dsat(OD(VO_COLOR0, MALL)), T(5));
    if (k->specular) {
        op2(g, O_MUL, TD(6, MXYZ), T(3), cs);
    } else op1(g, O_MOV, TD(6, MXYZ), CS(K, XXXX));
    op1(g, O_MOV, TD(6, MW), cs);
    op1(g, O_MOV, dsat(OD(VO_COLOR1, MALL)), T(6));
}

static void vs_texcoords(struct gen *g, const struct ff_vs_key *k)
{
    for (uint32_t s = 0; s < k->ntex && s < 8; s++) {
        uint32_t idx = k->tc_index[s] & 7, gen = k->tc_gen[s], count = k->tc_count[s];
        uint32_t o = VO_TEX + s, in = FF_IN_TEXCOORD + idx;
        if (gen == 0 && !k->in_dim[idx] && !count) { op1(g, O_MOV, OD(o, MALL), CS(K, SWZ(0, 0, 0, 1))); continue; }
        if (gen == 0 && !count) { op1(g, O_MOV, OD(o, MALL), V(in)); continue; }
        switch (gen) {
        case 0: {
            uint32_t dim = k->in_dim[idx];
            if (dim >= 4 || dim == 0) op1(g, O_MOV, TD(11, MALL), V(in));
            else {
                /* the component after the last one is 1, the rest 0 */
                op1(g, O_MOV, TD(11, MALL), CS(K, XXXX));
                op1(g, O_MOV, TD(11, (1u << dim) - 1), V(in));
                op1(g, O_MOV, TD(11, 1u << dim), CS(K, YYYY));
            }
            break;
        }
        case 1: op1(g, O_MOV, TD(11, MXYZ), T(1)); op1(g, O_MOV, TD(11, MW), CS(K, YYYY)); break;
        case 2: op1(g, O_MOV, TD(11, MXYZ), T(0)); op1(g, O_MOV, TD(11, MW), CS(K, YYYY)); break;
        case 3: case 4: {
            /* R = I - 2 (N.I) N, I = normalised camera-to-vertex */
            op1(g, O_NRM, TD(8, MXYZ), T(0));
            op2(g, O_DP3, TD(9, MX), T(1), T(8));
            op2(g, O_MUL, TD(9, MX), TS(9, XXXX), CS(K2, XXXX));
            op3(g, O_MAD, TD(11, MXYZ), src(R_TEMP, 1, XYZW, MOD_NEG), TS(9, XXXX), T(8));
            op1(g, O_MOV, TD(11, MW), CS(K, YYYY));
            if (gen == 4) {
                /* sphere map: m = 2 sqrt(Rx^2 + Ry^2 + (Rz + 1)^2), uv = R.xy / m + 0.5 */
                op2(g, O_ADD, TD(9, MXYZ), T(11), CS(K, SWZ(0, 0, 1, 0)));
                op2(g, O_DP3, TD(9, MX), T(9), T(9));
                op1(g, O_RSQ, TD(9, MX), TS(9, XXXX));
                op2(g, O_MUL, TD(9, MX), TS(9, XXXX), CS(K, WWWW));          /* 1/m */
                op3(g, O_MAD, TD(11, MX | MY), T(11), TS(9, XXXX), CS(K, WWWW));
                op1(g, O_MOV, TD(11, MZ), CS(K, XXXX));
            }
            break;
        }
        default: op1(g, O_MOV, TD(11, MALL), V(in)); break;
        }
        if (count) {
            op2(g, O_M4x4, TD(11, MALL), T(11), C(FF_VS_TEXTURE + s * 4));
            if (k->tc_proj[s] && count >= 2 && count <= 3)
                op1(g, O_MOV, TD(11, MW), TS(11, (count - 1) * 0x55u));
        }
        op1(g, O_MOV, OD(o, MALL), T(11));
    }
}

uint32_t *ff_vs_generate(const struct ff_vs_key *k, uint32_t *words)
{
    struct gen g0 = {0}, *g = &g0;
    put(g, 0xfffe03ffu);
    dcl(g, k->rhw ? 9 : D3DUSAGE_POSITION, 0, dst(R_INPUT, FF_IN_POSITION, MALL));
    if (k->has_normal) dcl(g, D3DUSAGE_NORMAL, 0, dst(R_INPUT, FF_IN_NORMAL, MALL));
    if (k->has_color[0]) dcl(g, D3DUSAGE_COLOR, 0, dst(R_INPUT, FF_IN_COLOR0, MALL));
    if (k->has_color[1]) dcl(g, D3DUSAGE_COLOR, 1, dst(R_INPUT, FF_IN_COLOR1, MALL));
    if (k->has_psize) dcl(g, D3DUSAGE_PSIZE, 0, dst(R_INPUT, FF_IN_PSIZE, MALL));
    if (k->blend && !k->rhw) dcl(g, D3DUSAGE_BLENDWEIGHT, 0, dst(R_INPUT, FF_IN_BLENDWEIGHT, MALL));
    for (uint32_t i = 0; i < 8; i++)
        if (k->in_dim[i]) dcl(g, D3DUSAGE_TEXCOORD, i, dst(R_INPUT, FF_IN_TEXCOORD + i, MALL));
    dcl(g, D3DUSAGE_POSITION, 0, OD(VO_POS, MALL));
    dcl(g, D3DUSAGE_COLOR, 0, OD(VO_COLOR0, MALL));
    dcl(g, D3DUSAGE_COLOR, 1, OD(VO_COLOR1, MALL));
    if (k->fog) dcl(g, D3DUSAGE_FOG, 0, OD(VO_FOG, MX));
    int psize = k->has_psize || (k->point_scale && !k->rhw);
    if (psize) dcl(g, D3DUSAGE_PSIZE, 0, OD(VO_PSIZE, MALL));
    for (uint32_t s = 0; s < k->ntex && s < 8; s++) dcl(g, D3DUSAGE_TEXCOORD, s, OD(VO_TEX + s, MALL));
    def(g, K, 0, 1, -1, 0.5f);
    def(g, K2, 2, 1.442695f, 0, 0);

    if (k->rhw) {
        /* window coordinates back to clip space; rhw 0 counts as 1 */
        op3(g, O_MAD, TD(5, MXYZ), V(FF_IN_POSITION), C(FF_VS_VIEWPORT_SCALE), C(FF_VS_VIEWPORT_OFFSET));
        op1(g, O_RCP, TD(6, MY), VS_(FF_IN_POSITION, WWWW));
        op2(g, O_SGE, TD(6, MX), src(R_INPUT, FF_IN_POSITION, WWWW, 12 /* -|x| */), CS(K, XXXX));
        op2(g, O_ADD, TD(6, MZ), CS(K, YYYY), src(R_TEMP, 6, XXXX, MOD_NEG));
        op2(g, O_MUL, TD(6, MY), TS(6, YYYY), TS(6, ZZZZ));
        op2(g, O_ADD, TD(6, MY), TS(6, YYYY), TS(6, XXXX));
        op2(g, O_MUL, OD(VO_POS, MXYZ), T(5), TS(6, YYYY));
        op1(g, O_MOV, OD(VO_POS, MW), TS(6, YYYY));
        op1(g, O_MOV, OD(VO_COLOR0, MALL), k->has_color[0] ? V(FF_IN_COLOR0) : CS(K, YYYY));
        op1(g, O_MOV, OD(VO_COLOR1, MALL), k->has_color[1] ? V(FF_IN_COLOR1) : CS(K, XXXX));
        if (k->fog) op1(g, O_MOV, OD(VO_FOG, MX), k->has_color[1] ? VS_(FF_IN_COLOR1, WWWW) : CS(K, XXXX));
        if (psize) op1(g, O_MOV, OD(VO_PSIZE, MALL), VS_(FF_IN_PSIZE, XXXX));
        op1(g, O_MOV, TD(0, MALL), V(FF_IN_POSITION));
        op1(g, O_MOV, TD(1, MALL), CS(K, XXXX));
        vs_texcoords(g, k);
        return finish(g, words);
    }

    /* position and normal in view space */
    if (!k->blend) {
        op2(g, O_M4x4, TD(0, MALL), V(FF_IN_POSITION), C(FF_VS_WORLDVIEW));
        if (k->has_normal) op2(g, O_M3x3, TD(1, MXYZ), V(FF_IN_NORMAL), C(FF_VS_NORMAL));
    } else {
        /* the last weight is 1 - the others */
        op1(g, O_MOV, TD(7, MX), CS(K, YYYY));
        for (uint32_t i = 0; i < k->blend; i++)
            op2(g, O_ADD, TD(7, MX), TS(7, XXXX), src(R_INPUT, FF_IN_BLENDWEIGHT, i * 0x55u, MOD_NEG));
        for (uint32_t i = 0; i <= k->blend; i++) {
            uint32_t w = i < k->blend ? src(R_INPUT, FF_IN_BLENDWEIGHT, i * 0x55u, 0) : TS(7, XXXX);
            op2(g, O_M4x4, TD(5, MALL), V(FF_IN_POSITION), C(FF_VS_WORLDVIEW + i * 4));
            if (i == 0) op2(g, O_MUL, TD(0, MALL), T(5), w);
            else op3(g, O_MAD, TD(0, MALL), T(5), w, T(0));
            if (k->has_normal) {
                op2(g, O_M3x3, TD(6, MXYZ), V(FF_IN_NORMAL), C(FF_VS_NORMAL + i * 4));
                if (i == 0) op2(g, O_MUL, TD(1, MXYZ), T(6), w);
                else op3(g, O_MAD, TD(1, MXYZ), T(6), w, T(1));
            }
        }
    }
    if (!k->has_normal) op1(g, O_MOV, TD(1, MALL), CS(K, XXXX));
    else if (k->normalize) op1(g, O_NRM, TD(1, MXYZ), T(1));
    op2(g, O_M4x4, OD(VO_POS, MALL), T(0), C(FF_VS_PROJ));

    if (k->lighting) vs_lighting(g, k);
    else {
        op1(g, O_MOV, OD(VO_COLOR0, MALL), k->has_color[0] ? V(FF_IN_COLOR0) : CS(K, YYYY));
        op1(g, O_MOV, OD(VO_COLOR1, MALL), k->has_color[1] ? V(FF_IN_COLOR1) : CS(K, XXXX));
    }

    if (k->fog == FF_FOG_SPECULAR_ALPHA) {
        uint32_t sa = k->lighting ? TS(6, WWWW) : k->has_color[1] ? VS_(FF_IN_COLOR1, WWWW) : CS(K, XXXX);
        op1(g, O_MOV, OD(VO_FOG, MX), sa);
    } else if (k->fog) {
        if (k->range_fog) {
            op2(g, O_DP3, TD(5, MX), T(0), T(0));
            op1(g, O_RSQ, TD(5, MX), TS(5, XXXX));
            op1(g, O_RCP, TD(5, MX), TS(5, XXXX));
        } else op1(g, O_MOV, TD(5, MX), src(R_TEMP, 0, ZZZZ, MOD_ABS));
        if (k->fog == FF_FOG_LINEAR) {
            op2(g, O_ADD, TD(5, MY), CS(FF_VS_FOG, YYYY), src(R_TEMP, 5, XXXX, MOD_NEG));
            op2(g, O_MUL, OD(VO_FOG, MX), TS(5, YYYY), CS(FF_VS_FOG, ZZZZ));
        } else {
            op2(g, O_MUL, TD(5, MX), TS(5, XXXX), CS(FF_VS_FOG, WWWW));
            if (k->fog == FF_FOG_EXP2) op2(g, O_MUL, TD(5, MX), TS(5, XXXX), TS(5, XXXX));
            op2(g, O_MUL, TD(5, MX), TS(5, XXXX), src(R_CONST, K2, YYYY, MOD_NEG));
            op1(g, O_EXP, OD(VO_FOG, MX), TS(5, XXXX));
        }
    }
    if (psize) {
        op1(g, O_MOV, TD(5, MX), k->has_psize ? VS_(FF_IN_PSIZE, XXXX) : CS(FF_VS_POINT, XXXX));
        if (k->point_scale) {
            /* size * viewport height * sqrt(1 / (A + B d + C d^2)) */
            op2(g, O_DP3, TD(6, MY), T(0), T(0));
            op1(g, O_RSQ, TD(6, MZ), TS(6, YYYY));
            op2(g, O_MUL, TD(6, MX), TS(6, YYYY), TS(6, ZZZZ));
            op3(g, O_MAD, TD(7, MX), CS(FF_VS_POINT_SCALE, ZZZZ), TS(6, YYYY), CS(FF_VS_POINT_SCALE, XXXX));
            op3(g, O_MAD, TD(7, MX), CS(FF_VS_POINT_SCALE, YYYY), TS(6, XXXX), TS(7, XXXX));
            op1(g, O_RSQ, TD(7, MX), TS(7, XXXX));
            op2(g, O_MUL, TD(5, MX), TS(5, XXXX), TS(7, XXXX));
            op2(g, O_MUL, TD(5, MX), TS(5, XXXX), CS(FF_VS_POINT, WWWW));
        }
        op1(g, O_MOV, OD(VO_PSIZE, MALL), TS(5, XXXX));
    }
    vs_texcoords(g, k);
    return finish(g, words);
}

/* ---- pixel ---- */

enum {
    TOP_DISABLE = 1, TOP_SELECTARG1, TOP_SELECTARG2, TOP_MODULATE, TOP_MODULATE2X, TOP_MODULATE4X, TOP_ADD,
    TOP_ADDSIGNED, TOP_ADDSIGNED2X, TOP_SUBTRACT, TOP_ADDSMOOTH, TOP_BLENDDIFFUSEALPHA, TOP_BLENDTEXTUREALPHA,
    TOP_BLENDFACTORALPHA, TOP_BLENDTEXTUREALPHAPM, TOP_BLENDCURRENTALPHA, TOP_PREMODULATE,
    TOP_MODULATEALPHA_ADDCOLOR, TOP_MODULATECOLOR_ADDALPHA, TOP_MODULATEINVALPHA_ADDCOLOR,
    TOP_MODULATEINVCOLOR_ADDALPHA, TOP_BUMPENVMAP, TOP_BUMPENVMAPLUMINANCE, TOP_DOTPRODUCT3, TOP_MULTIPLYADD,
    TOP_LERP,
};
enum { TA_DIFFUSE, TA_CURRENT, TA_TEXTURE, TA_TFACTOR, TA_SPECULAR, TA_TEMP, TA_CONSTANT };
#define TA_COMPLEMENT 0x10
#define TA_ALPHAREPLICATE 0x20
#define PK FF_PS_CONSTS   /* (0, 1, 0.5, 0) */
/* r0 current, r1 temp, r2 this stage's texture, r3 coordinates, r4 previous bump texture, r5 result */
#define PV_COLOR0 8
#define PV_COLOR1 9

static int arg_uses_texture(uint8_t a) { return (a & 7) == TA_TEXTURE; }
static int uses_texture(const struct ff_stage *s)
{
    if (s->cop == TOP_BLENDTEXTUREALPHA || s->cop == TOP_BLENDTEXTUREALPHAPM || s->aop == TOP_BLENDTEXTUREALPHA ||
        s->aop == TOP_BLENDTEXTUREALPHAPM || s->cop == TOP_BUMPENVMAP || s->cop == TOP_BUMPENVMAPLUMINANCE)
        return 1;
    for (int i = 0; i < 3; i++)
        if (arg_uses_texture(s->carg[i]) || (s->aop != TOP_DISABLE && arg_uses_texture(s->aarg[i]))) return 1;
    return 0;
}

static uint32_t arg_src(uint8_t a, uint32_t stage)
{
    uint32_t s;
    switch (a & 7) {
    case TA_DIFFUSE: s = src(R_INPUT, PV_COLOR0, XYZW, 0); break;
    case TA_CURRENT: s = T(0); break;
    case TA_TEXTURE: s = T(2); break;
    case TA_TFACTOR: s = C(FF_PS_TFACTOR); break;
    case TA_SPECULAR: s = src(R_INPUT, PV_COLOR1, XYZW, 0); break;
    case TA_TEMP: s = T(1); break;
    default: s = C(FF_PS_STAGE_CONST + stage); break;
    }
    if (a & TA_ALPHAREPLICATE) s = (s & ~(0xffu << 16)) | WWWW << 16;
    if (a & TA_COMPLEMENT) s |= (uint32_t)MOD_COMP << 24;
    return s;
}

static uint32_t with_swz(uint32_t s, uint32_t swz) { return (s & ~(0xffu << 16)) | swz << 16; }
static uint32_t with_mod(uint32_t s, uint32_t mod) { return (s & ~(0xfu << 24)) | mod << 24; }

/* one colour or alpha operation into r5 under `mask` */
static void blend_op(struct gen *g, int op, const uint8_t *arg, uint32_t stage, uint32_t mask)
{
    uint32_t a0 = arg_src(arg[0], stage), a1 = arg_src(arg[1], stage), a2 = arg_src(arg[2], stage);
    uint32_t d = TD(5, mask);
    switch (op) {
    case TOP_SELECTARG1: case TOP_PREMODULATE: op1(g, O_MOV, d, a1); break;
    case TOP_SELECTARG2: op1(g, O_MOV, d, a2); break;
    case TOP_MODULATE: op2(g, O_MUL, d, a1, a2); break;
    case TOP_MODULATE2X: op2(g, O_MUL, dshift(d, 1), a1, a2); break;
    case TOP_MODULATE4X: op2(g, O_MUL, dshift(d, 2), a1, a2); break;
    case TOP_ADD: op2(g, O_ADD, d, a1, a2); break;
    case TOP_ADDSIGNED:
        op2(g, O_ADD, d, a1, a2);
        op2(g, O_ADD, d, T(5), src(R_CONST, PK, ZZZZ, MOD_NEG));
        break;
    case TOP_ADDSIGNED2X:
        op2(g, O_ADD, d, a1, a2);
        op2(g, O_ADD, dshift(d, 1), T(5), src(R_CONST, PK, ZZZZ, MOD_NEG));
        break;
    case TOP_SUBTRACT:
        if ((a2 >> 24 & 15) == MOD_COMP) { /* a1 - (1 - x) = a1 + x - 1 */
            op2(g, O_ADD, d, a1, with_mod(a2, 0));
            op2(g, O_ADD, d, T(5), src(R_CONST, PK, YYYY, MOD_NEG));
        } else op2(g, O_ADD, d, a1, with_mod(a2, MOD_NEG));
        break;
    case TOP_ADDSMOOTH: op3(g, O_MAD, d, with_mod(a1, MOD_COMP), a2, a1); break;
    case TOP_BLENDDIFFUSEALPHA: op3(g, O_LRP, d, src(R_INPUT, PV_COLOR0, WWWW, 0), a1, a2); break;
    case TOP_BLENDTEXTUREALPHA: op3(g, O_LRP, d, TS(2, WWWW), a1, a2); break;
    case TOP_BLENDFACTORALPHA: op3(g, O_LRP, d, CS(FF_PS_TFACTOR, WWWW), a1, a2); break;
    case TOP_BLENDCURRENTALPHA: op3(g, O_LRP, d, TS(0, WWWW), a1, a2); break;
    case TOP_BLENDTEXTUREALPHAPM: op3(g, O_MAD, d, a2, src(R_TEMP, 2, WWWW, MOD_COMP), a1); break;
    case TOP_MODULATEALPHA_ADDCOLOR: op3(g, O_MAD, d, with_swz(a1, WWWW), a2, a1); break;
    case TOP_MODULATECOLOR_ADDALPHA: op3(g, O_MAD, d, a1, a2, with_swz(a1, WWWW)); break;
    case TOP_MODULATEINVALPHA_ADDCOLOR: op3(g, O_MAD, d, with_mod(with_swz(a1, WWWW), MOD_COMP), a2, a1); break;
    case TOP_MODULATEINVCOLOR_ADDALPHA: op3(g, O_MAD, d, with_mod(a1, MOD_COMP), a2, with_swz(a1, WWWW)); break;
    case TOP_DOTPRODUCT3: op2(g, O_DP3, TD(5, MALL), with_mod(a1, MOD_BX2), with_mod(a2, MOD_BX2)); break;
    case TOP_MULTIPLYADD: op3(g, O_MAD, d, a1, a2, a0); break;
    case TOP_LERP: op3(g, O_LRP, d, a0, a1, a2); break;
    default: op1(g, O_MOV, d, T(0)); break;
    }
}

uint32_t *ff_ps_generate(const struct ff_ps_key *k, uint32_t *words)
{
    struct gen g0 = {0}, *g = &g0;
    put(g, 0xffff03ffu);
    int nst = 0;
    while (nst < 8 && k->st[nst].cop != TOP_DISABLE && k->st[nst].cop != 0) nst++;
    uint32_t need_tex = 0;
    for (int s = 0; s < nst; s++) {
        if (uses_texture(&k->st[s])) need_tex |= 1u << s;
        if (s > 0 && (k->st[s - 1].cop == TOP_BUMPENVMAP || k->st[s - 1].cop == TOP_BUMPENVMAPLUMINANCE))
            need_tex |= 1u << s;
    }
    for (int s = 0; s < nst; s++) {
        if (!(need_tex >> s & 1) || !k->st[s].tex) continue;
        dcl(g, D3DUSAGE_TEXCOORD, (uint32_t)s, dst(R_INPUT, (uint32_t)s, MALL));
        static const uint32_t kinds[3] = {2, 3, 4};
        dcl_sampler(g, kinds[(k->st[s].tex - 1) % 3], (uint32_t)s);
    }
    dcl(g, D3DUSAGE_COLOR, 0, dst(R_INPUT, PV_COLOR0, MALL));
    dcl(g, D3DUSAGE_COLOR, 1, dst(R_INPUT, PV_COLOR1, MALL));
    def(g, PK, 0, 1, 0.5f, 0);
    op1(g, O_MOV, TD(0, MALL), src(R_INPUT, PV_COLOR0, XYZW, 0));
    op1(g, O_MOV, TD(1, MALL), CS(PK, XXXX));
    for (int s = 0; s < nst; s++) {
        const struct ff_stage *st = &k->st[s];
        int prev_bump = s > 0 && (k->st[s - 1].cop == TOP_BUMPENVMAP || k->st[s - 1].cop == TOP_BUMPENVMAPLUMINANCE);
        if (need_tex >> s & 1) {
            if (!st->tex) op1(g, O_MOV, TD(2, MALL), CS(PK, YYYY));
            else {
                uint32_t coord = V((uint32_t)s);
                if (prev_bump) {
                    uint32_t b = FF_PS_BUMP + (uint32_t)s - 1;
                    op1(g, O_MOV, TD(3, MALL), V((uint32_t)s));
                    op3(g, O_MAD, TD(3, MX), CS(b, XXXX), TS(4, XXXX), TS(3, XXXX));
                    op3(g, O_MAD, TD(3, MX), CS(b, ZZZZ), TS(4, YYYY), TS(3, XXXX));
                    op3(g, O_MAD, TD(3, MY), CS(b, YYYY), TS(4, XXXX), TS(3, YYYY));
                    op3(g, O_MAD, TD(3, MY), CS(b, WWWW), TS(4, YYYY), TS(3, YYYY));
                    coord = T(3);
                }
                put(g, O_TEX | (st->proj ? 1u : 0u) << 16 | 3u << 24);
                put(g, TD(2, MALL)); put(g, coord); put(g, src(R_SAMPLER, (uint32_t)s, XYZW, 0));
                if (prev_bump && k->st[s - 1].cop == TOP_BUMPENVMAPLUMINANCE) {
                    uint32_t l = FF_PS_LUM + (uint32_t)s - 1;
                    op3(g, O_MAD, dsat(TD(3, MX)), TS(4, ZZZZ), CS(l, XXXX), CS(l, YYYY));
                    op2(g, O_MUL, TD(2, MXYZ), T(2), TS(3, XXXX));
                }
            }
        }
        if (st->cop == TOP_BUMPENVMAP || st->cop == TOP_BUMPENVMAPLUMINANCE) {
            op1(g, O_MOV, TD(4, MALL), T(2));
            continue;
        }
        uint32_t out = st->temp ? 1 : 0;
        if (st->cop == TOP_DOTPRODUCT3) {
            blend_op(g, st->cop, st->carg, (uint32_t)s, MALL);
            op1(g, O_MOV, dsat(TD(out, MALL)), T(5));
            continue;
        }
        blend_op(g, st->cop, st->carg, (uint32_t)s, MXYZ);
        if (st->aop == TOP_DISABLE || st->aop == 0) op1(g, O_MOV, TD(5, MW), TS(0, WWWW));
        else blend_op(g, st->aop, st->aarg, (uint32_t)s, MW);
        op1(g, O_MOV, dsat(TD(out, MALL)), T(5));
    }
    if (k->specular) op2(g, O_ADD, dsat(TD(0, MXYZ)), T(0), src(R_INPUT, PV_COLOR1, XYZW, 0));
    op1(g, O_MOV, dst(R_COLOROUT, 0, MALL), T(0));
    return finish(g, words);
}
