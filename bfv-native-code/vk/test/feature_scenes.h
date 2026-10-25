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
