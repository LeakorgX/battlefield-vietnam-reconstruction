/* Shader selection, dynamic rendering, descriptors and all four draw paths. */
static void color_float(float *p, DWORD c)
{
    p[0] = ((c >> 16) & 255) / 255.0f;
    p[1] = ((c >> 8) & 255) / 255.0f;
    p[2] = (c & 255) / 255.0f;
    p[3] = (c >> 24) / 255.0f;
}
static void matrix_constants(float (*p)[4], const D3DMATRIX *m)
{
    for (UINT i = 0; i < 4; i++)
        for (UINT j = 0; j < 4; j++)
            p[i][j] = m->m[j][i];
}
static void transform_vec(float *out, const float *in, const D3DMATRIX *m)
{
    for (UINT j = 0; j < 4; j++) {
        out[j] = 0;
        for (UINT i = 0; i < 4; i++)
            out[j] += in[i] * m->m[i][j];
    }
}
static void normalize_vec(float *v)
{
    float n = vkd_sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (n)
        for (UINT i = 0; i < 3; i++)
            v[i] /= n;
}
static void ff_keys(struct vkd_device *d, struct ff_vs_key *vs, struct ff_ps_key *ps)
{
    struct vkd_state *s = &d->st;
    struct vkd_vdecl *v = s->decl;

    memset(vs, 0, sizeof *vs);
    memset(ps, 0, sizeof *ps);

    vs->rhw = v->has_positiont;
    vs->has_normal = v->has_normal;

    vs->has_color[0] = v->has_color[0];
    vs->has_color[1] = v->has_color[1];
    vs->has_psize = v->has_psize;

    vs->blend = s->rs[D3DRS_VERTEXBLEND] <= 3 ? s->rs[D3DRS_VERTEXBLEND] : 0;

    vs->lighting = !vs->rhw && s->rs[D3DRS_LIGHTING];
    vs->normalize = !!s->rs[D3DRS_NORMALIZENORMALS];

    vs->local_viewer = !!s->rs[D3DRS_LOCALVIEWER];
    vs->specular = !!s->rs[D3DRS_SPECULARENABLE];

    vs->color_vertex = !!s->rs[D3DRS_COLORVERTEX];

    vs->mat_src[0] = s->rs[D3DRS_DIFFUSEMATERIALSOURCE];
    vs->mat_src[1] = s->rs[D3DRS_AMBIENTMATERIALSOURCE];

    vs->mat_src[2] = s->rs[D3DRS_SPECULARMATERIALSOURCE];
    vs->mat_src[3] = s->rs[D3DRS_EMISSIVEMATERIALSOURCE];

    for (UINT i = 0; i < d->nactive; i++)
        vs->light[i] = s->lights[d->active_lights[i]].l.Type;

    if (s->rs[D3DRS_FOGENABLE])
        vs->fog = vs->rhw ? FF_FOG_SPECULAR_ALPHA : s->rs[D3DRS_FOGVERTEXMODE];

    vs->range_fog = !!s->rs[D3DRS_RANGEFOGENABLE];
    vs->point_scale = !!s->rs[D3DRS_POINTSCALEENABLE];

    for (UINT i = 0; i < 8; i++) {
        vs->in_dim[i] = v->tex_dim[i];
        vs->tc_index[i] = s->tss[i][D3DTSS_TEXCOORDINDEX] & 7;

        vs->tc_gen[i] = s->tss[i][D3DTSS_TEXCOORDINDEX] >> 16;

        DWORD tf = s->tss[i][D3DTSS_TEXTURETRANSFORMFLAGS];
        vs->tc_count[i] = tf & 7;
        vs->tc_proj[i] = !!(tf & D3DTTFF_PROJECTED);

        vs->ntex = i + 1;

        struct ff_stage *st = &ps->st[i];

        st->cop = s->tss[i][D3DTSS_COLOROP];
        st->aop = s->tss[i][D3DTSS_ALPHAOP];

        st->carg[0] = s->tss[i][D3DTSS_COLORARG0];
        st->carg[1] = s->tss[i][D3DTSS_COLORARG1];
        st->carg[2] = s->tss[i][D3DTSS_COLORARG2];

        st->aarg[0] = s->tss[i][D3DTSS_ALPHAARG0];
        st->aarg[1] = s->tss[i][D3DTSS_ALPHAARG1];
        st->aarg[2] = s->tss[i][D3DTSS_ALPHAARG2];

        st->temp = s->tss[i][D3DTSS_RESULTARG] == D3DTA_TEMP;
        st->proj = !!(tf & D3DTTFF_PROJECTED);

        struct vkd_texture *t = vkd_res_texture(s->tex[i]);

        st->tex = t ? (t->r.type == D3DRTYPE_CUBETEXTURE     ? DXSO_TEX_CUBE
                       : t->r.type == D3DRTYPE_VOLUMETEXTURE ? DXSO_TEX_3D
                                                             : DXSO_TEX_2D) +
                          1
                    : 0;

        if (st->cop == D3DTOP_DISABLE)
            break;
    }
    ps->specular = !!s->rs[D3DRS_SPECULARENABLE];
}
static struct vkd_shader *ff_shader(struct vkd_device *d, const void *key, UINT size, int ps)
{
    struct vkd_map *map = ps ? &d->ff_ps : &d->ff_vs;

    struct vkd_shader *s = vkd_map_get(map, key, size);
    if (s)
        return s;

    UINT words;
    uint32_t *code = ps ? ff_ps_generate(key, &words) : ff_vs_generate(key, &words);

    if (!code)
        return 0;

    HRESULT hr = vkd_create_shader(d, (const DWORD *)code, ps, &s);
    vkp_free(code);

    if (FAILED(hr))
        return 0;

    vkd_map_put(map, key, size, s);

    if (vkd_map_get(map, key, size) != s) {
        vkd_shader_free(s);
        return 0;
    }
    return s;
}
/* Fixed-function constants, recomputed when their render states change. */
static void ff_vs_constants(struct vkd_device *d)
{
    struct vkd_state *s = &d->st;

    memset(d->ffvs.f, 0, FF_VS_CONSTS * sizeof d->ffvs.f[0]);

    for (UINT i = 0; i < 4; i++) {
        D3DMATRIX wv;
        matrix_mul(&wv, &s->world[i], &s->view);

        matrix_constants(&d->ffvs.f[FF_VS_WORLDVIEW + i * 4], &wv);

        /* Inverse transpose of the 3x3 world-view, for row-vector normals. */
        float a = wv.m[0][0], b = wv.m[0][1], c = wv.m[0][2], e = wv.m[1][0], f = wv.m[1][1], g = wv.m[1][2];

        float h = wv.m[2][0], j = wv.m[2][1], k = wv.m[2][2];

        float det = a * (f * k - g * j) - b * (e * k - g * h) + c * (e * j - f * h);

        if (det) {
            float inv = 1.0f / det;

            float n[3][3] = {{(f * k - g * j) * inv, (g * h - e * k) * inv, (e * j - f * h) * inv},
                             {(c * j - b * k) * inv, (a * k - c * h) * inv, (b * h - a * j) * inv},
                             {(b * g - c * f) * inv, (c * e - a * g) * inv, (a * f - b * e) * inv}};

            for (UINT r = 0; r < 3; r++)
                for (UINT q = 0; q < 3; q++)
                    d->ffvs.f[FF_VS_NORMAL + i * 4 + r][q] = n[q][r];
        }
    }
    matrix_constants(&d->ffvs.f[FF_VS_PROJ], &s->proj);

    for (UINT i = 0; i < 8; i++)
        matrix_constants(&d->ffvs.f[FF_VS_TEXTURE + i * 4], &s->texm[i]);

    memcpy(d->ffvs.f[FF_VS_MAT_DIFFUSE], &s->material.Diffuse, 16);

    memcpy(d->ffvs.f[FF_VS_MAT_AMBIENT], &s->material.Ambient, 16);

    memcpy(d->ffvs.f[FF_VS_MAT_SPECULAR], &s->material.Specular, 16);

    memcpy(d->ffvs.f[FF_VS_MAT_EMISSIVE], &s->material.Emissive, 16);

    d->ffvs.f[FF_VS_MAT_POWER][0] = s->material.Power;

    color_float(d->ffvs.f[FF_VS_AMBIENT], s->rs[D3DRS_AMBIENT]);

    float start = vkd_f(s->rs[D3DRS_FOGSTART]), end = vkd_f(s->rs[D3DRS_FOGEND]);

    float *fog = d->ffvs.f[FF_VS_FOG];
    fog[0] = start;
    fog[1] = end;
    fog[2] = end != start ? 1.0f / (end - start) : 0;
    fog[3] = vkd_f(s->rs[D3DRS_FOGDENSITY]);

    float *p = d->ffvs.f[FF_VS_POINT];
    p[0] = vkd_f(s->rs[D3DRS_POINTSIZE]);
    p[1] = vkd_f(s->rs[D3DRS_POINTSIZE_MIN]);
    p[2] = vkd_f(s->rs[D3DRS_POINTSIZE_MAX]);
    p[3] = s->viewport.Height;

    p = d->ffvs.f[FF_VS_POINT_SCALE];
    p[0] = vkd_f(s->rs[D3DRS_POINTSCALE_A]);
    p[1] = vkd_f(s->rs[D3DRS_POINTSCALE_B]);
    p[2] = vkd_f(s->rs[D3DRS_POINTSCALE_C]);

    p = d->ffvs.f[FF_VS_VIEWPORT_SCALE];
    p[0] = 2.0f / s->viewport.Width;
    p[1] = -2.0f / s->viewport.Height;

    p[2] = s->viewport.MaxZ != s->viewport.MinZ ? 1.0f / (s->viewport.MaxZ - s->viewport.MinZ) : 0;
    p[3] = 1;

    p = d->ffvs.f[FF_VS_VIEWPORT_OFFSET];
    p[0] = -1 - 2.0f * s->viewport.X / s->viewport.Width;
    p[1] = 1 + 2.0f * s->viewport.Y / s->viewport.Height;

    p[2] = -s->viewport.MinZ * d->ffvs.f[FF_VS_VIEWPORT_SCALE][2];

    for (UINT i = 0; i < d->nactive; i++) {
        D3DLIGHT9 *l = &s->lights[d->active_lights[i]].l;
        float (*cst)[4] = &d->ffvs.f[FF_VS_LIGHT + i * FF_LIGHT_STRIDE];

        memcpy(cst[FF_LIGHT_DIFFUSE], &l->Diffuse, 16);
        memcpy(cst[FF_LIGHT_SPECULAR], &l->Specular, 16);
        memcpy(cst[FF_LIGHT_AMBIENT], &l->Ambient, 16);

        float pos[4] = {l->Position.x, l->Position.y, l->Position.z, 1},
              dir[4] = {-l->Direction.x, -l->Direction.y, -l->Direction.z, 0};

        transform_vec(cst[FF_LIGHT_POSITION], pos, &s->view);
        transform_vec(cst[FF_LIGHT_DIRECTION], dir, &s->view);
        normalize_vec(cst[FF_LIGHT_DIRECTION]);

        p = cst[FF_LIGHT_ATTEN];
        p[0] = l->Range;
        p[1] = l->Attenuation0;
        p[2] = l->Attenuation1;
        p[3] = l->Attenuation2;

        p = cst[FF_LIGHT_SPOT];
        p[0] = vkd_cosf(l->Phi * 0.5f);
        float delta = vkd_cosf(l->Theta * 0.5f) - p[0];
        p[1] = delta ? 1.0f / delta : 1e20f;
        p[2] = l->Falloff;
    }
}
static void ff_ps_constants(struct vkd_device *d)
{
    struct vkd_state *s = &d->st;

    memset(d->ffps.f, 0, FF_PS_CONSTS * sizeof d->ffps.f[0]);
    color_float(d->ffps.f[FF_PS_TFACTOR], s->rs[D3DRS_TEXTUREFACTOR]);

    for (UINT i = 0; i < 8; i++) {
        color_float(d->ffps.f[FF_PS_STAGE_CONST + i], s->tss[i][D3DTSS_CONSTANT]);

        float *bump = d->ffps.f[FF_PS_BUMP + i];

        bump[0] = vkd_f(s->tss[i][D3DTSS_BUMPENVMAT00]);
        bump[1] = vkd_f(s->tss[i][D3DTSS_BUMPENVMAT01]);

        bump[2] = vkd_f(s->tss[i][D3DTSS_BUMPENVMAT10]);
        bump[3] = vkd_f(s->tss[i][D3DTSS_BUMPENVMAT11]);

        d->ffps.f[FF_PS_LUM + i][0] = vkd_f(s->tss[i][D3DTSS_BUMPENVLSCALE]);
        d->ffps.f[FF_PS_LUM + i][1] = vkd_f(s->tss[i][D3DTSS_BUMPENVLOFFSET]);
    }
}
static VkFilter filter_mode(DWORD f)
{
    return f == D3DTEXF_POINT || f == D3DTEXF_NONE ? VK_FILTER_NEAREST : VK_FILTER_LINEAR;
}
static VkSamplerAddressMode address_mode(DWORD a)
{
    switch (a) {
    case D3DTADDRESS_MIRROR:
        return VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    case D3DTADDRESS_CLAMP:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;

    case D3DTADDRESS_BORDER:
        return VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
    case D3DTADDRESS_MIRRORONCE:
        return vkd.mirror_clamp ? VK_SAMPLER_ADDRESS_MODE_MIRROR_CLAMP_TO_EDGE : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    default:
        return VK_SAMPLER_ADDRESS_MODE_REPEAT;
    }
}
static VkSampler draw_sampler(struct vkd_device *d, UINT i, int compare)
{
    struct {
        DWORD ss[14];
        int cmp;
    } key;
    memcpy(key.ss, d->st.ss[i], sizeof key.ss);
    key.cmp = compare;

    VkSampler *smp = vkd_map_get(&d->samplers, &key, sizeof key);
    if (smp)
        return *smp;

    DWORD *s = key.ss;
    VkSamplerCreateInfo ci = {VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};

    ci.magFilter = filter_mode(s[D3DSAMP_MAGFILTER]);
    ci.minFilter = filter_mode(s[D3DSAMP_MINFILTER]);

    ci.mipmapMode =
        s[D3DSAMP_MIPFILTER] == D3DTEXF_LINEAR ? VK_SAMPLER_MIPMAP_MODE_LINEAR : VK_SAMPLER_MIPMAP_MODE_NEAREST;

    ci.addressModeU = address_mode(s[D3DSAMP_ADDRESSU]);
    ci.addressModeV = address_mode(s[D3DSAMP_ADDRESSV]);
    ci.addressModeW = address_mode(s[D3DSAMP_ADDRESSW]);

    ci.mipLodBias = vkd_f(s[D3DSAMP_MIPMAPLODBIAS]);
    ci.mipLodBias =
        VKP_MAX(-vkd.props.limits.maxSamplerLodBias, VKP_MIN(ci.mipLodBias, vkd.props.limits.maxSamplerLodBias));

    ci.minLod = s[D3DSAMP_MAXMIPLEVEL];
    ci.maxLod = s[D3DSAMP_MIPFILTER] == D3DTEXF_NONE ? ci.minLod : VK_LOD_CLAMP_NONE;

    ci.anisotropyEnable =
        vkd.anisotropy && (s[D3DSAMP_MINFILTER] == D3DTEXF_ANISOTROPIC || s[D3DSAMP_MAGFILTER] == D3DTEXF_ANISOTROPIC);

    ci.maxAnisotropy =
        VKP_MAX(1.0f, VKP_MIN(vkd.max_aniso, d->option_aniso >= 0 ? (DWORD)d->option_aniso : s[D3DSAMP_MAXANISOTROPY]));

    ci.compareEnable = compare;
    ci.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;

    DWORD border = s[D3DSAMP_BORDERCOLOR];

    ci.borderColor = border == 0xffffffffu   ? VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE
                     : border == 0xff000000u ? VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK
                                             : VK_BORDER_COLOR_FLOAT_TRANSPARENT_BLACK;

    VkSamplerCustomBorderColorCreateInfoEXT bc = {VK_STRUCTURE_TYPE_SAMPLER_CUSTOM_BORDER_COLOR_CREATE_INFO_EXT};

    if (vkd.custom_border && border && border != 0xff000000u && border != 0xffffffffu) {
        color_float(bc.customBorderColor.float32, border);
        bc.format = VK_FORMAT_UNDEFINED;
        ci.borderColor = VK_BORDER_COLOR_FLOAT_CUSTOM_EXT;
        ci.pNext = &bc;
    }
    smp = vkp_alloc(sizeof *smp);
    if (!smp)
        return 0;

    if (vkCreateSampler(vkd.dev, &ci, 0, smp) != VK_SUCCESS) {
        vkp_free(smp);
        return 0;
    }
    vkd_map_put(&d->samplers, &key, sizeof key, smp);

    if (vkd_map_get(&d->samplers, &key, sizeof key) != smp) {
        vkDestroySampler(vkd.dev, *smp, 0);
        vkp_free(smp);
        return 0;
    }
    return *smp;
}
static struct vkd_image *draw_dummy(struct vkd_device *d, UINT kind, int white)
{
    struct vkd_image **p = white ? &d->white[kind] : &d->dummy[kind];
    if (*p)
        return *p;

    *p = vkd_image_create(d, vkd_format(D3DFMT_A8R8G8B8), kind == DXSO_TEX_3D ? VK_IMAGE_TYPE_3D : VK_IMAGE_TYPE_2D, 1,
                          1, 1, 1, kind == DXSO_TEX_CUBE ? 6 : 1, VK_SAMPLE_COUNT_1_BIT, kind == DXSO_TEX_CUBE, 0);

    if (!*p)
        return 0;

    DWORD pixel = white ? 0xffffffffu : 0xff000000u;

    for (UINT i = 0; i < (*p)->layers; i++)
        vkd_image_write(d, *p, 0, i, 0, 0, 0, 1, 1, 1, (const uint8_t *)&pixel, 4, 4);

    return *p;
}
static UINT texture_kind(struct vkd_texture *t)
{
    return t && t->r.type == D3DRTYPE_CUBETEXTURE     ? DXSO_TEX_CUBE
           : t && t->r.type == D3DRTYPE_VOLUMETEXTURE ? DXSO_TEX_3D
                                                      : DXSO_TEX_2D;
}
static VkFormat decl_format(BYTE type)
{
    static const VkFormat f[] = {VK_FORMAT_R32_SFLOAT,
                                 VK_FORMAT_R32G32_SFLOAT,
                                 VK_FORMAT_R32G32B32_SFLOAT,
                                 VK_FORMAT_R32G32B32A32_SFLOAT,
                                 VK_FORMAT_B8G8R8A8_UNORM,
                                 VK_FORMAT_R8G8B8A8_USCALED,
                                 VK_FORMAT_R16G16_SSCALED,
                                 VK_FORMAT_R16G16B16A16_SSCALED,
                                 VK_FORMAT_R8G8B8A8_UNORM,
                                 VK_FORMAT_R16G16_SNORM,
                                 VK_FORMAT_R16G16B16A16_SNORM,
                                 VK_FORMAT_R16G16_UNORM,
                                 VK_FORMAT_R16G16B16A16_UNORM,
                                 VK_FORMAT_A2B10G10R10_USCALED_PACK32,
                                 VK_FORMAT_A2B10G10R10_SNORM_PACK32,
                                 VK_FORMAT_R16G16_SFLOAT,
                                 VK_FORMAT_R16G16B16A16_SFLOAT};

    return type < sizeof f / sizeof f[0] ? f[type] : VK_FORMAT_UNDEFINED;
}
static VkBlendFactor blend_factor(DWORD f)
{
    static const VkBlendFactor b[] = {VK_BLEND_FACTOR_ZERO,
                                      VK_BLEND_FACTOR_ZERO,
                                      VK_BLEND_FACTOR_ONE,
                                      VK_BLEND_FACTOR_SRC_COLOR,
                                      VK_BLEND_FACTOR_ONE_MINUS_SRC_COLOR,
                                      VK_BLEND_FACTOR_SRC_ALPHA,
                                      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                      VK_BLEND_FACTOR_DST_ALPHA,
                                      VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA,
                                      VK_BLEND_FACTOR_DST_COLOR,
                                      VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR,
                                      VK_BLEND_FACTOR_SRC_ALPHA_SATURATE,
                                      VK_BLEND_FACTOR_SRC_ALPHA,
                                      VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
                                      VK_BLEND_FACTOR_CONSTANT_COLOR,
                                      VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_COLOR,
                                      VK_BLEND_FACTOR_SRC1_COLOR,
                                      VK_BLEND_FACTOR_ONE_MINUS_SRC1_COLOR};

    return f < sizeof b / sizeof b[0] ? b[f] : VK_BLEND_FACTOR_ONE;
}
static VkBlendOp blend_op(DWORD op)
{
    return op >= D3DBLENDOP_ADD && op <= D3DBLENDOP_MAX ? (VkBlendOp)(op - 1) : VK_BLEND_OP_ADD;
}
/* One attachment's blend equation from the render states. BOTH(INV)SRCALPHA
 * also sets the destination factor, and a target without alpha (X8R8G8B8)
 * blends with destination alpha 1. */
static void blend_equation(const DWORD *rs, int no_alpha, VkColorBlendEquationEXT *eq)
{
    int separate = !!rs[D3DRS_SEPARATEALPHABLENDENABLE];
    VkBlendFactor *factors[2][2] = {{&eq->srcColorBlendFactor, &eq->dstColorBlendFactor},
                                    {&eq->srcAlphaBlendFactor, &eq->dstAlphaBlendFactor}};
    for (UINT a = 0; a < 2; a++) {
        DWORD src = rs[a && separate ? D3DRS_SRCBLENDALPHA : D3DRS_SRCBLEND];
        DWORD dst = rs[a && separate ? D3DRS_DESTBLENDALPHA : D3DRS_DESTBLEND];
        VkBlendFactor *s = factors[a][0], *d = factors[a][1];
        *s = blend_factor(src);
        *d = blend_factor(dst);
        if (src == D3DBLEND_BOTHSRCALPHA) {
            *s = VK_BLEND_FACTOR_SRC_ALPHA;
            *d = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        } else if (src == D3DBLEND_BOTHINVSRCALPHA) {
            *s = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            *d = VK_BLEND_FACTOR_SRC_ALPHA;
        }
        if (no_alpha)
            for (UINT k = 0; k < 2; k++) {
                VkBlendFactor *f = factors[a][k];
                if (*f == VK_BLEND_FACTOR_DST_ALPHA)
                    *f = VK_BLEND_FACTOR_ONE;
                else if (*f == VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA)
                    *f = VK_BLEND_FACTOR_ZERO;
            }
    }
    eq->colorBlendOp = blend_op(rs[D3DRS_BLENDOP]);
    eq->alphaBlendOp = blend_op(rs[separate ? D3DRS_BLENDOPALPHA : D3DRS_BLENDOP]);
}
static int target_no_alpha(struct vkd_device *d, UINT i)
{
    return d->rt[i] && vkd_format(d->rt[i]->r.format)->no_alpha;
}
static VkPrimitiveTopology draw_topology(D3DPRIMITIVETYPE t)
{
    static const VkPrimitiveTopology p[] = {VK_PRIMITIVE_TOPOLOGY_POINT_LIST,    VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
                                            VK_PRIMITIVE_TOPOLOGY_LINE_LIST,     VK_PRIMITIVE_TOPOLOGY_LINE_STRIP,
                                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
                                            VK_PRIMITIVE_TOPOLOGY_TRIANGLE_FAN};
    return p[t];
}

/* Dynamic state removes application render states and input layouts from the
 * pipeline cache key. On devices missing EDS3/input extensions, include those
 * state groups in the key and use ordinary Vulkan pipeline descriptions. */
struct draw_pipeline_key {
    VkShaderModule vs, ps;
    UINT vs_id, ps_id, topology_class;

    VkFormat color[4], depth, stencil;

    UINT ncolor, samples;

    DWORD fallback[15];

    VkVertexInputBindingDescription binding[17];

    VkVertexInputAttributeDescription attr[16];

    UINT nbind, nattr;
};

static VkPipeline draw_pipeline(struct vkd_device *d, const struct dxso_info *vi, VkShaderModule vs, VkShaderModule ps,
                                UINT vs_id, UINT ps_id, D3DPRIMITIVETYPE topology,
                                VkVertexInputBindingDescription *bindings, UINT nb,
                                VkVertexInputAttributeDescription *attrs, UINT na)
{
    (void)vi;

    struct draw_pipeline_key key;
    memset(&key, 0, sizeof key);
    key.vs = vs;
    key.ps = ps;
    key.samples = 1;
    key.vs_id = vs_id;
    key.ps_id = ps_id;
    key.topology_class = topology == D3DPT_POINTLIST ? 0 : topology <= D3DPT_LINESTRIP ? 1 : 2;

    for (UINT i = 0; i < 4; i++)
        if (d->rt[i]) {
            const struct vkd_format *f = vkd_format(d->rt[i]->r.format);

            key.color[i] = d->st.rs[D3DRS_SRGBWRITEENABLE] && f->vk_srgb ? f->vk_srgb : vkd_attach_format(f);
            key.ncolor = i + 1;
            key.samples = vkd_surface_image(d->rt[i])->samples;
        }
    if (d->ds) {
        key.depth = vkd_attach_format(vkd_format(d->ds->r.format));
        if (vkd_format(d->ds->r.format)->stencil)
            key.stencil = key.depth;
    }
    if (!vkd.vi_dynamic) {
        key.nbind = nb;
        key.nattr = na;
        memcpy(key.binding, bindings, nb * sizeof *bindings);
        memcpy(key.attr, attrs, na * sizeof *attrs);
    }
    DWORD *rs = d->st.rs;

    if (!vkd.eds3_blend) {
        DWORD names[] = {D3DRS_ALPHABLENDENABLE,         D3DRS_SRCBLEND,      D3DRS_DESTBLEND,      D3DRS_BLENDOP,
                         D3DRS_SEPARATEALPHABLENDENABLE, D3DRS_SRCBLENDALPHA, D3DRS_DESTBLENDALPHA, D3DRS_BLENDOPALPHA,
                         D3DRS_COLORWRITEENABLE};

        for (UINT i = 0; i < 9; i++)
            key.fallback[i] = rs[names[i]];
    }
    if (!vkd.eds3_poly)
        key.fallback[9] = rs[D3DRS_FILLMODE];
    if (!vkd.eds3_mask)
        key.fallback[10] = rs[D3DRS_MULTISAMPLEMASK];
    if (!vkd.eds3_blend) {
        key.fallback[11] = rs[D3DRS_COLORWRITEENABLE1];
        key.fallback[12] = rs[D3DRS_COLORWRITEENABLE2];
        key.fallback[13] = rs[D3DRS_COLORWRITEENABLE3];
        /* A8R8G8B8 and X8R8G8B8 share a Vulkan format but not their blend factors. */
        for (UINT i = 0; i < 4; i++)
            key.fallback[14] |= (DWORD)target_no_alpha(d, i) << i;
    }

    /* with dynamic vertex input the binding arrays stay zero and are not hashed */
    uint32_t key_size = vkd.vi_dynamic ? (uint32_t)offsetof(struct draw_pipeline_key, binding) : (uint32_t)sizeof key;
    struct vkd_pipeline *cached = vkd_map_get(&d->pipelines, &key, key_size);
    if (cached && cached->pipe)
        return cached->pipe;

    VkPipelineShaderStageCreateInfo st[2] = {{VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO},
                                             {VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO}};

    st[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    st[0].module = vs;
    st[0].pName = "main";

    st[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    st[1].module = ps;
    st[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo input = {VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO};

    if (!vkd.vi_dynamic) {
        input.vertexBindingDescriptionCount = nb;
        input.pVertexBindingDescriptions = bindings;
        input.vertexAttributeDescriptionCount = na;
        input.pVertexAttributeDescriptions = attrs;
    }
    VkPipelineInputAssemblyStateCreateInfo ia = {VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO};
    ia.topology = key.topology_class == 0   ? VK_PRIMITIVE_TOPOLOGY_POINT_LIST
                  : key.topology_class == 1 ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST
                                            : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo vp = {VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO};
    vp.viewportCount = vp.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo raster = {VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO};
    raster.lineWidth = 1;

    raster.polygonMode = rs[D3DRS_FILLMODE] == D3DFILL_WIREFRAME && vkd.fill_non_solid ? VK_POLYGON_MODE_LINE
                         : rs[D3DRS_FILLMODE] == D3DFILL_POINT && vkd.fill_non_solid   ? VK_POLYGON_MODE_POINT
                                                                                       : VK_POLYGON_MODE_FILL;

    VkPipelineMultisampleStateCreateInfo ms = {VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO};
    ms.rasterizationSamples = key.samples;
    VkSampleMask sample_mask = rs[D3DRS_MULTISAMPLEMASK];
    ms.pSampleMask = &sample_mask;

    VkPipelineDepthStencilStateCreateInfo depth = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};

    VkPipelineColorBlendAttachmentState blend[4];
    memset(blend, 0, sizeof blend);

    for (UINT i = 0; i < key.ncolor; i++) {
        VkColorBlendEquationEXT eq;
        blend_equation(rs, target_no_alpha(d, i), &eq);
        blend[i].blendEnable = !!rs[D3DRS_ALPHABLENDENABLE];
        blend[i].srcColorBlendFactor = eq.srcColorBlendFactor;
        blend[i].dstColorBlendFactor = eq.dstColorBlendFactor;
        blend[i].colorBlendOp = eq.colorBlendOp;
        blend[i].srcAlphaBlendFactor = eq.srcAlphaBlendFactor;
        blend[i].dstAlphaBlendFactor = eq.dstAlphaBlendFactor;
        blend[i].alphaBlendOp = eq.alphaBlendOp;
        DWORD masks[] = {D3DRS_COLORWRITEENABLE, D3DRS_COLORWRITEENABLE1, D3DRS_COLORWRITEENABLE2,
                         D3DRS_COLORWRITEENABLE3};
        blend[i].colorWriteMask = rs[masks[i]];
    }
    VkPipelineColorBlendStateCreateInfo cb = {VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO};
    cb.attachmentCount = key.ncolor;
    cb.pAttachments = blend;

    VkDynamicState dyn[40] = {VK_DYNAMIC_STATE_VIEWPORT,
                              VK_DYNAMIC_STATE_SCISSOR,
                              VK_DYNAMIC_STATE_CULL_MODE,
                              VK_DYNAMIC_STATE_FRONT_FACE,
                              VK_DYNAMIC_STATE_PRIMITIVE_TOPOLOGY,
                              VK_DYNAMIC_STATE_DEPTH_TEST_ENABLE,
                              VK_DYNAMIC_STATE_DEPTH_WRITE_ENABLE,
                              VK_DYNAMIC_STATE_DEPTH_COMPARE_OP,
                              VK_DYNAMIC_STATE_DEPTH_BOUNDS_TEST_ENABLE,
                              VK_DYNAMIC_STATE_STENCIL_TEST_ENABLE,
                              VK_DYNAMIC_STATE_STENCIL_OP,
                              VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK,
                              VK_DYNAMIC_STATE_STENCIL_WRITE_MASK,
                              VK_DYNAMIC_STATE_STENCIL_REFERENCE,
                              VK_DYNAMIC_STATE_DEPTH_BIAS,
                              VK_DYNAMIC_STATE_DEPTH_BIAS_ENABLE,
                              VK_DYNAMIC_STATE_RASTERIZER_DISCARD_ENABLE,
                              VK_DYNAMIC_STATE_PRIMITIVE_RESTART_ENABLE,
                              VK_DYNAMIC_STATE_BLEND_CONSTANTS};

    UINT nd = 19;

    if (vkd.eds3_blend) {
        dyn[nd++] = VK_DYNAMIC_STATE_COLOR_BLEND_ENABLE_EXT;
        dyn[nd++] = VK_DYNAMIC_STATE_COLOR_BLEND_EQUATION_EXT;
        dyn[nd++] = VK_DYNAMIC_STATE_COLOR_WRITE_MASK_EXT;
    }
    if (vkd.eds3_poly)
        dyn[nd++] = VK_DYNAMIC_STATE_POLYGON_MODE_EXT;

    if (vkd.vi_dynamic)
        dyn[nd++] = VK_DYNAMIC_STATE_VERTEX_INPUT_EXT;
    if (vkd.eds3_mask)
        dyn[nd++] = VK_DYNAMIC_STATE_SAMPLE_MASK_EXT;

    VkPipelineDynamicStateCreateInfo dynamic = {VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO};
    dynamic.dynamicStateCount = nd;
    dynamic.pDynamicStates = dyn;

    VkPipelineRenderingCreateInfo ri = {VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
    ri.colorAttachmentCount = key.ncolor;
    ri.pColorAttachmentFormats = key.color;
    ri.depthAttachmentFormat = key.depth;
    ri.stencilAttachmentFormat = key.stencil;

    VkGraphicsPipelineCreateInfo ci = {VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO};
    ci.pNext = &ri;
    ci.stageCount = 2;
    ci.pStages = st;

    ci.pVertexInputState = &input;
    ci.pInputAssemblyState = &ia;
    ci.pViewportState = &vp;
    ci.pRasterizationState = &raster;
    ci.pMultisampleState = &ms;

    ci.pDepthStencilState = &depth;
    ci.pColorBlendState = &cb;
    ci.pDynamicState = &dynamic;
    ci.layout = d->layout;

    VkPipeline pipe;

    if (vkCreateGraphicsPipelines(vkd.dev, d->pcache, 1, &ci, 0, &pipe) != VK_SUCCESS)
        return 0;

    struct vkd_pipeline *p = cached ? cached : vkp_alloc(sizeof *p);
    if (!p) {
        vkDestroyPipeline(vkd.dev, pipe, 0);
        return 0;
    }
    p->pipe = pipe;
    p->vs_id = vs_id;
    p->ps_id = ps_id;

    vkd_map_put(&d->pipelines, &key, key_size, p);

    if (vkd_map_get(&d->pipelines, &key, key_size) != p) {
        vkDestroyPipeline(vkd.dev, pipe, 0);
        vkp_free(p);
        return 0;
    }
    return pipe;
}
static VkCompareOp compare_op(DWORD op) { return op >= 1 && op <= 8 ? (VkCompareOp)(op - 1) : VK_COMPARE_OP_ALWAYS; }
static VkStencilOp stencil_op(DWORD op)
{
    static const VkStencilOp map[] = {VK_STENCIL_OP_KEEP,
                                      VK_STENCIL_OP_KEEP,
                                      VK_STENCIL_OP_ZERO,
                                      VK_STENCIL_OP_REPLACE,
                                      VK_STENCIL_OP_INCREMENT_AND_CLAMP,
                                      VK_STENCIL_OP_DECREMENT_AND_CLAMP,
                                      VK_STENCIL_OP_INVERT,
                                      VK_STENCIL_OP_INCREMENT_AND_WRAP,
                                      VK_STENCIL_OP_DECREMENT_AND_WRAP};
    return op <= 8 ? map[op] : VK_STENCIL_OP_KEEP;
}
static void draw_dynamic(struct vkd_device *d, D3DPRIMITIVETYPE topology)
{
    DWORD *s = d->st.rs;
    VkCommandBuffer cmd = vkd_cmd(d);
    D3DVIEWPORT9 *vp = &d->st.viewport;

    VkViewport view = {vp->X + 0.5f, vp->Y + vp->Height + 0.5f, vp->Width, -(float)vp->Height, vp->MinZ, vp->MaxZ};

    vkCmdSetViewport(cmd, 0, 1, &view);

    RECT r = s[D3DRS_SCISSORTESTENABLE] ? d->st.scissor : (RECT){0, 0, d->rt_extent.width, d->rt_extent.height};

    r.left = VKP_MAX(0, r.left);
    r.top = VKP_MAX(0, r.top);
    r.right = VKP_MIN((LONG)d->rt_extent.width, r.right);
    r.bottom = VKP_MIN((LONG)d->rt_extent.height, r.bottom);

    VkRect2D scissor = {{r.left, r.top}, {VKP_MAX(0, r.right - r.left), VKP_MAX(0, r.bottom - r.top)}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    vkCmdSetCullMode(cmd, s[D3DRS_CULLMODE] == D3DCULL_NONE ? VK_CULL_MODE_NONE
                          : s[D3DRS_CULLMODE] == D3DCULL_CW ? VK_CULL_MODE_FRONT_BIT
                                                            : VK_CULL_MODE_BACK_BIT);

    vkCmdSetFrontFace(cmd, VK_FRONT_FACE_CLOCKWISE);
    vkCmdSetPrimitiveTopology(cmd, draw_topology(topology));

    vkCmdSetDepthTestEnable(cmd, d->ds && s[D3DRS_ZENABLE]);
    vkCmdSetDepthWriteEnable(cmd, d->ds && s[D3DRS_ZWRITEENABLE]);
    vkCmdSetDepthCompareOp(cmd, compare_op(s[D3DRS_ZFUNC]));

    vkCmdSetDepthBoundsTestEnable(cmd, VK_FALSE);
    vkCmdSetStencilTestEnable(cmd, d->ds && s[D3DRS_STENCILENABLE]);

    for (UINT i = 0; i < 2; i++) {
        int ccw = i && s[D3DRS_TWOSIDEDSTENCILMODE];
        VkStencilFaceFlags face = i ? VK_STENCIL_FACE_BACK_BIT : VK_STENCIL_FACE_FRONT_BIT;

        vkCmdSetStencilOp(cmd, face, stencil_op(s[ccw ? D3DRS_CCW_STENCILFAIL : D3DRS_STENCILFAIL]),
                          stencil_op(s[ccw ? D3DRS_CCW_STENCILPASS : D3DRS_STENCILPASS]),
                          stencil_op(s[ccw ? D3DRS_CCW_STENCILZFAIL : D3DRS_STENCILZFAIL]),
                          compare_op(s[ccw ? D3DRS_CCW_STENCILFUNC : D3DRS_STENCILFUNC]));
    }
    vkCmdSetStencilCompareMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, s[D3DRS_STENCILMASK]);
    vkCmdSetStencilWriteMask(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, s[D3DRS_STENCILWRITEMASK]);
    vkCmdSetStencilReference(cmd, VK_STENCIL_FACE_FRONT_AND_BACK, s[D3DRS_STENCILREF]);

    float bias = vkd_f(s[D3DRS_DEPTHBIAS]);
    if (d->ds)
        bias *= vkd_format(d->ds->r.format)->depth == 16 ? 65535.0f : 16777215.0f;

    vkCmdSetDepthBias(cmd, bias, 0, vkd_f(s[D3DRS_SLOPESCALEDEPTHBIAS]));
    vkCmdSetDepthBiasEnable(cmd, bias != 0 || s[D3DRS_SLOPESCALEDEPTHBIAS]);

    vkCmdSetRasterizerDiscardEnable(cmd, VK_FALSE);
    vkCmdSetPrimitiveRestartEnable(cmd, VK_FALSE);

    float constant[4];
    color_float(constant, s[D3DRS_BLENDFACTOR]);
    vkCmdSetBlendConstants(cmd, constant);

    if (vkd.eds3_poly)
        vkCmdSetPolygonModeEXT(cmd, s[D3DRS_FILLMODE] == D3DFILL_WIREFRAME && vkd.fill_non_solid ? VK_POLYGON_MODE_LINE
                                    : s[D3DRS_FILLMODE] == D3DFILL_POINT && vkd.fill_non_solid ? VK_POLYGON_MODE_POINT
                                                                                               : VK_POLYGON_MODE_FILL);

    if (vkd.eds3_blend) {
        VkBool32 enable[4];
        VkColorBlendEquationEXT eq[4];
        VkColorComponentFlags mask[4];

        DWORD cm[] = {D3DRS_COLORWRITEENABLE, D3DRS_COLORWRITEENABLE1, D3DRS_COLORWRITEENABLE2,
                      D3DRS_COLORWRITEENABLE3};

        for (UINT i = 0; i < 4; i++) {
            enable[i] = !!s[D3DRS_ALPHABLENDENABLE];
            mask[i] = s[cm[i]];
            blend_equation(s, target_no_alpha(d, i), &eq[i]);
        }
        UINT n = 0;
        for (UINT i = 0; i < 4; i++)
            if (d->rt[i])
                n = i + 1;

        if (n) {
            vkCmdSetColorBlendEnableEXT(cmd, 0, n, enable);
            vkCmdSetColorBlendEquationEXT(cmd, 0, n, eq);
            vkCmdSetColorWriteMaskEXT(cmd, 0, n, mask);
        }
    }
}
static int draw_begin(struct vkd_device *d)
{
    if (d->rendering)
        return 1;

    VkRenderingAttachmentInfo ca[4], ds = {VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO};
    memset(ca, 0, sizeof ca);

    UINT n = 0;
    VkExtent2D extent = {0, 0};

    for (UINT i = 0; i < 4; i++) {
        ca[i].sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO;
        ca[i].imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        ca[i].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        ca[i].storeOp = VK_ATTACHMENT_STORE_OP_STORE;

        if (d->rt[i]) {
            n = i + 1;
            ca[i].imageView = vkd_surface_rtv(d->rt[i], !!d->st.rs[D3DRS_SRGBWRITEENABLE]);
            if (!ca[i].imageView)
                return 0;

            if (!extent.width)
                extent = (VkExtent2D){d->rt[i]->w, d->rt[i]->h};
            vkd_surface_image(d->rt[i])->use_seq = d->seq;
        }
    }
    if (d->ds) {
        ds.imageView = vkd_surface_rtv(d->ds, 0);
        ds.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        ds.loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        ds.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        vkd_surface_image(d->ds)->use_seq = d->seq;

        if (!extent.width)
            extent = (VkExtent2D){d->ds->w, d->ds->h};
    }
    VkRenderingInfo ri = {VK_STRUCTURE_TYPE_RENDERING_INFO};
    ri.renderArea.extent = extent;
    ri.layerCount = 1;
    ri.colorAttachmentCount = n;
    ri.pColorAttachments = ca;

    if (d->ds) {
        ri.pDepthAttachment = &ds;
        if (vkd_format(d->ds->r.format)->stencil)
            ri.pStencilAttachment = &ds;
    }
    if (!extent.width || !extent.height)
        return 0;

    vkCmdBeginRendering(vkd_cmd(d), &ri);
    d->rendering = 1;
    d->rt_extent = extent;
    vkd_queries_pass_begin(d);
    return 1;
}
/* Source of a constant block: fixed-function constants, application constants,
 * or application constants clamped to [-1, 1] for ps 1.x. */
enum { UBO_FF = 1, UBO_APP, UBO_APP_PS1 };

/* The block written earlier in this submission still holds what the shader reads. */
static int ubo_current(struct vkd_device *d, const struct vkd_ubo *u, const struct dxso_info *info, int src,
                       uint32_t inputs)
{
    return u->seq == d->seq && u->src == src && u->floats >= info->floats_read && u->tail >= info->tail_read &&
           !(d->dirty & inputs);
}
/* A constant block in the upload ring. The descriptor covers the whole block,
 * but only registers [0, floats_read) and, when read, the tail are written, so
 * the rest of the block belongs to later allocations. Mapped memory is
 * write-combined: values are built locally and written once. */
static int draw_vs_constants(struct vkd_device *d, const struct vkd_shader *vs, int src, VkDeviceSize align)
{
    struct vkd_state *s = &d->st;
    uint32_t floats = vs->info.floats_read;
    int tail = vs->info.tail_read;
    struct vkd_buf b;
    VkDeviceSize size = tail ? sizeof(struct dxso_vs_consts) : VKP_MAX(16, floats * 16);

    if (!vkd_ring_span(d, size, sizeof(struct dxso_vs_consts), align, &b))
        return 0;
    struct dxso_vs_consts *c = (struct dxso_vs_consts *)b.map;
    memcpy(c->f, src == UBO_FF ? d->ffvs.f : s->vsf, floats * 16);
    if (tail) {
        uint32_t bits[4] = {0};
        for (UINT i = 0; src != UBO_FF && i < 16; i++)
            bits[0] |= (uint32_t)!!s->vsb[i] << i;
        memcpy(c->i, src == UBO_FF ? d->ffvs.i : s->vsi, sizeof c->i);
        memcpy(c->b, bits, sizeof bits);
    }
    d->vs_ubo = (struct vkd_ubo){b.buf, b.off, d->seq, floats, tail, src};
    return 1;
}
static int draw_ps_constants(struct vkd_device *d, const struct vkd_shader *ps, int src, VkDeviceSize align)
{
    struct vkd_state *s = &d->st;
    uint32_t floats = ps->info.floats_read;
    int tail = ps->info.tail_read;
    struct vkd_buf b;
    VkDeviceSize size = tail ? sizeof(struct dxso_ps_consts) : VKP_MAX(16, floats * 16);

    if (!vkd_ring_span(d, size, sizeof(struct dxso_ps_consts), align, &b))
        return 0;
    struct dxso_ps_consts *c = (struct dxso_ps_consts *)b.map;
    const float(*f)[4] = src == UBO_FF ? d->ffps.f : s->psf;
    if (src == UBO_APP_PS1)
        for (UINT i = 0; i < floats; i++) {
            float v[4];
            for (UINT j = 0; j < 4; j++)
                v[j] = VKP_MAX(-1.0f, VKP_MIN(1.0f, f[i][j]));
            memcpy(c->f[i], v, sizeof v);
        }
    else
        memcpy(c->f, f, floats * 16);
    if (tail) {
        uint32_t bits[4] = {0};
        for (UINT i = 0; src != UBO_FF && i < 16; i++)
            bits[0] |= (uint32_t)!!s->psb[i] << i;
        memcpy(c->i, src == UBO_FF ? d->ffps.i : s->psi, sizeof c->i);
        memcpy(c->b, bits, sizeof bits);
        for (UINT i = 0; i < 8; i++) {
            memcpy(c->bump[i], d->ffps.f[FF_PS_BUMP + i], 16);
            memcpy(c->lum[i], d->ffps.f[FF_PS_LUM + i], 16);
        }
    }
    d->ps_ubo = (struct vkd_ubo){b.buf, b.off, d->seq, floats, tail, src};
    return 1;
}
static int draw_prepare(struct vkd_device *d, D3DPRIMITIVETYPE topology, const struct vkd_buf *up, UINT up_stride)
{
    struct vkd_state *s = &d->st;
    if (!d->rt[0])
        return 0;
    VkSampleCountFlagBits attachment_samples = vkd_surface_image(d->rt[0])->samples;
    for (UINT i = 1; i < 4; i++)
        if (d->rt[i] && (d->rt[i]->w != d->rt[0]->w || d->rt[i]->h != d->rt[0]->h ||
                         vkd_surface_image(d->rt[i])->samples != attachment_samples))
            return 0;
    if (d->ds &&
        (d->ds->w < d->rt[0]->w || d->ds->h < d->rt[0]->h || vkd_surface_image(d->ds)->samples != attachment_samples))
        return 0;
    if (!s->decl)
        return 0;

    /* Fixed-function shaders are looked up again only when fixed-function state
     * changed, also while shaders are bound, so they are current whenever
     * SetVertexShader(0) or SetPixelShader(0) returns to them. */
    if (!d->ff_vs_cur || !d->ff_ps_cur || (d->dirty & (DIRTY_FF_VS | DIRTY_FF_PS))) {
        struct ff_vs_key fv;
        struct ff_ps_key fp;
        ff_keys(d, &fv, &fp);
        d->ff_vs_cur = ff_shader(d, &fv, sizeof fv, 0);
        d->ff_ps_cur = ff_shader(d, &fp, sizeof fp, 1);
    }
    struct vkd_shader *vs = s->vs ? s->vs : d->ff_vs_cur, *ps = s->ps ? s->ps : d->ff_ps_cur;

    if (!vs || !ps)
        return 0;

    struct dxso_key vk, pk;
    memset(&vk, 0, sizeof vk);
    memset(&pk, 0, sizeof pk);

    vk.flat = pk.flat = s->rs[D3DRS_SHADEMODE] == D3DSHADE_FLAT;
    vk.vs_extra = ps->info.slots_read;

    pk.point_sprite = topology == D3DPT_POINTLIST && s->rs[D3DRS_POINTSPRITEENABLE];

    for (UINT i = 0; i < 20; i++) {
        struct vkd_texture *t = vkd_res_texture(s->tex[i]);
        UINT kind = texture_kind(t);

        int cmp = t && t->fmt->depth && !t->fmt->sample_depth;
        if (i < 16)
            pk.tex[i] = kind | (cmp ? DXSO_TEX_COMPARE : 0);
        else
            vk.tex[i - 16] = kind | (cmp ? DXSO_TEX_COMPARE : 0);

        if (i < 8 && (s->tss[i][D3DTSS_TEXTURETRANSFORMFLAGS] & D3DTTFF_PROJECTED))
            pk.proj[i] = s->tss[i][D3DTSS_TEXTURETRANSFORMFLAGS] & 7;
    }
    VkShaderModule vmod = vkd_shader_module(vs, &vk), pmod = vkd_shader_module(ps, &pk);
    if (!vmod || !pmod)
        return 0;

    /* Constant blocks are written again only when their inputs changed in this
     * submission, and then only with the registers their shaders read. */
    if (d->dirty & DIRTY_FF_VS_CONST)
        ff_vs_constants(d);
    if (d->dirty & DIRTY_FF_PS_CONST)
        ff_ps_constants(d);
    VkDeviceSize alignment = VKP_MAX(16, vkd.props.limits.minUniformBufferOffsetAlignment);

    int vsrc = s->vs ? UBO_APP : UBO_FF;
    if (!ubo_current(d, &d->vs_ubo, &vs->info, vsrc, s->vs ? DIRTY_VS_CONST : DIRTY_FF_VS_CONST) &&
        !draw_vs_constants(d, vs, vsrc, alignment))
        return 0;
    int psrc = !s->ps ? UBO_FF : ps->info.major == 1 ? UBO_APP_PS1 : UBO_APP;
    if (!ubo_current(d, &d->ps_ubo, &ps->info, psrc, s->ps ? DIRTY_PS_CONST : DIRTY_FF_PS_CONST) &&
        !draw_ps_constants(d, ps, psrc, alignment))
        return 0;
    VkDescriptorBufferInfo buffers[2] = {{d->vs_ubo.buf, d->vs_ubo.off, sizeof(struct dxso_vs_consts)},
                                         {d->ps_ubo.buf, d->ps_ubo.off, sizeof(struct dxso_ps_consts)}};

    /* Sampled images, again only when bindings, sampler states or shaders changed. */
    int images_changed = d->images_seq != d->seq || (d->dirty & (DIRTY_SAMPLERS | DIRTY_SHADERS));
    for (UINT i = 0; i < 20; i++) {
        struct vkd_texture *t = vkd_res_texture(s->tex[i]);
        if (!images_changed) {
            /* Lock/Unlock of a bound texture changes no device state */
            if (t && t->autogen_dirty)
                vkd_texture_genmips(t);
            continue;
        }
        UINT si = i < 16 ? i : i - 16;

        UINT declared = (i < 16 ? ps : vs)->info.sampler_dcl[si];
        UINT kind = declared ? declared - 1 : texture_kind(t);

        if (kind > 2)
            return 0;
        int cmp = t && t->fmt->depth && !t->fmt->sample_depth;

        VkDescriptorImageInfo *image = &d->images[i];
        image->sampler = draw_sampler(d, i, cmp);
        image->imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        if (t && texture_kind(t) == kind) {
            if (t->autogen_dirty)
                vkd_texture_genmips(t);
            image->imageView = vkd_texture_view(t, !!s->ss[i][D3DSAMP_SRGBTEXTURE]);
            t->img->use_seq = d->seq;
        } else {
            struct vkd_image *img = draw_dummy(d, kind, i < 16 && !s->ps);
            if (!img)
                return 0;
            image->imageView = img->view;
            img->use_seq = d->seq;
        }
        if (!image->sampler || !image->imageView)
            return 0;
    }
    if (images_changed)
        d->images_seq = d->seq;
    int push_descriptors = images_changed || d->desc_seq != d->seq || memcmp(buffers, d->desc_buffers, sizeof buffers);
    VkVertexInputBindingDescription bindings[17];
    VkVertexInputAttributeDescription attrs[16];
    UINT nb = 0, na = 0;

    VkBuffer vb[17];
    VkDeviceSize offsets[17];
    UINT binding_mask = 0;

    for (UINT reg = 0; reg < 16; reg++)
        if (vs->info.in_mask & (1u << reg)) {
            const D3DVERTEXELEMENT9 *el = 0;

            for (UINT j = 0; j < s->decl->n; j++) {
                const D3DVERTEXELEMENT9 *e = &s->decl->el[j];
                UINT usage = e->Usage == D3DDECLUSAGE_POSITIONT ? D3DDECLUSAGE_POSITION : e->Usage;

                if ((usage == vs->info.in_usage[reg] ||
                     (e->Usage == D3DDECLUSAGE_POSITIONT && vs->info.in_usage[reg] == D3DDECLUSAGE_POSITIONT)) &&
                    e->UsageIndex == vs->info.in_index[reg]) {
                    el = e;
                    break;
                }
            }
            UINT stream = el ? el->Stream : 16;
            struct vkd_buffer *b = stream < 16 ? s->vb[stream] : 0;

            if (stream < 16 && !b && !(up && stream == 0))
                stream = 16;

            if (!(binding_mask & (1u << stream))) {
                binding_mask |= 1u << stream;
                UINT stride = stream == 16 ? 0 : up && stream == 0 ? up_stride : s->vb_stride[stream];

                bindings[nb] = (VkVertexInputBindingDescription){
                    stream, stride,
                    stream < 16 && (s->vb_freq[stream] & D3DSTREAMSOURCE_INSTANCEDATA) ? VK_VERTEX_INPUT_RATE_INSTANCE
                                                                                       : VK_VERTEX_INPUT_RATE_VERTEX};

                if (stream == 16) {
                    vb[nb] = d->zero_vb.buf;
                    offsets[nb] = d->zero_vb.off;

                } else if (up && stream == 0) {
                    vb[nb] = up->buf;
                    offsets[nb] = up->off;

                } else {
                    vkd_buffer_flush(b);
                    vkd_buffer_used(b);
                    vb[nb] = b->cur.buf;
                    offsets[nb] = b->cur.off + s->vb_off[stream];
                }

                nb++;
            }
            VkFormat fmt = el && stream != 16 ? decl_format(el->Type) : VK_FORMAT_R32G32B32A32_SFLOAT;

            if (fmt == VK_FORMAT_UNDEFINED)
                return 0;

            attrs[na++] = (VkVertexInputAttributeDescription){reg, stream, fmt, el && stream != 16 ? el->Offset : 0};
        }
    UINT vs_id = 0, ps_id = 0;
    for (UINT i = 0; i < vs->nvar; i++)
        if (vs->var[i].mod == vmod)
            vs_id = vs->var[i].id;
    for (UINT i = 0; i < ps->nvar; i++)
        if (ps->var[i].mod == pmod)
            ps_id = ps->var[i].id;
    VkPipeline pipe = draw_pipeline(d, &vs->info, vmod, pmod, vs_id, ps_id, topology, bindings, nb, attrs, na);
    if (!pipe)
        return 0;
    if (!draw_begin(d))
        return 0;
    VkCommandBuffer cmd = vkd_cmd(d);

    /* Command buffer state below lasts for the submission (d->seq): it is
     * recorded again only when it differs from what the last draw set. */
    if (pipe != d->bound_pipe) {
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
        d->bound_pipe = pipe;
    }
    if (vkd.vi_dynamic) {
        VkVertexInputBindingDescription2EXT bd[17];
        VkVertexInputAttributeDescription2EXT ad[16];
        memset(bd, 0, nb * sizeof *bd);
        memset(ad, 0, na * sizeof *ad);

        for (UINT i = 0; i < nb; i++) {
            bd[i].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_BINDING_DESCRIPTION_2_EXT;
            bd[i].binding = bindings[i].binding;
            bd[i].stride = bindings[i].stride;
            bd[i].inputRate = bindings[i].inputRate;
            bd[i].divisor = 1;

            UINT stream = bd[i].binding;
            if (stream < 16 && bd[i].inputRate == VK_VERTEX_INPUT_RATE_INSTANCE)
                bd[i].divisor = s->vb_freq[stream] & 0x3fffffffu;
        }
        for (UINT i = 0; i < na; i++) {
            ad[i].sType = VK_STRUCTURE_TYPE_VERTEX_INPUT_ATTRIBUTE_DESCRIPTION_2_EXT;
            ad[i].location = attrs[i].location;
            ad[i].binding = attrs[i].binding;
            ad[i].format = attrs[i].format;
            ad[i].offset = attrs[i].offset;
        }
        if (d->vi_seq != d->seq || nb != d->vi_nbind || na != d->vi_nattr ||
            memcmp(bd, d->vi_bind, nb * sizeof *bd) || memcmp(ad, d->vi_attr, na * sizeof *ad)) {
            vkCmdSetVertexInputEXT(cmd, nb, bd, na, ad);
            memcpy(d->vi_bind, bd, nb * sizeof *bd);
            memcpy(d->vi_attr, ad, na * sizeof *ad);
            d->vi_nbind = nb;
            d->vi_nattr = na;
            d->vi_seq = d->seq;
        }
    }
    if (d->vb_seq != d->seq) {
        memset(d->vb_bound, 0, sizeof d->vb_bound);
        d->vb_seq = d->seq;
    }
    for (UINT i = 0; i < nb; i++) {
        UINT bi = bindings[i].binding;
        if (d->vb_bound[bi] == vb[i] && d->vb_bound_off[bi] == offsets[i])
            continue;
        vkCmdBindVertexBuffers2(cmd, bi, 1, &vb[i], &offsets[i], 0, 0);
        d->vb_bound[bi] = vb[i];
        d->vb_bound_off[bi] = offsets[i];
    }
    if (d->dyn_seq != d->seq || d->dyn_topology != topology ||
        (d->dirty & (DIRTY_VIEWPORT | DIRTY_RASTER | DIRTY_DEPTH | DIRTY_STENCIL | DIRTY_BLEND))) {
        draw_dynamic(d, topology);
        if (vkd.eds3_mask) {
            VkSampleMask mask = s->rs[D3DRS_MULTISAMPLEMASK];
            VkSampleCountFlagBits samples = vkd_surface_image(d->rt[0])->samples;
            vkCmdSetSampleMaskEXT(cmd, samples, &mask);
        }
        d->dyn_seq = d->seq;
        d->dyn_topology = topology;
    }
    if (push_descriptors) {
        VkWriteDescriptorSet writes[22];
        memset(writes, 0, sizeof writes);
        for (UINT i = 0; i < 22; i++) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType =
                i < 2 ? VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            if (i < 2)
                writes[i].pBufferInfo = &buffers[i];
            else
                writes[i].pImageInfo = &d->images[i - 2];
        }
        vkCmdPushDescriptorSetKHR(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, d->layout, 0, 22, writes);
        memcpy(d->desc_buffers, buffers, sizeof buffers);
        d->desc_seq = d->seq;
    }

    struct dxso_push push;
    memset(&push, 0, sizeof push);
    color_float(push.fog_color, s->rs[D3DRS_FOGCOLOR]);

    push.fog_start = vkd_f(s->rs[D3DRS_FOGSTART]);
    push.fog_end = vkd_f(s->rs[D3DRS_FOGEND]);
    push.fog_density = vkd_f(s->rs[D3DRS_FOGDENSITY]);

    if (s->rs[D3DRS_FOGENABLE] && (!s->ps || ps->info.major < 3)) {
        DWORD mode = s->rs[D3DRS_FOGTABLEMODE];
        push.fog_mode = mode == D3DFOG_LINEAR ? DXSO_FOG_LINEAR
                        : mode == D3DFOG_EXP  ? DXSO_FOG_EXP
                        : mode == D3DFOG_EXP2 ? DXSO_FOG_EXP2
                                              : DXSO_FOG_VERTEX;
    }
    push.alpha_ref = s->rs[D3DRS_ALPHAREF] & 255;
    push.alpha_func = s->rs[D3DRS_ALPHATESTENABLE] ? s->rs[D3DRS_ALPHAFUNC] : D3DCMP_ALWAYS;

    push.point_size = vkd_f(s->rs[D3DRS_POINTSIZE]);
    push.point_min = vkd_f(s->rs[D3DRS_POINTSIZE_MIN]);
    push.point_max = vkd_f(s->rs[D3DRS_POINTSIZE_MAX]);

    push.clip_mask = s->rs[D3DRS_CLIPPLANEENABLE];
    memcpy(push.clip, s->clip, sizeof push.clip);

    if (d->push_seq != d->seq || memcmp(&push, &d->push, sizeof push)) {
        vkCmdPushConstants(cmd, d->layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof push,
                           &push);
        memcpy(&d->push, &push, sizeof push);
        d->push_seq = d->seq;
    }

    d->dirty = 0;
    return 1;
}
static UINT primitive_vertices(D3DPRIMITIVETYPE type, UINT n)
{
    switch (type) {
    case D3DPT_POINTLIST:
        return n;
    case D3DPT_LINELIST:
        return n * 2;
    case D3DPT_LINESTRIP:
        return n ? n + 1 : 0;
    case D3DPT_TRIANGLELIST:
        return n * 3;
    case D3DPT_TRIANGLESTRIP:
    case D3DPT_TRIANGLEFAN:
        return n ? n + 2 : 0;
    default:
        return 0;
    }
}
static HRESULT STDMETHODCALLTYPE dev_DrawPrimitive(IDirect3DDevice9 *self, D3DPRIMITIVETYPE t, UINT start, UINT n)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (t < 1 || t > 6 || n > d->caps.MaxPrimitiveCount || !d->in_scene)
        return D3DERR_INVALIDCALL;

    if (!n)
        return D3D_OK;
    VKD_LOCK(d);
    int ok = draw_prepare(d, t, 0, 0);

    if (ok) {
        vkCmdDraw(vkd_cmd(d), primitive_vertices(t, n), 1, start, 0);
        d->draws_in_pass++;
    }
    VKD_UNLOCK(d);
    return ok ? D3D_OK : D3DERR_INVALIDCALL;
}
static HRESULT STDMETHODCALLTYPE dev_DrawIndexedPrimitive(IDirect3DDevice9 *self, D3DPRIMITIVETYPE t, INT base,
                                                          UINT min, UINT vertices, UINT start, UINT n)
{
    struct vkd_device *d = (struct vkd_device *)self;
    struct vkd_buffer *b = d->st.ib;
    UINT count = primitive_vertices(t, n);

    if (t < 2 || t > 6 || n > d->caps.MaxPrimitiveCount || !d->in_scene || !b || !vertices ||
        (uint64_t)(start + count) * (b->r.format == D3DFMT_INDEX32 ? 4 : 2) > b->size)
        return D3DERR_INVALIDCALL;

    if (!n)
        return D3D_OK;
    VKD_LOCK(d);
    vkd_buffer_flush(b);
    int ok = draw_prepare(d, t, 0, 0);

    if (ok) {
        vkd_buffer_used(b);
        vkCmdBindIndexBuffer(vkd_cmd(d), b->cur.buf, b->cur.off,
                             b->r.format == D3DFMT_INDEX32 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);

        UINT inst = (d->st.vb_freq[0] & D3DSTREAMSOURCE_INDEXEDDATA) ? d->st.vb_freq[0] & 0x3fffffffu : 1;

        vkCmdDrawIndexed(vkd_cmd(d), count, inst, start, base, 0);
        d->draws_in_pass++;
    }
    VKD_UNLOCK(d);
    return ok ? D3D_OK : D3DERR_INVALIDCALL;
}
static HRESULT STDMETHODCALLTYPE dev_DrawPrimitiveUP(IDirect3DDevice9 *self, D3DPRIMITIVETYPE t, UINT n,
                                                     const void *data, UINT stride)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (t < 1 || t > 6 || !data || !stride || n > d->caps.MaxPrimitiveCount || !d->in_scene)
        return D3DERR_INVALIDCALL;

    if (!n)
        return D3D_OK;
    VKD_LOCK(d);
    struct vkd_buf up;
    UINT count = primitive_vertices(t, n);

    int ok = vkd_ring(d, (VkDeviceSize)count * stride, 16, &up);
    if (ok) {
        memcpy(up.map, data, (size_t)count * stride);
        ok = draw_prepare(d, t, &up, stride);
    }
    if (ok) {
        vkCmdDraw(vkd_cmd(d), count, 1, 0, 0);
        d->draws_in_pass++;
    }
    vkd_ref_buf(&d->st.vb[0], 0);
    d->st.vb_off[0] = 0;
    d->st.vb_stride[0] = 0;
    d->dirty |= DIRTY_VB | DIRTY_INPUT;

    VKD_UNLOCK(d);
    return ok ? D3D_OK : D3DERR_INVALIDCALL;
}
static HRESULT STDMETHODCALLTYPE dev_DrawIndexedPrimitiveUP(IDirect3DDevice9 *self, D3DPRIMITIVETYPE t, UINT min,
                                                            UINT vertices, UINT n, const void *indices,
                                                            D3DFORMAT format, const void *data, UINT stride)
{
    struct vkd_device *d = (struct vkd_device *)self;
    if (t < 2 || t > 6 || !indices || !data || !stride || !vertices || n > d->caps.MaxPrimitiveCount || !d->in_scene ||
        (format != D3DFMT_INDEX16 && format != D3DFMT_INDEX32))
        return D3DERR_INVALIDCALL;

    if (!n)
        return D3D_OK;
    VKD_LOCK(d);
    struct vkd_buf up, ib;
    UINT count = primitive_vertices(t, n), is = format == D3DFMT_INDEX32 ? 4 : 2;

    int ok =
        vkd_ring(d, (VkDeviceSize)(min + vertices) * stride, 16, &up) && vkd_ring(d, (VkDeviceSize)count * is, 4, &ib);

    if (ok) {
        memcpy(up.map, data, (size_t)(min + vertices) * stride);
        memcpy(ib.map, indices, (size_t)count * is);
        ok = draw_prepare(d, t, &up, stride);
    }
    if (ok) {
        vkCmdBindIndexBuffer(vkd_cmd(d), ib.buf, ib.off, is == 4 ? VK_INDEX_TYPE_UINT32 : VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(vkd_cmd(d), count, 1, 0, 0, 0);
        d->draws_in_pass++;
    }
    vkd_ref_buf(&d->st.vb[0], 0);
    vkd_ref_buf(&d->st.ib, 0);
    d->st.vb_off[0] = d->st.vb_stride[0] = 0;
    d->dirty |= DIRTY_VB | DIRTY_IB | DIRTY_INPUT;

    VKD_UNLOCK(d);
    return ok ? D3D_OK : D3DERR_INVALIDCALL;
}
static void draw_drop_pipeline(void *value, void *ctx)
{
    struct vkd_pipeline *p = value;
    if (p->pipe)
        vkDestroyPipeline(vkd.dev, p->pipe, 0);
    vkp_free(p);
}
static void draw_drop_sampler(void *value, void *ctx)
{
    VkSampler *p = value;
    vkDestroySampler(vkd.dev, *p, 0);
    vkp_free(p);
}
static void draw_drop_shader(void *value, void *ctx) { vkd_shader_free(value); }
static void draw_drop_decl(void *value, void *ctx) { vkd_vdecl_free(value); }
static void device_draw_free(struct vkd_device *d)
{
    vkd_map_clear(&d->pipelines, draw_drop_pipeline, d);
    vkd_map_clear(&d->samplers, draw_drop_sampler, d);

    vkd_map_clear(&d->ff_vs, draw_drop_shader, d);
    vkd_map_clear(&d->ff_ps, draw_drop_shader, d);
    vkd_map_clear(&d->fvf_decls, draw_drop_decl, d);

    for (UINT i = 0; i < 3; i++) {
        vkd_image_destroy(d, d->dummy[i]);
        vkd_image_destroy(d, d->white[i]);
        d->dummy[i] = d->white[i] = 0;
    }
}
