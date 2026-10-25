/* Remaining COM methods. Included after the recording helpers. */
static HRESULT device_unsupported(const char *name)
{
    vkp_log("vulkan: unsupported optional device operation %s", name);
    return D3DERR_INVALIDCALL;
}
#define CREATE_TEXTURE(name, iface, type, width, height, depth)                                                        \
    static HRESULT STDMETHODCALLTYPE dev_##name(IDirect3DDevice9 *self, UINT w, UINT h, UINT dep, UINT levels,         \
                                                DWORD usage, D3DFORMAT f, D3DPOOL pool, iface **out, HANDLE *shared)

static HRESULT device_texture(IDirect3DDevice9 *self, D3DRESOURCETYPE type, UINT w, UINT h, UINT depth, UINT levels,
                              DWORD usage, D3DFORMAT f, D3DPOOL pool, void *out, HANDLE *shared)
{
    if (!out || shared)
        return D3DERR_INVALIDCALL;

    struct vkd_device *d = (struct vkd_device *)self;

    VKD_LOCK(d);

    HRESULT hr = vkd_create_texture(d, type, w, h, depth, levels, usage, f, pool, out);

    if (SUCCEEDED(hr))
        IDirect3DDevice9_AddRef(self);

    VKD_UNLOCK(d);
    return hr;
}
static HRESULT STDMETHODCALLTYPE dev_CreateTexture(IDirect3DDevice9 *s, UINT w, UINT h, UINT levels, DWORD usage,
                                                   D3DFORMAT f, D3DPOOL pool, IDirect3DTexture9 **out, HANDLE *shared)
{
    return device_texture(s, D3DRTYPE_TEXTURE, w, h, 1, levels, usage, f, pool, out, shared);
}
static HRESULT STDMETHODCALLTYPE dev_CreateCubeTexture(IDirect3DDevice9 *s, UINT w, UINT levels, DWORD usage,
                                                       D3DFORMAT f, D3DPOOL pool, IDirect3DCubeTexture9 **out,
                                                       HANDLE *shared)
{
    return device_texture(s, D3DRTYPE_CUBETEXTURE, w, w, 1, levels, usage, f, pool, out, shared);
}
static HRESULT STDMETHODCALLTYPE dev_CreateVolumeTexture(IDirect3DDevice9 *s, UINT w, UINT h, UINT depth, UINT levels,
                                                         DWORD usage, D3DFORMAT f, D3DPOOL pool,
                                                         IDirect3DVolumeTexture9 **out, HANDLE *shared)
{
    return device_texture(s, D3DRTYPE_VOLUMETEXTURE, w, h, depth, levels, usage, f, pool, out, shared);
}
static HRESULT device_buffer(IDirect3DDevice9 *self, UINT size, DWORD usage, DWORD fvf, D3DFORMAT f, D3DPOOL pool,
                             int index, void *out, HANDLE *shared)
{
    if (!out || shared)
        return D3DERR_INVALIDCALL;

    struct vkd_device *d = (struct vkd_device *)self;

    VKD_LOCK(d);

    HRESULT hr = vkd_create_buffer(d, size, usage, fvf, f, pool, index, out);

    if (SUCCEEDED(hr))
        IDirect3DDevice9_AddRef(self);

    VKD_UNLOCK(d);
    return hr;
}
static HRESULT STDMETHODCALLTYPE dev_CreateVertexBuffer(IDirect3DDevice9 *s, UINT n, DWORD u, DWORD fvf, D3DPOOL p,
                                                        IDirect3DVertexBuffer9 **out, HANDLE *shared)
{
    return device_buffer(s, n, u, fvf, D3DFMT_VERTEXDATA, p, 0, out, shared);
}
static HRESULT STDMETHODCALLTYPE dev_CreateIndexBuffer(IDirect3DDevice9 *s, UINT n, DWORD u, D3DFORMAT f, D3DPOOL p,
                                                       IDirect3DIndexBuffer9 **out, HANDLE *shared)
{
    return device_buffer(s, n, u, 0, f, p, 1, out, shared);
}
static HRESULT device_surface(IDirect3DDevice9 *self, UINT w, UINT h, D3DFORMAT f, D3DMULTISAMPLE_TYPE ms, DWORD q,
                              DWORD usage, int lockable, int discard, void *out, HANDLE *shared)
{
    if (!out || shared)
        return D3DERR_INVALIDCALL;

    struct vkd_device *d = (struct vkd_device *)self;

    VKD_LOCK(d);

    HRESULT hr = vkd_create_surface(d, w, h, f, ms, q, usage, D3DPOOL_DEFAULT, lockable, discard, out);

    if (SUCCEEDED(hr))
        IDirect3DDevice9_AddRef(self);

    VKD_UNLOCK(d);
    return hr;
}
static HRESULT STDMETHODCALLTYPE dev_CreateRenderTarget(IDirect3DDevice9 *s, UINT w, UINT h, D3DFORMAT f,
                                                        D3DMULTISAMPLE_TYPE ms, DWORD q, BOOL lockable,
                                                        IDirect3DSurface9 **out, HANDLE *shared)
{
    return device_surface(s, w, h, f, ms, q, D3DUSAGE_RENDERTARGET, lockable, 0, out, shared);
}
static HRESULT STDMETHODCALLTYPE dev_CreateDepthStencilSurface(IDirect3DDevice9 *s, UINT w, UINT h, D3DFORMAT f,
                                                               D3DMULTISAMPLE_TYPE ms, DWORD q, BOOL discard,
                                                               IDirect3DSurface9 **out, HANDLE *shared)
{
    return device_surface(s, w, h, f, ms, q, D3DUSAGE_DEPTHSTENCIL, f == D3DFMT_D16_LOCKABLE, discard, out, shared);
}

static HRESULT STDMETHODCALLTYPE dev_SetTexture(IDirect3DDevice9 *self, DWORD stage, IDirect3DBaseTexture9 *tex)
{
    struct vkd_device *d = (struct vkd_device *)self;
    int i = sampler_index(stage);

    struct vkd_res *r = (struct vkd_res *)tex;

    if (i < 0 ||
        (r && (r->dev != d || !vkd_res_texture(r) || r->pool == D3DPOOL_SYSTEMMEM || r->pool == D3DPOOL_SCRATCH)))
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    vkd_ref_tex(&STATE(d)->tex[i], r);

    if (d->recording)
        d->recording->m.tex |= 1u << i;

    else
        d->dirty |= DIRTY_SAMPLERS | DIRTY_SHADERS | DIRTY_FF_PS;

    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetTexture(IDirect3DDevice9 *self, DWORD stage, IDirect3DBaseTexture9 **out)
{
    int i = sampler_index(stage);
    if (i < 0 || !out)
        return D3DERR_INVALIDCALL;

    *out = (IDirect3DBaseTexture9 *)((struct vkd_device *)self)->st.tex[i];

    if (*out)
        IDirect3DBaseTexture9_AddRef(*out);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetStreamSource(IDirect3DDevice9 *self, UINT i, IDirect3DVertexBuffer9 *vb,
                                                     UINT off, UINT stride)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_buffer *b = (struct vkd_buffer *)vb;

    if (i >= 16 || stride > d->caps.MaxStreamStride || (b && (b->r.dev != d || off > b->size)))
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    struct vkd_state *s = STATE(d);
    vkd_ref_buf(&s->vb[i], b);
    s->vb_off[i] = off;
    s->vb_stride[i] = stride;

    if (d->recording)
        d->recording->m.vb |= 1u << i;
    else
        d->dirty |= DIRTY_VB | DIRTY_INPUT;

    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetStreamSource(IDirect3DDevice9 *self, UINT i, IDirect3DVertexBuffer9 **out,
                                                     UINT *off, UINT *stride)
{
    if (i >= 16 || !out || !off || !stride)
        return D3DERR_INVALIDCALL;

    struct vkd_state *s = &((struct vkd_device *)self)->st;

    *out = (IDirect3DVertexBuffer9 *)s->vb[i];
    *off = s->vb_off[i];
    *stride = s->vb_stride[i];

    if (*out)
        IDirect3DVertexBuffer9_AddRef(*out);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetStreamSourceFreq(IDirect3DDevice9 *self, UINT i, UINT f)
{
    struct vkd_device *d = (struct vkd_device *)self;

    if (i >= 16 || !f || (f & 0xc0000000u) == 0xc0000000u || (!i && (f & D3DSTREAMSOURCE_INSTANCEDATA)))
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    STATE(d)->vb_freq[i] = f;

    if (d->recording)
        d->recording->m.vb_freq |= 1u << i;
    else
        d->dirty |= DIRTY_INPUT;

    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetStreamSourceFreq(IDirect3DDevice9 *self, UINT i, UINT *f)
{
    if (i >= 16 || !f)
        return D3DERR_INVALIDCALL;
    *f = ((struct vkd_device *)self)->st.vb_freq[i];
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetIndices(IDirect3DDevice9 *self, IDirect3DIndexBuffer9 *ib)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_buffer *b = (struct vkd_buffer *)ib;

    if (b && b->r.dev != d)
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    vkd_ref_buf(&STATE(d)->ib, b);

    if (d->recording)
        d->recording->m.misc |= VKD_SB_IB;
    else
        d->dirty |= DIRTY_IB;

    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetIndices(IDirect3DDevice9 *self, IDirect3DIndexBuffer9 **out)
{
    if (!out)
        return D3DERR_INVALIDCALL;
    *out = (IDirect3DIndexBuffer9 *)((struct vkd_device *)self)->st.ib;

    if (*out)
        IDirect3DIndexBuffer9_AddRef(*out);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetVertexDeclaration(IDirect3DDevice9 *self, IDirect3DVertexDeclaration9 *decl)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_vdecl *v = (struct vkd_vdecl *)decl;

    if (v && v->dev != d)
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    vkd_ref_decl(&STATE(d)->decl, v);
    STATE(d)->fvf = v ? v->fvf : 0;

    if (d->recording)
        d->recording->m.misc |= VKD_SB_DECL;
    else
        d->dirty |= DIRTY_INPUT | DIRTY_FF_VS | DIRTY_FF_VS_CONST;

    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetVertexDeclaration(IDirect3DDevice9 *self, IDirect3DVertexDeclaration9 **out)
{
    if (!out)
        return D3DERR_INVALIDCALL;
    *out = (IDirect3DVertexDeclaration9 *)((struct vkd_device *)self)->st.decl;

    if (*out)
        IDirect3DVertexDeclaration9_AddRef(*out);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetFVF(IDirect3DDevice9 *self, DWORD fvf)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!fvf)
        return D3D_OK;

    VKD_LOCK(d);
    struct vkd_vdecl *v = vkd_fvf_decl(d, fvf);

    if (!v) {
        VKD_UNLOCK(d);
        return E_OUTOFMEMORY;
    }
    vkd_ref_decl(&STATE(d)->decl, v);
    STATE(d)->fvf = fvf;

    if (d->recording)
        d->recording->m.misc |= VKD_SB_DECL;
    else
        d->dirty |= DIRTY_INPUT | DIRTY_FF_VS | DIRTY_FF_VS_CONST;

    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetFVF(IDirect3DDevice9 *self, DWORD *out)
{
    if (!out)
        return D3DERR_INVALIDCALL;
    *out = ((struct vkd_device *)self)->st.fvf;
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_CreateVertexDeclaration(IDirect3DDevice9 *self, const D3DVERTEXELEMENT9 *el,
                                                             IDirect3DVertexDeclaration9 **out)
{
    if (!out)
        return D3DERR_INVALIDCALL;
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);

    HRESULT hr = vkd_create_vdecl(d, el, (struct vkd_vdecl **)out);
    if (SUCCEEDED(hr))
        IDirect3DDevice9_AddRef(self);
    VKD_UNLOCK(d);
    return hr;
}
#define CREATE_SHADER(prefix, iface, ps)                                                                               \
    static HRESULT STDMETHODCALLTYPE dev_Create##prefix##Shader(IDirect3DDevice9 *self, const DWORD *code,             \
                                                                iface **out)                                           \
    {                                                                                                                  \
        if (!out)                                                                                                      \
            return D3DERR_INVALIDCALL;                                                                                 \
        struct vkd_device *d = (struct vkd_device *)self;                                                              \
        VKD_LOCK(d);                                                                                                   \
        HRESULT hr = vkd_create_shader(d, code, ps, (struct vkd_shader **)out);                                        \
        if (SUCCEEDED(hr))                                                                                             \
            IDirect3DDevice9_AddRef(self);                                                                             \
        VKD_UNLOCK(d);                                                                                                 \
        return hr;                                                                                                     \
    }
CREATE_SHADER(Vertex, IDirect3DVertexShader9, 0)
CREATE_SHADER(Pixel, IDirect3DPixelShader9, 1)
static HRESULT STDMETHODCALLTYPE dev_CreateQuery(IDirect3DDevice9 *self, D3DQUERYTYPE type, IDirect3DQuery9 **out)
{
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);

    HRESULT hr = vkd_create_query(d, type, (struct vkd_query **)out);
    if (SUCCEEDED(hr) && out)
        IDirect3DDevice9_AddRef(self);
    VKD_UNLOCK(d);
    return hr;
}

static HRESULT STDMETHODCALLTYPE dev_SetRenderTarget(IDirect3DDevice9 *self, DWORD i, IDirect3DSurface9 *surface)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_surface *s = (struct vkd_surface *)surface;

    if (i >= 4 || (!i && !s) || (s && (s->r.dev != d || !(s->r.usage & D3DUSAGE_RENDERTARGET))))
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    vkd_end_rendering(d);

    if (s)
        vkd_res_bind(&s->r);
    if (d->rt[i])
        vkd_res_unbind(&d->rt[i]->r);
    d->rt[i] = s;

    if (!i) {
        d->st.viewport = (D3DVIEWPORT9){0, 0, s->w, s->h, 0, 1};
        d->st.scissor = (RECT){0, 0, s->w, s->h};
    }
    d->dirty = DIRTY_ALL;
    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetRenderTarget(IDirect3DDevice9 *self, DWORD i, IDirect3DSurface9 **out)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (i >= 4 || !out)
        return D3DERR_INVALIDCALL;

    *out = (IDirect3DSurface9 *)d->rt[i];
    if (!*out)
        return D3DERR_NOTFOUND;
    IDirect3DSurface9_AddRef(*out);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetDepthStencilSurface(IDirect3DDevice9 *self, IDirect3DSurface9 *surface)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_surface *s = (struct vkd_surface *)surface;

    if (s && (s->r.dev != d || !(s->r.usage & D3DUSAGE_DEPTHSTENCIL)))
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    vkd_end_rendering(d);
    if (s)
        vkd_res_bind(&s->r);
    if (d->ds)
        vkd_res_unbind(&d->ds->r);
    d->ds = s;

    d->dirty = DIRTY_ALL;
    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetDepthStencilSurface(IDirect3DDevice9 *self, IDirect3DSurface9 **out)
{
    if (!out)
        return D3DERR_INVALIDCALL;
    *out = (IDirect3DSurface9 *)((struct vkd_device *)self)->ds;

    if (!*out)
        return D3DERR_NOTFOUND;
    IDirect3DSurface9_AddRef(*out);
    return D3D_OK;
}

static D3DMATRIX *device_matrix(struct vkd_state *s, D3DTRANSFORMSTATETYPE t)
{
    if (t == D3DTS_VIEW)
        return &s->view;
    if (t == D3DTS_PROJECTION)
        return &s->proj;

    if ((UINT)t >= D3DTS_TEXTURE0 && (UINT)t <= D3DTS_TEXTURE7)
        return &s->texm[t - D3DTS_TEXTURE0];

    if ((UINT)t >= D3DTS_WORLD && (UINT)t <= D3DTS_WORLD + 255)
        return &s->world[t - D3DTS_WORLD];
    return 0;
}
static void matrix_mul(D3DMATRIX *out, const D3DMATRIX *a, const D3DMATRIX *b)
{
    D3DMATRIX m;
    for (UINT i = 0; i < 4; i++)
        for (UINT j = 0; j < 4; j++) {
            m.m[i][j] = 0;
            for (UINT k = 0; k < 4; k++)
                m.m[i][j] += a->m[i][k] * b->m[k][j];
        }
    *out = m;
}
static HRESULT STDMETHODCALLTYPE dev_SetTransform(IDirect3DDevice9 *self, D3DTRANSFORMSTATETYPE t, const D3DMATRIX *m)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!m)
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    D3DMATRIX *dst = device_matrix(STATE(d), t);
    if (!dst) {
        VKD_UNLOCK(d);
        return D3DERR_INVALIDCALL;
    }
    *dst = *m;

    if (d->recording) {
        if (t == D3DTS_VIEW)
            d->recording->m.misc |= VKD_SB_VIEW;

        else if (t == D3DTS_PROJECTION)
            d->recording->m.misc |= VKD_SB_PROJ;

        else if ((UINT)t < D3DTS_WORLD)
            d->recording->m.texm |= 1u << (t - D3DTS_TEXTURE0);

        else
            MASK_BIT(d->recording->m.world, t - D3DTS_WORLD);
    } else
        d->dirty |= DIRTY_FF_VS_CONST | DIRTY_PUSH;

    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetTransform(IDirect3DDevice9 *self, D3DTRANSFORMSTATETYPE t, D3DMATRIX *out)
{
    D3DMATRIX *m = device_matrix(&((struct vkd_device *)self)->st, t);
    if (!m || !out)
        return D3DERR_INVALIDCALL;
    *out = *m;
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_MultiplyTransform(IDirect3DDevice9 *self, D3DTRANSFORMSTATETYPE t,
                                                       const D3DMATRIX *m)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!m)
        return D3DERR_INVALIDCALL;
    VKD_LOCK(d);

    D3DMATRIX *old = device_matrix(STATE(d), t), result;
    if (!old) {
        VKD_UNLOCK(d);
        return D3DERR_INVALIDCALL;
    }
    matrix_mul(&result, m, old);
    HRESULT hr = dev_SetTransform(self, t, &result);
    VKD_UNLOCK(d);
    return hr;
}
static HRESULT STDMETHODCALLTYPE dev_SetMaterial(IDirect3DDevice9 *self, const D3DMATERIAL9 *m)
{
    if (!m)
        return D3DERR_INVALIDCALL;
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);
    STATE(d)->material = *m;

    if (d->recording)
        d->recording->m.misc |= VKD_SB_MATERIAL;
    else
        d->dirty |= DIRTY_FF_VS_CONST;
    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetMaterial(IDirect3DDevice9 *self, D3DMATERIAL9 *m)
{
    if (!m)
        return D3DERR_INVALIDCALL;
    *m = ((struct vkd_device *)self)->st.material;
    return D3D_OK;
}
static int record_light(struct vkd_device *d, DWORD i)
{
    if (!d->recording) {
        vkd_apply_state_all(d);
        return 1;
    }
    struct vkd_mask *m = &d->recording->m;

    if (i >= m->nlight) {
        uint8_t *p = vkp_realloc(m->light, (size_t)i + 1);
        if (!p)
            return 0;

        memset(p + m->nlight, 0, i + 1 - m->nlight);
        m->light = p;
        m->nlight = i + 1;
    }
    m->light[i] = 1;
    return 1;
}
static HRESULT STDMETHODCALLTYPE dev_SetLight(IDirect3DDevice9 *self, DWORD i, const D3DLIGHT9 *l)
{
    if (!l || l->Type < D3DLIGHT_POINT || l->Type > D3DLIGHT_DIRECTIONAL || l->Attenuation0 < 0 ||
        l->Attenuation1 < 0 || l->Attenuation2 < 0)
        return D3DERR_INVALIDCALL;

    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);
    struct vkd_light *p = vkd_light_slot(STATE(d), i);

    if (!p) {
        VKD_UNLOCK(d);
        return E_OUTOFMEMORY;
    }
    p->l = *l;
    p->set = 1;

    int ok = record_light(d, i);
    VKD_UNLOCK(d);
    return ok ? D3D_OK : E_OUTOFMEMORY;
}
static HRESULT STDMETHODCALLTYPE dev_GetLight(IDirect3DDevice9 *self, DWORD i, D3DLIGHT9 *l)
{
    struct vkd_state *s = &((struct vkd_device *)self)->st;
    if (!l || i >= s->nlights || !s->lights[i].set)
        return D3DERR_INVALIDCALL;
    *l = s->lights[i].l;
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_LightEnable(IDirect3DDevice9 *self, DWORD i, BOOL on)
{
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);
    struct vkd_light *p = vkd_light_slot(STATE(d), i);

    if (!p) {
        VKD_UNLOCK(d);
        return E_OUTOFMEMORY;
    }
    if (!p->set) {
        p->l.Type = D3DLIGHT_DIRECTIONAL;
        p->l.Diffuse = (D3DCOLORVALUE){1, 1, 1, 0};
        p->l.Direction.z = 1;
        p->set = 1;
    }
    p->on = !!on;
    int ok = record_light(d, i);
    VKD_UNLOCK(d);
    return ok ? D3D_OK : E_OUTOFMEMORY;
}
static HRESULT STDMETHODCALLTYPE dev_GetLightEnable(IDirect3DDevice9 *self, DWORD i, BOOL *on)
{
    struct vkd_state *s = &((struct vkd_device *)self)->st;
    if (!on || i >= s->nlights || !s->lights[i].set)
        return D3DERR_INVALIDCALL;
    *on = s->lights[i].on ? 128 : 0;
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetClipPlane(IDirect3DDevice9 *self, DWORD i, const float *p)
{
    if (i >= 6 || !p)
        return D3DERR_INVALIDCALL;
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);
    memcpy(STATE(d)->clip[i], p, 16);

    if (d->recording)
        d->recording->m.misc |= VKD_SB_CLIP;
    else
        d->dirty |= DIRTY_PUSH;
    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetClipPlane(IDirect3DDevice9 *self, DWORD i, float *p)
{
    if (i >= 6 || !p)
        return D3DERR_INVALIDCALL;
    memcpy(p, ((struct vkd_device *)self)->st.clip[i], 16);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetScissorRect(IDirect3DDevice9 *self, const RECT *r)
{
    if (!r)
        return D3DERR_INVALIDCALL;
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);
    STATE(d)->scissor = *r;

    if (d->recording)
        d->recording->m.misc |= VKD_SB_SCISSOR;
    else
        d->dirty |= DIRTY_VIEWPORT;
    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetScissorRect(IDirect3DDevice9 *self, RECT *r)
{
    if (!r)
        return D3DERR_INVALIDCALL;
    *r = ((struct vkd_device *)self)->st.scissor;
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetClipStatus(IDirect3DDevice9 *self, const D3DCLIPSTATUS9 *p)
{
    if (!p)
        return D3DERR_INVALIDCALL;
    ((struct vkd_device *)self)->clip_status = *p;
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetClipStatus(IDirect3DDevice9 *self, D3DCLIPSTATUS9 *p)
{
    if (!p)
        return D3DERR_INVALIDCALL;
    *p = ((struct vkd_device *)self)->clip_status;
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetSoftwareVertexProcessing(IDirect3DDevice9 *self, BOOL on)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!(d->cp.BehaviorFlags & D3DCREATE_MIXED_VERTEXPROCESSING))
        return D3DERR_INVALIDCALL;
    d->sw_vp = !!on;
    return D3D_OK;
}
static BOOL STDMETHODCALLTYPE dev_GetSoftwareVertexProcessing(IDirect3DDevice9 *self)
{
    return ((struct vkd_device *)self)->sw_vp;
}
static HRESULT STDMETHODCALLTYPE dev_SetNPatchMode(IDirect3DDevice9 *self, float n)
{
    if (n > 0)
        return D3DERR_INVALIDCALL;
    struct vkd_device *d = (struct vkd_device *)self;
    STATE(d)->npatch = n;

    if (d->recording)
        d->recording->m.misc |= VKD_SB_NPATCH;
    return D3D_OK;
}
static float STDMETHODCALLTYPE dev_GetNPatchMode(IDirect3DDevice9 *self)
{
    return ((struct vkd_device *)self)->st.npatch;
}
static HRESULT STDMETHODCALLTYPE dev_ValidateDevice(IDirect3DDevice9 *self, DWORD *passes)
{
    if (!passes)
        return D3DERR_INVALIDCALL;
    *passes = 1;
    return D3D_OK;
}
static UINT STDMETHODCALLTYPE dev_GetAvailableTextureMem(IDirect3DDevice9 *self)
{
    VkDeviceSize n = 0;
    for (UINT i = 0; i < vkd.memprops.memoryHeapCount; i++)
        if (vkd.memprops.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)
            n += vkd.memprops.memoryHeaps[i].size;

    return n > 0xfff00000u ? 0xfff00000u : (UINT)n;
}
static HRESULT STDMETHODCALLTYPE dev_EvictManagedResources(IDirect3DDevice9 *self)
{
    vkd_flush_wait((struct vkd_device *)self);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetDisplayMode(IDirect3DDevice9 *self, UINT i, D3DDISPLAYMODE *m)
{
    if (i || !m)
        return D3DERR_INVALIDCALL;
    vkd_desktop_mode(m);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetSwapChain(IDirect3DDevice9 *self, UINT i, IDirect3DSwapChain9 **out)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!out || i > d->nextra_sc)
        return D3DERR_INVALIDCALL;

    *out = (IDirect3DSwapChain9 *)(i ? d->extra_sc[i - 1] : d->swapchain);
    IDirect3DSwapChain9_AddRef(*out);
    return D3D_OK;
}
static UINT STDMETHODCALLTYPE dev_GetNumberOfSwapChains(IDirect3DDevice9 *self)
{
    return 1 + ((struct vkd_device *)self)->nextra_sc;
}
static HRESULT STDMETHODCALLTYPE dev_CreateAdditionalSwapChain(IDirect3DDevice9 *self, D3DPRESENT_PARAMETERS *pp,
                                                               IDirect3DSwapChain9 **out)
{
    if (!pp || !out || !pp->Windowed)
        return D3DERR_INVALIDCALL;
    *out = 0;
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);
    struct vkd_swapchain **list = vkp_realloc(d->extra_sc, (d->nextra_sc + 1) * sizeof *list);
    if (!list) {
        VKD_UNLOCK(d);
        return E_OUTOFMEMORY;
    }
    d->extra_sc = list;
    HRESULT hr = vkd_swapchain_create(d, pp, 0, (struct vkd_swapchain **)out);
    if (SUCCEEDED(hr)) {
        d->extra_sc[d->nextra_sc++] = (struct vkd_swapchain *)*out;
        IDirect3DDevice9_AddRef(self);
    }
    VKD_UNLOCK(d);
    return hr;
}
static HRESULT STDMETHODCALLTYPE dev_GetRasterStatus(IDirect3DDevice9 *self, UINT i, D3DRASTER_STATUS *out)
{
    if (i || !out)
        return D3DERR_INVALIDCALL;
    return IDirect3DSwapChain9_GetRasterStatus((IDirect3DSwapChain9 *)((struct vkd_device *)self)->swapchain, out);
}
static HRESULT STDMETHODCALLTYPE dev_GetFrontBufferData(IDirect3DDevice9 *self, UINT i, IDirect3DSurface9 *dst)
{
    if (i)
        return D3DERR_INVALIDCALL;
    return IDirect3DSwapChain9_GetFrontBufferData((IDirect3DSwapChain9 *)((struct vkd_device *)self)->swapchain, dst);
}
static HRESULT STDMETHODCALLTYPE dev_SetDialogBoxMode(IDirect3DDevice9 *self, BOOL enable)
{
    return device_unsupported("SetDialogBoxMode");
}
static HRESULT STDMETHODCALLTYPE dev_SetCursorProperties(IDirect3DDevice9 *self, UINT x, UINT y, IDirect3DSurface9 *s)
{
    return device_unsupported("SetCursorProperties");
}
static void STDMETHODCALLTYPE dev_SetCursorPosition(IDirect3DDevice9 *self, int x, int y, DWORD flags) { (void)self; }
static BOOL STDMETHODCALLTYPE dev_ShowCursor(IDirect3DDevice9 *self, BOOL show)
{
    struct vkd_device *d = (struct vkd_device *)self;
    BOOL old = d->cursor_shown;
    d->cursor_shown = !!show;
    return old;
}
static HRESULT STDMETHODCALLTYPE dev_SetPaletteEntries(IDirect3DDevice9 *self, UINT i, const PALETTEENTRY *p)
{
    return device_unsupported("SetPaletteEntries");
}
static HRESULT STDMETHODCALLTYPE dev_GetPaletteEntries(IDirect3DDevice9 *self, UINT i, PALETTEENTRY *p)
{
    return device_unsupported("GetPaletteEntries");
}
static HRESULT STDMETHODCALLTYPE dev_SetCurrentTexturePalette(IDirect3DDevice9 *self, UINT i)
{
    return device_unsupported("SetCurrentTexturePalette");
}
static HRESULT STDMETHODCALLTYPE dev_GetCurrentTexturePalette(IDirect3DDevice9 *self, UINT *i)
{
    return device_unsupported("GetCurrentTexturePalette");
}
static HRESULT STDMETHODCALLTYPE dev_ProcessVertices(IDirect3DDevice9 *s, UINT a, UINT b, UINT n,
                                                     IDirect3DVertexBuffer9 *vb, IDirect3DVertexDeclaration9 *v,
                                                     DWORD f)
{
    return device_unsupported("ProcessVertices");
}
static HRESULT STDMETHODCALLTYPE dev_DrawRectPatch(IDirect3DDevice9 *s, UINT h, const float *n,
                                                   const D3DRECTPATCH_INFO *p)
{
    return device_unsupported("DrawRectPatch");
}
static HRESULT STDMETHODCALLTYPE dev_DrawTriPatch(IDirect3DDevice9 *s, UINT h, const float *n,
                                                  const D3DTRIPATCH_INFO *p)
{
    return device_unsupported("DrawTriPatch");
}
static HRESULT STDMETHODCALLTYPE dev_DeletePatch(IDirect3DDevice9 *s, UINT h)
{
    return device_unsupported("DeletePatch");
}

static HRESULT STDMETHODCALLTYPE dev_UpdateSurface(IDirect3DDevice9 *self, IDirect3DSurface9 *src, const RECT *rect,
                                                   IDirect3DSurface9 *dst, const POINT *point)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_surface *s = (struct vkd_surface *)src, *t = (struct vkd_surface *)dst;

    if (!s || !t || s->r.dev != d || t->r.dev != d || s->r.pool != D3DPOOL_SYSTEMMEM || t->r.pool != D3DPOOL_DEFAULT ||
        s->r.format != t->r.format || s->locked || t->locked || s->ms || t->ms)
        return D3DERR_INVALIDCALL;

    RECT r = rect ? *rect : (RECT){0, 0, s->w, s->h};
    POINT p = point ? *point : (POINT){0, 0};

    if (r.left < 0 || r.top < 0 || r.right < r.left || r.bottom < r.top || r.right > (LONG)s->w ||
        r.bottom > (LONG)s->h || p.x < 0 || p.y < 0 || p.x + r.right - r.left > (LONG)t->w ||
        p.y + r.bottom - r.top > (LONG)t->h)
        return D3DERR_INVALIDCALL;

    const struct vkd_format *f = vkd_format(s->r.format);
    if (r.left % f->block || r.top % f->block || p.x % f->block || p.y % f->block)
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    UINT pitch;
    uint8_t *cpu = vkd_surface_cpu(s, &pitch, 0);
    if (!cpu) {
        VKD_UNLOCK(d);
        return E_OUTOFMEMORY;
    }
    cpu += (r.top / f->block) * pitch + (r.left / f->block) * f->bytes;

    struct vkd_image *img = vkd_surface_image(t);
    if (!img) {
        VKD_UNLOCK(d);
        return D3DERR_INVALIDCALL;
    }
    vkd_image_write(d, img, t->level, t->layer, p.x, p.y, 0, r.right - r.left, r.bottom - r.top, 1, cpu, pitch, 0);

    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_UpdateTexture(IDirect3DDevice9 *self, IDirect3DBaseTexture9 *src,
                                                   IDirect3DBaseTexture9 *dst)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_texture *s = vkd_res_texture((struct vkd_res *)src), *t = vkd_res_texture((struct vkd_res *)dst);

    if (!s || !t || s->r.dev != d || t->r.dev != d || s->r.pool != D3DPOOL_SYSTEMMEM || t->r.pool != D3DPOOL_DEFAULT ||
        s->r.format != t->r.format || s->r.type != t->r.type)
        return D3DERR_INVALIDCALL;

    UINT base = 0;
    while (base < s->levels &&
           (VKP_MAX(1, s->w >> base) != t->w || VKP_MAX(1, s->h >> base) != t->h || VKP_MAX(1, s->d >> base) != t->d))
        base++;

    if (base == s->levels || s->levels - base < t->levels)
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);

    for (UINT face = 0; face < t->faces; face++)
        for (UINT level = 0; level < t->levels; level++) {
            UINT w = VKP_MAX(1, t->w >> level), h = VKP_MAX(1, t->h >> level), dep = VKP_MAX(1, t->d >> level), pitch,
                 slice;

            vkd_layout(s->fmt, w, h, dep, &pitch, &slice, 0);

            const uint8_t *cpu = s->shadow + s->sub_off[face * s->levels + level + base];

            vkd_image_write(d, t->img, level, face, 0, 0, 0, w, h, dep, cpu, pitch, slice);
        }
    s->dirty = 0;
    t->autogen_dirty = t->autogen;
    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_ColorFill(IDirect3DDevice9 *self, IDirect3DSurface9 *surface, const RECT *rect,
                                               D3DCOLOR color)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_surface *s = (struct vkd_surface *)surface;

    if (!s || s->r.dev != d || s->r.pool != D3DPOOL_DEFAULT || vkd_format(s->r.format)->depth || s->locked)
        return D3DERR_INVALIDCALL;

    RECT r = rect ? *rect : (RECT){0, 0, s->w, s->h};
    if (r.left < 0 || r.top < 0 || r.right < r.left || r.bottom < r.top || r.right > (LONG)s->w ||
        r.bottom > (LONG)s->h)
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    vkd_end_rendering(d);
    VkCommandBuffer cmd = vkd_cmd(d);
    struct vkd_image *img = vkd_surface_image(s);

    if (!img) {
        VKD_UNLOCK(d);
        return D3DERR_INVALIDCALL;
    }
    if (!img->attachable) {
        if (!img->fmt || img->fmt->block != 1 || img->samples != VK_SAMPLE_COUNT_1_BIT) {
            VKD_UNLOCK(d);
            return D3DERR_INVALIDCALL;
        }
        VkClearColorValue value;
        value.float32[0] = ((color >> 16) & 255) / 255.0f;
        value.float32[1] = ((color >> 8) & 255) / 255.0f;
        value.float32[2] = (color & 255) / 255.0f;
        value.float32[3] = (color >> 24) / 255.0f;
        VkImageSubresourceRange range = {VK_IMAGE_ASPECT_COLOR_BIT, s->level, 1, s->layer, 1};
        vkd_barrier_all(cmd);
        if (r.left == 0 && r.top == 0 && r.right == (LONG)s->w && r.bottom == (LONG)s->h) {
            vkCmdClearColorImage(cmd, img->img, VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
        } else if (r.left < r.right && r.top < r.bottom) {
            struct vkd_image *tmp = vkd_image_create(d, img->fmt, VK_IMAGE_TYPE_2D, r.right - r.left, r.bottom - r.top,
                                                     1, 1, 1, VK_SAMPLE_COUNT_1_BIT, 0, 0);
            if (!tmp) {
                VKD_UNLOCK(d);
                return D3DERR_OUTOFVIDEOMEMORY;
            }
            range.baseMipLevel = range.baseArrayLayer = 0;
            vkCmdClearColorImage(cmd, tmp->img, VK_IMAGE_LAYOUT_GENERAL, &value, 1, &range);
            vkd_barrier_all(cmd);
            VkImageCopy copy = {{VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1},
                                {0, 0, 0},
                                {VK_IMAGE_ASPECT_COLOR_BIT, s->level, s->layer, 1},
                                {r.left, r.top, 0},
                                {r.right - r.left, r.bottom - r.top, 1}};
            vkCmdCopyImage(cmd, tmp->img, VK_IMAGE_LAYOUT_GENERAL, img->img, VK_IMAGE_LAYOUT_GENERAL, 1, &copy);
            tmp->use_seq = d->seq;
            vkd_image_destroy(d, tmp);
        }
        img->use_seq = d->seq;
        vkd_barrier_all(cmd);
        VKD_UNLOCK(d);
        return D3D_OK;
    }
    VkRenderingAttachmentInfo att = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    att.imageView = vkd_surface_rtv(s, 0);
    att.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
    att.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    att.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

    VkRenderingInfo ri = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea.extent = (VkExtent2D){s->w, s->h};
    ri.layerCount = 1;
    ri.colorAttachmentCount = 1;
    ri.pColorAttachments = &att;

    VkClearAttachment clear = {0};
    clear.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;

    clear.clearValue.color.float32[0] = ((color >> 16) & 255) / 255.0f;
    clear.clearValue.color.float32[1] = ((color >> 8) & 255) / 255.0f;
    clear.clearValue.color.float32[2] = (color & 255) / 255.0f;
    clear.clearValue.color.float32[3] = (color >> 24) / 255.0f;

    VkClearRect cr = {{{r.left, r.top}, {r.right - r.left, r.bottom - r.top}}, 0, 1};

    vkd_barrier_all(cmd);
    vkCmdBeginRendering(cmd, &ri);
    if (cr.rect.extent.width && cr.rect.extent.height)
        vkCmdClearAttachments(cmd, 1, &clear, 1, &cr);
    vkCmdEndRendering(cmd);
    vkd_barrier_all(cmd);
    img->use_seq = d->seq;

    VKD_UNLOCK(d);
    return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_StretchRect(IDirect3DDevice9 *self, IDirect3DSurface9 *src, const RECT *sr,
                                                 IDirect3DSurface9 *dst, const RECT *dr, D3DTEXTUREFILTERTYPE filter)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_surface *s = (struct vkd_surface *)src, *t = (struct vkd_surface *)dst;

    if (!s || !t || s == t || s->r.dev != d || t->r.dev != d || s->r.pool != D3DPOOL_DEFAULT ||
        t->r.pool != D3DPOOL_DEFAULT || s->locked || t->locked || filter > D3DTEXF_LINEAR)
        return D3DERR_INVALIDCALL;

    struct vkd_image *a = vkd_surface_image(s), *b = vkd_surface_image(t);
    if (!a || !b)
        return D3DERR_INVALIDCALL;

    RECT x = sr ? *sr : (RECT){0, 0, s->w, s->h}, y = dr ? *dr : (RECT){0, 0, t->w, t->h};

    if (x.left < 0 || x.top < 0 || x.right > (LONG)s->w || x.bottom > (LONG)s->h || x.right <= x.left ||
        x.bottom <= x.top || y.left < 0 || y.top < 0 || y.right > (LONG)t->w || y.bottom > (LONG)t->h ||
        y.right <= y.left || y.bottom <= y.top)
        return D3DERR_INVALIDCALL;

    int equal = x.right - x.left == y.right - y.left && x.bottom - x.top == y.bottom - y.top;

    if ((a->aspect != VK_IMAGE_ASPECT_COLOR_BIT || b->aspect != VK_IMAGE_ASPECT_COLOR_BIT) &&
        (!equal || a->vkfmt != b->vkfmt))
        return D3DERR_INVALIDCALL;

    if (a->samples != b->samples && (a->samples == VK_SAMPLE_COUNT_1_BIT || b->samples != VK_SAMPLE_COUNT_1_BIT ||
                                     !equal || a->vkfmt != b->vkfmt || a->aspect != VK_IMAGE_ASPECT_COLOR_BIT))
        return D3DERR_INVALIDCALL;

    if (a->samples != VK_SAMPLE_COUNT_1_BIT && a->samples == b->samples && !equal)
        return D3DERR_INVALIDCALL;

    VKD_LOCK(d);
    vkd_end_rendering(d);
    VkCommandBuffer cmd = vkd_cmd(d);
    vkd_barrier_all(cmd);

    VkImageSubresourceLayers sa = {a->aspect, s->level, s->layer, 1}, sb = {b->aspect, t->level, t->layer, 1};

    if (a->samples != b->samples) {
        VkImageResolve c = {sa, {x.left, x.top, 0}, sb, {y.left, y.top, 0}, {x.right - x.left, x.bottom - x.top, 1}};
        vkCmdResolveImage(cmd, a->img, VK_IMAGE_LAYOUT_GENERAL, b->img, VK_IMAGE_LAYOUT_GENERAL, 1, &c);
    } else if (equal && a->vkfmt == b->vkfmt) {
        VkImageCopy c[2] = {{sa, {x.left, x.top, 0}, sb, {y.left, y.top, 0}, {x.right - x.left, x.bottom - x.top, 1}}};
        UINT count = 1;
        if ((a->aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) ==
            (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)) {
            c[0].srcSubresource.aspectMask = c[0].dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            c[1] = c[0];
            c[1].srcSubresource.aspectMask = c[1].dstSubresource.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
            count = 2;
        }
        vkCmdCopyImage(cmd, a->img, VK_IMAGE_LAYOUT_GENERAL, b->img, VK_IMAGE_LAYOUT_GENERAL, count, c);
    } else {
        VkImageBlit c = {
            sa, {{x.left, x.top, 0}, {x.right, x.bottom, 1}}, sb, {{y.left, y.top, 0}, {y.right, y.bottom, 1}}};
        vkCmdBlitImage(cmd, a->img, VK_IMAGE_LAYOUT_GENERAL, b->img, VK_IMAGE_LAYOUT_GENERAL, 1, &c,
                       filter == D3DTEXF_LINEAR ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
    }
    a->use_seq = b->use_seq = d->seq;
    vkd_barrier_all(cmd);
    VKD_UNLOCK(d);
    return D3D_OK;
}
