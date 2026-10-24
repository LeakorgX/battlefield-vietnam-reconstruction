/* Fixed-function vertex and pixel processing as generated shader bytecode.
 *
 * The device reduces the D3D9 fixed-function state to a key, the generator
 * turns the key into vs_3_0 / ps_3_0 tokens (minor version 0xff marks them
 * as fixed-function for dxso.c) and the result is translated like any
 * application shader. Uniform values live in the ordinary float constants,
 * at the FF_VS_* / FF_PS_* registers below, filled by the device. */
#ifndef BFV_FF_H
#define BFV_FF_H
#include <stdint.h>

/* Vertex shader inputs (v#), matched to the declaration by semantic */
enum { FF_IN_POSITION, FF_IN_NORMAL, FF_IN_COLOR0, FF_IN_COLOR1, FF_IN_PSIZE, FF_IN_BLENDWEIGHT, FF_IN_TEXCOORD };

/* Vertex constants (vec4 index). Matrices are stored transposed (column per
 * register), so dp4 against consecutive registers is a row-vector multiply. */
#define FF_VS_WORLDVIEW 0      /* 4 x 4: world(i) * view, i = blend matrix */
#define FF_VS_NORMAL 16        /* 4 x 4: inverse transpose of worldview(i), 3 used */
#define FF_VS_PROJ 32          /* 4 */
#define FF_VS_TEXTURE 36       /* 8 x 4: D3DTS_TEXTURE0..7 */
#define FF_VS_LIGHT 68         /* 8 x 8, see FF_LIGHT_* */
#define FF_VS_MAT_DIFFUSE 132
#define FF_VS_MAT_AMBIENT 133
#define FF_VS_MAT_SPECULAR 134
#define FF_VS_MAT_EMISSIVE 135
#define FF_VS_MAT_POWER 136    /* x */
#define FF_VS_AMBIENT 137      /* D3DRS_AMBIENT */
#define FF_VS_FOG 138          /* start, end, 1 / (end - start), density */
#define FF_VS_POINT 139        /* size, min, max, viewport height */
#define FF_VS_POINT_SCALE 140  /* A, B, C, 0 */
#define FF_VS_VIEWPORT_SCALE 141   /* window -> NDC for XYZRHW */
#define FF_VS_VIEWPORT_OFFSET 142
#define FF_VS_CONSTS 143

/* per light: */
#define FF_LIGHT_DIFFUSE 0
#define FF_LIGHT_SPECULAR 1
#define FF_LIGHT_AMBIENT 2
#define FF_LIGHT_POSITION 3    /* view space */
#define FF_LIGHT_DIRECTION 4   /* view space, normalised, towards the light (-Direction) */
#define FF_LIGHT_ATTEN 5       /* range, att0, att1, att2 */
#define FF_LIGHT_SPOT 6        /* cos(phi/2), 1 / (cos(theta/2) - cos(phi/2)), falloff, 0 */
#define FF_LIGHT_STRIDE 8

/* Pixel constants */
#define FF_PS_TFACTOR 0
#define FF_PS_STAGE_CONST 1    /* 8: D3DTSS_CONSTANT */
#define FF_PS_BUMP 16          /* 8: BUMPENVMAT00, 01, 10, 11 */
#define FF_PS_LUM 24           /* 8: BUMPENVLSCALE, BUMPENVLOFFSET */
#define FF_PS_CONSTS 32

enum { FF_LIGHT_NONE, FF_LIGHT_POINT_T, FF_LIGHT_SPOT_T, FF_LIGHT_DIRECTIONAL_T };
enum { FF_FOG_NONE, FF_FOG_EXP, FF_FOG_EXP2, FF_FOG_LINEAR, FF_FOG_SPECULAR_ALPHA };

struct ff_vs_key {
    uint8_t rhw;               /* D3DDECLUSAGE_POSITIONT */
    uint8_t has_normal, has_color[2], has_psize;
    uint8_t blend;             /* blend weights from D3DRS_VERTEXBLEND (0..3) */
    uint8_t lighting, normalize, local_viewer, specular, color_vertex;
    uint8_t mat_src[4];        /* diffuse, ambient, specular, emissive: D3DMCS_* */
    uint8_t light[8];          /* FF_LIGHT_*_T of enabled lights, packed */
    uint8_t fog;               /* FF_FOG_*: vertex fog computed in the shader */
    uint8_t range_fog;
    uint8_t point_scale;
    uint8_t ntex;              /* texture coordinate outputs (stages) */
    uint8_t tc_index[8];       /* D3DTSS_TEXCOORDINDEX & 7 */
    uint8_t tc_gen[8];         /* D3DTSS_TCI_* >> 16 */
    uint8_t tc_count[8];       /* D3DTTFF count, 0 = no transform */
    uint8_t tc_proj[8];        /* D3DTTFF_PROJECTED */
    uint8_t in_dim[8];         /* components of input texture coordinate set n (0 = absent) */
};

struct ff_stage {
    uint8_t cop, carg[3];      /* D3DTOP_*, args 0, 1, 2 (D3DTA_*) */
    uint8_t aop, aarg[3];
    uint8_t temp;              /* D3DTSS_RESULTARG == D3DTA_TEMP */
    uint8_t tex;               /* DXSO_TEX_* + 1, 0 = no texture bound (reads white) */
    uint8_t proj;              /* sample projected */
    uint8_t pad;
};
struct ff_ps_key {
    struct ff_stage st[8];
    uint8_t specular;          /* D3DRS_SPECULARENABLE */
    uint8_t pad[3];
};

/* Token streams (vkp_free); *words receives the count */
uint32_t *ff_vs_generate(const struct ff_vs_key *k, uint32_t *words);
uint32_t *ff_ps_generate(const struct ff_ps_key *k, uint32_t *words);

#endif
