#include "Common.hlsl"

float4 Screen2Perspective(float2 pos)
{
  pos.y = viewportSize.y - pos.y;
  pos = pos * viewportSize.zw * 2 - 1;
  return float4(pos, 0, 1);
}

UIVertexOut VS(UIVertexIn vin)
{
  UIVertexOut vout;
  vout.pos = Screen2Perspective(vin.pos);
  vout.uv = vin.uv;
  return vout;
}

Texture2D bitmap : register(t0);

float4 PS(UIPixelIn pin) : SV_Target
{
  clip(pin.id % 4 <= 1 ? 1 : -1);
  float alpha =  bitmap.Sample(Tri, pin.uv).r;
  clip(alpha - 0.01);
  float4 c = float4(color.rgb, alpha);
  c = EncodeSRGB(c);
  return c;
}