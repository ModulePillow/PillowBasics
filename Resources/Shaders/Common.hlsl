/*
  This is the common hlsl header.
  
  You can create your own headers,
  but you could only reference them from relative paths.

  Notice:
  Shader Model = 5.0
  mtx = matrix
*/

struct VertexIn
{
  float3 pL : POSITION;
  float3 nL : NORMAL;
  float3 tL : TANGENT;
  float2 uv : TEXCOORD;

  uint idI : SV_InstanceID;
  uint idV : SV_VertexID;
};

struct VertexOut
{
  float4 pos : SV_POSITION;
  float3 normal: TEXCOORD0;
  float3 tangent: TEXCOORD1;
  float2 uv : TEXCOORD2;
};

struct UIVertexIn
{
  float2 pos : POSITION;
  float2 uv : TEXCOORD;
};

struct UIVertexOut
{
  float4 pos : SV_POSITION;
  float2 uv : TEXCOORD;
};

struct UIPixelIn
{
  float4 pos : SV_POSITION;
  float2 uv : TEXCOORD;
  uint id : SV_PrimitiveID;
};

/*
  These are static samplers.
  Some materials need the ability to choose whether the clamp or wrap address mode.
  Thus, use "Tri" or "Ani" instead, and define "VARIATION_CLAMP_WRAP" in your shader,
  then the engine will create two materials for it.
  (As for programmers, a material means a pipeline state in DX12)
*/
SamplerState Point : register(s0);
SamplerState TriClamp : register(s1);
SamplerState TriWrap : register(s2);
SamplerState AniClamp : register(s3);
SamplerState AniWrap : register(s4);
SamplerComparisonState Cmp : register(s5);
#ifdef CLAMP_ADDRESS
#define Tri TriClamp
#define Ani AniClamp
#else
#define Tri TriWrap
#define Ani AniWrap
#endif

/*
  These are resources.

  About Textures:
  You SHOULD define your textures in your shader, following these RULES:
  1 A texture cannot be used both in the VS and PS.
  2 The texture register numbers used in the VS or PS must be successive and start from 0.

  Tips:
  1 Textures in an XML material file are bound based on their written order, not names.
  2 If you ACTUALLY want to use a texture in both the VS and PS,
    you can define two in your shader and bind two in your material.
    only one texture will be loaded into memory for this.
  3 These rules seem complicated but are necessary to reduce the overheads
    of the root signature (one kind of DX12 object).
*/
cbuffer SuperObjectConsts : register(b0)
{
  float4x4 mtxW;
  float4 color;
};
cbuffer SuperPassConsts : register(b1)
{
  float4x4 mtxVP;
  float4 viewportSize; // w, h, 1/w, 1/h
};

/*
  Functions
*/
float4 EncodeSRGB(float4 color)
{
  float4 result;
  result.rgb =  1.055*pow(color.rgb, 1.0/2.4) - 0.055;
  result.a = color.a;
  return result;
}

float3 GetWorldNormal(float3 value, float3 nW, float3 tW)
{
  nW = normalize(nW);
  tW = normalize(tW - nW * dot(nW, tW));
  float3x3 T2W = float3x3(tW, nW, cross(tW, nW));
  value = 2 * value - 1;
  value = normalize(mul(value, T2W));
  return value;
}