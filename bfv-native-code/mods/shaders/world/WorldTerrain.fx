// Terrain keeps the baked macro texture; detail gains actual directional relief.
float4x4 WorldViewProjection;
float4x4 World;
float4x4 ReflectionWVP;
float ReflectionPass=0;
float4 ReflectionPlane=float4(0,1,0,0);
float WorldUpgradeEnabled=1;
float2 FogData=float2(200,950);
float4 TerrainSun=float4(0.3,0.8,0.3,0);
float4x4 ShadowMatrix0, ShadowMatrix1;
float4 ShadowParams=float4(0,0.0002,0.08,0.000244);
float ShadowPass=0;
texture ShadowMap;
sampler TerrainShadow=sampler_state { Texture=(ShadowMap); MinFilter=LINEAR; MagFilter=LINEAR;
 MipFilter=NONE; AddressU=CLAMP; AddressV=CLAMP; };
texture DiffuseMap,DetailTexture;
sampler Macro=sampler_state { Texture=(DiffuseMap); MinFilter=ANISOTROPIC;
 MaxAnisotropy=8; MagFilter=LINEAR; MipFilter=LINEAR; AddressU=CLAMP; AddressV=CLAMP; };
sampler Detail=sampler_state { Texture=(DetailTexture); MinFilter=ANISOTROPIC;
 MaxAnisotropy=8; MagFilter=LINEAR; MipFilter=LINEAR; AddressU=WRAP; AddressV=WRAP; };
struct TerrainV { float4 pos:POSITION; float2 uv:TEXCOORD0;float2 detail:TEXCOORD1;
 float3 p:TEXCOORD2;float depth:TEXCOORD3;float planeHeight:TEXCOORD4;
 float3 shadow0:TEXCOORD5;float3 shadow1:TEXCOORD6;float fog:FOG; };
TerrainV terrainVS(float4 p:POSITION,float2 uv:TEXCOORD0,float2 detail:TEXCOORD1) {
 TerrainV o=(TerrainV)0; o.pos=lerp(mul(p,WorldViewProjection),mul(p,ReflectionWVP),ReflectionPass); o.p=mul(p,World).xyz;
 o.uv=uv;o.detail=detail;o.depth=o.pos.w;
 // Retail terrain is fixed-function with device fog: the same linear factor.
 o.fog=(FogData.y-o.pos.w)/(FogData.y-FogData.x);
 // Terrain is near horizontal: offset straight up against acne.
 float4 shadowP=float4(o.p+float3(0,ShadowParams.z,0),1);
 o.shadow0=mul(shadowP,ShadowMatrix0).xyz;o.shadow1=mul(shadowP,ShadowMatrix1).xyz;
 o.planeHeight=dot(float4(o.p,1),ReflectionPlane);return o;
}
float terrainShadow(float3 a,float3 b) {
 float2 e0=abs(a.xy-float2(0.25,0.5)),e1=abs(b.xy-float2(0.75,0.5));
 float in0=(e0.x<0.245&&e0.y<0.49&&a.z<1)?1:0;
 float in1=(e1.x<0.245&&e1.y<0.49&&b.z<1)?1:0;
 float3 s=lerp(b,a,in0);
 float o=ShadowParams.w*0.75;
 s.z-=ShadowParams.y*lerp(2.5,1,in0);
 float v=tex2Dproj(TerrainShadow,float4(s.xy+float2(-o,-o)*float2(1,2),s.z,1)).r
        +tex2Dproj(TerrainShadow,float4(s.xy+float2( o,-o)*float2(1,2),s.z,1)).r
        +tex2Dproj(TerrainShadow,float4(s.xy+float2(-o, o)*float2(1,2),s.z,1)).r
        +tex2Dproj(TerrainShadow,float4(s.xy+float2( o, o)*float2(1,2),s.z,1)).r;
 // Fade out over the outer part of the far cascade.
 float fade=saturate((0.49-max(e1.x*2,e1.y))*12);
 return lerp(1,v*0.25,ShadowParams.x*max(in0,in1*fade));
}
float4 terrainPS(TerrainV i) : COLOR0 {
 clip(lerp(1,i.planeHeight+0.05,ReflectionPass));
 float3 macro=tex2D(Macro,i.uv).rgb,detail=tex2D(Detail,i.detail).rgb;
 float h=dot(detail,float3(0.2126,0.7152,0.0722));
 float3 px=ddx(i.p),py=ddy(i.p),n=normalize(cross(py,px));
 n*=sign(n.y);
 float3 r1=cross(py,n),r2=cross(n,px);float det=dot(px,r1);
 float3 grad=(ddx(h)*r1+ddy(h)*r2)*(sign(det)/max(abs(det),0.00001));
 float fade=1-saturate((i.depth-40)/160);
 float3 relief=normalize(n-grad*(0.22*fade));
 float3 l=normalize(TerrainSun.xyz);
 // Relative relighting preserves baked broad shadows and slope lighting.
 float light=(0.38+0.9*saturate(dot(relief,l)))/(0.38+0.9*saturate(dot(n,l)));
 float cavity=lerp(0.78,1.05,saturate(h*1.6));
 float3 base=macro*lerp(float3(1,1,1),2*detail,0.85);
 // Dynamic sun shadow on top of the baked macro lighting: darken only
 // the part of the light that faces the sun.
 float nl=saturate(dot(n,l));
 float shadow=terrainShadow(i.shadow0,i.shadow1);
 float shade=lerp(1-0.62*nl,1,shadow);
 float3 color=lerp(2*macro*detail,base*clamp(light,0.52,1.65)*lerp(1,cavity,fade)*shade,WorldUpgradeEnabled);
 return lerp(float4(color,1),float4(0,0,0,1),ShadowPass);
}
technique world_terrain_v2 {
 pass P0 {
  VertexShader=compile vs_2_0 terrainVS();PixelShader=compile ps_2_a terrainPS();
 AlphaTestEnable=false;
#ifdef ENABLE_WIREFRAME
 FillMode=WireFrame;
#endif
 }
}
