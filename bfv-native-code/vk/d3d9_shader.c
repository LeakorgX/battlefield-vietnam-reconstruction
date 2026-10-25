/* Vertex declarations, shaders, queries, state blocks and swap chains. */
#include "vkd3d9.h"

/* ---- references held by device state and state blocks ---- */

void vkd_ref_tex(struct vkd_res **slot, struct vkd_res *r)
{
    if (*slot == r) return;
    vkd_res_bind(r);
    struct vkd_res *old = *slot;
    *slot = r;
    vkd_res_unbind(old);
}

void vkd_ref_buf(struct vkd_buffer **slot, struct vkd_buffer *b)
{
    if (*slot == b) return;
    if (b) vkd_res_bind(&b->r);
    struct vkd_buffer *old = *slot;
    *slot = b;
    if (old) vkd_res_unbind(&old->r);
}

static void vdecl_maybe_destroy(struct vkd_vdecl *v);
static void shader_maybe_destroy(struct vkd_shader *s);

void vkd_ref_decl(struct vkd_vdecl **slot, struct vkd_vdecl *v)
{
    if (*slot == v) return;
    if (v) v->bind++;
    struct vkd_vdecl *old = *slot;
    *slot = v;
    if (old && --old->bind <= 0) { old->bind = 0; vdecl_maybe_destroy(old); }
}

void vkd_ref_shader(struct vkd_shader **slot, struct vkd_shader *s)
{
    if (*slot == s) return;
    if (s) s->bind++;
    struct vkd_shader *old = *slot;
    *slot = s;
    if (old && --old->bind <= 0) { old->bind = 0; shader_maybe_destroy(old); }
}

void vkd_shader_addref_bind(struct vkd_shader *s) { if (s) s->bind++; }

void vkd_shader_unbind(struct vkd_shader *s)
{
    if (s && --s->bind <= 0) { s->bind = 0; shader_maybe_destroy(s); }
}

/* ---- vertex declarations ---- */

static const uint8_t decl_size[17] = {4, 8, 12, 16, 4, 4, 4, 8, 4, 4, 8, 4, 8, 4, 4, 4, 8};
static const uint8_t decl_count[17] = {1, 2, 3, 4, 4, 4, 2, 4, 4, 2, 4, 2, 4, 3, 3, 2, 4};

static void decl_analyze(struct vkd_vdecl *v)
{
    v->has_positiont = v->has_normal = v->has_psize = v->blend = 0;
    v->has_color[0] = v->has_color[1] = 0;
    memset(v->tex_dim, 0, sizeof v->tex_dim);
    for (UINT i = 0; i < v->n; i++) {
        const D3DVERTEXELEMENT9 *e = &v->el[i];
        int count = e->Type < 17 ? decl_count[e->Type] : 0;
        switch (e->Usage) {
        case D3DDECLUSAGE_POSITIONT: v->has_positiont = 1; break;
        case D3DDECLUSAGE_NORMAL: if (!e->UsageIndex) v->has_normal = 1; break;
        case D3DDECLUSAGE_COLOR: if (e->UsageIndex < 2) v->has_color[e->UsageIndex] = 1; break;
        case D3DDECLUSAGE_PSIZE: if (!e->UsageIndex) v->has_psize = 1; break;
        case D3DDECLUSAGE_BLENDWEIGHT: if (!e->UsageIndex) v->blend = count; break;
        case D3DDECLUSAGE_TEXCOORD: if (e->UsageIndex < 8) v->tex_dim[e->UsageIndex] = count; break;
        }
    }
}

static void add_el(D3DVERTEXELEMENT9 *el, UINT *n, WORD *off, BYTE type, BYTE usage, BYTE index)
{
    D3DVERTEXELEMENT9 *e = &el[(*n)++];
    e->Stream = 0;
    e->Offset = *off;
    e->Type = type;
    e->Method = D3DDECLMETHOD_DEFAULT;
    e->Usage = usage;
    e->UsageIndex = index;
    *off = (WORD)(*off + decl_size[type]);
}

/* elements of an FVF code (without the end marker); count */
static UINT fvf_elements(DWORD fvf, D3DVERTEXELEMENT9 *el)
{
    UINT n = 0;
    WORD off = 0;
    DWORD pos = fvf & D3DFVF_POSITION_MASK;
    int betas = 0;
    switch (pos) {
    case D3DFVF_XYZ: add_el(el, &n, &off, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0); break;
    case D3DFVF_XYZRHW: add_el(el, &n, &off, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITIONT, 0); break;
    case D3DFVF_XYZW: add_el(el, &n, &off, D3DDECLTYPE_FLOAT4, D3DDECLUSAGE_POSITION, 0); break;
    case D3DFVF_XYZB1: case D3DFVF_XYZB2: case D3DFVF_XYZB3: case D3DFVF_XYZB4: case D3DFVF_XYZB5:
        add_el(el, &n, &off, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_POSITION, 0);
        betas = (int)((pos - D3DFVF_XYZB1) >> 1) + 1;
        break;
    }
    if (betas) {
        int idx = (fvf & (D3DFVF_LASTBETA_UBYTE4 | D3DFVF_LASTBETA_D3DCOLOR)) ? 1 : 0;
        int weights = betas - idx;
        if (weights > 0) add_el(el, &n, &off, (BYTE)(D3DDECLTYPE_FLOAT1 + weights - 1), D3DDECLUSAGE_BLENDWEIGHT, 0);
        if (idx)
            add_el(el, &n, &off, (fvf & D3DFVF_LASTBETA_UBYTE4) ? D3DDECLTYPE_UBYTE4 : D3DDECLTYPE_D3DCOLOR,
                   D3DDECLUSAGE_BLENDINDICES, 0);
    }
    if (fvf & D3DFVF_NORMAL) add_el(el, &n, &off, D3DDECLTYPE_FLOAT3, D3DDECLUSAGE_NORMAL, 0);
    if (fvf & D3DFVF_PSIZE) add_el(el, &n, &off, D3DDECLTYPE_FLOAT1, D3DDECLUSAGE_PSIZE, 0);
    if (fvf & D3DFVF_DIFFUSE) add_el(el, &n, &off, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 0);
    if (fvf & D3DFVF_SPECULAR) add_el(el, &n, &off, D3DDECLTYPE_D3DCOLOR, D3DDECLUSAGE_COLOR, 1);
    UINT ntex = (fvf & D3DFVF_TEXCOUNT_MASK) >> D3DFVF_TEXCOUNT_SHIFT;
    if (ntex > 8) ntex = 8;
    for (UINT i = 0; i < ntex; i++) {
        static const BYTE types[4] = {D3DDECLTYPE_FLOAT2, D3DDECLTYPE_FLOAT3, D3DDECLTYPE_FLOAT4, D3DDECLTYPE_FLOAT1};
        add_el(el, &n, &off, types[(fvf >> (16 + i * 2)) & 3], D3DDECLUSAGE_TEXCOORD, (BYTE)i);
    }
    return n;
}

/* the FVF a declaration corresponds to, or 0 */
static DWORD decl_to_fvf(const struct vkd_vdecl *v)
{
    DWORD fvf = 0;
    int weights = 0, idx = 0;
    DWORD idxflag = 0;
    UINT ntex = 0;
    DWORD texfmt = 0;
    for (UINT i = 0; i < v->n; i++) {
        const D3DVERTEXELEMENT9 *e = &v->el[i];
        switch (e->Usage) {
        case D3DDECLUSAGE_POSITION:
            if (e->UsageIndex) return 0;
            fvf |= e->Type == D3DDECLTYPE_FLOAT4 ? D3DFVF_XYZW : D3DFVF_XYZ;
            break;
        case D3DDECLUSAGE_POSITIONT: fvf |= D3DFVF_XYZRHW; break;
        case D3DDECLUSAGE_BLENDWEIGHT: weights = e->Type < 4 ? e->Type + 1 : 0; break;
        case D3DDECLUSAGE_BLENDINDICES:
            idx = 1;
            idxflag = e->Type == D3DDECLTYPE_UBYTE4 ? D3DFVF_LASTBETA_UBYTE4 : D3DFVF_LASTBETA_D3DCOLOR;
            break;
        case D3DDECLUSAGE_NORMAL: fvf |= D3DFVF_NORMAL; break;
        case D3DDECLUSAGE_PSIZE: fvf |= D3DFVF_PSIZE; break;
        case D3DDECLUSAGE_COLOR: fvf |= e->UsageIndex ? D3DFVF_SPECULAR : D3DFVF_DIFFUSE; break;
        case D3DDECLUSAGE_TEXCOORD: {
            if (e->UsageIndex >= 8) return 0;
            static const DWORD f[4] = {3, 0, 1, 2};   /* FLOAT1..4 -> D3DFVF_TEXTUREFORMAT */
            if (e->Type > D3DDECLTYPE_FLOAT4) return 0;
            texfmt |= f[e->Type] << (16 + e->UsageIndex * 2);
            if (e->UsageIndex + 1u > ntex) ntex = e->UsageIndex + 1u;
            break;
        }
        default: return 0;
        }
    }
    int betas = weights + idx;
    if (betas) {
        if ((fvf & D3DFVF_POSITION_MASK) != D3DFVF_XYZ) return 0;
        fvf = (fvf & ~(DWORD)D3DFVF_POSITION_MASK) | (D3DFVF_XYZB1 + (DWORD)(betas - 1) * 2) | idxflag;
    }
    fvf |= texfmt | ntex << D3DFVF_TEXCOUNT_SHIFT;
    /* exact only if the FVF reproduces the declaration */
    D3DVERTEXELEMENT9 el[MAXD3DDECLLENGTH];
    UINT n = fvf_elements(fvf, el);
    if (n != v->n) return 0;
    for (UINT i = 0; i < n; i++)
        if (el[i].Stream != v->el[i].Stream || el[i].Offset != v->el[i].Offset || el[i].Type != v->el[i].Type ||
            el[i].Usage != v->el[i].Usage || el[i].UsageIndex != v->el[i].UsageIndex)
            return 0;
    return fvf;
}

static void vdecl_destroy(struct vkd_vdecl *v)
{
    vkp_free(v);
}

static void vdecl_maybe_destroy(struct vkd_vdecl *v)
{
    if (!v->ref && !v->bind && !v->cached) vdecl_destroy(v);
}

void vkd_vdecl_free(struct vkd_vdecl *v) { vkp_free(v); }

HRESULT vkd_create_vdecl(struct vkd_device *d, const D3DVERTEXELEMENT9 *el, struct vkd_vdecl **out)
{
    *out = 0;
    if (!el) return D3DERR_INVALIDCALL;
    UINT n = 0;
    while (el[n].Stream != 0xff && n < MAXD3DDECLLENGTH) {
        if (el[n].Type >= D3DDECLTYPE_UNUSED || el[n].Stream >= 16) return D3DERR_INVALIDCALL;
        n++;
    }
    if (el[n].Stream != 0xff) return D3DERR_INVALIDCALL;
    struct vkd_vdecl *v = vkp_alloc(sizeof *v);
    if (!v) return E_OUTOFMEMORY;
    v->vtbl = &vkd_vdecl_vtbl;
    v->ref = 1;
    v->dev = d;
    memcpy(v->el, el, (n + 1) * sizeof *el);
    v->n = n;
    decl_analyze(v);
    v->fvf = decl_to_fvf(v);
    *out = v;
    return D3D_OK;
}

/* cached declaration of an FVF code (owned by the device) */
struct vkd_vdecl *vkd_fvf_decl(struct vkd_device *d, DWORD fvf)
{
    struct vkd_vdecl *v = vkd_map_get(&d->fvf_decls, &fvf, sizeof fvf);
    if (v) return v;
    D3DVERTEXELEMENT9 el[MAXD3DDECLLENGTH + 1];
    UINT n = fvf_elements(fvf, el);
    static const D3DVERTEXELEMENT9 end = D3DDECL_END();
    el[n] = end;
    if (FAILED(vkd_create_vdecl(d, el, &v))) return 0;
    v->fvf = fvf;
    v->ref = 0;
    v->cached = 1;
    vkd_map_put(&d->fvf_decls, &fvf, sizeof fvf, v);
    if (vkd_map_get(&d->fvf_decls, &fvf, sizeof fvf) != v) { vkp_free(v); return 0; }
    return v;
}

static HRESULT STDMETHODCALLTYPE vdecl_QueryInterface(IDirect3DVertexDeclaration9 *self, REFIID riid, void **out)
{
    if (!out) return E_POINTER;
    if (vkd_guid_eq(riid, &IID_IUnknown) || vkd_guid_eq(riid, &IID_IDirect3DVertexDeclaration9)) {
        *out = self;
        IDirect3DVertexDeclaration9_AddRef(self);
        return S_OK;
    }
    *out = 0;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE vdecl_AddRef(IDirect3DVertexDeclaration9 *self)
{
    struct vkd_vdecl *v = (struct vkd_vdecl *)self;
    LONG n = InterlockedIncrement(&v->ref);
    if (n == 1) IDirect3DDevice9_AddRef((IDirect3DDevice9 *)v->dev);
    return (ULONG)n;
}
static ULONG STDMETHODCALLTYPE vdecl_Release(IDirect3DVertexDeclaration9 *self)
{
    struct vkd_vdecl *v = (struct vkd_vdecl *)self;
    LONG n = InterlockedDecrement(&v->ref);
    if (n < 0) { v->ref = 0; n = 0; }
    if (!n) {
        struct vkd_device *d = v->dev;
        VKD_LOCK(d);
        vdecl_maybe_destroy(v);
        VKD_UNLOCK(d);
        IDirect3DDevice9_Release((IDirect3DDevice9 *)d);
    }
    return (ULONG)n;
}
static HRESULT STDMETHODCALLTYPE vdecl_GetDevice(IDirect3DVertexDeclaration9 *self, IDirect3DDevice9 **out)
{
    if (!out) return D3DERR_INVALIDCALL;
    *out = (IDirect3DDevice9 *)((struct vkd_vdecl *)self)->dev;
    IDirect3DDevice9_AddRef(*out);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE vdecl_GetDeclaration(IDirect3DVertexDeclaration9 *self, D3DVERTEXELEMENT9 *el, UINT *num)
{
    struct vkd_vdecl *v = (struct vkd_vdecl *)self;
    if (!num) return D3DERR_INVALIDCALL;
    if (el) memcpy(el, v->el, (v->n + 1) * sizeof *el);
    *num = v->n + 1;
    return D3D_OK;
}

const IDirect3DVertexDeclaration9Vtbl vkd_vdecl_vtbl = {
    .QueryInterface = vdecl_QueryInterface,
    .AddRef = vdecl_AddRef,
    .Release = vdecl_Release,
    .GetDevice = vdecl_GetDevice,
    .GetDeclaration = vdecl_GetDeclaration,
};

/* ---- shaders ---- */

static void shader_drop_variants(struct vkd_shader *s)
{
    struct vkd_device *d = s->dev;
    for (uint32_t i = 0; i < s->nvar; i++) {
        vkd_pipelines_forget(d, s->var[i].id);
        struct vkd_garbage g = {0};
        g.kind = G_MODULE;
        g.h.mod = s->var[i].mod;
        if (g.h.mod) vkd_defer(d, &g);
    }
    vkp_free(s->var);
    s->var = 0;
    s->nvar = 0;
}

void vkd_shader_free(struct vkd_shader *s)
{
    if (!s) return;
    shader_drop_variants(s);
    vkp_free(s->code);
    vkp_free(s);
}

static void shader_maybe_destroy(struct vkd_shader *s)
{
    if (s->ref || s->bind) return;
    vkd_shader_free(s);
}

HRESULT vkd_create_shader(struct vkd_device *d, const DWORD *code, int ps, struct vkd_shader **out)
{
    *out = 0;
    if (!code) return D3DERR_INVALIDCALL;
    struct vkd_shader *s = vkp_alloc(sizeof *s);
    if (!s) return E_OUTOFMEMORY;
    if (!dxso_scan((const uint32_t *)code, 1u << 20, &s->info) || !s->info.ps != !ps) {
        vkp_log("vulkan: rejected %s shader %08x", ps ? "pixel" : "vertex", (unsigned)code[0]);
        vkp_free(s);
        return D3DERR_INVALIDCALL;
    }
    s->code = vkp_alloc(s->info.words * 4);
    if (!s->code) { vkp_free(s); return E_OUTOFMEMORY; }
    memcpy(s->code, code, s->info.words * 4);
    s->vtbl = ps ? (const void *)&vkd_ps_vtbl : (const void *)&vkd_vs_vtbl;
    s->ref = 1;
    s->dev = d;
    *out = s;
    return D3D_OK;
}

VkShaderModule vkd_shader_module(struct vkd_shader *s, const struct dxso_key *key)
{
    for (uint32_t i = 0; i < s->nvar; i++)
        if (!memcmp(&s->var[i].key, key, sizeof *key)) return s->var[i].mod;
    uint32_t words = 0;
    uint32_t *spv = dxso_compile(s->code, &s->info, key, &words);
    VkShaderModule mod = spv ? vkd_module(spv, words) : 0;
    vkp_free(spv);
    if (!mod) {
        vkp_log("vulkan: %s shader %08x failed to translate", s->info.ps ? "pixel" : "vertex", (unsigned)s->info.version);
        return 0;
    }
    struct vkd_variant *v = vkp_realloc(s->var, (s->nvar + 1) * sizeof *v);
    if (!v) {
        struct vkd_garbage g = {0};
        g.kind = G_MODULE; g.h.mod = mod;
        vkd_defer(s->dev, &g);
        return 0;
    }
    s->var = v;
    v[s->nvar].key = *key;
    v[s->nvar].mod = mod;
    v[s->nvar].id = ++s->dev->next_variant_id;
    s->nvar++;
    return mod;
}

static ULONG shader_addref(struct vkd_shader *s)
{
    LONG n = InterlockedIncrement(&s->ref);
    if (n == 1) IDirect3DDevice9_AddRef((IDirect3DDevice9 *)s->dev);
    return (ULONG)n;
}

#define SHADER_METHODS(pfx, I, IID)                                                                                \
    static HRESULT STDMETHODCALLTYPE pfx##_QueryInterface(I *self, REFIID riid, void **out)                        \
    {                                                                                                              \
        if (!out) return E_POINTER;                                                                                \
        if (vkd_guid_eq(riid, &IID_IUnknown) || vkd_guid_eq(riid, &IID)) {                                         \
            *out = self;                                                                                           \
            shader_addref((struct vkd_shader *)self);                                                              \
            return S_OK;                                                                                           \
        }                                                                                                          \
        *out = 0;                                                                                                  \
        return E_NOINTERFACE;                                                                                      \
    }                                                                                                              \
    static ULONG STDMETHODCALLTYPE pfx##_AddRef(I *self)                                                           \
    {                                                                                                              \
        return shader_addref((struct vkd_shader *)self);                                                           \
    }                                                                                                              \
    static ULONG STDMETHODCALLTYPE pfx##_Release(I *self)                                                          \
    {                                                                                                              \
        struct vkd_shader *s = (struct vkd_shader *)self;                                                          \
        LONG n = InterlockedDecrement(&s->ref);                                                                    \
        if (n < 0) { s->ref = 0; n = 0; }                                                                          \
        if (!n) {                                                                                                  \
            struct vkd_device *d = s->dev;                                                                         \
            VKD_LOCK(d);                                                                                           \
            shader_maybe_destroy(s);                                                                               \
            VKD_UNLOCK(d);                                                                                         \
            IDirect3DDevice9_Release((IDirect3DDevice9 *)d);                                                         \
        }                                                                                                          \
        return (ULONG)n;                                                                                           \
    }                                                                                                              \
    static HRESULT STDMETHODCALLTYPE pfx##_GetDevice(I *self, IDirect3DDevice9 **out)                              \
    {                                                                                                              \
        if (!out) return D3DERR_INVALIDCALL;                                                                       \
        *out = (IDirect3DDevice9 *)((struct vkd_shader *)self)->dev;                                               \
        IDirect3DDevice9_AddRef(*out);                                                                             \
        return D3D_OK;                                                                                             \
    }                                                                                                              \
    static HRESULT STDMETHODCALLTYPE pfx##_GetFunction(I *self, void *data, UINT *size)                            \
    {                                                                                                              \
        struct vkd_shader *s = (struct vkd_shader *)self;                                                          \
        if (!size) return D3DERR_INVALIDCALL;                                                                      \
        if (!data) { *size = s->info.words * 4; return D3D_OK; }                                                   \
        if (*size < s->info.words * 4) return D3DERR_INVALIDCALL;                                                  \
        memcpy(data, s->code, s->info.words * 4);                                                                  \
        *size = s->info.words * 4;                                                                                 \
        return D3D_OK;                                                                                             \
    }

SHADER_METHODS(vs, IDirect3DVertexShader9, IID_IDirect3DVertexShader9)
SHADER_METHODS(ps, IDirect3DPixelShader9, IID_IDirect3DPixelShader9)

const IDirect3DVertexShader9Vtbl vkd_vs_vtbl = {
    .QueryInterface = vs_QueryInterface,
    .AddRef = vs_AddRef,
    .Release = vs_Release,
    .GetDevice = vs_GetDevice,
    .GetFunction = vs_GetFunction,
};
const IDirect3DPixelShader9Vtbl vkd_ps_vtbl = {
    .QueryInterface = ps_QueryInterface,
    .AddRef = ps_AddRef,
    .Release = ps_Release,
    .GetDevice = ps_GetDevice,
    .GetFunction = ps_GetFunction,
};

/* ---- queries ----
 *
 * Vulkan allows one occlusion query at a time, inside one rendering pass.
 * D3D9 queries overlap and span passes and submissions, so the device runs
 * a single Vulkan query while any D3D9 query is active and starts a new one
 * whenever the active set or the pass changes. Every active D3D9 query
 * records the slot; the counts are summed when the submission retires. */

static int occ_slot(struct vkd_device *d, VkQueryPool *pool, uint32_t *index)
{
    struct vkd_context *c = &d->ctx[d->cur];
    uint32_t p = c->qused / VKD_QUERY_POOL;
    if (p == c->nqpools) {
        VkQueryPool *pools = vkp_realloc(c->qpools, (p + 1) * sizeof *pools);
        if (!pools) return 0;
        c->qpools = pools;
        VkQueryPoolCreateInfo ci = {VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};
        ci.queryType = VK_QUERY_TYPE_OCCLUSION;
        ci.queryCount = VKD_QUERY_POOL;
        if (vkCreateQueryPool(vkd.dev, &ci, 0, &pools[p]) != VK_SUCCESS) return 0;
        c->nqpools++;
    }
    if (c->qused % VKD_QUERY_POOL == 0) vkCmdResetQueryPool(vkd_init_cmd(d), c->qpools[p], 0, VKD_QUERY_POOL);
    *pool = c->qpools[p];
    *index = c->qused % VKD_QUERY_POOL;
    c->qused++;
    return 1;
}

static void occ_end(struct vkd_device *d)
{
    if (!d->occ_open) return;
    vkCmdEndQuery(vkd_cmd(d), d->occ_pool, d->occ_slot);
    d->occ_open = 0;
}

/* end the running Vulkan query and start one for the current active set */
static void occ_restart(struct vkd_device *d)
{
    occ_end(d);
    if (!d->rendering || !d->nactive_queries) return;
    VkQueryPool pool;
    uint32_t index;
    if (!occ_slot(d, &pool, &index)) return;
    struct vkd_context *c = &d->ctx[d->cur];
    if (c->nqrefs + d->nactive_queries > c->capqrefs) {
        uint32_t cap = VKP_MAX(c->capqrefs * 2, c->nqrefs + d->nactive_queries + 64);
        struct vkd_qref *r = vkp_realloc(c->qrefs, cap * sizeof *r);
        if (!r) return;
        c->qrefs = r; c->capqrefs = cap;
    }
    uint32_t global = c->qused - 1;
    for (uint32_t i = 0; i < d->nactive_queries; i++) {
        struct vkd_query *q = d->active_queries[i];
        c->qrefs[c->nqrefs].q = q;
        c->qrefs[c->nqrefs].slot = global;
        c->qrefs[c->nqrefs].gen = q->gen;
        c->nqrefs++;
        q->pending++;
    }
    vkCmdBeginQuery(vkd_cmd(d), pool, index, vkd.occ_precise ? VK_QUERY_CONTROL_PRECISE_BIT : 0);
    d->occ_pool = pool;
    d->occ_slot = index;
    d->occ_open = 1;
}

void vkd_queries_pass_begin(struct vkd_device *d) { occ_restart(d); }
void vkd_queries_pass_end(struct vkd_device *d) { occ_end(d); }

static void query_free(struct vkd_query *q) { vkp_free(q); }

void vkd_queries_retire(struct vkd_device *d, struct vkd_context *c)
{
    uint64_t res[VKD_QUERY_POOL];
    for (uint32_t p = 0; p * VKD_QUERY_POOL < c->qused; p++) {
        uint32_t n = VKP_MIN(c->qused - p * VKD_QUERY_POOL, (uint32_t)VKD_QUERY_POOL);
        int have = c->submitted &&
                   vkGetQueryPoolResults(vkd.dev, c->qpools[p], 0, n, sizeof res, res, 8, VK_QUERY_RESULT_64_BIT) == VK_SUCCESS;
        for (uint32_t i = 0; i < c->nqrefs; i++) {
            struct vkd_qref *r = &c->qrefs[i];
            if (r->slot / VKD_QUERY_POOL != p) continue;
            if (have && r->gen == r->q->gen) r->q->result += res[r->slot % VKD_QUERY_POOL];
        }
    }
    for (uint32_t i = 0; i < c->nqrefs; i++) {
        struct vkd_query *q = c->qrefs[i].q;
        if (!--q->pending && q->dead) query_free(q);
    }
    c->nqrefs = 0;
    c->qused = 0;
    (void)d;
}

void vkd_queries_free(struct vkd_device *d, struct vkd_context *c)
{
    for (uint32_t i = 0; i < c->nqpools; i++) vkDestroyQueryPool(vkd.dev, c->qpools[i], 0);
    vkp_free(c->qpools);
    vkp_free(c->qrefs);
    c->qpools = 0; c->qrefs = 0;
    c->nqpools = c->nqrefs = c->capqrefs = 0;
    (void)d;
}

static void active_remove(struct vkd_device *d, struct vkd_query *q)
{
    for (uint32_t i = 0; i < d->nactive_queries; i++)
        if (d->active_queries[i] == q) {
            d->active_queries[i] = d->active_queries[--d->nactive_queries];
            return;
        }
}

HRESULT vkd_create_query(struct vkd_device *d, D3DQUERYTYPE type, struct vkd_query **out)
{
    if (type != D3DQUERYTYPE_EVENT && type != D3DQUERYTYPE_OCCLUSION) return D3DERR_NOTAVAILABLE;
    if (!out) return D3D_OK;
    *out = 0;
    struct vkd_query *q = vkp_alloc(sizeof *q);
    if (!q) return E_OUTOFMEMORY;
    q->vtbl = &vkd_query_vtbl;
    q->ref = 1;
    q->dev = d;
    q->type = type;
    *out = q;
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE query_QueryInterface(IDirect3DQuery9 *self, REFIID riid, void **out)
{
    if (!out) return E_POINTER;
    if (vkd_guid_eq(riid, &IID_IUnknown) || vkd_guid_eq(riid, &IID_IDirect3DQuery9)) {
        *out = self;
        IDirect3DQuery9_AddRef(self);
        return S_OK;
    }
    *out = 0;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE query_AddRef(IDirect3DQuery9 *self)
{
    return (ULONG)InterlockedIncrement(&((struct vkd_query *)self)->ref);
}
static ULONG STDMETHODCALLTYPE query_Release(IDirect3DQuery9 *self)
{
    struct vkd_query *q = (struct vkd_query *)self;
    LONG n = InterlockedDecrement(&q->ref);
    if (n < 0) { q->ref = 0; n = 0; }
    if (!n) {
        struct vkd_device *d = q->dev;
        VKD_LOCK(d);
        if (q->state == 1) {
            active_remove(d, q);
            if (d->occ_open) occ_restart(d);
        }
        if (q->pending) q->dead = 1;
        else query_free(q);
        VKD_UNLOCK(d);
        IDirect3DDevice9_Release((IDirect3DDevice9 *)d);
    }
    return (ULONG)n;
}
static HRESULT STDMETHODCALLTYPE query_GetDevice(IDirect3DQuery9 *self, IDirect3DDevice9 **out)
{
    if (!out) return D3DERR_INVALIDCALL;
    *out = (IDirect3DDevice9 *)((struct vkd_query *)self)->dev;
    IDirect3DDevice9_AddRef(*out);
    return D3D_OK;
}
static D3DQUERYTYPE STDMETHODCALLTYPE query_GetType(IDirect3DQuery9 *self) { return ((struct vkd_query *)self)->type; }
static DWORD STDMETHODCALLTYPE query_GetDataSize(IDirect3DQuery9 *self)
{
    return ((struct vkd_query *)self)->type == D3DQUERYTYPE_EVENT ? sizeof(BOOL) : sizeof(DWORD);
}
static HRESULT STDMETHODCALLTYPE query_Issue(IDirect3DQuery9 *self, DWORD flags)
{
    struct vkd_query *q = (struct vkd_query *)self;
    struct vkd_device *d = q->dev;
    VKD_LOCK(d);
    if (q->type == D3DQUERYTYPE_EVENT) {
        if (flags & D3DISSUE_END) { q->seq = d->seq; q->state = 2; }
    } else if (flags & D3DISSUE_BEGIN) {
        if (q->state != 1) {
            if (d->nactive_queries == d->capactive_queries) {
                uint32_t cap = d->capactive_queries ? d->capactive_queries * 2 : 16;
                struct vkd_query **a = vkp_realloc(d->active_queries, cap * sizeof *a);
                if (!a) { VKD_UNLOCK(d); return E_OUTOFMEMORY; }
                d->active_queries = a; d->capactive_queries = cap;
            }
            d->active_queries[d->nactive_queries++] = q;
        }
        q->gen++;
        q->result = 0;
        q->state = 1;
        occ_restart(d);
    } else if (flags & D3DISSUE_END) {
        if (q->state == 1) {
            active_remove(d, q);
            occ_restart(d);
        } else {
            /* END without BEGIN: an empty query */
            q->gen++;
            q->result = 0;
        }
        q->seq = d->seq;
        q->state = 2;
    }
    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE query_GetData(IDirect3DQuery9 *self, void *data, DWORD size, DWORD flags)
{
    struct vkd_query *q = (struct vkd_query *)self;
    struct vkd_device *d = q->dev;
    DWORD need = query_GetDataSize(self);
    if (data && size && size < need) return D3DERR_INVALIDCALL;
    if (q->state == 1) return S_FALSE;
    VKD_LOCK(d);
    HRESULT hr = S_OK;
    if (q->state == 2) {
        if (q->seq > d->completed) vkd_poll(d);
        if (q->seq > d->completed) {
            if ((flags & D3DGETDATA_FLUSH) && q->seq == d->seq) vkd_submit(d, 0, 0);
            hr = S_FALSE;
        }
    }
    if (hr == S_OK && data && size) {
        if (q->type == D3DQUERYTYPE_EVENT) *(BOOL *)data = TRUE;
        else *(DWORD *)data = (DWORD)q->result;
    }
    VKD_UNLOCK(d);
    return hr;
}

const IDirect3DQuery9Vtbl vkd_query_vtbl = {
    .QueryInterface = query_QueryInterface,
    .AddRef = query_AddRef,
    .Release = query_Release,
    .GetDevice = query_GetDevice,
    .GetType = query_GetType,
    .GetDataSize = query_GetDataSize,
    .Issue = query_Issue,
    .GetData = query_GetData,
};

/* ---- state ---- */

struct vkd_light *vkd_light_slot(struct vkd_state *s, DWORD index)
{
    if (index >= s->nlights) {
        uint32_t n = index + 1;
        uint32_t cap = VKP_MAX(n, s->nlights * 2);
        struct vkd_light *l = vkp_realloc(s->lights, cap * sizeof *l);
        if (!l) return 0;
        memset(l + s->nlights, 0, (cap - s->nlights) * sizeof *l);
        s->lights = l;
        s->nlights = cap;
    }
    return &s->lights[index];
}

static const D3DMATRIX identity = {{{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}}};

void vkd_state_defaults(struct vkd_device *d, struct vkd_state *s)
{
    DWORD *rs = s->rs;
    memset(rs, 0, sizeof s->rs);
    rs[D3DRS_ZENABLE] = d->auto_ds ? D3DZB_TRUE : D3DZB_FALSE;
    rs[D3DRS_FILLMODE] = D3DFILL_SOLID;
    rs[D3DRS_SHADEMODE] = D3DSHADE_GOURAUD;
    rs[D3DRS_ZWRITEENABLE] = TRUE;
    rs[D3DRS_LASTPIXEL] = TRUE;
    rs[D3DRS_SRCBLEND] = D3DBLEND_ONE;
    rs[D3DRS_DESTBLEND] = D3DBLEND_ZERO;
    rs[D3DRS_CULLMODE] = D3DCULL_CCW;
    rs[D3DRS_ZFUNC] = D3DCMP_LESSEQUAL;
    rs[D3DRS_ALPHAFUNC] = D3DCMP_ALWAYS;
    rs[D3DRS_FOGEND] = vkd_u(1.0f);
    rs[D3DRS_FOGDENSITY] = vkd_u(1.0f);
    rs[D3DRS_STENCILFAIL] = rs[D3DRS_STENCILZFAIL] = rs[D3DRS_STENCILPASS] = D3DSTENCILOP_KEEP;
    rs[D3DRS_STENCILFUNC] = D3DCMP_ALWAYS;
    rs[D3DRS_STENCILMASK] = rs[D3DRS_STENCILWRITEMASK] = 0xffffffff;
    rs[D3DRS_TEXTUREFACTOR] = 0xffffffff;
    rs[D3DRS_CLIPPING] = TRUE;
    rs[D3DRS_LIGHTING] = TRUE;
    rs[D3DRS_COLORVERTEX] = TRUE;
    rs[D3DRS_LOCALVIEWER] = TRUE;
    rs[D3DRS_DIFFUSEMATERIALSOURCE] = D3DMCS_COLOR1;
    rs[D3DRS_SPECULARMATERIALSOURCE] = D3DMCS_COLOR2;
    rs[D3DRS_AMBIENTMATERIALSOURCE] = D3DMCS_MATERIAL;
    rs[D3DRS_EMISSIVEMATERIALSOURCE] = D3DMCS_MATERIAL;
    rs[D3DRS_POINTSIZE] = vkd_u(1.0f);
    rs[D3DRS_POINTSIZE_MIN] = vkd_u(1.0f);
    rs[D3DRS_POINTSCALE_A] = vkd_u(1.0f);
    rs[D3DRS_MULTISAMPLEANTIALIAS] = TRUE;
    rs[D3DRS_MULTISAMPLEMASK] = 0xffffffff;
    rs[D3DRS_DEBUGMONITORTOKEN] = 0xbaadcafe;
    rs[D3DRS_POINTSIZE_MAX] = vkd_u(64.0f);
    rs[D3DRS_COLORWRITEENABLE] = rs[D3DRS_COLORWRITEENABLE1] = rs[D3DRS_COLORWRITEENABLE2] =
        rs[D3DRS_COLORWRITEENABLE3] = 0xf;
    rs[D3DRS_BLENDOP] = D3DBLENDOP_ADD;
    rs[D3DRS_POSITIONDEGREE] = D3DDEGREE_CUBIC;
    rs[D3DRS_NORMALDEGREE] = D3DDEGREE_LINEAR;
    rs[D3DRS_MINTESSELLATIONLEVEL] = vkd_u(1.0f);
    rs[D3DRS_MAXTESSELLATIONLEVEL] = vkd_u(1.0f);
    rs[D3DRS_ADAPTIVETESS_Z] = vkd_u(1.0f);
    rs[D3DRS_CCW_STENCILFAIL] = rs[D3DRS_CCW_STENCILZFAIL] = rs[D3DRS_CCW_STENCILPASS] = D3DSTENCILOP_KEEP;
    rs[D3DRS_CCW_STENCILFUNC] = D3DCMP_ALWAYS;
    rs[D3DRS_BLENDFACTOR] = 0xffffffff;
    rs[D3DRS_SRCBLENDALPHA] = D3DBLEND_ONE;
    rs[D3DRS_DESTBLENDALPHA] = D3DBLEND_ZERO;
    rs[D3DRS_BLENDOPALPHA] = D3DBLENDOP_ADD;

    for (int i = 0; i < VKD_SAMPLERS; i++) {
        DWORD *ss = s->ss[i];
        memset(ss, 0, sizeof s->ss[i]);
        ss[D3DSAMP_ADDRESSU] = ss[D3DSAMP_ADDRESSV] = ss[D3DSAMP_ADDRESSW] = D3DTADDRESS_WRAP;
        ss[D3DSAMP_MAGFILTER] = ss[D3DSAMP_MINFILTER] = D3DTEXF_POINT;
        ss[D3DSAMP_MIPFILTER] = D3DTEXF_NONE;
        ss[D3DSAMP_MAXANISOTROPY] = 1;
    }
    for (int i = 0; i < 8; i++) {
        DWORD *t = s->tss[i];
        memset(t, 0, sizeof s->tss[i]);
        t[D3DTSS_COLOROP] = i ? D3DTOP_DISABLE : D3DTOP_MODULATE;
        t[D3DTSS_COLORARG1] = D3DTA_TEXTURE;
        t[D3DTSS_COLORARG2] = D3DTA_CURRENT;
        t[D3DTSS_ALPHAOP] = i ? D3DTOP_DISABLE : D3DTOP_SELECTARG1;
        t[D3DTSS_ALPHAARG1] = D3DTA_TEXTURE;
        t[D3DTSS_ALPHAARG2] = D3DTA_CURRENT;
        t[D3DTSS_TEXCOORDINDEX] = (DWORD)i;
        t[D3DTSS_TEXTURETRANSFORMFLAGS] = D3DTTFF_DISABLE;
        t[D3DTSS_COLORARG0] = t[D3DTSS_ALPHAARG0] = t[D3DTSS_RESULTARG] = D3DTA_CURRENT;
    }
    for (int i = 0; i < 16; i++) s->vb_freq[i] = 1;
    s->view = s->proj = identity;
    for (int i = 0; i < 8; i++) s->texm[i] = identity;
    for (int i = 0; i < 256; i++) s->world[i] = identity;
    memset(&s->material, 0, sizeof s->material);
    memset(s->clip, 0, sizeof s->clip);
    s->npatch = 0;
}

void vkd_state_release(struct vkd_state *s)
{
    for (int i = 0; i < VKD_SAMPLERS; i++) vkd_ref_tex(&s->tex[i], 0);
    for (int i = 0; i < 16; i++) vkd_ref_buf(&s->vb[i], 0);
    vkd_ref_buf(&s->ib, 0);
    vkd_ref_decl(&s->decl, 0);
    vkd_ref_shader(&s->vs, 0);
    vkd_ref_shader(&s->ps, 0);
    vkp_free(s->lights);
    s->lights = 0;
    s->nlights = 0;
}

void vkd_mask_free(struct vkd_mask *m)
{
    vkp_free(m->light);
    m->light = 0;
    m->nlight = 0;
}

/* ---- state blocks ---- */

static const uint8_t pixel_rs[] = {
    D3DRS_ALPHABLENDENABLE, D3DRS_ALPHAFUNC, D3DRS_ALPHAREF, D3DRS_ALPHATESTENABLE, D3DRS_ANTIALIASEDLINEENABLE,
    D3DRS_BLENDFACTOR, D3DRS_BLENDOP, D3DRS_BLENDOPALPHA, D3DRS_CCW_STENCILFAIL, D3DRS_CCW_STENCILPASS,
    D3DRS_CCW_STENCILZFAIL, D3DRS_COLORWRITEENABLE, D3DRS_COLORWRITEENABLE1, D3DRS_COLORWRITEENABLE2,
    D3DRS_COLORWRITEENABLE3, D3DRS_DEPTHBIAS, D3DRS_DESTBLEND, D3DRS_DESTBLENDALPHA, D3DRS_DITHERENABLE,
    D3DRS_FILLMODE, D3DRS_FOGDENSITY, D3DRS_FOGEND, D3DRS_FOGSTART, D3DRS_LASTPIXEL, D3DRS_SCISSORTESTENABLE,
    D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SHADEMODE, D3DRS_SLOPESCALEDEPTHBIAS, D3DRS_SRCBLEND, D3DRS_SRCBLENDALPHA,
    D3DRS_SRGBWRITEENABLE, D3DRS_STENCILENABLE, D3DRS_STENCILFAIL, D3DRS_STENCILFUNC, D3DRS_STENCILMASK,
    D3DRS_STENCILPASS, D3DRS_STENCILREF, D3DRS_STENCILWRITEMASK, D3DRS_STENCILZFAIL, D3DRS_TEXTUREFACTOR,
    D3DRS_TWOSIDEDSTENCILMODE, D3DRS_WRAP0, D3DRS_WRAP1, D3DRS_WRAP2, D3DRS_WRAP3, D3DRS_WRAP4, D3DRS_WRAP5,
    D3DRS_WRAP6, D3DRS_WRAP7, D3DRS_WRAP8, D3DRS_WRAP9, D3DRS_WRAP10, D3DRS_WRAP11, D3DRS_WRAP12, D3DRS_WRAP13,
    D3DRS_WRAP14, D3DRS_WRAP15, D3DRS_ZENABLE, D3DRS_ZFUNC, D3DRS_ZWRITEENABLE, D3DRS_CCW_STENCILFUNC,
};
static const uint8_t vertex_rs[] = {
    D3DRS_ADAPTIVETESS_W, D3DRS_ADAPTIVETESS_X, D3DRS_ADAPTIVETESS_Y, D3DRS_ADAPTIVETESS_Z, D3DRS_AMBIENT,
    D3DRS_AMBIENTMATERIALSOURCE, D3DRS_CLIPPING, D3DRS_CLIPPLANEENABLE, D3DRS_COLORVERTEX, D3DRS_CULLMODE,
    D3DRS_DIFFUSEMATERIALSOURCE, D3DRS_EMISSIVEMATERIALSOURCE, D3DRS_ENABLEADAPTIVETESSELLATION, D3DRS_FOGCOLOR,
    D3DRS_FOGDENSITY, D3DRS_FOGENABLE, D3DRS_FOGEND, D3DRS_FOGSTART, D3DRS_FOGTABLEMODE, D3DRS_FOGVERTEXMODE,
    D3DRS_INDEXEDVERTEXBLENDENABLE, D3DRS_LIGHTING, D3DRS_LOCALVIEWER, D3DRS_MAXTESSELLATIONLEVEL,
    D3DRS_MINTESSELLATIONLEVEL, D3DRS_MULTISAMPLEANTIALIAS, D3DRS_MULTISAMPLEMASK, D3DRS_NORMALDEGREE,
    D3DRS_NORMALIZENORMALS, D3DRS_PATCHEDGESTYLE, D3DRS_POINTSCALE_A, D3DRS_POINTSCALE_B, D3DRS_POINTSCALE_C,
    D3DRS_POINTSCALEENABLE, D3DRS_POINTSIZE, D3DRS_POINTSIZE_MAX, D3DRS_POINTSIZE_MIN, D3DRS_POINTSPRITEENABLE,
    D3DRS_POSITIONDEGREE, D3DRS_RANGEFOGENABLE, D3DRS_SHADEMODE, D3DRS_SPECULARENABLE,
    D3DRS_SPECULARMATERIALSOURCE, D3DRS_TWEENFACTOR, D3DRS_VERTEXBLEND,
};

#define BIT_SET(a, i) ((a)[(i) >> 5] |= 1u << ((i) & 31))
#define BIT_GET(a, i) ((a)[(i) >> 5] >> ((i) & 31) & 1)

static int mask_lights(struct vkd_mask *m, uint32_t n)
{
    if (n <= m->nlight) return 1;
    uint8_t *l = vkp_realloc(m->light, n);
    if (!l) return 0;
    memset(l + m->nlight, 0, n - m->nlight);
    m->light = l;
    m->nlight = n;
    return 1;
}

static void mask_for_type(struct vkd_device *d, struct vkd_mask *m, D3DSTATEBLOCKTYPE type)
{
    int all = type == D3DSBT_ALL, pixel = all || type == D3DSBT_PIXELSTATE, vertex = all || type == D3DSBT_VERTEXSTATE;
    if (all) {
        for (int i = 1; i < VKD_MAX_RS; i++) BIT_SET(m->rs, i);
    } else {
        if (pixel) for (uint32_t i = 0; i < sizeof pixel_rs; i++) BIT_SET(m->rs, pixel_rs[i]);
        if (vertex) for (uint32_t i = 0; i < sizeof vertex_rs; i++) BIT_SET(m->rs, vertex_rs[i]);
    }
    for (int i = 0; i < VKD_SAMPLERS; i++) {
        if (pixel) m->ss[i] |= 0x3ffeu & ~(1u << D3DSAMP_DMAPOFFSET);
        if (vertex) m->ss[i] |= 1u << D3DSAMP_DMAPOFFSET;
    }
    for (int i = 0; i < 8; i++) {
        uint64_t vbits = UINT64_C(1) << D3DTSS_TEXCOORDINDEX | UINT64_C(1) << D3DTSS_TEXTURETRANSFORMFLAGS;
        /* Native D3D9 excludes CONSTANT from PIXELSTATE; custom blocks and
         * ALL still need its bit 32 (test_gpu.c compares the native runtime). */
        if (pixel) m->tss[i] |= (all ? UINT64_C(0x1fffffffe) : UINT64_C(0xfffffffe)) & ~vbits;
        if (vertex) m->tss[i] |= vbits;
    }
    if (pixel) {
        for (int i = 0; i < 7; i++) m->psf[i] = 0xffffffff;
        m->psi = m->psb = 0xffff;
        m->misc |= VKD_SB_PS;
    }
    if (vertex) {
        for (int i = 0; i < 8; i++) m->vsf[i] = 0xffffffff;
        m->vsi = m->vsb = 0xffff;
        m->vb_freq = 0xffff;
        m->misc |= VKD_SB_VS | VKD_SB_DECL | VKD_SB_LIGHTS_ALL;
        if (mask_lights(m, d->st.nlights)) memset(m->light, 1, m->nlight);
    }
    if (all) {
        m->tex = (1u << VKD_SAMPLERS) - 1;
        m->vb = 0xffff;
        for (int i = 0; i < 8; i++) m->world[i] = 0xffffffff;
        m->texm = 0xff;
        m->misc |= VKD_SB_IB | VKD_SB_VIEW | VKD_SB_PROJ | VKD_SB_MATERIAL | VKD_SB_VIEWPORT | VKD_SB_SCISSOR |
                   VKD_SB_CLIP | VKD_SB_NPATCH;
    }
}

/* copy the masked state from the device into the block */
void vkd_stateblock_capture(struct vkd_stateblock *sb)
{
    struct vkd_device *d = sb->dev;
    struct vkd_state *s = &sb->s, *src = &d->st;
    struct vkd_mask *m = &sb->m;
    for (int i = 1; i < VKD_MAX_RS; i++)
        if (BIT_GET(m->rs, i)) s->rs[i] = src->rs[i];
    for (int i = 0; i < VKD_SAMPLERS; i++) {
        for (int k = 1; k < 14; k++)
            if (m->ss[i] >> k & 1) s->ss[i][k] = src->ss[i][k];
        if (m->tex >> i & 1) vkd_ref_tex(&s->tex[i], src->tex[i]);
    }
    for (int i = 0; i < 8; i++)
        for (int k = 1; k < 33; k++)
            if (m->tss[i] >> k & 1) s->tss[i][k] = src->tss[i][k];
    for (int i = 0; i < 16; i++) {
        if (m->vb >> i & 1) {
            vkd_ref_buf(&s->vb[i], src->vb[i]);
            s->vb_off[i] = src->vb_off[i];
            s->vb_stride[i] = src->vb_stride[i];
        }
        if (m->vb_freq >> i & 1) s->vb_freq[i] = src->vb_freq[i];
    }
    if (m->misc & VKD_SB_IB) vkd_ref_buf(&s->ib, src->ib);
    if (m->misc & VKD_SB_DECL) { vkd_ref_decl(&s->decl, src->decl); s->fvf = src->fvf; }
    if (m->misc & VKD_SB_VS) vkd_ref_shader(&s->vs, src->vs);
    if (m->misc & VKD_SB_PS) vkd_ref_shader(&s->ps, src->ps);
    for (int i = 0; i < 256; i++) if (BIT_GET(m->vsf, i)) memcpy(s->vsf[i], src->vsf[i], 16);
    for (int i = 0; i < 224; i++) if (BIT_GET(m->psf, i)) memcpy(s->psf[i], src->psf[i], 16);
    for (int i = 0; i < 16; i++) {
        if (m->vsi >> i & 1) memcpy(s->vsi[i], src->vsi[i], 16);
        if (m->psi >> i & 1) memcpy(s->psi[i], src->psi[i], 16);
        if (m->vsb >> i & 1) s->vsb[i] = src->vsb[i];
        if (m->psb >> i & 1) s->psb[i] = src->psb[i];
    }
    if (m->misc & VKD_SB_VIEW) s->view = src->view;
    if (m->misc & VKD_SB_PROJ) s->proj = src->proj;
    for (int i = 0; i < 8; i++) if (m->texm >> i & 1) s->texm[i] = src->texm[i];
    for (int i = 0; i < 256; i++) if (BIT_GET(m->world, i)) s->world[i] = src->world[i];
    if (m->misc & VKD_SB_LIGHTS_ALL) {
        if (mask_lights(m, src->nlights)) memset(m->light, 1, m->nlight);
    }
    for (uint32_t i = 0; i < m->nlight; i++) {
        if (!m->light[i]) continue;
        struct vkd_light *l = vkd_light_slot(s, i);
        if (!l) break;
        if (i < src->nlights) *l = src->lights[i];
        else memset(l, 0, sizeof *l);
    }
    if (m->misc & VKD_SB_MATERIAL) s->material = src->material;
    if (m->misc & VKD_SB_VIEWPORT) s->viewport = src->viewport;
    if (m->misc & VKD_SB_SCISSOR) s->scissor = src->scissor;
    if (m->misc & VKD_SB_CLIP) memcpy(s->clip, src->clip, sizeof s->clip);
    if (m->misc & VKD_SB_NPATCH) s->npatch = src->npatch;
}

static DWORD sampler_id(int i) { return i < 16 ? (DWORD)i : D3DVERTEXTEXTURESAMPLER0 + (DWORD)(i - 16); }

static void stateblock_apply(struct vkd_stateblock *sb)
{
    IDirect3DDevice9 *dev = (IDirect3DDevice9 *)sb->dev;
    struct vkd_state *s = &sb->s;
    struct vkd_mask *m = &sb->m;
    if (m->misc & VKD_SB_VS) IDirect3DDevice9_SetVertexShader(dev, (IDirect3DVertexShader9 *)s->vs);
    if (m->misc & VKD_SB_PS) IDirect3DDevice9_SetPixelShader(dev, (IDirect3DPixelShader9 *)s->ps);
    if (m->misc & VKD_SB_DECL) {
        if (s->decl) IDirect3DDevice9_SetVertexDeclaration(dev, (IDirect3DVertexDeclaration9 *)s->decl);
        else if (s->fvf) IDirect3DDevice9_SetFVF(dev, s->fvf);
    }
    for (int i = 0; i < 256; i++)
        if (BIT_GET(m->vsf, i)) IDirect3DDevice9_SetVertexShaderConstantF(dev, (UINT)i, s->vsf[i], 1);
    for (int i = 0; i < 224; i++)
        if (BIT_GET(m->psf, i)) IDirect3DDevice9_SetPixelShaderConstantF(dev, (UINT)i, s->psf[i], 1);
    for (int i = 0; i < 16; i++) {
        if (m->vsi >> i & 1) IDirect3DDevice9_SetVertexShaderConstantI(dev, (UINT)i, s->vsi[i], 1);
        if (m->psi >> i & 1) IDirect3DDevice9_SetPixelShaderConstantI(dev, (UINT)i, s->psi[i], 1);
        if (m->vsb >> i & 1) IDirect3DDevice9_SetVertexShaderConstantB(dev, (UINT)i, &s->vsb[i], 1);
        if (m->psb >> i & 1) IDirect3DDevice9_SetPixelShaderConstantB(dev, (UINT)i, &s->psb[i], 1);
    }
    for (int i = 1; i < VKD_MAX_RS; i++)
        if (BIT_GET(m->rs, i)) IDirect3DDevice9_SetRenderState(dev, (D3DRENDERSTATETYPE)i, s->rs[i]);
    for (uint32_t i = 0; i < m->nlight; i++) {
        if (!m->light[i] || i >= s->nlights) continue;
        if (s->lights[i].set) IDirect3DDevice9_SetLight(dev, i, &s->lights[i].l);
        if (s->lights[i].set || s->lights[i].on) IDirect3DDevice9_LightEnable(dev, i, s->lights[i].on);
    }
    for (int i = 0; i < VKD_SAMPLERS; i++) {
        for (int k = 1; k < 14; k++)
            if (m->ss[i] >> k & 1)
                IDirect3DDevice9_SetSamplerState(dev, sampler_id(i), (D3DSAMPLERSTATETYPE)k, s->ss[i][k]);
        if (m->tex >> i & 1)
            IDirect3DDevice9_SetTexture(dev, sampler_id(i), (IDirect3DBaseTexture9 *)s->tex[i]);
    }
    for (int i = 0; i < 8; i++)
        for (int k = 1; k < 33; k++)
            if (m->tss[i] >> k & 1)
                IDirect3DDevice9_SetTextureStageState(dev, (DWORD)i, (D3DTEXTURESTAGESTATETYPE)k, s->tss[i][k]);
    for (int i = 0; i < 16; i++) {
        if (m->vb >> i & 1)
            IDirect3DDevice9_SetStreamSource(dev, (UINT)i, (IDirect3DVertexBuffer9 *)s->vb[i], s->vb_off[i],
                                             s->vb_stride[i]);
        if (m->vb_freq >> i & 1) IDirect3DDevice9_SetStreamSourceFreq(dev, (UINT)i, s->vb_freq[i]);
    }
    if (m->misc & VKD_SB_IB) IDirect3DDevice9_SetIndices(dev, (IDirect3DIndexBuffer9 *)s->ib);
    if (m->misc & VKD_SB_VIEW) IDirect3DDevice9_SetTransform(dev, D3DTS_VIEW, &s->view);
    if (m->misc & VKD_SB_PROJ) IDirect3DDevice9_SetTransform(dev, D3DTS_PROJECTION, &s->proj);
    for (int i = 0; i < 8; i++)
        if (m->texm >> i & 1) IDirect3DDevice9_SetTransform(dev, (D3DTRANSFORMSTATETYPE)(D3DTS_TEXTURE0 + i), &s->texm[i]);
    for (int i = 0; i < 256; i++)
        if (BIT_GET(m->world, i)) IDirect3DDevice9_SetTransform(dev, D3DTS_WORLDMATRIX(i), &s->world[i]);
    if (m->misc & VKD_SB_MATERIAL) IDirect3DDevice9_SetMaterial(dev, &s->material);
    if (m->misc & VKD_SB_VIEWPORT) IDirect3DDevice9_SetViewport(dev, &s->viewport);
    if (m->misc & VKD_SB_SCISSOR) IDirect3DDevice9_SetScissorRect(dev, &s->scissor);
    if (m->misc & VKD_SB_CLIP)
        for (int i = 0; i < 6; i++) IDirect3DDevice9_SetClipPlane(dev, (DWORD)i, s->clip[i]);
    if (m->misc & VKD_SB_NPATCH) IDirect3DDevice9_SetNPatchMode(dev, s->npatch);
}

static struct vkd_stateblock *stateblock_new(struct vkd_device *d)
{
    struct vkd_stateblock *sb = vkp_alloc(sizeof *sb);
    if (!sb) return 0;
    sb->vtbl = &vkd_sb_vtbl;
    sb->ref = 1;
    sb->dev = d;
    vkd_state_defaults(d, &sb->s);
    return sb;
}

HRESULT vkd_create_stateblock(struct vkd_device *d, D3DSTATEBLOCKTYPE type, struct vkd_stateblock **out)
{
    *out = 0;
    if (type != D3DSBT_ALL && type != D3DSBT_PIXELSTATE && type != D3DSBT_VERTEXSTATE) return D3DERR_INVALIDCALL;
    struct vkd_stateblock *sb = stateblock_new(d);
    if (!sb) return E_OUTOFMEMORY;
    mask_for_type(d, &sb->m, type);
    vkd_stateblock_capture(sb);
    *out = sb;
    return D3D_OK;
}

/* BeginStateBlock: the device records Set* calls into d->recording */
void vkd_stateblock_begin(struct vkd_device *d) { d->recording = stateblock_new(d); }

static HRESULT STDMETHODCALLTYPE sb_QueryInterface(IDirect3DStateBlock9 *self, REFIID riid, void **out)
{
    if (!out) return E_POINTER;
    if (vkd_guid_eq(riid, &IID_IUnknown) || vkd_guid_eq(riid, &IID_IDirect3DStateBlock9)) {
        *out = self;
        IDirect3DStateBlock9_AddRef(self);
        return S_OK;
    }
    *out = 0;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE sb_AddRef(IDirect3DStateBlock9 *self)
{
    return (ULONG)InterlockedIncrement(&((struct vkd_stateblock *)self)->ref);
}
static ULONG STDMETHODCALLTYPE sb_Release(IDirect3DStateBlock9 *self)
{
    struct vkd_stateblock *sb = (struct vkd_stateblock *)self;
    LONG n = InterlockedDecrement(&sb->ref);
    if (n < 0) { sb->ref = 0; n = 0; }
    if (!n) {
        struct vkd_device *d = sb->dev;
        VKD_LOCK(d);
        vkd_state_release(&sb->s);
        vkd_mask_free(&sb->m);
        vkp_free(sb);
        VKD_UNLOCK(d);
        IDirect3DDevice9_Release((IDirect3DDevice9 *)d);
    }
    return (ULONG)n;
}
static HRESULT STDMETHODCALLTYPE sb_GetDevice(IDirect3DStateBlock9 *self, IDirect3DDevice9 **out)
{
    if (!out) return D3DERR_INVALIDCALL;
    *out = (IDirect3DDevice9 *)((struct vkd_stateblock *)self)->dev;
    IDirect3DDevice9_AddRef(*out);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE sb_Capture(IDirect3DStateBlock9 *self)
{
    struct vkd_stateblock *sb = (struct vkd_stateblock *)self;
    if (sb->dev->recording) return D3DERR_INVALIDCALL;
    VKD_LOCK(sb->dev);
    vkd_stateblock_capture(sb);
    VKD_UNLOCK(sb->dev);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE sb_Apply(IDirect3DStateBlock9 *self)
{
    struct vkd_stateblock *sb = (struct vkd_stateblock *)self;
    if (sb->dev->recording) return D3DERR_INVALIDCALL;
    stateblock_apply(sb);
    return D3D_OK;
}

const IDirect3DStateBlock9Vtbl vkd_sb_vtbl = {
    .QueryInterface = sb_QueryInterface,
    .AddRef = sb_AddRef,
    .Release = sb_Release,
    .GetDevice = sb_GetDevice,
    .Capture = sb_Capture,
    .Apply = sb_Apply,
};

/* ---- present shaders ----
 * Written as D3D9 bytecode and translated like application shaders, so they
 * share the device's descriptor layout. */

enum { T_TEMP = 0, T_INPUT = 1, T_CONST = 2, T_OUTPUT = 6, T_COLOROUT = 8, T_SAMPLER = 10 };
#define TOK_RT(t) (((uint32_t)(t) & 7u) << 28 | ((uint32_t)(t) & 0x18u) << 8)
#define TOK_D(t, n, m) (0x80000000u | TOK_RT(t) | (uint32_t)(m) << 16 | (uint32_t)(n))
#define TOK_S(t, n, swz) (0x80000000u | TOK_RT(t) | (uint32_t)(swz) << 16 | (uint32_t)(n))
#define TOK_OP(op, len) ((uint32_t)(op) | (uint32_t)(len) << 24)
#define XYZW 0xe4u
#define XXXX 0x00u
#define YYYY 0x55u
#define ZZZZ 0xaau
#define WWWW 0xffu
enum { OPC_MOV = 1, OPC_MAD = 4, OPC_DCL = 31, OPC_DEF = 81, OPC_TEX = 66 };

static const uint32_t present_vs_code[] = {
    0xfffe0300,
    TOK_OP(OPC_DCL, 2), 0x80000000u | D3DDECLUSAGE_POSITION, TOK_D(T_INPUT, 0, 15),
    TOK_OP(OPC_DCL, 2), 0x80000000u | D3DDECLUSAGE_TEXCOORD, TOK_D(T_INPUT, 1, 15),
    TOK_OP(OPC_DCL, 2), 0x80000000u | D3DDECLUSAGE_POSITION, TOK_D(T_OUTPUT, 0, 15),
    TOK_OP(OPC_DCL, 2), 0x80000000u | D3DDECLUSAGE_TEXCOORD, TOK_D(T_OUTPUT, 1, 15),
    TOK_OP(OPC_MOV, 2), TOK_D(T_OUTPUT, 0, 15), TOK_S(T_INPUT, 0, XYZW),
    TOK_OP(OPC_MOV, 2), TOK_D(T_OUTPUT, 1, 15), TOK_S(T_INPUT, 1, XYZW),
    0x0000ffff,
};

static const uint32_t present_ps_code[] = {
    0xffff0300,
    TOK_OP(OPC_DCL, 2), 0x80000000u | D3DDECLUSAGE_TEXCOORD, TOK_D(T_INPUT, 0, 3),
    TOK_OP(OPC_DCL, 2), 0x80000000u | 2u << 27, TOK_D(T_SAMPLER, 0, 15),
    TOK_OP(OPC_TEX, 3), TOK_D(T_TEMP, 0, 15), TOK_S(T_INPUT, 0, XYZW), TOK_S(T_SAMPLER, 0, XYZW),
    TOK_OP(OPC_MOV, 2), TOK_D(T_COLOROUT, 0, 15), TOK_S(T_TEMP, 0, XYZW),
    0x0000ffff,
};

/* gamma: each channel through its row of the 256-entry ramp (texel centres) */
#define F(x) ((union { float f; uint32_t u; }){(x)}).u
static const uint32_t present_ps_gamma_code[] = {
    0xffff0300,
    TOK_OP(OPC_DEF, 5), TOK_D(T_CONST, 0, 15), 0x3f7f0000 /* 255/256 */, 0x3b000000 /* 0.5/256 */, 0x3f000000, 0x3f800000,
    TOK_OP(OPC_DCL, 2), 0x80000000u | D3DDECLUSAGE_TEXCOORD, TOK_D(T_INPUT, 0, 3),
    TOK_OP(OPC_DCL, 2), 0x80000000u | 2u << 27, TOK_D(T_SAMPLER, 0, 15),
    TOK_OP(OPC_DCL, 2), 0x80000000u | 2u << 27, TOK_D(T_SAMPLER, 1, 15),
    TOK_OP(OPC_TEX, 3), TOK_D(T_TEMP, 0, 15), TOK_S(T_INPUT, 0, XYZW), TOK_S(T_SAMPLER, 0, XYZW),
    TOK_OP(OPC_MOV, 2), TOK_D(T_TEMP, 1, 2), TOK_S(T_CONST, 0, ZZZZ),
    TOK_OP(OPC_MAD, 4), TOK_D(T_TEMP, 1, 1), TOK_S(T_TEMP, 0, XXXX), TOK_S(T_CONST, 0, XXXX), TOK_S(T_CONST, 0, YYYY),
    TOK_OP(OPC_TEX, 3), TOK_D(T_TEMP, 2, 15), TOK_S(T_TEMP, 1, XYZW), TOK_S(T_SAMPLER, 1, XYZW),
    TOK_OP(OPC_MAD, 4), TOK_D(T_TEMP, 1, 1), TOK_S(T_TEMP, 0, YYYY), TOK_S(T_CONST, 0, XXXX), TOK_S(T_CONST, 0, YYYY),
    TOK_OP(OPC_TEX, 3), TOK_D(T_TEMP, 3, 15), TOK_S(T_TEMP, 1, XYZW), TOK_S(T_SAMPLER, 1, XYZW),
    TOK_OP(OPC_MAD, 4), TOK_D(T_TEMP, 1, 1), TOK_S(T_TEMP, 0, ZZZZ), TOK_S(T_CONST, 0, XXXX), TOK_S(T_CONST, 0, YYYY),
    TOK_OP(OPC_TEX, 3), TOK_D(T_TEMP, 4, 15), TOK_S(T_TEMP, 1, XYZW), TOK_S(T_SAMPLER, 1, XYZW),
    TOK_OP(OPC_MOV, 2), TOK_D(T_TEMP, 5, 1), TOK_S(T_TEMP, 2, XXXX),
    TOK_OP(OPC_MOV, 2), TOK_D(T_TEMP, 5, 2), TOK_S(T_TEMP, 3, YYYY),
    TOK_OP(OPC_MOV, 2), TOK_D(T_TEMP, 5, 4), TOK_S(T_TEMP, 4, ZZZZ),
    TOK_OP(OPC_MOV, 2), TOK_D(T_TEMP, 5, 8), TOK_S(T_CONST, 0, WWWW),
    TOK_OP(OPC_MOV, 2), TOK_D(T_COLOROUT, 0, 15), TOK_S(T_TEMP, 5, XYZW),
    0x0000ffff,
};
#undef F

static VkShaderModule bytecode_module(const uint32_t *code, uint32_t n)
{
    struct dxso_info info;
    struct dxso_key key;
    memset(&key, 0, sizeof key);
    if (!dxso_scan(code, n, &info)) return 0;
    uint32_t words = 0;
    uint32_t *spv = dxso_compile(code, &info, &key, &words);
    VkShaderModule m = spv ? vkd_module(spv, words) : 0;
    vkp_free(spv);
    return m;
}

static int present_init(struct vkd_device *d)
{
    if (d->present_vs) return 1;
    d->present_vs = bytecode_module(present_vs_code, VKP_COUNT(present_vs_code));
    d->present_ps = bytecode_module(present_ps_code, VKP_COUNT(present_ps_code));
    d->present_ps_gamma = bytecode_module(present_ps_gamma_code, VKP_COUNT(present_ps_gamma_code));
    VkSamplerCreateInfo si = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};
    si.magFilter = si.minFilter = VK_FILTER_LINEAR;
    si.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    si.addressModeU = si.addressModeV = si.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    si.maxLod = 0;
    if (!d->present_sampler) vkCreateSampler(vkd.dev, &si, 0, &d->present_sampler);
    if (!d->lut_sampler) vkCreateSampler(vkd.dev, &si, 0, &d->lut_sampler);
    if (!d->present_vs || !d->present_ps || !d->present_ps_gamma) {
        vkp_log("vulkan: present shaders failed");
        return 0;
    }
    return 1;
}

static void drop_pipe(void *val, void *ctx)
{
    VkPipeline *pipe = val;
    if (pipe) { vkDestroyPipeline(vkd.dev, *pipe, 0); vkp_free(pipe); }
    (void)ctx;
}

void vkd_present_free(struct vkd_device *d)
{
    vkd_map_clear(&d->present_pipes, drop_pipe, 0);
    if (d->present_vs) vkDestroyShaderModule(vkd.dev, d->present_vs, 0);
    if (d->present_ps) vkDestroyShaderModule(vkd.dev, d->present_ps, 0);
    if (d->present_ps_gamma) vkDestroyShaderModule(vkd.dev, d->present_ps_gamma, 0);
    if (d->present_sampler) vkDestroySampler(vkd.dev, d->present_sampler, 0);
    if (d->lut_sampler) vkDestroySampler(vkd.dev, d->lut_sampler, 0);
    d->present_vs = d->present_ps = d->present_ps_gamma = 0;
    d->present_sampler = d->lut_sampler = 0;
}

static VkPipeline present_pipeline(struct vkd_device *d, VkFormat fmt, int gamma)
{
    struct { VkFormat f; int g; } key = {fmt, gamma};
    VkPipeline *p = vkd_map_get(&d->present_pipes, &key, sizeof key);
    if (p) return *p;
    VkPipelineShaderStageCreateInfo st[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                                             {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};
    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT; st[0].module = d->present_vs; st[0].pName = "main";
    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT; st[1].module = gamma ? d->present_ps_gamma : d->present_ps;
    st[1].pName = "main";
    VkVertexInputBindingDescription vb = {0, 24, VK_VERTEX_INPUT_RATE_VERTEX};
    VkVertexInputAttributeDescription va[2] = {{0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 0},
                                               {1, 0, VK_FORMAT_R32G32_SFLOAT, 16}};
    VkPipelineVertexInputStateCreateInfo vi = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};
    vi.vertexBindingDescriptionCount = 1; vi.pVertexBindingDescriptions = &vb;
    vi.vertexAttributeDescriptionCount = 2; vi.pVertexAttributeDescriptions = va;
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = vp.scissorCount = 1;
    VkPipelineRasterizationStateCreateInfo rs = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    rs.polygonMode = VK_POLYGON_MODE_FILL;
    rs.cullMode = VK_CULL_MODE_NONE;
    rs.lineWidth = 1;
    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState ba = {0};
    ba.colorWriteMask = 15;
    VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = 1; cb.pAttachments = &ba;
    VkDynamicState dyn[2] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo ds = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    ds.dynamicStateCount = 2; ds.pDynamicStates = dyn;
    VkPipelineRenderingCreateInfo ri = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    ri.colorAttachmentCount = 1; ri.pColorAttachmentFormats = &fmt;
    VkGraphicsPipelineCreateInfo ci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.pNext = &ri;
    ci.stageCount = 2; ci.pStages = st;
    ci.pVertexInputState = &vi; ci.pInputAssemblyState = &ia; ci.pViewportState = &vp;
    ci.pRasterizationState = &rs; ci.pMultisampleState = &ms; ci.pColorBlendState = &cb; ci.pDynamicState = &ds;
    ci.layout = d->layout;
    VkPipeline pipe = 0;
    if (vkCreateGraphicsPipelines(vkd.dev, d->pcache, 1, &ci, 0, &pipe) != VK_SUCCESS) return 0;
    p = vkp_alloc(sizeof *p);
    if (!p) { vkDestroyPipeline(vkd.dev, pipe, 0); return 0; }
    *p = pipe;
    vkd_map_put(&d->present_pipes, &key, sizeof key, p);
    if (vkd_map_get(&d->present_pipes, &key, sizeof key) != p) {
        vkp_free(p); vkDestroyPipeline(vkd.dev, pipe, 0); return 0;
    }
    return pipe;
}

/* ---- swap chains ---- */

static void sc_free_vk(struct vkd_swapchain *sc, int keep_swapchain)
{
    struct vkd_device *d = sc->dev;
    if (sc->swapchain || sc->surface) vkDeviceWaitIdle(vkd.dev);
    for (uint32_t i = 0; i < sc->nimages; i++) {
        if (sc->views && sc->views[i]) vkDestroyImageView(vkd.dev, sc->views[i], 0);
        if (sc->done && sc->done[i]) vkDestroySemaphore(vkd.dev, sc->done[i], 0);
    }
    vkp_free(sc->images); vkp_free(sc->views); vkp_free(sc->done);
    sc->images = 0; sc->views = 0; sc->done = 0; sc->nimages = 0;
    if (!keep_swapchain) {
        if (sc->swapchain) vkDestroySwapchainKHR(vkd.dev, sc->swapchain, 0);
        sc->swapchain = 0;
    }
    (void)d;
}

static UINT sc_interval(struct vkd_swapchain *sc)
{
    int opt = sc->dev->option_interval;
    if (opt >= 0) return (UINT)opt;
    UINT pi = sc->pp.PresentationInterval;
    if (pi == D3DPRESENT_INTERVAL_IMMEDIATE) return 0;
    if (pi == D3DPRESENT_INTERVAL_TWO) return 2;
    if (pi == D3DPRESENT_INTERVAL_THREE) return 3;
    if (pi == D3DPRESENT_INTERVAL_FOUR) return 4;
    return 1;
}

/* (re)create the Vulkan swap chain for the window's current size; 0 if it cannot be shown now */
static int sc_create_vk(struct vkd_swapchain *sc)
{
    if (!sc->surface) {
        VkWin32SurfaceCreateInfoKHR si = {VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
        si.hinstance = w32.GetModuleHandleA(0);
        si.hwnd = sc->hwnd;
        if (vkCreateWin32SurfaceKHR(vkd.instance, &si, 0, &sc->surface) != VK_SUCCESS) {
            vkp_log("vulkan: window surface failed");
            return 0;
        }
        VkBool32 ok = 0;
        vkGetPhysicalDeviceSurfaceSupportKHR(vkd.phys, vkd.queue_family, sc->surface, &ok);
        if (!ok) { vkp_log("vulkan: queue cannot present to the window"); return 0; }
    }
    VkSurfaceCapabilitiesKHR caps;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(vkd.phys, sc->surface, &caps) != VK_SUCCESS) return 0;
    VkExtent2D ext = caps.currentExtent;
    if (ext.width == 0xffffffffu) {
        RECT r;
        w32.GetClientRect(sc->hwnd, &r);
        ext.width = (uint32_t)(r.right - r.left);
        ext.height = (uint32_t)(r.bottom - r.top);
        ext.width = VKP_MAX(caps.minImageExtent.width, VKP_MIN(caps.maxImageExtent.width, ext.width));
        ext.height = VKP_MAX(caps.minImageExtent.height, VKP_MIN(caps.maxImageExtent.height, ext.height));
    }
    if (!ext.width || !ext.height) return 0;

    uint32_t nf = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkd.phys, sc->surface, &nf, 0);
    VkSurfaceFormatKHR *fmts = vkp_alloc((nf ? nf : 1) * sizeof *fmts);
    if (!fmts) return 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(vkd.phys, sc->surface, &nf, fmts);
    VkSurfaceFormatKHR pick = nf ? fmts[0] : (VkSurfaceFormatKHR){VK_FORMAT_B8G8R8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
    for (uint32_t i = 0; i < nf; i++)
        if ((fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM || fmts[i].format == VK_FORMAT_R8G8B8A8_UNORM) &&
            fmts[i].colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            pick = fmts[i];
            if (fmts[i].format == VK_FORMAT_B8G8R8A8_UNORM) break;
        }
    vkp_free(fmts);

    uint32_t nm = 0;
    VkPresentModeKHR modes[16];
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkd.phys, sc->surface, &nm, 0);
    nm = VKP_MIN(nm, 16u);
    vkGetPhysicalDeviceSurfacePresentModesKHR(vkd.phys, sc->surface, &nm, modes);
    VkPresentModeKHR mode = VK_PRESENT_MODE_FIFO_KHR;
    if (!sc_interval(sc)) {
        for (uint32_t i = 0; i < nm; i++) if (modes[i] == VK_PRESENT_MODE_MAILBOX_KHR) mode = modes[i];
        for (uint32_t i = 0; i < nm; i++) if (modes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) mode = modes[i];
    }

    uint32_t count = VKP_MAX(caps.minImageCount + 1, 3u);
    if (caps.maxImageCount) count = VKP_MIN(count, caps.maxImageCount);
    VkSwapchainCreateInfoKHR ci = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    ci.surface = sc->surface;
    ci.minImageCount = count;
    ci.imageFormat = pick.format;
    ci.imageColorSpace = pick.colorSpace;
    ci.imageExtent = ext;
    ci.imageArrayLayers = 1;
    ci.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    ci.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    ci.preTransform = (caps.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
                          ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : caps.currentTransform;
    ci.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    if (!(caps.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR))
        for (uint32_t b = 1; b; b <<= 1)
            if (caps.supportedCompositeAlpha & b) { ci.compositeAlpha = (VkCompositeAlphaFlagBitsKHR)b; break; }
    ci.presentMode = mode;
    ci.clipped = VK_TRUE;
    VkSwapchainKHR old = sc->swapchain;
    sc_free_vk(sc, 1);
    ci.oldSwapchain = old;
    VkSwapchainKHR n = 0;
    VkResult r = vkCreateSwapchainKHR(vkd.dev, &ci, 0, &n);
    if (old) vkDestroySwapchainKHR(vkd.dev, old, 0);
    sc->swapchain = 0;
    if (r != VK_SUCCESS) { vkp_log("vulkan: vkCreateSwapchainKHR failed (%d)", r); return 0; }
    sc->swapchain = n;
    sc->sc_format = pick.format;
    sc->extent = ext;
    sc->mode = mode;
    vkGetSwapchainImagesKHR(vkd.dev, n, &sc->nimages, 0);
    sc->images = vkp_alloc(sc->nimages * sizeof *sc->images);
    sc->views = vkp_alloc(sc->nimages * sizeof *sc->views);
    sc->done = vkp_alloc(sc->nimages * sizeof *sc->done);
    if (!sc->images || !sc->views || !sc->done) { sc->nimages = 0; return 0; }
    vkGetSwapchainImagesKHR(vkd.dev, n, &sc->nimages, sc->images);
    for (uint32_t i = 0; i < sc->nimages; i++) {
        VkImageViewCreateInfo vi = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
        vi.image = sc->images[i];
        vi.viewType = VK_IMAGE_VIEW_TYPE_2D;
        vi.format = pick.format;
        vi.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        vi.subresourceRange.levelCount = 1;
        vi.subresourceRange.layerCount = 1;
        if (vkCreateImageView(vkd.dev, &vi, 0, &sc->views[i]) != VK_SUCCESS) return 0;
        VkSemaphoreCreateInfo si = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
        if (vkCreateSemaphore(vkd.dev, &si, 0, &sc->done[i]) != VK_SUCCESS) return 0;
    }
    for (int i = 0; i < VKD_CONTEXTS; i++)
        if (!sc->acquire[i]) {
            VkSemaphoreCreateInfo si = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
            if (vkCreateSemaphore(vkd.dev, &si, 0, &sc->acquire[i]) != VK_SUCCESS) return 0;
        }
    sc->recreate = 0;
    vkp_log("vulkan: swap chain %ux%u, %u images, mode %d", ext.width, ext.height, sc->nimages, mode);
    return 1;
}

static void sc_free_buffers(struct vkd_swapchain *sc)
{
    struct vkd_device *d = sc->dev;
    for (UINT i = 0; i < sc->nback; i++) vkd_surface_free(sc->back[i]);
    vkp_free(sc->back);
    sc->back = 0;
    sc->nback = 0;
    if (sc->resolve) vkd_image_destroy(d, sc->resolve);
    sc->resolve = 0;
}

static HRESULT sc_create_buffers(struct vkd_swapchain *sc)
{
    struct vkd_device *d = sc->dev;
    D3DPRESENT_PARAMETERS *pp = &sc->pp;
    sc->nback = pp->BackBufferCount;
    sc->back = vkp_alloc(sc->nback * sizeof *sc->back);
    if (!sc->back) return E_OUTOFMEMORY;
    for (UINT i = 0; i < sc->nback; i++) {
        struct vkd_surface *s;
        HRESULT hr = vkd_create_surface(d, pp->BackBufferWidth, pp->BackBufferHeight, pp->BackBufferFormat,
                                        pp->MultiSampleType, pp->MultiSampleQuality, D3DUSAGE_RENDERTARGET,
                                        D3DPOOL_DEFAULT, !!(pp->Flags & D3DPRESENTFLAG_LOCKABLE_BACKBUFFER), 0, &s);
        if (FAILED(hr)) { sc->nback = i; sc_free_buffers(sc); return hr; }
        s->r.implicit = 1;
        s->r.ref = 0;
        s->swapchain = sc;
        sc->back[i] = s;
    }
    if (pp->MultiSampleType != D3DMULTISAMPLE_NONE) {
        sc->resolve = vkd_image_create(d, vkd_format(pp->BackBufferFormat), VK_IMAGE_TYPE_2D, pp->BackBufferWidth,
                                       pp->BackBufferHeight, 1, 1, 1, VK_SAMPLE_COUNT_1_BIT, 0, 1);
        if (!sc->resolve) { sc_free_buffers(sc); return D3DERR_OUTOFVIDEOMEMORY; }
    }
    return D3D_OK;
}

/* fill in the defaults D3D9 derives from the window */
static HRESULT sc_fix_params(struct vkd_swapchain *sc, D3DPRESENT_PARAMETERS *pp)
{
    if (pp->BackBufferCount > D3DPRESENT_BACK_BUFFERS_MAX) return D3DERR_INVALIDCALL;
    if (!pp->BackBufferCount) pp->BackBufferCount = 1;
    if (pp->Windowed) {
        RECT r = {0};
        w32.GetClientRect(sc->hwnd, &r);
        if (!pp->BackBufferWidth) pp->BackBufferWidth = r.right > r.left ? (UINT)(r.right - r.left) : 1;
        if (!pp->BackBufferHeight) pp->BackBufferHeight = r.bottom > r.top ? (UINT)(r.bottom - r.top) : 1;
        if (pp->BackBufferFormat == D3DFMT_UNKNOWN) {
            D3DDISPLAYMODE m;
            vkd_desktop_mode(&m);
            pp->BackBufferFormat = m.Format;
        }
    }
    if (!pp->BackBufferWidth || !pp->BackBufferHeight) return D3DERR_INVALIDCALL;
    D3DFORMAT f = pp->BackBufferFormat;
    if (f != D3DFMT_X8R8G8B8 && f != D3DFMT_A8R8G8B8 && f != D3DFMT_R5G6B5 && f != D3DFMT_X1R5G5B5 &&
        f != D3DFMT_A1R5G5B5 && f != D3DFMT_A2R10G10B10)
        return D3DERR_INVALIDCALL;
    return D3D_OK;
}

HRESULT vkd_swapchain_create(struct vkd_device *d, D3DPRESENT_PARAMETERS *pp, int implicit, struct vkd_swapchain **out)
{
    *out = 0;
    struct vkd_swapchain *sc = vkp_alloc(sizeof *sc);
    if (!sc) return E_OUTOFMEMORY;
    sc->vtbl = &vkd_sc_vtbl;
    sc->ref = implicit ? 0 : 1;
    sc->dev = d;
    sc->implicit = implicit;
    sc->hwnd = pp->hDeviceWindow ? pp->hDeviceWindow : d->cp.hFocusWindow;
    HRESULT hr = sc_fix_params(sc, pp);
    if (FAILED(hr)) { vkp_free(sc); return hr; }
    sc->pp = *pp;
    hr = sc_create_buffers(sc);
    if (FAILED(hr)) { vkp_free(sc); return hr; }
    for (int i = 0; i < 256; i++)
        sc->ramp.red[i] = sc->ramp.green[i] = sc->ramp.blue[i] = (WORD)(i * 257);
    if (!w32.IsIconic(sc->hwnd) && !sc_create_vk(sc)) {
        vkd_swapchain_destroy(sc);
        return D3DERR_NOTAVAILABLE;
    }
    *out = sc;
    return D3D_OK;
}

HRESULT vkd_swapchain_reset(struct vkd_swapchain *sc, D3DPRESENT_PARAMETERS *pp)
{
    HWND old = sc->hwnd;
    if (pp->hDeviceWindow) sc->hwnd = pp->hDeviceWindow;
    HRESULT hr = sc_fix_params(sc, pp);
    if (FAILED(hr)) { sc->hwnd = old; return hr; }
    sc_free_buffers(sc);
    sc->pp = *pp;
    hr = sc_create_buffers(sc);
    if (FAILED(hr)) return hr;
    if (old != sc->hwnd && sc->surface) {
        /* another window: a new surface */
        sc_free_vk(sc, 0);
        vkDestroySurfaceKHR(vkd.instance, sc->surface, 0);
        sc->surface = 0;
    }
    sc->recreate = 1;
    return D3D_OK;
}

void vkd_swapchain_destroy(struct vkd_swapchain *sc)
{
    if (!sc) return;
    sc_free_buffers(sc);
    sc_free_vk(sc, 0);
    for (int i = 0; i < VKD_CONTEXTS; i++)
        if (sc->acquire[i]) vkDestroySemaphore(vkd.dev, sc->acquire[i], 0);
    if (sc->surface) vkDestroySurfaceKHR(vkd.instance, sc->surface, 0);
    if (sc->gamma) vkd_image_destroy(sc->dev, sc->gamma);
    vkp_free(sc);
}

void vkd_swapchain_gamma(struct vkd_swapchain *sc, const D3DGAMMARAMP *ramp)
{
    sc->ramp = *ramp;
    sc->gamma_on = 0;
    for (int i = 0; i < 256; i++)
        if (ramp->red[i] != i * 257 || ramp->green[i] != i * 257 || ramp->blue[i] != i * 257) sc->gamma_on = 1;
    sc->gamma_dirty = 1;
}

static void sc_upload_gamma(struct vkd_swapchain *sc)
{
    struct vkd_device *d = sc->dev;
    if (!sc->gamma) {
        sc->gamma = vkd_image_create(d, vkd_format(D3DFMT_A16B16G16R16), VK_IMAGE_TYPE_2D, 256, 1, 1, 1, 1,
                                     VK_SAMPLE_COUNT_1_BIT, 0, 0);
        if (!sc->gamma) return;
    }
    uint16_t texels[256 * 4];
    for (int i = 0; i < 256; i++) {
        texels[i * 4 + 0] = sc->ramp.red[i];
        texels[i * 4 + 1] = sc->ramp.green[i];
        texels[i * 4 + 2] = sc->ramp.blue[i];
        texels[i * 4 + 3] = 0xffff;
    }
    vkd_image_write(d, sc->gamma, 0, 0, 0, 0, 0, 256, 1, 1, (const uint8_t *)texels, sizeof texels, sizeof texels);
    sc->gamma_dirty = 0;
}

static void image_barrier(VkCommandBuffer cmd, VkImage img, VkImageLayout from, VkImageLayout to,
                          VkPipelineStageFlags2 src, VkAccessFlags2 srca, VkPipelineStageFlags2 dst, VkAccessFlags2 dsta)
{
    VkImageMemoryBarrier2 b = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    b.srcStageMask = src; b.srcAccessMask = srca;
    b.dstStageMask = dst; b.dstAccessMask = dsta;
    b.oldLayout = from; b.newLayout = to;
    b.srcQueueFamilyIndex = b.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    b.image = img;
    b.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    b.subresourceRange.levelCount = 1;
    b.subresourceRange.layerCount = 1;
    VkDependencyInfo di = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.imageMemoryBarrierCount = 1;
    di.pImageMemoryBarriers = &b;
    vkCmdPipelineBarrier2(cmd, &di);
}

/* D3DSWAPEFFECT_FLIP with several back buffers: the next one becomes back buffer 0 */
static void sc_rotate(struct vkd_swapchain *sc)
{
    if (sc->nback < 2 || sc->pp.SwapEffect != D3DSWAPEFFECT_FLIP) return;
    struct vkd_surface *b0 = sc->back[0];
    struct vkd_image *img = b0->img;
    VkImageView rtv = b0->rtv, rtv_srgb = b0->rtv_srgb;
    uint8_t *shadow = b0->shadow;
    for (UINT i = 0; i + 1 < sc->nback; i++) {
        struct vkd_surface *a = sc->back[i], *b = sc->back[i + 1];
        a->img = b->img; a->rtv = b->rtv; a->rtv_srgb = b->rtv_srgb; a->shadow = b->shadow;
    }
    struct vkd_surface *last = sc->back[sc->nback - 1];
    last->img = img; last->rtv = rtv; last->rtv_srgb = rtv_srgb; last->shadow = shadow;
}

HRESULT vkd_swapchain_present(struct vkd_swapchain *sc, const RECT *src, const RECT *dst, HWND override)
{
    struct vkd_device *d = sc->dev;
    if (override && override != sc->hwnd) {
        /* another window: present there from now on */
        sc_free_vk(sc, 0);
        if (sc->surface) vkDestroySurfaceKHR(vkd.instance, sc->surface, 0);
        sc->surface = 0;
        sc->hwnd = override;
        sc->recreate = 1;
    }
    struct vkd_surface *bb = sc->back[0];
    int shown = !w32.IsIconic(sc->hwnd);
    if (shown && !present_init(d)) return D3DERR_DRIVERINTERNALERROR;
    if (shown) {
        RECT cr;
        w32.GetClientRect(sc->hwnd, &cr);
        UINT cw = (UINT)(cr.right - cr.left), ch = (UINT)(cr.bottom - cr.top);
        if (!cw || !ch) shown = 0;
        else if (sc->recreate || !sc->swapchain || cw != sc->extent.width || ch != sc->extent.height)
            if (!sc_create_vk(sc)) return D3DERR_DRIVERINTERNALERROR;
    }
    uint32_t index = 0;
    VkSemaphore acq = 0;
    if (shown) {
        acq = sc->acquire[sc->acquire_index];
        VkResult r = vkAcquireNextImageKHR(vkd.dev, sc->swapchain, UINT64_MAX, acq, 0, &index);
        if (r == VK_ERROR_OUT_OF_DATE_KHR) {
            if (sc_create_vk(sc)) r = vkAcquireNextImageKHR(vkd.dev, sc->swapchain, UINT64_MAX, acq, 0, &index);
        }
        if (r != VK_SUCCESS && r != VK_SUBOPTIMAL_KHR) {
            vkp_log("vulkan: swap chain acquire failed (%d)", r);
            return D3DERR_DEVICELOST;
        }
        else sc->acquire_index = (sc->acquire_index + 1) % VKD_CONTEXTS;
    }
    if (!shown) {
        vkd_submit(d, 0, 0);
        sc_rotate(sc);
        sc->present_count++;
        return D3D_OK;
    }

    vkd_end_rendering(d);
    if (sc->gamma_on && sc->gamma_dirty) sc_upload_gamma(sc);
    int gamma = sc->gamma_on && sc->gamma;
    VkCommandBuffer cmd = vkd_cmd(d);
    vkd_barrier_all(cmd);
    struct vkd_image *img = bb->img;
    if (sc->resolve) {
        VkImageResolve rs = {0};
        rs.srcSubresource.aspectMask = rs.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        rs.srcSubresource.layerCount = rs.dstSubresource.layerCount = 1;
        rs.extent.width = img->w; rs.extent.height = img->h; rs.extent.depth = 1;
        vkCmdResolveImage(cmd, img->img, VK_IMAGE_LAYOUT_GENERAL, sc->resolve->img, VK_IMAGE_LAYOUT_GENERAL, 1, &rs);
        vkd_barrier_all(cmd);
        img = sc->resolve;
    }
    img->use_seq = d->seq;
    if (gamma) sc->gamma->use_seq = d->seq;

    image_barrier(cmd, sc->images[index], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 0, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
                  VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT);
    VkRenderingAttachmentInfo ca = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    ca.imageView = sc->views[index];
    ca.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    ca.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    ca.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    VkRenderingInfo ri = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea.extent = sc->extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &ca;
    vkCmdBeginRendering(cmd, &ri);

    VkPipeline pipe = present_pipeline(d, sc->sc_format, gamma);
    if (pipe) {
        /* destination rectangle in the window, source rectangle in the back buffer */
        float x0 = 0, y0 = 0, x1 = (float)sc->extent.width, y1 = (float)sc->extent.height;
        if (dst) { x0 = (float)dst->left; y0 = (float)dst->top; x1 = (float)dst->right; y1 = (float)dst->bottom; }
        float u0 = 0, v0 = 0, u1 = 1, v1 = 1;
        if (src) {
            u0 = (float)src->left / (float)img->w; u1 = (float)src->right / (float)img->w;
            v0 = (float)src->top / (float)img->h; v1 = (float)src->bottom / (float)img->h;
        }
        struct vkd_buf ring;
        if (x1 > x0 && y1 > y0 && vkd_ring(d, 3 * 24, 16, &ring)) {
            float *v = (float *)ring.map;
            float du = u1 - u0, dv = v1 - v0;
            float verts[18] = {-1, -1, 0, 1, u0, v0, 3, -1, 0, 1, u0 + 2 * du, v0, -1, 3, 0, 1, u0, v0 + 2 * dv};
            memcpy(v, verts, sizeof verts);
            VkViewport vp = {x0, y0, x1 - x0, y1 - y0, 0, 1};
            VkRect2D sci = {{0, 0}, sc->extent};
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
            vkCmdSetViewport(cmd, 0, 1, &vp);
            vkCmdSetScissor(cmd, 0, 1, &sci);
            VkDescriptorBufferInfo ub[2] = {{d->zero_vb.buf, d->zero_vb.off, sizeof(struct dxso_vs_consts)},
                                            {d->zero_vb.buf, d->zero_vb.off, sizeof(struct dxso_ps_consts)}};
            VkDescriptorImageInfo ii[2] = {{d->present_sampler, img->view, VK_IMAGE_LAYOUT_GENERAL},
                                           {d->lut_sampler, gamma ? sc->gamma->view : img->view, VK_IMAGE_LAYOUT_GENERAL}};
            VkWriteDescriptorSet w[4] = {{VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET},
                                         {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}, {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET}};
            for (int i = 0; i < 4; i++) {
                w[i].dstBinding = (uint32_t)i;
                w[i].descriptorCount = 1;
                if (i < 2) { w[i].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER; w[i].pBufferInfo = &ub[i]; }
                else { w[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER; w[i].pImageInfo = &ii[i - 2]; }
            }
            vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, d->layout, 0, 4, w);
            struct dxso_push push;
            memset(&push, 0, sizeof push);
            push.alpha_func = D3DCMP_ALWAYS;
            push.point_size = push.point_min = push.point_max = 1;
            vkCmdPushConstants(cmd, d->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0,
                               sizeof push, &push);
            VkDeviceSize off = ring.off, size = 3 * 24;
            vkCmdBindVertexBuffers2(cmd, 0, 1, &ring.buf, &off, &size, 0);
            vkCmdDraw(cmd, 3, 1, 0, 0);
        }
    }
    vkCmdEndRendering(cmd);
    image_barrier(cmd, sc->images[index], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                  VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
                  VK_PIPELINE_STAGE_2_NONE, 0);
    VkSemaphore done = sc->done[index];
    vkd_submit(d, acq, done);
    VkPresentInfoKHR pi = {VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    pi.waitSemaphoreCount = 1;
    pi.pWaitSemaphores = &done;
    pi.swapchainCount = 1;
    pi.pSwapchains = &sc->swapchain;
    pi.pImageIndices = &index;
    VkResult r = vkQueuePresentKHR(vkd.queue, &pi);
    if (r == VK_ERROR_OUT_OF_DATE_KHR || r == VK_SUBOPTIMAL_KHR) sc->recreate = 1;
    else if (r != VK_SUCCESS) {
        vkp_log("vulkan: vkQueuePresentKHR failed (%d)", r);
        return D3DERR_DEVICELOST;
    }

    /* frame latency */
    sc->frame_seq[sc->present_count % 16] = d->seq - 1;
    sc->present_count++;
    uint32_t lat = (uint32_t)VKP_MAX(1, VKP_MIN(d->option_latency > 0 ? d->option_latency : 3, 16));
    if (sc->present_count > lat) vkd_wait_seq(d, sc->frame_seq[(sc->present_count - 1 - lat) % 16]);
    sc_rotate(sc);
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE sc_QueryInterface(IDirect3DSwapChain9 *self, REFIID riid, void **out)
{
    if (!out) return E_POINTER;
    if (vkd_guid_eq(riid, &IID_IUnknown) || vkd_guid_eq(riid, &IID_IDirect3DSwapChain9)) {
        *out = self;
        IDirect3DSwapChain9_AddRef(self);
        return S_OK;
    }
    *out = 0;
    return E_NOINTERFACE;
}
static ULONG STDMETHODCALLTYPE sc_AddRef(IDirect3DSwapChain9 *self)
{
    struct vkd_swapchain *sc = (struct vkd_swapchain *)self;
    LONG n = InterlockedIncrement(&sc->ref);
    if (sc->implicit && n == 1) IDirect3DDevice9_AddRef((IDirect3DDevice9 *)sc->dev);
    return (ULONG)n;
}
static ULONG STDMETHODCALLTYPE sc_Release(IDirect3DSwapChain9 *self)
{
    struct vkd_swapchain *sc = (struct vkd_swapchain *)self;
    struct vkd_device *d = sc->dev;
    LONG n = InterlockedDecrement(&sc->ref);
    if (n < 0) { sc->ref = 0; n = 0; }
    if (!n) {
        if (sc->implicit) {
            IDirect3DDevice9_Release((IDirect3DDevice9 *)d);
        } else {
            VKD_LOCK(d);
            for (uint32_t i = 0; i < d->nextra_sc; i++)
                if (d->extra_sc[i] == sc) { d->extra_sc[i] = d->extra_sc[--d->nextra_sc]; break; }
            vkd_swapchain_destroy(sc);
            VKD_UNLOCK(d);
            IDirect3DDevice9_Release((IDirect3DDevice9 *)d);
        }
    }
    return (ULONG)n;
}
static HRESULT STDMETHODCALLTYPE sc_Present(IDirect3DSwapChain9 *self, const RECT *src, const RECT *dst, HWND wnd,
                                            const RGNDATA *dirty, DWORD flags)
{
    struct vkd_swapchain *sc = (struct vkd_swapchain *)self;
    struct vkd_device *d = sc->dev;
    if (d->lost) return D3DERR_DEVICELOST;
    VKD_LOCK(d);
    HRESULT hr = vkd_swapchain_present(sc, src, dst, wnd);
    VKD_UNLOCK(d);
    return hr;
}
static HRESULT STDMETHODCALLTYPE sc_GetFrontBufferData(IDirect3DSwapChain9 *self, IDirect3DSurface9 *dest)
{
    struct vkd_swapchain *sc = (struct vkd_swapchain *)self;
    struct vkd_surface *ds = (struct vkd_surface *)dest, *bb = sc->back[0];
    if (!ds || ds->r.format != D3DFMT_A8R8G8B8 || ds->img || ds->w != bb->w || ds->h != bb->h)
        return D3DERR_INVALIDCALL;
    struct vkd_device *d = sc->dev;
    VKD_LOCK(d);
    UINT pitch;
    uint8_t *cpu = vkd_surface_cpu(ds, &pitch, 0);
    if (cpu) {
        vkd_surface_readback(bb, cpu, pitch);
        if (bb->r.format == D3DFMT_X8R8G8B8)
            for (UINT y = 0; y < ds->h; y++)
                for (UINT x = 0; x < ds->w; x++) cpu[y * pitch + x * 4 + 3] = 0xff;
    }
    VKD_UNLOCK(d);
    return cpu ? D3D_OK : D3DERR_INVALIDCALL;
}
static HRESULT STDMETHODCALLTYPE sc_GetBackBuffer(IDirect3DSwapChain9 *self, UINT i, D3DBACKBUFFER_TYPE type,
                                                  IDirect3DSurface9 **out)
{
    struct vkd_swapchain *sc = (struct vkd_swapchain *)self;
    if (!out) return D3DERR_INVALIDCALL;
    if (i >= sc->nback) { *out = 0; return D3DERR_INVALIDCALL; }
    *out = (IDirect3DSurface9 *)sc->back[i];
    IDirect3DSurface9_AddRef(*out);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE sc_GetRasterStatus(IDirect3DSwapChain9 *self, D3DRASTER_STATUS *rs)
{
    struct vkd_swapchain *sc = (struct vkd_swapchain *)self;
    if (!rs) return D3DERR_INVALIDCALL;
    UINT refresh = sc->pp.FullScreen_RefreshRateInHz ? sc->pp.FullScreen_RefreshRateInHz : 60;
    uint64_t period = 1000000 / refresh, t = vkp_time_us() % period;
    UINT lines = sc->pp.BackBufferHeight * 105 / 100;
    UINT line = (UINT)(t * lines / period);
    rs->InVBlank = line >= sc->pp.BackBufferHeight;
    rs->ScanLine = rs->InVBlank ? 0 : line;
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE sc_GetDisplayMode(IDirect3DSwapChain9 *self, D3DDISPLAYMODE *m)
{
    struct vkd_swapchain *sc = (struct vkd_swapchain *)self;
    if (!m) return D3DERR_INVALIDCALL;
    if (!sc->pp.Windowed) {
        m->Width = sc->pp.BackBufferWidth;
        m->Height = sc->pp.BackBufferHeight;
        m->RefreshRate = sc->pp.FullScreen_RefreshRateInHz;
        m->Format = sc->pp.BackBufferFormat == D3DFMT_A8R8G8B8 ? D3DFMT_X8R8G8B8 : sc->pp.BackBufferFormat;
    } else {
        vkd_desktop_mode(m);
    }
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE sc_GetDevice(IDirect3DSwapChain9 *self, IDirect3DDevice9 **out)
{
    if (!out) return D3DERR_INVALIDCALL;
    *out = (IDirect3DDevice9 *)((struct vkd_swapchain *)self)->dev;
    IDirect3DDevice9_AddRef(*out);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE sc_GetPresentParameters(IDirect3DSwapChain9 *self, D3DPRESENT_PARAMETERS *pp)
{
    if (!pp) return D3DERR_INVALIDCALL;
    *pp = ((struct vkd_swapchain *)self)->pp;
    return D3D_OK;
}

const IDirect3DSwapChain9Vtbl vkd_sc_vtbl = {
    .QueryInterface = sc_QueryInterface,
    .AddRef = sc_AddRef,
    .Release = sc_Release,
    .Present = sc_Present,
    .GetFrontBufferData = sc_GetFrontBufferData,
    .GetBackBuffer = sc_GetBackBuffer,
    .GetRasterStatus = sc_GetRasterStatus,
    .GetDisplayMode = sc_GetDisplayMode,
    .GetDevice = sc_GetDevice,
    .GetPresentParameters = sc_GetPresentParameters,
};
