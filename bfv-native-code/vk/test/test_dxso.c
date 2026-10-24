/* Translates a corpus of D3D9 shaders and writes each SPIR-V module to
 * out/<name>.spv for spirv-val (run_dxso.sh). Assembly and HLSL go through the
 * system d3dx9_43.dll; the project's precompiled shaders are included as-is. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>
#include "../dxso.h"
#include "../ff.h"
#include "../vkp.h"

#include "../../mods/shaders/world_post_aa.h"
#include "../../mods/shaders/world_post_adapt.h"
#include "../../mods/shaders/world_post_bloom_down.h"
#include "../../mods/shaders/world_post_bloom_up.h"
#include "../../mods/shaders/world_post_composite.h"
#include "../../mods/shaders/world_post_depth_lin.h"
#include "../../mods/shaders/world_post_finish.h"
#include "../../mods/shaders/world_post_post_vs.h"
#include "../../mods/shaders/world_post_ssao.h"

typedef struct buf_vt {
    void *qi, *addref;
    ULONG(__stdcall *release)(void *);
    void *(__stdcall *ptr)(void *);
    DWORD(__stdcall *size)(void *);
} buf_vt;
typedef struct { buf_vt *vt; } dxbuf;
typedef HRESULT(WINAPI *assemble_fn)(const char *, UINT, const void *, void *, DWORD, dxbuf **, dxbuf **);
typedef HRESULT(WINAPI *compile_fn)(const char *, UINT, const void *, void *, const char *, const char *, DWORD,
                                    dxbuf **, dxbuf **, void **);
static assemble_fn assemble;
static compile_fn compile;

static int failures, total;

static void translate(const char *name, const uint32_t *code, uint32_t words, const struct dxso_key *key)
{
    struct dxso_info info;
    total++;
    if (!dxso_scan(code, words, &info)) { printf("FAIL scan %s\n", name); failures++; return; }
    uint32_t n = 0;
    uint32_t *spv = dxso_compile(code, &info, key, &n);
    if (!spv) { printf("FAIL compile %s\n", name); failures++; return; }
    char path[256];
    snprintf(path, sizeof path, "out/%s.spv", name);
    vkp_write_file(path, spv, n * 4);
    vkp_free(spv);
}

static void from_buffer(const char *name, dxbuf *b, const struct dxso_key *key)
{
    translate(name, b->vt->ptr(b), b->vt->size(b) / 4, key);
    b->vt->release(b);
}

static void asm_case(const char *name, const char *src, const struct dxso_key *key)
{
    dxbuf *code = 0, *err = 0;
    HRESULT hr = assemble(src, (UINT)strlen(src), 0, 0, 2 /* SKIPVALIDATION */, &code, &err);
    if (FAILED(hr)) {
        printf("FAIL assemble %s: %s\n", name, err ? (char *)err->vt->ptr(err) : "?");
        failures++; total++;
        return;
    }
    from_buffer(name, code, key);
}

static void hlsl_case(const char *name, const char *src, const char *entry, const char *profile, const struct dxso_key *key)
{
    dxbuf *code = 0, *err = 0;
    HRESULT hr = compile(src, (UINT)strlen(src), 0, 0, entry, profile, 0, &code, &err, 0);
    if (FAILED(hr)) {
        printf("FAIL hlsl %s: %s\n", name, err ? (char *)err->vt->ptr(err) : "?");
        failures++; total++;
        return;
    }
    from_buffer(name, code, key);
}

static const struct dxso_key k0;

/* ---- assembly corpus ---- */
static const char vs11[] =
    "vs_1_1\n"
    "dcl_position v0\n dcl_normal v1\n dcl_texcoord v2\n dcl_color v3\n dcl_blendweight v4\n dcl_blendindices v5\n"
    "def c95, 0, 0.5, 1, 765.01\n"
    "m4x4 oPos, v0, c0\n"
    "dp3 r0.x, v1, c4\n max r0.x, r0.x, c95.x\n mul oD0, r0.x, v3\n mov oD1, c5\n"
    "mul r1.x, v5.z, c95.w\n mov a0.x, r1.x\n m4x3 r2.xyz, v0, c[a0.x + 10]\n"
    "dp4 r3.x, v0, c6\n rcp r3.y, r3.x\n rsq r3.z, r3.x\n expp r4, r3.x\n logp r5, r3.y\n exp r6.x, r3.z\n log r6.y, r3.z\n"
    "lit r7, r3\n dst r8, r3, r4\n frc r9.xy, r3\n slt r10, r3, r4\n sge r11, r3, r4\n min r0, r3, r4\n"
    "m3x3 r1.xyz, v1, c20\n m3x2 r2.xy, v1, c23\n m3x4 r4, v1, c24\n"
    "mad oT0.xy, v2, c7, c8\n mov oT1, r2\n mov oFog, r3.x\n mov oPts, c9.x\n add oT2, r7, r8\n sub oT3, r1, r2\n"
    "mad oT4, r4, r9, r10\n add oT5, r11, r0\n";

static const char vs20[] =
    "vs_2_0\n"
    "dcl_position v0\n dcl_texcoord0 v1\n"
    "defi i0, 4, 0, 1, 0\n defb b1, true\n def c100, 1, 2, 3, 4\n"
    "mov r0, c100\n mov r1, v0\n"
    "rep i0\n add r1, r1, r0\n endrep\n"
    "loop aL, i0\n add r1, r1, c[aL + 10]\n endloop\n"
    "if b1\n mul r1, r1, c1\n else\n mul r1, r1, c2\n endif\n"
    "if b2\n call l1\n endif\n callnz l1, b3\n"
    "mova a0.x, r0.x\n mov r2, c[a0.x + 5]\n"
    "sgn r3, r1, r4, r5\n abs r6, r1\n nrm r7, r1\n crs r8.xyz, r1, r2\n pow r9.x, r1.x, r2.y\n sincos r10.xy, r1.x, c50, c51\n lrp r11, r1, r2, r3\n"
    "mov oPos, r1\n add oT0, r2, r3\n add oT1, r6, r7\n add oT2, r8, r9\n add oT3, r10, r11\n mov oD0, v1\n"
    "ret\n"
    "label l1\n add r1, r1, c3\n ret\n";

static const char vs30[] =
    "vs_3_0\n"
    "dcl_position v0\n dcl_texcoord0 v1\n dcl_normal v2\n"
    "dcl_position o0\n dcl_texcoord0 o1\n dcl_texcoord1 o2\n dcl_color o3\n dcl_fog o4.x\n dcl_psize o5\n"
    "dcl_2d s0\n"
    "defi i0, 3, 0, 1, 0\n def c200, 0, 1, 0.5, 2\n"
    "mov r0, v0\n"
    "loop aL, i0\n add r0, r0, c[aL]\n endloop\n"
    "setp_gt p0, r0, c200.x\n (p0) mov r0.x, c200.y\n (!p0.y) add r0.y, r0.y, c200.z\n"
    "if_lt r0.x, c200.w\n break_ge r0.x, r0.y\n endif\n"
    "rep i0\n break_eq r0.x, c200.x\n breakp p0.x\n add r0.z, r0.z, c200.z\n endrep\n"
    "texldl r1, v1, s0\n"
    "sincos r2.xy, r0.x\n sgn r3, r0, r4, r5\n"
    "mov o0, r0\n mov o1, r1\n mov o2, r2\n mov o3, r3\n mov o4.x, r0.w\n mov o5, c200.w\n";

static const char ps11[] =
    "ps_1_1\n"
    "def c7, 0.5, 0.25, 1, 0\n"
    "tex t0\n texbem t1, t0\n texcoord t2\n texkill t3\n"
    "mul r0, t0, v0\n mad r1, t1, c0, -v1\n lrp r0.rgb, c1, r0, r1\n + mov r0.a, c7.y\n"
    "cnd r0, r0.a, r0, r1\n dp3_sat r1, t2_bx2, v0_bx2\n add_x2 r0, r0, r1\n sub_d2 r0, r0, 1-r1\n";

static const char ps11b[] =
    "ps_1_1\n"
    "tex t0\n texm3x3pad t1, t0_bx2\n texm3x3pad t2, t0_bx2\n texm3x3vspec t3, t0_bx2\n"
    "mul r0, t3, t0\n";

static const char ps12[] =
    "ps_1_2\n"
    "tex t0\n texreg2ar t1, t0\n texreg2gb t2, t0\n texdp3tex t3, t0\n"
    "add r0, t1, t2\n add r0, r0, t3\n";

static const char ps13[] =
    "ps_1_3\n"
    "tex t0\n texm3x2pad t1, t0_bx2\n texm3x2depth t2, t0_bx2\n texbeml t3, t0\n"
    "mov r0, t3\n";

static const char ps13b[] =
    "ps_1_3\n"
    "tex t0\n texm3x3pad t1, t0\n texm3x3pad t2, t0\n texm3x3spec t3, t0, c0\n"
    "mov r0, t3\n";

static const char ps13c[] =
    "ps_1_3\n"
    "tex t0\n texm3x2pad t1, t0\n texm3x2tex t2, t0\n texdp3 t3, t0\n"
    "add r0, t2, t3\n";

static const char ps14[] =
    "ps_1_4\n"
    "def c0, 0.5, 0.5, 0.5, 1\n"
    "texld r0, t0\n texcrd r1.rgb, t1\n texld r2, t2_dw.xyw\n texkill t3\n texdepth r5\n"
    "bem r3.rg, r1, r0\n"
    "phase\n"
    "texld r3, r3\n texld r4, r0\n"
    "mul r0, r3, r4\n cmp r1, r0, c0, r2\n dp3 r0.a, r0, c0\n cnd r0.rgb, r1, r0, c0\n mov r5.r, c0\n";

static const char ps20[] =
    "ps_2_0\n"
    "dcl t0.xy\n dcl t1\n dcl v0\n dcl_2d s0\n dcl_cube s1\n dcl_volume s2\n"
    "def c10, 0, 1, 0.5, 2\n"
    "texld r0, t0, s0\n texldp r1, t1, s0\n texldb r2, t1, s1\n texld r3, t1, s2\n"
    "texkill t1\n"
    "dp2add r4, r0, r1, c10.x\n rcp r5, r0.x\n rsq r6, r0.y\n exp r7, r0.z\n log r8, r0.w\n pow r9, r0.x, r1.x\n"
    "cmp r10, r0, r1, r2\n min r11, r0, r3\n max r11, r11, v0\n frc r11, r11\n"
    "mad r0, r4, r5, r6\n add r0, r0, r7\n add r0, r0, r8\n add r0, r0, r9\n add r0, r0, r10\n add r0, r0, r11\n"
    "mov oC0, r0\n mov oC1, r1\n mov oDepth, r0.x\n";

static const char ps2x[] =
    "ps_2_x\n"
    "dcl t0\n dcl_2d s0\n defb b0, true\n def c1, 0, 1, 0, 0\n"
    "texld r0, t0, s0\n dsx r1, t0\n dsy r2, t0\n texldd r3, t0, s0, r1, r2\n"
    "setp_lt p0, r0, c1.y\n (p0) mov r0, r3\n"
    "if b0\n add r0, r0, c1\n endif\n"
    "mov oC0, r0\n";

static const char ps30[] =
    "ps_3_0\n"
    "dcl_texcoord0 v0\n dcl_texcoord1_centroid v1\n dcl_color0 v2\n dcl_fog v3.x\n dcl vPos.xy\n dcl vFace\n"
    "dcl_2d s0\n dcl_cube s3\n"
    "defi i0, 4, 0, 1, 0\n def c0, 0, 1, 0.5, 2\n"
    "mov r0, c0.x\n"
    "loop aL, i0\n texld r1, v[aL], s0\n add r0, r0, r1\n endloop\n"
    "if_gt vFace, c0.x\n add r0, r0, v2\n endif\n"
    "texld r2, v1, s3\n mad r0, r2, vPos.xyxy, r0\n add r0.x, r0.x, v3.x\n"
    "texldl r3, v0, s0\n texkill r3\n mov oC0, r0\n mov oDepth, r3.x\n";

/* ---- HLSL corpus ---- */
static const char hlsl_src[] =
    "float4x4 wvp; float4 lights[8]; int nl; bool fog;\n"
    "sampler2D tex0; samplerCUBE env; sampler2D shadow;\n"
    "struct VO { float4 p : POSITION; float2 uv : TEXCOORD0; float3 n : TEXCOORD1; float4 c : COLOR0; float f : FOG; };\n"
    "VO vs(float4 p : POSITION, float3 n : NORMAL, float2 uv : TEXCOORD0, float4 bw : BLENDWEIGHT, float4 bi : BLENDINDICES) {\n"
    "  VO o; o.p = mul(p, wvp); o.uv = uv; o.n = normalize(n); o.c = 0;\n"
    "  for (int i = 0; i < nl; i++) o.c += max(0, dot(n, lights[i].xyz)) * lights[(int)bi.x + i];\n"
    "  o.f = fog ? o.p.z * 0.01 : 1; return o; }\n"
    "float4 ps(float2 uv : TEXCOORD0, float3 n : TEXCOORD1, float4 c : COLOR0) : COLOR0 {\n"
    "  float4 t = tex2D(tex0, uv); clip(t.a - 0.5);\n"
    "  float4 e = texCUBE(env, reflect(float3(0,0,1), n));\n"
    "  float s = 0; for (int i = 0; i < 4; i++) s += tex2Dlod(shadow, float4(uv + i * 0.01, 0, 0)).r;\n"
    "  return t * c + e * s * 0.25 + frac(uv.x) + log2(abs(uv.y) + 1) + sin(uv.x) * cos(uv.y); }\n"
    "float4 ps2(float2 uv : TEXCOORD0, float4 c : COLOR0) : COLOR0 { float4 t = tex2D(tex0, uv); return lerp(t, c, t.a) * pow(c.r, 2.2); }\n"
    "float4 vs1(float4 p : POSITION, float2 uv : TEXCOORD0, out float2 ouv : TEXCOORD0) : POSITION { ouv = uv * 2; return mul(p, wvp); }\n";

static void ff_vs(const char *name, const struct ff_vs_key *k)
{
    uint32_t n = 0, *code = ff_vs_generate(k, &n);
    if (!code) { printf("FAIL generate %s\n", name); failures++; total++; return; }
    translate(name, code, n, &k0);
    vkp_free(code);
}

static void ff_ps(const char *name, const struct ff_ps_key *k, const struct dxso_key *key)
{
    uint32_t n = 0, *code = ff_ps_generate(k, &n);
    if (!code) { printf("FAIL generate %s\n", name); failures++; total++; return; }
    translate(name, code, n, key);
    vkp_free(code);
}

static void ff_cases(void)
{
    struct ff_vs_key v = {0};
    v.has_color[0] = 1; v.ntex = 1; v.in_dim[0] = 2;
    ff_vs("ff_vs_unlit", &v);
    v.has_normal = 1; v.lighting = 1; v.normalize = 1; v.specular = 1; v.color_vertex = 1;
    v.mat_src[0] = 1; v.mat_src[1] = 1; v.mat_src[3] = 0;
    v.light[0] = FF_LIGHT_DIRECTIONAL_T; v.light[1] = FF_LIGHT_POINT_T; v.light[2] = FF_LIGHT_SPOT_T;
    v.fog = FF_FOG_LINEAR; v.range_fog = 1;
    ff_vs("ff_vs_lit", &v);
    v.local_viewer = 1; v.fog = FF_FOG_EXP2; v.blend = 2; v.has_color[1] = 1;
    v.ntex = 4; v.in_dim[1] = 3;
    v.tc_index[1] = 1; v.tc_count[1] = 3; v.tc_proj[1] = 1;
    v.tc_gen[2] = 3; v.tc_count[2] = 3;
    v.tc_gen[3] = 4;
    v.point_scale = 1;
    ff_vs("ff_vs_full", &v);
    struct ff_vs_key r = {0};
    r.rhw = 1; r.has_color[0] = 1; r.has_color[1] = 1; r.fog = FF_FOG_SPECULAR_ALPHA; r.ntex = 2;
    r.in_dim[0] = 2; r.in_dim[1] = 2; r.tc_index[1] = 1; r.tc_count[1] = 2;
    ff_vs("ff_vs_rhw", &r);

    /* D3DTOP and D3DTA values */
    struct ff_ps_key p = {0};
    p.st[0] = (struct ff_stage){4, {1, 2, 0}, 2, {1, 2, 0}, 0, 1, 0, 0};          /* modulate tex, diffuse; select tex */
    p.st[1] = (struct ff_stage){1, {0}, 1, {0}, 0, 0, 0, 0};
    ff_ps("ff_ps_modulate", &p, &k0);
    for (int op = 2; op <= 26; op++) {
        if (op == 22 || op == 23) continue;
        struct ff_ps_key q = {0};
        q.st[0] = (struct ff_stage){(uint8_t)op, {3, 2, 0}, (uint8_t)(op == 24 ? 1 : op),
                                    {3 | 0x10, 2 | 0x20, 1}, (uint8_t)(op & 1), 1, 0, 0};
        q.st[1] = (struct ff_stage){4, {1, 1 | 0x10, 0}, 2, {0, 1, 0}, 0, 0, 0, 0}; /* unbound texture */
        q.st[2] = (struct ff_stage){10, {5, 4, 0}, 7, {6, 2, 0}, 0, 2, 1, 0};
        q.st[3] = (struct ff_stage){1, {0}, 1, {0}, 0, 0, 0, 0};
        q.specular = (uint8_t)(op & 1);
        char name[32];
        snprintf(name, sizeof name, "ff_ps_op%d", op);
        ff_ps(name, &q, &k0);
    }
    struct ff_ps_key b = {0};
    b.st[0] = (struct ff_stage){2, {1, 0, 0}, 2, {0, 0, 0}, 0, 1, 0, 0};
    b.st[1] = (struct ff_stage){22, {2, 1, 0}, 2, {1, 1, 0}, 0, 1, 0, 0};          /* bump env map */
    b.st[2] = (struct ff_stage){4, {2, 1, 0}, 2, {1, 1, 0}, 0, 1, 1, 0};
    b.st[3] = (struct ff_stage){23, {2, 1, 0}, 2, {1, 1, 0}, 0, 1, 0, 0};          /* luminance */
    b.st[4] = (struct ff_stage){4, {2, 1, 0}, 4, {2, 1, 0}, 0, 2, 0, 0};
    b.st[5] = (struct ff_stage){1, {0}, 1, {0}, 0, 0, 0, 0};
    struct dxso_key kf = k0;
    ff_ps("ff_ps_bump", &b, &kf);
}

int main(void)
{
    CreateDirectoryA("out", 0);
    HMODULE x = LoadLibraryA("d3dx9_43.dll");
    if (!x) { printf("d3dx9_43.dll missing\n"); return 2; }
    assemble = (assemble_fn)GetProcAddress(x, "D3DXAssembleShader");
    compile = (compile_fn)GetProcAddress(x, "D3DXCompileShader");

    asm_case("vs11", vs11, &k0);
    asm_case("vs20", vs20, &k0);
    struct dxso_key kvs = k0;
    asm_case("vs30", vs30, &kvs);
    asm_case("ps11", ps11, &k0);
    struct dxso_key kp = k0;
    kp.proj[0] = 4; kp.tex[3] = DXSO_TEX_CUBE; kp.flat = 1;
    asm_case("ps11_proj", ps11, &kp);
    struct dxso_key kc = k0;
    kc.tex[3] = DXSO_TEX_CUBE;
    asm_case("ps11b", ps11b, &kc);
    asm_case("ps12", ps12, &k0);
    asm_case("ps13", ps13, &k0);
    asm_case("ps13b", ps13b, &kc);
    asm_case("ps13c", ps13c, &k0);
    struct dxso_key ks = k0;
    ks.point_sprite = 1;
    asm_case("ps13c_sprite", ps13c, &ks);
    asm_case("ps14", ps14, &k0);
    struct dxso_key kd = k0;
    kd.tex[3] = DXSO_TEX_2D | DXSO_TEX_COMPARE;
    asm_case("ps14_cmp", ps14, &kd);
    asm_case("ps20", ps20, &k0);
    struct dxso_key kd0 = k0;
    kd0.tex[0] = DXSO_TEX_2D | DXSO_TEX_COMPARE;
    asm_case("ps20_cmp", ps20, &kd0);
    asm_case("ps2x", ps2x, &kd0);
    asm_case("ps30", ps30, &k0);
    asm_case("ps30_cmp", ps30, &kd0);

    hlsl_case("hlsl_vs3", hlsl_src, "vs", "vs_3_0", &k0);
    hlsl_case("hlsl_vs2", hlsl_src, "vs", "vs_2_0", &k0);
    hlsl_case("hlsl_ps3", hlsl_src, "ps", "ps_3_0", &k0);
    hlsl_case("hlsl_ps2", hlsl_src, "ps2", "ps_2_0", &k0);
    hlsl_case("hlsl_vs1", hlsl_src, "vs1", "vs_1_1", &k0);

    ff_cases();

#define PRE(n) translate(#n, n, (uint32_t)(sizeof n / 4), &k0)
    PRE(world_post_aa); PRE(world_post_adapt); PRE(world_post_bloom_down); PRE(world_post_bloom_up);
    PRE(world_post_composite); PRE(world_post_depth_lin); PRE(world_post_finish); PRE(world_post_post_vs);
    PRE(world_post_ssao);

    printf("%d shaders, %d failed\n", total, failures);
    return failures != 0;
}
