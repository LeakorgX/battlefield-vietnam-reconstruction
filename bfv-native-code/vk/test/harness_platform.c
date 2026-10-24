/* vkp_* for the offline test harness, on the C runtime and Win32. */
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "../vkp.h"

void *vkp_alloc(size_t n) { return calloc(1, n ? n : 1); }
void *vkp_realloc(void *p, size_t n) { return realloc(p, n ? n : 1); }
void vkp_free(void *p) { free(p); }

void vkp_vformat(char *out, size_t size, const char *fmt, va_list ap) { vsnprintf(out, size, fmt, ap); }
void vkp_format(char *out, size_t size, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(out, size, fmt, ap);
    va_end(ap);
}
void vkp_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);
}
void *vkp_proc(const char *dll, const char *name)
{
    HMODULE m = GetModuleHandleA(dll);
    if (!m) m = LoadLibraryA(dll);
    return m ? (void *)GetProcAddress(m, name) : 0;
}
int vkp_option_int(const char *key, int def)
{
    const char *v = getenv(key);
    return v ? atoi(v) : def;
}
uint64_t vkp_time_us(void)
{
    LARGE_INTEGER f, c;
    QueryPerformanceFrequency(&f);
    QueryPerformanceCounter(&c);
    return (uint64_t)(c.QuadPart * 1000000.0 / f.QuadPart);
}
void vkp_sleep(uint32_t ms) { Sleep(ms); }
void *vkp_lock_new(void)
{
    CRITICAL_SECTION *cs = calloc(1, sizeof *cs);
    InitializeCriticalSection(cs);
    return cs;
}
void vkp_lock(void *l) { EnterCriticalSection(l); }
void vkp_unlock(void *l) { LeaveCriticalSection(l); }
void vkp_lock_free(void *l) { DeleteCriticalSection(l); free(l); }
void *vkp_read_file(const char *path, size_t *size)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    void *p = malloc(n ? (size_t)n : 1);
    if (p && fread(p, 1, (size_t)n, f) != (size_t)n) { free(p); p = 0; }
    fclose(f);
    if (p && size) *size = (size_t)n;
    return p;
}
int vkp_write_file(const char *path, const void *data, size_t size)
{
    FILE *f = fopen(path, "wb");
    if (!f) return 0;
    int ok = fwrite(data, 1, size, f) == size;
    fclose(f);
    return ok;
}
