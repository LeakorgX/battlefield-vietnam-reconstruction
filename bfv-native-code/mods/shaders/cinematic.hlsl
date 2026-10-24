// Authored for BF Vietnam's LDR D3D9 renderer, ps_2_0. Input is display RGB.
// This is a colour finish, not an HDR/PBR conversion of retail materials.
sampler2D Scene : register(s0);
float4 main(float2 uv : TEXCOORD0) : COLOR0
{
    float4 src = tex2D(Scene, uv);
    float3 c = src.rgb;
    float y = dot(c, float3(0.2126, 0.7152, 0.0722));
    // Muted foliage and paint; preserve separation in metal highlights.
    c = lerp(y.xxx, c, 0.82);
    // Gentle S curve: no crushed blacks, white remains white.
    c = c + 0.22 * c * (1.0 - c) * (2.0 * c - 1.0);
    float shadow = (1.0 - y) * (1.0 - y);
    float highlight = y * y;
    c *= 1.0 + shadow * float3(-0.055, 0.008, 0.060)
             + highlight * float3(0.025, 0.010, -0.025);
    // Roll the brightest values towards neutral rather than clipping colour.
    c = lerp(c, dot(c, float3(0.2126, 0.7152, 0.0722)).xxx,
             0.12 * highlight);
    float2 edge = uv * 2.0 - 1.0;
    c *= 1.0 - 0.055 * saturate(dot(edge, edge) * 0.5);
    return float4(saturate(c), src.a);
}
