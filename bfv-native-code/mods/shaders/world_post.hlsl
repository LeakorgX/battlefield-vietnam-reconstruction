// World post chain for the FP16 scene target (docs/RENDERER-PLAN.md).
// The scene target holds display-encoded colour, as native draws write it;
// upgraded materials may exceed 1. The composite pass decodes it to linear
// light once; every later pass works in linear light; finish applies
// exposure and tone mapping and encodes for the back buffer. The HUD is
// drawn after finish.
//
// All passes are vs_3_0/ps_3_0 fullscreen quads. The vertex carries a
// world-space ray per corner, scaled so that view depth 1 is one ray, so a
// pixel's world position is camera + ray * linear depth.

float4 P0:register(c0);
float4 P1:register(c1);
float4 P2:register(c2);
float4 P3:register(c3);
float4 P4:register(c4);
float4 P5:register(c5);
row_major float4x4 Mat:register(c8);
sampler2D S0:register(s0);
sampler2D S1:register(s1);
sampler2D S2:register(s2);

struct PostV { float4 pos:POSITION; float2 uv:TEXCOORD0; float3 ray:TEXCOORD1; };
PostV post_vs(float3 p:POSITION,float2 uv:TEXCOORD0,float3 ray:TEXCOORD1) {
    PostV o; o.pos=float4(p.xy,0.5,1); o.uv=uv; o.ray=ray; return o;
}
struct PostP { float2 uv:TEXCOORD0; float3 ray:TEXCOORD1; float2 vpos:VPOS; };

static const float3 LUMA=float3(0.2126,0.7152,0.0722);
float3 decode(float3 c) { return pow(max(c,0),2.2); }
float3 encode(float3 c) { return pow(saturate(c),1/2.2); }

// Linear view depth. P0 = (P22, P32) of the projection the current depth
// was drawn with, P0.z = 1 when a saved main-scene depth (S1) holds the
// pixels a mid-frame depth clear removed, P0.w = far value for empty depth.
float4 depth_lin(PostP i):COLOR0 {
    float d=tex2D(S0,i.uv).r;
    float saved=tex2D(S1,i.uv).r;
    float z=d<0.999999?P0.y/(d-P0.x):(P0.z>0.5?saved:P0.w);
    return float4(z,0,0,1);
}

// Screen-space ambient occlusion, half resolution (Alchemy estimator).
// S0 linear depth (full resolution). P0 = (1/w, 1/h, w, h) of the depth,
// P1 = (1/P00, 1/P11, radius m, intensity), P2 = (unused, sky depth).
// The sample rotation repeats over 4x4 pixels and composite blurs exactly
// that tile, so every pixel sees all 16 rotations and no noise pattern is
// left (a per-pixel hash without a blur reads as diagonal lines).
float3 view_pos(float2 uv) {
    float z=tex2Dlod(S0,float4(uv,0,0)).r;
    return float3((uv*float2(2,-2)+float2(-1,1))*P1.xy*z,z);
}
float4 ssao(PostP i):COLOR0 {
    float3 p=view_pos(i.uv);
    if(p.z>P2.y) return 1;
    float3 pr=view_pos(i.uv+float2(P0.x,0)),pl=view_pos(i.uv-float2(P0.x,0));
    float3 pd=view_pos(i.uv+float2(0,P0.y)),pu=view_pos(i.uv-float2(0,P0.y));
    float3 dx=abs(pr.z-p.z)<abs(pl.z-p.z)?pr-p:p-pl;
    float3 dy=abs(pd.z-p.z)<abs(pu.z-p.z)?pd-p:p-pu;
    float3 n=normalize(cross(dx,dy));
    n=dot(n,p)>0?-n:n;
    float2 q=fmod(floor(i.vpos),4);
    float angle=(q.x*4+q.y+fmod(q.x+q.y,2)*8)/16*6.2831853;
    float radius=P1.z;
    float r=clamp(radius*0.5/(p.z*P1.y),3*P0.y,0.12);
    float aspect=P0.w*P0.x;
    float sum=0;
    for(int k=0;k<12;k++) {
        float t=(k+0.5)/12;
        float a=angle+k*2.39996;
        float2 o=float2(cos(a)*aspect,sin(a))*(r*t);
        float3 v=view_pos(i.uv+o)-p;
        float vv=dot(v,v);
        float falloff=saturate(1-vv/(4*radius*radius));
        sum+=max(dot(v,n)-0.004*p.z,0)/(vv+0.02*radius*radius)*falloff;
    }
    float ao=saturate(1-P1.w*2*sum/12);
    return float4(ao*ao,0,0,1);
}

// Decode the scene once, apply ambient occlusion and height fog.
// S0 scene, S1 AO (half), S2 linear depth.
// P0 = (1/w, 1/h, AO strength, AO on), P1 = (fog density, falloff, base y, max),
// P2 = (camera xyz, fog start), P3 = (fog colour linear, fog on),
// P4 = (sun direction, sky depth), P5 = (sun colour linear, sky distance).
float4 composite(PostP i):COLOR0 {
    float3 c=decode(tex2D(S0,i.uv).rgb);
    float z=tex2D(S2,i.uv).r;
    // Depth-aware 4x4 blur of the half-resolution occlusion: one full
    // rotation tile, which removes the SSAO noise pattern.
    float ao=0,wsum=0;
    for(int y=0;y<4;y++) for(int x=0;x<4;x++) {
        float2 o=(float2(x,y)-1.5)*2*P0.xy;
        float zs=tex2Dlod(S2,float4(i.uv+o,0,0)).r;
        float w=1/(abs(zs-z)/max(z*0.04,0.05)+0.05);
        ao+=tex2Dlod(S1,float4(i.uv+o,0,0)).r*w;wsum+=w;
    }
    ao=ao/wsum;
    // Occlusion fades with distance: far depth is too coarse for it.
    c*=lerp(1,ao,P0.z*P0.w*saturate(1.5-z/60));
    // The engine already fogs every world draw and the sky has its own
    // horizon: this is only a light height term over geometry.
    if(z>P4.w) return float4(c,1);
    float dist=z*length(i.ray);
    float3 dir=normalize(i.ray);
    float d=max(dist-P2.w,0);
    float k2=P1.y*dir.y;
    float path=abs(k2)>0.0001?(1-exp(-k2*d))/k2:d;
    float f=min(P1.x*exp(-P1.y*(P2.y-P1.z))*path,50);
    float amount=min(1-exp(-f),P1.w)*P3.w;
    float scatter=pow(saturate(dot(dir,P4.xyz)),8);
    c=lerp(c,P3.rgb+P5.rgb*scatter*0.3,amount);
    return float4(c,1);
}

// Edge antialiasing (FXAA 3.11 quality, after Lottes), on one frame only:
// no history and no projection jitter, so nothing trails or ghosts when the
// camera or an object moves. S0 linear scene (bilinear), P0 = (1/w, 1/h,
// subpixel amount, 0). Luma and the final read use c/(1+max(c)), so bright
// highlights do not decide the edges alone.
float3 tm(float3 c) { return c/(1+max(c.r,max(c.g,c.b))); }
float3 itm(float3 c) { return c/max(1-max(c.r,max(c.g,c.b)),0.0001); }
float3 aa_read(float2 uv) { return tm(tex2Dlod(S0,float4(uv,0,0)).rgb); }
float aa_luma(float2 uv) { return sqrt(dot(aa_read(uv),LUMA)); }
float4 aa(PostP i):COLOR0 {
    float2 px=P0.xy,uv=i.uv;
    float3 cM=aa_read(uv);
    float lM=sqrt(dot(cM,LUMA));
    float lN=aa_luma(uv+float2(0,-px.y)),lS=aa_luma(uv+float2(0,px.y));
    float lE=aa_luma(uv+float2(px.x,0)),lW=aa_luma(uv+float2(-px.x,0));
    float lmax=max(lM,max(max(lN,lS),max(lE,lW))),lmin=min(lM,min(min(lN,lS),min(lE,lW)));
    float range=lmax-lmin;
    if(range<max(0.0312,lmax*0.125)) return float4(itm(cM),1);
    float lNW=aa_luma(uv-px),lSE=aa_luma(uv+px);
    float lNE=aa_luma(uv+float2(px.x,-px.y)),lSW=aa_luma(uv+float2(-px.x,px.y));
    float edgeH=abs(lNW+lNE-2*lN)+2*abs(lW+lE-2*lM)+abs(lSW+lSE-2*lS);
    float edgeV=abs(lNW+lSW-2*lW)+2*abs(lN+lS-2*lM)+abs(lNE+lSE-2*lE);
    bool horz=edgeH>=edgeV;
    float l1=horz?lN:lW,l2=horz?lS:lE;
    float g1=abs(l1-lM),g2=abs(l2-lM);
    float stepLen=horz?px.y:px.x,local=0.5*(l2+lM);
    if(g1>=g2){stepLen=-stepLen;local=0.5*(l1+lM);}
    float grad=0.25*max(g1,g2);
    float2 cur=uv+(horz?float2(0,stepLen):float2(stepLen,0))*0.5;
    float2 off=horz?float2(px.x,0):float2(0,px.y);
    float2 u1=cur-off,u2=cur+off;
    float e1=aa_luma(u1)-local,e2=aa_luma(u2)-local;
    bool r1=abs(e1)>=grad,r2=abs(e2)>=grad;
    [unroll] for(int k=0;k<10;k++) {
        float q=k<4?1:k==4?1.5:k<8?2:k==8?4:8;
        if(!r1){u1-=off*q;e1=aa_luma(u1)-local;r1=abs(e1)>=grad;}
        if(!r2){u2+=off*q;e2=aa_luma(u2)-local;r2=abs(e2)>=grad;}
    }
    float d1=horz?uv.x-u1.x:uv.y-u1.y,d2=horz?u2.x-uv.x:u2.y-uv.y;
    bool closer1=d1<d2;
    float o=0.5-min(d1,d2)/(d1+d2);
    bool correct=((closer1?e1:e2)<0)!=(lM<local);
    o=correct?o:0;
    float avg=(2*(lN+lS+lE+lW)+lNW+lNE+lSW+lSE)/12;
    float sub=saturate(abs(avg-lM)/range);
    sub=(-2*sub+3)*sub*sub;
    o=max(o,sub*sub*P0.z);
    return float4(itm(aa_read(uv+(horz?float2(0,o*stepLen):float2(o*stepLen,0)))),1);
}

// Average scene luminance with eye adaptation. S0 the smallest bloom level
// (pure downsample), S1 the previous adapted value.
// P0 = (adaptation step 0..1, reset).
float4 adapt(PostP i):COLOR0 {
    float s=0,wsum=0;
    for(int y=0;y<8;y++) for(int x=0;x<8;x++) {
        float2 uv=(float2(x,y)+0.5)/8;
        float w=1.25-length(uv-0.5);
        s+=log(max(dot(tex2Dlod(S0,float4(uv,0,0)).rgb,LUMA),0.0001))*w;wsum+=w;
    }
    float avg=exp(s/wsum);
    float prev=tex2D(S1,float2(0.5,0.5)).r;
    return float4(P0.y>0.5||!(prev>0)?avg:prev+(avg-prev)*P0.x,0,0,1);
}

// Bloom downsample, 13 taps (Jimenez, SIGGRAPH 2014). S0 the larger level,
// P0 = (1/w, 1/h of the source, Karis average on the first level).
float3 karis(float3 a,float3 b,float3 c,float3 d,float on) {
    float4 w=1/(1+float4(dot(a,LUMA),dot(b,LUMA),dot(c,LUMA),dot(d,LUMA)));
    w=lerp(1,w,on);
    return (a*w.x+b*w.y+c*w.z+d*w.w)/dot(w,1);
}
float4 bloom_down(PostP i):COLOR0 {
    float2 t=P0.xy;
    float3 a=tex2D(S0,i.uv+t*float2(-2,-2)).rgb,b=tex2D(S0,i.uv+t*float2(0,-2)).rgb,c=tex2D(S0,i.uv+t*float2(2,-2)).rgb;
    float3 d=tex2D(S0,i.uv+t*float2(-1,-1)).rgb,e=tex2D(S0,i.uv+t*float2(1,-1)).rgb;
    float3 f=tex2D(S0,i.uv+t*float2(-2,0)).rgb,g=tex2D(S0,i.uv).rgb,h=tex2D(S0,i.uv+t*float2(2,0)).rgb;
    float3 j=tex2D(S0,i.uv+t*float2(-1,1)).rgb,k=tex2D(S0,i.uv+t*float2(1,1)).rgb;
    float3 l=tex2D(S0,i.uv+t*float2(-2,2)).rgb,m=tex2D(S0,i.uv+t*float2(0,2)).rgb,n=tex2D(S0,i.uv+t*float2(2,2)).rgb;
    float on=P0.z;
    float3 r=karis(d,e,j,k,on)*0.5+(karis(a,b,f,g,on)+karis(b,c,g,h,on)+karis(f,g,l,m,on)+karis(g,h,m,n,on))*0.125;
    return float4(min(r,64),1);
}
// Bloom upsample, 3x3 tent, added to the larger level by blending.
// S0 the smaller level, P0 = (1/w, 1/h of the source).
float4 bloom_up(PostP i):COLOR0 {
    float2 t=P0.xy;
    float3 s=tex2D(S0,i.uv).rgb*4;
    s+=(tex2D(S0,i.uv+t*float2(-1,0)).rgb+tex2D(S0,i.uv+t*float2(1,0)).rgb+tex2D(S0,i.uv+t*float2(0,-1)).rgb+tex2D(S0,i.uv+t*float2(0,1)).rgb)*2;
    s+=tex2D(S0,i.uv+t*float2(-1,-1)).rgb+tex2D(S0,i.uv+t*float2(1,-1)).rgb+tex2D(S0,i.uv+t*float2(-1,1)).rgb+tex2D(S0,i.uv+t*float2(1,1)).rgb;
    return float4(s/16,1);
}

// Hue-preserving shoulder: identity below 0.8, so native colours and the
// plain resolve match; above it the brightest channel rolls off to 1 and
// the others scale with it. No toe and no desaturation toward white (those
// washed out sunlit surfaces).
float3 shoulder(float3 c) {
    float peak=max(c.r,max(c.g,c.b));
    if(peak<=0.8) return c;
    float np=0.8+0.2*(1-exp(-(peak-0.8)/0.2));
    return c*(np/peak);
}
// S0 linear scene (or the encoded scene when P2.w = 1), S1 bloom (half),
// S2 adapted luminance. P0 = (1/w, 1/h, bloom weight, sharpen),
// P1 = (exposure key, min, max, post on), P2 = (bloom normalisation,
// saturation, contrast, decode input).
float4 finish(PostP i):COLOR0 {
    float3 c=tex2D(S0,i.uv).rgb;
    if(P2.w>0.5) return float4(encode(decode(c)),1);
    // Sharpening against the softness of texture filtering, kept inside
    // the neighbours' range so high-contrast edges get no halo lines.
    float3 a=tex2D(S0,i.uv+float2(P0.x,0)).rgb,b2=tex2D(S0,i.uv-float2(P0.x,0)).rgb;
    float3 u=tex2D(S0,i.uv+float2(0,P0.y)).rgb,d=tex2D(S0,i.uv-float2(0,P0.y)).rgb;
    float3 lo=min(c,min(min(a,b2),min(u,d))),hi=max(c,max(max(a,b2),max(u,d)));
    c=clamp(c+(c-(a+b2+u+d)*0.25)*P0.w,lo,hi);
    float3 b=tex2D(S1,i.uv).rgb*P2.x;
    c=lerp(c,b,P0.z);
    float avg=tex2D(S2,float2(0.5,0.5)).r;
    float e=clamp(P1.x/max(avg,0.0001),P1.y,P1.z);
    c=shoulder(c*e);
    c=max(lerp(dot(c,LUMA),c,P2.y),0);
    // Gentle S-curve in display space for contrast.
    float3 o=encode(c);
    o=lerp(o,o*o*(3-2*o),P2.z);
    return float4(o,1);
}
