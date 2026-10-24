// Two normal scales, angle-dependent Fresnel and a real directional sun lobe.
float frameTime,opacity=0.3,reflectivity=0.5,waterScale=10;
float WorldUpgradeEnabled=1;
int waterFade=300;
float3 UVWSpeed=float3(0,0.07,0);
float4 invXfromEye,LightMaterialColor;
float4x4 WorldViewProjection;
float2 FogData;
float ReflectionReady=0;
texture SceneReflection;
sampler Reflection=sampler_state {Texture=(SceneReflection);MinFilter=LINEAR;MagFilter=LINEAR;MipFilter=NONE;AddressU=CLAMP;AddressV=CLAMP;};
float4 TerrainSun=float4(0.3,0.8,0.3,0);
// The HD sky's turn (world_render.c sky_yaw): cos, sin.
float2 SkyYaw=float2(1,0);
texture skyCube,NormalMap;
sampler Sky=sampler_state {Texture=(skyCube);MinFilter=LINEAR;MagFilter=LINEAR;MipFilter=LINEAR;};
sampler Waves=sampler_state {Texture=(NormalMap);MinFilter=ANISOTROPIC;MaxAnisotropy=8;
 MagFilter=LINEAR;MipFilter=LINEAR;AddressU=WRAP;AddressV=WRAP;};
struct WaterV {float4 pos:POSITION;float3 p:TEXCOORD0;float4 color:COLOR0;float4 screen:TEXCOORD1;float fog:FOG;};
WaterV waterVS(float4 p:POSITION,float4 c:COLOR0) {
 WaterV o=(WaterV)0;o.pos=mul(p,WorldViewProjection);o.p=p.xyz;o.color=c;
 o.screen=o.pos;
 // The retail water fog factor (RaShaderWater.fx calcFog).
 o.fog=(FogData.y-o.pos.w)/(FogData.y-FogData.x);return o;
}
float4 waterPS(WaterV i) : COLOR0 {
 float2 uv=i.p.xz/max(waterScale,0.1);
 float2 a=tex2D(Waves,uv+UVWSpeed.xy*frameTime).rg*2-1;
 float2 b=tex2D(Waves,uv*2.73-UVWSpeed.yx*frameTime*0.63).rg*2-1;
 float2 slope=(a+b*0.45)*0.24;
 float3 n=normalize(float3(slope.x,1,slope.y)),v=normalize(invXfromEye.xyz-i.p);
 float nv=saturate(dot(n,v));float fresnel=0.02+0.98*pow(1-nv,5);
 float3 rw=reflect(-v,n);
 // Turned back by the sky's yaw, then the cube's axes (-x, y, -z).
 float3 r=float3(-(rw.x*SkyYaw.x-rw.z*SkyYaw.y),rw.y,-(rw.x*SkyYaw.y+rw.z*SkyYaw.x));
 float3 sky=texCUBE(Sky,r).rgb;
 float2 screenUV=i.screen.xy/i.screen.w*float2(0.5,-0.5)+0.5;
 float4 scene=tex2D(Reflection,screenUV+slope*0.018);
 sky=lerp(sky,scene.rgb,saturate(scene.a*ReflectionReady));
 float3 l=normalize(TerrainSun.xyz),h=normalize(l+v);
 float nh=saturate(dot(n,h)),nl=saturate(dot(n,l));
 float a2=0.0016,d=nh*nh*(a2-1)+1;
 float sparkle=min(3,0.0008/max(d*d,0.00001))*nl;
 float3 water=i.color.rgb*float3(0.58,0.78,0.87);
 float3 color=lerp(water,sky,saturate(0.18+fresnel*0.82))*lerp(0.8,1.0,saturate(reflectivity));
 color+=pow(max(2*LightMaterialColor.rgb,0),2.2)*sparkle;
 // Vertex alpha is shoreline coverage. View distance is not water depth.
 float shore=saturate(i.color.a);
 float3 retail=lerp(i.color.rgb,sky,reflectivity);
 float4 result=lerp(float4(retail,saturate(opacity*shore)),float4(color,saturate((0.72+fresnel*0.28)*shore)),WorldUpgradeEnabled);
 return result;
}
technique world_water_v2 {
 pass P0 {VertexShader=compile vs_2_0 waterVS();PixelShader=compile ps_2_a waterPS();
 CullMode=NONE;AlphaBlendEnable=true;AlphaTestEnable=false;SrcBlend=SRCALPHA;DestBlend=INVSRCALPHA;}
}
