/* Vulkan core: entry points, instance and device, memory, formats, command
 * submission, the upload ring, images and buffers. */
#include "vkd3d9.h"

#define VKD_DEF(f) PFN_##f f;
VKD_GLOBAL_FUNCS(VKD_DEF)
VKD_INSTANCE_FUNCS(VKD_DEF)
VKD_DEVICE_FUNCS(VKD_DEF)
VKD_DEVICE_OPT_FUNCS(VKD_DEF)
#undef VKD_DEF
PFN_vkGetInstanceProcAddr vkGetInstanceProcAddr;
struct vkd_gpu vkd;
struct vkd_w32 w32;

/* ---- Win32 ---- */

int vkd_w32_init(void)
{
    static int done;
    if (done) return done > 0;
    int ok = 1;
#define VKD_W32_LOAD(dll, f) \
    if (!(w32.f = (__typeof__(f) *)vkp_proc(#dll ".dll", #f))) { vkp_log("vulkan: %s!%s missing", #dll, #f); ok = 0; }
    VKD_W32_FUNCS(VKD_W32_LOAD)
#undef VKD_W32_LOAD
    done = ok ? 1 : -1;
    return ok;
}

/* ---- math ---- */

float vkd_cosf(float x)
{
    /* reduce to [-pi, pi], then a minimax polynomial in x^2 */
    const float pi = 3.14159265358979f, two_pi = 6.28318530717959f;
    float k = (float)(int)(x / two_pi);
    x -= k * two_pi;
    if (x > pi) x -= two_pi;
    if (x < -pi) x += two_pi;
    float sign = 1;
    if (x < 0) x = -x;
    if (x > pi / 2) { x = pi - x; sign = -1; }
    float x2 = x * x;
    float r = 1 + x2 * (-0.5f + x2 * (1.0f / 24 + x2 * (-1.0f / 720 + x2 * (1.0f / 40320 - x2 / 3628800))));
    return sign * r;
}

static float log2f_(float x)
{
    union { float f; uint32_t u; } v = {x};
    int e = (int)((v.u >> 23) & 0xff) - 127;
    v.u = (v.u & 0x007fffff) | 0x3f800000;  /* mantissa in [1, 2) */
    float m = v.f, t = (m - 1) / (m + 1), t2 = t * t;
    float ln = 2 * t * (1 + t2 * (1.0f / 3 + t2 * (1.0f / 5 + t2 * (1.0f / 7 + t2 / 9))));
    return (float)e + ln * 1.44269504f;
}
static float exp2f_(float x)
{
    if (x < -126) return 0;
    if (x > 127) x = 127;
    int i = (int)x;
    if ((float)i > x) i--;
    float f = (x - (float)i) * 0.69314718f;
    float r = 1 + f * (1 + f * (0.5f + f * (1.0f / 6 + f * (1.0f / 24 + f * (1.0f / 120 + f / 720)))));
    union { float f; uint32_t u; } v;
    v.u = (uint32_t)(i + 127) << 23;
    return r * v.f;
}
float vkd_powf(float x, float y)
{
    if (x <= 0) return y == 0 ? 1.0f : 0.0f;
    return exp2f_(y * log2f_(x));
}

/* ---- instance and device ---- */

static int has_ext(const VkExtensionProperties *e, uint32_t n, const char *name)
{
    for (uint32_t i = 0; i < n; i++) if (vkp_streq(e[i].extensionName, name)) return 1;
    return 0;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL validation_message(VkDebugUtilsMessageSeverityFlagBitsEXT severity,
    VkDebugUtilsMessageTypeFlagsEXT type, const VkDebugUtilsMessengerCallbackDataEXT *data, void *user)
{
    (void)type; (void)user;
    /* The loader warns about every layer VK_LOADER_LAYERS_DISABLE filters out:
     * that describes the machine, not the application, so it is only logged. */
    int loader = data->pMessageIdName && vkp_streq(data->pMessageIdName, "Loader Message");
    if (severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) vkd.validation_errors++;
    if ((severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) && !loader) vkd.validation_warnings++;
    vkp_log("vulkan validation: %s", data->pMessage);
    return VK_FALSE;
}

static int gpu_state;           /* 0 untried, 1 ok, -1 failed */
static LONG gpu_users;          /* live IDirect3D9 objects */

int vkd_gpu_init(void)
{
    if (gpu_state) return gpu_state > 0;
    gpu_state = -1;
    if (!vkd_w32_init()) return 0;
    vkGetInstanceProcAddr = (PFN_vkGetInstanceProcAddr)vkp_proc("vulkan-1.dll", "vkGetInstanceProcAddr");
    if (!vkGetInstanceProcAddr) { vkp_log("vulkan: vulkan-1.dll not found"); return 0; }
#define VKD_LOAD_G(f) f = (PFN_##f)vkGetInstanceProcAddr(0, #f);
    VKD_GLOBAL_FUNCS(VKD_LOAD_G)
#undef VKD_LOAD_G
    uint32_t api = VK_API_VERSION_1_0;
    if (vkEnumerateInstanceVersion) vkEnumerateInstanceVersion(&api);
    if (api < VK_API_VERSION_1_3) { vkp_log("vulkan: loader supports only %u.%u", VK_API_VERSION_MAJOR(api), VK_API_VERSION_MINOR(api)); return 0; }

    VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "BfVietnam";
    app.pEngineName = "bfv-vkd3d9";
    app.apiVersion = VK_API_VERSION_1_3;
    const char *iext[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_WIN32_SURFACE_EXTENSION_NAME,
                          VK_EXT_DEBUG_UTILS_EXTENSION_NAME};
    const char *layer = "VK_LAYER_KHRONOS_validation";
    int validation = vkp_option_int("vulkan.validation", 0);
    VkDebugUtilsMessengerCreateInfoEXT debug = {VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT};
    debug.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debug.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                        VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    debug.pfnUserCallback = validation_message;
    VkInstanceCreateInfo ici = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    ici.pApplicationInfo = &app;
    ici.enabledExtensionCount = validation ? 3 : 2;
    ici.ppEnabledExtensionNames = iext;
    if (validation) {
        ici.enabledLayerCount = 1;
        ici.ppEnabledLayerNames = &layer;
        ici.pNext = &debug;
    }
    VkResult r = vkCreateInstance(&ici, 0, &vkd.instance);
    if (r != VK_SUCCESS) { vkp_log("vulkan: vkCreateInstance failed (%d)", r); return 0; }
#define VKD_LOAD_I(f) f = (PFN_##f)vkGetInstanceProcAddr(vkd.instance, #f);
    VKD_INSTANCE_FUNCS(VKD_LOAD_I)
#undef VKD_LOAD_I
    if (validation) {
        PFN_vkCreateDebugUtilsMessengerEXT create_debug =
            (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkd.instance, "vkCreateDebugUtilsMessengerEXT");
        if (!create_debug || create_debug(vkd.instance, &debug, 0, &vkd.debug_messenger) != VK_SUCCESS) {
            vkp_log("vulkan: validation messenger creation failed"); return 0;
        }
        vkd.validation = 1;
    }

    uint32_t n = 0;
    vkEnumeratePhysicalDevices(vkd.instance, &n, 0);
    VkPhysicalDevice pd[16];
    if (n > 16) n = 16;
    vkEnumeratePhysicalDevices(vkd.instance, &n, pd);
    int best = -1, best_score = -1;
    for (uint32_t i = 0; i < n; i++) {
        VkPhysicalDeviceProperties p;
        vkGetPhysicalDeviceProperties(pd[i], &p);
        if (p.apiVersion < VK_API_VERSION_1_3) continue;
        int score = p.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU ? 3
                  : p.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU ? 2 : 1;
        if (score > best_score) { best = (int)i; best_score = score; }
    }
    if (best < 0) { vkp_log("vulkan: no Vulkan 1.3 device"); return 0; }
    vkd.phys = pd[best];
    vkGetPhysicalDeviceProperties(vkd.phys, &vkd.props);
    vkGetPhysicalDeviceMemoryProperties(vkd.phys, &vkd.memprops);
    vkp_log("vulkan: %s, driver %x, api %u.%u.%u", vkd.props.deviceName, vkd.props.driverVersion,
            VK_API_VERSION_MAJOR(vkd.props.apiVersion), VK_API_VERSION_MINOR(vkd.props.apiVersion),
            VK_API_VERSION_PATCH(vkd.props.apiVersion));

    VkQueueFamilyProperties qf[16];
    uint32_t nq = 16;
    vkGetPhysicalDeviceQueueFamilyProperties(vkd.phys, &nq, qf);
    vkd.queue_family = ~0u;
    for (uint32_t i = 0; i < nq; i++)
        if (qf[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) { vkd.queue_family = i; break; }
    if (vkd.queue_family == ~0u) { vkp_log("vulkan: no graphics queue"); return 0; }

    uint32_t next = 0;
    vkEnumerateDeviceExtensionProperties(vkd.phys, 0, &next, 0);
    VkExtensionProperties *ext = vkp_alloc(next * sizeof *ext + 1);
    vkEnumerateDeviceExtensionProperties(vkd.phys, 0, &next, ext);
    if (!has_ext(ext, next, VK_KHR_SWAPCHAIN_EXTENSION_NAME) || !has_ext(ext, next, VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME)) {
        vkp_log("vulkan: device lacks VK_KHR_swapchain or VK_KHR_push_descriptor");
        vkp_free(ext);
        return 0;
    }
    /* vulkan.eds3 = 0 / vulkan.vertexInput = 0 force the pipeline-state fallbacks
     * of devices without these extensions (the GPU harness tests both paths). */
    int x_eds3 = has_ext(ext, next, VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME) && vkp_option_int("vulkan.eds3", 1);
    int x_vi = has_ext(ext, next, VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME) &&
               vkp_option_int("vulkan.vertexInput", 1);
    int x_border = has_ext(ext, next, VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME);
    int x_div = has_ext(ext, next, VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME);
    vkp_free(ext);

    VkPhysicalDeviceFeatures2 f2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan12Features f12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features f13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT fe3 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT};
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT fvi = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT};
    VkPhysicalDeviceCustomBorderColorFeaturesEXT fcb = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT};
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT fdv = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT};
    void **tail = &f2.pNext;
#define CHAIN(s) do { *tail = &(s); tail = &(s).pNext; } while (0)
    CHAIN(f12); CHAIN(f13);
    if (x_eds3) CHAIN(fe3);
    if (x_vi) CHAIN(fvi);
    if (x_border) CHAIN(fcb);
    if (x_div) CHAIN(fdv);
    vkGetPhysicalDeviceFeatures2(vkd.phys, &f2);
    if (!f13.dynamicRendering || !f13.synchronization2 || !f13.shaderDemoteToHelperInvocation ||
        !f2.features.shaderClipDistance) {
        vkp_log("vulkan: device lacks dynamic rendering, synchronization2, demote or clip distances");
        return 0;
    }

    /* enable what is used */
    VkPhysicalDeviceFeatures2 e2 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    VkPhysicalDeviceVulkan12Features e12 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
    VkPhysicalDeviceVulkan13Features e13 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};
    VkPhysicalDeviceExtendedDynamicState3FeaturesEXT ee3 = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_3_FEATURES_EXT};
    VkPhysicalDeviceVertexInputDynamicStateFeaturesEXT evi = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_INPUT_DYNAMIC_STATE_FEATURES_EXT};
    VkPhysicalDeviceCustomBorderColorFeaturesEXT ecb = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_CUSTOM_BORDER_COLOR_FEATURES_EXT};
    VkPhysicalDeviceVertexAttributeDivisorFeaturesEXT edv = {VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VERTEX_ATTRIBUTE_DIVISOR_FEATURES_EXT};
    const VkPhysicalDeviceFeatures *have = &f2.features;
    VkPhysicalDeviceFeatures *want = &e2.features;
    want->shaderClipDistance = 1;
    want->fillModeNonSolid = have->fillModeNonSolid;
    want->samplerAnisotropy = have->samplerAnisotropy;
    want->independentBlend = have->independentBlend;
    want->textureCompressionBC = have->textureCompressionBC;
    want->occlusionQueryPrecise = have->occlusionQueryPrecise;
    want->largePoints = have->largePoints;
    want->depthBounds = have->depthBounds;
    want->wideLines = have->wideLines;
    want->alphaToOne = have->alphaToOne;
    want->depthClamp = have->depthClamp;
    e12.samplerMirrorClampToEdge = f12.samplerMirrorClampToEdge;
    e13.dynamicRendering = 1;
    e13.synchronization2 = 1;
    e13.shaderDemoteToHelperInvocation = 1;
    e13.maintenance4 = f13.maintenance4;
    const char *dext[8];
    uint32_t ndext = 0;
    dext[ndext++] = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    dext[ndext++] = VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME;
    tail = &e2.pNext;
    CHAIN(e12); CHAIN(e13);
    if (x_eds3 && fe3.extendedDynamicState3PolygonMode && fe3.extendedDynamicState3ColorBlendEnable &&
        fe3.extendedDynamicState3ColorBlendEquation && fe3.extendedDynamicState3ColorWriteMask &&
        fe3.extendedDynamicState3SampleMask && fe3.extendedDynamicState3AlphaToCoverageEnable &&
        fe3.extendedDynamicState3RasterizationSamples) {
        ee3.extendedDynamicState3PolygonMode = 1;
        ee3.extendedDynamicState3ColorBlendEnable = 1;
        ee3.extendedDynamicState3ColorBlendEquation = 1;
        ee3.extendedDynamicState3ColorWriteMask = 1;
        ee3.extendedDynamicState3SampleMask = 1;
        ee3.extendedDynamicState3AlphaToCoverageEnable = 1;
        ee3.extendedDynamicState3RasterizationSamples = 1;
        CHAIN(ee3);
        dext[ndext++] = VK_EXT_EXTENDED_DYNAMIC_STATE_3_EXTENSION_NAME;
        vkd.eds3_blend = vkd.eds3_poly = vkd.eds3_mask = 1;
    }
    if (x_vi && fvi.vertexInputDynamicState) {
        evi.vertexInputDynamicState = 1;
        CHAIN(evi);
        dext[ndext++] = VK_EXT_VERTEX_INPUT_DYNAMIC_STATE_EXTENSION_NAME;
        vkd.vi_dynamic = 1;
    }
    if (x_border && fcb.customBorderColors && fcb.customBorderColorWithoutFormat) {
        ecb.customBorderColors = 1;
        ecb.customBorderColorWithoutFormat = 1;
        CHAIN(ecb);
        dext[ndext++] = VK_EXT_CUSTOM_BORDER_COLOR_EXTENSION_NAME;
        vkd.custom_border = 1;
    }
    if (x_div && fdv.vertexAttributeInstanceRateDivisor) {
        edv.vertexAttributeInstanceRateDivisor = 1;
        CHAIN(edv);
        dext[ndext++] = VK_EXT_VERTEX_ATTRIBUTE_DIVISOR_EXTENSION_NAME;
        vkd.divisor = 1;
    }
#undef CHAIN
    vkd.mirror_clamp = f12.samplerMirrorClampToEdge;
    vkd.fill_non_solid = have->fillModeNonSolid;
    vkd.anisotropy = have->samplerAnisotropy;
    vkd.max_aniso = vkd.props.limits.maxSamplerAnisotropy;
    vkd.occ_precise = have->occlusionQueryPrecise;
    vkd.depth_bounds = have->depthBounds;
    vkd.wide_lines = have->wideLines;
    vkd.clip_distance = 1;

    float prio = 1;
    VkDeviceQueueCreateInfo qci = {VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    qci.queueFamilyIndex = vkd.queue_family;
    qci.queueCount = 1;
    qci.pQueuePriorities = &prio;
    VkDeviceCreateInfo dci = {VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    dci.pNext = &e2;
    dci.queueCreateInfoCount = 1;
    dci.pQueueCreateInfos = &qci;
    dci.enabledExtensionCount = ndext;
    dci.ppEnabledExtensionNames = dext;
    r = vkCreateDevice(vkd.phys, &dci, 0, &vkd.dev);
    if (r != VK_SUCCESS) { vkp_log("vulkan: vkCreateDevice failed (%d)", r); return 0; }
#define VKD_LOAD_D(f) \
    if (!(f = (PFN_##f)vkGetDeviceProcAddr(vkd.dev, #f))) { vkp_log("vulkan: %s missing", #f); return 0; }
    VKD_DEVICE_FUNCS(VKD_LOAD_D)
#undef VKD_LOAD_D
#define VKD_LOAD_O(f) f = (PFN_##f)vkGetDeviceProcAddr(vkd.dev, #f);
    VKD_DEVICE_OPT_FUNCS(VKD_LOAD_O)
#undef VKD_LOAD_O
    if (!vkCmdSetVertexInputEXT) vkd.vi_dynamic = 0;
    if (!vkCmdSetColorBlendEnableEXT || !vkCmdSetPolygonModeEXT || !vkCmdSetRasterizationSamplesEXT)
        vkd.eds3_blend = vkd.eds3_poly = vkd.eds3_mask = 0;
    vkGetDeviceQueue(vkd.dev, vkd.queue_family, 0, &vkd.queue);

    /* depth formats */
    VkFormatProperties fp;
    vkGetPhysicalDeviceFormatProperties(vkd.phys, VK_FORMAT_D24_UNORM_S8_UINT, &fp);
    vkd.d24s8 = !!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    vkd.ds24 = vkd.d24s8 ? VK_FORMAT_D24_UNORM_S8_UINT : VK_FORMAT_D32_SFLOAT_S8_UINT;
    vkGetPhysicalDeviceFormatProperties(vkd.phys, VK_FORMAT_D16_UNORM_S8_UINT, &fp);
    vkd.d16s8 = !!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
    vkGetPhysicalDeviceFormatProperties(vkd.phys, VK_FORMAT_A4R4G4B4_UNORM_PACK16, &fp);
    vkd.format_4444 = !!(fp.optimalTilingFeatures & VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT);
    vkp_log("vulkan: eds3 %d, vertex input %d, border %d, divisor %d, d24s8 %d", vkd.eds3_blend, vkd.vi_dynamic,
            vkd.custom_border, vkd.divisor, vkd.d24s8);
    gpu_state = 1;
    return 1;
}

struct vkd_range { VkDeviceSize off, size; struct vkd_range *next; };
struct vkd_block {
    VkDeviceMemory mem;
    VkDeviceSize size;
    uint32_t type;
    int kind, image;
    uint8_t *map;
    VkBuffer buffer;
    struct vkd_range *free;
    struct vkd_block *next;
};

void vkd_gpu_addref(void) { InterlockedIncrement(&gpu_users); }

/* The last IDirect3D9 is gone, so no device is left (each holds one): destroy
 * the Vulkan device and instance. Layers then stop their threads here; left
 * for process exit, a validation layer built on winpthreads hangs in its
 * static destructors. The next Direct3DCreate9 initializes again. */
void vkd_gpu_release(void)
{
    if (InterlockedDecrement(&gpu_users) > 0 || gpu_state <= 0) return;
    vkDeviceWaitIdle(vkd.dev);
    while (vkd.blocks) {
        struct vkd_block *b = vkd.blocks;
        vkd.blocks = b->next;
        if (b->buffer) vkDestroyBuffer(vkd.dev, b->buffer, 0);
        vkFreeMemory(vkd.dev, b->mem, 0);
        while (b->free) { struct vkd_range *r = b->free; b->free = r->next; vkp_free(r); }
        vkp_free(b);
    }
    vkDestroyDevice(vkd.dev, 0);
    if (vkd.debug_messenger) {
        PFN_vkDestroyDebugUtilsMessengerEXT destroy_debug =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(vkd.instance, "vkDestroyDebugUtilsMessengerEXT");
        if (destroy_debug) destroy_debug(vkd.instance, vkd.debug_messenger, 0);
    }
    vkDestroyInstance(vkd.instance, 0);
    vkd.dev = 0; vkd.queue = 0; vkd.phys = 0; vkd.instance = 0; vkd.debug_messenger = 0;
    gpu_state = 0;
}

/* ---- memory ---- */

uint32_t vkd_memory_type(uint32_t bits, VkMemoryPropertyFlags want, VkMemoryPropertyFlags avoid)
{
    for (int pass = 0; pass < 2; pass++)
        for (uint32_t i = 0; i < vkd.memprops.memoryTypeCount; i++) {
            VkMemoryPropertyFlags f = vkd.memprops.memoryTypes[i].propertyFlags;
            if (!(bits >> i & 1) || (f & want) != want) continue;
            if (pass == 0 && (f & avoid)) continue;
            return i;
        }
    return ~0u;
}

static uint32_t kind_type(uint32_t bits, int kind)
{
    switch (kind) {
    case VKD_MEM_UPLOAD:
        return vkd_memory_type(bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 0);
    case VKD_MEM_READBACK: {
        uint32_t t = vkd_memory_type(bits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT |
                                     VK_MEMORY_PROPERTY_HOST_CACHED_BIT, 0);
        return t != ~0u ? t : kind_type(bits, VKD_MEM_UPLOAD);
    }
    default: {
        uint32_t t = vkd_memory_type(bits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT);
        return t != ~0u ? t : vkd_memory_type(bits, 0, 0);
    }
    }
}

#define BUFFER_USAGE (VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | \
                      VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | \
                      VK_BUFFER_USAGE_INDEX_BUFFER_BIT)

static VkResult raw_alloc(VkDeviceSize size, uint32_t type, VkDeviceMemory *mem)
{
    VkMemoryAllocateInfo ai = {VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    ai.allocationSize = size;
    ai.memoryTypeIndex = type;
    return vkAllocateMemory(vkd.dev, &ai, 0, mem);
}

static uint32_t buffer_type_bits(void)
{
    static uint32_t bits;
    if (!bits) {
        VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = 65536;
        bi.usage = BUFFER_USAGE;
        VkBuffer b;
        if (vkCreateBuffer(vkd.dev, &bi, 0, &b) == VK_SUCCESS) {
            VkMemoryRequirements req;
            vkGetBufferMemoryRequirements(vkd.dev, b, &req);
            bits = req.memoryTypeBits;
            vkDestroyBuffer(vkd.dev, b, 0);
        }
    }
    return bits;
}

static struct vkd_block *new_block(VkDeviceSize size, uint32_t type, int kind, int image)
{
    struct vkd_block *b = vkp_alloc(sizeof *b);
    if (!b) return 0;
    if (raw_alloc(size, type, &b->mem) != VK_SUCCESS) { vkp_free(b); return 0; }
    b->size = size; b->type = type; b->kind = kind; b->image = image;
    if (vkd.memprops.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
        vkMapMemory(vkd.dev, b->mem, 0, VK_WHOLE_SIZE, 0, (void **)&b->map);
    if (!image) {
        VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        bi.size = size;
        bi.usage = BUFFER_USAGE;
        vkCreateBuffer(vkd.dev, &bi, 0, &b->buffer);
        vkBindBufferMemory(vkd.dev, b->buffer, b->mem, 0);
    }
    b->free = vkp_alloc(sizeof *b->free);
    b->free->off = 0; b->free->size = size;
    b->next = vkd.blocks;
    vkd.blocks = b;
    return b;
}

static int block_alloc(struct vkd_block *b, VkDeviceSize size, VkDeviceSize align, VkDeviceSize *off)
{
    for (struct vkd_range **pr = &b->free; *pr; pr = &(*pr)->next) {
        struct vkd_range *r = *pr;
        VkDeviceSize start = (r->off + align - 1) / align * align;
        if (start + size > r->off + r->size) continue;
        VkDeviceSize head = start - r->off, tail = r->off + r->size - (start + size);
        if (head && tail) {
            struct vkd_range *t = vkp_alloc(sizeof *t);
            if (!t) return 0;
            t->off = start + size; t->size = tail; t->next = r->next;
            r->size = head; r->next = t;
        } else if (head) {
            r->size = head;
        } else if (tail) {
            r->off = start + size; r->size = tail;
        } else {
            *pr = r->next;
            vkp_free(r);
        }
        *off = start;
        return 1;
    }
    return 0;
}

static void block_free(struct vkd_block *b, VkDeviceSize off, VkDeviceSize size)
{
    struct vkd_range **pr = &b->free, *prev = 0;
    while (*pr && (*pr)->off < off) { prev = *pr; pr = &(*pr)->next; }
    struct vkd_range *next = *pr;
    if (prev && prev->off + prev->size == off) {
        prev->size += size;
        if (next && prev->off + prev->size == next->off) { prev->size += next->size; prev->next = next->next; vkp_free(next); }
        return;
    }
    if (next && off + size == next->off) { next->off = off; next->size += size; return; }
    struct vkd_range *r = vkp_alloc(sizeof *r);
    if (!r) return;     /* leaks the range; out of host memory anyway */
    r->off = off; r->size = size; r->next = next;
    *pr = r;
}

static int suballoc(VkDeviceSize size, VkDeviceSize align, uint32_t type, int kind, int image, struct vkd_mem *out)
{
    VkDeviceSize block = image ? 64u << 20 : kind == VKD_MEM_DEVICE ? 32u << 20 : 16u << 20;
    memset(out, 0, sizeof *out);
    if (size > block / 2) {
        /* dedicated */
        if (raw_alloc(size, type, &out->mem) != VK_SUCCESS) return 0;
        out->size = size;
        if (vkd.memprops.memoryTypes[type].propertyFlags & VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT)
            vkMapMemory(vkd.dev, out->mem, 0, VK_WHOLE_SIZE, 0, (void **)&out->map);
        if (!image) {
            VkBufferCreateInfo bi = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bi.size = size;
            bi.usage = BUFFER_USAGE;
            vkCreateBuffer(vkd.dev, &bi, 0, &out->buffer);
            vkBindBufferMemory(vkd.dev, out->buffer, out->mem, 0);
        }
        return 1;
    }
    for (struct vkd_block *b = vkd.blocks; b; b = b->next) {
        if (b->type != type || b->kind != kind || b->image != image) continue;
        VkDeviceSize off;
        if (block_alloc(b, size, align, &off)) {
            out->mem = b->mem; out->offset = off; out->size = size; out->block = b;
            out->map = b->map ? b->map + off : 0;
            out->buffer = b->buffer;
            return 1;
        }
    }
    struct vkd_block *b = new_block(block, type, kind, image);
    VkDeviceSize off;
    if (!b || !block_alloc(b, size, align, &off)) return 0;
    out->mem = b->mem; out->offset = off; out->size = size; out->block = b;
    out->map = b->map ? b->map + off : 0;
    out->buffer = b->buffer;
    return 1;
}

int vkd_alloc_image_memory(VkImage img, int kind, struct vkd_mem *out)
{
    VkMemoryRequirements req;
    vkGetImageMemoryRequirements(vkd.dev, img, &req);
    uint32_t type = kind_type(req.memoryTypeBits, kind);
    if (type == ~0u || !suballoc(req.size, req.alignment, type, kind, 1, out)) return 0;
    if (vkBindImageMemory(vkd.dev, img, out->mem, out->offset) != VK_SUCCESS) { vkd_free_mem(out); return 0; }
    return 1;
}

int vkd_alloc_buffer(VkDeviceSize size, int kind, struct vkd_mem *out)
{
    uint32_t type = kind_type(buffer_type_bits(), kind);
    VkDeviceSize align = VKP_MAX(vkd.props.limits.minUniformBufferOffsetAlignment, 256);
    if (type == ~0u) return 0;
    return suballoc((size + 15) & ~(VkDeviceSize)15, align, type, kind, 0, out);
}

void vkd_free_mem(struct vkd_mem *m)
{
    if (!m->mem) return;
    if (m->block) block_free(m->block, m->offset, m->size);
    else {
        if (m->buffer) vkDestroyBuffer(vkd.dev, m->buffer, 0);
        vkFreeMemory(vkd.dev, m->mem, 0);
    }
    memset(m, 0, sizeof *m);
}

/* ---- formats ---- */

#define SW(r, g, b, a) {VK_COMPONENT_SWIZZLE_##r, VK_COMPONENT_SWIZZLE_##g, VK_COMPONENT_SWIZZLE_##b, VK_COMPONENT_SWIZZLE_##a}
#define ID SW(IDENTITY, IDENTITY, IDENTITY, IDENTITY)
static struct vkd_format formats[] = {
    {D3DFMT_A8R8G8B8, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB, ID, 4, 1, 0, 0, 0, 0},
    {D3DFMT_X8R8G8B8, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_B8G8R8A8_SRGB, SW(IDENTITY, IDENTITY, IDENTITY, ONE), 4, 1, 0, 0, 0, 1},
    {D3DFMT_A8B8G8R8, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB, ID, 4, 1, 0, 0, 0, 0},
    {D3DFMT_X8B8G8R8, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R8G8B8A8_SRGB, SW(IDENTITY, IDENTITY, IDENTITY, ONE), 4, 1, 0, 0, 0, 1},
    {D3DFMT_R5G6B5, VK_FORMAT_R5G6B5_UNORM_PACK16, 0, ID, 2, 1, 0, 0, 0, 1},
    {D3DFMT_X1R5G5B5, VK_FORMAT_A1R5G5B5_UNORM_PACK16, 0, SW(IDENTITY, IDENTITY, IDENTITY, ONE), 2, 1, 0, 0, 0, 1},
    {D3DFMT_A1R5G5B5, VK_FORMAT_A1R5G5B5_UNORM_PACK16, 0, ID, 2, 1, 0, 0, 0, 0},
    {D3DFMT_A4R4G4B4, VK_FORMAT_B4G4R4A4_UNORM_PACK16, 0, SW(G, R, A, B), 2, 1, 0, 0, 0, 0},
    {D3DFMT_X4R4G4B4, VK_FORMAT_B4G4R4A4_UNORM_PACK16, 0, SW(G, R, A, ONE), 2, 1, 0, 0, 0, 1},
    {D3DFMT_A2R10G10B10, VK_FORMAT_A2R10G10B10_UNORM_PACK32, 0, ID, 4, 1, 0, 0, 0, 0},
    {D3DFMT_A2B10G10R10, VK_FORMAT_A2B10G10R10_UNORM_PACK32, 0, ID, 4, 1, 0, 0, 0, 0},
    {D3DFMT_G16R16, VK_FORMAT_R16G16_UNORM, 0, SW(IDENTITY, IDENTITY, ONE, ONE), 4, 1, 0, 0, 0, 0},
    {D3DFMT_A16B16G16R16, VK_FORMAT_R16G16B16A16_UNORM, 0, ID, 8, 1, 0, 0, 0, 0},
    {D3DFMT_A8, VK_FORMAT_R8_UNORM, 0, SW(ZERO, ZERO, ZERO, R), 1, 1, 0, 0, 0, 0},
    {D3DFMT_L8, VK_FORMAT_R8_UNORM, 0, SW(R, R, R, ONE), 1, 1, 0, 0, 0, 0},
    {D3DFMT_A8L8, VK_FORMAT_R8G8_UNORM, 0, SW(R, R, R, G), 2, 1, 0, 0, 0, 0},
    {D3DFMT_A4L4, VK_FORMAT_R4G4_UNORM_PACK8, 0, SW(G, G, G, R), 1, 1, 0, 0, 0, 0},
    {D3DFMT_L16, VK_FORMAT_R16_UNORM, 0, SW(R, R, R, ONE), 2, 1, 0, 0, 0, 0},
    {D3DFMT_V8U8, VK_FORMAT_R8G8_SNORM, 0, SW(IDENTITY, IDENTITY, ONE, ONE), 2, 1, 0, 0, 0, 0},
    {D3DFMT_Q8W8V8U8, VK_FORMAT_R8G8B8A8_SNORM, 0, ID, 4, 1, 0, 0, 0, 0},
    {D3DFMT_V16U16, VK_FORMAT_R16G16_SNORM, 0, SW(IDENTITY, IDENTITY, ONE, ONE), 4, 1, 0, 0, 0, 0},
    {D3DFMT_Q16W16V16U16, VK_FORMAT_R16G16B16A16_SNORM, 0, ID, 8, 1, 0, 0, 0, 0},
    {D3DFMT_R16F, VK_FORMAT_R16_SFLOAT, 0, SW(IDENTITY, ONE, ONE, ONE), 2, 1, 0, 0, 0, 0},
    {D3DFMT_G16R16F, VK_FORMAT_R16G16_SFLOAT, 0, SW(IDENTITY, IDENTITY, ONE, ONE), 4, 1, 0, 0, 0, 0},
    {D3DFMT_A16B16G16R16F, VK_FORMAT_R16G16B16A16_SFLOAT, 0, ID, 8, 1, 0, 0, 0, 0},
    {D3DFMT_R32F, VK_FORMAT_R32_SFLOAT, 0, SW(IDENTITY, ONE, ONE, ONE), 4, 1, 0, 0, 0, 0},
    {D3DFMT_G32R32F, VK_FORMAT_R32G32_SFLOAT, 0, SW(IDENTITY, IDENTITY, ONE, ONE), 8, 1, 0, 0, 0, 0},
    {D3DFMT_A32B32G32R32F, VK_FORMAT_R32G32B32A32_SFLOAT, 0, ID, 16, 1, 0, 0, 0, 0},
    {D3DFMT_DXT1, VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC1_RGBA_SRGB_BLOCK, ID, 8, 4, 0, 0, 0, 0},
    {D3DFMT_DXT2, VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK, ID, 16, 4, 0, 0, 0, 0},
    {D3DFMT_DXT3, VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC2_SRGB_BLOCK, ID, 16, 4, 0, 0, 0, 0},
    {D3DFMT_DXT4, VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK, ID, 16, 4, 0, 0, 0, 0},
    {D3DFMT_DXT5, VK_FORMAT_BC3_UNORM_BLOCK, VK_FORMAT_BC3_SRGB_BLOCK, ID, 16, 4, 0, 0, 0, 0},
    {D3DFMT_D16, VK_FORMAT_D16_UNORM, 0, ID, 2, 1, 16, 0, 0, 0},
    {D3DFMT_D16_LOCKABLE, VK_FORMAT_D16_UNORM, 0, ID, 2, 1, 16, 0, 0, 0},
    {D3DFMT_D24S8, VK_FORMAT_D24_UNORM_S8_UINT, 0, ID, 4, 1, 24, 8, 0, 0},
    {D3DFMT_D24X8, VK_FORMAT_D24_UNORM_S8_UINT, 0, ID, 4, 1, 24, 0, 0, 0},
    {D3DFMT_D24X4S4, VK_FORMAT_D24_UNORM_S8_UINT, 0, ID, 4, 1, 24, 4, 0, 0},
    {D3DFMT_D15S1, VK_FORMAT_D24_UNORM_S8_UINT, 0, ID, 2, 1, 24, 1, 0, 0},
    {D3DFMT_D32, VK_FORMAT_D32_SFLOAT, 0, ID, 4, 1, 32, 0, 0, 0},
    {D3DFMT_D32F_LOCKABLE, VK_FORMAT_D32_SFLOAT, 0, ID, 4, 1, 32, 0, 0, 0},
    {D3DFMT_D24FS8, VK_FORMAT_D32_SFLOAT_S8_UINT, 0, ID, 4, 1, 32, 8, 0, 0},
    {(D3DFORMAT)MAKEFOURCC('I', 'N', 'T', 'Z'), VK_FORMAT_D24_UNORM_S8_UINT, 0, SW(R, R, R, R), 4, 1, 24, 8, 1, 0},
    {(D3DFORMAT)MAKEFOURCC('D', 'F', '2', '4'), VK_FORMAT_D24_UNORM_S8_UINT, 0, SW(R, R, R, R), 4, 1, 24, 8, 1, 0},
    {(D3DFORMAT)MAKEFOURCC('D', 'F', '1', '6'), VK_FORMAT_D16_UNORM, 0, SW(R, R, R, R), 2, 1, 16, 0, 1, 0},
    {(D3DFORMAT)MAKEFOURCC('N', 'U', 'L', 'L'), VK_FORMAT_UNDEFINED, 0, ID, 0, 1, 0, 0, 0, 0},
    {D3DFMT_INDEX16, VK_FORMAT_UNDEFINED, 0, ID, 2, 1, 0, 0, 0, 0},
    {D3DFMT_INDEX32, VK_FORMAT_UNDEFINED, 0, ID, 4, 1, 0, 0, 0, 0},
    {D3DFMT_VERTEXDATA, VK_FORMAT_UNDEFINED, 0, ID, 1, 1, 0, 0, 0, 0},
};

const struct vkd_format *vkd_format(D3DFORMAT f)
{
    static int fixed;
    if (!fixed && vkd.dev) {
        fixed = 1;
        for (uint32_t i = 0; i < VKP_COUNT(formats); i++) {
            struct vkd_format *e = &formats[i];
            if (e->vk == VK_FORMAT_D24_UNORM_S8_UINT && !vkd.d24s8) e->vk = vkd.ds24;
            if (vkd.format_4444 && e->d3d == D3DFMT_A4R4G4B4) {
                e->vk = VK_FORMAT_A4R4G4B4_UNORM_PACK16;
                e->swz = (VkComponentMapping)ID;
            }
            if (vkd.format_4444 && e->d3d == D3DFMT_X4R4G4B4) {
                e->vk = VK_FORMAT_A4R4G4B4_UNORM_PACK16;
                e->swz = (VkComponentMapping)SW(IDENTITY, IDENTITY, IDENTITY, ONE);
            }
        }
    }
    for (uint32_t i = 0; i < VKP_COUNT(formats); i++)
        if (formats[i].d3d == f) return &formats[i];
    return 0;
}

VkFormat vkd_attach_format(const struct vkd_format *f) { return f ? f->vk : VK_FORMAT_UNDEFINED; }

int vkd_format_supported(const struct vkd_format *f, VkFormatFeatureFlags need, int optimal)
{
    if (!f || f->vk == VK_FORMAT_UNDEFINED) return 0;
    VkFormatProperties p;
    vkGetPhysicalDeviceFormatProperties(vkd.phys, f->vk, &p);
    VkFormatFeatureFlags have = optimal ? p.optimalTilingFeatures : p.linearTilingFeatures;
    return (have & need) == need;
}

void vkd_layout(const struct vkd_format *f, UINT w, UINT h, UINT d, UINT *pitch, UINT *slice, UINT *rows)
{
    UINT bw = f->block > 1 ? (w + 3) / 4 : w, bh = f->block > 1 ? (h + 3) / 4 : h;
    UINT p = bw * f->bytes;
    if (f->block == 1) p = (p + 3) & ~3u;
    if (pitch) *pitch = p;
    if (rows) *rows = bh;
    if (slice) *slice = p * bh;
    (void)d;
}

/* ---- hash map ---- */

uint32_t vkd_hash(const void *p, uint32_t n)
{
    const uint8_t *b = p;
    uint32_t h = 2166136261u;
    for (uint32_t i = 0; i < n; i++) h = (h ^ b[i]) * 16777619u;
    return h ? h : 1;
}

void *vkd_map_get(struct vkd_map *m, const void *key, uint32_t n)
{
    if (!m->cap) return 0;
    uint32_t h = vkd_hash(key, n);
    for (uint32_t i = h & (m->cap - 1);; i = (i + 1) & (m->cap - 1)) {
        struct vkd_map_entry *e = &m->e[i];
        if (!e->hash) return 0;
        if (e->hash == h && e->size == n && !memcmp(e->key, key, n)) return e->val;
    }
}

void vkd_map_put(struct vkd_map *m, const void *key, uint32_t n, void *val)
{
    uint32_t h = vkd_hash(key, n);
    if (m->cap) {
        for (uint32_t i = h & (m->cap - 1); m->e[i].hash; i = (i + 1) & (m->cap - 1)) {
            if (m->e[i].hash == h && m->e[i].size == n && !memcmp(m->e[i].key, key, n)) {
                m->e[i].val = val;
                return;
            }
        }
    }
    if ((m->count + 1) * 2 > m->cap) {
        uint32_t cap = m->cap ? m->cap * 2 : 64;
        struct vkd_map_entry *e = vkp_alloc(cap * sizeof *e);
        if (!e) return;
        for (uint32_t i = 0; i < m->cap; i++) {
            if (!m->e[i].hash) continue;
            uint32_t j = m->e[i].hash & (cap - 1);
            while (e[j].hash) j = (j + 1) & (cap - 1);
            e[j] = m->e[i];
        }
        vkp_free(m->e);
        m->e = e; m->cap = cap;
    }
    uint32_t i = h & (m->cap - 1);
    while (m->e[i].hash) i = (i + 1) & (m->cap - 1);
    void *k = vkp_alloc(n);
    if (!k) return;
    memcpy(k, key, n);
    m->e[i].hash = h; m->e[i].size = n; m->e[i].key = k; m->e[i].val = val;
    m->count++;
}

void vkd_map_clear(struct vkd_map *m, void (*drop)(void *val, void *ctx), void *ctx)
{
    for (uint32_t i = 0; i < m->cap; i++)
        if (m->e[i].hash) {
            if (drop) drop(m->e[i].val, ctx);
            vkp_free(m->e[i].key);
        }
    vkp_free(m->e);
    memset(m, 0, sizeof *m);
}

/* ---- submission ---- */

void vkd_barrier(VkCommandBuffer cmd, VkPipelineStageFlags2 src, VkAccessFlags2 srca, VkPipelineStageFlags2 dst,
                 VkAccessFlags2 dsta)
{
    VkMemoryBarrier2 mb = {VK_STRUCTURE_TYPE_MEMORY_BARRIER_2};
    mb.srcStageMask = src; mb.srcAccessMask = srca;
    mb.dstStageMask = dst; mb.dstAccessMask = dsta;
    VkDependencyInfo di = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.memoryBarrierCount = 1;
    di.pMemoryBarriers = &mb;
    vkCmdPipelineBarrier2(cmd, &di);
}

void vkd_barrier_all(VkCommandBuffer cmd)
{
    vkd_barrier(cmd, VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_WRITE_BIT,
                VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT, VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT);
}

static void retire(struct vkd_device *d, struct vkd_context *c)
{
    for (uint32_t i = 0; i < c->ngarbage; i++) {
        struct vkd_garbage *g = &c->garbage[i];
        switch (g->kind) {
        case G_BUFFER: vkDestroyBuffer(vkd.dev, g->h.buf, 0); break;
        case G_IMAGE: vkDestroyImage(vkd.dev, g->h.img, 0); break;
        case G_VIEW: vkDestroyImageView(vkd.dev, g->h.view, 0); break;
        case G_SAMPLER: vkDestroySampler(vkd.dev, g->h.smp, 0); break;
        case G_MODULE: vkDestroyShaderModule(vkd.dev, g->h.mod, 0); break;
        case G_PIPELINE: vkDestroyPipeline(vkd.dev, g->h.pipe, 0); break;
        case G_SWAPCHAIN: vkDestroySwapchainKHR(vkd.dev, g->h.sc, 0); break;
        case G_SEMAPHORE: vkDestroySemaphore(vkd.dev, g->h.sem, 0); break;
        case G_QUERYPOOL: vkDestroyQueryPool(vkd.dev, g->h.qp, 0); break;
        case G_SURFACE: vkDestroySurfaceKHR(vkd.instance, g->h.surf, 0); break;
        }
        vkd_free_mem(&g->mem);
    }
    c->ngarbage = 0;
    vkd_queries_retire(d, c);
    while (c->chunks) {
        struct vkd_chunk *k = c->chunks;
        c->chunks = k->next;
        k->used = 0;
        k->next = d->free_chunks;
        d->free_chunks = k;
    }
    c->submitted = 0;
    if (c->seq > d->completed) d->completed = c->seq;
}

static void begin_context(struct vkd_device *d)
{
    struct vkd_context *c = &d->ctx[d->cur];
    if (c->submitted) {
        vkWaitForFences(vkd.dev, 1, &c->fence, VK_TRUE, UINT64_MAX);
        retire(d, c);
    }
    vkResetFences(vkd.dev, 1, &c->fence);
    vkResetCommandPool(vkd.dev, c->pool, 0);
    c->seq = d->seq;
    c->init_used = 0;
    VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer(c->cmd, &bi);
}

int vkd_device_init_vk(struct vkd_device *d)
{
    for (int i = 0; i < VKD_CONTEXTS; i++) {
        struct vkd_context *c = &d->ctx[i];
        VkCommandPoolCreateInfo pci = {VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
        pci.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
        pci.queueFamilyIndex = vkd.queue_family;
        if (vkCreateCommandPool(vkd.dev, &pci, 0, &c->pool) != VK_SUCCESS) return 0;
        VkCommandBufferAllocateInfo ai = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
        ai.commandPool = c->pool;
        ai.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        ai.commandBufferCount = 2;
        VkCommandBuffer cb[2];
        if (vkAllocateCommandBuffers(vkd.dev, &ai, cb) != VK_SUCCESS) return 0;
        c->cmd = cb[0]; c->init = cb[1];
        VkFenceCreateInfo fci = {VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
        if (vkCreateFence(vkd.dev, &fci, 0, &c->fence) != VK_SUCCESS) return 0;
    }
    d->seq = 1;
    d->completed = 0;
    d->cur = 0;
    begin_context(d);
    return 1;
}

void vkd_device_free_vk(struct vkd_device *d)
{
    if (!vkd.dev) return;
    vkDeviceWaitIdle(vkd.dev);
    for (int i = 0; i < VKD_CONTEXTS; i++) {
        struct vkd_context *c = &d->ctx[i];
        if (!c->pool) continue;
        retire(d, c);
        vkd_queries_free(d, c);
        vkp_free(c->garbage);
        c->garbage = 0; c->capgarbage = 0;
        vkDestroyFence(vkd.dev, c->fence, 0);
        vkDestroyCommandPool(vkd.dev, c->pool, 0);
        c->pool = 0;
    }
    while (d->free_chunks) {
        struct vkd_chunk *k = d->free_chunks;
        d->free_chunks = k->next;
        vkd_free_mem(&k->mem);
        vkp_free(k);
    }
}

VkCommandBuffer vkd_cmd(struct vkd_device *d) { return d->ctx[d->cur].cmd; }

VkCommandBuffer vkd_init_cmd(struct vkd_device *d)
{
    struct vkd_context *c = &d->ctx[d->cur];
    if (!c->init_used) {
        VkCommandBufferBeginInfo bi = {VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
        bi.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(c->init, &bi);
        /* earlier submissions may still read what this buffer overwrites */
        vkd_barrier_all(c->init);
        c->init_used = 1;
    }
    return c->init;
}

VkCommandBuffer vkd_upload_cmd(struct vkd_device *d, uint64_t *use_seq)
{
    if (use_seq && *use_seq == d->seq) {
        vkd_end_rendering(d);
        VkCommandBuffer cmd = vkd_cmd(d);
        vkd_barrier_all(cmd);
        return cmd;
    }
    return vkd_init_cmd(d);
}

void vkd_submit(struct vkd_device *d, VkSemaphore wait, VkSemaphore signal)
{
    vkd_end_rendering(d);
    vkd_queries_pass_end(d);
    struct vkd_context *c = &d->ctx[d->cur];
    vkd_barrier_all(c->cmd);
    vkEndCommandBuffer(c->cmd);
    VkCommandBuffer cbs[2];
    uint32_t n = 0;
    if (c->init_used) {
        vkd_barrier_all(c->init);
        vkEndCommandBuffer(c->init);
        cbs[n++] = c->init;
    }
    cbs[n++] = c->cmd;
    VkSubmitInfo si = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
    VkPipelineStageFlags ws = VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
    if (wait) { si.waitSemaphoreCount = 1; si.pWaitSemaphores = &wait; si.pWaitDstStageMask = &ws; }
    if (signal) { si.signalSemaphoreCount = 1; si.pSignalSemaphores = &signal; }
    si.commandBufferCount = n;
    si.pCommandBuffers = cbs;
    VkResult r = vkQueueSubmit(vkd.queue, 1, &si, c->fence);
    if (r != VK_SUCCESS) vkp_log("vulkan: vkQueueSubmit failed (%d)", r);
    c->submitted = 1;
    d->cur = (d->cur + 1) % VKD_CONTEXTS;
    d->seq++;
    begin_context(d);
    d->dirty = DIRTY_ALL;
    d->bound_pipe = 0;
}

void vkd_poll(struct vkd_device *d)
{
    for (int i = 0; i < VKD_CONTEXTS; i++) {
        struct vkd_context *c = &d->ctx[i];
        if (c->submitted && vkGetFenceStatus(vkd.dev, c->fence) == VK_SUCCESS) retire(d, c);
    }
}

void vkd_wait_seq(struct vkd_device *d, uint64_t seq)
{
    if (seq <= d->completed) return;
    if (seq >= d->seq) vkd_submit(d, 0, 0);
    for (int i = 0; i < VKD_CONTEXTS; i++) {
        struct vkd_context *c = &d->ctx[i];
        if (c->submitted && c->seq <= seq) {
            vkWaitForFences(vkd.dev, 1, &c->fence, VK_TRUE, UINT64_MAX);
            retire(d, c);
        }
    }
    if (d->completed < seq) d->completed = seq;
}

void vkd_flush_wait(struct vkd_device *d) { vkd_wait_seq(d, d->seq); }

void vkd_defer(struct vkd_device *d, const struct vkd_garbage *g)
{
    struct vkd_context *c = &d->ctx[d->cur];
    if (c->ngarbage == c->capgarbage) {
        uint32_t cap = c->capgarbage ? c->capgarbage * 2 : 64;
        struct vkd_garbage *n = vkp_realloc(c->garbage, cap * sizeof *n);
        if (!n) return;
        c->garbage = n; c->capgarbage = cap;
    }
    c->garbage[c->ngarbage++] = *g;
}

int vkd_ring(struct vkd_device *d, VkDeviceSize size, VkDeviceSize align, struct vkd_buf *out)
{
    return vkd_ring_span(d, size, size, align, out);
}

int vkd_ring_span(struct vkd_device *d, VkDeviceSize size, VkDeviceSize span, VkDeviceSize align, struct vkd_buf *out)
{
    struct vkd_context *c = &d->ctx[d->cur];
    if (align < 16) align = 16;
    VkDeviceSize need = VKP_MAX(size, span);
    struct vkd_chunk *k = c->chunks;
    if (k) {
        VkDeviceSize off = (k->used + align - 1) / align * align;
        if (off + need <= k->mem.size) {
            out->buf = k->mem.buffer; out->off = k->mem.offset + off; out->map = k->mem.map + off;
            out->mem.mem = 0; out->use_seq = 0;
            k->used = off + size;
            return 1;
        }
    }
    /* another chunk */
    struct vkd_chunk **pk = &d->free_chunks;
    while (*pk && (*pk)->mem.size < need) pk = &(*pk)->next;
    if (*pk) {
        k = *pk;
        *pk = k->next;
    } else {
        k = vkp_alloc(sizeof *k);
        if (!k) return 0;
        VkDeviceSize csize = VKP_MAX((VkDeviceSize)4 << 20, (need + 0xffff) & ~(VkDeviceSize)0xffff);
        if (!vkd_alloc_buffer(csize, VKD_MEM_UPLOAD, &k->mem) || !k->mem.map) { vkp_free(k); return 0; }
        k->mem.size = csize;
    }
    k->used = 0;
    k->next = c->chunks;
    c->chunks = k;
    out->buf = k->mem.buffer; out->off = k->mem.offset; out->map = k->mem.map;
    out->mem.mem = 0; out->use_seq = 0;
    k->used = size;
    return 1;
}

/* ---- images ---- */

struct vkd_image *vkd_image_create(struct vkd_device *d, const struct vkd_format *f, VkImageType type, UINT w,
                                   UINT h, UINT depth, UINT levels, UINT layers, VkSampleCountFlagBits samples,
                                   int cube, int attach)
{
    struct vkd_image *img = vkp_alloc(sizeof *img);
    if (!img) return 0;
    VkImageCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO};
    VkFormat list[2] = {f->vk, f->vk_srgb};
    VkImageFormatListCreateInfo fl = {VK_STRUCTURE_TYPE_IMAGE_FORMAT_LIST_CREATE_INFO};
    if (f->vk_srgb) {
        ci.flags |= VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;
        fl.viewFormatCount = 2;
        fl.pViewFormats = list;
        ci.pNext = &fl;
    }
    if (cube) ci.flags |= VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT;
    ci.imageType = type;
    ci.format = f->vk;
    ci.extent.width = w; ci.extent.height = h; ci.extent.depth = depth;
    ci.mipLevels = levels;
    ci.arrayLayers = layers;
    ci.samples = samples;
    ci.tiling = VK_IMAGE_TILING_OPTIMAL;
    ci.usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    int ds = f->depth || f->stencil;
    if (attach) ci.usage |= ds ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    ci.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(vkd.dev, &ci, 0, &img->img) != VK_SUCCESS) { vkp_free(img); return 0; }
    if (!vkd_alloc_image_memory(img->img, VKD_MEM_DEVICE, &img->mem)) {
        vkDestroyImage(vkd.dev, img->img, 0);
        vkp_free(img);
        return 0;
    }
    img->fmt = f;
    img->vkfmt = f->vk;
    img->aspect = ds ? (VK_IMAGE_ASPECT_DEPTH_BIT | (f->vk == VK_FORMAT_D24_UNORM_S8_UINT || f->vk == VK_FORMAT_D32_SFLOAT_S8_UINT ||
                                                   f->vk == VK_FORMAT_D16_UNORM_S8_UINT ? VK_IMAGE_ASPECT_STENCIL_BIT : 0))
                     : VK_IMAGE_ASPECT_COLOR_BIT;
    img->w = w; img->h = h; img->d = depth; img->levels = levels; img->layers = layers;
    img->samples = samples;
    img->attachable = attach;
    img->view_type = cube ? VK_IMAGE_VIEW_TYPE_CUBE : type == VK_IMAGE_TYPE_3D ? VK_IMAGE_VIEW_TYPE_3D : VK_IMAGE_VIEW_TYPE_2D;
    img->view = vkd_image_view(img, f->vk, img->view_type, 0, levels, 0, layers, 1);
    if (f->vk_srgb) img->view_srgb = vkd_image_view(img, f->vk_srgb, img->view_type, 0, levels, 0, layers, 1);

    /* to GENERAL, cleared */
    VkCommandBuffer cmd = vkd_init_cmd(d);
    VkImageMemoryBarrier2 ib = {VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2};
    ib.srcStageMask = VK_PIPELINE_STAGE_2_NONE;
    ib.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
    ib.dstAccessMask = VK_ACCESS_2_MEMORY_READ_BIT | VK_ACCESS_2_MEMORY_WRITE_BIT;
    ib.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    ib.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    ib.srcQueueFamilyIndex = ib.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    ib.image = img->img;
    ib.subresourceRange.aspectMask = img->aspect;
    ib.subresourceRange.levelCount = VK_REMAINING_MIP_LEVELS;
    ib.subresourceRange.layerCount = VK_REMAINING_ARRAY_LAYERS;
    VkDependencyInfo di = {VK_STRUCTURE_TYPE_DEPENDENCY_INFO};
    di.imageMemoryBarrierCount = 1;
    di.pImageMemoryBarriers = &ib;
    vkCmdPipelineBarrier2(cmd, &di);
    if (attach) {
        if (ds) {
            VkClearDepthStencilValue cv = {1.0f, 0};
            vkCmdClearDepthStencilImage(cmd, img->img, VK_IMAGE_LAYOUT_GENERAL, &cv, 1, &ib.subresourceRange);
        } else {
            VkClearColorValue cv = {{0, 0, 0, 0}};
            vkCmdClearColorImage(cmd, img->img, VK_IMAGE_LAYOUT_GENERAL, &cv, 1, &ib.subresourceRange);
        }
        vkd_barrier_all(cmd);
    }
    return img;
}

void vkd_image_destroy(struct vkd_device *d, struct vkd_image *img)
{
    if (!img) return;
    struct vkd_garbage g = {0};
    if (img->view) { g.kind = G_VIEW; g.h.view = img->view; vkd_defer(d, &g); }
    if (img->view_srgb) { g.kind = G_VIEW; g.h.view = img->view_srgb; vkd_defer(d, &g); }
    g.kind = G_IMAGE; g.h.img = img->img; g.mem = img->mem;
    vkd_defer(d, &g);
    vkp_free(img);
}

VkImageView vkd_image_view(struct vkd_image *img, VkFormat fmt, VkImageViewType type, uint32_t level,
                           uint32_t nlevels, uint32_t layer, uint32_t nlayers, int swizzle)
{
    VkImageViewCreateInfo ci = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    ci.image = img->img;
    ci.viewType = type;
    ci.format = fmt;
    if (swizzle) ci.components = img->fmt->swz;
    ci.subresourceRange.aspectMask = img->aspect;
    /* sampled depth/stencil views read depth only */
    if (swizzle && (img->aspect & VK_IMAGE_ASPECT_DEPTH_BIT)) ci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    ci.subresourceRange.baseMipLevel = level;
    ci.subresourceRange.levelCount = nlevels;
    ci.subresourceRange.baseArrayLayer = layer;
    ci.subresourceRange.layerCount = nlayers;
    VkImageView v = 0;
    if (vkCreateImageView(vkd.dev, &ci, 0, &v) != VK_SUCCESS) return 0;
    return v;
}

int vkd_buf_alloc(struct vkd_device *d, VkDeviceSize size, int host, struct vkd_buf *out)
{
    memset(out, 0, sizeof *out);
    if (!vkd_alloc_buffer(size ? size : 16, host ? VKD_MEM_UPLOAD : VKD_MEM_DEVICE, &out->mem)) return 0;
    out->buf = out->mem.buffer;
    out->off = out->mem.offset;
    out->map = out->mem.map;
    (void)d;
    return 1;
}

void vkd_buf_free(struct vkd_device *d, struct vkd_buf *b)
{
    if (!b->mem.mem) return;
    struct vkd_garbage g = {0};
    g.kind = G_MEM;
    g.mem = b->mem;
    vkd_defer(d, &g);
    memset(b, 0, sizeof *b);
}

VkShaderModule vkd_module(const uint32_t *code, uint32_t words)
{
    VkShaderModuleCreateInfo ci = {VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};
    ci.codeSize = words * 4;
    ci.pCode = code;
    VkShaderModule m = 0;
    if (vkCreateShaderModule(vkd.dev, &ci, 0, &m) != VK_SUCCESS) return 0;
    return m;
}
