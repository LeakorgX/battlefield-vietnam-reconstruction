/* Build with mingw32 gcc; run <cinematic.hlsl> <cinematic_shader.h>.
 * Uses the installed Microsoft shader compiler, only at build time. */
#define COBJMACROS
#include <windows.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>
int main(int argc, char **argv)
{
    if (argc != 3) return 2;
    HMODULE dll = LoadLibraryA("d3dcompiler_47.dll");
    if (!dll) return 3;
    pD3DCompile compile = (pD3DCompile)GetProcAddress(dll, "D3DCompile");
    if (!compile) return 3;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 4;
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    char *s = malloc(n + 1);
    if (fread(s, 1, n, f) != (size_t)n) return 4;
    fclose(f); s[n] = 0;
    ID3DBlob *code = NULL, *err = NULL;
    HRESULT hr = compile(s, n, argv[1], NULL, NULL, "main", "ps_2_0",
                         D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &err);
    if (err) fprintf(stderr, "%s", (char *)ID3D10Blob_GetBufferPointer(err));
    if (FAILED(hr)) return 5;
    f = fopen(argv[2], "wb"); if (!f) return 4;
    fprintf(f, "/* Own cinematic.hlsl compiled as ps_2_0; regenerate with tools/compile_cinematic_shader.c. */\nstatic const uint32_t cinematic_shader_code[] = {\n");
    DWORD *p = ID3D10Blob_GetBufferPointer(code);
    size_t bytes = ID3D10Blob_GetBufferSize(code);
    for (size_t i = 0; i < bytes / 4; i++)
        fprintf(f, "0x%08lxu,%s", p[i], i % 6 == 5 ? "\n" : " ");
    fprintf(f, "\n};\n"); fclose(f);
    printf("ps_2_0: %zu bytes\n", bytes);
    return 0;
}
