static int shader_scene(IDirect3DDevice9 *dev, unsigned version, uint32_t *pixels)
{
    IDirect3DStateBlock9 *saved = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &saved)));
    char path[MAX_PATH]; CHECK(GetSystemDirectoryA(path, MAX_PATH) < MAX_PATH - 14);
    strcat(path, "\\d3dx9_43.dll"); HMODULE dll = LoadLibraryA(path); CHECK(dll);
    HRESULT (WINAPI *assemble)(const char *, UINT, const D3DXMACRO *, ID3DXInclude *, DWORD,
        ID3DXBuffer **, ID3DXBuffer **) = (void *)GetProcAddress(dll, "D3DXAssembleShader");
    HRESULT (WINAPI *effect_create)(IDirect3DDevice9 *, const void *, UINT, const D3DXMACRO *,
        ID3DXInclude *, DWORD, ID3DXEffectPool *, ID3DXEffect **, ID3DXBuffer **) =
        (void *)GetProcAddress(dll, "D3DXCreateEffect");
    CHECK(assemble && effect_create);
    const char *vs[] = {
        "vs.1.1\ndcl_position v0\ndcl_color v1\nmov oPos,v0\nmov oD0,v1\n",
        "vs.2.0\ndcl_position v0\ndcl_color v1\nmov oPos,v0\nmov oD0,v1\n",
        "vs.3.0\ndcl_position v0\ndcl_color v1\ndcl_position o0\ndcl_color o1\nmov o0,v0\nmov o1,v1\n"};
    const char *ps[] = {"ps.1.1\nmov r0,v0\n", "ps.2.0\ndcl v0\nmov oC0,v0\n",
        "ps.3.0\ndcl_color v0\nmov oC0,v0\n"};
    const D3DVERTEXELEMENT9 elements[] = {
        {0,0,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,16,D3DDECLTYPE_FLOAT4,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0}, D3DDECL_END()};
    IDirect3DVertexDeclaration9 *declaration = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(dev, elements, &declaration)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexDeclaration(dev, declaration)));
    IDirect3DVertexShader9 *vertex = 0; IDirect3DPixelShader9 *pixel = 0;
    ID3DXEffect *effect = 0; ID3DXBuffer *code = 0, *errors = 0;
    if (version < 3) {
        HRESULT hr = assemble(vs[version], (UINT)strlen(vs[version]), 0, 0, 0, &code, &errors);
        if (FAILED(hr) && errors) fprintf(stderr, "%s\n", (char *)ID3DXBuffer_GetBufferPointer(errors));
        CHECK(SUCCEEDED(hr));
        CHECK(SUCCEEDED(IDirect3DDevice9_CreateVertexShader(dev, ID3DXBuffer_GetBufferPointer(code), &vertex)));
        ID3DXBuffer_Release(code); code = 0;
        hr = assemble(ps[version], (UINT)strlen(ps[version]), 0, 0, 0, &code, &errors);
        if (FAILED(hr) && errors) fprintf(stderr, "%s\n", (char *)ID3DXBuffer_GetBufferPointer(errors));
        CHECK(SUCCEEDED(hr));
        CHECK(SUCCEEDED(IDirect3DDevice9_CreatePixelShader(dev, ID3DXBuffer_GetBufferPointer(code), &pixel)));
        ID3DXBuffer_Release(code); code = 0;
        CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShader(dev, vertex)));
        CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev, pixel)));
    } else {
        const char source[] = "struct V{float4 p:POSITION;float4 c:COLOR;}; V vs(V a){return a;}"
            "float4 ps(V a):COLOR{return a.c;} technique T {pass P {"
            "VertexShader=compile vs_2_0 vs();PixelShader=compile ps_2_0 ps();}}";
        HRESULT hr = effect_create(dev, source, sizeof source - 1, 0, 0, 0, 0, &effect, &errors);
        if (FAILED(hr) && errors) fprintf(stderr, "%s\n", (char *)ID3DXBuffer_GetBufferPointer(errors));
        CHECK(SUCCEEDED(hr)); CHECK(SUCCEEDED(effect->lpVtbl->SetTechnique(effect, "T")));
    }
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, FALSE)));
    const float verts[][8] = {{-.75f,.75f,.5f,1,.25f,.5f,.75f,1},{.75f,.75f,.5f,1,.25f,.5f,.75f,1},
        {-.75f,-.75f,.5f,1,.25f,.5f,.75f,1},{.75f,-.75f,.5f,1,.25f,.5f,.75f,1}};
    CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev, 0, 0, D3DCLEAR_TARGET, 0xff173b6d, 1, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_BeginScene(dev)));
    if (effect) { UINT passes; CHECK(SUCCEEDED(effect->lpVtbl->Begin(effect, &passes, 0)));
        CHECK(passes == 1); CHECK(SUCCEEDED(effect->lpVtbl->BeginPass(effect, 0))); }
    CHECK(SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLESTRIP,2,verts,sizeof verts[0])));
    if (effect) { CHECK(SUCCEEDED(effect->lpVtbl->EndPass(effect))); CHECK(SUCCEEDED(effect->lpVtbl->End(effect))); }
    CHECK(SUCCEEDED(IDirect3DDevice9_EndScene(dev)));
    CHECK(!read_pixels(dev,pixels)); CHECK(SUCCEEDED(IDirect3DStateBlock9_Apply(saved)));
    IDirect3DStateBlock9_Release(saved); IDirect3DVertexDeclaration9_Release(declaration);
    if (vertex) IDirect3DVertexShader9_Release(vertex);
    if (pixel) IDirect3DPixelShader9_Release(pixel);
    if (effect) effect->lpVtbl->Release(effect);
    if (errors) ID3DXBuffer_Release(errors);
    FreeLibrary(dll); return 0;
}
