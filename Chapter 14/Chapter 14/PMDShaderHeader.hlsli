struct Output {
	float4 pos : POSITION;
	float4 sv_pos : SV_POSITION;
	float4 normal : NORMAL;
	float4 vnormal : NORMAL1;
	float2 uv : TEXCOORD;
	float3 ray : VECTOR;
};

struct PS_Output {
	float4 col : SV_TARGET0;		// color
	float4 normal : SV_TARGET1;		// normal
	float4 highLum : SV_TARGET2;	// High Luminance
};

cbuffer Material : register(b2) {
	// alpha in float4
	float4 diffuse;
	// specularity in float4
	float4 specular;
	float3 ambient;
}