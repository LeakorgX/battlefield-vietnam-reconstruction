/* World material lighting, terrain relief and water, via engine effects.
 * See docs/WORLD-RENDER.md. Requires RPG/HD sky override archives.
 * Uses native setters and the native stream declarations; no D3D proxy.
 */
#include "bfv_mod.h"
#include "world_shared.h"
#define WPTR(p) ((uint32_t)(uintptr_t)(p))
struct world_shared world_shared;
static uint32_t world_mounting, world_sky, world_sky_missing;
static int world_enabled=1;
static uint32_t world_draws, terrain_draws, water_draws;
static uint32_t capture_stage,capture_wait,capture_requested;
static void capture_init(struct bfv_mod *mod);
static uint32_t reflection_texture,reflection_target,reflection_depth,reflection_busy,reflection_ready;
static uint32_t reflection_width,reflection_height,water_seen;
static float water_height;
static void reflection_begin(struct bfv_mod *mod,uint32_t dev);
static void shadow_params(struct bfv_mod *mod,uint32_t e);
static void shadow_frame(void);
static uint32_t shadow_on=1;

static uint32_t strchars(uint32_t s) { return M32(s+0x18)>=16?M32(s+4):s+4; }
static int stris(uint32_t s,const char *v) {
    uint32_t i=0,p=strchars(s);
    while(v[i]) { if(i>=M32(s+0x14) || (M8(p+i)|32)!=(uint8_t)(v[i]|32)) return 0; i++; }
    return i==M32(s+0x14);
}
static void world_log(struct bfv_mod *mod,const char *s) {
    uint32_t n=0,written=0;
    while(s[n]) n++;
    mod_log(mod,s);
    uint32_t f=GAME_STDCALL(M32(imp_CreateFileA),WPTR("world_render.log"),0x40000000u,1,0,4,0x80,0);
    if(f==0xffffffffu)return;
    GAME_STDCALL(M32(imp_SetFilePointer),f,0,0,2);
    GAME_STDCALL(M32(imp_WriteFile),f,WPTR(s),n,WPTR(&written),0);
    GAME_STDCALL(M32(imp_CloseHandle),f);
}

MOD_BEFORE(0x008ba040, world_mount)
{
    if(world_mounting)return;
    const char *extra;
    if(stris(MOD_ARG(1),"effects_002.rfa"))extra="effects_003.rfa";
    else if(stris(MOD_ARG(1),"texture_002.rfa"))extra="texture_003.rfa";
    else return;
    uint32_t name[7]={0,0,0,0,0,0,15};
    GAME_THISCALL(std_string_assign_chars,WPTR(name),WPTR(extra),15);
    world_mounting=1;
    uint32_t ok=GAME_THISCALL(0x008ba040,MOD_THIS,MOD_ARG(0),WPTR(name))&255;
    world_mounting=0;
    if(M32(WPTR(name)+0x18)>=16) GAME_CDECL(operator_delete,M32(WPTR(name)+4));
    world_log(mod,ok?"world: mounted before the RPG/retail archive: ":"world: ERROR mount failed: ");
    world_log(mod,extra);world_log(mod,"\r\n");
}
static int world_mesh(uint32_t s) {
    static const uint32_t globals[]={0xe17380,0xe173b4,0xe1735c,0xe17360,0xe17364,0xe17368,0xe17374,0xe1737c,0xe17384,
                                   0xe17394,0xe17398,0xe1739c,0xe173a0,0xe173a8,0xe173b0,0xe173b8,0xe17388,0xe173bc};
    for(uint32_t i=0;i<sizeof globals/sizeof globals[0];i++) if(s && s==M32(globals[i]))return 1;
    return 0; // RPG PBR1Dif is a separate instance, deliberately excluded.
}
static void set_vec(struct bfv_mod *mod,uint32_t e,const char *name,const float *v) {
    GAME_STDCALL(GAME_VIRTUAL(e,0x88/4),e,WPTR(name),WPTR(v));
}
static void set_matrix(struct bfv_mod *mod,uint32_t e,const char *name,uint32_t p) {
    GAME_STDCALL(GAME_VIRTUAL(e,0x98/4),e,WPTR(name),p);
}
static void set_enabled(struct bfv_mod *mod,uint32_t e) {
    GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("WorldUpgradeEnabled"),GAME_FLOAT(world_enabled?1.0f:0.0f));
}
/* Values that are the same for every draw of a frame are set once per
 * effect per frame: the effect keeps a parameter until it is set again. */
static uint32_t world_frame_id,bound_effect[48],bound_frame[48];
static int first_bind_this_frame(uint32_t e) {
    uint32_t free_slot=48;
    for(uint32_t i=0;i<48;i++) {
        if(bound_effect[i]==e) {
            if(bound_frame[i]==world_frame_id)return 0;
            bound_frame[i]=world_frame_id;return 1;
        }
        if(free_slot==48&&(!bound_effect[i]||bound_frame[i]!=world_frame_id))free_slot=i;
    }
    if(free_slot<48){bound_effect[free_slot]=e;bound_frame[free_slot]=world_frame_id;}
    return 1;
}
static void world_fog(struct bfv_mod *mod,uint32_t e,uint32_t dev) {
    (void)dev;
    uint32_t mgr=M32(g_pShaderManager_00e20b10);
    if(!mgr)return;
    // PVLS used device fog and never populated the effect's FogData.
    // Every programmable replacement needs the map's actual distances; the
    // effects output the retail fog factor and the device blends the colour.
    GAME_STDCALL(GAME_VIRTUAL(e,0x80/4),e,WPTR("FogData"),mgr+0x148,2);
}
/* The HD sky's own cube (texture_003.rfa, baked from the HDRI the sky faces
 * come from, with the same tone curve; GGX prefiltered by roughness per mip).
 * Its sun is at azimuth 0 like the faces, and the sky mesh draws turned by
 * sky+0xb4 + sky+0x38 degrees (hd_sky.c), so the effects turn a direction
 * back by that angle before the lookup: SkyYaw = (cos, sin). */
static void sky_yaw(struct bfv_mod *mod,uint32_t e) {
    uint32_t sky=M32(g_pSky);
    float v[4]={1,0,0,0};
    if(sky) {
        float a=(F32(sky+0xb4)+F32(sky+0x38))*0.0174532925f,c,s;
        __asm__("fsincos":"=t"(c),"=u"(s):"0"(a));
        v[0]=c;v[1]=s;
    }
    set_vec(mod,e,"SkyYaw",v);
}
static void bind_sky(struct bfv_mod *mod,uint32_t e) {
    sky_yaw(mod,e);
    if(!world_sky&&!world_sky_missing) {
        uint32_t mgr=M32(g_pTextureManager),name[7]={0,0,0,0,0,0,15};
        if(!mgr)return;
        GAME_THISCALL(std_string_assign_chars,WPTR(name),WPTR("texture/W_hd_sky_env.rcm"),24);
        world_sky=GAME_THISCALL(GAME_VIRTUAL(mgr,6),mgr,WPTR(name));
        if(M32(WPTR(name)+0x18)>=16)GAME_CDECL(operator_delete,M32(WPTR(name)+4));
        world_sky_missing=!world_sky;
        world_log(mod,world_sky?"world: HD sky cube loaded\r\n":"world: ERROR HD sky cube missing (texture_003.rfa)\r\n");
    }
    if(world_sky) {
        uint32_t cube=GAME_THISCALL(GAME_VIRTUAL(world_sky,3),world_sky,M32(TextureCubeInfo_classID));
        if(cube)GAME_STDCALL(RaShader_00a4bb70,e,WPTR("skyCube"),cube);
    }
}
static void surface(struct bfv_mod *mod,uint32_t shader,uint32_t item) {
    if(!world_mesh(shader)||!item)return;
    uint32_t e=M32(shader+0x54),render=M32(item+4),object=M32(item+8);
    if(!e||!render||!object)return;
    // The original gloss exponent, not a blanket metallic material.
    float power=F32(render+0x70);
    if(!(power>=0.0f&&power<4096.0f))power=16;
    float rough=mod_sqrtf(mod_sqrtf(2.0f/(power+2.0f)));
    float params[4]={mod_clampf(rough,0.24f,0.85f),0.04f,0.012f,1.0f};
    set_vec(mod,e,"WorldSurfaceParams",params);
    set_matrix(mod,e,"World",object);
    if(first_bind_this_frame(e)){world_fog(mod,e,M32(shader+0x48));set_enabled(mod,e);bind_sky(mod,e);shadow_params(mod,e);}
    if(!world_draws++)world_log(mod,"world: mesh per-pixel material bound\r\n");
}

// PVLS used fixed-function lighting. Feed the new per-pixel effect through
// the existing PPL object setter, which uses the same base-class fields.
MOD_AFTER(0x00a6e1a0, world_pvls_object)
{
    if(!world_mesh(MOD_THIS))return;
    GAME_THISCALL(0x00a4b650,MOD_THIS,MOD_ARG(0));
}
MOD_AFTER(0x00a6e0e0, world_pvls_lightmap_object)
{
    if(!world_mesh(MOD_THIS))return;
    // An object without a lightmap file (every bridge on Oblivion) gets no
    // texture: StandardMesh vf18 (0074a300) passes the texture manager's
    // null result on to render+0x98. The fixed-function stage reads an
    // unbound texture as white, a pixel shader as black.
    uint32_t e=M32(MOD_THIS+0x54),render=M32(MOD_ARG(0)+4);
    if(e&&render)GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("LightMapBound"),GAME_FLOAT(M32(render+0x98)?1.0f:0.0f));
    GAME_THISCALL(0x00a4b650,MOD_THIS,MOD_ARG(0));
}
MOD_AFTER(0x00a4b650, world_object) { surface(mod,MOD_THIS,MOD_ARG(0)); }
MOD_AFTER(0x00a4bf90, world_material)
{
    if(!world_mesh(MOD_THIS))return;
    GAME_THISCALL(0x00a4b5a0,MOD_THIS,MOD_ARG(0));
    surface(mod,MOD_THIS,MOD_ARG(0));
}
MOD_AFTER(0x00a6c450, world_environment_material)
{
    // The Environment subclass binds its old cube AFTER the base vf8 hook.
    // Bind the prefiltered sky last while keeping native reflectivity.
    // Its native setter rebinds the old cube every draw, so this one is
    // per draw, not once per frame.
    surface(mod,MOD_THIS,MOD_ARG(0));
    if(world_mesh(MOD_THIS)&&M32(MOD_THIS+0x54))bind_sky(mod,M32(MOD_THIS+0x54));
}
static void sun_vec(struct bfv_mod *mod,uint32_t e) {
    uint32_t sky=M32(g_pSky);
    if(!sky)return;
    float v[4]={F32(sky+0x3c),F32(sky+0x40),F32(sky+0x44),0};
    set_vec(mod,e,"TerrainSun",v);
}
static void multiply(float *o,const float *a,const float *b) {
    for(int r=0;r<4;r++)for(int k=0;k<4;k++) {
        float v=0;for(int j=0;j<4;j++)v+=a[r*4+j]*b[j*4+k];o[r*4+k]=v;
    }
}
MOD_AFTER(0x00a6cbb0, world_terrain_object)
{
    uint32_t s=MOD_THIS,e=M32(s+0x54),dev=M32(s+0x48),obj=M32(MOD_ARG(0)+8);
    if(!e||!dev||!obj)return;
    float view[16],proj[16],wv[16],wvp[16];
    if(GAME_STDCALL(GAME_VIRTUAL(dev,45),dev,2,WPTR(view)) ||
       GAME_STDCALL(GAME_VIRTUAL(dev,45),dev,3,WPTR(proj)))return;
    multiply(wv,(const float *)(uintptr_t)obj,view);multiply(wvp,wv,proj);
    set_matrix(mod,e,"WorldViewProjection",WPTR(wvp));set_matrix(mod,e,"World",obj);
    if(first_bind_this_frame(e)){world_fog(mod,e,dev);sun_vec(mod,e);set_enabled(mod,e);shadow_params(mod,e);}
    if(!terrain_draws++)world_log(mod,"world: terrain directional relief bound\r\n");
}
MOD_AFTER(0x00a6c6b0, world_water_object)
{
    uint32_t e=M32(MOD_THIS+0x54);if(!e)return;
    uint32_t obj=M32(MOD_ARG(0)+8),dev=M32(MOD_THIS+0x48);
    if(obj) {water_height=F32(obj+52);water_seen=1;}
    if(first_bind_this_frame(e)) {
        world_fog(mod,e,dev);
        GAME_STDCALL(GAME_VIRTUAL(e,0xd0/4),e,WPTR("SceneReflection"),reflection_texture);
        GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("ReflectionReady"),GAME_FLOAT(reflection_ready?1.0f:0.0f));
        sun_vec(mod,e);set_enabled(mod,e);bind_sky(mod,e);
    }
    uint32_t mgr=M32(g_pShaderManager_00e20b10);
    if(mgr) {
        uint32_t light=GAME_THISCALL(ShaderManager_RaShader_helper_00a49bd0,mgr,0);
        if(light) {
            float rgb[4]={F32(light+0x0c)*0.5f,F32(light+0x10)*0.5f,F32(light+0x14)*0.5f,1};
            set_vec(mod,e,"LightMaterialColor",rgb);
        }
    }
    if(!water_draws++)world_log(mod,"world: Fresnel water and sun bound\r\n");
}
MOD_AFTER(0x00a4bd40, world_effect_loaded)
{
    uint32_t s=MOD_THIS;
    if(!world_mesh(s)&&s!=M32(g_pRaShaderTerrain_00e173c8)&&s!=M32(g_pRaShaderWater_00e173cc))return;
    uint32_t e=M32(s+0x54),tech=M32(s+0x58),d[3]={0};
    if(!e||!tech){world_log(mod,"world: ERROR effect/technique missing\r\n");return;}
    GAME_STDCALL(GAME_VIRTUAL(e,5),e,tech,WPTR(d));
    if(d[0]) {world_log(mod,(const char *)(uintptr_t)d[0]);world_log(mod," loaded\r\n");}
}
MOD_BEFORE(0x00a55f50, world_frame)
{
    static uint32_t key_fn;
    capture_init(mod);
    if(MOD_RETURN_ADDRESS==0x004a8b71u){world_frame_id++;shadow_frame();reflection_begin(mod,M32(MOD_THIS+0x90));}
    if(capture_requested&&terrain_draws&&MOD_RETURN_ADDRESS==0x004a8b71u&&capture_stage==0&&++capture_wait==180) {capture_stage=1;world_enabled=0;}
    if(capture_stage==2)world_enabled=1;
    if(!key_fn) {
        uint32_t dll=GAME_STDCALL(M32(imp_GetModuleHandleA),WPTR("user32.dll"));
        if(dll)key_fn=GAME_STDCALL(M32(imp_GetProcAddress),dll,WPTR("GetAsyncKeyState"));
    }
    if(key_fn && (GAME_STDCALL(key_fn,0x77)&1)) {
        world_enabled=!world_enabled;world_frame_id+=2;
        world_log(mod,world_enabled?"world: F8 material upgrade ON\r\n":"world: F8 material upgrade OFF (comparison)\r\n");
    }
    if(key_fn && (GAME_STDCALL(key_fn,0x76)&1)) {
        shadow_on=!shadow_on;world_frame_id+=2;
        world_log(mod,shadow_on?"world: F7 sun shadows ON\r\n":"world: F7 sun shadows OFF\r\n");
    }
    if(key_fn && terrain_draws && (GAME_STDCALL(key_fn,0x78)&1)) {
        capture_stage=4;
        world_log(mod,"world: F9 live capture requested\r\n");
    }
}
static void capture_init(struct bfv_mod *mod) {
    static int capture_checked;
    if(!capture_checked) {
        capture_checked=1;
        uint32_t dll=GAME_STDCALL(M32(imp_GetModuleHandleA),WPTR("kernel32.dll"));
        uint32_t env=dll?GAME_STDCALL(M32(imp_GetProcAddress),dll,WPTR("GetEnvironmentVariableA")):0;
        char value[8]={0};
        if(env&&GAME_STDCALL(env,WPTR("BFV_WORLD_CAPTURE"),WPTR(value),sizeof value)&&value[0]=='1')capture_requested=1;
    }
}
MOD_AFTER(0x00a5f1b0, world_capture_params)
{
    capture_init(mod);
    if(capture_requested) {M32(MOD_THIS+0xe8)=1;M32(MOD_THIS+0xf8)=0;}
}
// Optional diagnostic framebuffer capture, enabled only in the test process
// with BFV_WORLD_CAPTURE=1. Readback happens after EndScene, not in a draw.
static void capture_frame(struct bfv_mod *mod,uint32_t dev,const char *path) {
    uint32_t bb=0,rt=0,sys=0,desc[8]={0},lock[2]={0},f=0xffffffffu,written;
    if(GAME_STDCALL(GAME_VIRTUAL(dev,18),dev,0,0,0,WPTR(&bb))||!bb)goto done;
    if(GAME_STDCALL(GAME_VIRTUAL(bb,12),bb,WPTR(desc)))goto done;
    uint32_t width=desc[6],height=desc[7],format=desc[0];
    if(width>4096||!height||(format!=22&&format!=21&&format!=23))goto done;
    if(GAME_STDCALL(GAME_VIRTUAL(dev,28),dev,width,height,format,0,0,0,WPTR(&rt),0)||!rt)goto done;
    if(GAME_STDCALL(GAME_VIRTUAL(dev,34),dev,bb,0,rt,0,0))goto done;
    if(GAME_STDCALL(GAME_VIRTUAL(dev,36),dev,width,height,format,2,WPTR(&sys),0)||!sys)goto done;
    if(GAME_STDCALL(GAME_VIRTUAL(dev,32),dev,rt,sys))goto done;
    if(GAME_STDCALL(GAME_VIRTUAL(sys,13),sys,WPTR(lock),0,0x10))goto done;
    f=GAME_STDCALL(M32(imp_CreateFileA),WPTR(path),0x40000000u,1,0,2,0x80,0);
    if(f!=0xffffffffu) {
        uint8_t header[54]={0};
        header[0]='B';header[1]='M';
        M32(WPTR(header)+2)=54+width*height*4;M32(WPTR(header)+10)=54;M32(WPTR(header)+14)=40;
        M32(WPTR(header)+18)=width;M32(WPTR(header)+22)=0-height;
        M16(WPTR(header)+26)=1;M16(WPTR(header)+28)=32;
        GAME_STDCALL(M32(imp_WriteFile),f,WPTR(header),54,WPTR(&written),0);
        static uint32_t row[4096];
        for(uint32_t y=0;y<height;y++) {
            uint32_t p=lock[1]+y*lock[0];
            for(uint32_t x=0;x<width;x++) {
                uint32_t color;
                if(format==23) {uint32_t c=M16(p+x*2);color=((c>>11)*255/31)<<16|(((c>>5)&63)*255/63)<<8|(c&31)*255/31;}
                else color=M32(p+x*4);
                row[x]=color|0xff000000u;
            }
            GAME_STDCALL(M32(imp_WriteFile),f,WPTR(row),width*4,WPTR(&written),0);
        }
        GAME_STDCALL(M32(imp_CloseHandle),f);
        world_log(mod,path);world_log(mod," captured\r\n");
    }
    GAME_STDCALL(GAME_VIRTUAL(sys,14),sys);
done:
    if(sys)GAME_STDCALL(GAME_VIRTUAL(sys,2),sys);
    if(rt)GAME_STDCALL(GAME_VIRTUAL(rt,2),rt);
    if(bb)GAME_STDCALL(GAME_VIRTUAL(bb,2),bb);
}
MOD_AFTER(0x00a55fa0, world_capture)
{
    if(MOD_RETURN_ADDRESS==0x004a9204u&&(capture_stage==1||capture_stage==2||capture_stage==4)) {
        uint32_t dev=M32(MOD_THIS+0x90);
        capture_frame(mod,dev,capture_stage==1?"world_render_before.bmp":capture_stage==2?"world_render_after.bmp":"world_render_live.bmp");
        capture_stage=capture_stage==4?3:capture_stage+1;
    }
}
static void reflection_drop(struct bfv_mod *mod) {
    if(reflection_target)GAME_STDCALL(GAME_VIRTUAL(reflection_target,2),reflection_target);
    if(reflection_depth)GAME_STDCALL(GAME_VIRTUAL(reflection_depth,2),reflection_depth);
    if(reflection_texture)GAME_STDCALL(GAME_VIRTUAL(reflection_texture,2),reflection_texture);
    reflection_target=reflection_depth=reflection_texture=reflection_ready=0;
}
static void reflection_begin(struct bfv_mod *mod,uint32_t dev) {
    if(!dev||!water_seen)return;
    uint32_t rt=0,depth=0,vp[6]={0},desc[8]={0};
    if(GAME_STDCALL(GAME_VIRTUAL(dev,38),dev,0,WPTR(&rt))||!rt)return;
    GAME_STDCALL(GAME_VIRTUAL(dev,40),dev,WPTR(&depth));
    GAME_STDCALL(GAME_VIRTUAL(dev,48),dev,WPTR(vp));
    GAME_STDCALL(GAME_VIRTUAL(rt,12),rt,WPTR(desc));
    uint32_t w=desc[6],h=desc[7];
    if(reflection_texture&&(w!=reflection_width||h!=reflection_height))reflection_drop(mod);
    if(!reflection_texture) {
        reflection_width=w;reflection_height=h;
        // FP16 like the scene target, so reflected highlights keep their range.
        if(GAME_STDCALL(GAME_VIRTUAL(dev,23),dev,w,h,1,1,113,0,WPTR(&reflection_texture),0)&&
           GAME_STDCALL(GAME_VIRTUAL(dev,23),dev,w,h,1,1,21,0,WPTR(&reflection_texture),0))goto done;
        if(GAME_STDCALL(GAME_VIRTUAL(reflection_texture,18),reflection_texture,0,WPTR(&reflection_target)))goto fail;
        if(GAME_STDCALL(GAME_VIRTUAL(dev,29),dev,w,h,75,0,0,1,WPTR(&reflection_depth),0))goto fail;
        world_log(mod,"world: full-resolution planar scene reflection allocated\r\n");
    }
    GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,0);
    if(GAME_STDCALL(GAME_VIRTUAL(dev,37),dev,0,reflection_target))goto done;
    GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,reflection_depth);
    if(!GAME_STDCALL(GAME_VIRTUAL(dev,43),dev,0,0,3,0, GAME_FLOAT(1.0f),0))reflection_ready=1;
    goto done;
fail: reflection_drop(mod);
done:
    GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,0);
    GAME_STDCALL(GAME_VIRTUAL(dev,37),dev,0,rt);
    GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,depth);
    GAME_STDCALL(GAME_VIRTUAL(dev,47),dev,WPTR(vp));
    if(depth)GAME_STDCALL(GAME_VIRTUAL(depth,2),depth);
    GAME_STDCALL(GAME_VIRTUAL(rt,2),rt);
}
/* ---- Main camera and sun shadows (renderer plan stage 8) -------------
 *
 * The camera is read at the first world draw of each frame from the shader
 * manager: view +0x14 and projection +0x94.
 *
 * Shadows: two orthographic cascades (radius 30 m and 150 m) side by side
 * in a 4096x2048 hardware-compared depth atlas (D24X8 texture). Casters are
 * the frame's own mesh and terrain draws, replayed with the light's
 * view-projection (ReflectionWVP, ShadowPass=1, colour writes off). Two
 * atlases alternate: frame N writes one while its draws sample the other,
 * written in frame N-1 with its own matrices (one frame of latency, never a
 * read of the map being written). Objects the main view culled, and
 * fixed-function foliage, do not cast. */
static uint32_t shadow_failed,shadow_busy,shadow_write,shadow_cleared;
static uint32_t shadow_tex[2],shadow_surf[2],shadow_color,shadow_frame_ready,shadow_write_valid,shadow_read_valid;
static float shadow_vp[2][16],shadow_sample[2][16],shadow_center[2][3],shadow_axis[3][3];
static uint32_t camera_frame_id,shadow_casts;
#define SHADOW_W 4096
#define SHADOW_H 2048
static const float shadow_radius[2]={30,150};

static void shadow_drop(struct bfv_mod *mod) {
    for(int i=0;i<2;i++) {
        if(shadow_surf[i])GAME_STDCALL(GAME_VIRTUAL(shadow_surf[i],2),shadow_surf[i]);
        if(shadow_tex[i])GAME_STDCALL(GAME_VIRTUAL(shadow_tex[i],2),shadow_tex[i]);
        shadow_surf[i]=shadow_tex[i]=0;
    }
    if(shadow_color)GAME_STDCALL(GAME_VIRTUAL(shadow_color,2),shadow_color);
    shadow_color=shadow_write_valid=shadow_read_valid=shadow_frame_ready=0;
}
static int shadow_create(struct bfv_mod *mod,uint32_t dev) {
    if(shadow_color)return 1;
    if(shadow_failed)return 0;
    for(int i=0;i<2;i++) {
        // D3DUSAGE_DEPTHSTENCIL, D3DFMT_D24X8: sampled with hardware comparison.
        if(GAME_STDCALL(GAME_VIRTUAL(dev,23),dev,SHADOW_W,SHADOW_H,1,2,77,0,WPTR(&shadow_tex[i]),0)||!shadow_tex[i])goto fail;
        if(GAME_STDCALL(GAME_VIRTUAL(shadow_tex[i],18),shadow_tex[i],0,WPTR(&shadow_surf[i]))||!shadow_surf[i])goto fail;
    }
    // A colour target is still required; R5G6B5, never written.
    if(GAME_STDCALL(GAME_VIRTUAL(dev,28),dev,SHADOW_W,SHADOW_H,23,0,0,0,WPTR(&shadow_color),0)||!shadow_color)goto fail;
    world_log(mod,"world: sun shadow cascades allocated (2 x 4096x2048 D24X8)\r\n");
    return 1;
fail:
    shadow_drop(mod);shadow_failed=1;
    world_log(mod,"world: ERROR shadow depth textures unavailable, sun shadows off\r\n");
    return 0;
}
static float wfloor(float x){int i=(int)x;return (float)(x<(float)i?i-1:i);}
static float dot3(const float *a,const float *b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];}
static void norm3(float *v){float l=mod_sqrtf(dot3(v,v));if(l>0){v[0]/=l;v[1]/=l;v[2]/=l;}}
static void cross3(float *o,const float *a,const float *b){
    o[0]=a[1]*b[2]-a[2]*b[1];o[1]=a[2]*b[0]-a[0]*b[2];o[2]=a[0]*b[1]-a[1]*b[0];
}
/* Frame start: last frame's map becomes the one the draws sample. */
static void shadow_frame(void) {
    if(shadow_write_valid) {
        for(int c=0;c<2;c++) {
            // Texture space of cascade c in the atlas, with the D3D9 half texel.
            float t[16]={0.25f,0,0,0, 0,-0.5f,0,0, 0,0,1,0,
                         0.25f+0.5f*(float)c+0.5f/SHADOW_W,0.5f+0.5f/SHADOW_H,0,1};
            multiply(shadow_sample[c],shadow_vp[c],t);
        }
    }
    shadow_read_valid=shadow_write_valid;
    shadow_write^=1;shadow_write_valid=0;shadow_cleared=0;shadow_frame_ready=0;
}
static void shadow_params(struct bfv_mod *mod,uint32_t e) {
    uint32_t on=shadow_on&&world_enabled&&shadow_read_valid&&shadow_color;
    float params[4]={on?1.0f:0.0f,0.00008f,0.08f,1.0f/SHADOW_W};
    set_vec(mod,e,"ShadowParams",params);
    GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("ShadowPass"),GAME_FLOAT(0.0f));
    GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("WorldHdrScale"),GAME_FLOAT(world_shared.post_on?1.1f:1.0f));
    if(!on)return;
    set_matrix(mod,e,"ShadowMatrix0",WPTR(shadow_sample[0]));
    set_matrix(mod,e,"ShadowMatrix1",WPTR(shadow_sample[1]));
    GAME_STDCALL(GAME_VIRTUAL(e,0xd0/4),e,WPTR("ShadowMap"),shadow_tex[shadow_write^1]);
}
static void camera_capture(struct bfv_mod *mod) {
    if(camera_frame_id==world_frame_id)return;
    camera_frame_id=world_frame_id;
    uint32_t mgr=M32(g_pShaderManager_00e20b10),sky=M32(g_pSky);
    world_shared.camera_valid=0;
    if(!mgr)return;
    float *v=world_shared.view,*p=world_shared.proj;
    for(int i=0;i<16;i++){v[i]=F32(mgr+0x14+4*i);p[i]=F32(mgr+0x94+4*i);}
    multiply(world_shared.viewproj,v,p);
    for(int k=0;k<3;k++){world_shared.right[k]=v[k*4];world_shared.up[k]=v[k*4+1];world_shared.forward[k]=v[k*4+2];}
    for(int k=0;k<3;k++)world_shared.camera[k]=-(v[12]*world_shared.right[k]+v[13]*world_shared.up[k]+v[14]*world_shared.forward[k]);
    world_shared.camera_valid=1;world_shared.frame=world_frame_id;
    world_shared.water_height=water_height;world_shared.water_seen=water_seen;
    if(sky) {
        float sun[3]={F32(sky+0x3c),F32(sky+0x40),F32(sky+0x44)};norm3(sun);
        for(int k=0;k<3;k++)world_shared.sun[k]=sun[k];
    }
    uint32_t light=GAME_THISCALL(ShaderManager_RaShader_helper_00a49bd0,mgr,0);
    if(light)for(int k=0;k<3;k++){float c=F32(light+0x0c+4*k)*2;world_shared.sun_color[k]=c*c;}
    /* Cascades for this frame's map. */
    if(!shadow_on||!world_enabled||!sky||world_shared.sun[1]<0.05f)return;
    float *x=shadow_axis[0],*y=shadow_axis[1],*z=shadow_axis[2],upv[3]={0,1,0};
    for(int k=0;k<3;k++)z[k]=-world_shared.sun[k];
    if(mod_fabsf(z[1])>0.99f){upv[1]=0;upv[2]=1;}
    cross3(x,upv,z);norm3(x);cross3(y,z,x);
    for(int c=0;c<2;c++) {
        float r=shadow_radius[c],*ctr=shadow_center[c],*m=shadow_vp[c];
        for(int k=0;k<3;k++)ctr[k]=world_shared.camera[k]+world_shared.forward[k]*r*0.6f;
        // Snap the centre to whole texels so the map does not shimmer.
        float texel=2*r/SHADOW_H,cx=wfloor(dot3(x,ctr)/texel)*texel,cy=wfloor(dot3(y,ctr)/texel)*texel,cz=dot3(z,ctr);
        for(int k=0;k<3;k++){m[k*4]=x[k]/r;m[k*4+1]=y[k]/r;m[k*4+2]=z[k]/1200;m[k*4+3]=0;}
        m[12]=-cx/r;m[13]=-cy/r;m[14]=(600-cz)/1200;m[15]=1;
    }
    shadow_frame_ready=1;
}
static int main_view(uint32_t mgr) {
    for(int i=0;i<16;i++)if(M32(mgr+0x14+4*i)!=bfv_mod_bits(world_shared.view[i]))return 0;
    return 1;
}
static void shadow_draw(struct bfv_mod *mod,uint32_t s,uint32_t item) {
    uint32_t e=M32(s+0x54),dev=M32(s+0x48),terrain=s==M32(g_pRaShaderTerrain_00e173c8);
    if(!shadow_frame_ready||!item||!e||!dev||!world_shared.camera_valid)return;
    if(!world_mesh(s)&&!terrain)return;
    uint32_t obj=M32(item+8),mgr=M32(g_pShaderManager_00e20b10);
    if(!obj||!mgr||!main_view(mgr))return;
    float pos[3]={F32(obj+48),F32(obj+52),F32(obj+56)},d[3];
    for(int k=0;k<3;k++)d[k]=pos[k]-world_shared.camera[k];
    if(!terrain&&dot3(d,d)<2.25f)return; // the viewer's own body or vehicle seat
    uint32_t draw[2];
    for(int c=0;c<2;c++) {
        for(int k=0;k<3;k++)d[k]=pos[k]-shadow_center[c][k];
        float lx=mod_fabsf(dot3(d,shadow_axis[0])),ly=mod_fabsf(dot3(d,shadow_axis[1]));
        draw[c]=terrain||(lx<shadow_radius[c]+60&&ly<shadow_radius[c]+60);
    }
    if(!draw[0]&&!draw[1])return;
    if(!shadow_create(mod,dev))return;
    uint32_t rt=0,rt1=0,depth=0,vp[6]={0},saved[8],names[8]={168,7,14,23,195,175,22,27};
    if(GAME_STDCALL(GAME_VIRTUAL(dev,38),dev,0,WPTR(&rt))||!rt)return;
    GAME_STDCALL(GAME_VIRTUAL(dev,38),dev,1,WPTR(&rt1));
    GAME_STDCALL(GAME_VIRTUAL(dev,40),dev,WPTR(&depth));
    GAME_STDCALL(GAME_VIRTUAL(dev,48),dev,WPTR(vp));
    for(int i=0;i<8;i++)GAME_STDCALL(GAME_VIRTUAL(dev,58),dev,names[i],WPTR(&saved[i]));
    shadow_busy=1;
    GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,0);
    GAME_STDCALL(GAME_VIRTUAL(dev,37),dev,1,0);
    if(!GAME_STDCALL(GAME_VIRTUAL(dev,37),dev,0,shadow_color)) {
        GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,shadow_surf[shadow_write]);
        if(!shadow_cleared) {
            // A stale sampler binding of this atlas (it was last frame's
            // shadow map) must not stay while it is the depth target.
            for(uint32_t st=0;st<8;st++) {
                uint32_t t=0;GAME_STDCALL(GAME_VIRTUAL(dev,64),dev,st,WPTR(&t));
                if(t){if(t==shadow_tex[shadow_write])GAME_STDCALL(GAME_VIRTUAL(dev,65),dev,st,0);GAME_STDCALL(GAME_VIRTUAL(t,2),t);}
            }
            GAME_STDCALL(GAME_VIRTUAL(dev,43),dev,0,0,2 /* Z */,0,GAME_FLOAT(1.0f),0);
            shadow_cleared=1;shadow_write_valid=1;
        }
        float plane[4]={0,0,0,1};set_vec(mod,e,"ReflectionPlane",plane);
        GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("ReflectionPass"),GAME_FLOAT(1.0f));
        GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("ShadowPass"),GAME_FLOAT(1.0f));
        for(int c=0;c<2;c++) {
            if(!draw[c])continue;
            float wvp[16];
            multiply(wvp,(const float *)(uintptr_t)obj,shadow_vp[c]);
            set_matrix(mod,e,"ReflectionWVP",WPTR(wvp));
            // Effect slot 0x104 re-applies the pass block (cull, depth write,
            // blend), so the caster states are set after it.
            GAME_STDCALL(GAME_VIRTUAL(e,0x104/4),e,0);
            GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,168,0);   // colour writes off
            GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,7,1);
            GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,14,1);
            GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,23,4);
            GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,195,GAME_FLOAT(0.00002f));
            GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,175,GAME_FLOAT(1.5f));
            GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,22,1);    // both faces: thin cards cast
            GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,27,0);
            uint32_t cvp[6]={(uint32_t)c*SHADOW_H,0,SHADOW_H,SHADOW_H,0,0x3f800000};
            GAME_STDCALL(GAME_VIRTUAL(dev,47),dev,WPTR(cvp));
            GAME_THISCALL(0x00a4b490,s,item);
        }
        if(!shadow_casts++)world_log(mod,"world: sun shadow casters replayed\r\n");
    }
    GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,0);
    GAME_STDCALL(GAME_VIRTUAL(dev,37),dev,0,rt);
    GAME_STDCALL(GAME_VIRTUAL(dev,37),dev,1,rt1);
    GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,depth);
    GAME_STDCALL(GAME_VIRTUAL(dev,47),dev,WPTR(vp));
    for(int i=0;i<8;i++)GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,names[i],saved[i]);
    GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("ReflectionPass"),GAME_FLOAT(0.0f));
    GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("ShadowPass"),GAME_FLOAT(0.0f));
    GAME_STDCALL(GAME_VIRTUAL(e,0x104/4),e,0);
    shadow_busy=0;
    if(depth)GAME_STDCALL(GAME_VIRTUAL(depth,2),depth);
    if(rt1)GAME_STDCALL(GAME_VIRTUAL(rt1,2),rt1);
    GAME_STDCALL(GAME_VIRTUAL(rt,2),rt);
}

// Replay submitted opaque geometry with a plane-reflected world transform.
// This is actual scene geometry, not a flipped copy of the displayed image.
// Main-view visibility is retained; objects culled from that view are absent.
MOD_BEFORE(0x00a4b490, world_reflection_draw) {
    uint32_t s=MOD_THIS,item=MOD_ARG(0),e=M32(s+0x54),dev=M32(s+0x48);
    if(!reflection_busy&&!shadow_busy) {
        // Only world draws carry the main camera (not sky or other passes).
        if(world_mesh(s)||s==M32(g_pRaShaderTerrain_00e173c8))camera_capture(mod);
        if(world_shared.scene_active)world_shared.world_drawn=1;
        shadow_draw(mod,s,item);
    }
    // The shadow replay re-enters this hook; it must not draw the reflection.
    if(shadow_busy||reflection_busy||!reflection_ready||!world_enabled||!item||!e||!dev)return;
    if(!world_mesh(s)&&s!=M32(g_pRaShaderTerrain_00e173c8))return;
    uint32_t obj=M32(item+8),rt=0,rt1=0,depth=0,vp[6]={0},cull=1;
    if(!obj)return;
    uint32_t mgr=M32(g_pShaderManager_00e20b10);
    if(!mgr)return;
    float mirror[16]={1,0,0,0, 0,-1,0,0, 0,0,1,0, 0,0,0,1},wm[16],wvp[16];
    mirror[13]=2*water_height;
    uint32_t viewproj=GAME_THISCALL(ShaderManager_sub_00a49420,mgr);
    multiply(wm,(const float *)(uintptr_t)obj,mirror);
    multiply(wvp,wm,(const float *)(uintptr_t)viewproj);
    if(GAME_STDCALL(GAME_VIRTUAL(dev,38),dev,0,WPTR(&rt))||!rt)return;
    GAME_STDCALL(GAME_VIRTUAL(dev,38),dev,1,WPTR(&rt1));
    GAME_STDCALL(GAME_VIRTUAL(dev,40),dev,WPTR(&depth));
    GAME_STDCALL(GAME_VIRTUAL(dev,48),dev,WPTR(vp));
    GAME_STDCALL(GAME_VIRTUAL(dev,58),dev,22,WPTR(&cull));
    reflection_busy=1;
    set_matrix(mod,e,"ReflectionWVP",WPTR(wvp));
    float plane[4]={0,1,0,-water_height};set_vec(mod,e,"ReflectionPlane",plane);
    GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("ReflectionPass"),GAME_FLOAT(1.0f));
    GAME_STDCALL(GAME_VIRTUAL(e,0x104/4),e,0);
    GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,22,cull==2?3:cull==3?2:1);
    GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,0);
    GAME_STDCALL(GAME_VIRTUAL(dev,37),dev,1,0);
    if(!GAME_STDCALL(GAME_VIRTUAL(dev,37),dev,0,reflection_target)) {
        GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,reflection_depth);
        uint32_t small[6]={0,0,reflection_width,reflection_height,0,0x3f800000};
        GAME_STDCALL(GAME_VIRTUAL(dev,47),dev,WPTR(small));
        GAME_THISCALL(0x00a4b490,s,item);
    }
    GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,0);
    GAME_STDCALL(GAME_VIRTUAL(dev,37),dev,0,rt);
    GAME_STDCALL(GAME_VIRTUAL(dev,37),dev,1,rt1);
    GAME_STDCALL(GAME_VIRTUAL(dev,39),dev,depth);
    GAME_STDCALL(GAME_VIRTUAL(dev,47),dev,WPTR(vp));
    GAME_STDCALL(GAME_VIRTUAL(e,0x78/4),e,WPTR("ReflectionPass"),GAME_FLOAT(0.0f));
    GAME_STDCALL(GAME_VIRTUAL(e,0x104/4),e,0);
    GAME_STDCALL(GAME_VIRTUAL(dev,57),dev,22,cull);
    reflection_busy=0;
    if(depth)GAME_STDCALL(GAME_VIRTUAL(depth,2),depth);
    if(rt1)GAME_STDCALL(GAME_VIRTUAL(rt1,2),rt1);
    GAME_STDCALL(GAME_VIRTUAL(rt,2),rt);
}
MOD_BEFORE(0x00a5fbc0, world_reset) {reflection_drop(mod);shadow_drop(mod);water_seen=0;}
MOD_BEFORE(0x00a5f650, world_release) {
    reflection_drop(mod);shadow_drop(mod);water_seen=0;
    if(world_sky)GAME_THISCALL(GAME_VIRTUAL(world_sky,2),world_sky);
    world_sky=world_sky_missing=0;
}
