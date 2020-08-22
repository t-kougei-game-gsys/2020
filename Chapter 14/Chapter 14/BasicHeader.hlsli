Texture2D<float4> tex : register (t0);
Texture2D<float4> texNormal : register (t1);
Texture2D<float4> texDepth : register (t2);
Texture2D<float4> texHighLum : register (t3);
Texture2D<float4> texShrink : register (t4);

SamplerState smp : register (s0);

struct VS_Output {
	float4 sv_pos : SV_POSITION;
	float2 uv : TEXCOORD;
};