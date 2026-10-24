// Authored world-material replacement. Retail interfaces, new lighting.
// WR_NORMAL: retail RGB tangent normal + gloss in A (NOT the RPG DXT5nm).
// WR_LIGHTMAP: preserve the level's static occlusion. WR_SKIN: 4-bone skinning.
// WR_LIGHTMAP_FF: the fixed-function RaShaderPVLS1DifLmp, whose native colour
// is albedo * saturate(lightmap.rgb * vertex light + textureFactor). The
// engine sets textureFactor per material (00a4b5a0): it is the light floor of
// lightmapped meshes, which are usually the near LODs.
#include "effects/RaCommon.fx"
float4x4 World;
float4x4 ReflectionWVP;
float ReflectionPass=0;
float4 ReflectionPlane=float4(0,1,0,0);
// Sun shadow cascades (renderer plan stage 8), written by world_render.c.
// ShadowParams = (on, depth bias, normal offset m, atlas texel u).
float4x4 ShadowMatrix0, ShadowMatrix1;
float4 ShadowParams=float4(0,0.0002,0.08,0.000244);
float ShadowPass=0;
float WorldHdrScale=1;
float2 SkyYaw=float2(1,0);   // the HD sky turn (world_render.c sky_yaw)
texture ShadowMap;
float WorldUpgradeEnabled=1;
#ifdef WR_LIGHTMAP_FF
float4 textureFactor=float4(.5,.5,.5,1);
float LightMapBound=1;   // 0: no lightmap texture, which fixed function reads as white
#endif
float4 WorldSurfaceParams = float4(0.65, 0.04, 0.012, 1.0);
float4 bones[84];
float4 UVWSpeed = 0;
int srcBlend=5, destBlend=6, cullMode=1, alphaRef=50;
bool alphaBlendEnable=true;
float opacity=1;
float reflectivity=0.5;
texture LightMap;
sampler WorldAlbedo = sampler_state {
 Texture=(DiffuseMap); MinFilter=ANISOTROPIC; MaxAnisotropy=8;
 MagFilter=LINEAR; MipFilter=LINEAR; AddressU=WRAP; AddressV=WRAP;
};
sampler WorldNormal = sampler_state {
 Texture=(NormalMap); MinFilter=ANISOTROPIC; MaxAnisotropy=8;
 MagFilter=LINEAR; MipFilter=LINEAR; AddressU=WRAP; AddressV=WRAP;
};
sampler WorldLightmap = sampler_state {
 Texture=(LightMap); MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR;
};
sampler WorldShadow = sampler_state {
 Texture=(ShadowMap); MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=NONE;
 AddressU=CLAMP; AddressV=CLAMP;
};
sampler WorldSky = sampler_state {
 Texture=(skyCube); MinFilter=LINEAR; MagFilter=LINEAR; MipFilter=LINEAR;
 AddressU=CLAMP; AddressV=CLAMP; AddressW=CLAMP;
};
struct WorldVarying {
 float4 pos:POSITION; float2 uv:TEXCOORD0; float3 p:TEXCOORD1;
 float3 n:TEXCOORD2; float2 lm:TEXCOORD3;
#ifdef WR_NORMAL
 float3 t:TEXCOORD4; float3 b:TEXCOORD5;
#endif
 float4 shadow1:TEXCOORD6;   // far cascade, reflection plane height
 float3 shadow0:TEXCOORD7;
 float fog:FOG;
};
WorldVarying WorldVS(float4 p:POSITION, float3 n:NORMAL, float2 uv:TEXCOORD0
#ifdef WR_LIGHTMAP
 ,float2 lm:TEXCOORD1
#endif
#ifdef WR_NORMAL
 ,float3 t:TANGENT, float3 b:BINORMAL
#endif
#ifdef WR_SKIN
 ,float4 weights:BLENDWEIGHT, float4 indices:BLENDINDICES
#endif
) {
 WorldVarying o=(WorldVarying)0;
#ifdef WR_SKIN
 int4 ids=(int4)D3DCOLORtoUBYTE4(indices);
 weights.w=1-dot(weights.xyz,1);
 float3x4 skin=(float3x4)0;
 for(int j=0;j<4;j++) {
  skin[0]+=bones[ids[j]]*weights[j];
  skin[1]+=bones[ids[j]+1]*weights[j];
  skin[2]+=bones[ids[j]+2]*weights[j];
 }
 p=float4(dot(p,skin[0]),dot(p,skin[1]),dot(p,skin[2]),1);
 n=mul((float3x3)skin,n);
#ifdef WR_NORMAL
 t=mul((float3x3)skin,t); b=mul((float3x3)skin,b);
#endif
#endif
 float4 worldP=mul(p,World);
 o.pos=lerp(mul(p,WorldViewProjection),mul(p,ReflectionWVP),ReflectionPass); o.p=p.xyz; o.n=n;
 o.uv=uv-UVWSpeed.xy;
#ifdef WR_LIGHTMAP
 o.lm=lm;
#endif
#ifdef WR_NORMAL
 o.t=t; o.b=b;
#endif
 // The retail fog factor; the device blends the map's fog colour (FOGENABLE
 // is left to the engine, as in the retail effects).
 o.fog=calcFog(FogData,o.pos.w);
 float4 shadowP=float4(worldP.xyz+normalize(mul(n,(float3x3)World))*ShadowParams.z,1);
 o.shadow0=mul(shadowP,ShadowMatrix0).xyz;
 o.shadow1=float4(mul(shadowP,ShadowMatrix1).xyz,dot(worldP,ReflectionPlane));
 return o;
}
float3 worldLinear(float3 c) { return pow(max(c,0),2.2); }
// Two cascades side by side in one hardware-compared depth atlas; 4 bilinear
// PCF taps. The vertex shader offsets the position along the normal against
// acne and projects it into both cascades (orthographic: exact per pixel).
float worldShadow(float3 a,float3 b) {
 float2 e0=abs(a.xy-float2(0.25,0.5)),e1=abs(b.xy-float2(0.75,0.5));
 float in0=(e0.x<0.245&&e0.y<0.49&&a.z<1)?1:0;
 float in1=(e1.x<0.245&&e1.y<0.49&&b.z<1)?1:0;
 float3 s=lerp(b,a,in0);
 float o=ShadowParams.w*0.75;
 s.z-=ShadowParams.y*lerp(2.5,1,in0);
 float v=tex2Dproj(WorldShadow,float4(s.xy+float2(-o,-o)*float2(1,2),s.z,1)).r
        +tex2Dproj(WorldShadow,float4(s.xy+float2( o,-o)*float2(1,2),s.z,1)).r
        +tex2Dproj(WorldShadow,float4(s.xy+float2(-o, o)*float2(1,2),s.z,1)).r
        +tex2Dproj(WorldShadow,float4(s.xy+float2( o, o)*float2(1,2),s.z,1)).r;
 // Fade out over the outer part of the far cascade.
 float fade=saturate((0.49-max(e1.x*2,e1.y))*12);
 return lerp(1,v*0.25,ShadowParams.x*max(in0,in1*fade));
}
// Surface-gradient bump mapping: relief inferred from diffuse detail where no
// authored normal exists. It is deliberately small and is NOT a metalness map.
float3 worldRelief(float3 n,float3 p,float h,float amount) {
 float3 px=ddx(p),py=ddy(p);
 float3 r1=cross(py,n),r2=cross(n,px);
 float det=dot(px,r1);
 float3 grad=(ddx(h)*r1+ddy(h)*r2)*(sign(det)/max(abs(det),0.00001));
 return normalize(n-grad*amount);
}
float3 worldGGX(float3 n,float3 v,float3 l,float rough,float3 f0,float3 base) {
 float nl=saturate(dot(n,l)),nv=max(saturate(dot(n,v)),0.001);
 float3 h=normalize(v+l); float nh=saturate(dot(n,h)),vh=saturate(dot(v,h));
 float a=rough*rough,a2=a*a,d=nh*nh*(a2-1)+1;
 float D=a2/max(3.14159265*d*d,0.00001);
 float vis=0.5/max(nl*(nv*(1-a)+a)+nv*(nl*(1-a)+a),0.001);
 float3 F=f0+(1-f0)*pow(1-vh,5);
 return ((1-F)*base+3.14159265*D*vis*F)*nl;
}
float3 worldEnv(float3 r,float rough) {
 float3 w=mul(r,(float3x3)World);
 float3 d=float3(-(w.x*SkyYaw.x-w.z*SkyYaw.y),w.y,-(w.x*SkyYaw.y+w.z*SkyYaw.x));
 float m=max(abs(d.x),max(abs(d.y),abs(d.z)));
 float rho=max(length(ddx(d)/m),length(ddy(d)/m))*64;
 return worldLinear(texCUBEbias(WorldSky,float4(d,rough*7-log2(max(rho,0.00001)))).rgb);
}
// Analytic split-sum BRDF: rough surfaces must not become mirrors at grazing angles.
float3 worldEnvBRDF(float3 f0,float rough,float nv) {
 float4 r=rough*float4(-1,-0.0275,-0.572,0.022)+float4(1,0.0425,1.04,-0.04);
 float a=min(r.x*r.x,exp2(-9.28*nv))*r.x+r.y;
 float2 ab=float2(-1.04,1.04)*a+r.zw;
 return f0*ab.x+ab.y;
}
float4 WorldPS(WorldVarying i) : COLOR0 {
 clip(lerp(1,i.shadow1.w+0.05,ReflectionPass));
 float4 tex=tex2D(WorldAlbedo,i.uv);
 float3 base=worldLinear(tex.rgb),n=normalize(i.n);
 float rough=WorldSurfaceParams.x;
#ifdef WR_GLOSS_ALPHA
 rough=lerp(0.9,rough,saturate(tex.a));
#endif
#ifdef WR_NORMAL
 float4 pack=tex2D(WorldNormal,i.uv);
 float3 tangentNormal=normalize(pack.rgb*2-1);
 n=normalize(tangentNormal.x*normalize(i.t)+tangentNormal.y*normalize(i.b)+tangentNormal.z*n);
 rough=lerp(0.88,rough,saturate(pack.a));
#elif defined(WR_RELIEF)
 n=worldRelief(n,i.p,dot(tex.rgb,float3(0.2126,0.7152,0.0722)),WorldSurfaceParams.z);
#endif
 // Normal variance raises roughness, reducing sparkle on distant details.
 rough=clamp(sqrt(rough*rough+0.25*(dot(ddx(n),ddx(n))+dot(ddy(n),ddy(n)))),0.19,0.95);
 float3 v=normalize(invXfromEye.xyz-i.p);
 float3 l=normalize(invXformLightDir.xyz),l2=normalize(invXformLightDir2.xyz);
 float visibility=1;
#ifdef WR_LIGHTMAP_FF
 float3 lightmap=lerp(1,tex2D(WorldLightmap,i.lm).rgb,LightMapBound);
 visibility=saturate(max(lightmap.r,max(lightmap.g,lightmap.b)));
#elif defined(WR_LIGHTMAP)
 visibility=tex2D(WorldLightmap,i.lm).b;
#endif
 // Baked occlusion for indirect light; baked or dynamic shadow for the sun.
 float sunShadow=worldShadow(i.shadow0,i.shadow1.xyz);
 float direct=min(visibility,sunShadow);
 float3 f0=WorldSurfaceParams.y.xxx;
#ifdef WR_ENV
 f0=lerp(f0,0.16.xxx,saturate(reflectivity));
#endif
 float3 sun=worldLinear(2*LightMaterialColor.rgb);
 float3 fill=worldLinear(2*secondaryLightColor.rgb);
 float3 ambient=worldLinear(2*LightAmbient.rgb);
 // More directional ambient than the flat retail wash; native map colours remain.
 float hemi=0.48+0.52*saturate(mul(n,(float3x3)World).y*0.5+0.5);
 float3 lit=worldGGX(n,v,l,rough,f0,base)*sun*(direct*WorldHdrScale);
 lit+=base*(fill*saturate(dot(n,l2))*visibility+ambient*hemi*lerp(0.55,1,visibility));
 float nv=saturate(dot(n,v));
 float envStrength=saturate(dot(sun+fill+ambient,float3(0.2126,0.7152,0.0722)));
 lit+=worldEnv(reflect(-v,n),rough)*worldEnvBRDF(f0,rough,nv)*envStrength*WorldSurfaceParams.w*lerp(0.35,1,visibility);
 float alpha=tex.a*LightMaterialColor.a;
#ifdef WR_GLOSS_ALPHA
 alpha=LightMaterialColor.a;
#endif
#ifdef WR_ENV
 alpha=saturate(tex.a+LightAmbient.a);
#endif
 float nl0=saturate(dot(normalize(i.n),l)),nl20=saturate(dot(normalize(i.n),l2));
#ifdef WR_LIGHTMAP_FF
 float3 vertexLight=saturate(2*(LightMaterialColor.rgb*nl0+secondaryLightColor.rgb*nl20+LightAmbient.rgb));
 float3 retail=tex.rgb*saturate(lightmap*vertexLight+textureFactor.rgb);
 // The same native shading with this frame's sun shadow applied.
 float3 shadowedLight=saturate(2*(LightMaterialColor.rgb*nl0*sunShadow+secondaryLightColor.rgb*nl20+LightAmbient.rgb));
 float3 floorLight=tex.rgb*saturate(lightmap*shadowedLight+textureFactor.rgb);
#else
 float3 retail=tex.rgb*saturate(2*((LightMaterialColor.rgb*nl0+secondaryLightColor.rgb*nl20)*visibility+LightAmbient.rgb));
 float3 floorLight=tex.rgb*saturate(2*((LightMaterialColor.rgb*nl0*sunShadow+secondaryLightColor.rgb*nl20)*visibility+LightAmbient.rgb));
#endif
#ifdef WR_LIGHTMAP
 // A lightmapped surface is never darker than its native shading (with the
 // sun shadow). The near LODs carry lightmaps, the far LODs do not, so
 // without this floor a bright mesh went dark as the camera approached.
 lit=max(lit,worldLinear(floorLight));
#endif
 // The scene target holds display-encoded colour, as the native draws write
 // it, unclamped (FP16 keeps values above 1). The post pass decodes it once.
 float3 color=lerp(retail,pow(max(lit,0),1/2.2),WorldUpgradeEnabled);
 // The shadow pass writes depth only; alpha keeps the native alpha test.
 return lerp(float4(color,lerp(alpha,1,ReflectionPass)),float4(0,0,0,alpha),ShadowPass);
}
technique world_material_v2 {
 pass P0 {
  VertexShader=compile vs_2_0 WorldVS(); PixelShader=compile ps_2_a WorldPS();
  AlphaBlendEnable=<alphaBlendEnable>; SrcBlend=<srcBlend>; DestBlend=<destBlend>;
  CullMode=<cullMode>; AlphaRef=<alphaRef>; ZWriteEnable=<depthWrite>;
#ifdef ENABLE_WIREFRAME
  FillMode=WireFrame;
#endif
 }
}
