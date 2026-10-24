/* Compile one authored HLSL entry point into a uint32_t header.
 * Usage: compile_shader input.hlsl entry profile output.h symbol
 */
#define COBJMACROS
#include <windows.h>
#include <d3dcompiler.h>
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
    if (argc != 6) return 2;
    HMODULE dll = LoadLibraryA("d3dcompiler_47.dll");
    if (!dll) return 3;
    pD3DCompile compile = (pD3DCompile)GetProcAddress(dll, "D3DCompile");
    if (!compile) return 3;
    FILE *f = fopen(argv[1], "rb");
    if (!f) return 4;
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    char *source = malloc((size_t)n + 1);
    if (!source || fread(source, 1, (size_t)n, f) != (size_t)n) return 4;
    fclose(f); source[n] = 0;
    ID3DBlob *code = NULL, *errors = NULL;
    HRESULT hr = compile(source, (size_t)n, argv[1], NULL, NULL, argv[2], argv[3],
                         D3DCOMPILE_OPTIMIZATION_LEVEL3, 0, &code, &errors);
    if (errors) fprintf(stderr, "%s", (char *)ID3D10Blob_GetBufferPointer(errors));
    if (FAILED(hr)) return 5;
    f = fopen(argv[4], "wb"); if (!f) return 4;
    fprintf(f, "/* Authored %s:%s, compiled as %s. */\nstatic const uint32_t %s[] = {\n",
            argv[1], argv[2], argv[3], argv[5]);
    DWORD *words = ID3D10Blob_GetBufferPointer(code);
    size_t bytes = ID3D10Blob_GetBufferSize(code);
    for (size_t i = 0; i < bytes / 4; i++)
        fprintf(f, "0x%08lxu,%s", words[i], i % 6 == 5 ? "\n" : " ");
    fprintf(f, "\n};\n"); fclose(f);
    printf("%s %s: %zu bytes\n", argv[2], argv[3], bytes);
    return 0;
}
