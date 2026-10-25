/* Freestanding Win32 platform. Only the two existing game imports bootstrap it.
 * BFV_VKP_TEST_SHIM selects host bootstrap imports for the comparison harness. */
#include <initguid.h>
#include "vkd3d9.h"

void *vkp_proc(const char *dll, const char *name)
{
#ifdef BFV_VKP_TEST_SHIM
    HMODULE m = LoadLibraryA(dll);
    return m ? (void *)GetProcAddress(m, name) : 0;
#else
    HMODULE (WINAPI *load)(LPCSTR) = *(void **)0x00b4224c;
    FARPROC (WINAPI *proc)(HMODULE, LPCSTR) = *(void **)0x00b42248;
    HMODULE m = load(dll);
    return m ? (void *)proc(m, name) : 0;
#endif
}

#define PLATFORM_FUNCS(X) \
    X(GetProcessHeap) X(HeapAlloc) X(HeapReAlloc) X(HeapFree) \
    X(CreateFileA) X(ReadFile) X(WriteFile) X(GetFileSizeEx) X(CloseHandle) \
    X(QueryPerformanceCounter) X(QueryPerformanceFrequency) X(Sleep) \
    X(InitializeCriticalSection) X(EnterCriticalSection) X(LeaveCriticalSection) X(DeleteCriticalSection) \
    X(OutputDebugStringA) X(GetEnvironmentVariableA) X(GetModuleFileNameA)
struct platform_api {
#define DECL(f) __typeof__(f) *f;
    PLATFORM_FUNCS(DECL)
#undef DECL
};
static struct platform_api api;
static int ready;
static int platform_init(void)
{
    if (ready) return ready > 0;
    ready = -1;
#define LOAD(f) if (!(api.f = (__typeof__(f) *)vkp_proc("kernel32.dll", #f))) return 0;
    PLATFORM_FUNCS(LOAD)
#undef LOAD
    ready = 1;
    return 1;
}
void *vkp_alloc(size_t n) { return platform_init() ? api.HeapAlloc(api.GetProcessHeap(), HEAP_ZERO_MEMORY, n ? n : 1) : 0; }
void *vkp_realloc(void *p, size_t n)
{
    if (!p) return vkp_alloc(n);
    return platform_init() ? api.HeapReAlloc(api.GetProcessHeap(), 0, p, n ? n : 1) : 0;
}
void vkp_free(void *p) { if (p && platform_init()) api.HeapFree(api.GetProcessHeap(), 0, p); }

struct writer { char *out; size_t n, size; };
static void put(struct writer *w, char c) { if (w->n + 1 < w->size) w->out[w->n] = c; w->n++; }
static void number(struct writer *w, uint64_t n, unsigned base, int width, char fill)
{
    char buf[24]; unsigned count = 0;
    do { buf[count++] = "0123456789abcdef"[n % base]; n /= base; } while (n && count < sizeof buf);
    while (width-- > (int)count) put(w, fill);
    while (count) put(w, buf[--count]);
}
void vkp_vformat(char *out, size_t size, const char *fmt, va_list ap)
{
    struct writer w = {out, 0, size};
    while (*fmt) {
        if (*fmt++ != '%') { put(&w, fmt[-1]); continue; }
        char fill = ' '; int width = 0;
        if (*fmt == '0') { fill = '0'; fmt++; }
        while (*fmt >= '0' && *fmt <= '9') { width = VKP_MIN(width * 10 + *fmt++ - '0', 256); }
        int wide = 0;
        if (*fmt == 'l') { fmt++; if (*fmt == 'l') { wide = 1; fmt++; } }
        char c = *fmt; if (!c) break; fmt++;
        if (c == 's') { const char *s = va_arg(ap, const char *); if (!s) s = "(null)"; while (*s) put(&w, *s++); }
        else if (c == 'c') put(&w, (char)va_arg(ap, int));
        else if (c == '%') put(&w, '%');
        else if (c == 'p') { put(&w, '0'); put(&w, 'x'); number(&w, (uintptr_t)va_arg(ap, void *), 16, sizeof(void *) * 2, '0'); }
        else if (c == 'd' || c == 'i') { int64_t v = wide ? va_arg(ap, long long) : va_arg(ap, int); if (v < 0) { put(&w, '-'); width--; } number(&w, v < 0 ? 0 - (uint64_t)v : (uint64_t)v, 10, width, fill); }
        else if (c == 'u' || c == 'x') number(&w, wide ? va_arg(ap, unsigned long long) : va_arg(ap, unsigned), c == 'x' ? 16 : 10, width, fill);
        else if (c == 'f') { double v = va_arg(ap, double); if (v != v) { put(&w,'n'); put(&w,'a'); put(&w,'n'); } else if (v > 1e18 || v < -1e18) { if(v < 0) put(&w,'-'); put(&w,'i'); put(&w,'n'); put(&w,'f'); } else { if(v < 0) { put(&w,'-'); v = -v; } uint64_t i = (uint64_t)v; number(&w,i,10,0,' '); put(&w,'.'); number(&w,(unsigned)((v-i)*1000000),10,6,'0'); } }
        else { put(&w, '%'); put(&w, c); }
    }
    if (size) out[VKP_MIN(w.n, size - 1)] = 0;
}
void vkp_format(char *out, size_t size, const char *fmt, ...)
{
    va_list ap; va_start(ap, fmt); vkp_vformat(out, size, fmt, ap); va_end(ap);
}
/* Game-relative config/log paths also work when launched from another folder. */
static const char *game_path(const char *name, char *out, size_t size)
{
#ifdef BFV_VKP_TEST_SHIM
    (void)out; (void)size;
    return name;
#else
    if (!platform_init() || !api.GetModuleFileNameA(0, out, (DWORD)size)) return name;
    size_t n = vkp_strlen(out);
    while (n && out[n-1] != '\\' && out[n-1] != '/') n--;
    if (n + vkp_strlen(name) + 1 > size) return name;
    memcpy(out + n, name, vkp_strlen(name) + 1);
    return out;
#endif
}
void vkp_log(const char *fmt, ...)
{
    if (!platform_init()) return;
    char line[2048], path[MAX_PATH]; va_list ap;
    va_start(ap, fmt); vkp_vformat(line, sizeof(line)-2, fmt, ap); va_end(ap);
    size_t n = vkp_strlen(line); line[n++] = '\n'; line[n] = 0;
    /* BFV routes debugger-print exceptions through its own exception handler.
     * File logging avoids a fatal DBG_PRINTEXCEPTION_C before initialization. */
    HANDLE f = api.CreateFileA(game_path("vulkan.log",path,sizeof path), FILE_APPEND_DATA, FILE_SHARE_READ|FILE_SHARE_WRITE, 0, OPEN_ALWAYS, FILE_ATTRIBUTE_NORMAL, 0);
    if (f != INVALID_HANDLE_VALUE) { DWORD written; api.WriteFile(f,line,(DWORD)n,&written,0); api.CloseHandle(f); }
}
void *vkp_read_file(const char *path, size_t *size)
{
    if (!platform_init()) return 0;
    HANDLE f = api.CreateFileA(path,GENERIC_READ,FILE_SHARE_READ,0,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,0);
    if(f == INVALID_HANDLE_VALUE) return 0;
    LARGE_INTEGER length; void *data = 0; DWORD got;
    if(api.GetFileSizeEx(f,&length) && length.QuadPart >= 0 && (uint64_t)length.QuadPart < (uint64_t)0xffffffffu) {
        data=vkp_alloc((size_t)length.QuadPart + 1);
        if(data && (!api.ReadFile(f,data,(DWORD)length.QuadPart,&got,0) || got != length.QuadPart)) { vkp_free(data); data=0; }
        if(data && size) *size=(size_t)length.QuadPart;
    }
    api.CloseHandle(f); return data;
}
int vkp_write_file(const char *path,const void *data,size_t size)
{
    if(!platform_init() || size > 0xffffffffu) return 0;
    HANDLE f=api.CreateFileA(path,GENERIC_WRITE,FILE_SHARE_READ,0,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,0);
    if(f==INVALID_HANDLE_VALUE) return 0;
    DWORD written; int ok=api.WriteFile(f,data,(DWORD)size,&written,0) && written==size;
    api.CloseHandle(f); return ok;
}
static int parse_int(const char *p, int def)
{
    while (*p==' ' || *p=='\t' || *p=='"') p++;
    if((p[0]=='T' || p[0]=='t') && p[1]=='r' && p[2]=='u' && p[3]=='e') return 1;
    if((p[0]=='F' || p[0]=='f') && p[1]=='a' && p[2]=='l' && p[3]=='s' && p[4]=='e') return 0;
    int negative=0; if(*p=='-' || *p=='+') { negative=*p=='-'; p++; }
    if(*p<'0' || *p>'9') return def;
    unsigned n=0; while(*p>='0' && *p<='9') { if(n>214748364u) return def; n=n*10+*p++-'0'; if(n>2147483647u+(unsigned)negative) return def; }
    return negative ? (int)(0u-n) : (int)n;
}
int vkp_option_int(const char *key,int def)
{
    if(!platform_init()) return def;
    char env[64],path[MAX_PATH];
    DWORD count=api.GetEnvironmentVariableA(key,env,sizeof env);
    if(count && count<sizeof env) return parse_int(env,def);
    size_t size; char *data=vkp_read_file(game_path("dxvk.conf",path,sizeof path),&size);
    if(!data) return def;
    size_t len=vkp_strlen(key); int result=def;
    for(char *p=data; *p;) {
        char *next=p; while(*next && *next!='\n') next++;
        if(*next) *next++=0;
        while(*p==' ' || *p=='\t') p++;
        if(vkp_strlen(p)>=len && !memcmp(p,key,len)) { p+=len; while(*p==' ' || *p=='\t') p++; if(*p=='=') result=parse_int(p+1,def); }
        p=next;
    }
    vkp_free(data); return result;
}
uint64_t vkp_time_us(void)
{
    LARGE_INTEGER f,c; if(!platform_init() || !api.QueryPerformanceFrequency(&f) || !api.QueryPerformanceCounter(&c) || !f.QuadPart) return 0;
    return (uint64_t)((double)c.QuadPart * 1000000.0 / (double)f.QuadPart);
}
void vkp_sleep(uint32_t ms) { if(platform_init()) api.Sleep(ms); }
void *vkp_lock_new(void) { CRITICAL_SECTION *p=vkp_alloc(sizeof *p); if(p) api.InitializeCriticalSection(p); return p; }
void vkp_lock(void *p) { if(p && platform_init()) api.EnterCriticalSection(p); }
void vkp_unlock(void *p) { if(p && platform_init()) api.LeaveCriticalSection(p); }
void vkp_lock_free(void *p) { if(p && platform_init()) { api.DeleteCriticalSection(p); vkp_free(p); } }
