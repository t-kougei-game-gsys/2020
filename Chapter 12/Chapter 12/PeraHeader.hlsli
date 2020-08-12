Texture2D<float4> tex : register (t0);
Texture2D<float4> effectTex : register (t1);
SamplerState smp : register (s0);

struct Output {
	float4 sv_pos : SV_POSITION;
	float2 uv : TEXCOORD;
};