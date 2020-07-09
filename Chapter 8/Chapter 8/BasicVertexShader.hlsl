#include "BasicShaderHeader.hlsli"

cbuffer SceneData : register (b0) {
	matrix world;
	matrix view;
	matrix proj;
	float3 eye;
};

Output main (float4 pos : POSITION, float4 normal : NORMAL, float2 uv : TEXCOORD, min16uint2 boneno : BONE_NO, min16uint weight : WEIGHT) {
	Output output;
	pos = mul (world, pos);
	output.sv_pos = mul (mul (proj, view), pos);
	output.pos = mul (view, pos);
	normal.w = 0;
	output.normal = mul (world, normal);
	output.vnormal = mul (view, output.normal);
	output.uv = uv;
	output.ray = normalize (pos.xyz - eye);	// vector of eye to po.
	return output;
}