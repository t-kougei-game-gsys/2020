#include "BasicShaderHeader.hlsli"

cbuffer SceneData : register (b0) {
	matrix view;
	matrix proj;
	matrix shadow;
	matrix lightCamera;
	float3 eye;
};

cbuffer Transform : register(b1) {
	matrix world;
	matrix bones[256];
}

Output main (float4 pos : POSITION, float4 normal : NORMAL, float2 uv : TEXCOORD, min16uint2 boneno : BONE_NO, min16uint weight : WEIGHT, uint instNo : SV_InstanceID) {
	Output output;
	
	// about bone.
	float w = (float)weight / 100.0f;
	matrix bm = bones[boneno[0]] * w + bones[boneno[1]] * (1.0 - w);
	output.pos = mul(world, mul (bm, pos));
	// chapter 13
	if (instNo == 1) {
		output.pos = mul (shadow, output.pos);
	}

	output.sv_pos = mul (mul (proj, view), output.pos);
	// output.sv_pos = mul (lightCamera, output.pos);
	output.t_pos = mul (lightCamera, output.pos);
	// chapter 13

	normal.w = 0;
	output.normal = mul (world, normal);
	output.vnormal = mul (view, output.normal);
	output.uv = uv;
	output.ray = normalize (pos.xyz - mul (view, eye));
	output.instNo = instNo;
	return output;
}

float4 ShadowVS (float4 pos : POSITION, float4 normal : NORMAL, float2 uv : TEXCOORD, min16uint2 boneno : BONE_NO, min16uint weight : WEIGHT) : SV_POSITION {
	float fWeight = float (weight) / 100.0f;
	matrix conBone = bones[boneno.x] * fWeight + bones[boneno.y] * (1.0f - fWeight);
	pos = mul (world, mul (conBone, pos));
	return  mul (lightCamera, pos);
}