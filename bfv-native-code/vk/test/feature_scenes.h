static int target_scene(IDirect3DDevice9 *dev, D3DMULTISAMPLE_TYPE samples, uint32_t *pixels)
{
    IDirect3DSurface9 *saved_rt = 0, *saved_ds = 0, *rt = 0, *ds = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev,0,&saved_rt)));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetDepthStencilSurface(dev,&saved_ds)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(dev,64,64,D3DFMT_A8R8G8B8,samples,0,FALSE,&rt,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateDepthStencilSurface(dev,64,64,D3DFMT_D24S8,samples,0,TRUE,&ds,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetDepthStencilSurface(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderTarget(dev,0,rt)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetDepthStencilSurface(dev,ds)));
    CHECK(!geometry_scene(dev,0,0,pixels));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetDepthStencilSurface(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderTarget(dev,0,saved_rt)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetDepthStencilSurface(dev,saved_ds)));
    IDirect3DSurface9_Release(rt); IDirect3DSurface9_Release(ds);
    IDirect3DSurface9_Release(saved_rt); IDirect3DSurface9_Release(saved_ds);
    return 0;
}

static int copy_scene(IDirect3DDevice9 *dev, uint32_t *pixels)
{
    IDirect3DSurface9 *rt = 0, *source = 0, *gpu = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev,0,&rt)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev,64,64,D3DFMT_A8R8G8B8,D3DPOOL_SYSTEMMEM,&source,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(dev,64,64,D3DFMT_A8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&gpu,0)));
    D3DLOCKED_RECT lock;
    CHECK(SUCCEEDED(IDirect3DSurface9_LockRect(source,&lock,0,0)));
    for (UINT y=0;y<64;++y) for (UINT x=0;x<64;++x)
        ((DWORD *)((BYTE *)lock.pBits+y*lock.Pitch))[x]=0xff000000 | x*3<<16 | y*3<<8 | 0x57;
    CHECK(SUCCEEDED(IDirect3DSurface9_UnlockRect(source)));
    CHECK(SUCCEEDED(IDirect3DDevice9_UpdateSurface(dev,source,0,gpu,0)));
    RECT rect={16,16,48,48}; CHECK(SUCCEEDED(IDirect3DDevice9_ColorFill(dev,gpu,&rect,0xffac7532)));
    CHECK(SUCCEEDED(IDirect3DDevice9_StretchRect(dev,gpu,0,rt,0,D3DTEXF_NONE)));
    CHECK(!read_pixels(dev,pixels));
    IDirect3DSurface9_Release(source); IDirect3DSurface9_Release(gpu); IDirect3DSurface9_Release(rt);
    return 0;
}

static int render_texture_scene(IDirect3DDevice9 *dev, uint32_t *pixels)
{
    IDirect3DStateBlock9 *saved=0; IDirect3DSurface9 *previous=0,*surface=0; IDirect3DTexture9 *texture=0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateStateBlock(dev,D3DSBT_ALL,&saved)));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev,0,&previous)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateTexture(dev,64,64,1,D3DUSAGE_RENDERTARGET,
        D3DFMT_A8R8G8B8,D3DPOOL_DEFAULT,&texture,0)));
    CHECK(SUCCEEDED(IDirect3DTexture9_GetSurfaceLevel(texture,0,&surface)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderTarget(dev,0,surface)));
    CHECK(!geometry_scene(dev,0,6,pixels));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderTarget(dev,0,previous)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetFVF(dev,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_CULLMODE,D3DCULL_NONE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_STENCILENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_TEXTURE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAARG1,D3DTA_TEXTURE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,1,D3DTSS_COLOROP,D3DTOP_DISABLE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev,0,(IDirect3DBaseTexture9 *)texture)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MINFILTER,D3DTEXF_POINT)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MAGFILTER,D3DTEXF_POINT)));
    const struct scene_vertex quad[]={{-.5f,-.5f,.5f,1,0xffffffff,0,0},{63.5f,-.5f,.5f,1,0xffffffff,1,0},
        {-.5f,63.5f,.5f,1,0xffffffff,0,1},{63.5f,63.5f,.5f,1,0xffffffff,1,1}};
    CHECK(SUCCEEDED(IDirect3DDevice9_BeginScene(dev)));
    CHECK(SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLESTRIP,2,quad,sizeof quad[0])));
    CHECK(SUCCEEDED(IDirect3DDevice9_EndScene(dev))); CHECK(!read_pixels(dev,pixels));
    CHECK(SUCCEEDED(IDirect3DStateBlock9_Apply(saved))); IDirect3DStateBlock9_Release(saved);
    IDirect3DSurface9_Release(previous); IDirect3DSurface9_Release(surface); IDirect3DTexture9_Release(texture);
    return 0;
}

static int lighting_scene(IDirect3DDevice9 *dev, uint32_t *pixels)
{
    IDirect3DStateBlock9 *saved=0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateStateBlock(dev,D3DSBT_ALL,&saved)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetFVF(dev,D3DFVF_XYZ|D3DFVF_NORMAL)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev,0,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,1,D3DTSS_COLOROP,D3DTOP_DISABLE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,TRUE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_COLORVERTEX,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_CULLMODE,D3DCULL_NONE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHABLENDENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHATESTENABLE,FALSE)));
    D3DMATRIX identity={.m={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTransform(dev,D3DTS_WORLD,&identity)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTransform(dev,D3DTS_VIEW,&identity)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTransform(dev,D3DTS_PROJECTION,&identity)));
    D3DMATERIAL9 material={0}; material.Diffuse=(D3DCOLORVALUE){.75f,.5f,.25f,1};
    material.Ambient=(D3DCOLORVALUE){.1f,.1f,.1f,1};
    CHECK(SUCCEEDED(IDirect3DDevice9_SetMaterial(dev,&material)));
    D3DLIGHT9 light={0}; light.Type=D3DLIGHT_DIRECTIONAL; light.Diffuse=(D3DCOLORVALUE){1,1,1,1};
    light.Direction=(D3DVECTOR){0,0,1};
    CHECK(SUCCEEDED(IDirect3DDevice9_SetLight(dev,0,&light)));
    CHECK(SUCCEEDED(IDirect3DDevice9_LightEnable(dev,0,TRUE)));
    const float vertices[][6]={{-.75f,.75f,.5f,0,0,-1},{.75f,.75f,.5f,0,0,-1},
        {-.75f,-.75f,.5f,0,0,-1},{.75f,-.75f,.5f,0,0,-1}};
    CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev,0,0,D3DCLEAR_TARGET,0xff173b6d,1,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_BeginScene(dev)));
    CHECK(SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLESTRIP,2,vertices,sizeof vertices[0])));
    CHECK(SUCCEEDED(IDirect3DDevice9_EndScene(dev))); CHECK(!read_pixels(dev,pixels));
    CHECK(SUCCEEDED(IDirect3DStateBlock9_Apply(saved))); IDirect3DStateBlock9_Release(saved);
    return 0;
}

/* An X8R8G8B8 target blends with destination alpha 1, and BOTH(INV)SRCALPHA
 * also chooses the destination factor: one quad per blend mode. The alpha
 * byte of an X8 target is undefined, so it is not compared. */
static int blend_scene(IDirect3DDevice9 *dev, uint32_t *pixels)
{
    IDirect3DStateBlock9 *saved=0; IDirect3DSurface9 *previous=0,*rt=0,*cpu=0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateStateBlock(dev,D3DSBT_ALL,&saved)));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev,0,&previous)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(dev,64,64,D3DFMT_X8R8G8B8,D3DMULTISAMPLE_NONE,0,FALSE,&rt,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderTarget(dev,0,rt)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetFVF(dev,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev,0,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,1,D3DTSS_COLOROP,D3DTOP_DISABLE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_CULLMODE,D3DCULL_NONE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_STENCILENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHATESTENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHABLENDENABLE,TRUE)));
    /* the clear alpha is not stored in an X8 target */
    CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev,0,0,D3DCLEAR_TARGET,0x40c08020,1,0)));
    const DWORD modes[4][2]={{D3DBLEND_INVDESTALPHA,D3DBLEND_DESTALPHA},{D3DBLEND_DESTALPHA,D3DBLEND_INVDESTALPHA},
        {D3DBLEND_BOTHSRCALPHA,D3DBLEND_ZERO},{D3DBLEND_BOTHINVSRCALPHA,D3DBLEND_ONE}};
    CHECK(SUCCEEDED(IDirect3DDevice9_BeginScene(dev)));
    for (unsigned i=0;i<4;++i) {
        float x=(float)(i%2*32)-.5f, y=(float)(i/2*32)-.5f;
        const struct scene_vertex quad[]={{x,y,.5f,1,0x8020a0e0,0,0},{x+32,y,.5f,1,0x8020a0e0,1,0},
            {x,y+32,.5f,1,0x8020a0e0,0,1},{x+32,y+32,.5f,1,0x8020a0e0,1,1}};
        CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_SRCBLEND,modes[i][0])));
        CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_DESTBLEND,modes[i][1])));
        CHECK(SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLESTRIP,2,quad,sizeof quad[0])));
    }
    CHECK(SUCCEEDED(IDirect3DDevice9_EndScene(dev)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev,64,64,D3DFMT_X8R8G8B8,D3DPOOL_SYSTEMMEM,&cpu,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev,rt,cpu)));
    D3DLOCKED_RECT lock;
    CHECK(SUCCEEDED(IDirect3DSurface9_LockRect(cpu,&lock,0,D3DLOCK_READONLY)));
    for (UINT y=0;y<64;++y) for (UINT x=0;x<64;++x)
        pixels[y*64+x]=((const DWORD *)((const BYTE *)lock.pBits+y*lock.Pitch))[x]|0xff000000;
    CHECK(SUCCEEDED(IDirect3DSurface9_UnlockRect(cpu)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderTarget(dev,0,previous)));
    CHECK(SUCCEEDED(IDirect3DStateBlock9_Apply(saved))); IDirect3DStateBlock9_Release(saved);
    IDirect3DSurface9_Release(cpu); IDirect3DSurface9_Release(rt); IDirect3DSurface9_Release(previous);
    return 0;
}

/* Sixteen draws in one pass, each after a different state change, against the
 * command buffer state and constant blocks the device keeps between draws:
 * world matrix only, texture only, texture factor, stage constant, XYZRHW
 * (two constant registers) and XYZ (twenty), lighting and material, vertex
 * shaders reading different constant ranges with unchanged constants, a world
 * matrix set while shaders are bound and used after returning to fixed
 * function, blending, fog and the viewport. One 16x16 cell per draw. */
static void cell_world(D3DMATRIX *m, unsigned cell)
{
    /* 0.3 pixel off the cell centre: no pixel centre on a texel boundary or an edge */
    memset(m, 0, sizeof *m);
    m->m[0][0] = m->m[1][1] = m->m[2][2] = m->m[3][3] = 1;
    m->m[3][0] = -.75f + .5f * (float)(cell % 4) + .3f / 32; m->m[3][1] = .75f - .5f * (float)(cell / 4) - .3f / 32;
}
static int churn_scene(IDirect3DDevice9 *dev, uint32_t *pixels)
{
    struct ff_vertex { float x, y, z; DWORD color; float u, v; };
    struct lit_vertex { float x, y, z, nx, ny, nz; };
    const struct ff_vertex quad[] = {{-.2f,.2f,.5f,0x80ffffff,0,0},{.2f,.2f,.5f,0x80ffffff,1,0},
        {-.2f,-.2f,.5f,0x80ffffff,0,1},{.2f,-.2f,.5f,0x80ffffff,1,1}};
    const struct lit_vertex lit[] = {{-.2f,.2f,.5f,0,0,-1},{.2f,.2f,.5f,0,0,-1},{-.2f,-.2f,.5f,0,0,-1},{.2f,-.2f,.5f,0,0,-1}};
    const D3DVERTEXELEMENT9 elements[] = {{0,0,D3DDECLTYPE_FLOAT3,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_POSITION,0},
        {0,12,D3DDECLTYPE_D3DCOLOR,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_COLOR,0},
        {0,16,D3DDECLTYPE_FLOAT2,D3DDECLMETHOD_DEFAULT,D3DDECLUSAGE_TEXCOORD,0}, D3DDECL_END()};
    const char *vs_source[2] = {"vs.1.1\ndcl_position v0\nm4x4 oPos, v0, c4\nmov oD0, c0\n",
                                "vs.1.1\ndcl_position v0\nm4x4 oPos, v0, c8\nmov oD0, c0\n"};
    IDirect3DStateBlock9 *saved=0; IDirect3DVertexDeclaration9 *decl=0; IDirect3DTexture9 *tex[2]={0};
    IDirect3DVertexShader9 *vs[2]={0}; IDirect3DPixelShader9 *ps=0; ID3DXBuffer *code=0;
    D3DCAPS9 caps; D3DMATRIX world, identity; D3DLOCKED_RECT lock;
    cell_world(&identity, 0); identity.m[3][0] = identity.m[3][1] = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_GetDeviceCaps(dev,&caps)));
    char path[MAX_PATH]; CHECK(GetSystemDirectoryA(path, MAX_PATH) < MAX_PATH - 14);
    strcat(path, "\\d3dx9_43.dll"); HMODULE dll = LoadLibraryA(path); CHECK(dll);
    HRESULT (WINAPI *assemble)(const char *, UINT, const D3DXMACRO *, ID3DXInclude *, DWORD,
        ID3DXBuffer **, ID3DXBuffer **) = (void *)GetProcAddress(dll, "D3DXAssembleShader");
    CHECK(assemble);
    for (unsigned i=0;i<2;++i) {
        CHECK(SUCCEEDED(assemble(vs_source[i],(UINT)strlen(vs_source[i]),0,0,0,&code,0)));
        CHECK(SUCCEEDED(IDirect3DDevice9_CreateVertexShader(dev,ID3DXBuffer_GetBufferPointer(code),&vs[i])));
        ID3DXBuffer_Release(code); code=0;
    }
    const char *ps_source = "ps.1.1\nmov r0, v0\n";
    CHECK(SUCCEEDED(assemble(ps_source,(UINT)strlen(ps_source),0,0,0,&code,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreatePixelShader(dev,ID3DXBuffer_GetBufferPointer(code),&ps)));
    ID3DXBuffer_Release(code); FreeLibrary(dll);
    for (unsigned i=0;i<2;++i) {
        CHECK(SUCCEEDED(IDirect3DDevice9_CreateTexture(dev,2,2,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex[i],0)));
        CHECK(SUCCEEDED(IDirect3DTexture9_LockRect(tex[i],0,&lock,0,0)));
        const DWORD texels[2][4]={{0xffff8040,0xffc06020,0xffe0a080,0xff804020},{0xff4080ff,0xff2060c0,0xff80a0e0,0xff204080}};
        memcpy(lock.pBits,texels[i],8); memcpy((BYTE *)lock.pBits+lock.Pitch,texels[i]+2,8);
        CHECK(SUCCEEDED(IDirect3DTexture9_UnlockRect(tex[i],0)));
    }
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateStateBlock(dev,D3DSBT_ALL,&saved)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateVertexDeclaration(dev,elements,&decl)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexDeclaration(dev,decl)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_CULLMODE,D3DCULL_NONE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_STENCILENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHATESTENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHABLENDENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTransform(dev,D3DTS_VIEW,&identity)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTransform(dev,D3DTS_PROJECTION,&identity)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_MODULATE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_TEXTURE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAARG1,D3DTA_DIFFUSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,1,D3DTSS_COLOROP,D3DTOP_DISABLE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MINFILTER,D3DTEXF_POINT)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetSamplerState(dev,0,D3DSAMP_MAGFILTER,D3DTEXF_POINT)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev,0,(IDirect3DBaseTexture9 *)tex[0])));
    CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev,0,0,D3DCLEAR_TARGET,0xff173b6d,1,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_BeginScene(dev)));
#define CHURN_DRAW() CHECK(SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLESTRIP,2,quad,sizeof quad[0])))
#define CHURN_WORLD(cell) do { cell_world(&world,cell); CHECK(SUCCEEDED(IDirect3DDevice9_SetTransform(dev,D3DTS_WORLD,&world))); } while (0)
    CHURN_WORLD(0); CHURN_DRAW();
    CHURN_WORLD(1); CHURN_DRAW();                                   /* world only */
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev,0,(IDirect3DBaseTexture9 *)tex[1])));
    CHURN_WORLD(2); CHURN_DRAW();                                   /* texture */
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG2,D3DTA_TFACTOR)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_TEXTUREFACTOR,0xff40c080)));
    CHURN_WORLD(3); CHURN_DRAW();
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_TEXTUREFACTOR,0xffc04020)));
    CHURN_WORLD(4); CHURN_DRAW();                                   /* texture factor */
    if (caps.PrimitiveMiscCaps & D3DPMISCCAPS_PERSTAGECONSTANT) {
        CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG2,D3DTA_CONSTANT)));
        CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_CONSTANT,0xff2080ff)));
        CHURN_WORLD(5); CHURN_DRAW();                               /* stage constant */
    }
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG2,D3DTA_DIFFUSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetFVF(dev,D3DFVF_XYZRHW|D3DFVF_DIFFUSE|D3DFVF_TEX1)));
    const struct scene_vertex rhw[] = {{34.3f,18.3f,.5f,1,0xffffffff,0,0},{45.3f,18.3f,.5f,1,0xffffffff,1,0},
        {34.3f,29.3f,.5f,1,0xffffffff,0,1},{45.3f,29.3f,.5f,1,0xffffffff,1,1}};
    CHECK(SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLESTRIP,2,rhw,sizeof rhw[0])));  /* cell 6: XYZRHW */
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexDeclaration(dev,decl)));
    CHURN_WORLD(7); CHURN_DRAW();                                   /* XYZ again */
    D3DMATERIAL9 material={0}; material.Diffuse=(D3DCOLORVALUE){.75f,.5f,.25f,1}; material.Ambient=(D3DCOLORVALUE){.1f,.1f,.1f,1};
    D3DLIGHT9 light={0}; light.Type=D3DLIGHT_DIRECTIONAL; light.Diffuse=(D3DCOLORVALUE){1,1,1,1}; light.Direction=(D3DVECTOR){0,0,1};
    CHECK(SUCCEEDED(IDirect3DDevice9_SetFVF(dev,D3DFVF_XYZ|D3DFVF_NORMAL)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,TRUE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_COLORVERTEX,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetMaterial(dev,&material)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetLight(dev,0,&light)));
    CHECK(SUCCEEDED(IDirect3DDevice9_LightEnable(dev,0,TRUE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_SELECTARG1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_DIFFUSE)));
    CHURN_WORLD(8);
    CHECK(SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLESTRIP,2,lit,sizeof lit[0])));
    material.Diffuse=(D3DCOLORVALUE){.25f,.75f,.5f,1};
    CHECK(SUCCEEDED(IDirect3DDevice9_SetMaterial(dev,&material)));
    CHURN_WORLD(11);
    CHECK(SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(dev,D3DPT_TRIANGLESTRIP,2,lit,sizeof lit[0])));  /* material */
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_MODULATE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLORARG1,D3DTA_TEXTURE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexDeclaration(dev,decl)));
    float vsc[12][4]={{.9f,.6f,.3f,1}};
    D3DMATRIX m9, m10; cell_world(&m9,9); cell_world(&m10,10);
    for (unsigned r=0;r<4;++r) for (unsigned c=0;c<4;++c) { vsc[4+r][c]=m9.m[c][r]; vsc[8+r][c]=m10.m[c][r]; }
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShaderConstantF(dev,0,vsc[0],12)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShader(dev,vs[0])));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev,ps)));
    CHURN_DRAW();                                                   /* cell 9: reads c0..c7 */
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShader(dev,vs[1])));
    CHURN_DRAW();                                                   /* cell 10: c0..c11, constants unchanged */
    CHURN_WORLD(12); CHURN_DRAW();                                  /* world set while shaders are bound */
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev,0)));
    CHURN_DRAW();                                                   /* cell 12: fixed function again */
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGENABLE,TRUE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGTABLEMODE,D3DFOG_LINEAR)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGCOLOR,0xff804020)));
    float fog[2] = {0, 1}; DWORD bits;
    memcpy(&bits,&fog[0],4); CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGSTART,bits)));
    memcpy(&bits,&fog[1],4); CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGEND,bits)));
    CHURN_WORLD(13); CHURN_DRAW();                                  /* fog: push constants */
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_FOGENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHABLENDENABLE,TRUE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_SRCBLEND,D3DBLEND_SRCALPHA)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_DESTBLEND,D3DBLEND_INVSRCALPHA)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_ALPHAOP,D3DTOP_SELECTARG1)));
    CHURN_WORLD(14); CHURN_DRAW();                                  /* blending */
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ALPHABLENDENABLE,FALSE)));
    D3DVIEWPORT9 vp={48,48,16,16,0,1};
    CHECK(SUCCEEDED(IDirect3DDevice9_SetViewport(dev,&vp)));
    world=identity; world.m[3][0]=.3f/8; world.m[3][1]=-.3f/8;     /* 0.3 pixel in the 16 pixel viewport */
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTransform(dev,D3DTS_WORLD,&world)));
    CHURN_DRAW();                                                   /* cell 15: viewport */
#undef CHURN_DRAW
#undef CHURN_WORLD
    CHECK(SUCCEEDED(IDirect3DDevice9_EndScene(dev)));
    CHECK(!read_pixels(dev,pixels));
    CHECK(SUCCEEDED(IDirect3DStateBlock9_Apply(saved))); IDirect3DStateBlock9_Release(saved);
    IDirect3DVertexDeclaration9_Release(decl); IDirect3DPixelShader9_Release(ps);
    IDirect3DVertexShader9_Release(vs[0]); IDirect3DVertexShader9_Release(vs[1]);
    IDirect3DTexture9_Release(tex[0]); IDirect3DTexture9_Release(tex[1]);
    return 0;
}

/* Per-draw CPU cost (opt-in, BFV_VK_BENCH): the fixed-function loop of a game
 * frame, with a texture and a world matrix change per indexed draw, then a
 * Present. Prints microseconds per draw; there is no pass/fail limit. */
static int bench_scene(IDirect3DDevice9 *dev, const char *name)
{
    enum { FRAMES = 30, DRAWS = 2000 };
    IDirect3DStateBlock9 *saved=0; IDirect3DVertexBuffer9 *vb=0; IDirect3DIndexBuffer9 *ib=0;
    IDirect3DTexture9 *tex[2]={0}; void *p;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateStateBlock(dev,D3DSBT_ALL,&saved)));
    const float verts[][6]={{-.1f,.1f,.5f,0,0,0},{.1f,.1f,.5f,0,1,0},{-.1f,-.1f,.5f,0,0,1},{.1f,-.1f,.5f,0,1,1}};
    const WORD indices[]={0,1,2,2,1,3};
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(dev,sizeof verts,0,0,D3DPOOL_MANAGED,&vb,0)));
    CHECK(SUCCEEDED(IDirect3DVertexBuffer9_Lock(vb,0,0,&p,0))); memcpy(p,verts,sizeof verts);
    CHECK(SUCCEEDED(IDirect3DVertexBuffer9_Unlock(vb)));
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateIndexBuffer(dev,sizeof indices,0,D3DFMT_INDEX16,D3DPOOL_MANAGED,&ib,0)));
    CHECK(SUCCEEDED(IDirect3DIndexBuffer9_Lock(ib,0,0,&p,0))); memcpy(p,indices,sizeof indices);
    CHECK(SUCCEEDED(IDirect3DIndexBuffer9_Unlock(ib)));
    for (unsigned i=0;i<2;++i) {
        D3DLOCKED_RECT lock;
        CHECK(SUCCEEDED(IDirect3DDevice9_CreateTexture(dev,4,4,1,0,D3DFMT_A8R8G8B8,D3DPOOL_MANAGED,&tex[i],0)));
        CHECK(SUCCEEDED(IDirect3DTexture9_LockRect(tex[i],0,&lock,0,0)));
        for (unsigned y=0;y<4;++y) memset((BYTE *)lock.pBits+y*lock.Pitch,i?0xc0:0x40,16);
        CHECK(SUCCEEDED(IDirect3DTexture9_UnlockRect(tex[i],0)));
    }
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetFVF(dev,D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetStreamSource(dev,0,vb,0,sizeof verts[0])));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetIndices(dev,ib)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_LIGHTING,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_CULLMODE,D3DCULL_NONE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev,D3DRS_ZENABLE,FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,0,D3DTSS_COLOROP,D3DTOP_MODULATE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev,1,D3DTSS_COLOROP,D3DTOP_DISABLE)));
    D3DMATRIX world={.m={{1,0,0,0},{0,1,0,0},{0,0,1,0},{0,0,0,1}}};
    LARGE_INTEGER f,t0,t1; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t0);
    for (unsigned frame=0;frame<FRAMES;++frame) {
        CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev,0,0,D3DCLEAR_TARGET,0xff173b6d,1,0)));
        CHECK(SUCCEEDED(IDirect3DDevice9_BeginScene(dev)));
        for (unsigned i=0;i<DRAWS;++i) {
            world.m[3][0]=(float)(i%9)*.2f-.8f; world.m[3][1]=(float)(i/9%9)*.2f-.8f;
            CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev,0,(IDirect3DBaseTexture9 *)tex[i&1])));
            CHECK(SUCCEEDED(IDirect3DDevice9_SetTransform(dev,D3DTS_WORLD,&world)));
            CHECK(SUCCEEDED(IDirect3DDevice9_DrawIndexedPrimitive(dev,D3DPT_TRIANGLELIST,0,0,4,0,2)));
        }
        CHECK(SUCCEEDED(IDirect3DDevice9_EndScene(dev)));
        CHECK(SUCCEEDED(IDirect3DDevice9_Present(dev,0,0,0,0)));
    }
    QueryPerformanceCounter(&t1);
    double ms=(double)(t1.QuadPart-t0.QuadPart)*1000.0/(double)f.QuadPart;
    printf("BENCH %s: %u frames x %u draws in %.1f ms = %.2f us/draw, %.2f ms/frame\n",name,(unsigned)FRAMES,
        (unsigned)DRAWS,ms,ms*1000.0/(FRAMES*DRAWS),ms/FRAMES);
    CHECK(SUCCEEDED(IDirect3DStateBlock9_Apply(saved))); IDirect3DStateBlock9_Release(saved);
    IDirect3DTexture9_Release(tex[0]); IDirect3DTexture9_Release(tex[1]);
    IDirect3DVertexBuffer9_Release(vb); IDirect3DIndexBuffer9_Release(ib);
    return 0;
}

static int swapchain_lifetime_scene(IDirect3DDevice9 *dev, HWND hwnd)
{
    D3DPRESENT_PARAMETERS pp={0}; pp.BackBufferWidth=64; pp.BackBufferHeight=64;
    pp.BackBufferFormat=D3DFMT_A8R8G8B8; pp.BackBufferCount=1;
    pp.SwapEffect=D3DSWAPEFFECT_COPY; pp.Windowed=TRUE; pp.hDeviceWindow=hwnd;
    IDirect3DSwapChain9 *sc=0; IDirect3DSurface9 *bb=0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateAdditionalSwapChain(dev,&pp,&sc)));
    CHECK(SUCCEEDED(IDirect3DSwapChain9_GetBackBuffer(sc,0,D3DBACKBUFFER_TYPE_MONO,&bb)));
    IDirect3DSwapChain9_Release(sc);
    D3DSURFACE_DESC desc; CHECK(SUCCEEDED(IDirect3DSurface9_GetDesc(bb,&desc)));
    CHECK(desc.Width==64&&desc.Height==64);
    CHECK(SUCCEEDED(IDirect3DDevice9_ColorFill(dev,bb,0,0xff58297a)));
    IDirect3DSurface9_Release(bb);
    return 0;
}

static int fullscreen_scene(IDirect3DDevice9 *dev, IDirect3D9 *api, HWND hwnd)
{
    D3DDISPLAYMODE mode; CHECK(SUCCEEDED(IDirect3D9_GetAdapterDisplayMode(api,0,&mode)));
    D3DPRESENT_PARAMETERS pp={0}; pp.BackBufferWidth=mode.Width; pp.BackBufferHeight=mode.Height;
    pp.BackBufferFormat=mode.Format; pp.BackBufferCount=1; pp.SwapEffect=D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow=hwnd; pp.PresentationInterval=D3DPRESENT_INTERVAL_IMMEDIATE;
    CHECK(SUCCEEDED(IDirect3DDevice9_Reset(dev,&pp)));
    CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev,0,0,D3DCLEAR_TARGET,0xff173b6d,1,0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_Present(dev,0,0,0,0)));
    SendMessageA(hwnd,WM_ACTIVATEAPP,FALSE,0);
    CHECK(IDirect3DDevice9_TestCooperativeLevel(dev)==D3DERR_DEVICELOST);
    CHECK(IDirect3DDevice9_Present(dev,0,0,0,0)==D3DERR_DEVICELOST);
    SendMessageA(hwnd,WM_ACTIVATEAPP,TRUE,0);
    CHECK(IDirect3DDevice9_TestCooperativeLevel(dev)==D3DERR_DEVICENOTRESET);
    CHECK(SUCCEEDED(IDirect3DDevice9_Reset(dev,&pp)));
    CHECK(SUCCEEDED(IDirect3DDevice9_TestCooperativeLevel(dev)));
    pp.Windowed=TRUE; pp.BackBufferWidth=64; pp.BackBufferHeight=64; pp.BackBufferFormat=D3DFMT_A8R8G8B8;
    pp.EnableAutoDepthStencil=TRUE; pp.AutoDepthStencilFormat=D3DFMT_D24S8;
    CHECK(SUCCEEDED(IDirect3DDevice9_Reset(dev,&pp)));
    printf("PASS fullscreen %ux%u: focus-loss DEVICELOST/reactivation NOTRESET/recovery/windowed Reset\n",mode.Width,mode.Height);
    return 0;
}
