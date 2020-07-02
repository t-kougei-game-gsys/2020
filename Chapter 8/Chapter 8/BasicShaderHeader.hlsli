struct Output {
	float4 pos : POSITION;
	float4 sv_pos : SV_POSITION;
	float4 normal : NORMAL;
	float2 uv : TEXCOORD;
};

cbuffer Material : register(b1) {
	// alpha in float4
	float4 diffuse;
	// specularity in float4
	float4 specular;
	float3 ambient;
}