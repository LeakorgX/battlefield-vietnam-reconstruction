/* D3D9 shader bytecode to SPIR-V. */
#ifndef BFV_DXSO_H
#define BFV_DXSO_H
#include <stdint.h>

/* Descriptor set 0 (push descriptors), shared by every pipeline */
#define DXSO_BIND_VS_CONST 0    /* struct dxso_vs_consts */
#define DXSO_BIND_PS_CONST 1    /* struct dxso_ps_consts */
#define DXSO_BIND_PS_SAMPLER 2  /* 2..17: ps samplers s0..s15 */
#define DXSO_BIND_VS_SAMPLER 18 /* 18..21: vertex samplers (D3DVERTEXTEXTURESAMPLER0..3) */
#define DXSO_BINDINGS 22

#define DXSO_VS_FLOATS 256
#define DXSO_PS_FLOATS 224
struct dxso_vs_consts {
    float f[DXSO_VS_FLOATS][4];
    int32_t i[16][4];
    uint32_t b[4];              /* b[0] bit n = bool constant n */
};
struct dxso_ps_consts {
    float f[DXSO_PS_FLOATS][4];
    int32_t i[16][4];
    uint32_t b[4];
    float bump[8][4];           /* stage n: BUMPENVMAT00, 01, 10, 11 */
    float lum[8][4];            /* stage n: BUMPENVLSCALE, BUMPENVLOFFSET */
};

/* Push constants (both stages) */
enum { DXSO_FOG_NONE, DXSO_FOG_VERTEX, DXSO_FOG_LINEAR, DXSO_FOG_EXP, DXSO_FOG_EXP2 };
struct dxso_push {
    float fog_color[4];
    float fog_start, fog_end, fog_density;
    uint32_t fog_mode;          /* DXSO_FOG_*; table modes use depth */
    float alpha_ref;            /* 0..255 */
    uint32_t alpha_func;        /* D3DCMP_* (8 = always) */
    uint32_t fog_use_w;         /* table fog from w instead of z */
    float point_size;
    float point_min, point_max;
    uint32_t clip_mask;         /* user clip planes enabled */
    uint32_t pad;
    float clip[6][4];           /* clip-space planes */
};

/* Linkage slots (Vulkan locations) by D3D semantic */
#define DXSO_SLOT_TEXCOORD 0    /* 0..15 */
#define DXSO_SLOT_COLOR 16      /* 16, 17 */
#define DXSO_SLOT_FOG 18        /* float */
#define DXSO_SLOT_NORMAL 19     /* 19, 20 */
#define DXSO_SLOT_TANGENT 21
#define DXSO_SLOT_BINORMAL 22
#define DXSO_SLOT_POSITION1 23
#define DXSO_SLOT_BLENDWEIGHT 24
#define DXSO_SLOT_BLENDINDICES 25
#define DXSO_SLOT_PSIZE1 26
#define DXSO_SLOT_DEPTH 27
#define DXSO_SLOT_COLOR2 28     /* 28, 29 */
#define DXSO_SLOTS 30
/* Slots every vertex shader writes (defaults when the shader does not). */
#define DXSO_SLOTS_DEFAULT (0xffu | 1u << 16 | 1u << 17 | 1u << 18)
int dxso_slot(uint32_t usage, uint32_t index);  /* -1: none */

/* Variant: what the shader code depends on besides its bytecode */
enum { DXSO_TEX_2D = 0, DXSO_TEX_CUBE = 1, DXSO_TEX_3D = 2, DXSO_TEX_KIND = 3, DXSO_TEX_COMPARE = 4 };
struct dxso_key {
    uint8_t tex[16];            /* ps: sampler kinds (ps 1.x: from bound textures) | COMPARE; vs: 0..3 */
    uint8_t proj[8];            /* ps 1.x: D3DTTFF count when D3DTTFF_PROJECTED, else 0 */
    uint8_t point_sprite;       /* ps < 3: texcoords from the point coordinate */
    uint8_t flat;               /* D3DSHADE_FLAT: colours not interpolated */
    uint8_t pad[2];
    uint32_t vs_extra;          /* vs: further slots to write (read by the pixel shader) */
};

/* Facts about a shader, independent of the variant */
struct dxso_info {
    uint32_t version;           /* first token */
    int ps, major, minor;
    uint32_t in_mask;           /* vs: input registers v# used */
    uint8_t in_usage[16], in_index[16];
    uint32_t slots_written;     /* vs: linkage slots written */
    uint32_t slots_read;        /* ps: linkage slots read */
    uint32_t samplers;          /* sampler registers used */
    uint8_t sampler_dcl[20];    /* DXSO_TEX_* + 1 from dcl, 0 = undeclared (ps 1.x) */
    uint32_t tex_proj_used;     /* ps 1.x: stages sampled from texcoords (projection applies) */
    int writes_depth;
    int writes_psize;
    uint32_t words;             /* bytecode length including END */
};

/* Parse the bytecode; 0 on malformed input. */
int dxso_scan(const uint32_t *code, uint32_t max_words, struct dxso_info *info);
/* Translate; returns SPIR-V words (vkp_free) or 0. */
uint32_t *dxso_compile(const uint32_t *code, const struct dxso_info *info, const struct dxso_key *key,
                       uint32_t *words);

#endif
