/* Platform layer of the Vulkan renderer. The game build implements it with the
 * game's own kernel32 imports (platform.c); the test harness with the C
 * runtime (test/harness_platform.c). */
#ifndef BFV_VKP_H
#define BFV_VKP_H
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

void *vkp_alloc(size_t n);                 /* zeroed; 0 on failure */
void *vkp_realloc(void *p, size_t n);      /* new tail is not zeroed */
void vkp_free(void *p);
/* Log line to the renderer log (vulkan.log in the game folder). printf
 * subset: %d %u %x %s %p %f %c %%, width and 0 flag for d/u/x. */
void vkp_log(const char *fmt, ...);
void vkp_vformat(char *out, size_t size, const char *fmt, va_list ap);
void vkp_format(char *out, size_t size, const char *fmt, ...);
/* exported function of a DLL, loading it on first use; 0 if missing */
void *vkp_proc(const char *dll, const char *name);
/* value of `key` in dxvk.conf of the game folder, or `def` */
int vkp_option_int(const char *key, int def);
uint64_t vkp_time_us(void);
void vkp_sleep(uint32_t ms);
/* recursive lock for D3DCREATE_MULTITHREADED */
void *vkp_lock_new(void);
void vkp_lock(void *l);
void vkp_unlock(void *l);
void vkp_lock_free(void *l);
/* whole file into memory (vkp_free it) or 0; write a file */
void *vkp_read_file(const char *path, size_t *size);
int vkp_write_file(const char *path, const void *data, size_t size);

/* memcpy and friends: the mod runtime in the game build, the C runtime in the harness */
#include <string.h>

static inline size_t vkp_strlen(const char *s) { size_t n = 0; while (s[n]) n++; return n; }
static inline int vkp_streq(const char *a, const char *b)
{
    while (*a && *a == *b) a++, b++;
    return *a == *b;
}
#define VKP_MIN(a, b) ((a) < (b) ? (a) : (b))
#define VKP_MAX(a, b) ((a) > (b) ? (a) : (b))
#define VKP_COUNT(a) (sizeof(a) / sizeof((a)[0]))

#endif
