/* Explicit opt-in renderer backend. 00439ee8 is the Create9 call site in
 * compatibility setup00439260. Primary renderer initialization00a5f700 calls
 * the same thunk earlier, so both entrypoints install the backend. */
#define BFV_MOD_NO_NAMES
#include "bfv_mod.h"
extern void *__attribute__((stdcall)) vk_Direct3DCreate9(unsigned sdk_version);
extern void *vkp_proc(const char *dll, const char *name);
extern void vkp_log(const char *fmt, ...);
MOD_BEFORE(0x00439260, vulkan_install_backend)
{
    typedef int (__attribute__((stdcall)) *protect_fn)(void *, unsigned, unsigned, unsigned *);
    protect_fn protect = (protect_fn)vkp_proc("kernel32.dll", "VirtualProtect");
    unsigned previous, ignored;
    if (!protect || !protect((void *)0x00b42444u, sizeof(uint32_t), 0x04u, &previous)) {
        vkp_log("vulkan: failed to make Direct3DCreate9 IAT writable");
        MOD_RETURN(0);
    }
    M32(0x00b42444u) = (uint32_t)(uintptr_t)vk_Direct3DCreate9;
    if (!protect((void *)0x00b42444u, sizeof(uint32_t), previous, &ignored)) {
        vkp_log("vulkan: failed to restore Direct3DCreate9 IAT protection");
        MOD_RETURN(0);
    }
    vkp_log("vulkan: renderer setup now uses bfv-vkd3d9");
}

/* PCDX8RendererInit.cpp primary path: call00a5f70e ->0086791a. */
MOD_BEFORE(0x00a5f700, vulkan_install_primary_backend)
{
    vulkan_install_backend(mod);
}
