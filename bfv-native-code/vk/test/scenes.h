/* Public D3D9 calls only: the identical scene runs against both backends. */
struct scene_vertex { float x, y, z, rhw; DWORD color; float u, v; };

static int read_pixels(IDirect3DDevice9 *dev, uint32_t *pixels)
{
    IDirect3DSurface9 *rt = 0, *cpu = 0, *resolved = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_GetRenderTarget(dev, 0, &rt)));
    D3DSURFACE_DESC description;
    CHECK(SUCCEEDED(IDirect3DSurface9_GetDesc(rt, &description)));
    if (description.MultiSampleType != D3DMULTISAMPLE_NONE) {
        CHECK(SUCCEEDED(IDirect3DDevice9_CreateRenderTarget(dev, 64, 64, D3DFMT_A8R8G8B8,
            D3DMULTISAMPLE_NONE, 0, FALSE, &resolved, 0)));
        CHECK(SUCCEEDED(IDirect3DDevice9_StretchRect(dev, rt, 0, resolved, 0, D3DTEXF_NONE)));
    }
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateOffscreenPlainSurface(dev, 64, 64,
        D3DFMT_A8R8G8B8, D3DPOOL_SYSTEMMEM, &cpu, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_GetRenderTargetData(dev, resolved ? resolved : rt, cpu)));
    D3DLOCKED_RECT lock;
    CHECK(SUCCEEDED(IDirect3DSurface9_LockRect(cpu, &lock, 0, D3DLOCK_READONLY)));
    for (UINT y = 0; y < 64; ++y) memcpy(pixels + y * 64,
        (const BYTE *)lock.pBits + y * lock.Pitch, 256);
    CHECK(SUCCEEDED(IDirect3DSurface9_UnlockRect(cpu)));
    IDirect3DSurface9_Release(cpu); IDirect3DSurface9_Release(rt);
    if (resolved) IDirect3DSurface9_Release(resolved);
    return 0;
}

static int compare_pixels(const char *name, const uint32_t *a, const uint32_t *b, unsigned tolerance)
{
    unsigned bad = 0, worst = 0, first = 0;
    for (unsigned i = 0; i < 4096; ++i) {
        unsigned error = 0;
        for (unsigned c = 0; c < 4; ++c) {
            int delta = (int)((a[i] >> (c * 8)) & 255) - (int)((b[i] >> (c * 8)) & 255);
            unsigned magnitude = delta < 0 ? (unsigned)-delta : (unsigned)delta;
            if (magnitude > error) error = magnitude;
        }
        if (error > worst) worst = error;
        if (error > tolerance) { if (!bad) first = i; ++bad; }
    }
    if (bad) {
        fprintf(stderr, "FAIL %s: %u pixels, max channel error %u, first (%u,%u) Vulkan=%08x D3D9=%08x\n",
            name, bad, worst, first % 64, first / 64, a[first], b[first]);
        return 1;
    }
    printf("PASS %s: 4096 pixels, max channel error %u (limit %u)\n", name, worst, tolerance);
    return 0;
}

static int lifetime_scene(IDirect3DDevice9 *dev)
{
    IDirect3DTexture9 *texture = 0;
    IDirect3DBaseTexture9 *recovered = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateTexture(dev, 4, 4, 1, 0,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)texture)));
    IDirect3DTexture9_Release(texture);
    CHECK(SUCCEEDED(IDirect3DDevice9_GetTexture(dev, 0, &recovered)));
    CHECK(recovered == (IDirect3DBaseTexture9 *)texture);
    D3DSURFACE_DESC desc;
    CHECK(SUCCEEDED(IDirect3DTexture9_GetLevelDesc((IDirect3DTexture9 *)recovered, 0, &desc)));
    CHECK(desc.Width == 4 && desc.Height == 4);
    IDirect3DBaseTexture9_Release(recovered);
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev, 0, 0)));
    /* Bound resources must not keep an external device reference alive. */
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateTexture(dev, 4, 4, 1, 0,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)texture)));
    IDirect3DTexture9_Release(texture);
    const DWORD ps[] = {0xffff0200, 0x02000001, 0x800f0800, 0xa0e40000, 0x0000ffff};
    IDirect3DPixelShader9 *shader = 0, *recovered_shader = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreatePixelShader(dev, ps, &shader)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev, shader)));
    IDirect3DPixelShader9_Release(shader);
    CHECK(SUCCEEDED(IDirect3DDevice9_GetPixelShader(dev, &recovered_shader)));
    CHECK(recovered_shader == shader);
    IDirect3DPixelShader9_Release(recovered_shader);
    return 0;
}

static int geometry_scene(IDirect3DDevice9 *dev, unsigned method, unsigned mode, uint32_t *pixels)
{
    IDirect3DStateBlock9 *saved = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateStateBlock(dev, D3DSBT_ALL, &saved)));
    IDirect3DTexture9 *texture = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_CreateTexture(dev, 2, 2, 1, 0,
        D3DFMT_A8R8G8B8, D3DPOOL_MANAGED, &texture, 0)));
    D3DLOCKED_RECT lock;
    CHECK(SUCCEEDED(IDirect3DTexture9_LockRect(texture, 0, &lock, 0, 0)));
    const DWORD texels[] = {0xff204080, 0xffc04020, 0xff40b060, 0xffa0c0e0};
    memcpy(lock.pBits, texels, 8); memcpy((BYTE *)lock.pBits + lock.Pitch, texels + 2, 8);
    CHECK(SUCCEEDED(IDirect3DTexture9_UnlockRect(texture, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetVertexShader(dev, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetPixelShader(dev, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetFVF(dev, D3DFVF_XYZRHW | D3DFVF_DIFFUSE | D3DFVF_TEX1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTexture(dev, 0, (IDirect3DBaseTexture9 *)texture)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_LIGHTING, FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_CULLMODE, D3DCULL_NONE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, FALSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGENABLE, mode == 5)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGTABLEMODE, D3DFOG_LINEAR)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGCOLOR, 0xff804020)));
    float fog_start = 0, fog_end = 1; DWORD fog_bits;
    memcpy(&fog_bits, &fog_start, 4); CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGSTART, fog_bits)));
    memcpy(&fog_bits, &fog_end, 4); CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_FOGEND, fog_bits)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MINFILTER, D3DTEXF_POINT)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetSamplerState(dev, 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLOROP,
        mode == 1 ? D3DTOP_ADD : mode == 2 ? D3DTOP_SUBTRACT : D3DTOP_MODULATE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG1, D3DTA_TEXTURE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_COLORARG2, D3DTA_DIFFUSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAOP, D3DTOP_SELECTARG1)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev, 0, D3DTSS_ALPHAARG1, D3DTA_DIFFUSE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetTextureStageState(dev, 1, D3DTSS_COLOROP, D3DTOP_DISABLE)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHABLENDENABLE, mode == 3)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_SRCBLEND, D3DBLEND_SRCALPHA)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_DESTBLEND, D3DBLEND_INVSRCALPHA)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHATESTENABLE, mode == 4)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHAFUNC, D3DCMP_GREATER)));
    CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ALPHAREF, 192)));
    struct scene_vertex quad[] = {
        {7.5f,7.5f,.5f,1,0x80808080,0,0}, {55.5f,7.5f,.5f,1,0x80808080,1,0},
        {7.5f,55.5f,.5f,1,0x80808080,0,1}, {55.5f,55.5f,.5f,1,0x80808080,1,1}};
    const WORD indices[] = {0,1,2,2,1,3};
    if (mode == 6) for (unsigned i = 0; i < 4; ++i) quad[i].color = 0xffe08020;
    if (mode == 7) {
        CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ZENABLE, TRUE)));
        CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ZFUNC, D3DCMP_LESS)));
        CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_ZWRITEENABLE, TRUE)));
        CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev, 0, 0, D3DCLEAR_ZBUFFER, 0, .25f, 0)));
    }
    if (mode == 8) {
        CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILENABLE, TRUE)));
        CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILFUNC, D3DCMP_EQUAL)));
        CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILREF, 7)));
        CHECK(SUCCEEDED(IDirect3DDevice9_SetRenderState(dev, D3DRS_STENCILMASK, 255)));
        CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev, 0, 0, D3DCLEAR_STENCIL, 0, 1, 3)));
        D3DRECT region = {0, 0, 32, 64};
        CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev, 1, &region, D3DCLEAR_STENCIL, 0, 1, 7)));
    }
    IDirect3DVertexBuffer9 *vb = 0; IDirect3DIndexBuffer9 *ib = 0;
    CHECK(SUCCEEDED(IDirect3DDevice9_Clear(dev, 0, 0, D3DCLEAR_TARGET, 0xff173b6d, 1, 0)));
    CHECK(SUCCEEDED(IDirect3DDevice9_BeginScene(dev)));
    IDirect3DQuery9 *occlusion = 0, *event = 0;
    if (mode == 9) {
        CHECK(SUCCEEDED(IDirect3DDevice9_CreateQuery(dev, D3DQUERYTYPE_OCCLUSION, &occlusion)));
        CHECK(SUCCEEDED(IDirect3DDevice9_CreateQuery(dev, D3DQUERYTYPE_EVENT, &event)));
        CHECK(SUCCEEDED(IDirect3DQuery9_Issue(occlusion, D3DISSUE_BEGIN)));
    }
    if (method < 2) {
        if (!method) CHECK(SUCCEEDED(IDirect3DDevice9_DrawPrimitiveUP(dev, D3DPT_TRIANGLESTRIP, 2, quad, sizeof quad[0])));
        else CHECK(SUCCEEDED(IDirect3DDevice9_DrawIndexedPrimitiveUP(dev, D3DPT_TRIANGLELIST, 0, 4, 2,
            indices, D3DFMT_INDEX16, quad, sizeof quad[0])));
    } else {
        void *p;
        CHECK(SUCCEEDED(IDirect3DDevice9_CreateVertexBuffer(dev, sizeof quad, D3DUSAGE_DYNAMIC,
            0, D3DPOOL_DEFAULT, &vb, 0)));
        CHECK(SUCCEEDED(IDirect3DVertexBuffer9_Lock(vb, 0, sizeof quad, &p, D3DLOCK_DISCARD)));
        memcpy(p, quad, sizeof quad); CHECK(SUCCEEDED(IDirect3DVertexBuffer9_Unlock(vb)));
        CHECK(SUCCEEDED(IDirect3DDevice9_SetStreamSource(dev, 0, vb, 0, sizeof quad[0])));
        if (method == 2) CHECK(SUCCEEDED(IDirect3DDevice9_DrawPrimitive(dev, D3DPT_TRIANGLESTRIP, 0, 2)));
        else {
            CHECK(SUCCEEDED(IDirect3DDevice9_CreateIndexBuffer(dev, sizeof indices, D3DUSAGE_DYNAMIC,
                D3DFMT_INDEX16, D3DPOOL_DEFAULT, &ib, 0)));
            CHECK(SUCCEEDED(IDirect3DIndexBuffer9_Lock(ib, 0, sizeof indices, &p, D3DLOCK_DISCARD)));
            memcpy(p, indices, sizeof indices); CHECK(SUCCEEDED(IDirect3DIndexBuffer9_Unlock(ib)));
            CHECK(SUCCEEDED(IDirect3DDevice9_SetIndices(dev, ib)));
            CHECK(SUCCEEDED(IDirect3DDevice9_DrawIndexedPrimitive(dev, D3DPT_TRIANGLELIST, 0, 0, 4, 0, 2)));
        }
    }
    if (occlusion) CHECK(SUCCEEDED(IDirect3DQuery9_Issue(occlusion, D3DISSUE_END)));
    CHECK(SUCCEEDED(IDirect3DDevice9_EndScene(dev)));
    if (occlusion) {
        CHECK(SUCCEEDED(IDirect3DQuery9_Issue(event, D3DISSUE_END)));
        DWORD samples = 0; BOOL complete = FALSE; HRESULT hr;
        DWORD start = GetTickCount();
        do { hr = IDirect3DQuery9_GetData(event, &complete, sizeof complete, D3DGETDATA_FLUSH);
            CHECK(SUCCEEDED(hr)); CHECK(GetTickCount() - start < 5000); } while (hr == S_FALSE);
        CHECK(complete);
        do { hr = IDirect3DQuery9_GetData(occlusion, &samples, sizeof samples, D3DGETDATA_FLUSH);
            CHECK(SUCCEEDED(hr)); CHECK(GetTickCount() - start < 5000); } while (hr == S_FALSE);
        CHECK(samples == 48 * 48);
        IDirect3DQuery9_Release(occlusion); IDirect3DQuery9_Release(event);
    }
    CHECK(!read_pixels(dev, pixels));
    CHECK(SUCCEEDED(IDirect3DStateBlock9_Apply(saved)));
    IDirect3DStateBlock9_Release(saved);
    if (vb) IDirect3DVertexBuffer9_Release(vb);
    if (ib) IDirect3DIndexBuffer9_Release(ib);
    IDirect3DTexture9_Release(texture);
    return 0;
}
