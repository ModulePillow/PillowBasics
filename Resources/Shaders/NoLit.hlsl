#include "Common.hlsl"

VertexOut VS(VertexIn vin)
{
  VertexOut vout;
  // Transform to homogeneous clip space.
  float4 posW = mul(float4(vin.pL, 1.0f), mtxW);
  vout.pos = mul(posW, mtxVP);
  vout.uv = vin.uv;
  return vout;
}

Texture2D albedo : register(t0);

float4 PS(VertexOut pin) : SV_Target
{
  float4 c = albedo.Sample(Ani, pin.uv);
  c = EncodeSRGB(c);
  return c;
}