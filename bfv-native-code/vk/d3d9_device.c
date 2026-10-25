/* Native Direct3D 9 device ABI backed by Vulkan dynamic rendering. */
#include "vkd3d9.h"

static void device_draw_free(struct vkd_device *d);
static struct vkd_device *device_window_owner;
#include "device_window.inc"

void vkd_end_rendering(struct vkd_device *d)
{
    if (!d->rendering) return;
    vkd_queries_pass_end(d);
    vkCmdEndRendering(vkd_cmd(d));
    d->rendering = 0;
    d->draws_in_pass = 0;
    vkd_barrier_all(vkd_cmd(d));
}

int vkd_device_layout_init(struct vkd_device *d)
{
    VkDescriptorSetLayoutBinding b[DXSO_BINDINGS];
    memset(b, 0, sizeof b);
    for (uint32_t i = 0; i < DXSO_BINDINGS; i++) {
        b[i].binding = i;
        b[i].descriptorType = i < 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        b[i].descriptorCount = 1;
        b[i].stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo si = {VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};
    si.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR;
    si.bindingCount = DXSO_BINDINGS;
    si.pBindings = b;
    if (vkCreateDescriptorSetLayout(vkd.dev, &si, 0, &d->set_layout) != VK_SUCCESS) return 0;
    VkPushConstantRange push = {VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(struct dxso_push)};
    VkPipelineLayoutCreateInfo li = {VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
    li.setLayoutCount = 1;
    li.pSetLayouts = &d->set_layout;
    li.pushConstantRangeCount = 1;
    li.pPushConstantRanges = &push;
    if (vkCreatePipelineLayout(vkd.dev, &li, 0, &d->layout) != VK_SUCCESS) return 0;
    VkPipelineCacheCreateInfo ci = {VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    if (vkCreatePipelineCache(vkd.dev, &ci, 0, &d->pcache) != VK_SUCCESS) return 0;
    if (!vkd_buf_alloc(d, sizeof(struct dxso_vs_consts), 1, &d->zero_vb) || !d->zero_vb.map) return 0;
    memset(d->zero_vb.map, 0, sizeof(struct dxso_vs_consts));
    return 1;
}

void vkd_device_layout_free(struct vkd_device *d)
{
    /* The caller must drain submissions before destroying layouts/pipelines. */
    vkd_buf_free(d, &d->zero_vb);
    if (d->pcache) vkDestroyPipelineCache(vkd.dev, d->pcache, 0);
    if (d->layout) vkDestroyPipelineLayout(vkd.dev, d->layout, 0);
    if (d->set_layout) vkDestroyDescriptorSetLayout(vkd.dev, d->set_layout, 0);
    d->pcache = 0; d->layout = 0; d->set_layout = 0;
}

void vkd_pipelines_forget(struct vkd_device *d, uint32_t id)
{
    for (uint32_t i = 0; i < d->pipelines.cap; i++) {
        struct vkd_pipeline *p = d->pipelines.e[i].val;
        if (!d->pipelines.e[i].hash || !p || !p->pipe || (p->vs_id != id && p->ps_id != id)) continue;
        struct vkd_garbage g = {0};
        g.kind = G_PIPELINE; g.h.pipe = p->pipe;
        vkd_defer(d, &g);
        if (d->bound_pipe == p->pipe) d->bound_pipe = 0;
        p->pipe = 0;
        d->dirty |= DIRTY_PIPELINE;
    }
}

void vkd_apply_state_all(struct vkd_device *d)
{
    d->nactive = 0;
    for (uint32_t i = 0; i < d->st.nlights && d->nactive < 8; i++)
        if (d->st.lights[i].on) d->active_lights[d->nactive++] = i;
    d->dirty = DIRTY_ALL;
}

void vkd_set_render_state(struct vkd_device *d, struct vkd_state *s, D3DRENDERSTATETYPE rs, DWORD v)
{
    if ((UINT)rs >= VKD_MAX_RS) return;
    if (s == &d->st && s->rs[rs] != v) {
        if (rs == D3DRS_SRGBWRITEENABLE) vkd_end_rendering(d);
        d->dirty = DIRTY_ALL;
    }
    s->rs[rs] = v;
}

static HRESULT STDMETHODCALLTYPE dev_QueryInterface(IDirect3DDevice9 *self, REFIID iid, void **out)
{
    if (!out) return E_POINTER;
    *out = 0;
    if (!vkd_guid_eq(iid, &IID_IUnknown) && !vkd_guid_eq(iid, &IID_IDirect3DDevice9)) return E_NOINTERFACE;
    *out = self; IDirect3DDevice9_AddRef(self); return S_OK;
}
static ULONG STDMETHODCALLTYPE dev_AddRef(IDirect3DDevice9 *self)
{
    return InterlockedIncrement(&((struct vkd_device *)self)->ref);
}
static ULONG STDMETHODCALLTYPE dev_Release(IDirect3DDevice9 *self)
{
    struct vkd_device *d = (struct vkd_device *)self;
    LONG n = InterlockedDecrement(&d->ref);
    if (n) return n;
    vkd_flush_wait(d);
    device_window_free(d);
    if (d->recording) {
        vkd_state_release(&d->recording->s);
        vkd_mask_free(&d->recording->m);
        vkp_free(d->recording);
    }
    vkd_state_release(&d->st);
    for (UINT i = 0; i < 4; i++) if (d->rt[i]) { vkd_res_unbind(&d->rt[i]->r); d->rt[i] = 0; }
    if (d->ds) { vkd_res_unbind(&d->ds->r); d->ds = 0; }
    device_draw_free(d);
    vkp_free(d->extra_sc);
    vkd_swapchain_destroy(d->swapchain);
    vkd_surface_free(d->auto_ds);
    vkd_present_free(d);
    vkd_device_layout_free(d);
    vkd_flush_wait(d);
    vkd_device_free_vk(d);
    if (d->lock) vkp_lock_free(d->lock);
    IDirect3D9_Release((IDirect3D9 *)d->d3d);
    vkp_free(d);
    return 0;
}
static HRESULT STDMETHODCALLTYPE dev_TestCooperativeLevel(IDirect3DDevice9 *self)
{
    struct vkd_device *d = (struct vkd_device *)self;
    return d->lost ? (d->not_reset ? D3DERR_DEVICENOTRESET : D3DERR_DEVICELOST) : D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetDirect3D(IDirect3DDevice9 *self, IDirect3D9 **out)
{
    if (!out) return D3DERR_INVALIDCALL;
    *out = (IDirect3D9 *)((struct vkd_device *)self)->d3d;
    IDirect3D9_AddRef(*out); return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetDeviceCaps(IDirect3DDevice9 *self, D3DCAPS9 *out)
{
    if (!out) return D3DERR_INVALIDCALL;
    *out = ((struct vkd_device *)self)->caps; return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetCreationParameters(IDirect3DDevice9 *self, D3DDEVICE_CREATION_PARAMETERS *out)
{
    if (!out) return D3DERR_INVALIDCALL;
    *out = ((struct vkd_device *)self)->cp; return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_BeginScene(IDirect3DDevice9 *self)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (d->in_scene) return D3DERR_INVALIDCALL;
    d->in_scene = 1; return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_EndScene(IDirect3DDevice9 *self)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!d->in_scene) return D3DERR_INVALIDCALL;
    vkd_end_rendering(d); d->in_scene = 0; return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_Present(IDirect3DDevice9 *self, const RECT *src, const RECT *dst, HWND hwnd,
                                             const RGNDATA *dirty)
{
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);
    HRESULT hr = d->lost ? D3DERR_DEVICELOST : vkd_swapchain_present(d->swapchain, src, dst, hwnd);
    VKD_UNLOCK(d); return hr;
}
static HRESULT STDMETHODCALLTYPE dev_GetBackBuffer(IDirect3DDevice9 *self, UINT sc, UINT index,
                                                   D3DBACKBUFFER_TYPE type, IDirect3DSurface9 **out)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (sc || !out || index >= d->swapchain->nback || type != D3DBACKBUFFER_TYPE_MONO) return D3DERR_INVALIDCALL;
    *out = (IDirect3DSurface9 *)d->swapchain->back[index];
    IDirect3DSurface9_AddRef(*out); return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_Clear(IDirect3DDevice9 *self, DWORD count, const D3DRECT *rects,
                                           DWORD flags, D3DCOLOR color, float z, DWORD stencil)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if ((count && !rects) || (flags & ~(D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL))) return D3DERR_INVALIDCALL;
    if ((flags & (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL)) && !d->ds) return D3DERR_INVALIDCALL;
    if ((flags & D3DCLEAR_STENCIL) && !vkd_format(d->ds->r.format)->stencil) return D3DERR_INVALIDCALL;
    VKD_LOCK(d);
    vkd_end_rendering(d);
    VkCommandBuffer cmd = vkd_cmd(d);
    vkd_barrier_all(cmd);
    VkRenderingAttachmentInfo ca[4], ds = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    memset(ca, 0, sizeof ca);
    VkClearAttachment clear[5]; uint32_t nc = 0, nrt = 0;
    VkExtent2D extent = {0, 0};
    for (uint32_t i = 0; i < 4; i++) {
        ca[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        ca[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        ca[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; ca[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        if (!d->rt[i]) continue;
        struct vkd_surface *s = d->rt[i];
        ca[i].imageView = vkd_surface_rtv(s, !!d->st.rs[D3DRS_SRGBWRITEENABLE]);
        vkd_surface_image(s)->use_seq = d->seq;
        if (!extent.width) extent = (VkExtent2D){s->w, s->h};
        nrt = i + 1;
        if (flags & D3DCLEAR_TARGET) {
            VkClearAttachment *c = &clear[nc++]; memset(c, 0, sizeof *c);
            c->aspectMask = VK_IMAGE_ASPECT_COLOR_BIT; c->colorAttachment = i;
            c->clearValue.color.float32[0] = ((color >> 16) & 255) / 255.0f;
            c->clearValue.color.float32[1] = ((color >> 8) & 255) / 255.0f;
            c->clearValue.color.float32[2] = (color & 255) / 255.0f;
            c->clearValue.color.float32[3] = (color >> 24) / 255.0f;
        }
    }
    if (d->ds) {
        ds.imageView = vkd_surface_rtv(d->ds, 0); ds.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        ds.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD; ds.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        vkd_surface_image(d->ds)->use_seq = d->seq;
        if (!extent.width) extent = (VkExtent2D){d->ds->w, d->ds->h};
        if (flags & (D3DCLEAR_ZBUFFER | D3DCLEAR_STENCIL)) {
            VkClearAttachment *c = &clear[nc++]; memset(c, 0, sizeof *c);
            if (flags & D3DCLEAR_ZBUFFER) c->aspectMask |= VK_IMAGE_ASPECT_DEPTH_BIT;
            if (flags & D3DCLEAR_STENCIL) c->aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
            c->clearValue.depthStencil = (VkClearDepthStencilValue){z, stencil};
        }
    }
    if (!nc) { VKD_UNLOCK(d); return D3D_OK; }
    VkRenderingInfo ri = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea.extent = extent; ri.layerCount = 1;
    ri.colorAttachmentCount = nrt; ri.pColorAttachments = ca;
    if (d->ds) {
        ri.pDepthAttachment = &ds;
        if (vkd_format(d->ds->r.format)->stencil) ri.pStencilAttachment = &ds;
    }
    vkCmdBeginRendering(cmd, &ri);
    const D3DVIEWPORT9 *vp = &d->st.viewport;
    for (DWORD i = 0; i < (count ? count : 1); i++) {
        LONG x0 = vp->X, y0 = vp->Y;
        LONG x1 = VKP_MIN((uint64_t)vp->X + vp->Width, extent.width);
        LONG y1 = VKP_MIN((uint64_t)vp->Y + vp->Height, extent.height);
        if (count) {
            x0 = VKP_MAX(x0, rects[i].x1); y0 = VKP_MAX(y0, rects[i].y1);
            x1 = VKP_MIN(x1, rects[i].x2); y1 = VKP_MIN(y1, rects[i].y2);
        }
        if (x1 <= x0 || y1 <= y0) continue;
        VkClearRect r = {{{x0, y0}, {(UINT)(x1 - x0), (UINT)(y1 - y0)}}, 0, 1};
        vkCmdClearAttachments(cmd, nc, clear, 1, &r);
    }
    vkCmdEndRendering(cmd);
    vkd_barrier_all(cmd);
    VKD_UNLOCK(d); return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE dev_SetViewport(IDirect3DDevice9 *self, const D3DVIEWPORT9 *vp)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!vp || !vp->Width || !vp->Height || vp->MinZ < 0 || vp->MaxZ > 1 || vp->MinZ > vp->MaxZ)
        return D3DERR_INVALIDCALL;
    VKD_LOCK(d);
    if (d->recording) { d->recording->s.viewport = *vp; d->recording->m.misc |= VKD_SB_VIEWPORT; }
    else { d->st.viewport = *vp; d->dirty |= DIRTY_VIEWPORT | DIRTY_FF_VS_CONST; }
    VKD_UNLOCK(d); return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetViewport(IDirect3DDevice9 *self, D3DVIEWPORT9 *vp)
{
    if (!vp) return D3DERR_INVALIDCALL;
    *vp = ((struct vkd_device *)self)->st.viewport; return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_CreateOffscreenPlainSurface(IDirect3DDevice9 *self, UINT w, UINT h,
    D3DFORMAT f, D3DPOOL pool, IDirect3DSurface9 **out, HANDLE *shared)
{
    if (!out) return D3DERR_INVALIDCALL;
    *out = 0;
    if (shared || pool == D3DPOOL_MANAGED) return D3DERR_INVALIDCALL;
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);
    HRESULT hr = vkd_create_surface(d, w, h, f, D3DMULTISAMPLE_NONE, 0, 0, pool, 1, 0,
                                    (struct vkd_surface **)out);
    if (SUCCEEDED(hr)) IDirect3DDevice9_AddRef(self);
    VKD_UNLOCK(d); return hr;
}
static HRESULT STDMETHODCALLTYPE dev_GetRenderTargetData(IDirect3DDevice9 *self, IDirect3DSurface9 *src,
                                                         IDirect3DSurface9 *dst)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_surface *s = (struct vkd_surface *)src, *t = (struct vkd_surface *)dst;
    if (!s || !t || s->r.dev != d || t->r.dev != d || !(s->r.usage & D3DUSAGE_RENDERTARGET) ||
        t->r.pool != D3DPOOL_SYSTEMMEM || s->ms != D3DMULTISAMPLE_NONE || s->w != t->w || s->h != t->h ||
        s->r.format != t->r.format || s->locked || t->locked) return D3DERR_INVALIDCALL;
    VKD_LOCK(d);
    UINT pitch;
    uint8_t *cpu = vkd_surface_cpu(t, &pitch, 0);
    if (!cpu) { VKD_UNLOCK(d); return E_OUTOFMEMORY; }
    vkd_surface_readback(s, cpu, pitch);
    VKD_UNLOCK(d); return D3D_OK;
}
static void STDMETHODCALLTYPE dev_SetGammaRamp(IDirect3DDevice9 *self, UINT index, DWORD flags, const D3DGAMMARAMP *ramp)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (index || !ramp) return;
    VKD_LOCK(d); vkd_swapchain_gamma(d->swapchain, ramp); VKD_UNLOCK(d);
}
static void STDMETHODCALLTYPE dev_GetGammaRamp(IDirect3DDevice9 *self, UINT index, D3DGAMMARAMP *ramp)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!index && ramp) *ramp = d->swapchain->ramp;
}
static HRESULT STDMETHODCALLTYPE dev_Reset(IDirect3DDevice9 *self, D3DPRESENT_PARAMETERS *pp)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!pp || d->in_scene || d->recording) return D3DERR_INVALIDCALL;
    if (vkp_option_int("vulkan.windowed", 0)) pp->Windowed = TRUE;
    VKD_LOCK(d);
    if (vkd_reset_resources_busy(d)) { VKD_UNLOCK(d); return D3DERR_INVALIDCALL; }
    for (UINT i = 0; i < d->swapchain->nback; i++)
        if (d->swapchain->back[i]->r.ref) { VKD_UNLOCK(d); return D3DERR_INVALIDCALL; }
    if (d->auto_ds && d->auto_ds->r.ref) { VKD_UNLOCK(d); return D3DERR_INVALIDCALL; }
    HRESULT winhr = device_window_set(d, pp);
    if (FAILED(winhr)) { VKD_UNLOCK(d); return winhr; }
    vkd_flush_wait(d);
    for (UINT i = 0; i < 4; i++) if (d->rt[i]) { vkd_res_unbind(&d->rt[i]->r); d->rt[i] = 0; }
    if (d->ds) { vkd_res_unbind(&d->ds->r); d->ds = 0; }
    HRESULT hr = vkd_swapchain_reset(d->swapchain, pp);
    if (FAILED(hr)) { VKD_UNLOCK(d); return hr; }
    vkd_state_release(&d->st);
    memset(&d->st, 0, sizeof d->st);
    vkd_surface_free(d->auto_ds); d->auto_ds = 0;
    if (pp->EnableAutoDepthStencil) {
        hr = vkd_create_surface(d, pp->BackBufferWidth, pp->BackBufferHeight, pp->AutoDepthStencilFormat,
            pp->MultiSampleType, pp->MultiSampleQuality, D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, 0, 0, &d->auto_ds);
        if (FAILED(hr)) { VKD_UNLOCK(d); return hr; }
        d->auto_ds->r.ref = 0; d->auto_ds->r.implicit = 1;
    }
    memset(d->rt, 0, sizeof d->rt);
    d->rt[0] = d->swapchain->back[0]; d->ds = d->auto_ds;
    vkd_res_bind(&d->rt[0]->r); if (d->ds) vkd_res_bind(&d->ds->r);
    vkd_state_defaults(d, &d->st);
    d->st.viewport = (D3DVIEWPORT9){0, 0, pp->BackBufferWidth, pp->BackBufferHeight, 0, 1};
    d->st.scissor = (RECT){0, 0, (LONG)pp->BackBufferWidth, (LONG)pp->BackBufferHeight};
    d->lost = d->not_reset = 0;
    vkd_apply_state_all(d);
    VKD_UNLOCK(d); return D3D_OK;
}

/* State-block recording changes only the recording state. Getters always read
 * the live device, and failed setters must not add capture bits. */
#define STATE(d) ((d)->recording ? &(d)->recording->s : &(d)->st)
#define MASK_BIT(a, i) ((a)[(i) >> 5] |= 1u << ((i) & 31))

static HRESULT STDMETHODCALLTYPE dev_SetRenderState(IDirect3DDevice9 *self, D3DRENDERSTATETYPE state, DWORD value)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!(UINT)state || (UINT)state >= VKD_MAX_RS) return D3DERR_INVALIDCALL;
    VKD_LOCK(d);
    vkd_set_render_state(d, STATE(d), state, value);
    if (d->recording) MASK_BIT(d->recording->m.rs, (UINT)state);
    VKD_UNLOCK(d); return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetRenderState(IDirect3DDevice9 *self, D3DRENDERSTATETYPE state, DWORD *value)
{
    if (!value || !(UINT)state || (UINT)state >= VKD_MAX_RS) return D3DERR_INVALIDCALL;
    *value = ((struct vkd_device *)self)->st.rs[state]; return D3D_OK;
}
static int sampler_index(DWORD id)
{
    if (id < 16) return (int)id;
    if (id >= D3DVERTEXTEXTURESAMPLER0 && id <= D3DVERTEXTEXTURESAMPLER3)
        return 16 + (int)(id - D3DVERTEXTEXTURESAMPLER0);
    return -1;
}
static HRESULT STDMETHODCALLTYPE dev_SetSamplerState(IDirect3DDevice9 *self, DWORD id, D3DSAMPLERSTATETYPE state, DWORD value)
{
    struct vkd_device *d = (struct vkd_device *)self;
    int i = sampler_index(id);
    if (i < 0 || !(UINT)state || (UINT)state >= 14) return D3DERR_INVALIDCALL;
    VKD_LOCK(d);
    STATE(d)->ss[i][state] = value;
    if (d->recording) d->recording->m.ss[i] |= 1u << state;
    else d->dirty |= DIRTY_SAMPLERS | DIRTY_SHADERS;
    VKD_UNLOCK(d); return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetSamplerState(IDirect3DDevice9 *self, DWORD id, D3DSAMPLERSTATETYPE state, DWORD *value)
{
    int i = sampler_index(id);
    if (!value || i < 0 || !(UINT)state || (UINT)state >= 14) return D3DERR_INVALIDCALL;
    *value = ((struct vkd_device *)self)->st.ss[i][state]; return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_SetTextureStageState(IDirect3DDevice9 *self, DWORD i, D3DTEXTURESTAGESTATETYPE state, DWORD value)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (i >= 8 || !(UINT)state || (UINT)state >= 33) return D3DERR_INVALIDCALL;
    VKD_LOCK(d);
    STATE(d)->tss[i][state] = value;
    if (d->recording) d->recording->m.tss[i] |= UINT64_C(1) << state;
    else d->dirty |= DIRTY_FF_VS | DIRTY_FF_PS | DIRTY_FF_VS_CONST | DIRTY_FF_PS_CONST | DIRTY_PS_CONST | DIRTY_SHADERS;
    VKD_UNLOCK(d); return D3D_OK;
}
static HRESULT STDMETHODCALLTYPE dev_GetTextureStageState(IDirect3DDevice9 *self, DWORD i, D3DTEXTURESTAGESTATETYPE state, DWORD *value)
{
    if (!value || i >= 8 || !(UINT)state || (UINT)state >= 33) return D3DERR_INVALIDCALL;
    *value = ((struct vkd_device *)self)->st.tss[i][state]; return D3D_OK;
}

/* Float constants use a bitset; integer and Boolean constants fit one word. */
#define CONSTANT_F(prefix, member, limit, dirty_flag) \
static HRESULT STDMETHODCALLTYPE dev_Set##prefix##ShaderConstantF(IDirect3DDevice9 *self, UINT start, const float *v, UINT n) \
{ \
    struct vkd_device *d = (struct vkd_device *)self; \
    if (start > limit || n > limit - start || (n && !v)) return D3DERR_INVALIDCALL; \
    VKD_LOCK(d); \
    if (n) memcpy(STATE(d)->member[start], v, n * 16); \
    if (d->recording) { for (UINT i = start; i < start + n; i++) MASK_BIT(d->recording->m.member, i); } \
    else d->dirty |= dirty_flag; \
    VKD_UNLOCK(d); return D3D_OK; \
} \
static HRESULT STDMETHODCALLTYPE dev_Get##prefix##ShaderConstantF(IDirect3DDevice9 *self, UINT start, float *v, UINT n) \
{ \
    if (start > limit || n > limit - start || (n && !v)) return D3DERR_INVALIDCALL; \
    if (n) memcpy(v, ((struct vkd_device *)self)->st.member[start], n * 16); \
    return D3D_OK; \
}
#define CONSTANT_SMALL(prefix, suffix, member, ctype, width, dirty_flag) \
static HRESULT STDMETHODCALLTYPE dev_Set##prefix##ShaderConstant##suffix(IDirect3DDevice9 *self, UINT start, const ctype *v, UINT n) \
{ \
    struct vkd_device *d = (struct vkd_device *)self; \
    if (start > 16 || n > 16 - start || (n && !v)) return D3DERR_INVALIDCALL; \
    VKD_LOCK(d); \
    if (n) memcpy(&STATE(d)->member[start], v, n * width * sizeof(ctype)); \
    if (d->recording) { for (UINT i = start; i < start + n; i++) d->recording->m.member |= 1u << i; } \
    else d->dirty |= dirty_flag; \
    VKD_UNLOCK(d); return D3D_OK; \
} \
static HRESULT STDMETHODCALLTYPE dev_Get##prefix##ShaderConstant##suffix(IDirect3DDevice9 *self, UINT start, ctype *v, UINT n) \
{ \
    if (start > 16 || n > 16 - start || (n && !v)) return D3DERR_INVALIDCALL; \
    if (n) memcpy(v, &((struct vkd_device *)self)->st.member[start], n * width * sizeof(ctype)); \
    return D3D_OK; \
}
CONSTANT_F(Vertex, vsf, 256u, DIRTY_VS_CONST)
CONSTANT_F(Pixel, psf, 224u, DIRTY_PS_CONST)
CONSTANT_SMALL(Vertex, I, vsi, int, 4, DIRTY_VS_CONST)
CONSTANT_SMALL(Vertex, B, vsb, BOOL, 1, DIRTY_VS_CONST)
CONSTANT_SMALL(Pixel, I, psi, int, 4, DIRTY_PS_CONST)
CONSTANT_SMALL(Pixel, B, psb, BOOL, 1, DIRTY_PS_CONST)

#define SHADER_BIND(prefix, iface, member, dirty_flag) \
static HRESULT STDMETHODCALLTYPE dev_Set##prefix##Shader(IDirect3DDevice9 *self, iface *shader) \
{ \
    struct vkd_device *d = (struct vkd_device *)self; struct vkd_shader *s = (struct vkd_shader *)shader; \
    if (s && s->dev != d) return D3DERR_INVALIDCALL; \
    VKD_LOCK(d); vkd_ref_shader(&STATE(d)->member, s); \
    if (d->recording) d->recording->m.misc |= dirty_flag; else d->dirty |= DIRTY_SHADERS | DIRTY_INPUT; \
    VKD_UNLOCK(d); return D3D_OK; \
} \
static HRESULT STDMETHODCALLTYPE dev_Get##prefix##Shader(IDirect3DDevice9 *self, iface **out) \
{ \
    if (!out) return D3DERR_INVALIDCALL; \
    *out = (iface *)((struct vkd_device *)self)->st.member; \
    if (*out) iface##_AddRef(*out); \
    return D3D_OK; \
}
SHADER_BIND(Vertex, IDirect3DVertexShader9, vs, VKD_SB_VS)
SHADER_BIND(Pixel, IDirect3DPixelShader9, ps, VKD_SB_PS)

static HRESULT STDMETHODCALLTYPE dev_CreateStateBlock(IDirect3DDevice9 *self, D3DSTATEBLOCKTYPE type, IDirect3DStateBlock9 **out)
{
    if (!out) return D3DERR_INVALIDCALL;
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);
    HRESULT hr = vkd_create_stateblock(d, type, (struct vkd_stateblock **)out);
    if (SUCCEEDED(hr)) IDirect3DDevice9_AddRef(self);
    VKD_UNLOCK(d); return hr;
}
static HRESULT STDMETHODCALLTYPE dev_BeginStateBlock(IDirect3DDevice9 *self)
{
    struct vkd_device *d = (struct vkd_device *)self;
    VKD_LOCK(d);
    if (d->recording) { VKD_UNLOCK(d); return D3DERR_INVALIDCALL; }
    vkd_stateblock_begin(d);
    HRESULT hr = d->recording ? D3D_OK : E_OUTOFMEMORY;
    VKD_UNLOCK(d); return hr;
}
static HRESULT STDMETHODCALLTYPE dev_EndStateBlock(IDirect3DDevice9 *self, IDirect3DStateBlock9 **out)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (!out) return D3DERR_INVALIDCALL;
    *out = 0;
    VKD_LOCK(d);
    if (!d->recording) { VKD_UNLOCK(d); return D3DERR_INVALIDCALL; }
    *out = (IDirect3DStateBlock9 *)d->recording; d->recording = 0;
    IDirect3DDevice9_AddRef(self);
    VKD_UNLOCK(d); return D3D_OK;
}

#include "device_methods.inc"
#include "device_draw.inc"

/* Complete IDirect3DDevice9 ABI. Unsupported optional operations fail explicitly. */
const IDirect3DDevice9Vtbl vkd_device_vtbl = {
    .CreateTexture = dev_CreateTexture,
    .CreateCubeTexture = dev_CreateCubeTexture,
    .CreateVolumeTexture = dev_CreateVolumeTexture,
    .CreateVertexBuffer = dev_CreateVertexBuffer,
    .CreateIndexBuffer = dev_CreateIndexBuffer,
    .CreateRenderTarget = dev_CreateRenderTarget,
    .CreateDepthStencilSurface = dev_CreateDepthStencilSurface,
    .SetTexture = dev_SetTexture,
    .GetTexture = dev_GetTexture,
    .SetStreamSource = dev_SetStreamSource,
    .GetStreamSource = dev_GetStreamSource,
    .SetStreamSourceFreq = dev_SetStreamSourceFreq,
    .GetStreamSourceFreq = dev_GetStreamSourceFreq,
    .SetIndices = dev_SetIndices,
    .GetIndices = dev_GetIndices,
    .SetVertexDeclaration = dev_SetVertexDeclaration,
    .GetVertexDeclaration = dev_GetVertexDeclaration,
    .SetFVF = dev_SetFVF,
    .GetFVF = dev_GetFVF,
    .CreateVertexDeclaration = dev_CreateVertexDeclaration,
    .CreateQuery = dev_CreateQuery,
    .SetRenderTarget = dev_SetRenderTarget,
    .GetRenderTarget = dev_GetRenderTarget,
    .SetDepthStencilSurface = dev_SetDepthStencilSurface,
    .GetDepthStencilSurface = dev_GetDepthStencilSurface,
    .SetTransform = dev_SetTransform,
    .GetTransform = dev_GetTransform,
    .MultiplyTransform = dev_MultiplyTransform,
    .SetMaterial = dev_SetMaterial,
    .GetMaterial = dev_GetMaterial,
    .SetLight = dev_SetLight,
    .GetLight = dev_GetLight,
    .LightEnable = dev_LightEnable,
    .GetLightEnable = dev_GetLightEnable,
    .SetClipPlane = dev_SetClipPlane,
    .GetClipPlane = dev_GetClipPlane,
    .SetScissorRect = dev_SetScissorRect,
    .GetScissorRect = dev_GetScissorRect,
    .SetClipStatus = dev_SetClipStatus,
    .GetClipStatus = dev_GetClipStatus,
    .SetSoftwareVertexProcessing = dev_SetSoftwareVertexProcessing,
    .GetSoftwareVertexProcessing = dev_GetSoftwareVertexProcessing,
    .SetNPatchMode = dev_SetNPatchMode,
    .GetNPatchMode = dev_GetNPatchMode,
    .ValidateDevice = dev_ValidateDevice,
    .GetAvailableTextureMem = dev_GetAvailableTextureMem,
    .EvictManagedResources = dev_EvictManagedResources,
    .GetDisplayMode = dev_GetDisplayMode,
    .GetSwapChain = dev_GetSwapChain,
    .GetNumberOfSwapChains = dev_GetNumberOfSwapChains,
    .CreateAdditionalSwapChain = dev_CreateAdditionalSwapChain,
    .GetRasterStatus = dev_GetRasterStatus,
    .GetFrontBufferData = dev_GetFrontBufferData,
    .SetDialogBoxMode = dev_SetDialogBoxMode,
    .SetCursorProperties = dev_SetCursorProperties,
    .SetCursorPosition = dev_SetCursorPosition,
    .ShowCursor = dev_ShowCursor,
    .SetPaletteEntries = dev_SetPaletteEntries,
    .GetPaletteEntries = dev_GetPaletteEntries,
    .SetCurrentTexturePalette = dev_SetCurrentTexturePalette,
    .GetCurrentTexturePalette = dev_GetCurrentTexturePalette,
    .ProcessVertices = dev_ProcessVertices,
    .DrawRectPatch = dev_DrawRectPatch,
    .DrawTriPatch = dev_DrawTriPatch,
    .DeletePatch = dev_DeletePatch,
    .UpdateSurface = dev_UpdateSurface,
    .UpdateTexture = dev_UpdateTexture,
    .ColorFill = dev_ColorFill,
    .StretchRect = dev_StretchRect,
    .DrawPrimitive = dev_DrawPrimitive,
    .DrawIndexedPrimitive = dev_DrawIndexedPrimitive,
    .DrawPrimitiveUP = dev_DrawPrimitiveUP,
    .DrawIndexedPrimitiveUP = dev_DrawIndexedPrimitiveUP,
    .CreateVertexShader = dev_CreateVertexShader,
    .CreatePixelShader = dev_CreatePixelShader,
    .QueryInterface = dev_QueryInterface, .AddRef = dev_AddRef, .Release = dev_Release,
    .TestCooperativeLevel = dev_TestCooperativeLevel, .GetDirect3D = dev_GetDirect3D,
    .GetDeviceCaps = dev_GetDeviceCaps, .GetCreationParameters = dev_GetCreationParameters,
    .BeginScene = dev_BeginScene, .EndScene = dev_EndScene, .Clear = dev_Clear,
    .Present = dev_Present, .GetBackBuffer = dev_GetBackBuffer,
    .SetViewport = dev_SetViewport, .GetViewport = dev_GetViewport,
    .CreateOffscreenPlainSurface = dev_CreateOffscreenPlainSurface, .GetRenderTargetData = dev_GetRenderTargetData,
    .SetGammaRamp = dev_SetGammaRamp, .GetGammaRamp = dev_GetGammaRamp, .Reset = dev_Reset,
    .SetRenderState = dev_SetRenderState, .GetRenderState = dev_GetRenderState,
    .SetTextureStageState = dev_SetTextureStageState, .GetTextureStageState = dev_GetTextureStageState,
    .SetSamplerState = dev_SetSamplerState, .GetSamplerState = dev_GetSamplerState,
    .SetVertexShader = dev_SetVertexShader, .GetVertexShader = dev_GetVertexShader,
    .SetPixelShader = dev_SetPixelShader, .GetPixelShader = dev_GetPixelShader,
#define CONSTANT_SLOTS(prefix, suffix) \
    .Set##prefix##ShaderConstant##suffix = dev_Set##prefix##ShaderConstant##suffix, \
    .Get##prefix##ShaderConstant##suffix = dev_Get##prefix##ShaderConstant##suffix,
    CONSTANT_SLOTS(Vertex, F) CONSTANT_SLOTS(Vertex, I) CONSTANT_SLOTS(Vertex, B)
    CONSTANT_SLOTS(Pixel, F) CONSTANT_SLOTS(Pixel, I) CONSTANT_SLOTS(Pixel, B)
#undef CONSTANT_SLOTS
    .CreateStateBlock = dev_CreateStateBlock, .BeginStateBlock = dev_BeginStateBlock, .EndStateBlock = dev_EndStateBlock,
};

HRESULT vkd_device_create(struct vkd_d3d9 *d3d, UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags,
                          D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    if (!out) return D3DERR_INVALIDCALL;
    *out = 0;
    if (!pp || adapter || type != D3DDEVTYPE_HAL) return D3DERR_NOTAVAILABLE;
    if (!vkd_gpu_init()) return D3DERR_NOTAVAILABLE;
    if (vkp_option_int("vulkan.windowed", 0)) pp->Windowed = TRUE;
    struct vkd_device *d = vkp_alloc(sizeof *d);
    if (!d) return E_OUTOFMEMORY;
    d->vtbl = &vkd_device_vtbl; d->ref = 1; d->d3d = d3d;
    d->cp = (D3DDEVICE_CREATION_PARAMETERS){adapter, type, focus, flags};
    IDirect3D9_AddRef((IDirect3D9 *)d3d);
    vkd_fill_caps(&d->caps);
    d->option_interval = vkp_option_int("d3d9.presentInterval", -1);
    d->option_latency = vkp_option_int("d3d9.maxFrameLatency", 3);
    d->option_aniso = vkp_option_int("d3d9.samplerAnisotropy", -1);
    if (flags & D3DCREATE_MULTITHREADED) {
        d->lock = vkp_lock_new();
        if (!d->lock) goto fail;
    }
    if (!vkd_device_init_vk(d) || !vkd_device_layout_init(d)) goto fail;
    if (FAILED(device_window_set(d, pp))) goto fail;
    if (FAILED(vkd_swapchain_create(d, pp, 1, &d->swapchain))) goto fail;
    if (pp->EnableAutoDepthStencil) {
        if (FAILED(vkd_create_surface(d, pp->BackBufferWidth, pp->BackBufferHeight, pp->AutoDepthStencilFormat,
            pp->MultiSampleType, pp->MultiSampleQuality, D3DUSAGE_DEPTHSTENCIL, D3DPOOL_DEFAULT, 0, 0, &d->auto_ds))) goto fail;
        d->auto_ds->r.ref = 0; d->auto_ds->r.implicit = 1;
    }
    vkd_state_defaults(d, &d->st);
    d->rt[0] = d->swapchain->back[0]; d->ds = d->auto_ds;
    vkd_res_bind(&d->rt[0]->r); if (d->ds) vkd_res_bind(&d->ds->r);
    d->st.viewport = (D3DVIEWPORT9){0, 0, pp->BackBufferWidth, pp->BackBufferHeight, 0, 1};
    d->st.scissor = (RECT){0, 0, (LONG)pp->BackBufferWidth, (LONG)pp->BackBufferHeight};
    d->fpu_preserve = !!(flags & D3DCREATE_FPU_PRESERVE);
    if (!d->fpu_preserve) {
        unsigned short cw;
        __asm__ volatile("fnstcw %0" : "=m"(cw));
        cw &= (unsigned short)~0x300;
        __asm__ volatile("fldcw %0" : : "m"(cw));
    }
    vkd_apply_state_all(d);
    *out = (IDirect3DDevice9 *)d; return D3D_OK;
fail:
    device_window_free(d);
    /* Context setup may have failed before its first command buffer began. */
    if (d->seq) {
        vkd_swapchain_destroy(d->swapchain); vkd_surface_free(d->auto_ds);
        vkd_device_layout_free(d); vkd_flush_wait(d);
    } else {
        if (d->pcache) vkDestroyPipelineCache(vkd.dev, d->pcache, 0);
        if (d->layout) vkDestroyPipelineLayout(vkd.dev, d->layout, 0);
        if (d->set_layout) vkDestroyDescriptorSetLayout(vkd.dev, d->set_layout, 0);
    }
    vkd_device_free_vk(d);
    if (d->lock) vkp_lock_free(d->lock);
    IDirect3D9_Release((IDirect3D9 *)d3d); vkp_free(d);
    return D3DERR_NOTAVAILABLE;
}
