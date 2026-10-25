/* Direct3D 9 on Vulkan: internal header shared by the backend files. */
#ifndef BFV_VKD3D9_H
#define BFV_VKD3D9_H
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"

#define CINTERFACE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <d3d9.h>
#define VK_NO_PROTOTYPES
#define VK_USE_PLATFORM_WIN32_KHR
#include <vulkan/vulkan.h>
#include "vkp.h"
#include "dxso.h"
#include "ff.h"

/* ---- Vulkan entry points ---- */

#define VKD_GLOBAL_FUNCS(X) \
    X(vkCreateInstance) X(vkEnumerateInstanceExtensionProperties) X(vkEnumerateInstanceLayerProperties) \
    X(vkEnumerateInstanceVersion)
#define VKD_INSTANCE_FUNCS(X) \
    X(vkDestroyInstance) X(vkEnumeratePhysicalDevices) X(vkGetPhysicalDeviceProperties) \
    X(vkGetPhysicalDeviceProperties2) X(vkGetPhysicalDeviceFeatures2) X(vkGetPhysicalDeviceMemoryProperties) \
    X(vkGetPhysicalDeviceQueueFamilyProperties) X(vkGetPhysicalDeviceFormatProperties) \
    X(vkGetPhysicalDeviceImageFormatProperties) X(vkEnumerateDeviceExtensionProperties) X(vkCreateDevice) \
    X(vkGetDeviceProcAddr) X(vkCreateWin32SurfaceKHR) X(vkDestroySurfaceKHR) \
    X(vkGetPhysicalDeviceSurfaceSupportKHR) X(vkGetPhysicalDeviceSurfaceCapabilitiesKHR) \
    X(vkGetPhysicalDeviceSurfaceFormatsKHR) X(vkGetPhysicalDeviceSurfacePresentModesKHR)
#define VKD_DEVICE_FUNCS(X) \
    X(vkDestroyDevice) X(vkGetDeviceQueue) X(vkDeviceWaitIdle) X(vkQueueSubmit) X(vkQueueWaitIdle) \
    X(vkAllocateMemory) X(vkFreeMemory) X(vkMapMemory) X(vkUnmapMemory) \
    X(vkCreateBuffer) X(vkDestroyBuffer) X(vkGetBufferMemoryRequirements) X(vkBindBufferMemory) \
    X(vkCreateImage) X(vkDestroyImage) X(vkGetImageMemoryRequirements) X(vkBindImageMemory) \
    X(vkCreateImageView) X(vkDestroyImageView) X(vkCreateSampler) X(vkDestroySampler) \
    X(vkCreateShaderModule) X(vkDestroyShaderModule) X(vkCreatePipelineCache) X(vkDestroyPipelineCache) \
    X(vkGetPipelineCacheData) X(vkCreateGraphicsPipelines) X(vkDestroyPipeline) \
    X(vkCreatePipelineLayout) X(vkDestroyPipelineLayout) X(vkCreateDescriptorSetLayout) \
    X(vkDestroyDescriptorSetLayout) X(vkCreateCommandPool) X(vkDestroyCommandPool) X(vkResetCommandPool) \
    X(vkAllocateCommandBuffers) X(vkBeginCommandBuffer) X(vkEndCommandBuffer) \
    X(vkCreateFence) X(vkDestroyFence) X(vkResetFences) X(vkWaitForFences) X(vkGetFenceStatus) \
    X(vkCreateSemaphore) X(vkDestroySemaphore) X(vkCreateQueryPool) X(vkDestroyQueryPool) \
    X(vkGetQueryPoolResults) X(vkCreateSwapchainKHR) X(vkDestroySwapchainKHR) X(vkGetSwapchainImagesKHR) \
    X(vkAcquireNextImageKHR) X(vkQueuePresentKHR) \
    X(vkCmdBeginRendering) X(vkCmdEndRendering) X(vkCmdPipelineBarrier2) X(vkCmdBindPipeline) \
    X(vkCmdBindVertexBuffers2) X(vkCmdBindIndexBuffer) X(vkCmdDraw) X(vkCmdDrawIndexed) \
    X(vkCmdPushConstants) X(vkCmdPushDescriptorSetKHR) X(vkCmdSetViewport) X(vkCmdSetScissor) \
    X(vkCmdSetDepthBias) X(vkCmdSetBlendConstants) X(vkCmdSetStencilCompareMask) \
    X(vkCmdSetStencilWriteMask) X(vkCmdSetStencilReference) X(vkCmdSetCullMode) X(vkCmdSetFrontFace) \
    X(vkCmdSetPrimitiveTopology) X(vkCmdSetDepthTestEnable) X(vkCmdSetDepthWriteEnable) \
    X(vkCmdSetDepthCompareOp) X(vkCmdSetDepthBoundsTestEnable) X(vkCmdSetStencilTestEnable) \
    X(vkCmdSetStencilOp) X(vkCmdSetDepthBiasEnable) X(vkCmdSetRasterizerDiscardEnable) \
    X(vkCmdSetPrimitiveRestartEnable) X(vkCmdClearAttachments) X(vkCmdClearColorImage) \
    X(vkCmdClearDepthStencilImage) X(vkCmdCopyBuffer) X(vkCmdCopyBufferToImage) \
    X(vkCmdCopyImageToBuffer) X(vkCmdCopyImage) X(vkCmdBlitImage) X(vkCmdResolveImage) \
    X(vkCmdBeginQuery) X(vkCmdEndQuery) X(vkCmdResetQueryPool) X(vkCmdCopyQueryPoolResults)
/* optional (extended dynamic state 3, vertex input dynamic state) */
#define VKD_DEVICE_OPT_FUNCS(X) \
    X(vkCmdSetPolygonModeEXT) X(vkCmdSetColorBlendEnableEXT) X(vkCmdSetColorBlendEquationEXT) \
    X(vkCmdSetColorWriteMaskEXT) X(vkCmdSetSampleMaskEXT) X(vkCmdSetAlphaToCoverageEnableEXT) \
    X(vkCmdSetVertexInputEXT) X(vkCmdSetRasterizationSamplesEXT)

#define VKD_DECL(f) extern PFN_##f f;
VKD_GLOBAL_FUNCS(VKD_DECL)
VKD_INSTANCE_FUNCS(VKD_DECL)
VKD_DEVICE_FUNCS(VKD_DECL)
VKD_DEVICE_OPT_FUNCS(VKD_DECL)
#undef VKD_DECL
extern PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;

/* ---- Win32 entry points (the game build has no import table) ---- */

#ifdef _WIN64
#define VKD_WNDLONG(X) X(user32, GetWindowLongPtrA) X(user32, SetWindowLongPtrA)
#define VKD_GETWNDPROC(h) ((WNDPROC)w32.GetWindowLongPtrA((h), GWLP_WNDPROC))
#define VKD_SETWNDPROC(h, p) ((WNDPROC)w32.SetWindowLongPtrA((h), GWLP_WNDPROC, (LONG_PTR)(p)))
#else
#define VKD_WNDLONG(X)
#define VKD_GETWNDPROC(h) ((WNDPROC)w32.GetWindowLongA((h), GWL_WNDPROC))
#define VKD_SETWNDPROC(h, p) ((WNDPROC)w32.SetWindowLongA((h), GWL_WNDPROC, (LONG)(p)))
#endif
#define VKD_W32_FUNCS(X) \
    X(user32, GetClientRect) X(user32, GetWindowRect) X(user32, GetWindowLongA) X(user32, SetWindowLongA) \
    X(user32, SetWindowPos) X(user32, CallWindowProcA) X(user32, ChangeDisplaySettingsExA) \
    X(user32, EnumDisplaySettingsA) X(user32, IsWindow) X(user32, IsIconic) X(user32, ShowWindow) \
    X(user32, MonitorFromWindow) X(user32, GetMonitorInfoA) X(user32, GetDC) X(user32, ReleaseDC) \
    X(user32, ClientToScreen) X(user32, GetSystemMetrics) X(user32, GetForegroundWindow) X(user32, MonitorFromPoint) \
    X(user32, PostMessageA) \
    X(gdi32, CreateCompatibleDC) X(gdi32, CreateDIBSection) X(gdi32, SelectObject) X(gdi32, DeleteObject) \
    X(gdi32, DeleteDC) X(gdi32, GdiFlush) X(gdi32, BitBlt) \
    X(kernel32, GetModuleHandleA) \
    VKD_WNDLONG(X)
struct vkd_w32 {
#define VKD_W32_DECL(dll, f) __typeof__(f) *f;
    VKD_W32_FUNCS(VKD_W32_DECL)
#undef VKD_W32_DECL
};
extern struct vkd_w32 w32;
int vkd_w32_init(void);

/* ---- Vulkan context (one per process) ---- */

struct vkd_block;
struct vkd_mem {
    VkDeviceMemory mem;
    VkDeviceSize offset, size;
    uint8_t *map;               /* host-visible: mapped address of this allocation */
    struct vkd_block *block;    /* 0: dedicated */
    VkBuffer buffer;            /* buffer allocations: the block's buffer */
};

struct vkd_gpu {
    VkInstance instance;
    VkPhysicalDevice phys;
    VkDevice dev;
    VkQueue queue;
    uint32_t queue_family;
    VkPhysicalDeviceProperties props;
    VkPhysicalDeviceMemoryProperties memprops;
    int eds3_blend, eds3_poly, eds3_mask, vi_dynamic, custom_border, mirror_clamp, divisor, d24s8, d16s8;
    int format_4444, fill_non_solid, clip_distance, wide_lines, anisotropy, depth_bounds;
    float max_aniso;
    int occ_precise;
    VkFormat ds24;              /* D24S8 or D32S8 fallback */
    struct vkd_block *blocks;
    void *lock;
    VkDebugUtilsMessengerEXT debug_messenger;
    int validation;             /* the messenger ran (kept after vkd_gpu_release) */
    uint32_t validation_errors, validation_warnings;
};
extern struct vkd_gpu vkd;

int vkd_gpu_init(void);         /* idempotent; 0 on failure */
void vkd_gpu_addref(void);      /* an IDirect3D9 holds the instance and device */
void vkd_gpu_release(void);     /* the last one destroys them */
uint32_t vkd_memory_type(uint32_t bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags avoid);
enum { VKD_MEM_DEVICE, VKD_MEM_UPLOAD, VKD_MEM_READBACK };
int vkd_alloc_image_memory(VkImage img, int kind, struct vkd_mem *out);
int vkd_alloc_buffer(VkDeviceSize size, int kind, struct vkd_mem *out);
void vkd_free_mem(struct vkd_mem *m);

/* ---- formats ---- */

struct vkd_format {
    D3DFORMAT d3d;
    VkFormat vk, vk_srgb;
    VkComponentMapping swz;
    uint8_t bytes;              /* per pixel, or per 4x4 block */
    uint8_t block;              /* 1, or 4 for BC */
    uint8_t depth, stencil;     /* bits */
    uint8_t sample_depth;       /* INTZ / DF16 / DF24: sampled as depth values, no comparison */
    uint8_t no_alpha;           /* X8R8G8B8 etc.: destination alpha reads one */
};
const struct vkd_format *vkd_format(D3DFORMAT f);
VkFormat vkd_attach_format(const struct vkd_format *f);
int vkd_format_supported(const struct vkd_format *f, VkFormatFeatureFlags need, int optimal);
/* subresource layout of the CPU copy */
void vkd_layout(const struct vkd_format *f, UINT w, UINT h, UINT d, UINT *pitch, UINT *slice, UINT *rows);

/* ---- small hash map (byte keys) ---- */

struct vkd_map {
    struct vkd_map_entry { uint32_t hash, size; void *key, *val; } *e;
    uint32_t cap, count;
};
uint32_t vkd_hash(const void *p, uint32_t n);
void *vkd_map_get(struct vkd_map *m, const void *key, uint32_t n);
void vkd_map_put(struct vkd_map *m, const void *key, uint32_t n, void *val);
void vkd_map_clear(struct vkd_map *m, void (*drop)(void *val, void *ctx), void *ctx);

/* ---- submission ---- */

#define VKD_CONTEXTS 8
struct vkd_garbage {
    int kind;
    union { VkBuffer buf; VkImage img; VkImageView view; VkSampler smp; VkShaderModule mod; VkPipeline pipe;
            VkSwapchainKHR sc; VkSemaphore sem; VkQueryPool qp; VkSurfaceKHR surf; } h;
    struct vkd_mem mem;
};
enum { G_MEM, G_BUFFER, G_IMAGE, G_VIEW, G_SAMPLER, G_MODULE, G_PIPELINE, G_SWAPCHAIN, G_SEMAPHORE, G_QUERYPOOL, G_SURFACE };
struct vkd_chunk { struct vkd_mem mem; VkDeviceSize used; struct vkd_chunk *next; };
struct vkd_context {
    VkCommandPool pool;
    VkCommandBuffer cmd, init;
    VkFence fence;
    uint64_t seq;
    int submitted, init_used;
    struct vkd_garbage *garbage;
    uint32_t ngarbage, capgarbage;
    struct vkd_chunk *chunks;   /* upload ring memory used by this submission */
    /* occlusion queries recorded in this submission, summed at retire */
    VkQueryPool *qpools;        /* VKD_QUERY_POOL slots each */
    uint32_t nqpools, qused;
    struct vkd_qref { struct vkd_query *q; uint32_t slot, gen; } *qrefs;
    uint32_t nqrefs, capqrefs;
};
#define VKD_QUERY_POOL 256

/* ---- objects ---- */

struct vkd_device;
struct vkd_priv { GUID guid; DWORD flags; DWORD size; void *data; struct vkd_priv *next; };

struct vkd_image {
    VkImage img;
    struct vkd_mem mem;
    const struct vkd_format *fmt;
    VkFormat vkfmt;
    VkImageAspectFlags aspect;
    VkImageViewType view_type;
    VkImageView view, view_srgb;    /* sampling, whole chain */
    uint32_t w, h, d, levels, layers;
    VkSampleCountFlagBits samples;
    uint64_t use_seq;
    int attachable;
};

/* common head of every resource object */
struct vkd_res {
    const void *vtbl;
    LONG ref;
    LONG bind;                  /* references held by device state */
    struct vkd_device *dev;
    D3DRESOURCETYPE type;
    D3DPOOL pool;
    DWORD usage;
    D3DFORMAT format;
    DWORD priority;
    struct vkd_priv *priv;
    void (*destroy)(struct vkd_res *r);
    int implicit;               /* back buffer, auto depth stencil: owned by the device */
    struct vkd_res *next, *prev; /* canonical live resources for Reset validation */
};

struct vkd_texture;
struct vkd_swapchain;
struct vkd_surface {
    struct vkd_res r;
    struct vkd_texture *tex;    /* container texture (refcount forwarded) or 0 */
    struct vkd_swapchain *swapchain;    /* back buffer of */
    struct vkd_image *img;      /* own image when standalone */
    uint32_t sub;               /* subresource index in tex */
    UINT level, layer, w, h, d;
    D3DMULTISAMPLE_TYPE ms;
    DWORD msq;
    int lockable, discard;
    VkImageView rtv, rtv_srgb;  /* attachment views */
    uint8_t *shadow;            /* standalone surfaces with a CPU copy */
    int locked;
    DWORD lock_flags;
    UINT lx0, ly0, lz0, lx1, ly1, lz1;  /* locked box */
    HDC dc; HBITMAP bmp; void *bits; HGDIOBJ oldbmp;
};

struct vkd_texture {
    struct vkd_res r;
    struct vkd_image *img;      /* 0: system memory or scratch */
    const struct vkd_format *fmt;
    UINT w, h, d, levels, faces;
    uint8_t *shadow;
    uint32_t *sub_off;          /* per subresource (face * levels + level) */
    uint32_t nsub;
    struct vkd_surface **surf;  /* per subresource; volumes for volume textures */
    DWORD lod;
    VkImageView view_lod, view_lod_srgb;    /* SetLOD > 0 */
    int autogen;                /* D3DUSAGE_AUTOGENMIPMAP: one visible level */
    D3DTEXTUREFILTERTYPE autogen_filter;
    int autogen_dirty;
    int dirty;                  /* for UpdateTexture */
};

struct vkd_buf {
    VkBuffer buf;
    VkDeviceSize off;
    uint8_t *map;
    struct vkd_mem mem;
    uint64_t use_seq;
};
struct vkd_buffer {
    struct vkd_res r;
    UINT size;
    DWORD fvf;
    int host;                   /* host-visible, renamed on discard */
    uint8_t *shadow;            /* device-local buffers */
    struct vkd_buf cur;
    struct vkd_buf *spare;      /* renamed copies, reused once idle */
    int nspare, capspare;
    int locks;
    uint32_t dirty_lo, dirty_hi;
};

struct vkd_vdecl {
    const void *vtbl;
    LONG ref;
    struct vkd_device *dev;
    LONG bind;
    int cached;                 /* FVF declaration retained by the device cache */
    D3DVERTEXELEMENT9 el[MAXD3DDECLLENGTH + 1];
    UINT n;
    DWORD fvf;                  /* converted from (0 = created from elements) */
    int has_positiont, has_normal, has_color[2], has_psize, blend, tex_dim[8];
};

struct vkd_variant { struct dxso_key key; VkShaderModule mod; uint32_t id; };
struct vkd_shader {
    const void *vtbl;
    LONG ref;
    LONG bind;
    struct vkd_device *dev;
    uint32_t *code;
    struct dxso_info info;
    struct vkd_variant *var;
    uint32_t nvar;
};

struct vkd_query {
    const void *vtbl;
    LONG ref;
    struct vkd_device *dev;
    D3DQUERYTYPE type;
    int state;                  /* 0 idle, 1 building, 2 issued */
    uint64_t seq;               /* submission that completes it */
    uint32_t gen;               /* Issue(BEGIN) count: older slots are ignored */
    uint32_t pending;           /* slots not yet retired (the object lives until 0) */
    uint64_t result;
    int dead;
};

struct vkd_swapchain {
    const void *vtbl;
    LONG ref;
    struct vkd_device *dev;
    D3DPRESENT_PARAMETERS pp;
    HWND hwnd;
    struct vkd_surface **back;
    UINT nback;
    struct vkd_image *resolve;
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    VkFormat sc_format;
    VkExtent2D extent;
    VkImage *images;
    VkImageView *views;
    VkSemaphore *done;
    uint32_t nimages;
    VkSemaphore acquire[VKD_CONTEXTS];
    uint32_t acquire_index;
    VkPresentModeKHR mode;
    int implicit;
    struct vkd_image *gamma;
    int gamma_dirty;
    D3DGAMMARAMP ramp;
    int gamma_on;               /* ramp is not the identity */
    uint64_t present_count;
    uint64_t frame_seq[16];     /* submissions of recent presents (frame latency) */
    int recreate;
};

/* ---- state ---- */

#define VKD_SAMPLERS 20         /* 0..15 pixel, 16..19 vertex */
#define VKD_MAX_RS 256
struct vkd_light { D3DLIGHT9 l; int set, on; };
struct vkd_state {
    DWORD rs[VKD_MAX_RS];
    DWORD ss[VKD_SAMPLERS][14];
    DWORD tss[8][33];
    struct vkd_res *tex[VKD_SAMPLERS];
    struct vkd_buffer *vb[16];
    UINT vb_off[16], vb_stride[16], vb_freq[16];
    struct vkd_buffer *ib;
    struct vkd_vdecl *decl;
    DWORD fvf;
    struct vkd_shader *vs, *ps;
    float vsf[256][4];
    int vsi[16][4];
    BOOL vsb[16];
    float psf[224][4];
    int psi[16][4];
    BOOL psb[16];
    D3DMATRIX view, proj, texm[8], world[256];
    struct vkd_light *lights;
    uint32_t nlights;
    D3DMATERIAL9 material;
    D3DVIEWPORT9 viewport;
    RECT scissor;
    float clip[6][4];
    float npatch;
};

/* capture masks of a state block */
struct vkd_mask {
    uint32_t rs[VKD_MAX_RS / 32];
    uint32_t ss[VKD_SAMPLERS];  /* bit per sampler state */
    uint64_t tss[8];            /* bit per stage state, including D3DTSS_CONSTANT (32) */
    uint32_t tex;               /* bit per sampler */
    uint32_t vb, vb_freq;       /* bit per stream */
    uint32_t vsf[8], psf[7];    /* bit per float constant */
    uint32_t vsi, vsb, psi, psb;
    uint32_t world[8];
    uint32_t misc;              /* VKD_SB_* */
    uint32_t texm;
    uint8_t *light;             /* per light index: captured */
    uint32_t nlight;
};
enum {
    VKD_SB_IB = 1, VKD_SB_DECL = 2, VKD_SB_VS = 4, VKD_SB_PS = 8, VKD_SB_VIEW = 16, VKD_SB_PROJ = 32,
    VKD_SB_MATERIAL = 64, VKD_SB_VIEWPORT = 128, VKD_SB_SCISSOR = 256, VKD_SB_CLIP = 512, VKD_SB_NPATCH = 1024,
    VKD_SB_LIGHTS_ALL = 2048,
};
struct vkd_stateblock {
    const void *vtbl;
    LONG ref;
    struct vkd_device *dev;
    struct vkd_state s;
    struct vkd_mask m;
};

/* ---- device ---- */

enum {
    DIRTY_PIPELINE = 1 << 0, DIRTY_VIEWPORT = 1 << 1, DIRTY_DEPTH = 1 << 2, DIRTY_STENCIL = 1 << 3,
    DIRTY_BLEND = 1 << 4, DIRTY_RASTER = 1 << 5, DIRTY_VB = 1 << 6, DIRTY_IB = 1 << 7,
    DIRTY_VS_CONST = 1 << 8, DIRTY_PS_CONST = 1 << 9, DIRTY_FF_VS = 1 << 10, DIRTY_FF_PS = 1 << 11,
    DIRTY_SAMPLERS = 1 << 12, DIRTY_PUSH = 1 << 13, DIRTY_INPUT = 1 << 14, DIRTY_SHADERS = 1 << 15,
    DIRTY_FF_VS_CONST = 1 << 16, DIRTY_FF_PS_CONST = 1 << 17, DIRTY_ALL = 0x7fffffff,
};

struct vkd_d3d9 {
    const void *vtbl;
    LONG ref;
    int ex;
};

struct vkd_pipeline {
    VkPipeline pipe;
    uint32_t vs_id, ps_id;
};

struct vkd_device {
    const void *vtbl;
    LONG ref;
    struct vkd_d3d9 *d3d;
    D3DDEVICE_CREATION_PARAMETERS cp;
    D3DCAPS9 caps;
    void *lock;                 /* D3DCREATE_MULTITHREADED */

    struct vkd_context ctx[VKD_CONTEXTS];
    uint32_t cur;               /* context being recorded */
    uint64_t seq, completed;    /* seq of the open submission; last completed */
    struct vkd_chunk *free_chunks;

    VkDescriptorSetLayout set_layout;
    VkPipelineLayout layout;
    VkPipelineCache pcache;
    struct vkd_map pipelines, samplers, ff_vs, ff_ps, fvf_decls;
    VkShaderModule present_vs, present_ps, present_ps_gamma;
    struct vkd_map present_pipes;
    VkSampler present_sampler, lut_sampler;
    struct vkd_image *dummy[3];     /* 2D, cube, 3D: (0, 0, 0, 1) */
    struct vkd_image *white[3];     /* fixed-function: unbound stage */
    struct vkd_buf zero_vb;

    struct vkd_swapchain *swapchain;
    struct vkd_surface *auto_ds;
    struct vkd_swapchain **extra_sc;
    uint32_t nextra_sc;

    struct vkd_state st;
    struct vkd_stateblock *recording;
    uint32_t active_lights[8], nactive;
    struct vkd_surface *rt[4], *ds;
    struct vkd_res *resources;
    uint32_t dirty;
    int in_scene;
    int lost, not_reset;
    float gamma_dummy;

    /* recording */
    int rendering;
    VkExtent2D rt_extent;
    VkPipeline bound_pipe;
    int draws_in_pass;
    struct vkd_query **active_queries;
    uint32_t nactive_queries, capactive_queries;
    int occ_open;               /* a Vulkan occlusion query is running */
    VkQueryPool occ_pool;
    uint32_t occ_slot;
    uint32_t next_variant_id;

    /* derived per draw; command buffer state is tracked for the submission d->seq
     * and recorded again only when its inputs change */
    struct dxso_vs_consts ffvs;
    struct dxso_ps_consts ffps;
    struct vkd_shader *ff_vs_cur, *ff_ps_cur;   /* fixed-function shaders of the current state */
    struct vkd_ubo { VkBuffer buf; VkDeviceSize off; uint64_t seq; uint32_t floats; int tail, src; } vs_ubo, ps_ubo;
    VkDescriptorImageInfo images[VKD_SAMPLERS];
    VkDescriptorBufferInfo desc_buffers[2];
    uint64_t images_seq, desc_seq, dyn_seq, push_seq, vi_seq, vb_seq;
    D3DPRIMITIVETYPE dyn_topology;
    struct dxso_push push;
    VkVertexInputBindingDescription2EXT vi_bind[17];
    VkVertexInputAttributeDescription2EXT vi_attr[16];
    UINT vi_nbind, vi_nattr;
    VkBuffer vb_bound[17];
    VkDeviceSize vb_bound_off[17];

    int fpu_preserve;
    WNDPROC old_wndproc;
    HWND hooked;
    DWORD win_style, win_exstyle;
    RECT win_rect;
    int fullscreen_mode;
    char fullscreen_device[32];
    int option_interval, option_latency, option_aniso;

    int rendering_srgb;         /* the open pass uses sRGB attachment views */
    int cursor_shown, sw_vp;
    D3DCLIPSTATUS9 clip_status;
    PALETTEENTRY (*palette)[256];
    UINT npalette, cur_palette;
};

/* ---- shared functions ---- */

#define VKD_LOCK(d) do { if ((d)->lock) vkp_lock((d)->lock); } while (0)
#define VKD_UNLOCK(d) do { if ((d)->lock) vkp_unlock((d)->lock); } while (0)

/* d3d9_main.c */
IDirect3D9 *WINAPI vk_Direct3DCreate9(UINT sdk);
void vkd_fill_caps(D3DCAPS9 *caps);
HRESULT vkd_check_format(D3DFORMAT adapter, DWORD usage, D3DRESOURCETYPE type, D3DFORMAT f);
void vkd_desktop_mode(D3DDISPLAYMODE *m);

/* vk_core.c: submission and memory */
int vkd_device_init_vk(struct vkd_device *d);
void vkd_device_free_vk(struct vkd_device *d);
VkCommandBuffer vkd_cmd(struct vkd_device *d);         /* main command buffer of the open submission */
VkCommandBuffer vkd_init_cmd(struct vkd_device *d);    /* runs before it (uploads to idle resources) */
void vkd_submit(struct vkd_device *d, VkSemaphore wait, VkSemaphore signal);
void vkd_wait_seq(struct vkd_device *d, uint64_t seq);
void vkd_poll(struct vkd_device *d);
void vkd_flush_wait(struct vkd_device *d);              /* submit and wait for everything */
void vkd_defer(struct vkd_device *d, const struct vkd_garbage *g);
/* upload ring: host-visible scratch valid for the open submission */
int vkd_ring(struct vkd_device *d, VkDeviceSize size, VkDeviceSize align, struct vkd_buf *out);
/* size bytes, with span bytes of the same buffer addressable from the offset (a constant
 * block whose unread tail later allocations reuse) */
int vkd_ring_span(struct vkd_device *d, VkDeviceSize size, VkDeviceSize span, VkDeviceSize align,
                  struct vkd_buf *out);
void vkd_barrier(VkCommandBuffer cmd, VkPipelineStageFlags2 src, VkAccessFlags2 srca, VkPipelineStageFlags2 dst,
                 VkAccessFlags2 dsta);
void vkd_barrier_all(VkCommandBuffer cmd);
/* command buffer for writing an image/buffer: init buffer when the resource is idle in this submission */
VkCommandBuffer vkd_upload_cmd(struct vkd_device *d, uint64_t *use_seq);

struct vkd_image *vkd_image_create(struct vkd_device *d, const struct vkd_format *f, VkImageType type, UINT w,
                                   UINT h, UINT depth, UINT levels, UINT layers, VkSampleCountFlagBits samples,
                                   int cube, int attach);
void vkd_image_destroy(struct vkd_device *d, struct vkd_image *img);
VkImageView vkd_image_view(struct vkd_image *img, VkFormat fmt, VkImageViewType type, uint32_t level,
                           uint32_t nlevels, uint32_t layer, uint32_t nlayers, int swizzle);
int vkd_buf_alloc(struct vkd_device *d, VkDeviceSize size, int host, struct vkd_buf *out);
void vkd_buf_free(struct vkd_device *d, struct vkd_buf *b);
VkShaderModule vkd_module(const uint32_t *code, uint32_t words);

/* d3d9_resource.c */
HRESULT vkd_create_surface(struct vkd_device *d, UINT w, UINT h, D3DFORMAT f, D3DMULTISAMPLE_TYPE ms, DWORD msq,
                           DWORD usage, D3DPOOL pool, int lockable, int discard, struct vkd_surface **out);
HRESULT vkd_create_texture(struct vkd_device *d, D3DRESOURCETYPE type, UINT w, UINT h, UINT depth, UINT levels,
                           DWORD usage, D3DFORMAT f, D3DPOOL pool, struct vkd_texture **out);
HRESULT vkd_create_buffer(struct vkd_device *d, UINT size, DWORD usage, DWORD fvf, D3DFORMAT f, D3DPOOL pool,
                          int index, struct vkd_buffer **out);
struct vkd_image *vkd_surface_image(struct vkd_surface *s);
VkImageView vkd_surface_rtv(struct vkd_surface *s, int srgb);
struct vkd_texture *vkd_res_texture(struct vkd_res *r);
void vkd_res_bind(struct vkd_res *r);
int vkd_reset_resources_busy(struct vkd_device *d);
void vkd_res_unbind(struct vkd_res *r);
void vkd_res_addref(struct vkd_res *r);
ULONG vkd_res_release(struct vkd_res *r);
void vkd_texture_upload(struct vkd_texture *t, uint32_t sub, const RECT *rect, UINT z0, UINT z1);
void vkd_texture_genmips(struct vkd_texture *t);
void vkd_surface_readback(struct vkd_surface *s, uint8_t *dst, UINT pitch);
uint8_t *vkd_surface_shadow(struct vkd_surface *s, UINT *pitch);
void vkd_buffer_used(struct vkd_buffer *b);
void vkd_buffer_flush(struct vkd_buffer *b);
uint8_t *vkd_surface_cpu(struct vkd_surface *s, UINT *pitch, UINT *slice);
void vkd_surface_upload(struct vkd_surface *s, UINT x0, UINT y0, UINT z0, UINT x1, UINT y1, UINT z1);
void vkd_surface_free(struct vkd_surface *s);
void vkd_image_write(struct vkd_device *d, struct vkd_image *img, UINT level, UINT layer, UINT x, UINT y, UINT z,
                     UINT w, UINT h, UINT depth, const uint8_t *src, UINT pitch, UINT slice);
void vkd_image_read(struct vkd_device *d, struct vkd_image *img, UINT level, UINT layer, uint8_t *dst, UINT pitch);
VkImageView vkd_texture_view(struct vkd_texture *t, int srgb);
VkSampleCountFlagBits vkd_samples(D3DMULTISAMPLE_TYPE ms, DWORD quality);
HRESULT vkd_priv_set(struct vkd_priv **list, REFGUID g, const void *data, DWORD size, DWORD flags);
HRESULT vkd_priv_get(struct vkd_priv **list, REFGUID g, void *data, DWORD *size);
HRESULT vkd_priv_free(struct vkd_priv **list, REFGUID g);
extern const IDirect3DSurface9Vtbl vkd_surface_vtbl;
extern const IDirect3DTexture9Vtbl vkd_texture_vtbl;
extern const IDirect3DCubeTexture9Vtbl vkd_cube_vtbl;
extern const IDirect3DVolumeTexture9Vtbl vkd_voltex_vtbl;
extern const IDirect3DVolume9Vtbl vkd_volume_vtbl;
extern const IDirect3DVertexBuffer9Vtbl vkd_vb_vtbl;
extern const IDirect3DIndexBuffer9Vtbl vkd_ib_vtbl;

/* d3d9_shader.c */
HRESULT vkd_create_vdecl(struct vkd_device *d, const D3DVERTEXELEMENT9 *el, struct vkd_vdecl **out);
struct vkd_vdecl *vkd_fvf_decl(struct vkd_device *d, DWORD fvf);
HRESULT vkd_create_shader(struct vkd_device *d, const DWORD *code, int ps, struct vkd_shader **out);
VkShaderModule vkd_shader_module(struct vkd_shader *s, const struct dxso_key *key);
void vkd_shader_addref_bind(struct vkd_shader *s);
void vkd_shader_unbind(struct vkd_shader *s);
HRESULT vkd_create_query(struct vkd_device *d, D3DQUERYTYPE type, struct vkd_query **out);
void vkd_queries_pass_begin(struct vkd_device *d);
void vkd_queries_pass_end(struct vkd_device *d);
void vkd_queries_retire(struct vkd_device *d, struct vkd_context *c);
void vkd_queries_free(struct vkd_device *d, struct vkd_context *c);
void vkd_ref_tex(struct vkd_res **slot, struct vkd_res *r);
void vkd_ref_buf(struct vkd_buffer **slot, struct vkd_buffer *b);
void vkd_ref_decl(struct vkd_vdecl **slot, struct vkd_vdecl *v);
void vkd_ref_shader(struct vkd_shader **slot, struct vkd_shader *s);
void vkd_vdecl_free(struct vkd_vdecl *v);
void vkd_shader_free(struct vkd_shader *s);
void vkd_stateblock_capture(struct vkd_stateblock *sb);
void vkd_swapchain_gamma(struct vkd_swapchain *sc, const D3DGAMMARAMP *ramp);
HRESULT vkd_swapchain_reset(struct vkd_swapchain *sc, D3DPRESENT_PARAMETERS *pp);
HRESULT vkd_create_stateblock(struct vkd_device *d, D3DSTATEBLOCKTYPE type, struct vkd_stateblock **out);
void vkd_stateblock_begin(struct vkd_device *d);
void vkd_state_defaults(struct vkd_device *d, struct vkd_state *s);
void vkd_state_release(struct vkd_state *s);
void vkd_mask_free(struct vkd_mask *m);
HRESULT vkd_swapchain_create(struct vkd_device *d, D3DPRESENT_PARAMETERS *pp, int implicit, struct vkd_swapchain **out);
void vkd_swapchain_destroy(struct vkd_swapchain *sc);
HRESULT vkd_swapchain_present(struct vkd_swapchain *sc, const RECT *src, const RECT *dst, HWND override);
extern const IDirect3DVertexDeclaration9Vtbl vkd_vdecl_vtbl;
extern const IDirect3DVertexShader9Vtbl vkd_vs_vtbl;
extern const IDirect3DPixelShader9Vtbl vkd_ps_vtbl;
extern const IDirect3DQuery9Vtbl vkd_query_vtbl;
extern const IDirect3DStateBlock9Vtbl vkd_sb_vtbl;
extern const IDirect3DSwapChain9Vtbl vkd_sc_vtbl;

/* d3d9_device.c */
int vkd_device_layout_init(struct vkd_device *d);
void vkd_device_layout_free(struct vkd_device *d);
void vkd_present_free(struct vkd_device *d);
HRESULT vkd_device_create(struct vkd_d3d9 *d3d, UINT adapter, D3DDEVTYPE type, HWND focus, DWORD flags,
                          D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out);
void vkd_end_rendering(struct vkd_device *d);
void vkd_set_render_state(struct vkd_device *d, struct vkd_state *s, D3DRENDERSTATETYPE rs, DWORD v);
void vkd_apply_state_all(struct vkd_device *d);
void vkd_pipelines_forget(struct vkd_device *d, uint32_t variant_id);
struct vkd_light *vkd_light_slot(struct vkd_state *s, DWORD index);
extern const IDirect3DDevice9Vtbl vkd_device_vtbl;

/* platform.c (game build) */
void vkp_init(void *load_library, void *get_proc_address);

static inline float vkd_sqrtf(float x) { float r; __asm__("sqrtss %1, %0" : "=x"(r) : "x"(x)); return r; }
float vkd_cosf(float x);
float vkd_powf(float x, float y);
static inline float vkd_fabsf(float x) { return x < 0 ? -x : x; }
static inline float vkd_f(DWORD v) { union { DWORD u; float f; } x; x.u = v; return x.f; }
static inline DWORD vkd_u(float v) { union { DWORD u; float f; } x; x.f = v; return x.u; }
static inline int vkd_guid_eq(REFGUID a, const GUID *b) { return memcmp(a, b, sizeof(GUID)) == 0; }

#endif
