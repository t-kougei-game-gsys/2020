struct Output {
	float4 pos : POSITION;
	float4 sv_pos : SV_POSITION;
	float4 normal : NORMAL;
	float4 vnormal : NORMAL1;
	float2 uv : TEXCOORD;
	float3 ray : VECTOR;
	float4 t_pos : TPOS;
	uint instNo : SV_InstanceID;
};

cbuffer Material : register(b2) {
	// alpha in float4
	float4 diffuse;
	// specularity in float4
	float4 specular;
	float3 ambient;
}