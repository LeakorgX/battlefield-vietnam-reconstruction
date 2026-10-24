/* IDirect3D9: the adapter, its display modes, capabilities and format
 * support, and CreateDevice. */
#define INITGUID
#include "vkd3d9.h"

#define D3D(p) ((struct vkd_d3d9 *)(p))

/* ---- display modes ---- */

static D3DDISPLAYMODE *modes;
static UINT nmodes;

static D3DFORMAT bpp_format(DWORD bpp)
{
    return bpp == 32 ? D3DFMT_X8R8G8B8 : bpp == 16 ? D3DFMT_R5G6B5 : D3DFMT_UNKNOWN;
}

static void load_modes(void)
{
    if (modes) return;
    UINT cap = 64;
    modes = vkp_alloc(cap * sizeof *modes);
    if (!modes) return;
    DEVMODEA dm;
    for (DWORD i = 0;; i++) {
        memset(&dm, 0, sizeof dm);
        dm.dmSize = sizeof dm;
        if (!w32.EnumDisplaySettingsA(0, i, &dm)) break;
        D3DFORMAT f = bpp_format(dm.dmBitsPerPel);
        if (f == D3DFMT_UNKNOWN || dm.dmPelsWidth < 640 || dm.dmPelsHeight < 480) continue;
        int dup = 0;
        for (UINT j = 0; j < nmodes; j++)
            if (modes[j].Width == dm.dmPelsWidth && modes[j].Height == dm.dmPelsHeight &&
                modes[j].RefreshRate == dm.dmDisplayFrequency && modes[j].Format == f) { dup = 1; break; }
        if (dup) continue;
        if (nmodes == cap) {
            D3DDISPLAYMODE *n = vkp_realloc(modes, cap * 2 * sizeof *n);
            if (!n) break;
            modes = n; cap *= 2;
        }
        D3DDISPLAYMODE *m = &modes[nmodes++];
        m->Width = dm.dmPelsWidth; m->Height = dm.dmPelsHeight;
        m->RefreshRate = dm.dmDisplayFrequency; m->Format = f;
    }
    /* sorted by size, then refresh rate, as d3d9 lists them */
    for (UINT i = 1; i < nmodes; i++) {
        D3DDISPLAYMODE t = modes[i];
        UINT j = i;
        while (j > 0) {
            D3DDISPLAYMODE *p = &modes[j - 1];
            int gt = p->Width > t.Width || (p->Width == t.Width && (p->Height > t.Height ||
                     (p->Height == t.Height && p->RefreshRate > t.RefreshRate)));
            if (!gt) break;
            modes[j] = *p;
            j--;
        }
        modes[j] = t;
    }
}

void vkd_desktop_mode(D3DDISPLAYMODE *m)
{
    DEVMODEA dm;
    memset(&dm, 0, sizeof dm);
    dm.dmSize = sizeof dm;
    w32.EnumDisplaySettingsA(0, ENUM_CURRENT_SETTINGS, &dm);
    m->Width = dm.dmPelsWidth;
    m->Height = dm.dmPelsHeight;
    m->RefreshRate = dm.dmDisplayFrequency;
    m->Format = bpp_format(dm.dmBitsPerPel);
    if (m->Format == D3DFMT_UNKNOWN) m->Format = D3DFMT_X8R8G8B8;
}

/* ---- capabilities ---- */

void vkd_fill_caps(D3DCAPS9 *c)
{
    memset(c, 0, sizeof *c);
    c->DeviceType = D3DDEVTYPE_HAL;
    c->AdapterOrdinal = 0;
    c->Caps = D3DCAPS_READ_SCANLINE;
    c->Caps2 = D3DCAPS2_FULLSCREENGAMMA | D3DCAPS2_CANMANAGERESOURCE | D3DCAPS2_DYNAMICTEXTURES |
               D3DCAPS2_CANAUTOGENMIPMAP;
    c->Caps3 = D3DCAPS3_ALPHA_FULLSCREEN_FLIP_OR_DISCARD | D3DCAPS3_LINEAR_TO_SRGB_PRESENTATION |
               D3DCAPS3_COPY_TO_VIDMEM | D3DCAPS3_COPY_TO_SYSTEMMEM;
    c->PresentationIntervals = D3DPRESENT_INTERVAL_DEFAULT | D3DPRESENT_INTERVAL_ONE | D3DPRESENT_INTERVAL_TWO |
                               D3DPRESENT_INTERVAL_THREE | D3DPRESENT_INTERVAL_FOUR | D3DPRESENT_INTERVAL_IMMEDIATE;
    c->CursorCaps = D3DCURSORCAPS_COLOR | D3DCURSORCAPS_LOWRES;
    c->DevCaps = D3DDEVCAPS_EXECUTESYSTEMMEMORY | D3DDEVCAPS_EXECUTEVIDEOMEMORY | D3DDEVCAPS_TLVERTEXSYSTEMMEMORY |
                 D3DDEVCAPS_TLVERTEXVIDEOMEMORY | D3DDEVCAPS_TEXTURESYSTEMMEMORY | D3DDEVCAPS_TEXTUREVIDEOMEMORY |
                 D3DDEVCAPS_DRAWPRIMTLVERTEX | D3DDEVCAPS_CANRENDERAFTERFLIP | D3DDEVCAPS_TEXTURENONLOCALVIDMEM |
                 D3DDEVCAPS_DRAWPRIMITIVES2 | D3DDEVCAPS_SEPARATETEXTUREMEMORIES | D3DDEVCAPS_DRAWPRIMITIVES2EX |
                 D3DDEVCAPS_HWTRANSFORMANDLIGHT | D3DDEVCAPS_CANBLTSYSTONONLOCAL | D3DDEVCAPS_HWRASTERIZATION |
                 D3DDEVCAPS_PUREDEVICE;
    c->PrimitiveMiscCaps = D3DPMISCCAPS_MASKZ | D3DPMISCCAPS_CULLNONE | D3DPMISCCAPS_CULLCW | D3DPMISCCAPS_CULLCCW |
                           D3DPMISCCAPS_COLORWRITEENABLE | D3DPMISCCAPS_CLIPPLANESCALEDPOINTS |
                           D3DPMISCCAPS_TSSARGTEMP | D3DPMISCCAPS_BLENDOP | D3DPMISCCAPS_INDEPENDENTWRITEMASKS |
                           D3DPMISCCAPS_PERSTAGECONSTANT | D3DPMISCCAPS_FOGANDSPECULARALPHA |
                           D3DPMISCCAPS_SEPARATEALPHABLEND | D3DPMISCCAPS_MRTINDEPENDENTBITDEPTHS |
                           D3DPMISCCAPS_MRTPOSTPIXELSHADERBLENDING | D3DPMISCCAPS_FOGVERTEXCLAMPED;
    c->RasterCaps = D3DPRASTERCAPS_DITHER | D3DPRASTERCAPS_ZTEST | D3DPRASTERCAPS_FOGVERTEX | D3DPRASTERCAPS_FOGTABLE |
                    D3DPRASTERCAPS_MIPMAPLODBIAS | D3DPRASTERCAPS_ZFOG | D3DPRASTERCAPS_WFOG |
                    D3DPRASTERCAPS_FOGRANGE | D3DPRASTERCAPS_ANISOTROPY | D3DPRASTERCAPS_COLORPERSPECTIVE |
                    D3DPRASTERCAPS_SCISSORTEST | D3DPRASTERCAPS_SLOPESCALEDEPTHBIAS | D3DPRASTERCAPS_DEPTHBIAS |
                    D3DPRASTERCAPS_MULTISAMPLE_TOGGLE;
    c->ZCmpCaps = c->AlphaCmpCaps = D3DPCMPCAPS_NEVER | D3DPCMPCAPS_LESS | D3DPCMPCAPS_EQUAL | D3DPCMPCAPS_LESSEQUAL |
                                    D3DPCMPCAPS_GREATER | D3DPCMPCAPS_NOTEQUAL | D3DPCMPCAPS_GREATEREQUAL |
                                    D3DPCMPCAPS_ALWAYS;
    c->SrcBlendCaps = c->DestBlendCaps =
        D3DPBLENDCAPS_ZERO | D3DPBLENDCAPS_ONE | D3DPBLENDCAPS_SRCCOLOR | D3DPBLENDCAPS_INVSRCCOLOR |
        D3DPBLENDCAPS_SRCALPHA | D3DPBLENDCAPS_INVSRCALPHA | D3DPBLENDCAPS_DESTALPHA | D3DPBLENDCAPS_INVDESTALPHA |
        D3DPBLENDCAPS_DESTCOLOR | D3DPBLENDCAPS_INVDESTCOLOR | D3DPBLENDCAPS_SRCALPHASAT |
        D3DPBLENDCAPS_BOTHSRCALPHA | D3DPBLENDCAPS_BOTHINVSRCALPHA | D3DPBLENDCAPS_BLENDFACTOR;
    c->ShadeCaps = D3DPSHADECAPS_COLORGOURAUDRGB | D3DPSHADECAPS_SPECULARGOURAUDRGB |
                   D3DPSHADECAPS_ALPHAGOURAUDBLEND | D3DPSHADECAPS_FOGGOURAUD;
    c->TextureCaps = D3DPTEXTURECAPS_PERSPECTIVE | D3DPTEXTURECAPS_ALPHA | D3DPTEXTURECAPS_PROJECTED |
                     D3DPTEXTURECAPS_CUBEMAP | D3DPTEXTURECAPS_VOLUMEMAP | D3DPTEXTURECAPS_MIPMAP |
                     D3DPTEXTURECAPS_MIPVOLUMEMAP | D3DPTEXTURECAPS_MIPCUBEMAP;
    c->TextureFilterCaps = c->CubeTextureFilterCaps = c->VolumeTextureFilterCaps =
        D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR | D3DPTFILTERCAPS_MINFANISOTROPIC |
        D3DPTFILTERCAPS_MIPFPOINT | D3DPTFILTERCAPS_MIPFLINEAR | D3DPTFILTERCAPS_MAGFPOINT |
        D3DPTFILTERCAPS_MAGFLINEAR | D3DPTFILTERCAPS_MAGFANISOTROPIC;
    c->TextureAddressCaps = c->VolumeTextureAddressCaps =
        D3DPTADDRESSCAPS_WRAP | D3DPTADDRESSCAPS_MIRROR | D3DPTADDRESSCAPS_CLAMP | D3DPTADDRESSCAPS_BORDER |
        D3DPTADDRESSCAPS_INDEPENDENTUV | (vkd.mirror_clamp ? D3DPTADDRESSCAPS_MIRRORONCE : 0);
    c->LineCaps = D3DLINECAPS_TEXTURE | D3DLINECAPS_ZTEST | D3DLINECAPS_BLEND | D3DLINECAPS_ALPHACMP |
                  D3DLINECAPS_FOG | D3DLINECAPS_ANTIALIAS;
    c->MaxTextureWidth = c->MaxTextureHeight = VKP_MIN(vkd.props.limits.maxImageDimension2D, 8192);
    c->MaxVolumeExtent = VKP_MIN(vkd.props.limits.maxImageDimension3D, 2048);
    c->MaxTextureRepeat = 8192;
    c->MaxTextureAspectRatio = 8192;
    c->MaxAnisotropy = vkd.anisotropy ? (DWORD)VKP_MIN(vkd.max_aniso, 16) : 1;
    c->MaxVertexW = 1e10f;
    c->GuardBandLeft = -32768; c->GuardBandTop = -32768;
    c->GuardBandRight = 32768; c->GuardBandBottom = 32768;
    c->ExtentsAdjust = 0;
    c->StencilCaps = D3DSTENCILCAPS_KEEP | D3DSTENCILCAPS_ZERO | D3DSTENCILCAPS_REPLACE | D3DSTENCILCAPS_INCRSAT |
                     D3DSTENCILCAPS_DECRSAT | D3DSTENCILCAPS_INVERT | D3DSTENCILCAPS_INCR | D3DSTENCILCAPS_DECR |
                     D3DSTENCILCAPS_TWOSIDED;
    c->FVFCaps = 8 | D3DFVFCAPS_PSIZE;
    c->TextureOpCaps = D3DTEXOPCAPS_DISABLE | D3DTEXOPCAPS_SELECTARG1 | D3DTEXOPCAPS_SELECTARG2 |
                       D3DTEXOPCAPS_MODULATE | D3DTEXOPCAPS_MODULATE2X | D3DTEXOPCAPS_MODULATE4X |
                       D3DTEXOPCAPS_ADD | D3DTEXOPCAPS_ADDSIGNED | D3DTEXOPCAPS_ADDSIGNED2X |
                       D3DTEXOPCAPS_SUBTRACT | D3DTEXOPCAPS_ADDSMOOTH | D3DTEXOPCAPS_BLENDDIFFUSEALPHA |
                       D3DTEXOPCAPS_BLENDTEXTUREALPHA | D3DTEXOPCAPS_BLENDFACTORALPHA |
                       D3DTEXOPCAPS_BLENDTEXTUREALPHAPM | D3DTEXOPCAPS_BLENDCURRENTALPHA |
                       D3DTEXOPCAPS_PREMODULATE | D3DTEXOPCAPS_MODULATEALPHA_ADDCOLOR |
                       D3DTEXOPCAPS_MODULATECOLOR_ADDALPHA | D3DTEXOPCAPS_MODULATEINVALPHA_ADDCOLOR |
                       D3DTEXOPCAPS_MODULATEINVCOLOR_ADDALPHA | D3DTEXOPCAPS_BUMPENVMAP |
                       D3DTEXOPCAPS_BUMPENVMAPLUMINANCE | D3DTEXOPCAPS_DOTPRODUCT3 | D3DTEXOPCAPS_MULTIPLYADD |
                       D3DTEXOPCAPS_LERP;
    c->MaxTextureBlendStages = 8;
    c->MaxSimultaneousTextures = 8;
    c->VertexProcessingCaps = D3DVTXPCAPS_TEXGEN | D3DVTXPCAPS_MATERIALSOURCE7 | D3DVTXPCAPS_DIRECTIONALLIGHTS |
                              D3DVTXPCAPS_POSITIONALLIGHTS | D3DVTXPCAPS_LOCALVIEWER | D3DVTXPCAPS_TEXGEN_SPHEREMAP;
    c->MaxActiveLights = 8;
    c->MaxUserClipPlanes = 6;
    c->MaxVertexBlendMatrices = 4;
    c->MaxVertexBlendMatrixIndex = 0;
    c->MaxPointSize = VKP_MIN(vkd.props.limits.pointSizeRange[1], 256.0f);
    if (c->MaxPointSize < 1) c->MaxPointSize = 1;
    c->MaxPrimitiveCount = 0x00555555;
    c->MaxVertexIndex = 0x00ffffff;
    c->MaxStreams = 16;
    c->MaxStreamStride = 508;
    c->VertexShaderVersion = D3DVS_VERSION(3, 0);
    c->MaxVertexShaderConst = 256;
    c->PixelShaderVersion = D3DPS_VERSION(3, 0);
    c->PixelShader1xMaxValue = 8.0f;
    c->DevCaps2 = D3DDEVCAPS2_STREAMOFFSET | D3DDEVCAPS2_VERTEXELEMENTSCANSHARESTREAMOFFSET |
                  D3DDEVCAPS2_CAN_STRETCHRECT_FROM_TEXTURES;
    c->MaxNpatchTessellationLevel = 0;
    c->MasterAdapterOrdinal = 0;
    c->AdapterOrdinalInGroup = 0;
    c->NumberOfAdaptersInGroup = 1;
    c->DeclTypes = D3DDTCAPS_UBYTE4 | D3DDTCAPS_UBYTE4N | D3DDTCAPS_SHORT2N | D3DDTCAPS_SHORT4N |
                   D3DDTCAPS_USHORT2N | D3DDTCAPS_USHORT4N | D3DDTCAPS_UDEC3 | D3DDTCAPS_DEC3N |
                   D3DDTCAPS_FLOAT16_2 | D3DDTCAPS_FLOAT16_4;
    c->NumSimultaneousRTs = 4;
    c->StretchRectFilterCaps = D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MINFLINEAR |
                               D3DPTFILTERCAPS_MAGFPOINT | D3DPTFILTERCAPS_MAGFLINEAR;
    c->VS20Caps.Caps = D3DVS20CAPS_PREDICATION;
    c->VS20Caps.DynamicFlowControlDepth = D3DVS20_MAX_DYNAMICFLOWCONTROLDEPTH;
    c->VS20Caps.NumTemps = D3DVS20_MAX_NUMTEMPS;
    c->VS20Caps.StaticFlowControlDepth = D3DVS20_MAX_STATICFLOWCONTROLDEPTH;
    c->PS20Caps.Caps = D3DPS20CAPS_ARBITRARYSWIZZLE | D3DPS20CAPS_GRADIENTINSTRUCTIONS | D3DPS20CAPS_PREDICATION |
                       D3DPS20CAPS_NODEPENDENTREADLIMIT | D3DPS20CAPS_NOTEXINSTRUCTIONLIMIT;
    c->PS20Caps.DynamicFlowControlDepth = D3DPS20_MAX_DYNAMICFLOWCONTROLDEPTH;
    c->PS20Caps.NumTemps = D3DPS20_MAX_NUMTEMPS;
    c->PS20Caps.StaticFlowControlDepth = D3DPS20_MAX_STATICFLOWCONTROLDEPTH;
    c->PS20Caps.NumInstructionSlots = D3DPS20_MAX_NUMINSTRUCTIONSLOTS;
    c->VertexTextureFilterCaps = D3DPTFILTERCAPS_MINFPOINT | D3DPTFILTERCAPS_MAGFPOINT;
    c->MaxVShaderInstructionsExecuted = 0xffffffff;
    c->MaxPShaderInstructionsExecuted = 0xffffffff;
    c->MaxVertexShader30InstructionSlots = 32768;
    c->MaxPixelShader30InstructionSlots = 32768;
}

/* ---- format support ---- */

static int is_depth(const struct vkd_format *f) { return f->depth || f->stencil; }

HRESULT vkd_check_format(D3DFORMAT adapter, DWORD usage, D3DRESOURCETYPE type, D3DFORMAT fmt)
{
    if (adapter != D3DFMT_X8R8G8B8 && adapter != D3DFMT_R5G6B5 && adapter != D3DFMT_X1R5G5B5)
        return D3DERR_NOTAVAILABLE;
    const struct vkd_format *f = vkd_format(fmt);
    if (!f) return D3DERR_NOTAVAILABLE;
    if (fmt == D3DFMT_INDEX16 || fmt == D3DFMT_INDEX32 || fmt == D3DFMT_VERTEXDATA)
        return type == D3DRTYPE_VERTEXBUFFER || type == D3DRTYPE_INDEXBUFFER ? D3D_OK : D3DERR_NOTAVAILABLE;
    /* formats NVIDIA's d3d9 never offered */
    if (fmt == D3DFMT_D15S1 || fmt == D3DFMT_D24X4S4 || fmt == D3DFMT_D24FS8 || fmt == D3DFMT_D16_LOCKABLE ||
        fmt == D3DFMT_D32F_LOCKABLE || fmt == D3DFMT_A4L4)
        return D3DERR_NOTAVAILABLE;
    if (fmt == (D3DFORMAT)MAKEFOURCC('N', 'U', 'L', 'L'))
        return (usage & D3DUSAGE_RENDERTARGET) && type == D3DRTYPE_SURFACE ? D3D_OK : D3DERR_NOTAVAILABLE;
    VkFormatFeatureFlags need = 0;
    if (usage & D3DUSAGE_RENDERTARGET) {
        if (is_depth(f)) return D3DERR_NOTAVAILABLE;
        need |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BIT;
    }
    if (usage & D3DUSAGE_DEPTHSTENCIL) {
        if (!is_depth(f)) return D3DERR_NOTAVAILABLE;
        need |= VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    if ((usage & D3DUSAGE_QUERY_POSTPIXELSHADER_BLENDING) && !is_depth(f))
        need |= VK_FORMAT_FEATURE_COLOR_ATTACHMENT_BLEND_BIT;
    if (type == D3DRTYPE_TEXTURE || type == D3DRTYPE_CUBETEXTURE || type == D3DRTYPE_VOLUMETEXTURE) {
        need |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
        if (usage & D3DUSAGE_QUERY_FILTER) need |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if (type == D3DRTYPE_VOLUMETEXTURE && is_depth(f)) return D3DERR_NOTAVAILABLE;
        if (is_depth(f) && !(usage & D3DUSAGE_DEPTHSTENCIL)) return D3DERR_NOTAVAILABLE;
        if (fmt == D3DFMT_D32 && type == D3DRTYPE_TEXTURE) return D3DERR_NOTAVAILABLE;
    }
    if ((usage & D3DUSAGE_QUERY_VERTEXTEXTURE) && !is_depth(f)) need |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if ((usage & D3DUSAGE_QUERY_SRGBREAD) && !f->vk_srgb) return D3DERR_NOTAVAILABLE;
    if ((usage & D3DUSAGE_QUERY_SRGBWRITE) && (!f->vk_srgb || f->block > 1)) return D3DERR_NOTAVAILABLE;
    if (usage & D3DUSAGE_QUERY_WRAPANDMIP) need |= VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT;
    if (!need) need = VK_FORMAT_FEATURE_TRANSFER_DST_BIT;
    if (!vkd_format_supported(f, need, 1)) return D3DERR_NOTAVAILABLE;
    if ((usage & D3DUSAGE_AUTOGENMIPMAP) &&
        (f->block > 1 || is_depth(f) || !vkd_format_supported(f, VK_FORMAT_FEATURE_BLIT_SRC_BIT |
                                                              VK_FORMAT_FEATURE_BLIT_DST_BIT |
                                                              VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT, 1)))
        return D3DOK_NOAUTOGEN;
    return D3D_OK;
}

static int backbuffer_ok(D3DFORMAT f)
{
    return f == D3DFMT_A8R8G8B8 || f == D3DFMT_X8R8G8B8 || f == D3DFMT_R5G6B5 || f == D3DFMT_X1R5G5B5 ||
           f == D3DFMT_A1R5G5B5 || f == D3DFMT_A2R10G10B10;
}

/* ---- IDirect3D9 ---- */

static HRESULT STDMETHODCALLTYPE d3d_QueryInterface(IDirect3D9 *self, REFIID riid, void **out)
{
    if (!out) return E_POINTER;
    if (vkd_guid_eq(riid, &IID_IUnknown) || vkd_guid_eq(riid, &IID_IDirect3D9)) {
        *out = self;
        IDirect3D9_AddRef(self);
        return S_OK;
    }
    *out = 0;
    return E_NOINTERFACE;
}

static ULONG STDMETHODCALLTYPE d3d_AddRef(IDirect3D9 *self) { return (ULONG)InterlockedIncrement(&D3D(self)->ref); }

static ULONG STDMETHODCALLTYPE d3d_Release(IDirect3D9 *self)
{
    LONG r = InterlockedDecrement(&D3D(self)->ref);
    if (!r) vkp_free(self);
    return (ULONG)r;
}

static HRESULT STDMETHODCALLTYPE d3d_RegisterSoftwareDevice(IDirect3D9 *self, void *init) { return D3D_OK; }
static UINT STDMETHODCALLTYPE d3d_GetAdapterCount(IDirect3D9 *self) { return 1; }

static HRESULT STDMETHODCALLTYPE d3d_GetAdapterIdentifier(IDirect3D9 *self, UINT adapter, DWORD flags,
                                                          D3DADAPTER_IDENTIFIER9 *id)
{
    if (adapter || !id) return D3DERR_INVALIDCALL;
    memset(id, 0, sizeof *id);
    const char *drv = vkd.props.vendorID == 0x10de ? "nvd3dum.dll" : vkd.props.vendorID == 0x1002 ? "aticfx32.dll" : "igdumd32.dll";
    for (int i = 0; drv[i] && i < MAX_DEVICE_IDENTIFIER_STRING - 1; i++) id->Driver[i] = drv[i];
    for (int i = 0; vkd.props.deviceName[i] && i < MAX_DEVICE_IDENTIFIER_STRING - 1; i++)
        id->Description[i] = vkd.props.deviceName[i];
    memcpy(id->DeviceName, "\\\\.\\DISPLAY1", 13);
    id->DriverVersion.HighPart = (6 << 16) | 14;
    id->DriverVersion.LowPart = (10 << 16) | 1;
    id->VendorId = vkd.props.vendorID;
    id->DeviceId = vkd.props.deviceID;
    id->SubSysId = 0;
    id->Revision = 0;
    memcpy(&id->DeviceIdentifier, vkd.props.pipelineCacheUUID, sizeof(GUID));
    id->WHQLLevel = 1;
    return D3D_OK;
}

static UINT STDMETHODCALLTYPE d3d_GetAdapterModeCount(IDirect3D9 *self, UINT adapter, D3DFORMAT format)
{
    if (adapter) return 0;
    load_modes();
    UINT n = 0;
    for (UINT i = 0; i < nmodes; i++) n += modes[i].Format == format;
    return n;
}

static HRESULT STDMETHODCALLTYPE d3d_EnumAdapterModes(IDirect3D9 *self, UINT adapter, D3DFORMAT format, UINT index,
                                                      D3DDISPLAYMODE *mode)
{
    if (adapter || !mode) return D3DERR_INVALIDCALL;
    load_modes();
    for (UINT i = 0; i < nmodes; i++) {
        if (modes[i].Format != format) continue;
        if (!index--) { *mode = modes[i]; return D3D_OK; }
    }
    return D3DERR_INVALIDCALL;
}

static HRESULT STDMETHODCALLTYPE d3d_GetAdapterDisplayMode(IDirect3D9 *self, UINT adapter, D3DDISPLAYMODE *mode)
{
    if (adapter || !mode) return D3DERR_INVALIDCALL;
    vkd_desktop_mode(mode);
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE d3d_CheckDeviceType(IDirect3D9 *self, UINT adapter, D3DDEVTYPE type,
                                                     D3DFORMAT display, D3DFORMAT back, BOOL windowed)
{
    if (adapter) return D3DERR_INVALIDCALL;
    if (type != D3DDEVTYPE_HAL) return D3DERR_NOTAVAILABLE;
    if (display != D3DFMT_X8R8G8B8 && display != D3DFMT_R5G6B5 && display != D3DFMT_X1R5G5B5 &&
        !(windowed && display == D3DFMT_UNKNOWN))
        return D3DERR_NOTAVAILABLE;
    if (back == D3DFMT_UNKNOWN && windowed) return D3D_OK;
    return backbuffer_ok(back) ? D3D_OK : D3DERR_NOTAVAILABLE;
}

static HRESULT STDMETHODCALLTYPE d3d_CheckDeviceFormat(IDirect3D9 *self, UINT adapter, D3DDEVTYPE type,
                                                       D3DFORMAT adapter_format, DWORD usage,
                                                       D3DRESOURCETYPE rtype, D3DFORMAT format)
{
    if (adapter) return D3DERR_INVALIDCALL;
    if (type != D3DDEVTYPE_HAL) return D3DERR_NOTAVAILABLE;
    return vkd_check_format(adapter_format, usage, rtype, format);
}

static VkSampleCountFlags sample_counts(const struct vkd_format *f)
{
    if (!f || f->vk == VK_FORMAT_UNDEFINED) return VK_SAMPLE_COUNT_1_BIT;
    VkImageFormatProperties p;
    VkImageUsageFlags usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                              (is_depth(f) ? VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT
                                           : VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT);
    if (vkGetPhysicalDeviceImageFormatProperties(vkd.phys, f->vk, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, usage, 0,
                                                 &p) != VK_SUCCESS)
        return VK_SAMPLE_COUNT_1_BIT;
    return p.sampleCounts;
}

static HRESULT STDMETHODCALLTYPE d3d_CheckDeviceMultiSampleType(IDirect3D9 *self, UINT adapter, D3DDEVTYPE type,
                                                                D3DFORMAT format, BOOL windowed,
                                                                D3DMULTISAMPLE_TYPE ms, DWORD *quality)
{
    if (adapter) return D3DERR_INVALIDCALL;
    if (quality) *quality = 0;
    if (type != D3DDEVTYPE_HAL) return D3DERR_NOTAVAILABLE;
    const struct vkd_format *f = vkd_format(format);
    if (!f || f->vk == VK_FORMAT_UNDEFINED) return D3DERR_NOTAVAILABLE;
    VkSampleCountFlags have = sample_counts(f);
    if (ms == D3DMULTISAMPLE_NONE) { if (quality) *quality = 1; return D3D_OK; }
    if (ms == D3DMULTISAMPLE_NONMASKABLE) {
        DWORD q = 0;
        for (int s = 2; s <= 8; s *= 2) q += !!(have & (VkSampleCountFlags)s);
        if (!q) return D3DERR_NOTAVAILABLE;
        if (quality) *quality = q;
        return D3D_OK;
    }
    if (ms != 2 && ms != 4 && ms != 8) return D3DERR_NOTAVAILABLE;
    if (!(have & (VkSampleCountFlags)ms)) return D3DERR_NOTAVAILABLE;
    if (quality) *quality = 1;
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE d3d_CheckDepthStencilMatch(IDirect3D9 *self, UINT adapter, D3DDEVTYPE type,
                                                            D3DFORMAT adapter_format, D3DFORMAT rt, D3DFORMAT ds)
{
    if (adapter) return D3DERR_INVALIDCALL;
    const struct vkd_format *r = vkd_format(rt), *d = vkd_format(ds);
    if (!r || !d || !is_depth(d) || is_depth(r)) return D3DERR_NOTAVAILABLE;
    return D3D_OK;
}

static HRESULT STDMETHODCALLTYPE d3d_CheckDeviceFormatConversion(IDirect3D9 *self, UINT adapter, D3DDEVTYPE type,
                                                                 D3DFORMAT src, D3DFORMAT dst)
{
    if (adapter) return D3DERR_INVALIDCALL;
    return backbuffer_ok(dst) && vkd_format(src) ? D3D_OK : D3DERR_NOTAVAILABLE;
}

static HRESULT STDMETHODCALLTYPE d3d_GetDeviceCaps(IDirect3D9 *self, UINT adapter, D3DDEVTYPE type, D3DCAPS9 *caps)
{
    if (adapter || !caps) return D3DERR_INVALIDCALL;
    if (type != D3DDEVTYPE_HAL) return D3DERR_NOTAVAILABLE;
    vkd_fill_caps(caps);
    return D3D_OK;
}

static HMONITOR STDMETHODCALLTYPE d3d_GetAdapterMonitor(IDirect3D9 *self, UINT adapter)
{
    POINT p = {0, 0};
    return adapter ? 0 : w32.MonitorFromPoint(p, MONITOR_DEFAULTTOPRIMARY);
}

static HRESULT STDMETHODCALLTYPE d3d_CreateDevice(IDirect3D9 *self, UINT adapter, D3DDEVTYPE type, HWND focus,
                                                  DWORD flags, D3DPRESENT_PARAMETERS *pp, IDirect3DDevice9 **out)
{
    if (!out) return D3DERR_INVALIDCALL;
    *out = 0;
    if (adapter || !pp) return D3DERR_INVALIDCALL;
    if (type != D3DDEVTYPE_HAL) return D3DERR_NOTAVAILABLE;
    return vkd_device_create(D3D(self), adapter, type, focus, flags, pp, out);
}

static const IDirect3D9Vtbl d3d_vtbl = {
    .QueryInterface = d3d_QueryInterface,
    .AddRef = d3d_AddRef,
    .Release = d3d_Release,
    .RegisterSoftwareDevice = d3d_RegisterSoftwareDevice,
    .GetAdapterCount = d3d_GetAdapterCount,
    .GetAdapterIdentifier = d3d_GetAdapterIdentifier,
    .GetAdapterModeCount = d3d_GetAdapterModeCount,
    .EnumAdapterModes = d3d_EnumAdapterModes,
    .GetAdapterDisplayMode = d3d_GetAdapterDisplayMode,
    .CheckDeviceType = d3d_CheckDeviceType,
    .CheckDeviceFormat = d3d_CheckDeviceFormat,
    .CheckDeviceMultiSampleType = d3d_CheckDeviceMultiSampleType,
    .CheckDepthStencilMatch = d3d_CheckDepthStencilMatch,
    .CheckDeviceFormatConversion = d3d_CheckDeviceFormatConversion,
    .GetDeviceCaps = d3d_GetDeviceCaps,
    .GetAdapterMonitor = d3d_GetAdapterMonitor,
    .CreateDevice = d3d_CreateDevice,
};

IDirect3D9 *WINAPI vk_Direct3DCreate9(UINT sdk)
{
    if (!vkd_gpu_init()) {
        vkp_log("vulkan: Direct3DCreate9 failed: no usable Vulkan device");
        return 0;
    }
    struct vkd_d3d9 *d = vkp_alloc(sizeof *d);
    if (!d) return 0;
    d->vtbl = &d3d_vtbl;
    d->ref = 1;
    vkp_log("vulkan: Direct3DCreate9(%u)", sdk);
    return (IDirect3D9 *)d;
}
