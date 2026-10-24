/* World scene target and post chain (docs/RENDERER-PLAN.md, stages 1-9).
 *
 * The world is redirected to an A16B16G16R16F target with an INTZ depth
 * texture after the engine begins its scene, and resolved into the back
 * buffer at renderList_end, before the HUD. The target holds
 * display-encoded colour exactly as the native draws write it (their
 * blending and device fog are unchanged); upgraded effects write the same
 * encoding, unclamped, with a brighter sun (WorldHdrScale), so the target
 * holds real highlights above 1.
 *
 * Resolve passes (mods/shaders/world_post.hlsl, vs_3_0/ps_3_0):
 *   depth_lin   INTZ depth -> linear view depth (R32F). A mid-frame depth
 *               clear (the first-person pass) first saves the world's depth,
 *               so the merged depth covers both.
 *   ssao        half-resolution ambient occlusion (R16F)
 *   composite   decode to linear light, apply AO and height fog (FP16)
 *   aa          edge antialiasing (FXAA 3.11 quality) on the current frame
 *   bloom_down  6 levels; adapt: average luminance with eye adaptation;
 *   bloom_up    additive tent upsampling
 *   finish      bloom, exposure, hue-preserving highlight shoulder, encode
 *
 * There is no temporal antialiasing. Camera-only TAA (no per-object motion
 * vectors) left see-through trails behind moving and alpha-tested geometry,
 * and its jitter and history softened the whole image (game test,
 * 2026-10-24). The projection is never changed.
 *
 * Without INTZ a plain D24S8 depth is used and only the plain resolve runs.
 * The world target is never multisampled (the edge AA replaces MSAA for the
 * world; rpg7_hd's MSAA back buffer still serves the HUD).
 *
 * The engine's own setRenderTarget (00a563e0, PCDX8Renderer::vf23) rebinds
 * its stored back buffer when a render-to-texture pass ends; while the world
 * target is active it is bound again instead.
 *
 * Keys: F10 post chain on/off, F11 edge AA on/off (F7 shadows and F8 materials
 * are in world_render.c, which this mod requires).
 */
#include "bfv_mod.h"
#include "world_shared.h"
#include "../shaders/world_post_post_vs.h"
#include "../shaders/world_post_depth_lin.h"
#include "../shaders/world_post_ssao.h"
#include "../shaders/world_post_composite.h"
#include "../shaders/world_post_aa.h"
#include "../shaders/world_post_adapt.h"
#include "../shaders/world_post_bloom_down.h"
#include "../shaders/world_post_bloom_up.h"
#include "../shaders/world_post_finish.h"
#define RPTR(p) ((uint32_t)(uintptr_t)(p))
#define RDEV(slot, ...) GAME_STDCALL(GAME_VIRTUAL(dev,slot),dev __VA_OPT__(,) __VA_ARGS__)

#define D3DFMT_A16B16G16R16F 113u
#define D3DFMT_R16F 111u
#define D3DFMT_R32F 114u
#define D3DFMT_D24S8 75u
#define D3DFMT_INTZ 0x5a544e49u
#define BLOOM_LEVELS 6
#define FAR_DEPTH 60000.0f
#define SKY_DEPTH 50000.0f

static uint32_t hdr_tex,hdr_surf,depth_tex,hdr_depth,depth_readable;
static uint32_t lin_tex,lin_surf,snap_tex,snap_surf,ao_tex,ao_surf,comp_tex,comp_surf;
static uint32_t aa_tex,aa_surf,bloom_tex[BLOOM_LEVELS],bloom_surf[BLOOM_LEVELS],adapt_tex[2],adapt_surf[2];
static uint32_t bloom_w[BLOOM_LEVELS],bloom_h[BLOOM_LEVELS];
static uint32_t vs_post,ps_depth,ps_ssao,ps_comp,ps_aa,ps_adapt,ps_down,ps_up,ps_finish;
static uint32_t back_rt,back_depth,saved_vp[6],post_w,post_h,post_format;
static uint32_t hdr_active,post_failed,full_post;
static uint32_t snap_valid,adapt_index,adapt_reset=1,last_tick,post_frame;
static uint32_t post_on=1,aa_on=1,key_fn,tick_fn;
static uint32_t fog_color;

static void post_log(struct bfv_mod *mod,const char *s) {
    uint32_t n=0,w=0;while(s[n])n++;mod_log(mod,s);
    uint32_t f=GAME_STDCALL(M32(imp_CreateFileA),RPTR("world_render.log"),0x40000000u,1,0,4,0x80,0);
    if(f==0xffffffffu)return;
    GAME_STDCALL(M32(imp_SetFilePointer),f,0,0,2);
    GAME_STDCALL(M32(imp_WriteFile),f,RPTR(s),n,RPTR(&w),0);GAME_STDCALL(M32(imp_CloseHandle),f);
}
static void rel(struct bfv_mod *mod,uint32_t p){if(p)GAME_STDCALL(GAME_VIRTUAL(p,2),p);}
#define DROP(x) do{rel(mod,x);x=0;}while(0)
static void post_drop(struct bfv_mod *mod) {
    DROP(hdr_surf);DROP(hdr_tex);DROP(hdr_depth);DROP(depth_tex);
    DROP(lin_surf);DROP(lin_tex);DROP(snap_surf);DROP(snap_tex);DROP(ao_surf);DROP(ao_tex);DROP(comp_surf);DROP(comp_tex);
    DROP(aa_surf);DROP(aa_tex);
    for(int i=0;i<2;i++){DROP(adapt_surf[i]);DROP(adapt_tex[i]);}
    for(int i=0;i<BLOOM_LEVELS;i++){DROP(bloom_surf[i]);DROP(bloom_tex[i]);}
    DROP(vs_post);DROP(ps_depth);DROP(ps_ssao);DROP(ps_comp);DROP(ps_aa);DROP(ps_adapt);DROP(ps_down);DROP(ps_up);DROP(ps_finish);
    DROP(back_rt);DROP(back_depth);
    depth_readable=full_post=post_w=post_h=post_format=hdr_active=post_failed=0;
    snap_valid=0;adapt_reset=1;
    world_shared.scene_active=0;
}
static int texture_rt(struct bfv_mod *mod,uint32_t dev,uint32_t w,uint32_t h,uint32_t format,uint32_t *tex,uint32_t *rt) {
    uint32_t hr=RDEV(23,w,h,1,1 /* D3DUSAGE_RENDERTARGET */,format,0,RPTR(tex),0);
    if(!hr)hr=GAME_STDCALL(GAME_VIRTUAL(*tex,18),*tex,0,RPTR(rt));
    return !hr&&*tex&&*rt;
}
static int pixel_shader(struct bfv_mod *mod,uint32_t dev,const uint32_t *code,uint32_t *out) {
    return !RDEV(106,RPTR(code),RPTR(out))&&*out;
}
/* The depth-based chain: everything after the plain resolve. */
static int post_chain_prepare(struct bfv_mod *mod,uint32_t dev,uint32_t w,uint32_t h) {
    uint32_t hw=(w+1)/2,hh=(h+1)/2;
    if(!texture_rt(mod,dev,w,h,D3DFMT_R32F,&lin_tex,&lin_surf))return 0;
    if(!texture_rt(mod,dev,w,h,D3DFMT_R32F,&snap_tex,&snap_surf))return 0;
    if(!texture_rt(mod,dev,hw,hh,D3DFMT_R16F,&ao_tex,&ao_surf))return 0;
    if(!texture_rt(mod,dev,w,h,D3DFMT_A16B16G16R16F,&comp_tex,&comp_surf))return 0;
    if(!texture_rt(mod,dev,w,h,D3DFMT_A16B16G16R16F,&aa_tex,&aa_surf))return 0;
    for(int i=0;i<2;i++)
        if(!texture_rt(mod,dev,1,1,D3DFMT_R32F,&adapt_tex[i],&adapt_surf[i]))return 0;
    uint32_t bw=w,bh=h;
    for(int i=0;i<BLOOM_LEVELS;i++) {
        bw=bw>3?(bw+1)/2:2;bh=bh>3?(bh+1)/2:2;bloom_w[i]=bw;bloom_h[i]=bh;
        if(!texture_rt(mod,dev,bw,bh,D3DFMT_A16B16G16R16F,&bloom_tex[i],&bloom_surf[i]))return 0;
    }
    return pixel_shader(mod,dev,world_post_depth_lin,&ps_depth)&&pixel_shader(mod,dev,world_post_ssao,&ps_ssao)&&
           pixel_shader(mod,dev,world_post_composite,&ps_comp)&&pixel_shader(mod,dev,world_post_aa,&ps_aa)&&
           pixel_shader(mod,dev,world_post_adapt,&ps_adapt)&&pixel_shader(mod,dev,world_post_bloom_down,&ps_down)&&
           pixel_shader(mod,dev,world_post_bloom_up,&ps_up);
}
static int post_prepare(struct bfv_mod *mod,uint32_t dev,uint32_t w,uint32_t h,uint32_t format) {
    if(post_w!=w||post_h!=h||post_format!=format)post_drop(mod);
    if(post_failed)return 0;
    if(hdr_surf)return 1;
    post_w=w;post_h=h;post_format=format;
    if(!texture_rt(mod,dev,w,h,D3DFMT_A16B16G16R16F,&hdr_tex,&hdr_surf))goto fail;
    if(!RDEV(23,w,h,1,2 /* D3DUSAGE_DEPTHSTENCIL */,D3DFMT_INTZ,0,RPTR(&depth_tex),0) && depth_tex &&
       !GAME_STDCALL(GAME_VIRTUAL(depth_tex,18),depth_tex,0,RPTR(&hdr_depth)) && hdr_depth) depth_readable=1;
    if(!depth_readable) {
        DROP(hdr_depth);DROP(depth_tex);
        if(RDEV(29,w,h,D3DFMT_D24S8,0,0,1,RPTR(&hdr_depth),0)||!hdr_depth)goto fail;
    }
    /* The finish pass needs the fullscreen vertex shader too. */
    if(RDEV(91,RPTR(world_post_post_vs),RPTR(&vs_post))||!vs_post)goto fail;
    if(!pixel_shader(mod,dev,world_post_finish,&ps_finish))goto fail;
    if(depth_readable) {
        full_post=post_chain_prepare(mod,dev,w,h);
        if(!full_post) {
            DROP(lin_surf);DROP(lin_tex);DROP(snap_surf);DROP(snap_tex);DROP(ao_surf);DROP(ao_tex);DROP(comp_surf);DROP(comp_tex);
            DROP(aa_surf);DROP(aa_tex);
            for(int i=0;i<2;i++){DROP(adapt_surf[i]);DROP(adapt_tex[i]);}
            for(int i=0;i<BLOOM_LEVELS;i++){DROP(bloom_surf[i]);DROP(bloom_tex[i]);}
            DROP(ps_depth);DROP(ps_ssao);DROP(ps_comp);DROP(ps_aa);DROP(ps_adapt);DROP(ps_down);DROP(ps_up);
            post_log(mod,"world: ERROR post chain resources unavailable, plain resolve only\r\n");
        }
    }
    post_log(mod,full_post?"world: FP16 scene target ready (INTZ depth, full post chain)\r\n":
             depth_readable?"world: FP16 scene target ready (INTZ depth, plain resolve)\r\n"
                           :"world: FP16 scene target ready (no INTZ: depth stages off)\r\n");
    return 1;
fail: post_drop(mod);post_w=w;post_h=h;post_format=format;post_failed=1;
    post_log(mod,"world: ERROR FP16 scene target unavailable, native back buffer used\r\n");return 0;
}

/* Fullscreen pass state. The caller captured a D3DSBT_ALL state block first
 * and applies it afterwards, so the engine's state cache and its shader
 * constants stay true. */
static void post_states(struct bfv_mod *mod,uint32_t dev) {
    RDEV(92,vs_post);RDEV(89,0x40202 /* XYZ | TEX2 | TEXCOORDSIZE3(1) */);
    RDEV(57,7,0);RDEV(57,14,0);RDEV(57,15,0);RDEV(57,27,0);RDEV(57,28,0);RDEV(57,22,1);RDEV(57,8,3);
    RDEV(57,52,0);RDEV(57,174,0);RDEV(57,152,0);RDEV(57,168,15);RDEV(57,194,0);RDEV(57,206,0);
    for(uint32_t s=0;s<4;s++) {
        RDEV(69,s,1,3);RDEV(69,s,2,3);RDEV(69,s,7,0);
        RDEV(69,s,11,0); // D3DSAMP_SRGBTEXTURE off: decoding is in the shader
    }
}
static void consts(struct bfv_mod *mod,uint32_t dev,uint32_t start,const float *v,uint32_t count) {RDEV(109,start,RPTR(v),count);}
/* The world camera's projection, captured with its view at the first world
 * draw. The projection set last (raw_proj) can belong to a later pass such
 * as the first-person weapon, whose field of view differs: rays built from
 * it put every reconstructed world position in the wrong place. */
static const float *world_proj(void){return world_shared.camera_valid?world_shared.proj:world_shared.raw_proj;}
/* A fullscreen quad into target (tw x th). linear: bit per sampler. Each
 * corner carries the world-space view ray for unit view depth. */
static uint32_t pass(struct bfv_mod *mod,uint32_t dev,uint32_t target,uint32_t tw,uint32_t th,uint32_t shader,
                     uint32_t s0,uint32_t s1,uint32_t s2,uint32_t linear) {
    struct vertex{float x,y,z,u,v,rx,ry,rz;} q[4];
    const float *p=world_proj(),*f=world_shared.forward,*r=world_shared.right,*up=world_shared.up;
    float sx=p[0]!=0?1/p[0]:1,sy=p[5]!=0?1/p[5]:1,ox=-1.0f/(float)tw,oy=1.0f/(float)th;
    for(int i=0;i<4;i++) {
        float u=(float)(i&1),v=(float)(i>>1),nx=u*2-1,ny=1-v*2;
        q[i].x=nx+ox;q[i].y=ny+oy;q[i].z=0.5f;q[i].u=u;q[i].v=v;
        for(int k=0;k<3;k++)(&q[i].rx)[k]=f[k]+r[k]*nx*sx+up[k]*ny*sy;
    }
    uint32_t vp[6]={0,0,tw,th,0,0x3f800000};
    uint32_t tex[3]={s0,s1,s2};
    uint32_t hr=RDEV(37,0,target);if(hr)return hr;
    RDEV(47,RPTR(vp));RDEV(107,shader);
    for(uint32_t s=0;s<3;s++) {
        uint32_t filter=linear>>s&1?2:1;
        RDEV(65,s,tex[s]);RDEV(69,s,5,filter);RDEV(69,s,6,filter);
    }
    hr=RDEV(83,5 /* TRIANGLESTRIP */,2,RPTR(q),sizeof q[0]);
    for(uint32_t s=0;s<3;s++)RDEV(65,s,0);
    return hr;
}
static void vec4(float *o,float a,float b,float c,float d){o[0]=a;o[1]=b;o[2]=c;o[3]=d;}
static uint32_t tick_now(struct bfv_mod *mod) {
    if(!tick_fn) {
        uint32_t dll=GAME_STDCALL(M32(imp_GetModuleHandleA),RPTR("kernel32.dll"));
        if(dll)tick_fn=GAME_STDCALL(M32(imp_GetProcAddress),dll,RPTR("GetTickCount"));
    }
    return tick_fn?GAME_STDCALL(tick_fn):0;
}
static float fexp(float x) {          // e^x for |x| < ~20, enough for adaptation
    float r=1,t=1;for(int i=1;i<16;i++){t*=x/(float)i;r+=t;}return r;
}

/* Linear view depth of the current depth buffer into target. */
static void depth_pass(struct bfv_mod *mod,uint32_t dev,uint32_t target,uint32_t with_snap) {
    float c[4];const float *p=world_shared.raw_proj;
    vec4(c,p[10],p[14],with_snap?1.0f:0.0f,FAR_DEPTH);consts(mod,dev,0,c,1);
    pass(mod,dev,target,post_w,post_h,ps_depth,depth_tex,snap_tex,0,0);
}
static int perspective(void){return world_shared.raw_proj[11]>0.5f&&world_shared.raw_proj[14]!=0;}
static int world_perspective(void){const float *p=world_proj();return p[11]>0.5f&&p[14]!=0;}

static void post_full(struct bfv_mod *mod,uint32_t dev) {
    float c[12*4]={0};
    float w=(float)post_w,h=(float)post_h;
    const float *p=world_proj();
    post_frame++;
    /* 1 linear depth */
    depth_pass(mod,dev,lin_surf,snap_valid);
    /* 2 ambient occlusion, half resolution */
    vec4(c,1/w,1/h,w,h);vec4(c+4,1/p[0],1/p[5],1.0f,0.6f);
    vec4(c+8,0,SKY_DEPTH,0,0);consts(mod,dev,0,c,3);
    pass(mod,dev,ao_surf,(post_w+1)/2,(post_h+1)/2,ps_ssao,lin_tex,0,0,0);
    /* 3 decode, AO, height fog */
    uint32_t fog=fog_color;
    float fr=(float)(fog>>16&255)/255,fg=(float)(fog>>8&255)/255,fb=(float)(fog&255)/255;
    const float *cam=world_shared.camera,*sun=world_shared.sun,*sc=world_shared.sun_color;
    float base=world_shared.water_seen?world_shared.water_height:cam[1]-40;
    vec4(c,1/w,1/h,0.6f,1);vec4(c+4,0.0003f,0.035f,base,0.12f);vec4(c+8,cam[0],cam[1],cam[2],80);
    vec4(c+12,fr*fr,fg*fg,fb*fb,fog&0xffffff?1.0f:0.0f);vec4(c+16,sun[0],sun[1],sun[2],SKY_DEPTH);vec4(c+20,sc[0],sc[1],sc[2],2000);
    consts(mod,dev,0,c,6);
    pass(mod,dev,comp_surf,post_w,post_h,ps_comp,hdr_tex,ao_tex,lin_tex,2);
    uint32_t src=comp_tex;
    /* 4 edge antialiasing, subpixel amount 0.5 (FXAA's default 0.75 is softer) */
    if(aa_on) {
        vec4(c,1/w,1/h,0.5f,0);consts(mod,dev,0,c,1);
        pass(mod,dev,aa_surf,post_w,post_h,ps_aa,comp_tex,0,0,1);
        src=aa_tex;
    }
    /* 5 bloom downsample chain */
    uint32_t from=src,fw=post_w,fh=post_h;
    for(int i=0;i<BLOOM_LEVELS;i++) {
        vec4(c,1/(float)fw,1/(float)fh,i==0?1.0f:0.0f,0);consts(mod,dev,0,c,1);
        pass(mod,dev,bloom_surf[i],bloom_w[i],bloom_h[i],ps_down,from,0,0,1);
        from=bloom_tex[i];fw=bloom_w[i];fh=bloom_h[i];
    }
    /* 6 eye adaptation */
    uint32_t now=tick_now(mod),dt=now-last_tick;last_tick=now;
    float step=1-fexp(-(float)(dt>500?500:dt)*0.0015f);
    vec4(c,step,adapt_reset?1.0f:0.0f,0,0);consts(mod,dev,0,c,1);
    pass(mod,dev,adapt_surf[adapt_index],1,1,ps_adapt,bloom_tex[BLOOM_LEVELS-1],adapt_tex[adapt_index^1],0,1);
    adapt_reset=0;
    /* 7 bloom upsample, added into the larger level */
    RDEV(57,27,1);RDEV(57,19,2);RDEV(57,20,2);RDEV(57,171,1);
    for(int i=BLOOM_LEVELS-1;i>0;i--) {
        vec4(c,1/(float)bloom_w[i],1/(float)bloom_h[i],0,0);consts(mod,dev,0,c,1);
        pass(mod,dev,bloom_surf[i-1],bloom_w[i-1],bloom_h[i-1],ps_up,bloom_tex[i],0,0,1);
    }
    RDEV(57,27,0);
    /* 8 bloom, exposure, tone mapping, encode */
    /* Exposure stays near 1 so the native palette survives: the auto range
     * only lifts dark scenes a little. */
    vec4(c,1/w,1/h,0.04f,0.35f);vec4(c+4,0.18f,1.0f,1.1f,1);vec4(c+8,1.0f/BLOOM_LEVELS,1.08f,0.12f,0);consts(mod,dev,0,c,3);
    pass(mod,dev,back_rt,post_w,post_h,ps_finish,src,bloom_tex[0],adapt_tex[adapt_index],2);
    adapt_index^=1;
}

static void hdr_resolve(struct bfv_mod *mod) {
    uint32_t dev=M32(g_pD3DDevice),saved_block=0;if(!dev)return;
    RDEV(58,34,RPTR(&fog_color)); // the map's fog colour, before post state
    RDEV(59,1,RPTR(&saved_block));post_states(mod,dev);
    RDEV(39,0); // the depth texture is sampled
    if(full_post&&post_on&&world_shared.camera_valid&&world_perspective())post_full(mod,dev);
    else {
        float c[12]={0};
        vec4(c,1/(float)post_w,1/(float)post_h,0,0);vec4(c+8,0,0,0,1);consts(mod,dev,0,c,3);
        pass(mod,dev,back_rt,post_w,post_h,ps_finish,hdr_tex,0,0,0);
    }
    RDEV(39,back_depth);RDEV(47,RPTR(saved_vp));
    if(saved_block){GAME_STDCALL(GAME_VIRTUAL(saved_block,5),saved_block);rel(mod,saved_block);}
    rel(mod,back_rt);rel(mod,back_depth);back_rt=back_depth=0;hdr_active=0;world_shared.scene_active=0;
}

static void keys(struct bfv_mod *mod) {
    if(!key_fn) {
        uint32_t dll=GAME_STDCALL(M32(imp_GetModuleHandleA),RPTR("user32.dll"));
        if(dll)key_fn=GAME_STDCALL(M32(imp_GetProcAddress),dll,RPTR("GetAsyncKeyState"));
        if(!key_fn)return;
    }
    if(GAME_STDCALL(key_fn,0x79)&1) {
        post_on=!post_on;adapt_reset=1;
        post_log(mod,post_on?"world: F10 post chain ON\r\n":"world: F10 post chain OFF (plain resolve)\r\n");
    }
    if(GAME_STDCALL(key_fn,0x7a)&1) {
        aa_on=!aa_on;
        post_log(mod,aa_on?"world: F11 edge AA ON\r\n":"world: F11 edge AA OFF\r\n");
    }
}
MOD_AFTER(0x00a55f50, hdr_world_begin) {
    if(MOD_RETURN_ADDRESS!=0x004a8b71u||!M8(0x00e20bb4u)||hdr_active)return;
    keys(mod);
    /* The frame routine skips renderList_end when this flag is set (menu
     * frames jump from 004a8bac to 004a9162), so the target would never be
     * resolved. Leave those frames on the back buffer. */
    uint32_t game=M32(0x00d444c4u);if(game&&M8(game+0x10c))return;
    uint32_t dev=M32(g_pD3DDevice),desc[8]={0},candidate_rt=0,candidate_depth=0;if(!dev)return;
    if(RDEV(38,0,RPTR(&candidate_rt))||!candidate_rt)return;
    RDEV(40,RPTR(&candidate_depth));RDEV(48,RPTR(saved_vp));GAME_STDCALL(GAME_VIRTUAL(candidate_rt,12),candidate_rt,RPTR(desc));
    if(!post_prepare(mod,dev,desc[6],desc[7],desc[0])){rel(mod,candidate_rt);rel(mod,candidate_depth);return;}
    back_rt=candidate_rt;back_depth=candidate_depth;
    RDEV(37,0,hdr_surf);RDEV(39,hdr_depth);RDEV(43,0,0,7 /* target|z|stencil */,0,0x3f800000,0);
    hdr_active=1;snap_valid=0;
    world_shared.scene_active=1;world_shared.post_on=post_on&&full_post;world_shared.world_drawn=0;
    uint32_t mgr=M32(g_pShaderManager_00e20b10);
    if(mgr)for(int i=0;i<16;i++)world_shared.raw_proj[i]=F32(mgr+0x94+4*i);
}

/* setProj: the projection the engine set last. */
MOD_AFTER(0x00a48a50, hdr_set_projection) {
    uint32_t mgr=MOD_THIS;
    if(mgr!=M32(g_pShaderManager_00e20b10))return;
    for(int i=0;i<16;i++)world_shared.raw_proj[i]=F32(mgr+0x94+4*i);
}
/* A depth clear in the middle of the world (the first-person pass) would
 * remove the scene depth the post chain needs: save it first as linear
 * depth; depth_lin merges the two. */
MOD_BEFORE(0x00a55e30, hdr_mid_clear) {
    static uint32_t logged;
    if(!hdr_active||!full_post||!post_on||snap_valid||!(MOD_ARG(0)&2)||!world_shared.world_drawn||!perspective())return;
    uint32_t dev=M32(g_pD3DDevice),rt=0,saved_block=0,vp[6];if(!dev)return;
    if(RDEV(38,0,RPTR(&rt))||!rt)return;
    rel(mod,rt);if(rt!=hdr_surf)return;
    RDEV(48,RPTR(vp));
    RDEV(59,1,RPTR(&saved_block));post_states(mod,dev);
    RDEV(39,0);
    depth_pass(mod,dev,snap_surf,0);
    RDEV(37,0,hdr_surf);RDEV(39,hdr_depth);RDEV(47,RPTR(vp));
    if(saved_block){GAME_STDCALL(GAME_VIRTUAL(saved_block,5),saved_block);rel(mod,saved_block);}
    snap_valid=1;
    if(!logged++)post_log(mod,"world: scene depth saved before a mid-frame depth clear\r\n");
}

MOD_AFTER(0x00a401b0, hdr_world_finish) {
    if(MOD_RETURN_ADDRESS!=0x004a8ca2u||!hdr_active)return;
    hdr_resolve(mod);
}

/* End scene (00a55fa0, before the device EndScene). Any frame path that
 * began the world but did not reach renderList_end (004a8b79 returns early,
 * 004a8bb4 jumps past it) is resolved here, so the back buffer is never
 * left unwritten. */
MOD_BEFORE(0x00a55fa0, hdr_end_scene) {
    if(hdr_active)hdr_resolve(mod);
}

MOD_AFTER(0x00a563e0, hdr_target_restore) {
    if(!hdr_active||MOD_ARG(0))return;
    uint32_t dev=M32(MOD_THIS+0x90);if(!dev)return;
    RDEV(37,0,hdr_surf);RDEV(39,hdr_depth);
}

static void post_reset(struct bfv_mod *mod){post_drop(mod);}
MOD_BEFORE(0x00a5fbc0, hdr_reset){post_reset(mod);}
MOD_BEFORE(0x00a5f650, hdr_release){post_reset(mod);}
MOD_BEFORE(0x00a603e0, hdr_create){post_reset(mod);}
