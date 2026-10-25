/* Resources: surfaces, volumes, textures (2D, cube, volume), vertex and
 * index buffers, and private data.
 *
 * CPU copies: managed, system-memory and scratch resources and dynamic
 * textures keep a "shadow" the application locks; unlocking uploads the
 * locked region to the Vulkan image. Static buffers are device-local with a
 * shadow; dynamic buffers live in host-visible memory and are renamed on
 * D3DLOCK_DISCARD while the GPU still reads the old copy. */
#include "vkd3d9.h"

#define RES(p) ((struct vkd_res *)(p))
#define SURF(p) ((struct vkd_surface *)(p))
#define TEX(p) ((struct vkd_texture *)(p))
#define BUF(p) ((struct vkd_buffer *)(p))

/* ---- private data ---- */

HRESULT vkd_priv_free(struct vkd_priv **list, REFGUID g)
{
    for (struct vkd_priv **pp = list; *pp; pp = &(*pp)->next) {
        struct vkd_priv *p = *pp;
        if (!vkd_guid_eq(g, &p->guid)) continue;
        *pp = p->next;
        if (p->flags & D3DSPD_IUNKNOWN) IUnknown_Release((IUnknown *)p->data);
        vkp_free(p);
        return D3D_OK;
    }
    return D3DERR_NOTFOUND;
}

HRESULT vkd_priv_set(struct vkd_priv **list, REFGUID g, const void *data, DWORD size, DWORD flags)
{
    if (!data && size) return D3DERR_INVALIDCALL;
    if ((flags & D3DSPD_IUNKNOWN) && size != sizeof(IUnknown *)) return D3DERR_INVALIDCALL;
    vkd_priv_free(list, g);
    struct vkd_priv *p = vkp_alloc(sizeof *p + ((flags & D3DSPD_IUNKNOWN) ? 0 : size));
    if (!p) return E_OUTOFMEMORY;
    p->guid = *g;
    p->flags = flags;
    p->size = size;
    if (flags & D3DSPD_IUNKNOWN) {
        p->data = (void *)data;
        IUnknown_AddRef((IUnknown *)data);
    } else {
        p->data = p + 1;
        if (size) memcpy(p->data, data, size);
    }
    p->next = *list;
    *list = p;
    return D3D_OK;
}

HRESULT vkd_priv_get(struct vkd_priv **list, REFGUID g, void *data, DWORD *size)
{
    if (!size) return D3DERR_INVALIDCALL;
    for (struct vkd_priv *p = *list; p; p = p->next) {
        if (!vkd_guid_eq(g, &p->guid)) continue;
        if (!data) { *size = p->size; return D3D_OK; }
        if (*size < p->size) { *size = p->size; return D3DERR_MOREDATA; }
        if (p->flags & D3DSPD_IUNKNOWN) {
            *(IUnknown **)data = (IUnknown *)p->data;
            IUnknown_AddRef((IUnknown *)p->data);
        } else {
            memcpy(data, p->data, p->size);
        }
        *size = p->size;
        return D3D_OK;
    }
    return D3DERR_NOTFOUND;
}

static void priv_clear(struct vkd_priv **list)
{
    while (*list) vkd_priv_free(list, &(*list)->guid);
}

/* ---- reference counting ---- */

static struct vkd_res *owner(struct vkd_res *r)
{
    if ((r->type == D3DRTYPE_SURFACE || r->type == D3DRTYPE_VOLUME) && SURF(r)->tex) return &SURF(r)->tex->r;
    return r;
}

void vkd_res_addref(struct vkd_res *r)
{
    r = owner(r);
    if (InterlockedIncrement(&r->ref) == 1) {
        struct vkd_swapchain *sc = r->type == D3DRTYPE_SURFACE ? SURF(r)->swapchain : 0;
        if (sc && !sc->implicit) IDirect3DSwapChain9_AddRef((IDirect3DSwapChain9 *)sc);
        else IDirect3DDevice9_AddRef((IDirect3DDevice9 *)r->dev);
    }
}

static void maybe_destroy(struct vkd_res *r)
{
    if (!r->ref && !r->bind && !r->implicit && r->destroy) r->destroy(r);
}

ULONG vkd_res_release(struct vkd_res *r)
{
    r = owner(r);
    LONG n = InterlockedDecrement(&r->ref);
    if (n < 0) { r->ref = 0; n = 0; }
    if (!n) {
        struct vkd_device *d = r->dev;
        struct vkd_swapchain *sc = r->type == D3DRTYPE_SURFACE ? SURF(r)->swapchain : 0;
        if (sc && !sc->implicit) IDirect3DSwapChain9_Release((IDirect3DSwapChain9 *)sc);
        else {
            maybe_destroy(r);
            IDirect3DDevice9_Release((IDirect3DDevice9 *)d);
        }
    }
    return (ULONG)n;
}

void vkd_res_bind(struct vkd_res *r) { if (r) owner(r)->bind++; }

void vkd_res_unbind(struct vkd_res *r)
{
    if (!r) return;
    r = owner(r);
    if (--r->bind <= 0) {
        r->bind = 0;
        maybe_destroy(r);
    }
}

static void track_resource(struct vkd_res *r)
{
    r->next = r->dev->resources;
    if (r->next) r->next->prev = r;
    r->dev->resources = r;
}

static void untrack_resource(struct vkd_res *r)
{
    if (r->prev) r->prev->next = r->next;
    else if (r->dev->resources == r) r->dev->resources = r->next;
    if (r->next) r->next->prev = r->prev;
}

int vkd_reset_resources_busy(struct vkd_device *d)
{
    if (d->nextra_sc) return 1;
    for (struct vkd_res *r = d->resources; r; r = r->next) {
        if (r->implicit || r->pool != D3DPOOL_DEFAULT) continue;
        if (r->ref) return 1;
        LONG live = 0;
        for (UINT i = 0; i < VKD_SAMPLERS; i++) if (d->st.tex[i] && owner(d->st.tex[i]) == r) live++;
        for (UINT i = 0; i < 16; i++) if (d->st.vb[i] && owner(&d->st.vb[i]->r) == r) live++;
        if (d->st.ib && owner(&d->st.ib->r) == r) live++;
        for (UINT i = 0; i < 4; i++) if (d->rt[i] && owner(&d->rt[i]->r) == r) live++;
        if (d->ds && owner(&d->ds->r) == r) live++;
        if (r->bind > live) return 1; /* state blocks retain a resource outside device state */
    }
    return 0;
}

struct vkd_texture *vkd_res_texture(struct vkd_res *r)
{
    if (!r) return 0;
    if (r->type == D3DRTYPE_TEXTURE || r->type == D3DRTYPE_CUBETEXTURE || r->type == D3DRTYPE_VOLUMETEXTURE)
        return TEX(r);
    return 0;
}

/* ---- helpers ---- */

VkSampleCountFlagBits vkd_samples(D3DMULTISAMPLE_TYPE ms, DWORD quality)
{
    if (ms == D3DMULTISAMPLE_NONMASKABLE) {
        static const VkSampleCountFlagBits order[3] = {VK_SAMPLE_COUNT_2_BIT, VK_SAMPLE_COUNT_4_BIT,
                                                       VK_SAMPLE_COUNT_8_BIT};
        return order[quality < 3 ? quality : 2];
    }
    if (ms >= 8) return VK_SAMPLE_COUNT_8_BIT;
    if (ms >= 4) return VK_SAMPLE_COUNT_4_BIT;
    if (ms >= 2) return VK_SAMPLE_COUNT_2_BIT;
    return VK_SAMPLE_COUNT_1_BIT;
}

static UINT mip(UINT x, UINT level) { x >>= level; return x ? x : 1; }

static UINT full_chain(UINT w, UINT h, UINT d)
{
    UINT m = VKP_MAX(VKP_MAX(w, h), d), n = 1;
    while (m > 1) { m >>= 1; n++; }
    return n;
}

struct vkd_image *vkd_surface_image(struct vkd_surface *s)
{
    return s->tex ? s->tex->img : s->img;
}

static const struct vkd_format *surface_format(struct vkd_surface *s)
{
    return s->tex ? s->tex->fmt : vkd_format(s->r.format);
}

VkImageView vkd_surface_rtv(struct vkd_surface *s, int srgb)
{
    struct vkd_image *img = vkd_surface_image(s);
    if (!img) return 0;
    const struct vkd_format *f = img->fmt;
    srgb = srgb && f->vk_srgb;
    VkImageView *v = srgb ? &s->rtv_srgb : &s->rtv;
    if (!*v) *v = vkd_image_view(img, srgb ? f->vk_srgb : f->vk, VK_IMAGE_VIEW_TYPE_2D, s->level, 1, s->layer, 1, 0);
    return *v;
}

/* Write CPU texels into an image region, ordered with the commands recorded so far. */
void vkd_image_write(struct vkd_device *d, struct vkd_image *img, UINT level, UINT layer, UINT x, UINT y, UINT z,
                     UINT w, UINT h, UINT depth, const uint8_t *src, UINT pitch, UINT slice)
{
    if (!img || !w || !h || !depth || (img->aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)))
        return;
    const struct vkd_format *f = img->fmt;
    UINT bs = f->block;
    UINT rows = (h + bs - 1) / bs, rowbytes = (w + bs - 1) / bs * f->bytes;
    VkDeviceSize size = (VkDeviceSize)rowbytes * rows * depth;
    struct vkd_buf ring;
    if (!vkd_ring(d, size, 16, &ring)) { vkp_log("vulkan: upload of %u bytes failed", (unsigned)size); return; }
    uint8_t *o = ring.map;
    for (UINT k = 0; k < depth; k++)
        for (UINT r = 0; r < rows; r++, o += rowbytes) memcpy(o, src + k * slice + r * pitch, rowbytes);
    VkBufferImageCopy c = {0};
    c.bufferOffset = ring.off;
    c.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    c.imageSubresource.mipLevel = level;
    c.imageSubresource.baseArrayLayer = layer;
    c.imageSubresource.layerCount = 1;
    c.imageOffset.x = (int32_t)x; c.imageOffset.y = (int32_t)y; c.imageOffset.z = (int32_t)z;
    c.imageExtent.width = w; c.imageExtent.height = h; c.imageExtent.depth = depth;
    VkCommandBuffer cmd = vkd_upload_cmd(d, &img->use_seq);
    vkCmdCopyBufferToImage(cmd, ring.buf, img->img, VK_IMAGE_LAYOUT_GENERAL, 1, &c);
}

/* Copy an image subresource to CPU memory (waits for the GPU). */
void vkd_image_read(struct vkd_device *d, struct vkd_image *img, UINT level, UINT layer, uint8_t *dst, UINT pitch)
{
    if (!img || (img->aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT))) return;
    const struct vkd_format *f = img->fmt;
    UINT w = mip(img->w, level), h = mip(img->h, level), dd = mip(img->d, level);
    UINT bs = f->block, rows = (h + bs - 1) / bs, rowbytes = (w + bs - 1) / bs * f->bytes;
    VkDeviceSize size = (VkDeviceSize)rowbytes * rows * dd;
    struct vkd_mem mem;
    if (!vkd_alloc_buffer(size, VKD_MEM_READBACK, &mem) || !mem.map) { vkp_log("vulkan: readback failed"); return; }
    vkd_end_rendering(d);
    VkCommandBuffer cmd = vkd_cmd(d);
    vkd_barrier_all(cmd);
    struct vkd_image *src = img;
    struct vkd_image *tmp = 0;
    if (img->samples != VK_SAMPLE_COUNT_1_BIT) {
        tmp = vkd_image_create(d, f, VK_IMAGE_TYPE_2D, w, h, 1, 1, 1, VK_SAMPLE_COUNT_1_BIT, 0, 0);
        if (!tmp) { vkd_free_mem(&mem); return; }
        VkImageResolve rs = {0};
        rs.srcSubresource.aspectMask = rs.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        rs.srcSubresource.mipLevel = level;
        rs.srcSubresource.baseArrayLayer = layer;
        rs.srcSubresource.layerCount = rs.dstSubresource.layerCount = 1;
        rs.extent.width = w; rs.extent.height = h; rs.extent.depth = 1;
        vkCmdResolveImage(cmd, img->img, VK_IMAGE_LAYOUT_GENERAL, tmp->img, VK_IMAGE_LAYOUT_GENERAL, 1, &rs);
        vkd_barrier_all(cmd);
        src = tmp;
        level = layer = 0;
    }
    VkBufferImageCopy c = {0};
    c.bufferOffset = mem.offset;
    c.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    c.imageSubresource.mipLevel = level;
    c.imageSubresource.baseArrayLayer = layer;
    c.imageSubresource.layerCount = 1;
    c.imageExtent.width = w; c.imageExtent.height = h; c.imageExtent.depth = dd;
    vkCmdCopyImageToBuffer(cmd, src->img, VK_IMAGE_LAYOUT_GENERAL, mem.buffer, 1, &c);
    vkd_barrier(cmd, VK_PIPELINE_STAGE_2_ALL_TRANSFER_BIT, VK_ACCESS_2_TRANSFER_WRITE_BIT, VK_PIPELINE_STAGE_2_HOST_BIT,
                VK_ACCESS_2_HOST_READ_BIT);
    img->use_seq = d->seq;
    vkd_flush_wait(d);
    const uint8_t *s = mem.map;
    for (UINT k = 0; k < dd; k++)
        for (UINT r = 0; r < rows; r++, s += rowbytes) memcpy(dst + (k * rows + r) * pitch, s, rowbytes);
    vkd_free_mem(&mem);
    if (tmp) vkd_image_destroy(d, tmp);
}

/* ---- CPU copies ---- */

/* CPU copy of a surface (no GPU readback); 0 when it has none */
uint8_t *vkd_surface_cpu(struct vkd_surface *s, UINT *pitch, UINT *slice)
{
    const struct vkd_format *f = surface_format(s);
    UINT p, sl;
    vkd_layout(f, s->w, s->h, s->d, &p, &sl, 0);
    if (pitch) *pitch = p;
    if (slice) *slice = sl;
    if (s->tex) return s->tex->shadow ? s->tex->shadow + s->tex->sub_off[s->sub] : 0;
    if (!s->shadow && f->bytes) s->shadow = vkp_alloc((size_t)sl * (s->d ? s->d : 1));
    return s->shadow;
}

/* CPU copy brought up to date with the GPU */
uint8_t *vkd_surface_shadow(struct vkd_surface *s, UINT *pitch)
{
    UINT p;
    uint8_t *cpu = vkd_surface_cpu(s, &p, 0);
    if (pitch) *pitch = p;
    struct vkd_image *img = vkd_surface_image(s);
    if (cpu && img && !s->tex) vkd_image_read(s->r.dev, img, s->level, s->layer, cpu, p);
    return cpu;
}

void vkd_surface_readback(struct vkd_surface *s, uint8_t *dst, UINT pitch)
{
    vkd_image_read(s->r.dev, vkd_surface_image(s), s->level, s->layer, dst, pitch);
}

/* Upload a box of a surface's CPU copy to its image. */
void vkd_surface_upload(struct vkd_surface *s, UINT x0, UINT y0, UINT z0, UINT x1, UINT y1, UINT z1)
{
    struct vkd_image *img = vkd_surface_image(s);
    UINT pitch, slice;
    uint8_t *cpu = vkd_surface_cpu(s, &pitch, &slice);
    if (!img || !cpu) return;
    const struct vkd_format *f = img->fmt;
    UINT bs = f->block;
    x0 = x0 / bs * bs; y0 = y0 / bs * bs;
    x1 = VKP_MIN((x1 + bs - 1) / bs * bs, s->w);
    y1 = VKP_MIN((y1 + bs - 1) / bs * bs, s->h);
    if (x1 <= x0 || y1 <= y0 || z1 <= z0) return;
    const uint8_t *src = cpu + z0 * slice + (y0 / bs) * pitch + (x0 / bs) * f->bytes;
    vkd_image_write(s->r.dev, img, s->level, s->layer, x0, y0, z0, x1 - x0, y1 - y0, z1 - z0, src, pitch, slice);
}

void vkd_texture_upload(struct vkd_texture *t, uint32_t sub, const RECT *rect, UINT z0, UINT z1)
{
    struct vkd_surface *s = t->surf[sub];
    if (rect) vkd_surface_upload(s, (UINT)rect->left, (UINT)rect->top, z0, (UINT)rect->right, (UINT)rect->bottom, z1);
    else vkd_surface_upload(s, 0, 0, 0, s->w, s->h, s->d);
}

void vkd_texture_genmips(struct vkd_texture *t)
{
    t->autogen_dirty = 0;
    struct vkd_image *img = t->img;
    if (!img || img->levels < 2) return;
    struct vkd_device *d = t->r.dev;
    vkd_end_rendering(d);
    VkCommandBuffer cmd = vkd_cmd(d);
    VkFilter filter = t->autogen_filter == D3DTEXF_POINT ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
    for (UINT l = 1; l < img->levels; l++) {
        vkd_barrier_all(cmd);
        VkImageBlit b = {0};
        b.srcSubresource.aspectMask = b.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        b.srcSubresource.mipLevel = l - 1;
        b.dstSubresource.mipLevel = l;
        b.srcSubresource.layerCount = b.dstSubresource.layerCount = img->layers;
        b.srcOffsets[1].x = (int32_t)mip(img->w, l - 1); b.srcOffsets[1].y = (int32_t)mip(img->h, l - 1);
        b.srcOffsets[1].z = (int32_t)mip(img->d, l - 1);
        b.dstOffsets[1].x = (int32_t)mip(img->w, l); b.dstOffsets[1].y = (int32_t)mip(img->h, l);
        b.dstOffsets[1].z = (int32_t)mip(img->d, l);
        vkCmdBlitImage(cmd, img->img, VK_IMAGE_LAYOUT_GENERAL, img->img, VK_IMAGE_LAYOUT_GENERAL, 1, &b, filter);
    }
    vkd_barrier_all(cmd);
    img->use_seq = d->seq;
}

/* ---- locking (surfaces and volumes) ---- */

static HRESULT lock_box(struct vkd_surface *s, UINT x0, UINT y0, UINT z0, UINT x1, UINT y1, UINT z1, DWORD flags,
                        void **bits, UINT *pitch_out, UINT *slice_out)
{
    if (s->locked) return D3DERR_INVALIDCALL;
    const struct vkd_format *f = surface_format(s);
    if (!f || !f->bytes) return D3DERR_INVALIDCALL;
    UINT pitch, slice;
    uint8_t *base;
    if (s->tex) {
        base = vkd_surface_cpu(s, &pitch, &slice);
        if (!base) return D3DERR_INVALIDCALL;
        if (!(flags & (D3DLOCK_READONLY | D3DLOCK_NO_DIRTY_UPDATE))) s->tex->dirty = 1;
    } else {
        struct vkd_image *img = s->img;
        if (img && !s->lockable && s->r.pool == D3DPOOL_DEFAULT && (s->r.usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)))
            return D3DERR_INVALIDCALL;
        if (img && (img->aspect & (VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT)))
            return D3DERR_INVALIDCALL;
        base = (flags & D3DLOCK_DISCARD) ? vkd_surface_cpu(s, &pitch, &slice) : vkd_surface_shadow(s, &pitch);
        vkd_surface_cpu(s, &pitch, &slice);
        if (!base) return D3DERR_INVALIDCALL;
    }
    UINT bs = f->block;
    *bits = base + (size_t)z0 * slice + (y0 / bs) * pitch + (x0 / bs) * f->bytes;
    *pitch_out = pitch;
    if (slice_out) *slice_out = slice;
    s->lx0 = x0; s->ly0 = y0; s->lz0 = z0; s->lx1 = x1; s->ly1 = y1; s->lz1 = z1;
    s->lock_flags = flags;
    s->locked = 1;
    return D3D_OK;
}

static HRESULT unlock_box(struct vkd_surface *s)
{
    if (!s->locked) return D3DERR_INVALIDCALL;
    s->locked = 0;
    if (s->lock_flags & D3DLOCK_READONLY) return D3D_OK;
    struct vkd_texture *t = s->tex;
    if (t && t->autogen && t->img) {
        /* the visible level changed: upload it and rebuild the chain on next use */
        vkd_surface_upload(s, s->lx0, s->ly0, s->lz0, s->lx1, s->ly1, s->lz1);
        t->autogen_dirty = 1;
        return D3D_OK;
    }
    vkd_surface_upload(s, s->lx0, s->ly0, s->lz0, s->lx1, s->ly1, s->lz1);
    return D3D_OK;
}

/* ---- common resource methods ---- */

#define RES_COMMON(pfx, I)                                                                                         \
    static ULONG STDMETHODCALLTYPE pfx##_AddRef(I *self) { vkd_res_addref(RES(self)); return (ULONG)owner(RES(self))->ref; } \
    static ULONG STDMETHODCALLTYPE pfx##_Release(I *self) { return vkd_res_release(RES(self)); }                   \
    static HRESULT STDMETHODCALLTYPE pfx##_GetDevice(I *self, IDirect3DDevice9 **out)                              \
    {                                                                                                              \
        if (!out) return D3DERR_INVALIDCALL;                                                                       \
        *out = (IDirect3DDevice9 *)RES(self)->dev;                                                                 \
        IDirect3DDevice9_AddRef(*out);                                                                             \
        return D3D_OK;                                                                                             \
    }                                                                                                              \
    static HRESULT STDMETHODCALLTYPE pfx##_SetPrivateData(I *self, REFGUID g, const void *data, DWORD size, DWORD flags) \
    {                                                                                                              \
        return vkd_priv_set(&RES(self)->priv, g, data, size, flags);                                               \
    }                                                                                                              \
    static HRESULT STDMETHODCALLTYPE pfx##_GetPrivateData(I *self, REFGUID g, void *data, DWORD *size)             \
    {                                                                                                              \
        return vkd_priv_get(&RES(self)->priv, g, data, size);                                                      \
    }                                                                                                              \
    static HRESULT STDMETHODCALLTYPE pfx##_FreePrivateData(I *self, REFGUID g)                                     \
    {                                                                                                              \
        return vkd_priv_free(&RES(self)->priv, g);                                                                 \
    }

#define RES_PRIORITY(pfx, I)                                                                                       \
    static DWORD STDMETHODCALLTYPE pfx##_SetPriority(I *self, DWORD p)                                             \
    {                                                                                                              \
        DWORD old = RES(self)->priority;                                                                           \
        if (RES(self)->pool == D3DPOOL_MANAGED) RES(self)->priority = p;                                           \
        return old;                                                                                                \
    }                                                                                                              \
    static DWORD STDMETHODCALLTYPE pfx##_GetPriority(I *self) { return RES(self)->priority; }                      \
    static void STDMETHODCALLTYPE pfx##_PreLoad(I *self) {}                                                        \
    static D3DRESOURCETYPE STDMETHODCALLTYPE pfx##_GetType(I *self) { return RES(self)->type; }

static HRESULT query(void *self, REFIID riid, void **out, const GUID *const *iids)
{
    if (!out) return E_POINTER;
    for (; *iids; iids++)
        if (vkd_guid_eq(riid, *iids)) {
            *out = self;
            vkd_res_addref(RES(self));
            return S_OK;
        }
    *out = 0;
    return E_NOINTERFACE;
}

/* ---- surfaces ---- */

RES_COMMON(surf, IDirect3DSurface9)
RES_PRIORITY(surf, IDirect3DSurface9)

static HRESULT STDMETHODCALLTYPE surf_QueryInterface(IDirect3DSurface9 *self, REFIID riid, void **out)
{
    static const GUID *const iids[] = {&IID_IUnknown, &IID_IDirect3DResource9, &IID_IDirect3DSurface9, 0};
    return query(self, riid, out, iids);
}

static HRESULT STDMETHODCALLTYPE surf_GetContainer(IDirect3DSurface9 *self, REFIID riid, void **out)
{
    struct vkd_surface *s = SURF(self);
    if (!out) return D3DERR_INVALIDCALL;
    if (s->tex) return IUnknown_QueryInterface((IUnknown *)s->tex, riid, out);
    if (s->swapchain) return IUnknown_QueryInterface((IUnknown *)s->swapchain, riid, out);
    return IUnknown_QueryInterface((IUnknown *)s->r.dev, riid, out);
}

static HRESULT STDMETHODCALLTYPE surf_GetDesc(IDirect3DSurface9 *self, D3DSURFACE_DESC *desc)
{
    struct vkd_surface *s = SURF(self);
    if (!desc) return D3DERR_INVALIDCALL;
    desc->Format = s->r.format;
    desc->Type = D3DRTYPE_SURFACE;
    desc->Usage = s->r.usage;
    desc->Pool = s->r.pool;
    desc->MultiSampleType = s->ms;
    desc->MultiSampleQuality = s->msq;
    desc->Width = s->w;
    desc->Height = s->h;
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE surf_LockRect(IDirect3DSurface9 *self, D3DLOCKED_RECT *lr, const RECT *rect, DWORD flags)
{
    struct vkd_surface *s = SURF(self);
    if (!lr) return D3DERR_INVALIDCALL;
    UINT x0 = 0, y0 = 0, x1 = s->w, y1 = s->h;
    if (rect) { x0 = (UINT)rect->left; y0 = (UINT)rect->top; x1 = (UINT)rect->right; y1 = (UINT)rect->bottom; }
    VKD_LOCK(s->r.dev);
    UINT pitch;
    HRESULT hr = lock_box(s, x0, y0, 0, x1, y1, 1, flags, &lr->pBits, &pitch, 0);
    VKD_UNLOCK(s->r.dev);
    if (FAILED(hr)) { lr->pBits = 0; lr->Pitch = 0; return hr; }
    lr->Pitch = (INT)pitch;
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE surf_UnlockRect(IDirect3DSurface9 *self)
{
    struct vkd_surface *s = SURF(self);
    VKD_LOCK(s->r.dev);
    HRESULT hr = unlock_box(s);
    VKD_UNLOCK(s->r.dev);
    return hr;
}

static HRESULT STDMETHODCALLTYPE surf_GetDC(IDirect3DSurface9 *self, HDC *out)
{
    struct vkd_surface *s = SURF(self);
    if (!out) return D3DERR_INVALIDCALL;
    *out = 0;
    D3DFORMAT f = s->r.format;
    int bpp = f == D3DFMT_X8R8G8B8 || f == D3DFMT_A8R8G8B8 ? 32
            : f == D3DFMT_R5G6B5 || f == D3DFMT_X1R5G5B5 || f == D3DFMT_A1R5G5B5 ? 16 : 0;
    if (!bpp || s->dc) return D3DERR_INVALIDCALL;
    D3DLOCKED_RECT lr;
    HRESULT hr = surf_LockRect(self, &lr, 0, 0);
    if (FAILED(hr)) return hr;
    struct { BITMAPINFOHEADER h; DWORD masks[3]; } bi;
    memset(&bi, 0, sizeof bi);
    bi.h.biSize = sizeof bi.h;
    bi.h.biWidth = (LONG)s->w;
    bi.h.biHeight = -(LONG)s->h;
    bi.h.biPlanes = 1;
    bi.h.biBitCount = (WORD)bpp;
    bi.h.biCompression = f == D3DFMT_R5G6B5 ? BI_BITFIELDS : BI_RGB;
    if (f == D3DFMT_R5G6B5) { bi.masks[0] = 0xf800; bi.masks[1] = 0x07e0; bi.masks[2] = 0x001f; }
    s->dc = w32.CreateCompatibleDC(0);
    s->bmp = s->dc ? w32.CreateDIBSection(s->dc, (BITMAPINFO *)&bi, DIB_RGB_COLORS, &s->bits, 0, 0) : 0;
    if (!s->bmp) {
        if (s->dc) w32.DeleteDC(s->dc);
        s->dc = 0;
        surf_UnlockRect(self);
        return D3DERR_INVALIDCALL;
    }
    UINT row = s->w * (UINT)bpp / 8, dpitch = (row + 3) & ~3u;
    for (UINT y = 0; y < s->h; y++)
        memcpy((uint8_t *)s->bits + y * dpitch, (uint8_t *)lr.pBits + y * (UINT)lr.Pitch, row);
    s->oldbmp = w32.SelectObject(s->dc, s->bmp);
    *out = s->dc;
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE surf_ReleaseDC(IDirect3DSurface9 *self, HDC dc)
{
    struct vkd_surface *s = SURF(self);
    if (!s->dc || dc != s->dc) return D3DERR_INVALIDCALL;
    w32.GdiFlush();
    UINT pitch, slice;
    uint8_t *cpu = vkd_surface_cpu(s, &pitch, &slice);
    int bpp = s->r.format == D3DFMT_X8R8G8B8 || s->r.format == D3DFMT_A8R8G8B8 ? 32 : 16;
    UINT row = s->w * (UINT)bpp / 8, dpitch = (row + 3) & ~3u;
    if (cpu)
        for (UINT y = 0; y < s->h; y++) memcpy(cpu + y * pitch, (uint8_t *)s->bits + y * dpitch, row);
    w32.SelectObject(s->dc, s->oldbmp);
    w32.DeleteObject(s->bmp);
    w32.DeleteDC(s->dc);
    s->dc = 0; s->bmp = 0; s->bits = 0;
    return surf_UnlockRect(self);
}

const IDirect3DSurface9Vtbl vkd_surface_vtbl = {
    .QueryInterface = surf_QueryInterface,
    .AddRef = surf_AddRef,
    .Release = surf_Release,
    .GetDevice = surf_GetDevice,
    .SetPrivateData = surf_SetPrivateData,
    .GetPrivateData = surf_GetPrivateData,
    .FreePrivateData = surf_FreePrivateData,
    .SetPriority = surf_SetPriority,
    .GetPriority = surf_GetPriority,
    .PreLoad = surf_PreLoad,
    .GetType = surf_GetType,
    .GetContainer = surf_GetContainer,
    .GetDesc = surf_GetDesc,
    .LockRect = surf_LockRect,
    .UnlockRect = surf_UnlockRect,
    .GetDC = surf_GetDC,
    .ReleaseDC = surf_ReleaseDC,
};

static void surface_free_views(struct vkd_surface *s)
{
    struct vkd_garbage g = {0};
    g.kind = G_VIEW;
    if (s->rtv) { g.h.view = s->rtv; vkd_defer(s->r.dev, &g); }
    if (s->rtv_srgb) { g.h.view = s->rtv_srgb; vkd_defer(s->r.dev, &g); }
    s->rtv = s->rtv_srgb = 0;
}

static void surface_destroy(struct vkd_res *r)
{
    untrack_resource(r);
    struct vkd_surface *s = SURF(r);
    struct vkd_device *d = r->dev;
    if (s->dc) { w32.SelectObject(s->dc, s->oldbmp); w32.DeleteObject(s->bmp); w32.DeleteDC(s->dc); }
    surface_free_views(s);
    if (s->img) vkd_image_destroy(d, s->img);
    vkp_free(s->shadow);
    priv_clear(&r->priv);
    vkp_free(s);
}

/* Device-owned surfaces (back buffers, auto depth stencil) are freed here. */
void vkd_surface_free(struct vkd_surface *s)
{
    if (!s) return;
    s->r.implicit = 1;
    surface_destroy(&s->r);
}

HRESULT vkd_create_surface(struct vkd_device *d, UINT w, UINT h, D3DFORMAT format, D3DMULTISAMPLE_TYPE ms, DWORD msq,
                           DWORD usage, D3DPOOL pool, int lockable, int discard, struct vkd_surface **out)
{
    *out = 0;
    const struct vkd_format *f = vkd_format(format);
    if (!f || !w || !h || format == D3DFMT_INDEX16 || format == D3DFMT_INDEX32 || format == D3DFMT_VERTEXDATA)
        return D3DERR_INVALIDCALL;
    struct vkd_surface *s = vkp_alloc(sizeof *s);
    if (!s) return E_OUTOFMEMORY;
    s->r.vtbl = &vkd_surface_vtbl;
    s->r.ref = 1;
    s->r.dev = d;
    s->r.type = D3DRTYPE_SURFACE;
    s->r.pool = pool;
    s->r.usage = usage;
    s->r.format = format;
    s->r.destroy = surface_destroy;
    s->w = w; s->h = h; s->d = 1;
    s->ms = ms; s->msq = msq;
    s->lockable = lockable;
    s->discard = discard;
    if (pool == D3DPOOL_DEFAULT && f->vk != VK_FORMAT_UNDEFINED) {
        int attach = !!(usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL));
        s->img = vkd_image_create(d, f, VK_IMAGE_TYPE_2D, w, h, 1, 1, 1, vkd_samples(ms, msq), 0, attach);
        if (!s->img) { vkp_free(s); return D3DERR_OUTOFVIDEOMEMORY; }
    }
    track_resource(&s->r);
    *out = s;
    return D3D_OK;
}

/* ---- volumes ---- */

RES_COMMON(vol, IDirect3DVolume9)

static HRESULT STDMETHODCALLTYPE vol_QueryInterface(IDirect3DVolume9 *self, REFIID riid, void **out)
{
    static const GUID *const iids[] = {&IID_IUnknown, &IID_IDirect3DVolume9, 0};
    return query(self, riid, out, iids);
}

static HRESULT STDMETHODCALLTYPE vol_GetContainer(IDirect3DVolume9 *self, REFIID riid, void **out)
{
    if (!out) return D3DERR_INVALIDCALL;
    return IUnknown_QueryInterface((IUnknown *)SURF(self)->tex, riid, out);
}

static HRESULT STDMETHODCALLTYPE vol_GetDesc(IDirect3DVolume9 *self, D3DVOLUME_DESC *desc)
{
    struct vkd_surface *s = SURF(self);
    if (!desc) return D3DERR_INVALIDCALL;
    desc->Format = s->r.format;
    desc->Type = D3DRTYPE_VOLUME;
    desc->Usage = s->r.usage;
    desc->Pool = s->r.pool;
    desc->Width = s->w;
    desc->Height = s->h;
    desc->Depth = s->d;
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE vol_LockBox(IDirect3DVolume9 *self, D3DLOCKED_BOX *lb, const D3DBOX *box, DWORD flags)
{
    struct vkd_surface *s = SURF(self);
    if (!lb) return D3DERR_INVALIDCALL;
    UINT x0 = 0, y0 = 0, z0 = 0, x1 = s->w, y1 = s->h, z1 = s->d;
    if (box) { x0 = box->Left; y0 = box->Top; z0 = box->Front; x1 = box->Right; y1 = box->Bottom; z1 = box->Back; }
    VKD_LOCK(s->r.dev);
    UINT pitch, slice;
    HRESULT hr = lock_box(s, x0, y0, z0, x1, y1, z1, flags, &lb->pBits, &pitch, &slice);
    VKD_UNLOCK(s->r.dev);
    if (FAILED(hr)) { lb->pBits = 0; lb->RowPitch = lb->SlicePitch = 0; return hr; }
    lb->RowPitch = (INT)pitch;
    lb->SlicePitch = (INT)slice;
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE vol_UnlockBox(IDirect3DVolume9 *self)
{
    struct vkd_surface *s = SURF(self);
    VKD_LOCK(s->r.dev);
    HRESULT hr = unlock_box(s);
    VKD_UNLOCK(s->r.dev);
    return hr;
}

const IDirect3DVolume9Vtbl vkd_volume_vtbl = {
    .QueryInterface = vol_QueryInterface,
    .AddRef = vol_AddRef,
    .Release = vol_Release,
    .GetDevice = vol_GetDevice,
    .SetPrivateData = vol_SetPrivateData,
    .GetPrivateData = vol_GetPrivateData,
    .FreePrivateData = vol_FreePrivateData,
    .GetContainer = vol_GetContainer,
    .GetDesc = vol_GetDesc,
    .LockBox = vol_LockBox,
    .UnlockBox = vol_UnlockBox,
};

/* ---- textures ---- */

static void texture_destroy(struct vkd_res *r)
{
    untrack_resource(r);
    struct vkd_texture *t = TEX(r);
    struct vkd_device *d = r->dev;
    for (uint32_t i = 0; i < t->nsub; i++) {
        struct vkd_surface *s = t->surf[i];
        if (!s) continue;
        surface_free_views(s);
        priv_clear(&s->r.priv);
        vkp_free(s);
    }
    struct vkd_garbage g = {0};
    g.kind = G_VIEW;
    if (t->view_lod) { g.h.view = t->view_lod; vkd_defer(d, &g); }
    if (t->view_lod_srgb) { g.h.view = t->view_lod_srgb; vkd_defer(d, &g); }
    if (t->img) vkd_image_destroy(d, t->img);
    vkp_free(t->surf);
    vkp_free(t->sub_off);
    vkp_free(t->shadow);
    priv_clear(&r->priv);
    vkp_free(t);
}

HRESULT vkd_create_texture(struct vkd_device *d, D3DRESOURCETYPE type, UINT w, UINT h, UINT depth, UINT levels,
                           DWORD usage, D3DFORMAT format, D3DPOOL pool, struct vkd_texture **out)
{
    *out = 0;
    const struct vkd_format *f = vkd_format(format);
    if (!f || f->vk == VK_FORMAT_UNDEFINED || !w || !h || !depth) return D3DERR_INVALIDCALL;
    if ((usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL)) && pool != D3DPOOL_DEFAULT) return D3DERR_INVALIDCALL;
    if ((usage & D3DUSAGE_DYNAMIC) && pool == D3DPOOL_MANAGED) return D3DERR_INVALIDCALL;
    int cube = type == D3DRTYPE_CUBETEXTURE, vol = type == D3DRTYPE_VOLUMETEXTURE;
    if (cube) h = w;
    if (!vol) depth = 1;
    UINT full = full_chain(w, h, depth);
    int autogen = !!(usage & D3DUSAGE_AUTOGENMIPMAP);
    if (autogen) {
        if (pool == D3DPOOL_SYSTEMMEM || (levels != 0 && levels != 1)) return D3DERR_INVALIDCALL;
        if (f->block > 1 || f->depth || f->stencil ||
            !vkd_format_supported(f, VK_FORMAT_FEATURE_BLIT_SRC_BIT | VK_FORMAT_FEATURE_BLIT_DST_BIT, 1)) {
            autogen = 0;
            usage &= ~(DWORD)D3DUSAGE_AUTOGENMIPMAP;
            levels = 1;
        } else {
            levels = full;
        }
    }
    if (!levels || levels > full) levels = levels ? full : full;
    struct vkd_texture *t = vkp_alloc(sizeof *t);
    if (!t) return E_OUTOFMEMORY;
    t->r.vtbl = cube ? (const void *)&vkd_cube_vtbl : vol ? (const void *)&vkd_voltex_vtbl : (const void *)&vkd_texture_vtbl;
    t->r.ref = 1;
    t->r.dev = d;
    t->r.type = type;
    t->r.pool = pool;
    t->r.usage = usage;
    t->r.format = format;
    t->r.destroy = texture_destroy;
    t->fmt = f;
    t->w = w; t->h = h; t->d = depth;
    t->levels = levels;
    t->faces = cube ? 6 : 1;
    t->autogen = autogen;
    t->autogen_filter = D3DTEXF_LINEAR;
    t->dirty = 1;
    t->nsub = t->faces * levels;
    t->surf = vkp_alloc(t->nsub * sizeof *t->surf);
    t->sub_off = vkp_alloc(t->nsub * sizeof *t->sub_off);
    if (!t->surf || !t->sub_off) goto oom;

    if (pool == D3DPOOL_DEFAULT || pool == D3DPOOL_MANAGED) {
        int attach = !!(usage & (D3DUSAGE_RENDERTARGET | D3DUSAGE_DEPTHSTENCIL));
        t->img = vkd_image_create(d, f, vol ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D, w, h, depth, levels, t->faces,
                                  VK_SAMPLE_COUNT_1_BIT, cube, attach);
        if (!t->img) {
            vkp_free(t->surf); vkp_free(t->sub_off); vkp_free(t);
            return D3DERR_OUTOFVIDEOMEMORY;
        }
    }
    int shadow = pool != D3DPOOL_DEFAULT || (usage & D3DUSAGE_DYNAMIC);
    size_t total = 0;
    for (UINT face = 0; face < t->faces; face++)
        for (UINT l = 0; l < levels; l++) {
            uint32_t sub = face * levels + l;
            UINT lw = mip(w, l), lh = mip(h, l), ld = mip(depth, l), slice;
            vkd_layout(f, lw, lh, ld, 0, &slice, 0);
            t->sub_off[sub] = (uint32_t)total;
            total += ((size_t)slice * ld + 15) & ~(size_t)15;
            struct vkd_surface *s = vkp_alloc(sizeof *s);
            if (!s) goto oom;
            s->r.vtbl = vol ? (const void *)&vkd_volume_vtbl : (const void *)&vkd_surface_vtbl;
            s->r.dev = d;
            s->r.type = vol ? D3DRTYPE_VOLUME : D3DRTYPE_SURFACE;
            s->r.pool = pool;
            s->r.usage = usage;
            s->r.format = format;
            s->tex = t;
            s->sub = sub;
            s->level = l;
            s->layer = face;
            s->w = lw; s->h = lh; s->d = ld;
            s->ms = D3DMULTISAMPLE_NONE;
            t->surf[sub] = s;
        }
    if (shadow) {
        t->shadow = vkp_alloc(total ? total : 16);
        if (!t->shadow) goto oom;
    }
    track_resource(&t->r);
    *out = t;
    return D3D_OK;
oom:
    if (t->img) vkd_image_destroy(d, t->img);
    if (t->surf) for (uint32_t i = 0; i < t->nsub; i++) vkp_free(t->surf[i]);
    vkp_free(t->surf);
    vkp_free(t->sub_off);
    vkp_free(t->shadow);
    vkp_free(t);
    return E_OUTOFMEMORY;
}

/* image view the samplers use: the LOD-clamped one for SetLOD */
VkImageView vkd_texture_view(struct vkd_texture *t, int srgb)
{
    struct vkd_image *img = t->img;
    if (!img) return 0;
    srgb = srgb && img->view_srgb;
    if (t->lod && t->lod < img->levels && !t->autogen) {
        VkImageView *v = srgb ? &t->view_lod_srgb : &t->view_lod;
        if (!*v)
            *v = vkd_image_view(img, srgb ? img->fmt->vk_srgb : img->fmt->vk, img->view_type, t->lod,
                                img->levels - t->lod, 0, img->layers, 1);
        if (*v) return *v;
    }
    return srgb ? img->view_srgb : img->view;
}

#define TEX_BASE(pfx, I)                                                                                           \
    RES_COMMON(pfx, I)                                                                                             \
    RES_PRIORITY(pfx, I)                                                                                           \
    static DWORD STDMETHODCALLTYPE pfx##_SetLOD(I *self, DWORD lod)                                                \
    {                                                                                                              \
        struct vkd_texture *t = TEX(self);                                                                         \
        DWORD old = t->lod;                                                                                        \
        if (t->r.pool != D3DPOOL_MANAGED) return 0;                                                                \
        if (lod >= t->levels) lod = t->levels - 1;                                                                 \
        if (lod != t->lod) {                                                                                       \
            VKD_LOCK(t->r.dev);                                                                                    \
            struct vkd_garbage g = {0};                                                                            \
            g.kind = G_VIEW;                                                                                       \
            if (t->view_lod) { g.h.view = t->view_lod; vkd_defer(t->r.dev, &g); }                                  \
            if (t->view_lod_srgb) { g.h.view = t->view_lod_srgb; vkd_defer(t->r.dev, &g); }                        \
            t->view_lod = t->view_lod_srgb = 0;                                                                    \
            t->lod = lod;                                                                                          \
            t->r.dev->dirty |= DIRTY_SAMPLERS;                                                                     \
            VKD_UNLOCK(t->r.dev);                                                                                  \
        }                                                                                                          \
        return old;                                                                                                \
    }                                                                                                              \
    static DWORD STDMETHODCALLTYPE pfx##_GetLOD(I *self) { return TEX(self)->r.pool == D3DPOOL_MANAGED ? TEX(self)->lod : 0; } \
    static DWORD STDMETHODCALLTYPE pfx##_GetLevelCount(I *self) { return TEX(self)->autogen ? 1 : TEX(self)->levels; } \
    static HRESULT STDMETHODCALLTYPE pfx##_SetAutoGenFilterType(I *self, D3DTEXTUREFILTERTYPE f)                   \
    {                                                                                                              \
        struct vkd_texture *t = TEX(self);                                                                         \
        if (f == D3DTEXF_NONE || f > D3DTEXF_ANISOTROPIC) return D3DERR_INVALIDCALL;                               \
        if (!(t->r.usage & D3DUSAGE_AUTOGENMIPMAP)) return D3DERR_INVALIDCALL;                                    \
        if (f != t->autogen_filter) { t->autogen_filter = f; t->autogen_dirty = 1; }                               \
        return D3D_OK;                                                                                             \
    }                                                                                                              \
    static D3DTEXTUREFILTERTYPE STDMETHODCALLTYPE pfx##_GetAutoGenFilterType(I *self)                              \
    {                                                                                                              \
        return (TEX(self)->r.usage & D3DUSAGE_AUTOGENMIPMAP) ? TEX(self)->autogen_filter : D3DTEXF_NONE;           \
    }                                                                                                              \
    static void STDMETHODCALLTYPE pfx##_GenerateMipSubLevels(I *self)                                              \
    {                                                                                                              \
        struct vkd_texture *t = TEX(self);                                                                         \
        if (!t->autogen) return;                                                                                   \
        VKD_LOCK(t->r.dev);                                                                                        \
        vkd_texture_genmips(t);                                                                                    \
        VKD_UNLOCK(t->r.dev);                                                                                      \
    }

static HRESULT level_desc(struct vkd_texture *t, uint32_t sub, D3DSURFACE_DESC *desc)
{
    if (!desc || sub >= t->nsub) return D3DERR_INVALIDCALL;
    return surf_GetDesc((IDirect3DSurface9 *)t->surf[sub], desc);
}

static HRESULT get_level(struct vkd_texture *t, uint32_t sub, void **out)
{
    if (!out) return D3DERR_INVALIDCALL;
    if (sub >= t->nsub) { *out = 0; return D3DERR_INVALIDCALL; }
    *out = t->surf[sub];
    vkd_res_addref(&t->r);
    return D3D_OK;
}

static HRESULT lock_level(struct vkd_texture *t, uint32_t sub, D3DLOCKED_RECT *lr, const RECT *rect, DWORD flags)
{
    if (sub >= t->nsub || !lr) return D3DERR_INVALIDCALL;
    return surf_LockRect((IDirect3DSurface9 *)t->surf[sub], lr, rect, flags);
}

static HRESULT unlock_level(struct vkd_texture *t, uint32_t sub)
{
    if (sub >= t->nsub) return D3DERR_INVALIDCALL;
    return surf_UnlockRect((IDirect3DSurface9 *)t->surf[sub]);
}

/* 2D */
TEX_BASE(tex, IDirect3DTexture9)

static HRESULT STDMETHODCALLTYPE tex_QueryInterface(IDirect3DTexture9 *self, REFIID riid, void **out)
{
    static const GUID *const iids[] = {&IID_IUnknown, &IID_IDirect3DResource9, &IID_IDirect3DBaseTexture9,
                                       &IID_IDirect3DTexture9, 0};
    return query(self, riid, out, iids);
}
static HRESULT STDMETHODCALLTYPE tex_GetLevelDesc(IDirect3DTexture9 *self, UINT level, D3DSURFACE_DESC *desc)
{
    return level_desc(TEX(self), level, desc);
}
static HRESULT STDMETHODCALLTYPE tex_GetSurfaceLevel(IDirect3DTexture9 *self, UINT level, IDirect3DSurface9 **out)
{
    return get_level(TEX(self), level, (void **)out);
}
static HRESULT STDMETHODCALLTYPE tex_LockRect(IDirect3DTexture9 *self, UINT level, D3DLOCKED_RECT *lr, const RECT *rect,
                                              DWORD flags)
{
    return lock_level(TEX(self), level, lr, rect, flags);
}
static HRESULT STDMETHODCALLTYPE tex_UnlockRect(IDirect3DTexture9 *self, UINT level)
{
    return unlock_level(TEX(self), level);
}
static HRESULT STDMETHODCALLTYPE tex_AddDirtyRect(IDirect3DTexture9 *self, const RECT *rect)
{
    TEX(self)->dirty = 1;
    return D3D_OK;
}

const IDirect3DTexture9Vtbl vkd_texture_vtbl = {
    .QueryInterface = tex_QueryInterface,
    .AddRef = tex_AddRef,
    .Release = tex_Release,
    .GetDevice = tex_GetDevice,
    .SetPrivateData = tex_SetPrivateData,
    .GetPrivateData = tex_GetPrivateData,
    .FreePrivateData = tex_FreePrivateData,
    .SetPriority = tex_SetPriority,
    .GetPriority = tex_GetPriority,
    .PreLoad = tex_PreLoad,
    .GetType = tex_GetType,
    .SetLOD = tex_SetLOD,
    .GetLOD = tex_GetLOD,
    .GetLevelCount = tex_GetLevelCount,
    .SetAutoGenFilterType = tex_SetAutoGenFilterType,
    .GetAutoGenFilterType = tex_GetAutoGenFilterType,
    .GenerateMipSubLevels = tex_GenerateMipSubLevels,
    .GetLevelDesc = tex_GetLevelDesc,
    .GetSurfaceLevel = tex_GetSurfaceLevel,
    .LockRect = tex_LockRect,
    .UnlockRect = tex_UnlockRect,
    .AddDirtyRect = tex_AddDirtyRect,
};

/* cube */
TEX_BASE(cube, IDirect3DCubeTexture9)

static HRESULT STDMETHODCALLTYPE cube_QueryInterface(IDirect3DCubeTexture9 *self, REFIID riid, void **out)
{
    static const GUID *const iids[] = {&IID_IUnknown, &IID_IDirect3DResource9, &IID_IDirect3DBaseTexture9,
                                       &IID_IDirect3DCubeTexture9, 0};
    return query(self, riid, out, iids);
}
static HRESULT STDMETHODCALLTYPE cube_GetLevelDesc(IDirect3DCubeTexture9 *self, UINT level, D3DSURFACE_DESC *desc)
{
    return level_desc(TEX(self), level, desc);
}
static uint32_t cube_sub(struct vkd_texture *t, D3DCUBEMAP_FACES face, UINT level)
{
    if ((UINT)face > 5 || level >= t->levels) return ~0u;
    return (uint32_t)face * t->levels + level;
}
static HRESULT STDMETHODCALLTYPE cube_GetCubeMapSurface(IDirect3DCubeTexture9 *self, D3DCUBEMAP_FACES face, UINT level,
                                                        IDirect3DSurface9 **out)
{
    return get_level(TEX(self), cube_sub(TEX(self), face, level), (void **)out);
}
static HRESULT STDMETHODCALLTYPE cube_LockRect(IDirect3DCubeTexture9 *self, D3DCUBEMAP_FACES face, UINT level,
                                               D3DLOCKED_RECT *lr, const RECT *rect, DWORD flags)
{
    return lock_level(TEX(self), cube_sub(TEX(self), face, level), lr, rect, flags);
}
static HRESULT STDMETHODCALLTYPE cube_UnlockRect(IDirect3DCubeTexture9 *self, D3DCUBEMAP_FACES face, UINT level)
{
    return unlock_level(TEX(self), cube_sub(TEX(self), face, level));
}
static HRESULT STDMETHODCALLTYPE cube_AddDirtyRect(IDirect3DCubeTexture9 *self, D3DCUBEMAP_FACES face, const RECT *rect)
{
    TEX(self)->dirty = 1;
    return D3D_OK;
}

const IDirect3DCubeTexture9Vtbl vkd_cube_vtbl = {
    .QueryInterface = cube_QueryInterface,
    .AddRef = cube_AddRef,
    .Release = cube_Release,
    .GetDevice = cube_GetDevice,
    .SetPrivateData = cube_SetPrivateData,
    .GetPrivateData = cube_GetPrivateData,
    .FreePrivateData = cube_FreePrivateData,
    .SetPriority = cube_SetPriority,
    .GetPriority = cube_GetPriority,
    .PreLoad = cube_PreLoad,
    .GetType = cube_GetType,
    .SetLOD = cube_SetLOD,
    .GetLOD = cube_GetLOD,
    .GetLevelCount = cube_GetLevelCount,
    .SetAutoGenFilterType = cube_SetAutoGenFilterType,
    .GetAutoGenFilterType = cube_GetAutoGenFilterType,
    .GenerateMipSubLevels = cube_GenerateMipSubLevels,
    .GetLevelDesc = cube_GetLevelDesc,
    .GetCubeMapSurface = cube_GetCubeMapSurface,
    .LockRect = cube_LockRect,
    .UnlockRect = cube_UnlockRect,
    .AddDirtyRect = cube_AddDirtyRect,
};

/* volume */
TEX_BASE(vtex, IDirect3DVolumeTexture9)

static HRESULT STDMETHODCALLTYPE vtex_QueryInterface(IDirect3DVolumeTexture9 *self, REFIID riid, void **out)
{
    static const GUID *const iids[] = {&IID_IUnknown, &IID_IDirect3DResource9, &IID_IDirect3DBaseTexture9,
                                       &IID_IDirect3DVolumeTexture9, 0};
    return query(self, riid, out, iids);
}
static HRESULT STDMETHODCALLTYPE vtex_GetLevelDesc(IDirect3DVolumeTexture9 *self, UINT level, D3DVOLUME_DESC *desc)
{
    struct vkd_texture *t = TEX(self);
    if (level >= t->nsub || !desc) return D3DERR_INVALIDCALL;
    return vol_GetDesc((IDirect3DVolume9 *)t->surf[level], desc);
}
static HRESULT STDMETHODCALLTYPE vtex_GetVolumeLevel(IDirect3DVolumeTexture9 *self, UINT level, IDirect3DVolume9 **out)
{
    return get_level(TEX(self), level, (void **)out);
}
static HRESULT STDMETHODCALLTYPE vtex_LockBox(IDirect3DVolumeTexture9 *self, UINT level, D3DLOCKED_BOX *lb,
                                              const D3DBOX *box, DWORD flags)
{
    struct vkd_texture *t = TEX(self);
    if (level >= t->nsub) return D3DERR_INVALIDCALL;
    return vol_LockBox((IDirect3DVolume9 *)t->surf[level], lb, box, flags);
}
static HRESULT STDMETHODCALLTYPE vtex_UnlockBox(IDirect3DVolumeTexture9 *self, UINT level)
{
    struct vkd_texture *t = TEX(self);
    if (level >= t->nsub) return D3DERR_INVALIDCALL;
    return vol_UnlockBox((IDirect3DVolume9 *)t->surf[level]);
}
static HRESULT STDMETHODCALLTYPE vtex_AddDirtyBox(IDirect3DVolumeTexture9 *self, const D3DBOX *box)
{
    TEX(self)->dirty = 1;
    return D3D_OK;
}

const IDirect3DVolumeTexture9Vtbl vkd_voltex_vtbl = {
    .QueryInterface = vtex_QueryInterface,
    .AddRef = vtex_AddRef,
    .Release = vtex_Release,
    .GetDevice = vtex_GetDevice,
    .SetPrivateData = vtex_SetPrivateData,
    .GetPrivateData = vtex_GetPrivateData,
    .FreePrivateData = vtex_FreePrivateData,
    .SetPriority = vtex_SetPriority,
    .GetPriority = vtex_GetPriority,
    .PreLoad = vtex_PreLoad,
    .GetType = vtex_GetType,
    .SetLOD = vtex_SetLOD,
    .GetLOD = vtex_GetLOD,
    .GetLevelCount = vtex_GetLevelCount,
    .SetAutoGenFilterType = vtex_SetAutoGenFilterType,
    .GetAutoGenFilterType = vtex_GetAutoGenFilterType,
    .GenerateMipSubLevels = vtex_GenerateMipSubLevels,
    .GetLevelDesc = vtex_GetLevelDesc,
    .GetVolumeLevel = vtex_GetVolumeLevel,
    .LockBox = vtex_LockBox,
    .UnlockBox = vtex_UnlockBox,
    .AddDirtyBox = vtex_AddDirtyBox,
};

/* ---- buffers ---- */

static int busy(struct vkd_device *d, uint64_t seq)
{
    if (seq <= d->completed) return 0;
    if (seq < d->seq) vkd_poll(d);
    return seq > d->completed;
}

void vkd_buffer_used(struct vkd_buffer *b) { b->cur.use_seq = b->r.dev->seq; }

/* D3DLOCK_DISCARD: a fresh copy when the GPU may still read the current one */
static int rename(struct vkd_buffer *b)
{
    struct vkd_device *d = b->r.dev;
    if (!busy(d, b->cur.use_seq)) return 1;
    for (int i = 0; i < b->nspare; i++)
        if (!busy(d, b->spare[i].use_seq)) {
            struct vkd_buf t = b->spare[i];
            b->spare[i] = b->cur;
            b->cur = t;
            return 1;
        }
    struct vkd_buf n;
    if (b->nspare >= 64) {
        /* bound the copies: wait for the oldest */
        int oldest = 0;
        for (int i = 1; i < b->nspare; i++)
            if (b->spare[i].use_seq < b->spare[oldest].use_seq) oldest = i;
        vkd_wait_seq(d, b->spare[oldest].use_seq);
        n = b->spare[oldest];
        b->spare[oldest] = b->cur;
        b->cur = n;
        return 1;
    }
    if (!vkd_buf_alloc(d, b->size, 1, &n)) return 0;
    if (b->nspare == b->capspare) {
        int cap = b->capspare ? b->capspare * 2 : 4;
        struct vkd_buf *s = vkp_realloc(b->spare, (size_t)cap * sizeof *s);
        if (!s) { vkd_buf_free(d, &n); return 0; }
        b->spare = s; b->capspare = cap;
    }
    b->spare[b->nspare++] = b->cur;
    b->cur = n;
    return 1;
}

static HRESULT buffer_lock(struct vkd_buffer *b, UINT offset, UINT size, void **data, DWORD flags)
{
    if (!data) return D3DERR_INVALIDCALL;
    struct vkd_device *d = b->r.dev;
    if (!size) size = offset < b->size ? b->size - offset : 0;
    VKD_LOCK(d);
    if (b->host) {
        if ((flags & D3DLOCK_DISCARD) && (b->r.usage & D3DUSAGE_DYNAMIC)) {
            if (!rename(b)) { VKD_UNLOCK(d); *data = 0; return D3DERR_OUTOFVIDEOMEMORY; }
        } else if (!(flags & (D3DLOCK_NOOVERWRITE | D3DLOCK_READONLY)) && busy(d, b->cur.use_seq)) {
            vkd_wait_seq(d, b->cur.use_seq);
        }
        *data = b->cur.map + offset;
    } else {
        *data = b->shadow + offset;
        if (!(flags & D3DLOCK_READONLY) && size) {
            uint32_t hi = VKP_MIN(offset + size, b->size);
            if (b->dirty_hi <= b->dirty_lo) { b->dirty_lo = offset; b->dirty_hi = hi; }
            else { b->dirty_lo = VKP_MIN(b->dirty_lo, offset); b->dirty_hi = VKP_MAX(b->dirty_hi, hi); }
        }
    }
    b->locks++;
    VKD_UNLOCK(d);
    return D3D_OK;
}

/* upload the dirty range of a static buffer */
void vkd_buffer_flush(struct vkd_buffer *b)
{
    if (b->host || b->dirty_hi <= b->dirty_lo) return;
    struct vkd_device *d = b->r.dev;
    uint32_t lo = b->dirty_lo & ~3u, hi = VKP_MIN((b->dirty_hi + 3) & ~3u, (b->size + 3) & ~3u);
    b->dirty_lo = b->dirty_hi = 0;
    struct vkd_buf ring;
    if (!vkd_ring(d, hi - lo, 16, &ring)) return;
    uint32_t n = VKP_MIN(hi, b->size) - lo;
    memcpy(ring.map, b->shadow + lo, n);
    if (hi - lo > n) memset(ring.map + n, 0, hi - lo - n);
    VkBufferCopy c = {ring.off, b->cur.off + lo, hi - lo};
    VkCommandBuffer cmd = vkd_upload_cmd(d, &b->cur.use_seq);
    vkCmdCopyBuffer(cmd, ring.buf, b->cur.buf, 1, &c);
}

static HRESULT buffer_unlock(struct vkd_buffer *b)
{
    struct vkd_device *d = b->r.dev;
    VKD_LOCK(d);
    if (b->locks > 0 && !--b->locks) vkd_buffer_flush(b);
    VKD_UNLOCK(d);
    return D3D_OK;
}

static void buffer_destroy(struct vkd_res *r)
{
    untrack_resource(r);
    struct vkd_buffer *b = BUF(r);
    struct vkd_device *d = r->dev;
    vkd_buf_free(d, &b->cur);
    for (int i = 0; i < b->nspare; i++) vkd_buf_free(d, &b->spare[i]);
    vkp_free(b->spare);
    vkp_free(b->shadow);
    priv_clear(&r->priv);
    vkp_free(b);
}

HRESULT vkd_create_buffer(struct vkd_device *d, UINT size, DWORD usage, DWORD fvf, D3DFORMAT f, D3DPOOL pool,
                          int index, struct vkd_buffer **out)
{
    *out = 0;
    if (!size) return D3DERR_INVALIDCALL;
    if (index && f != D3DFMT_INDEX16 && f != D3DFMT_INDEX32) return D3DERR_INVALIDCALL;
    if (pool == D3DPOOL_SCRATCH) return D3DERR_INVALIDCALL;
    struct vkd_buffer *b = vkp_alloc(sizeof *b);
    if (!b) return E_OUTOFMEMORY;
    b->r.vtbl = index ? (const void *)&vkd_ib_vtbl : (const void *)&vkd_vb_vtbl;
    b->r.ref = 1;
    b->r.dev = d;
    b->r.type = index ? D3DRTYPE_INDEXBUFFER : D3DRTYPE_VERTEXBUFFER;
    b->r.pool = pool;
    b->r.usage = usage;
    b->r.format = index ? f : D3DFMT_VERTEXDATA;
    b->r.destroy = buffer_destroy;
    b->size = size;
    b->fvf = fvf;
    b->host = (usage & D3DUSAGE_DYNAMIC) || pool == D3DPOOL_SYSTEMMEM;
    if (!vkd_buf_alloc(d, size, b->host, &b->cur)) { vkp_free(b); return D3DERR_OUTOFVIDEOMEMORY; }
    if (b->host) {
        memset(b->cur.map, 0, size);
    } else {
        b->shadow = vkp_alloc(size);
        if (!b->shadow) { vkd_buf_free(d, &b->cur); vkp_free(b); return E_OUTOFMEMORY; }
        b->dirty_lo = 0; b->dirty_hi = size;
        vkd_buffer_flush(b);
    }
    track_resource(&b->r);
    *out = b;
    return D3D_OK;
}

RES_COMMON(vb, IDirect3DVertexBuffer9)
RES_PRIORITY(vb, IDirect3DVertexBuffer9)

static HRESULT STDMETHODCALLTYPE vb_QueryInterface(IDirect3DVertexBuffer9 *self, REFIID riid, void **out)
{
    static const GUID *const iids[] = {&IID_IUnknown, &IID_IDirect3DResource9, &IID_IDirect3DVertexBuffer9, 0};
    return query(self, riid, out, iids);
}
static HRESULT STDMETHODCALLTYPE vb_Lock(IDirect3DVertexBuffer9 *self, UINT off, UINT size, void **data, DWORD flags)
{
    return buffer_lock(BUF(self), off, size, data, flags);
}
static HRESULT STDMETHODCALLTYPE vb_Unlock(IDirect3DVertexBuffer9 *self) { return buffer_unlock(BUF(self)); }
static HRESULT STDMETHODCALLTYPE vb_GetDesc(IDirect3DVertexBuffer9 *self, D3DVERTEXBUFFER_DESC *desc)
{
    struct vkd_buffer *b = BUF(self);
    if (!desc) return D3DERR_INVALIDCALL;
    desc->Format = D3DFMT_VERTEXDATA;
    desc->Type = D3DRTYPE_VERTEXBUFFER;
    desc->Usage = b->r.usage;
    desc->Pool = b->r.pool;
    desc->Size = b->size;
    desc->FVF = b->fvf;
    return D3D_OK;
}

const IDirect3DVertexBuffer9Vtbl vkd_vb_vtbl = {
    .QueryInterface = vb_QueryInterface,
    .AddRef = vb_AddRef,
    .Release = vb_Release,
    .GetDevice = vb_GetDevice,
    .SetPrivateData = vb_SetPrivateData,
    .GetPrivateData = vb_GetPrivateData,
    .FreePrivateData = vb_FreePrivateData,
    .SetPriority = vb_SetPriority,
    .GetPriority = vb_GetPriority,
    .PreLoad = vb_PreLoad,
    .GetType = vb_GetType,
    .Lock = vb_Lock,
    .Unlock = vb_Unlock,
    .GetDesc = vb_GetDesc,
};

RES_COMMON(ib, IDirect3DIndexBuffer9)
RES_PRIORITY(ib, IDirect3DIndexBuffer9)

static HRESULT STDMETHODCALLTYPE ib_QueryInterface(IDirect3DIndexBuffer9 *self, REFIID riid, void **out)
{
    static const GUID *const iids[] = {&IID_IUnknown, &IID_IDirect3DResource9, &IID_IDirect3DIndexBuffer9, 0};
    return query(self, riid, out, iids);
}
static HRESULT STDMETHODCALLTYPE ib_Lock(IDirect3DIndexBuffer9 *self, UINT off, UINT size, void **data, DWORD flags)
{
    return buffer_lock(BUF(self), off, size, data, flags);
}
static HRESULT STDMETHODCALLTYPE ib_Unlock(IDirect3DIndexBuffer9 *self) { return buffer_unlock(BUF(self)); }
static HRESULT STDMETHODCALLTYPE ib_GetDesc(IDirect3DIndexBuffer9 *self, D3DINDEXBUFFER_DESC *desc)
{
    struct vkd_buffer *b = BUF(self);
    if (!desc) return D3DERR_INVALIDCALL;
    desc->Format = b->r.format;
    desc->Type = D3DRTYPE_INDEXBUFFER;
    desc->Usage = b->r.usage;
    desc->Pool = b->r.pool;
    desc->Size = b->size;
    return D3D_OK;
}

const IDirect3DIndexBuffer9Vtbl vkd_ib_vtbl = {
    .QueryInterface = ib_QueryInterface,
    .AddRef = ib_AddRef,
    .Release = ib_Release,
    .GetDevice = ib_GetDevice,
    .SetPrivateData = ib_SetPrivateData,
    .GetPrivateData = ib_GetPrivateData,
    .FreePrivateData = ib_FreePrivateData,
    .SetPriority = ib_SetPriority,
    .GetPriority = ib_GetPriority,
    .PreLoad = ib_PreLoad,
    .GetType = ib_GetType,
    .Lock = ib_Lock,
    .Unlock = ib_Unlock,
    .GetDesc = ib_GetDesc,
};
