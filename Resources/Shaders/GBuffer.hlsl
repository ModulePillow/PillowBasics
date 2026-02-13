#include "Common.hlsl"

VertexOut VS(VertexIn vin)
{
  VertexOut vout;
  // Transform to homogeneous clip space.
  float4 posW = mul(float4(vin.pL, 1.0f), mtxW);
  vout.pos = mul(posW, mtxVP);
  vout.normal = normalize(vin.nL);
  vout.tangent = normalize(vin.tL);
  vout.uv = vin.uv;
  return vout;
}

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

struct GBuffer
{
  float4 backBuffer : SV_Target0; // rgb(emissive+GI) + a(?)
  float4 gBuffer0 : SV_Target1;   // rgb(albedo)      + a(smoothness)
  float4 gBuffer1 : SV_Target2;   // rgb(normal)      + a(metallic)
};

Texture2D tex0 : register(t0); // color, smoothness
Texture2D tex1 : register(t1); // normal, metallic 

[earlydepthstencil]
GBuffer PS(VertexOut pin)
{
  GBuffer output;
  float4 albedo_s = tex0.Sample(Ani, pin.uv);
  float4 normal_m = tex1.Sample(Ani, pin.uv);
  //normal_m.xyz = GetWorldNormal(normal_m.xyz, 0, 0);

  // spec/diff/reflectvitity are all generated from metallic.
  //
  //unity_ColorSpaceDielectricSpec half4(0.04, 0.04, 0.04, 1.0 - 0.04)
  //standard dielectric reflectivity coef at incident angle (= 4%)
  //float3 specColor = lerp(float3(0.04, 0.04, 0.04), albedo_s.rgb, normal_m.a);
  //float oneMinusReflectivity = 0.96 - normal_m.a * 0.96;
  //float3 diffColor = albedo_s.rgb * oneMinusReflectivity;

  float3 giColor = 0;
  float3 emissiveColor = 0;
  float3 extraColor = giColor + emissiveColor;

  //output.backBuffer = float4(extraColor, 0);
  //output.backBuffer = float4(EncodeSRGB(albedo_s).rgb, 0);
  output.backBuffer = EncodeSRGB(albedo_s);//EncodeSRGB(normal_m);//float4(pin.normal, 0);
  output.gBuffer0 = albedo_s;
  output.gBuffer1 = normal_m;

  return output;
}