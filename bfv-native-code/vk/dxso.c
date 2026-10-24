/* D3D9 shader bytecode (vs 1.1-3.0, ps 1.0-3.0) to SPIR-V.
 *
 * Every D3D register is a Private variable. Inputs are copied into Private
 * arrays at entry and outputs written out at exit, so relative addressing,
 * subroutines (`label`/`call`) and early returns need nothing special. The
 * shader body is its own function, called by the entry point between the
 * input copy and the epilogue (fixed-function fog and alpha test for pixel
 * shaders; clip distances, point size and linkage for vertex shaders). */
#include "dxso.h"
#include "spirv.h"
#include "vkp.h"

enum {
    OP_NOP = 0, OP_MOV, OP_ADD, OP_SUB, OP_MAD, OP_MUL, OP_RCP, OP_RSQ, OP_DP3, OP_DP4, OP_MIN, OP_MAX, OP_SLT,
    OP_SGE, OP_EXP, OP_LOG, OP_LIT, OP_DST, OP_LRP, OP_FRC, OP_M4x4, OP_M4x3, OP_M3x4, OP_M3x3, OP_M3x2,
    OP_CALL, OP_CALLNZ, OP_LOOP, OP_RET, OP_ENDLOOP, OP_LABEL, OP_DCL, OP_POW, OP_CRS, OP_SGN, OP_ABS, OP_NRM,
    OP_SINCOS, OP_REP, OP_ENDREP, OP_IF, OP_IFC, OP_ELSE, OP_ENDIF, OP_BREAK, OP_BREAKC, OP_MOVA, OP_DEFB,
    OP_DEFI,
    OP_TEXCOORD = 64, OP_TEXKILL, OP_TEX, OP_TEXBEM, OP_TEXBEML, OP_TEXREG2AR, OP_TEXREG2GB, OP_TEXM3x2PAD,
    OP_TEXM3x2TEX, OP_TEXM3x3PAD, OP_TEXM3x3TEX, OP_RESERVED0, OP_TEXM3x3SPEC, OP_TEXM3x3VSPEC, OP_EXPP,
    OP_LOGP, OP_CND, OP_DEF, OP_TEXREG2RGB, OP_TEXDP3TEX, OP_TEXM3x2DEPTH, OP_TEXDP3, OP_TEXM3x3, OP_TEXDEPTH,
    OP_CMP, OP_BEM, OP_DP2ADD, OP_DSX, OP_DSY, OP_TEXLDD, OP_SETP, OP_TEXLDL, OP_BREAKP,
    OP_PHASE = 0xfffd, OP_COMMENT = 0xfffe, OP_END = 0xffff,
};
enum {
    R_TEMP = 0, R_INPUT = 1, R_CONST = 2, R_ADDR = 3, R_RASTOUT = 4, R_ATTROUT = 5, R_OUTPUT = 6, R_CONSTINT = 7,
    R_COLOROUT = 8, R_DEPTHOUT = 9, R_SAMPLER = 10, R_CONST2 = 11, R_CONST3 = 12, R_CONST4 = 13,
    R_CONSTBOOL = 14, R_LOOP = 15, R_TEMPF16 = 16, R_MISC = 17, R_LABEL = 18, R_PRED = 19,
};
enum {
    M_NONE, M_NEG, M_BIAS, M_BIASNEG, M_SIGN, M_SIGNNEG, M_COMP, M_X2, M_X2NEG, M_DZ, M_DW, M_ABS, M_ABSNEG,
    M_NOT,
};
/* D3DDECLUSAGE */
enum {
    U_POSITION, U_BLENDWEIGHT, U_BLENDINDICES, U_NORMAL, U_PSIZE, U_TEXCOORD, U_TANGENT, U_BINORMAL,
    U_TESSFACTOR, U_POSITIONT, U_COLOR, U_FOG, U_DEPTH, U_SAMPLE,
};
/* SM1 instructions carry no length: (has destination, sources), -1 = special */
static const signed char sm1_shape[97][2] = {
    [OP_NOP] = {0, 0}, [OP_MOV] = {1, 1}, [OP_ADD] = {1, 2}, [OP_SUB] = {1, 2}, [OP_MAD] = {1, 3},
    [OP_MUL] = {1, 2}, [OP_RCP] = {1, 1}, [OP_RSQ] = {1, 1}, [OP_DP3] = {1, 2}, [OP_DP4] = {1, 2},
    [OP_MIN] = {1, 2}, [OP_MAX] = {1, 2}, [OP_SLT] = {1, 2}, [OP_SGE] = {1, 2}, [OP_EXP] = {1, 1},
    [OP_LOG] = {1, 1}, [OP_LIT] = {1, 1}, [OP_DST] = {1, 2}, [OP_LRP] = {1, 3}, [OP_FRC] = {1, 1},
    [OP_M4x4] = {1, 2}, [OP_M4x3] = {1, 2}, [OP_M3x4] = {1, 2}, [OP_M3x3] = {1, 2}, [OP_M3x2] = {1, 2},
    [OP_CALL] = {0, 1}, [OP_CALLNZ] = {0, 2}, [OP_LOOP] = {0, 2}, [OP_RET] = {0, 0}, [OP_ENDLOOP] = {0, 0},
    [OP_LABEL] = {0, 1}, [OP_DCL] = {-1, -1}, [OP_POW] = {1, 2}, [OP_CRS] = {1, 2}, [OP_SGN] = {1, 3},
    [OP_ABS] = {1, 1}, [OP_NRM] = {1, 1}, [OP_SINCOS] = {1, 3}, [OP_REP] = {0, 1}, [OP_ENDREP] = {0, 0},
    [OP_IF] = {0, 1}, [OP_IFC] = {0, 2}, [OP_ELSE] = {0, 0}, [OP_ENDIF] = {0, 0}, [OP_BREAK] = {0, 0},
    [OP_BREAKC] = {0, 2}, [OP_MOVA] = {1, 1}, [OP_DEFB] = {-1, -1}, [OP_DEFI] = {-1, -1},
    [OP_TEXCOORD] = {1, 0}, [OP_TEXKILL] = {1, 0}, [OP_TEX] = {1, 0}, [OP_TEXBEM] = {1, 1},
    [OP_TEXBEML] = {1, 1}, [OP_TEXREG2AR] = {1, 1}, [OP_TEXREG2GB] = {1, 1}, [OP_TEXM3x2PAD] = {1, 1},
    [OP_TEXM3x2TEX] = {1, 1}, [OP_TEXM3x3PAD] = {1, 1}, [OP_TEXM3x3TEX] = {1, 1}, [OP_RESERVED0] = {0, 0},
    [OP_TEXM3x3SPEC] = {1, 2}, [OP_TEXM3x3VSPEC] = {1, 1}, [OP_EXPP] = {1, 1}, [OP_LOGP] = {1, 1},
    [OP_CND] = {1, 3}, [OP_DEF] = {-1, -1}, [OP_TEXREG2RGB] = {1, 1}, [OP_TEXDP3TEX] = {1, 1},
    [OP_TEXM3x2DEPTH] = {1, 1}, [OP_TEXDP3] = {1, 1}, [OP_TEXM3x3] = {1, 1}, [OP_TEXDEPTH] = {1, 0},
    [OP_CMP] = {1, 3}, [OP_BEM] = {1, 2}, [OP_DP2ADD] = {1, 3}, [OP_DSX] = {1, 1}, [OP_DSY] = {1, 1},
    [OP_TEXLDD] = {1, 4}, [OP_SETP] = {1, 2}, [OP_TEXLDL] = {1, 2}, [OP_BREAKP] = {0, 1},
};

struct param {
    uint8_t type, swz, mod, mask, dmod, rel, rel_type, rel_comp;
    int8_t shift;
    uint16_t num, rel_num;
};
struct ins {
    uint16_t op;
    uint8_t ctrl, pred, coissue, has_dst, nsrc;
    struct param dst, src[4], predp;
    uint32_t lit[4];
    uint32_t dcl;
};

int dxso_slot(uint32_t usage, uint32_t index)
{
    switch (usage) {
    case U_TEXCOORD: return index < 16 ? (int)(DXSO_SLOT_TEXCOORD + index) : -1;
    case U_COLOR: return index < 2 ? (int)(DXSO_SLOT_COLOR + index) : index < 4 ? (int)(DXSO_SLOT_COLOR2 + index - 2) : -1;
    case U_FOG: return index == 0 ? DXSO_SLOT_FOG : -1;
    case U_NORMAL: return index < 2 ? (int)(DXSO_SLOT_NORMAL + index) : -1;
    case U_TANGENT: return index == 0 ? DXSO_SLOT_TANGENT : -1;
    case U_BINORMAL: return index == 0 ? DXSO_SLOT_BINORMAL : -1;
    case U_POSITION: return index == 1 ? DXSO_SLOT_POSITION1 : -1;
    case U_BLENDWEIGHT: return index == 0 ? DXSO_SLOT_BLENDWEIGHT : -1;
    case U_BLENDINDICES: return index == 0 ? DXSO_SLOT_BLENDINDICES : -1;
    case U_PSIZE: return index == 1 ? DXSO_SLOT_PSIZE1 : -1;
    case U_DEPTH: return index == 0 ? DXSO_SLOT_DEPTH : -1;
    }
    return -1;
}

static int version_major(uint32_t v) { return (v >> 8) & 0xff; }
static int version_minor(uint32_t v) { return v & 0xff; }

static void decode_param(struct param *p, uint32_t t, int dst)
{
    memset(p, 0, sizeof *p);
    p->type = (uint8_t)(((t >> 28) & 7) | ((t >> 8) & 0x18));
    p->num = t & 0x7ff;
    p->rel = (t >> 13) & 1;
    if (dst) {
        p->mask = (t >> 16) & 15;
        p->dmod = (t >> 20) & 15;
        int sh = (t >> 24) & 15;
        p->shift = (int8_t)(sh >= 8 ? sh - 16 : sh);
        p->swz = 0xe4;
    } else {
        p->swz = (t >> 16) & 0xff;
        p->mod = (t >> 24) & 15;
        p->mask = 15;
    }
}

/* One instruction at code[*pos]; returns 0 at END or on error (*err set). */
static int decode(const uint32_t *code, uint32_t n, uint32_t *pos, int major, int ps, struct ins *in, int *err)
{
    for (;;) {
        if (*pos >= n) { *err = 1; return 0; }
        uint32_t t = code[*pos];
        uint32_t op = t & 0xffff;
        if (op == OP_COMMENT) { *pos += 1 + ((t >> 16) & 0x7fff); continue; }
        if (op == OP_END) { (*pos)++; return 0; }
        memset(in, 0, sizeof *in);
        in->op = (uint16_t)op;
        in->ctrl = (t >> 16) & 0xff;
        in->pred = (t >> 28) & 1;
        in->coissue = (t >> 30) & 1;
        uint32_t len = (t >> 24) & 15, start = ++*pos;
        if (op == OP_PHASE) return 1;
        if (op > OP_BREAKP) { *err = 1; return 0; }
        if (op == OP_DCL) {
            in->dcl = code[(*pos)++];
            in->has_dst = 1;
            decode_param(&in->dst, code[(*pos)++], 1);
        } else if (op == OP_DEF || op == OP_DEFI || op == OP_DEFB) {
            in->has_dst = 1;
            decode_param(&in->dst, code[(*pos)++], 1);
            uint32_t k = op == OP_DEFB ? 1 : 4;
            for (uint32_t i = 0; i < k; i++) in->lit[i] = code[(*pos)++];
        } else {
            int has_dst = sm1_shape[op][0], nsrc = sm1_shape[op][1];
            if (op == OP_TEX || op == OP_TEXCOORD) {
                if (!ps) { *err = 1; return 0; }
                if (major >= 2) nsrc = op == OP_TEX ? 2 : 0;
                else if (major == 1 && len == 0) nsrc = 0;
            }
            if (op == OP_SINCOS && major >= 3) nsrc = 1;
            in->has_dst = (uint8_t)has_dst;
            if (has_dst) {
                decode_param(&in->dst, code[(*pos)++], 1);
                if (in->dst.rel && major >= 3) {
                    /* vs 3.0 o[aL]: the address token follows the destination */
                    uint32_t r = code[(*pos)++];
                    in->dst.rel_type = (uint8_t)(((r >> 28) & 7) | ((r >> 8) & 0x18));
                    in->dst.rel_num = r & 0x7ff;
                    in->dst.rel_comp = (r >> 16) & 3;
                }
            }
            if (in->pred) decode_param(&in->predp, code[(*pos)++], 0);
            int count = nsrc;
            if (major == 1 && ps && (op == OP_TEX || op == OP_TEXCOORD)) {
                /* ps 1.4 texld/texcrd rN, src; 1.0-1.3 have no source. A parameter
                 * token has bit 31 set, an instruction token does not. */
                count = (*pos < n && (code[*pos] & 0x80000000u)) ? 1 : 0;
            }
            if (major >= 2) count = 4;  /* bounded by the length below */
            for (int i = 0; i < count && i < 4; i++) {
                if (major >= 2 && *pos >= start + len) { count = i; break; }
                decode_param(&in->src[i], code[(*pos)++], 0);
                if (in->src[i].rel) {
                    if (major >= 2) {
                        uint32_t r = code[(*pos)++];
                        in->src[i].rel_type = (uint8_t)(((r >> 28) & 7) | ((r >> 8) & 0x18));
                        in->src[i].rel_num = r & 0x7ff;
                        in->src[i].rel_comp = (r >> 16) & 3;
                    } else {
                        in->src[i].rel_type = R_ADDR;
                        in->src[i].rel_comp = 0;
                    }
                }
            }
            in->nsrc = (uint8_t)count;
        }
        if (major >= 2 && op != OP_DCL && op != OP_DEF && op != OP_DEFI && op != OP_DEFB && len &&
            *pos != start + len) {
            /* lengths disagree: trust the length (unknown trailing tokens) */
            if (*pos > start + len) { *err = 1; return 0; }
            *pos = start + len;
        }
        return 1;
    }
}

int dxso_scan(const uint32_t *code, uint32_t max_words, struct dxso_info *info)
{
    memset(info, 0, sizeof *info);
    if (max_words < 2) return 0;
    uint32_t v = code[0];
    if ((v >> 16) != 0xfffe && (v >> 16) != 0xffff) return 0;
    info->version = v;
    info->ps = (v >> 16) == 0xffff;
    info->major = version_major(v);
    info->minor = version_minor(v);
    if (info->major < 1 || info->major > 3) return 0;
    uint32_t pos = 1;
    struct ins in;
    int err = 0;
    while (decode(code, max_words, &pos, info->major, info->ps, &in, &err)) {
        const struct param *d = &in.dst;
        if (in.op == OP_DCL) {
            if (d->type == R_SAMPLER) {
                uint32_t tt = (in.dcl >> 27) & 15;
                if (d->num < 20) info->sampler_dcl[d->num] = (uint8_t)(tt == 3 ? DXSO_TEX_CUBE + 1 : tt == 4 ? DXSO_TEX_3D + 1 : DXSO_TEX_2D + 1);
                info->samplers |= 1u << (d->num & 31);
            } else if (d->type == R_INPUT && !info->ps && d->num < 16) {
                info->in_mask |= 1u << d->num;
                info->in_usage[d->num] = in.dcl & 31;
                info->in_index[d->num] = (in.dcl >> 16) & 15;
            } else if (d->type == R_INPUT && info->ps && info->major >= 3) {
                int s = dxso_slot(in.dcl & 31, (in.dcl >> 16) & 15);
                if (s >= 0) info->slots_read |= 1u << s;
            } else if (d->type == R_OUTPUT && !info->ps && info->major >= 3) {
                uint32_t u = in.dcl & 31, ix = (in.dcl >> 16) & 15;
                int s = dxso_slot(u, ix);
                if (s >= 0) info->slots_written |= 1u << s;
                if (u == U_PSIZE && ix == 0) info->writes_psize = 1;
            }
            continue;
        }
        /* registers used */
        if (in.has_dst) {
            if (info->ps && d->type == R_DEPTHOUT) info->writes_depth = 1;
            if (!info->ps && info->major < 3) {
                if (d->type == R_OUTPUT && d->num < 8) info->slots_written |= 1u << d->num;
                if (d->type == R_ATTROUT && d->num < 2) info->slots_written |= 1u << (DXSO_SLOT_COLOR + d->num);
                if (d->type == R_RASTOUT && d->num == 1) info->slots_written |= 1u << DXSO_SLOT_FOG;
                if (d->type == R_RASTOUT && d->num == 2) info->writes_psize = 1;
            }
        }
        if (info->ps && info->major == 1) {
            if (in.op == OP_TEXDEPTH || in.op == OP_TEXM3x2DEPTH) info->writes_depth = 1;
            if (info->minor < 4 && in.has_dst && d->type == R_ADDR && d->num < 8) {
                info->slots_read |= 1u << d->num;
                if (in.op == OP_TEX) info->tex_proj_used |= 1u << d->num;
            }
            for (int i = 0; i < in.nsrc; i++) {
                const struct param *p = &in.src[i];
                if (p->type == R_ADDR && p->num < 8) info->slots_read |= 1u << p->num;
                if (info->minor == 4 && in.op == OP_TEX && p->type == R_ADDR && p->num < 8)
                    info->tex_proj_used |= 1u << d->num;
            }
            if (in.op == OP_TEX || in.op == OP_TEXBEM || in.op == OP_TEXBEML || in.op == OP_TEXREG2AR ||
                in.op == OP_TEXREG2GB || in.op == OP_TEXREG2RGB || in.op == OP_TEXM3x2TEX ||
                in.op == OP_TEXM3x3TEX || in.op == OP_TEXM3x3SPEC || in.op == OP_TEXM3x3VSPEC ||
                in.op == OP_TEXDP3TEX)
                info->samplers |= 1u << (d->num & 15);
        }
        if (info->ps && info->major < 3) {
            for (int i = 0; i < in.nsrc; i++) {
                const struct param *p = &in.src[i];
                if (p->type == R_INPUT && p->num < 2) info->slots_read |= 1u << (DXSO_SLOT_COLOR + p->num);
                if (info->major == 2 && p->type == R_ADDR && p->num < 8) info->slots_read |= 1u << p->num;
            }
        }
        if (info->ps && info->major >= 2 && (in.op == OP_TEX || in.op == OP_TEXLDL || in.op == OP_TEXLDD))
            for (int i = 0; i < in.nsrc; i++) if (in.src[i].type == R_SAMPLER) info->samplers |= 1u << (in.src[i].num & 15);
        if (!info->ps && in.op == OP_TEXLDL)
            for (int i = 0; i < in.nsrc; i++) if (in.src[i].type == R_SAMPLER) info->samplers |= 1u << (in.src[i].num & 3);
        if (!info->ps && info->major < 2) {
            for (int i = 0; i < in.nsrc; i++) if (in.src[i].type == R_INPUT && in.src[i].num < 16) {
                /* vs 1.1 without dcl (D3D8-style) is not valid D3D9; dcl always exists */
            }
        }
    }
    if (err) return 0;
    if (info->ps && info->major < 3) info->slots_read |= 1u << DXSO_SLOT_FOG;
    if (!info->ps) info->slots_written |= 0;
    info->words = pos;
    return 1;
}

/* ---------------------------------------------------------------------------------------------- */

struct cfe { int kind; uint32_t merge, cont, header, false_lbl, counter, saved_al, count, step, start; int has_else; };
enum { CF_IF, CF_LOOP, CF_REP };

struct ctx {
    struct spv s;
    const struct dxso_info *info;
    const struct dxso_key *key;
    int ps, major, minor, ff;
    /* types */
    uint32_t t_void, t_f, t_v2, t_v3, t_v4, t_i, t_iv4, t_u, t_uv4, t_b, t_bv4;
    uint32_t p_v4, p_i, p_iv4, p_bv4, f_i, u_v4, u_iv4, u_uv4, pc_f, pc_u, pc_v4, in_v4, in_f, out_v4, out_f;
    uint32_t zero4, one4;
    /* registers */
    uint32_t r[32], treg[8], tcin[8], oc[4], odepth, vin, outs, a0, al, p0;
    uint32_t vin_t, outs_t;
    uint32_t def_f[256]; uint8_t has_def[256];
    int32_t defi[16][4]; uint16_t defi_mask;
    uint16_t defb_mask, defb_val;
    uint32_t ubo, push, samplers[20], sampler_t[20];
    uint32_t cfloat_array;  /* Private copy of the float constants (relative addressing with defs) */
    int need_float_array;
    uint32_t nfloats;
    /* functions */
    uint32_t body_fn, label_fn[2048 / 32 * 0 + 64];
    int in_function, terminated;
    struct cfe cf[64]; int ncf;
    /* interface */
    uint32_t iface[64]; uint32_t niface;
    uint32_t in_vars[DXSO_SLOTS], frag_coord, front_facing, point_coord;
    uint32_t vs_in[16];
    uint32_t decorated[16]; int ndecorated;
    uint8_t out_usage[16], out_index[16], out_dcl[16];
    uint8_t in_usage[16], in_index[16], in_dcl[16], in_centroid[16];
    int uses_vpos, uses_vface;
};

#define S (&c->s)

static uint32_t cf32(struct ctx *c, float v) { return spv_c_f32(S, v); }
static uint32_t ci32(struct ctx *c, int32_t v) { return spv_c_i32(S, v); }
static uint32_t cu32(struct ctx *c, uint32_t v) { return spv_c_u32(S, v); }

static uint32_t var_init(struct ctx *c, uint32_t ptr_type, uint32_t storage, uint32_t init)
{
    uint32_t id = spv_id(S);
    spv_inst(&S->types, SpvOpVariable, (const uint32_t[]){ptr_type, id, storage, init}, 4);
    return id;
}

static void decorate_stride(struct ctx *c, uint32_t type, uint32_t stride)
{
    for (int i = 0; i < c->ndecorated; i++) if (c->decorated[i] == type) return;
    if (c->ndecorated < 16) c->decorated[c->ndecorated++] = type;
    spv_decorate(S, type, SpvDecorationArrayStride, 1, stride);
}

static uint32_t splat4(struct ctx *c, uint32_t x) { return S_OP(S, SpvOpCompositeConstruct, c->t_v4, x, x, x, x); }
static uint32_t bsplat4(struct ctx *c, uint32_t x) { return S_OP(S, SpvOpCompositeConstruct, c->t_bv4, x, x, x, x); }
static uint32_t comp(struct ctx *c, uint32_t v, int i) { return spv_extract(S, c->t_f, v, (uint32_t)i); }
static uint32_t fadd(struct ctx *c, uint32_t t, uint32_t a, uint32_t b) { return S_OP(S, SpvOpFAdd, t, a, b); }
static uint32_t fsub(struct ctx *c, uint32_t t, uint32_t a, uint32_t b) { return S_OP(S, SpvOpFSub, t, a, b); }
static uint32_t fmul(struct ctx *c, uint32_t t, uint32_t a, uint32_t b) { return S_OP(S, SpvOpFMul, t, a, b); }
static uint32_t fdiv(struct ctx *c, uint32_t t, uint32_t a, uint32_t b) { return S_OP(S, SpvOpFDiv, t, a, b); }
static uint32_t sel(struct ctx *c, uint32_t t, uint32_t cond, uint32_t a, uint32_t b) { return S_OP(S, SpvOpSelect, t, cond, a, b); }
static uint32_t shuffle4(struct ctx *c, uint32_t a, uint32_t b, int x, int y, int z, int w)
{
    return S_OP(S, SpvOpVectorShuffle, c->t_v4, a, b, (uint32_t)x, (uint32_t)y, (uint32_t)z, (uint32_t)w);
}
static uint32_t vec4of(struct ctx *c, uint32_t x, uint32_t y, uint32_t z, uint32_t w)
{
    return S_OP(S, SpvOpCompositeConstruct, c->t_v4, x, y, z, w);
}
static uint32_t ext1(struct ctx *c, uint32_t t, uint32_t inst, uint32_t a) { return S_EXT(S, t, inst, a); }
static uint32_t ext2(struct ctx *c, uint32_t t, uint32_t inst, uint32_t a, uint32_t b) { return S_EXT(S, t, inst, a, b); }
static uint32_t saturate(struct ctx *c, uint32_t t, uint32_t v)
{
    uint32_t z = t == c->t_v4 ? c->zero4 : cf32(c, 0), o = t == c->t_v4 ? c->one4 : cf32(c, 1);
    return S_EXT(S, t, GLSLstd450NClamp, v, z, o);
}
static uint32_t boolvec(struct ctx *c, uint32_t t)
{
    return t == c->t_v4 ? c->t_bv4 : t == c->t_v3 ? spv_t_vec(S, c->t_b, 3) : t == c->t_v2 ? spv_t_vec(S, c->t_b, 2) : c->t_b;
}
static uint32_t zero_of(struct ctx *c, uint32_t t)
{
    if (t == c->t_v4) return c->zero4;
    if (t == c->t_f) return cf32(c, 0);
    return spv_c_null(S, t);
}
/* D3D9 multiply: 0 * anything = 0 (d3d9.floatEmulation = strict) */
static uint32_t lmul(struct ctx *c, uint32_t t, uint32_t a, uint32_t b)
{
    uint32_t bt = boolvec(c, t), z = zero_of(c, t);
    uint32_t m = fmul(c, t, a, b);
    uint32_t za = S_OP(S, SpvOpFOrdEqual, bt, a, z), zb = S_OP(S, SpvOpFOrdEqual, bt, b, z);
    uint32_t any = S_OP(S, SpvOpLogicalOr, bt, za, zb);
    return sel(c, t, any, z, m);
}
static uint32_t ldot(struct ctx *c, uint32_t a, uint32_t b, int n)
{
    uint32_t m = lmul(c, c->t_v4, a, b);
    uint32_t r = comp(c, m, 0);
    for (int i = 1; i < n; i++) r = fadd(c, c->t_f, r, comp(c, m, i));
    return r;
}

/* ---- registers ---- */

static uint32_t private_v4(struct ctx *c, uint32_t *slot)
{
    if (!*slot) *slot = var_init(c, c->p_v4, SpvStoragePrivate, c->zero4);
    return *slot;
}

static uint32_t rel_value(struct ctx *c, const struct param *p)
{
    if (p->rel_type == R_LOOP) return spv_load(S, c->t_i, c->al);
    uint32_t a = spv_load(S, c->t_iv4, c->a0);
    return spv_extract(S, c->t_i, a, p->rel_comp);
}

static uint32_t array_elem(struct ctx *c, uint32_t arr, const struct param *p, uint32_t base, uint32_t size)
{
    uint32_t idx;
    if (p->rel) {
        idx = S_OP(S, SpvOpIAdd, c->t_i, rel_value(c, p), ci32(c, (int32_t)base));
        idx = S_EXT(S, c->t_i, GLSLstd450SClamp, idx, ci32(c, 0), ci32(c, (int32_t)size - 1));
    } else idx = ci32(c, (int32_t)(base < size ? base : size - 1));
    return S_OP(S, SpvOpAccessChain, c->p_v4, arr, idx);
}

static int out_index(struct ctx *c, const struct param *p)
{
    if (c->major >= 3) return p->num & 15;
    switch (p->type) {
    case R_OUTPUT: return p->num & 7;           /* oT0-7 */
    case R_ATTROUT: return 8 + (p->num & 1);    /* oD0-1 */
    case R_RASTOUT: return 10 + (p->num % 3);   /* oPos, oFog, oPts */
    }
    return 0;
}

static uint32_t float_const(struct ctx *c, const struct param *p)
{
    uint32_t n = p->num;
    if (p->type == R_CONST2) n += 2048;
    else if (p->type == R_CONST3) n += 4096;
    else if (p->type == R_CONST4) n += 6144;
    uint32_t v;
    if (!p->rel) {
        if (n < 256 && c->has_def[n]) v = c->def_f[n];
        else if (n >= c->nfloats) v = c->zero4;
        else if (c->cfloat_array) v = spv_load(S, c->t_v4, S_OP(S, SpvOpAccessChain, c->p_v4, c->cfloat_array, ci32(c, (int32_t)n)));
        else v = spv_load(S, c->t_v4, S_OP(S, SpvOpAccessChain, c->u_v4, c->ubo, ci32(c, 0), ci32(c, (int32_t)n)));
    } else {
        uint32_t idx = S_OP(S, SpvOpIAdd, c->t_i, rel_value(c, p), ci32(c, (int32_t)n));
        uint32_t inb = S_OP(S, SpvOpULessThan, c->t_b, idx, ci32(c, (int32_t)c->nfloats));
        uint32_t ci = S_EXT(S, c->t_i, GLSLstd450SClamp, idx, ci32(c, 0), ci32(c, (int32_t)c->nfloats - 1));
        if (c->cfloat_array) v = spv_load(S, c->t_v4, S_OP(S, SpvOpAccessChain, c->p_v4, c->cfloat_array, ci));
        else v = spv_load(S, c->t_v4, S_OP(S, SpvOpAccessChain, c->u_v4, c->ubo, ci32(c, 0), ci));
        v = sel(c, c->t_v4, bsplat4(c, inb), v, c->zero4);
    }
    if (c->ps && c->major == 1) v = S_EXT(S, c->t_v4, GLSLstd450NClamp, v, c->ps ? splat4(c, cf32(c, -1)) : 0, c->one4);
    return v;
}

static uint32_t int_const(struct ctx *c, uint32_t n)
{
    n &= 15;
    if (c->defi_mask >> n & 1) {
        uint32_t k[4];
        for (int i = 0; i < 4; i++) k[i] = ci32(c, c->defi[n][i]);
        return spv_c_comp(S, c->t_iv4, k, 4);
    }
    return spv_load(S, c->t_iv4, S_OP(S, SpvOpAccessChain, c->u_iv4, c->ubo, ci32(c, 1), ci32(c, (int32_t)n)));
}

static uint32_t bool_const(struct ctx *c, uint32_t n)
{
    n &= 15;
    if (c->defb_mask >> n & 1) return spv_c_bool(S, c->defb_val >> n & 1);
    uint32_t b = spv_load(S, c->t_uv4, S_OP(S, SpvOpAccessChain, c->u_uv4, c->ubo, ci32(c, 2)));
    uint32_t w = spv_extract(S, c->t_u, b, 0);
    uint32_t bit = S_OP(S, SpvOpBitwiseAnd, c->t_u, w, cu32(c, 1u << n));
    return S_OP(S, SpvOpINotEqual, c->t_b, bit, cu32(c, 0));
}

static uint32_t load_reg(struct ctx *c, const struct param *p)
{
    switch (p->type) {
    case R_TEMP: case R_TEMPF16: return spv_load(S, c->t_v4, private_v4(c, &c->r[p->num & 31]));
    case R_INPUT:
        return spv_load(S, c->t_v4, array_elem(c, c->vin, p, p->num, 16));
    case R_CONST: case R_CONST2: case R_CONST3: case R_CONST4: return float_const(c, p);
    case R_ADDR:
        if (c->ps) return spv_load(S, c->t_v4, private_v4(c, &c->treg[p->num & 7]));
        return S_OP(S, SpvOpConvertSToF, c->t_v4, spv_load(S, c->t_iv4, c->a0));
    case R_RASTOUT: case R_ATTROUT: case R_OUTPUT:
        return spv_load(S, c->t_v4, array_elem(c, c->outs, p, (uint32_t)out_index(c, p), 16));
    case R_CONSTINT: return S_OP(S, SpvOpConvertSToF, c->t_v4, int_const(c, p->num));
    case R_CONSTBOOL: return sel(c, c->t_v4, bsplat4(c, bool_const(c, p->num)), c->one4, c->zero4);
    case R_COLOROUT: return spv_load(S, c->t_v4, private_v4(c, &c->oc[p->num & 3]));
    case R_DEPTHOUT: return spv_load(S, c->t_v4, private_v4(c, &c->odepth));
    case R_LOOP: return splat4(c, S_OP(S, SpvOpConvertSToF, c->t_f, spv_load(S, c->t_i, c->al)));
    case R_MISC:
        if (p->num == 0) {
            uint32_t fc = spv_load(S, c->t_v4, c->frag_coord);
            return vec4of(c, fsub(c, c->t_f, comp(c, fc, 0), cf32(c, 0.5f)), fsub(c, c->t_f, comp(c, fc, 1), cf32(c, 0.5f)),
                          cf32(c, 0), cf32(c, 0));
        } else {
            uint32_t ff = spv_load(S, c->t_b, c->front_facing);
            return splat4(c, sel(c, c->t_f, ff, cf32(c, 1), cf32(c, -1)));
        }
    case R_PRED: return sel(c, c->t_v4, spv_load(S, c->t_bv4, c->p0), c->one4, c->zero4);
    }
    return c->zero4;
}

static uint32_t swizzle(struct ctx *c, uint32_t v, uint32_t swz)
{
    if (swz == 0xe4) return v;
    return shuffle4(c, v, v, swz & 3, swz >> 2 & 3, swz >> 4 & 3, swz >> 6 & 3);
}

static uint32_t apply_mod(struct ctx *c, uint32_t v, uint32_t mod)
{
    uint32_t t = c->t_v4;
    switch (mod) {
    case M_NEG: return S_OP(S, SpvOpFNegate, t, v);
    case M_BIAS: return fsub(c, t, v, splat4(c, cf32(c, 0.5f)));
    case M_BIASNEG: return S_OP(S, SpvOpFNegate, t, fsub(c, t, v, splat4(c, cf32(c, 0.5f))));
    case M_SIGN: return fsub(c, t, fmul(c, t, v, splat4(c, cf32(c, 2))), c->one4);
    case M_SIGNNEG: return S_OP(S, SpvOpFNegate, t, fsub(c, t, fmul(c, t, v, splat4(c, cf32(c, 2))), c->one4));
    case M_COMP: return fsub(c, t, c->one4, v);
    case M_X2: return fmul(c, t, v, splat4(c, cf32(c, 2)));
    case M_X2NEG: return S_OP(S, SpvOpFNegate, t, fmul(c, t, v, splat4(c, cf32(c, 2))));
    case M_DZ: { uint32_t z = comp(c, v, 2); return vec4of(c, fdiv(c, c->t_f, comp(c, v, 0), z), fdiv(c, c->t_f, comp(c, v, 1), z), z, comp(c, v, 3)); }
    case M_DW: { uint32_t w = comp(c, v, 3); return vec4of(c, fdiv(c, c->t_f, comp(c, v, 0), w), fdiv(c, c->t_f, comp(c, v, 1), w), comp(c, v, 2), w); }
    case M_ABS: return ext1(c, t, GLSLstd450FAbs, v);
    case M_ABSNEG: return S_OP(S, SpvOpFNegate, t, ext1(c, t, GLSLstd450FAbs, v));
    }
    return v;
}

static uint32_t src(struct ctx *c, const struct param *p)
{
    return apply_mod(c, swizzle(c, load_reg(c, p), p->swz), p->mod);
}
/* scalar source: component 3 after the swizzle (replicate swizzles make this any component) */
static uint32_t src_w(struct ctx *c, const struct param *p) { return comp(c, src(c, p), 3); }

static uint32_t pred_vec(struct ctx *c, const struct param *pp)
{
    uint32_t p = spv_load(S, c->t_bv4, c->p0);
    if (pp->swz != 0xe4) p = S_OP(S, SpvOpVectorShuffle, c->t_bv4, p, p, pp->swz & 3u, pp->swz >> 2 & 3u, pp->swz >> 4 & 3u, pp->swz >> 6 & 3u);
    if (pp->mod == M_NOT) p = S_OP(S, SpvOpLogicalNot, c->t_bv4, p);
    return p;
}

static uint32_t dst_ptr(struct ctx *c, const struct param *d)
{
    switch (d->type) {
    case R_TEMP: case R_TEMPF16: return private_v4(c, &c->r[d->num & 31]);
    case R_ADDR: return private_v4(c, &c->treg[d->num & 7]);  /* ps texture registers */
    case R_RASTOUT: case R_ATTROUT: case R_OUTPUT:
        return array_elem(c, c->outs, d, (uint32_t)out_index(c, d), 16);
    case R_COLOROUT: return private_v4(c, &c->oc[d->num & 3]);
    case R_DEPTHOUT: return private_v4(c, &c->odepth);
    case R_INPUT: return array_elem(c, c->vin, d, d->num, 16);
    }
    return private_v4(c, &c->r[0]);
}

static uint32_t masked(struct ctx *c, uint32_t old, uint32_t val, uint32_t mask)
{
    if (mask == 15) return val;
    return shuffle4(c, old, val, mask & 1 ? 4 : 0, mask & 2 ? 5 : 1, mask & 4 ? 6 : 2, mask & 8 ? 7 : 3);
}

static void wr(struct ctx *c, const struct ins *in, uint32_t val)
{
    const struct param *d = &in->dst;
    uint32_t mask = d->mask;
    if (d->shift) {
        float k = d->shift > 0 ? (float)(1 << d->shift) : 1.0f / (float)(1 << -d->shift);
        val = S_OP(S, SpvOpVectorTimesScalar, c->t_v4, val, cf32(c, k));
    }
    if (d->dmod & 1) val = saturate(c, c->t_v4, val);
    if (d->type == R_ADDR && !c->ps) {
        /* vs 1.1 mov a0: floor; mova: round to nearest */
        uint32_t f = in->op == OP_MOVA || c->major >= 2 ? ext1(c, c->t_v4, GLSLstd450Floor, fadd(c, c->t_v4, val, splat4(c, cf32(c, 0.5f))))
                                                       : ext1(c, c->t_v4, GLSLstd450Floor, val);
        uint32_t iv = S_OP(S, SpvOpConvertFToS, c->t_iv4, f);
        uint32_t old = spv_load(S, c->t_iv4, c->a0);
        if (mask != 15)
            iv = S_OP(S, SpvOpVectorShuffle, c->t_iv4, old, iv, mask & 1 ? 4u : 0u, mask & 2 ? 5u : 1u, mask & 4 ? 6u : 2u, mask & 8 ? 7u : 3u);
        if (in->pred) iv = sel(c, c->t_iv4, pred_vec(c, &in->predp), iv, old);
        spv_store(S, c->a0, iv);
        return;
    }
    uint32_t ptr = dst_ptr(c, d);
    if (mask != 15 || in->pred) {
        uint32_t old = spv_load(S, c->t_v4, ptr);
        val = masked(c, old, val, mask);
        if (in->pred) val = sel(c, c->t_v4, pred_vec(c, &in->predp), val, old);
    }
    spv_store(S, ptr, val);
}

/* ---- textures ---- */

static int tex_kind(struct ctx *c, int s)
{
    if (!c->ps) return c->key->tex[s & 3] & DXSO_TEX_KIND;
    if (c->info->sampler_dcl[s & 15] && c->major >= 2) return c->info->sampler_dcl[s & 15] - 1;
    return c->key->tex[s & 15] & DXSO_TEX_KIND;
}
static int tex_compare(struct ctx *c, int s)
{
    int k = c->ps ? c->key->tex[s & 15] : c->key->tex[s & 3];
    return (k & DXSO_TEX_COMPARE) && tex_kind(c, s) == DXSO_TEX_2D;
}

static uint32_t sampler_var(struct ctx *c, int s, uint32_t *type)
{
    int slot = c->ps ? (s & 15) : 16 + (s & 3);
    if (!c->samplers[slot]) {
        int kind = tex_kind(c, s), cmp = tex_compare(c, s);
        uint32_t dim = kind == DXSO_TEX_CUBE ? 3 : kind == DXSO_TEX_3D ? 2 : 1;
        uint32_t img = spv_t_image(S, dim, cmp ? 1 : 0, 0);
        uint32_t st = spv_t_sampled(S, img);
        c->sampler_t[slot] = st;
        c->samplers[slot] = spv_var(S, spv_t_ptr(S, SpvStorageUniformConstant, st), SpvStorageUniformConstant);
        spv_decorate(S, c->samplers[slot], SpvDecorationDescriptorSet, 1, 0);
        spv_decorate(S, c->samplers[slot], SpvDecorationBinding, 1,
                     c->ps ? DXSO_BIND_PS_SAMPLER + (uint32_t)(s & 15) : DXSO_BIND_VS_SAMPLER + (uint32_t)(s & 3));
    }
    *type = c->sampler_t[slot];
    return c->samplers[slot];
}

enum { TS_IMPLICIT, TS_PROJ, TS_BIAS, TS_LOD, TS_GRAD };

/* proj_comp: component to divide by (TS_PROJ), else ignored */
static uint32_t sample(struct ctx *c, int s, uint32_t coord, int mode, int proj_comp, uint32_t dx, uint32_t dy)
{
    uint32_t st, var = sampler_var(c, s, &st);
    int kind = tex_kind(c, s), cmp = tex_compare(c, s);
    int n = kind == DXSO_TEX_2D ? 2 : 3;
    uint32_t ct = n == 2 ? c->t_v2 : c->t_v3;
    uint32_t cx = coord;
    if (mode == TS_PROJ) {
        uint32_t d = comp(c, coord, proj_comp);
        cx = S_OP(S, SpvOpVectorTimesScalar, c->t_v4, coord, fdiv(c, c->t_f, cf32(c, 1), d));
    }
    uint32_t xy = n == 2 ? S_OP(S, SpvOpVectorShuffle, ct, cx, cx, 0, 1) : S_OP(S, SpvOpVectorShuffle, ct, cx, cx, 0, 1, 2);
    uint32_t img = spv_load(S, st, var);
    int fragment = c->ps;
    uint32_t r;
    if (cmp) {
        uint32_t dref = comp(c, cx, 2);
        if (mode == TS_LOD || !fragment) {
            uint32_t lod = mode == TS_LOD ? comp(c, coord, 3) : cf32(c, 0);
            r = S_OP(S, SpvOpImageSampleDrefExplicitLod, c->t_f, img, xy, dref, 2, lod);
        } else if (mode == TS_GRAD) {
            uint32_t gx = S_OP(S, SpvOpVectorShuffle, c->t_v2, dx, dx, 0, 1), gy = S_OP(S, SpvOpVectorShuffle, c->t_v2, dy, dy, 0, 1);
            r = S_OP(S, SpvOpImageSampleDrefExplicitLod, c->t_f, img, xy, dref, 4, gx, gy);
        } else if (mode == TS_BIAS) {
            r = S_OP(S, SpvOpImageSampleDrefImplicitLod, c->t_f, img, xy, dref, 1, comp(c, coord, 3));
        } else r = S_OP(S, SpvOpImageSampleDrefImplicitLod, c->t_f, img, xy, dref);
        return splat4(c, r);
    }
    if (mode == TS_LOD || !fragment) {
        uint32_t lod = mode == TS_LOD ? comp(c, coord, 3) : cf32(c, 0);
        return S_OP(S, SpvOpImageSampleExplicitLod, c->t_v4, img, xy, 2, lod);
    }
    if (mode == TS_GRAD) {
        uint32_t gx = n == 2 ? S_OP(S, SpvOpVectorShuffle, ct, dx, dx, 0, 1) : S_OP(S, SpvOpVectorShuffle, ct, dx, dx, 0, 1, 2);
        uint32_t gy = n == 2 ? S_OP(S, SpvOpVectorShuffle, ct, dy, dy, 0, 1) : S_OP(S, SpvOpVectorShuffle, ct, dy, dy, 0, 1, 2);
        return S_OP(S, SpvOpImageSampleExplicitLod, c->t_v4, img, xy, 4, gx, gy);
    }
    if (mode == TS_BIAS) return S_OP(S, SpvOpImageSampleImplicitLod, c->t_v4, img, xy, 1, comp(c, coord, 3));
    return S_OP(S, SpvOpImageSampleImplicitLod, c->t_v4, img, xy);
}

/* ps 1.x: sample stage s at `coord`, applying D3DTTFF_PROJECTED from the variant */
static uint32_t sample_stage(struct ctx *c, int s, uint32_t coord, int allow_proj)
{
    int count = allow_proj ? c->key->proj[s & 7] : 0;
    if (count >= 2 && count <= 4) return sample(c, s, coord, TS_PROJ, count - 1, 0, 0);
    return sample(c, s, coord, TS_IMPLICIT, 0, 0, 0);
}

static uint32_t texcoord_in(struct ctx *c, int n) { return spv_load(S, c->t_v4, private_v4(c, &c->tcin[n & 7])); }
static uint32_t treg_val(struct ctx *c, int n) { return spv_load(S, c->t_v4, private_v4(c, &c->treg[n & 7])); }
static void treg_set(struct ctx *c, int n, uint32_t v) { spv_store(S, private_v4(c, &c->treg[n & 7]), v); }
static uint32_t dot3(struct ctx *c, uint32_t a, uint32_t b) { return ldot(c, a, b, 3); }

static void demote(struct ctx *c, uint32_t cond)
{
    uint32_t yes = spv_id(S), merge = spv_id(S);
    S_OPV(S, SpvOpSelectionMerge, merge, 0);
    S_OPV(S, SpvOpBranchConditional, cond, yes, merge);
    spv_label(S, yes);
    spv_opv(S, SpvOpDemoteToHelperInvocation, 0, 0);
    spv_branch(S, merge);
    spv_label(S, merge);
}

/* ---- flow control ---- */

static void new_block_if_terminated(struct ctx *c)
{
    if (c->terminated) { spv_label(S, 0); c->terminated = 0; }
}

static uint32_t compare(struct ctx *c, uint32_t t, int ctrl, uint32_t a, uint32_t b)
{
    uint32_t bt = boolvec(c, t);
    static const uint32_t ops[8] = {0, SpvOpFOrdGreaterThan, SpvOpFOrdEqual, SpvOpFOrdGreaterThanEqual,
                                    SpvOpFOrdLessThan, SpvOpFUnordNotEqual, SpvOpFOrdLessThanEqual, 0};
    uint32_t op = ops[ctrl & 7];
    if (!op) return t == c->t_f ? spv_c_bool(S, 0) : spv_c_null(S, bt);
    return S_OP(S, op, bt, a, b);
}

static uint32_t cond_of(struct ctx *c, const struct param *p)
{
    if (p->type == R_PRED) {
        uint32_t v = spv_load(S, c->t_bv4, c->p0);
        uint32_t b = S_OP(S, SpvOpCompositeExtract, c->t_b, v, (uint32_t)(p->swz & 3));
        if (p->mod == M_NOT) b = S_OP(S, SpvOpLogicalNot, c->t_b, b);
        return b;
    }
    return bool_const(c, p->num);
}

static void begin_if(struct ctx *c, uint32_t cond)
{
    struct cfe *e = &c->cf[c->ncf++];
    memset(e, 0, sizeof *e);
    e->kind = CF_IF;
    e->merge = spv_id(S);
    e->false_lbl = spv_id(S);
    uint32_t t = spv_id(S);
    S_OPV(S, SpvOpSelectionMerge, e->merge, 0);
    S_OPV(S, SpvOpBranchConditional, cond, t, e->false_lbl);
    spv_label(S, t);
}

static void do_else(struct ctx *c)
{
    if (!c->ncf || c->cf[c->ncf - 1].kind != CF_IF) return;
    struct cfe *e = &c->cf[c->ncf - 1];
    if (!c->terminated) spv_branch(S, e->merge);
    spv_label(S, e->false_lbl);
    c->terminated = 0;
    e->has_else = 1;
}

static void end_if(struct ctx *c)
{
    if (!c->ncf || c->cf[c->ncf - 1].kind != CF_IF) return;
    struct cfe *e = &c->cf[--c->ncf];
    if (!c->terminated) spv_branch(S, e->merge);
    if (!e->has_else) { spv_label(S, e->false_lbl); spv_branch(S, e->merge); }
    spv_label(S, e->merge);
    c->terminated = 0;
}

static void begin_loop(struct ctx *c, int kind, uint32_t count, uint32_t start, uint32_t step)
{
    struct cfe *e = &c->cf[c->ncf++];
    memset(e, 0, sizeof *e);
    e->kind = kind;
    e->counter = spv_fvar(S, c->f_i);
    spv_store(S, e->counter, ci32(c, 0));
    if (kind == CF_LOOP) {
        e->saved_al = spv_fvar(S, c->f_i);
        spv_store(S, e->saved_al, spv_load(S, c->t_i, c->al));
        spv_store(S, c->al, start);
        e->step = step;
    }
    e->count = count;
    e->header = spv_id(S);
    e->merge = spv_id(S);
    e->cont = spv_id(S);
    uint32_t check = spv_id(S), body = spv_id(S);
    spv_branch(S, e->header);
    spv_label(S, e->header);
    S_OPV(S, SpvOpLoopMerge, e->merge, e->cont, 0);
    spv_branch(S, check);
    spv_label(S, check);
    uint32_t n = spv_load(S, c->t_i, e->counter);
    uint32_t go = S_OP(S, SpvOpSLessThan, c->t_b, n, count);
    S_OPV(S, SpvOpBranchConditional, go, body, e->merge);
    spv_label(S, body);
}

static void end_loop(struct ctx *c)
{
    if (!c->ncf || c->cf[c->ncf - 1].kind == CF_IF) return;
    struct cfe *e = &c->cf[--c->ncf];
    if (!c->terminated) spv_branch(S, e->cont);
    spv_label(S, e->cont);
    uint32_t n = spv_load(S, c->t_i, e->counter);
    spv_store(S, e->counter, S_OP(S, SpvOpIAdd, c->t_i, n, ci32(c, 1)));
    if (e->kind == CF_LOOP) spv_store(S, c->al, S_OP(S, SpvOpIAdd, c->t_i, spv_load(S, c->t_i, c->al), e->step));
    spv_branch(S, e->header);
    spv_label(S, e->merge);
    c->terminated = 0;
    if (e->kind == CF_LOOP) spv_store(S, c->al, spv_load(S, c->t_i, e->saved_al));
}

static struct cfe *inner_loop(struct ctx *c)
{
    for (int i = c->ncf - 1; i >= 0; i--) if (c->cf[i].kind != CF_IF) return &c->cf[i];
    return 0;
}

static void do_break(struct ctx *c, uint32_t cond)
{
    struct cfe *l = inner_loop(c);
    if (!l) return;
    if (!cond) { spv_branch(S, l->merge); c->terminated = 1; return; }
    uint32_t yes = spv_id(S), merge = spv_id(S);
    S_OPV(S, SpvOpSelectionMerge, merge, 0);
    S_OPV(S, SpvOpBranchConditional, cond, yes, merge);
    spv_label(S, yes);
    spv_branch(S, l->merge);
    spv_label(S, merge);
}

static uint32_t label_function(struct ctx *c, uint32_t n)
{
    n &= 63;
    if (!c->label_fn[n]) c->label_fn[n] = spv_id(S);
    return c->label_fn[n];
}

static void call_label(struct ctx *c, uint32_t n)
{
    S_OP(S, SpvOpFunctionCall, c->t_void, label_function(c, n));
}

/* ---- instructions ---- */

static uint32_t scalar_op(struct ctx *c, int op, uint32_t x)
{
    switch (op) {
    case OP_RCP: {
        uint32_t z = S_OP(S, SpvOpFOrdEqual, c->t_b, x, cf32(c, 0));
        union { uint32_t u; float f; } inf = { .u = 0x7f800000u };
        return sel(c, c->t_f, z, cf32(c, inf.f), fdiv(c, c->t_f, cf32(c, 1), x));
    }
    case OP_RSQ: return ext1(c, c->t_f, GLSLstd450InverseSqrt, ext1(c, c->t_f, GLSLstd450FAbs, x));
    case OP_EXP: case OP_EXPP: return ext1(c, c->t_f, GLSLstd450Exp2, x);
    case OP_LOG: case OP_LOGP: {
        uint32_t a = ext1(c, c->t_f, GLSLstd450FAbs, x);
        uint32_t z = S_OP(S, SpvOpFOrdEqual, c->t_b, a, cf32(c, 0));
        return sel(c, c->t_f, z, cf32(c, -3.402823466e38f), ext1(c, c->t_f, GLSLstd450Log2, a));
    }
    }
    return x;
}

static void texm_row(struct ctx *c, int stage, int src_reg, uint32_t *dot)
{
    *dot = dot3(c, texcoord_in(c, stage), treg_val(c, src_reg));
}

static void emit(struct ctx *c, const struct ins *in, const struct ins *all, int count, int idx)
{
    const struct param *d = &in->dst;
    uint32_t a, b, e, r;
    uint32_t t4 = c->t_v4, tf = c->t_f;
    (void)all; (void)count; (void)idx;
    switch (in->op) {
    case OP_NOP: case OP_PHASE: case OP_DCL: case OP_DEF: case OP_DEFI: case OP_DEFB: case OP_RESERVED0: return;
    case OP_MOV: wr(c, in, src(c, &in->src[0])); return;
    case OP_MOVA: wr(c, in, src(c, &in->src[0])); return;
    case OP_ADD: wr(c, in, fadd(c, t4, src(c, &in->src[0]), src(c, &in->src[1]))); return;
    case OP_SUB: wr(c, in, fsub(c, t4, src(c, &in->src[0]), src(c, &in->src[1]))); return;
    case OP_MUL: wr(c, in, lmul(c, t4, src(c, &in->src[0]), src(c, &in->src[1]))); return;
    case OP_MAD: wr(c, in, fadd(c, t4, lmul(c, t4, src(c, &in->src[0]), src(c, &in->src[1])), src(c, &in->src[2]))); return;
    case OP_DP3: wr(c, in, splat4(c, ldot(c, src(c, &in->src[0]), src(c, &in->src[1]), 3))); return;
    case OP_DP4: wr(c, in, splat4(c, ldot(c, src(c, &in->src[0]), src(c, &in->src[1]), 4))); return;
    case OP_DP2ADD:
        wr(c, in, splat4(c, fadd(c, tf, ldot(c, src(c, &in->src[0]), src(c, &in->src[1]), 2), comp(c, src(c, &in->src[2]), 0))));
        return;
    case OP_MIN: wr(c, in, ext2(c, t4, GLSLstd450NMin, src(c, &in->src[0]), src(c, &in->src[1]))); return;
    case OP_MAX: wr(c, in, ext2(c, t4, GLSLstd450NMax, src(c, &in->src[0]), src(c, &in->src[1]))); return;
    case OP_SLT: case OP_SGE:
        a = src(c, &in->src[0]); b = src(c, &in->src[1]);
        r = S_OP(S, in->op == OP_SLT ? SpvOpFOrdLessThan : SpvOpFOrdGreaterThanEqual, c->t_bv4, a, b);
        wr(c, in, sel(c, t4, r, c->one4, c->zero4));
        return;
    case OP_RCP: case OP_RSQ: case OP_EXP: case OP_LOG:
        wr(c, in, splat4(c, scalar_op(c, in->op, src_w(c, &in->src[0]))));
        return;
    case OP_EXPP:
        a = src_w(c, &in->src[0]);
        if (c->major < 2) {
            uint32_t fl = ext1(c, tf, GLSLstd450Floor, a);
            wr(c, in, vec4of(c, ext1(c, tf, GLSLstd450Exp2, fl), fsub(c, tf, a, fl), ext1(c, tf, GLSLstd450Exp2, a), cf32(c, 1)));
        } else wr(c, in, splat4(c, ext1(c, tf, GLSLstd450Exp2, a)));
        return;
    case OP_LOGP:
        a = src_w(c, &in->src[0]);
        if (c->major < 2) {
            uint32_t l = scalar_op(c, OP_LOG, a);
            uint32_t ex = ext1(c, tf, GLSLstd450Floor, l);
            uint32_t mant = fdiv(c, tf, ext1(c, tf, GLSLstd450FAbs, a), ext1(c, tf, GLSLstd450Exp2, ex));
            wr(c, in, vec4of(c, ex, mant, l, cf32(c, 1)));
        } else wr(c, in, splat4(c, scalar_op(c, OP_LOG, a)));
        return;
    case OP_POW: {
        uint32_t x = ext1(c, tf, GLSLstd450FAbs, src_w(c, &in->src[0])), y = src_w(c, &in->src[1]);
        wr(c, in, splat4(c, ext1(c, tf, GLSLstd450Exp2, lmul(c, tf, y, ext1(c, tf, GLSLstd450Log2, x)))));
        return;
    }
    case OP_FRC: a = src(c, &in->src[0]); wr(c, in, fsub(c, t4, a, ext1(c, t4, GLSLstd450Floor, a))); return;
    case OP_ABS: wr(c, in, ext1(c, t4, GLSLstd450FAbs, src(c, &in->src[0]))); return;
    case OP_SGN: wr(c, in, ext1(c, t4, GLSLstd450FSign, src(c, &in->src[0]))); return;
    case OP_NRM: {
        a = src(c, &in->src[0]);
        uint32_t l = ldot(c, a, a, 3);
        uint32_t rs = ext1(c, tf, GLSLstd450InverseSqrt, l);
        wr(c, in, lmul(c, t4, a, splat4(c, rs)));
        return;
    }
    case OP_CRS: {
        a = src(c, &in->src[0]); b = src(c, &in->src[1]);
        uint32_t a3 = S_OP(S, SpvOpVectorShuffle, c->t_v3, a, a, 0, 1, 2), b3 = S_OP(S, SpvOpVectorShuffle, c->t_v3, b, b, 0, 1, 2);
        uint32_t x = ext2(c, c->t_v3, GLSLstd450Cross, a3, b3);
        wr(c, in, S_OP(S, SpvOpCompositeConstruct, t4, x, cf32(c, 0)));
        return;
    }
    case OP_SINCOS: {
        a = c->major >= 3 ? comp(c, src(c, &in->src[0]), 0) : src_w(c, &in->src[0]);
        if (c->major >= 3 || c->ps) a = comp(c, src(c, &in->src[0]), (in->src[0].swz & 3));
        wr(c, in, vec4of(c, ext1(c, tf, GLSLstd450Cos, a), ext1(c, tf, GLSLstd450Sin, a), cf32(c, 0), cf32(c, 0)));
        return;
    }
    case OP_LIT: {
        a = src(c, &in->src[0]);
        uint32_t x = comp(c, a, 0), y = comp(c, a, 1), w = comp(c, a, 3);
        uint32_t pw = S_EXT(S, tf, GLSLstd450FClamp, w, cf32(c, -127.9961f), cf32(c, 127.9961f));
        uint32_t xp = S_OP(S, SpvOpFOrdGreaterThan, c->t_b, x, cf32(c, 0));
        uint32_t yp = S_OP(S, SpvOpFOrdGreaterThan, c->t_b, y, cf32(c, 0));
        uint32_t spec = ext1(c, tf, GLSLstd450Exp2, lmul(c, tf, pw, ext1(c, tf, GLSLstd450Log2, y)));
        uint32_t z = sel(c, tf, S_OP(S, SpvOpLogicalAnd, c->t_b, xp, yp), spec, cf32(c, 0));
        wr(c, in, vec4of(c, cf32(c, 1), sel(c, tf, xp, x, cf32(c, 0)), z, cf32(c, 1)));
        return;
    }
    case OP_DST:
        a = src(c, &in->src[0]); b = src(c, &in->src[1]);
        wr(c, in, vec4of(c, cf32(c, 1), lmul(c, tf, comp(c, a, 1), comp(c, b, 1)), comp(c, a, 2), comp(c, b, 3)));
        return;
    case OP_LRP:
        a = src(c, &in->src[0]); b = src(c, &in->src[1]); e = src(c, &in->src[2]);
        wr(c, in, fadd(c, t4, e, lmul(c, t4, a, fsub(c, t4, b, e))));
        return;
    case OP_CMP:
        a = src(c, &in->src[0]); b = src(c, &in->src[1]); e = src(c, &in->src[2]);
        wr(c, in, sel(c, t4, S_OP(S, SpvOpFOrdGreaterThanEqual, c->t_bv4, a, c->zero4), b, e));
        return;
    case OP_CND:
        a = src(c, &in->src[0]); b = src(c, &in->src[1]); e = src(c, &in->src[2]);
        if (c->ps && c->major == 1 && c->minor < 4) a = splat4(c, comp(c, a, 3));
        wr(c, in, sel(c, t4, S_OP(S, SpvOpFOrdGreaterThan, c->t_bv4, a, splat4(c, cf32(c, 0.5f))), b, e));
        return;
    case OP_M4x4: case OP_M4x3: case OP_M3x4: case OP_M3x3: case OP_M3x2: {
        int rows = in->op == OP_M4x4 || in->op == OP_M3x4 ? 4 : in->op == OP_M3x2 ? 2 : 3;
        int n = in->op == OP_M4x4 || in->op == OP_M4x3 ? 4 : 3;
        a = src(c, &in->src[0]);
        uint32_t v[4] = {cf32(c, 0), cf32(c, 0), cf32(c, 0), cf32(c, 0)};
        uint32_t old = 0;
        for (int i = 0; i < rows; i++) {
            struct param p = in->src[1];
            p.num = (uint16_t)(p.num + i);
            v[i] = ldot(c, a, src(c, &p), n);
        }
        r = vec4of(c, v[0], v[1], v[2], v[3]);
        struct ins m = *in;
        m.dst.mask &= (uint8_t)((1 << rows) - 1);
        (void)old;
        wr(c, &m, r);
        return;
    }
    case OP_DSX: wr(c, in, S_OP(S, SpvOpDPdx, t4, src(c, &in->src[0]))); return;
    case OP_DSY: wr(c, in, S_OP(S, SpvOpDPdy, t4, src(c, &in->src[0]))); return;
    case OP_SETP: {
        a = src(c, &in->src[0]); b = src(c, &in->src[1]);
        uint32_t cmp = compare(c, t4, in->ctrl, a, b);
        uint32_t old = spv_load(S, c->t_bv4, c->p0);
        uint32_t m = d->mask;
        if (m != 15) cmp = S_OP(S, SpvOpVectorShuffle, c->t_bv4, old, cmp, m & 1 ? 4u : 0u, m & 2 ? 5u : 1u, m & 4 ? 6u : 2u, m & 8 ? 7u : 3u);
        if (in->pred) cmp = sel(c, c->t_bv4, pred_vec(c, &in->predp), cmp, old);
        spv_store(S, c->p0, cmp);
        return;
    }
    /* flow control */
    case OP_IF: begin_if(c, cond_of(c, &in->src[0])); return;
    case OP_IFC:
        begin_if(c, compare(c, tf, in->ctrl, comp(c, src(c, &in->src[0]), 0), comp(c, src(c, &in->src[1]), 0)));
        return;
    case OP_ELSE: do_else(c); return;
    case OP_ENDIF: end_if(c); return;
    case OP_REP: {
        uint32_t iv = int_const(c, in->src[0].num);
        begin_loop(c, CF_REP, spv_extract(S, c->t_i, iv, 0), 0, 0);
        return;
    }
    case OP_LOOP: {
        uint32_t iv = int_const(c, in->src[1].num);
        begin_loop(c, CF_LOOP, spv_extract(S, c->t_i, iv, 0), spv_extract(S, c->t_i, iv, 1), spv_extract(S, c->t_i, iv, 2));
        return;
    }
    case OP_ENDREP: case OP_ENDLOOP: end_loop(c); return;
    case OP_BREAK: do_break(c, 0); return;
    case OP_BREAKC:
        do_break(c, compare(c, tf, in->ctrl, comp(c, src(c, &in->src[0]), 0), comp(c, src(c, &in->src[1]), 0)));
        return;
    case OP_BREAKP: do_break(c, cond_of(c, &in->src[0])); return;
    case OP_CALL: call_label(c, in->src[0].num); return;
    case OP_CALLNZ:
        begin_if(c, cond_of(c, &in->src[1]));
        call_label(c, in->src[0].num);
        end_if(c);
        return;
    /* textures */
    case OP_TEXKILL: {
        uint32_t v, m;
        if (c->major == 1 && (c->minor < 4 || d->type == R_ADDR)) { v = texcoord_in(c, d->num); m = 7; }
        else { v = load_reg(c, d); m = c->major == 1 ? 7 : d->mask; }
        uint32_t lt = S_OP(S, SpvOpFOrdLessThan, c->t_bv4, v, c->zero4);
        uint32_t any = 0;
        for (int i = 0; i < 4; i++) if (m >> i & 1) {
            uint32_t bi = S_OP(S, SpvOpCompositeExtract, c->t_b, lt, (uint32_t)i);
            any = any ? S_OP(S, SpvOpLogicalOr, c->t_b, any, bi) : bi;
        }
        if (any) demote(c, any);
        return;
    }
    case OP_TEX:
        if (c->major >= 2) {
            int s = in->src[1].num;
            uint32_t coord = src(c, &in->src[0]);
            int mode = in->ctrl == 1 ? TS_PROJ : in->ctrl == 2 ? TS_BIAS : TS_IMPLICIT;
            r = sample(c, s, coord, mode, 3, 0, 0);
            r = swizzle(c, r, in->src[1].swz);
            wr(c, in, r);
        } else if (c->minor < 4) {
            r = sample_stage(c, d->num, texcoord_in(c, d->num), 1);
            treg_set(c, d->num, r);
        } else {
            const struct param *p = &in->src[0];
            uint32_t coord;
            int allow = 0;
            if (p->type == R_ADDR) {
                coord = texcoord_in(c, p->num);
                if (p->mod == M_DZ || p->mod == M_DW) coord = apply_mod(c, coord, p->mod);
                else allow = 1;
            } else coord = src(c, p);
            wr(c, in, sample_stage(c, d->num, coord, allow));
        }
        return;
    case OP_TEXLDL:
        r = sample(c, in->src[1].num, src(c, &in->src[0]), TS_LOD, 0, 0, 0);
        wr(c, in, swizzle(c, r, in->src[1].swz));
        return;
    case OP_TEXLDD:
        r = sample(c, in->src[1].num, src(c, &in->src[0]), TS_GRAD, 0, src(c, &in->src[2]), src(c, &in->src[3]));
        wr(c, in, swizzle(c, r, in->src[1].swz));
        return;
    case OP_TEXCOORD:
        if (c->minor < 4) {
            uint32_t t = saturate(c, t4, texcoord_in(c, d->num));
            treg_set(c, d->num, S_OP(S, SpvOpCompositeInsert, t4, cf32(c, 1), t, 3));
        } else {
            const struct param *p = &in->src[0];
            uint32_t v = p->type == R_ADDR ? texcoord_in(c, p->num) : load_reg(c, p);
            v = swizzle(c, v, p->swz);
            if (p->mod == M_DZ || p->mod == M_DW) v = apply_mod(c, v, p->mod);
            wr(c, in, S_OP(S, SpvOpCompositeInsert, t4, cf32(c, 1), v, 3));
        }
        return;
    case OP_TEXBEM: case OP_TEXBEML: {
        int n = d->num, m = in->src[0].num;
        uint32_t tm = treg_val(c, m);
        uint32_t mat = spv_load(S, t4, S_OP(S, SpvOpAccessChain, c->u_v4, c->ubo, ci32(c, 3), ci32(c, n)));
        uint32_t du = comp(c, tm, 0), dv = comp(c, tm, 1);
        uint32_t tc = texcoord_in(c, n);
        uint32_t u = fadd(c, tf, comp(c, tc, 0), fadd(c, tf, fmul(c, tf, comp(c, mat, 0), du), fmul(c, tf, comp(c, mat, 2), dv)));
        uint32_t v = fadd(c, tf, comp(c, tc, 1), fadd(c, tf, fmul(c, tf, comp(c, mat, 1), du), fmul(c, tf, comp(c, mat, 3), dv)));
        r = sample_stage(c, n, vec4of(c, u, v, comp(c, tc, 2), comp(c, tc, 3)), 1);
        if (in->op == OP_TEXBEML) {
            uint32_t lum = spv_load(S, t4, S_OP(S, SpvOpAccessChain, c->u_v4, c->ubo, ci32(c, 4), ci32(c, n)));
            uint32_t l = saturate(c, tf, fadd(c, tf, fmul(c, tf, comp(c, tm, 2), comp(c, lum, 0)), comp(c, lum, 1)));
            r = S_OP(S, SpvOpVectorTimesScalar, t4, r, l);
        }
        treg_set(c, n, r);
        return;
    }
    case OP_BEM: {
        int n = d->num;
        a = src(c, &in->src[0]); b = src(c, &in->src[1]);
        uint32_t mat = spv_load(S, t4, S_OP(S, SpvOpAccessChain, c->u_v4, c->ubo, ci32(c, 3), ci32(c, n)));
        uint32_t x = comp(c, b, 0), y = comp(c, b, 1);
        uint32_t u = fadd(c, tf, comp(c, a, 0), fadd(c, tf, fmul(c, tf, comp(c, mat, 0), x), fmul(c, tf, comp(c, mat, 2), y)));
        uint32_t v = fadd(c, tf, comp(c, a, 1), fadd(c, tf, fmul(c, tf, comp(c, mat, 1), x), fmul(c, tf, comp(c, mat, 3), y)));
        struct ins m = *in;
        m.dst.mask &= 3;
        wr(c, &m, vec4of(c, u, v, cf32(c, 0), cf32(c, 0)));
        return;
    }
    case OP_TEXREG2AR: case OP_TEXREG2GB: case OP_TEXREG2RGB: {
        uint32_t tm = treg_val(c, in->src[0].num);
        uint32_t coord = in->op == OP_TEXREG2AR ? shuffle4(c, tm, tm, 3, 0, 2, 3)
                       : in->op == OP_TEXREG2GB ? shuffle4(c, tm, tm, 1, 2, 2, 3) : tm;
        treg_set(c, d->num, sample_stage(c, d->num, coord, 0));
        return;
    }
    case OP_TEXDP3TEX: {
        uint32_t x = dot3(c, texcoord_in(c, d->num), treg_val(c, in->src[0].num));
        treg_set(c, d->num, sample_stage(c, d->num, vec4of(c, x, cf32(c, 0), cf32(c, 0), cf32(c, 1)), 0));
        return;
    }
    case OP_TEXDP3:
        treg_set(c, d->num, splat4(c, dot3(c, texcoord_in(c, d->num), treg_val(c, in->src[0].num))));
        return;
    case OP_TEXM3x2PAD: case OP_TEXM3x3PAD: {
        /* the dot product is kept in the destination's own register until the final row */
        uint32_t x;
        texm_row(c, d->num, in->src[0].num, &x);
        treg_set(c, d->num, splat4(c, x));
        return;
    }
    case OP_TEXM3x2TEX: case OP_TEXM3x2DEPTH: {
        uint32_t x = comp(c, treg_val(c, d->num - 1), 0), y;
        texm_row(c, d->num, in->src[0].num, &y);
        if (in->op == OP_TEXM3x2TEX)
            treg_set(c, d->num, sample_stage(c, d->num, vec4of(c, x, y, cf32(c, 0), cf32(c, 1)), 0));
        else {
            uint32_t z = sel(c, tf, S_OP(S, SpvOpFOrdEqual, c->t_b, y, cf32(c, 0)), cf32(c, 1), fdiv(c, tf, x, y));
            spv_store(S, private_v4(c, &c->odepth), splat4(c, z));
            treg_set(c, d->num, vec4of(c, x, y, cf32(c, 0), cf32(c, 1)));
        }
        return;
    }
    case OP_TEXM3x3TEX: case OP_TEXM3x3: case OP_TEXM3x3SPEC: case OP_TEXM3x3VSPEC: {
        uint32_t x = comp(c, treg_val(c, d->num - 2), 0), y = comp(c, treg_val(c, d->num - 1), 0), z;
        texm_row(c, d->num, in->src[0].num, &z);
        uint32_t nrm = vec4of(c, x, y, z, cf32(c, 1));
        if (in->op == OP_TEXM3x3) { treg_set(c, d->num, nrm); return; }
        if (in->op == OP_TEXM3x3TEX) { treg_set(c, d->num, sample_stage(c, d->num, nrm, 0)); return; }
        uint32_t eye;
        if (in->op == OP_TEXM3x3SPEC) eye = src(c, &in->src[1]);
        else eye = vec4of(c, comp(c, texcoord_in(c, d->num - 2), 3), comp(c, texcoord_in(c, d->num - 1), 3),
                          comp(c, texcoord_in(c, d->num), 3), cf32(c, 0));
        /* R = 2 * N * (N.E) / (N.N) - E */
        uint32_t ne = dot3(c, nrm, eye), nn = dot3(c, nrm, nrm);
        uint32_t k = fdiv(c, tf, fmul(c, tf, cf32(c, 2), ne), nn);
        uint32_t refl = fsub(c, t4, S_OP(S, SpvOpVectorTimesScalar, t4, nrm, k), eye);
        treg_set(c, d->num, sample_stage(c, d->num, S_OP(S, SpvOpCompositeInsert, t4, cf32(c, 1), refl, 3), 0));
        return;
    }
    case OP_TEXDEPTH: {
        uint32_t v = load_reg(c, d);
        uint32_t x = comp(c, v, 0), y = comp(c, v, 1);
        uint32_t z = sel(c, tf, S_OP(S, SpvOpFOrdEqual, c->t_b, y, cf32(c, 0)), cf32(c, 1), fdiv(c, tf, x, y));
        spv_store(S, private_v4(c, &c->odepth), splat4(c, saturate(c, tf, z)));
        return;
    }
    }
}

/* ---- module ---- */

static void setup_types(struct ctx *c)
{
    c->t_void = spv_t_void(S);
    c->t_f = spv_t_float(S);
    c->t_v2 = spv_t_vec(S, c->t_f, 2);
    c->t_v3 = spv_t_vec(S, c->t_f, 3);
    c->t_v4 = spv_t_vec(S, c->t_f, 4);
    c->t_i = spv_t_int(S, 1);
    c->t_iv4 = spv_t_vec(S, c->t_i, 4);
    c->t_u = spv_t_int(S, 0);
    c->t_uv4 = spv_t_vec(S, c->t_u, 4);
    c->t_b = spv_t_bool(S);
    c->t_bv4 = spv_t_vec(S, c->t_b, 4);
    c->p_v4 = spv_t_ptr(S, SpvStoragePrivate, c->t_v4);
    c->p_i = spv_t_ptr(S, SpvStoragePrivate, c->t_i);
    c->p_iv4 = spv_t_ptr(S, SpvStoragePrivate, c->t_iv4);
    c->p_bv4 = spv_t_ptr(S, SpvStoragePrivate, c->t_bv4);
    c->f_i = spv_t_ptr(S, SpvStorageFunction, c->t_i);
    c->u_v4 = spv_t_ptr(S, SpvStorageUniform, c->t_v4);
    c->u_iv4 = spv_t_ptr(S, SpvStorageUniform, c->t_iv4);
    c->u_uv4 = spv_t_ptr(S, SpvStorageUniform, c->t_uv4);
    c->pc_f = spv_t_ptr(S, SpvStoragePushConstant, c->t_f);
    c->pc_u = spv_t_ptr(S, SpvStoragePushConstant, c->t_u);
    c->pc_v4 = spv_t_ptr(S, SpvStoragePushConstant, c->t_v4);
    c->in_v4 = spv_t_ptr(S, SpvStorageInput, c->t_v4);
    c->in_f = spv_t_ptr(S, SpvStorageInput, c->t_f);
    c->out_v4 = spv_t_ptr(S, SpvStorageOutput, c->t_v4);
    c->out_f = spv_t_ptr(S, SpvStorageOutput, c->t_f);
    c->zero4 = spv_c_vec4f(S, 0, 0, 0, 0);
    c->one4 = spv_c_vec4f(S, 1, 1, 1, 1);
}

static void setup_ubo(struct ctx *c)
{
    uint32_t nf = c->ps ? DXSO_PS_FLOATS : DXSO_VS_FLOATS;
    c->nfloats = nf;
    uint32_t af = spv_t_array(S, c->t_v4, nf), ai = spv_t_array(S, c->t_iv4, 16);
    decorate_stride(c, af, 16);
    decorate_stride(c, ai, 16);
    uint32_t members[5] = {af, ai, c->t_uv4, 0, 0};
    uint32_t n = 3;
    if (c->ps) {
        uint32_t a8 = spv_t_array(S, c->t_v4, 8);
        decorate_stride(c, a8, 16);
        members[3] = a8; members[4] = a8; n = 5;
    }
    uint32_t st = spv_t_struct(S, members, n);
    spv_decorate(S, st, SpvDecorationBlock, 0);
    uint32_t off = 0;
    spv_member_decorate(S, st, 0, SpvDecorationOffset, 1, off); off += nf * 16;
    spv_member_decorate(S, st, 1, SpvDecorationOffset, 1, off); off += 256;
    spv_member_decorate(S, st, 2, SpvDecorationOffset, 1, off); off += 16;
    if (c->ps) {
        spv_member_decorate(S, st, 3, SpvDecorationOffset, 1, off); off += 128;
        spv_member_decorate(S, st, 4, SpvDecorationOffset, 1, off);
    }
    c->ubo = spv_var(S, spv_t_ptr(S, SpvStorageUniform, st), SpvStorageUniform);
    spv_decorate(S, c->ubo, SpvDecorationDescriptorSet, 1, 0);
    spv_decorate(S, c->ubo, SpvDecorationBinding, 1, c->ps ? DXSO_BIND_PS_CONST : DXSO_BIND_VS_CONST);
}

/* push constant member indices */
enum { PC_FOG_COLOR, PC_FOG_START, PC_FOG_END, PC_FOG_DENSITY, PC_FOG_MODE, PC_ALPHA_REF, PC_ALPHA_FUNC,
       PC_FOG_USE_W, PC_POINT_SIZE, PC_POINT_MIN, PC_POINT_MAX, PC_CLIP_MASK, PC_PAD, PC_CLIP };

static void setup_push(struct ctx *c)
{
    uint32_t a6 = spv_t_array(S, c->t_v4, 6);
    decorate_stride(c, a6, 16);
    uint32_t f = c->t_f, u = c->t_u;
    uint32_t m[14] = {c->t_v4, f, f, f, u, f, u, u, f, f, f, u, u, a6};
    uint32_t st = spv_t_struct(S, m, 14);
    spv_decorate(S, st, SpvDecorationBlock, 0);
    static const uint32_t off[14] = {0, 16, 20, 24, 28, 32, 36, 40, 44, 48, 52, 56, 60, 64};
    for (uint32_t i = 0; i < 14; i++) spv_member_decorate(S, st, i, SpvDecorationOffset, 1, off[i]);
    c->push = spv_var(S, spv_t_ptr(S, SpvStoragePushConstant, st), SpvStoragePushConstant);
}

static uint32_t push_f(struct ctx *c, int m) { return spv_load(S, c->t_f, S_OP(S, SpvOpAccessChain, c->pc_f, c->push, ci32(c, m))); }
static uint32_t push_u(struct ctx *c, int m) { return spv_load(S, c->t_u, S_OP(S, SpvOpAccessChain, c->pc_u, c->push, ci32(c, m))); }

static uint32_t iface_var(struct ctx *c, uint32_t ptr, uint32_t storage)
{
    uint32_t v = spv_var(S, ptr, storage);
    if (c->niface < VKP_COUNT(c->iface)) c->iface[c->niface++] = v;
    return v;
}

static uint32_t builtin(struct ctx *c, uint32_t ptr, uint32_t storage, uint32_t which)
{
    uint32_t v = iface_var(c, ptr, storage);
    spv_decorate(S, v, SpvDecorationBuiltIn, 1, which);
    return v;
}

static uint32_t slot_var(struct ctx *c, int slot, uint32_t storage)
{
    uint32_t ptr = slot == DXSO_SLOT_FOG ? (storage == SpvStorageInput ? c->in_f : c->out_f)
                                         : (storage == SpvStorageInput ? c->in_v4 : c->out_v4);
    uint32_t v = iface_var(c, ptr, storage);
    spv_decorate(S, v, SpvDecorationLocation, 1, (uint32_t)slot);
    return v;
}

/* prologue: copy inputs into the register arrays (runs in the entry point) */
static void prologue(struct ctx *c)
{
    if (!c->ps) {
        for (int r = 0; r < 16; r++) {
            if (!(c->info->in_mask >> r & 1)) continue;
            uint32_t v = iface_var(c, c->in_v4, SpvStorageInput);
            spv_decorate(S, v, SpvDecorationLocation, 1, (uint32_t)r);
            spv_store(S, S_OP(S, SpvOpAccessChain, c->p_v4, c->vin, ci32(c, r)), spv_load(S, c->t_v4, v));
        }
        return;
    }
    uint32_t rd = c->info->slots_read;
    if (c->major < 3 && !c->ff) {
        for (int i = 0; i < 2; i++) {
            if (!(rd >> (DXSO_SLOT_COLOR + i) & 1)) continue;
            uint32_t v = slot_var(c, DXSO_SLOT_COLOR + i, SpvStorageInput);
            if (c->key->flat) spv_decorate(S, v, SpvDecorationFlat, 0);
            spv_store(S, S_OP(S, SpvOpAccessChain, c->p_v4, c->vin, ci32(c, i)), spv_load(S, c->t_v4, v));
        }
        for (int i = 0; i < 8; i++) {
            if (!(rd >> i & 1)) continue;
            uint32_t val;
            if (c->key->point_sprite) {
                uint32_t pc = spv_load(S, c->t_v2, c->point_coord);
                val = vec4of(c, comp(c, S_OP(S, SpvOpCompositeConstruct, c->t_v4, pc, cf32(c, 0), cf32(c, 1)), 0),
                             comp(c, S_OP(S, SpvOpCompositeConstruct, c->t_v4, pc, cf32(c, 0), cf32(c, 1)), 1), cf32(c, 0), cf32(c, 1));
            } else {
                uint32_t v = slot_var(c, i, SpvStorageInput);
                if (c->info->major == 2 && c->in_centroid[i]) spv_decorate(S, v, SpvDecorationCentroid, 0);
                val = spv_load(S, c->t_v4, v);
            }
            spv_store(S, private_v4(c, &c->tcin[i]), val);
            spv_store(S, private_v4(c, &c->treg[i]), val);
        }
    } else {
        /* ps 3.0 (and the fixed-function pixel shader): dcl'd inputs by semantic */
        for (int r = 0; r < 16; r++) {
            if (!c->in_dcl[r]) continue;
            int slot = dxso_slot(c->in_usage[r], c->in_index[r]);
            if (slot < 0) continue;
            uint32_t val;
            if (c->key->point_sprite && c->in_usage[r] == U_TEXCOORD) {
                uint32_t pc = spv_load(S, c->t_v2, c->point_coord);
                val = S_OP(S, SpvOpCompositeConstruct, c->t_v4, pc, cf32(c, 0), cf32(c, 1));
            } else {
                uint32_t v = c->in_vars[slot] ? c->in_vars[slot] : (c->in_vars[slot] = slot_var(c, slot, SpvStorageInput));
                if (c->in_vars[slot] == v && c->in_centroid[r]) spv_decorate(S, v, SpvDecorationCentroid, 0);
                if (c->key->flat && c->in_usage[r] == U_COLOR) spv_decorate(S, v, SpvDecorationFlat, 0);
                if (slot == DXSO_SLOT_FOG) {
                    uint32_t f = spv_load(S, c->t_f, v);
                    val = vec4of(c, f, cf32(c, 0), cf32(c, 0), cf32(c, 0));
                } else val = spv_load(S, c->t_v4, v);
            }
            spv_store(S, S_OP(S, SpvOpAccessChain, c->p_v4, c->vin, ci32(c, r)), val);
        }
    }
}

static void ps_epilogue(struct ctx *c)
{
    uint32_t tf = c->t_f;
    uint32_t col = spv_load(S, c->t_v4, private_v4(c, c->major == 1 ? &c->r[0] : &c->oc[0]));
    if (c->major < 3 || c->ff) {
        /* fixed-function fog */
        uint32_t mode = push_u(c, PC_FOG_MODE);
        uint32_t fc = spv_load(S, c->t_v4, c->frag_coord);
        uint32_t usew = S_OP(S, SpvOpINotEqual, c->t_b, push_u(c, PC_FOG_USE_W), cu32(c, 0));
        uint32_t depth = sel(c, tf, usew, fdiv(c, tf, cf32(c, 1), comp(c, fc, 3)), comp(c, fc, 2));
        uint32_t start = push_f(c, PC_FOG_START), end = push_f(c, PC_FOG_END), dens = push_f(c, PC_FOG_DENSITY);
        uint32_t lin = fdiv(c, tf, fsub(c, tf, end, depth), fsub(c, tf, end, start));
        uint32_t dd = fmul(c, tf, depth, dens);
        uint32_t ex = ext1(c, tf, GLSLstd450Exp2, fmul(c, tf, dd, cf32(c, -1.442695f)));
        uint32_t ex2 = ext1(c, tf, GLSLstd450Exp2, fmul(c, tf, fmul(c, tf, dd, dd), cf32(c, -1.442695f)));
        uint32_t vf = c->in_vars[DXSO_SLOT_FOG];
        if (!vf) vf = c->in_vars[DXSO_SLOT_FOG] = slot_var(c, DXSO_SLOT_FOG, SpvStorageInput);
        uint32_t vert = spv_load(S, tf, vf);
        uint32_t f = sel(c, tf, S_OP(S, SpvOpIEqual, c->t_b, mode, cu32(c, DXSO_FOG_VERTEX)), vert, lin);
        f = sel(c, tf, S_OP(S, SpvOpIEqual, c->t_b, mode, cu32(c, DXSO_FOG_EXP)), ex, f);
        f = sel(c, tf, S_OP(S, SpvOpIEqual, c->t_b, mode, cu32(c, DXSO_FOG_EXP2)), ex2, f);
        f = saturate(c, tf, f);
        uint32_t fogc = spv_load(S, c->t_v4, S_OP(S, SpvOpAccessChain, c->pc_v4, c->push, ci32(c, PC_FOG_COLOR)));
        uint32_t mixed = S_EXT(S, c->t_v4, GLSLstd450FMix, fogc, col, splat4(c, f));
        mixed = S_OP(S, SpvOpCompositeInsert, c->t_v4, comp(c, col, 3), mixed, 3);
        uint32_t on = S_OP(S, SpvOpINotEqual, c->t_b, mode, cu32(c, DXSO_FOG_NONE));
        col = sel(c, c->t_v4, bsplat4(c, on), mixed, col);
    }
    /* alpha test against the 8-bit reference */
    uint32_t func = push_u(c, PC_ALPHA_FUNC);
    uint32_t a = ext1(c, tf, GLSLstd450RoundEven, fmul(c, tf, comp(c, col, 3), cf32(c, 255)));
    uint32_t ref = push_f(c, PC_ALPHA_REF);
    uint32_t pass = spv_c_bool(S, 1);
    static const uint32_t ops[9] = {0, 0, SpvOpFOrdLessThan, SpvOpFOrdEqual, SpvOpFOrdLessThanEqual,
                                    SpvOpFOrdGreaterThan, SpvOpFUnordNotEqual, SpvOpFOrdGreaterThanEqual, 0};
    for (uint32_t k = 1; k <= 7; k++) {
        uint32_t r = k == 1 ? spv_c_bool(S, 0) : S_OP(S, ops[k], c->t_b, a, ref);
        pass = sel(c, c->t_b, S_OP(S, SpvOpIEqual, c->t_b, func, cu32(c, k)), r, pass);
    }
    demote(c, S_OP(S, SpvOpLogicalNot, c->t_b, pass));
    /* outputs */
    uint32_t o0 = iface_var(c, c->out_v4, SpvStorageOutput);
    spv_decorate(S, o0, SpvDecorationLocation, 1, 0);
    spv_store(S, o0, col);
    if (c->major >= 2) {
        for (int i = 1; i < 4; i++) {
            if (!c->oc[i]) continue;
            uint32_t o = iface_var(c, c->out_v4, SpvStorageOutput);
            spv_decorate(S, o, SpvDecorationLocation, 1, (uint32_t)i);
            spv_store(S, o, spv_load(S, c->t_v4, c->oc[i]));
        }
    }
    if (c->info->writes_depth) {
        uint32_t fd = builtin(c, c->out_f, SpvStorageOutput, SpvBuiltInFragDepth);
        spv_store(S, fd, comp(c, spv_load(S, c->t_v4, private_v4(c, &c->odepth)), 0));
    }
}

static void vs_epilogue(struct ctx *c)
{
    uint32_t tf = c->t_f;
    uint32_t pos = 0, psize = 0, written = 0;
    uint32_t slot_val[DXSO_SLOTS] = {0};
#define OUTV(i) spv_load(S, c->t_v4, S_OP(S, SpvOpAccessChain, c->p_v4, c->outs, ci32(c, (i))))
    if (c->major < 3) {
        pos = OUTV(10);
        for (int i = 0; i < 8; i++) if (c->info->slots_written >> i & 1) { slot_val[i] = OUTV(i); written |= 1u << i; }
        for (int i = 0; i < 2; i++) if (c->info->slots_written >> (DXSO_SLOT_COLOR + i) & 1) {
            slot_val[DXSO_SLOT_COLOR + i] = saturate(c, c->t_v4, OUTV(8 + i));
            written |= 1u << (DXSO_SLOT_COLOR + i);
        }
        if (c->info->slots_written >> DXSO_SLOT_FOG & 1) { slot_val[DXSO_SLOT_FOG] = comp(c, OUTV(11), 0); written |= 1u << DXSO_SLOT_FOG; }
        if (c->info->writes_psize) psize = comp(c, OUTV(12), 0);
    } else {
        for (int r = 0; r < 16; r++) {
            if (!c->out_dcl[r]) continue;
            uint32_t u = c->out_usage[r], ix = c->out_index[r];
            if (u == U_POSITION && ix == 0) { pos = OUTV(r); continue; }
            if (u == U_PSIZE && ix == 0) { psize = comp(c, OUTV(r), 0); continue; }
            int s = dxso_slot(u, ix);
            if (s < 0 || (written >> s & 1)) continue;
            slot_val[s] = s == DXSO_SLOT_FOG ? comp(c, OUTV(r), 0) : OUTV(r);
            written |= 1u << s;
        }
    }
    if (!pos) pos = c->zero4;
    uint32_t vpos = builtin(c, c->out_v4, SpvStorageOutput, SpvBuiltInPosition);
    spv_store(S, vpos, pos);
    /* point size */
    uint32_t ps = psize ? psize : push_f(c, PC_POINT_SIZE);
    ps = S_EXT(S, tf, GLSLstd450FClamp, ps, push_f(c, PC_POINT_MIN), push_f(c, PC_POINT_MAX));
    spv_store(S, builtin(c, c->out_f, SpvStorageOutput, SpvBuiltInPointSize), ps);
    /* user clip planes */
    uint32_t a6 = spv_t_array(S, tf, 6);
    uint32_t clip = builtin(c, spv_t_ptr(S, SpvStorageOutput, a6), SpvStorageOutput, SpvBuiltInClipDistance);
    uint32_t mask = push_u(c, PC_CLIP_MASK);
    uint32_t out_fp = spv_t_ptr(S, SpvStorageOutput, tf);
    for (int i = 0; i < 6; i++) {
        uint32_t plane = spv_load(S, c->t_v4, S_OP(S, SpvOpAccessChain, c->pc_v4, c->push, ci32(c, PC_CLIP), ci32(c, i)));
        uint32_t d = S_OP(S, SpvOpDot, tf, pos, plane);
        uint32_t on = S_OP(S, SpvOpINotEqual, c->t_b, S_OP(S, SpvOpBitwiseAnd, c->t_u, mask, cu32(c, 1u << i)), cu32(c, 0));
        spv_store(S, S_OP(S, SpvOpAccessChain, out_fp, clip, ci32(c, i)), sel(c, tf, on, d, cf32(c, 1)));
    }
    /* linkage: what the shader wrote, defaults for the rest */
    uint32_t want = DXSO_SLOTS_DEFAULT | c->key->vs_extra | written;
    for (int s = 0; s < DXSO_SLOTS; s++) {
        if (!(want >> s & 1)) continue;
        uint32_t v = slot_val[s];
        if (!v) {
            if (s == DXSO_SLOT_FOG) v = cf32(c, 0);
            else if (s == DXSO_SLOT_COLOR || s == DXSO_SLOT_COLOR + 1) v = c->one4;
            else v = c->zero4;
        }
        spv_store(S, slot_var(c, s, SpvStorageOutput), v);
    }
#undef OUTV
}

static void collect(struct ctx *c, const struct ins *in)
{
    const struct param *d = &in->dst;
    if (in->op == OP_DEF && d->num < 256) {
        union { uint32_t u; float f; } v[4];
        for (int i = 0; i < 4; i++) v[i].u = in->lit[i];
        c->def_f[d->num] = spv_c_vec4f(S, v[0].f, v[1].f, v[2].f, v[3].f);
        c->has_def[d->num] = 1;
    } else if (in->op == OP_DEFI) {
        for (int i = 0; i < 4; i++) c->defi[d->num & 15][i] = (int32_t)in->lit[i];
        c->defi_mask |= (uint16_t)(1u << (d->num & 15));
    } else if (in->op == OP_DEFB) {
        c->defb_mask |= (uint16_t)(1u << (d->num & 15));
        if (in->lit[0]) c->defb_val |= (uint16_t)(1u << (d->num & 15));
    } else if (in->op == OP_DCL) {
        if (d->type == R_INPUT && d->num < 16) {
            c->in_dcl[d->num] = 1;
            c->in_usage[d->num] = in->dcl & 31;
            c->in_index[d->num] = (in->dcl >> 16) & 15;
            c->in_centroid[d->num] = (d->dmod & 4) != 0;
        } else if (d->type == R_ADDR && d->num < 8) {
            c->in_centroid[d->num] = (d->dmod & 4) != 0;  /* ps 2.x dcl t# */
        } else if (d->type == R_OUTPUT && d->num < 16) {
            c->out_dcl[d->num] = 1;
            c->out_usage[d->num] = in->dcl & 31;
            c->out_index[d->num] = (in->dcl >> 16) & 15;
        } else if (d->type == R_MISC) {
            if (d->num == 0) c->uses_vpos = 1; else c->uses_vface = 1;
        }
    }
    for (int i = 0; i < in->nsrc; i++) {
        const struct param *p = &in->src[i];
        if (p->type == R_MISC) { if (p->num == 0) c->uses_vpos = 1; else c->uses_vface = 1; }
        if (p->rel && (p->type == R_CONST)) c->need_float_array = 1;
    }
}

uint32_t *dxso_compile(const uint32_t *code, const struct dxso_info *info, const struct dxso_key *key, uint32_t *words)
{
    struct ctx *c = vkp_alloc(sizeof *c);
    if (!c) return 0;
    spv_init(S);
    c->info = info; c->key = key;
    c->ps = info->ps; c->major = info->major; c->minor = info->minor;
    c->ff = info->major == 3 && info->minor == 0xff;
    /* decode everything once */
    uint32_t cap = 256, n = 0;
    struct ins *all = vkp_alloc(cap * sizeof *all);
    uint32_t pos = 1;
    int err = 0;
    for (;;) {
        if (n == cap) {
            struct ins *grown = vkp_realloc(all, cap * 2 * sizeof *all);
            if (!grown) { err = 1; break; }
            all = grown; cap *= 2;
        }
        if (!all || !decode(code, info->words, &pos, c->major, c->ps, &all[n], &err)) break;
        n++;
    }
    if (err || !all) { vkp_free(all); spv_free(S); vkp_free(c); return 0; }

    spv_capability(S, 1 /* Shader */);
    setup_types(c);
    for (uint32_t i = 0; i < n; i++) collect(c, &all[i]);
    setup_ubo(c);
    setup_push(c);
    if (c->ps) {
        spv_extension(S, "SPV_EXT_demote_to_helper_invocation");
        spv_capability(S, 5379 /* DemoteToHelperInvocationEXT */);
        c->frag_coord = builtin(c, c->in_v4, SpvStorageInput, SpvBuiltInFragCoord);
        if (c->uses_vface) c->front_facing = builtin(c, spv_t_ptr(S, SpvStorageInput, c->t_b), SpvStorageInput, SpvBuiltInFrontFacing);
        if (key->point_sprite) c->point_coord = builtin(c, spv_t_ptr(S, SpvStorageInput, c->t_v2), SpvStorageInput, SpvBuiltInPointCoord);
    } else {
        spv_capability(S, 32 /* ClipDistance */);
    }
    c->vin_t = spv_t_array(S, c->t_v4, 16);
    c->vin = var_init(c, spv_t_ptr(S, SpvStoragePrivate, c->vin_t), SpvStoragePrivate, spv_c_null(S, c->vin_t));
    if (!c->ps) {
        c->outs = var_init(c, spv_t_ptr(S, SpvStoragePrivate, c->vin_t), SpvStoragePrivate, spv_c_null(S, c->vin_t));
        c->a0 = var_init(c, c->p_iv4, SpvStoragePrivate, spv_c_null(S, c->t_iv4));
    }
    c->al = var_init(c, c->p_i, SpvStoragePrivate, ci32(c, 0));
    c->p0 = var_init(c, c->p_bv4, SpvStoragePrivate, spv_c_null(S, c->t_bv4));
    if (c->need_float_array) {
        int any_def = 0;
        for (int i = 0; i < 256; i++) any_def |= c->has_def[i];
        if (any_def) {
            uint32_t at = spv_t_array(S, c->t_v4, c->nfloats);
            c->cfloat_array = var_init(c, spv_t_ptr(S, SpvStoragePrivate, at), SpvStoragePrivate, spv_c_null(S, at));
        }
    }

    /* body and subroutines */
    c->body_fn = spv_id(S);
    uint32_t fn_t = spv_t_fn(S, c->t_void, 0, 0);
    spv_func_begin(S, c->body_fn, c->t_void, fn_t);
    spv_label(S, 0);
    c->in_function = 1;
    for (uint32_t i = 0; i < n; i++) {
        const struct ins *in = &all[i];
        if (in->op == OP_LABEL) {
            if (c->in_function) {
                while (c->ncf) { if (c->cf[c->ncf - 1].kind == CF_IF) end_if(c); else end_loop(c); }
                if (!c->terminated) S_OPV(S, SpvOpReturn);
                spv_func_end(S);
            }
            spv_func_begin(S, label_function(c, in->src[0].num), c->t_void, fn_t);
            spv_label(S, 0);
            c->in_function = 1; c->terminated = 0;
            continue;
        }
        if (!c->in_function) continue;
        if (in->op == OP_RET) {
            if (c->ncf == 0) {
                if (!c->terminated) S_OPV(S, SpvOpReturn);
                spv_func_end(S);
                c->in_function = 0; c->terminated = 0;
            } else {
                new_block_if_terminated(c);
                S_OPV(S, SpvOpReturn);
                c->terminated = 1;
            }
            continue;
        }
        new_block_if_terminated(c);
        emit(c, in, all, (int)n, (int)i);
    }
    if (c->in_function) {
        while (c->ncf) { if (c->cf[c->ncf - 1].kind == CF_IF) end_if(c); else end_loop(c); }
        if (!c->terminated) S_OPV(S, SpvOpReturn);
        spv_func_end(S);
    }
    /* subroutines that were called but never defined */
    for (int i = 0; i < 64; i++) {
        if (!c->label_fn[i]) continue;
        int defined = 0;
        for (uint32_t k = 0; k < n; k++) if (all[k].op == OP_LABEL && (all[k].src[0].num & 63) == (uint32_t)i) defined = 1;
        if (defined) continue;
        spv_func_begin(S, c->label_fn[i], c->t_void, fn_t);
        spv_label(S, 0);
        S_OPV(S, SpvOpReturn);
        spv_func_end(S);
    }

    /* entry point */
    uint32_t main_fn = spv_id(S);
    spv_func_begin(S, main_fn, c->t_void, fn_t);
    spv_label(S, 0);
    if (c->cfloat_array) {
        for (uint32_t i = 0; i < c->nfloats; i++) {
            uint32_t v = i < 256 && c->has_def[i] ? c->def_f[i]
                       : spv_load(S, c->t_v4, S_OP(S, SpvOpAccessChain, c->u_v4, c->ubo, ci32(c, 0), ci32(c, (int32_t)i)));
            spv_store(S, S_OP(S, SpvOpAccessChain, c->p_v4, c->cfloat_array, ci32(c, (int32_t)i)), v);
        }
    }
    prologue(c);
    S_OP(S, SpvOpFunctionCall, c->t_void, c->body_fn);
    if (c->ps) ps_epilogue(c); else vs_epilogue(c);
    S_OPV(S, SpvOpReturn);
    spv_func_end(S);
    spv_entry(S, c->ps ? 4 : 0, main_fn, "main", c->iface, c->niface);
    if (c->ps) {
        spv_mode(S, main_fn, 7 /* OriginUpperLeft */);
        if (info->writes_depth) spv_mode(S, main_fn, 12 /* DepthReplacing */);
    }
    vkp_free(all);
    uint32_t *out = S->failed ? 0 : spv_finish(S, words);
    spv_free(S);
    vkp_free(c);
    return out;
}
