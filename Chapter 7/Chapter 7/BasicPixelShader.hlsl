#include "BasicShaderHeader.hlsli"

// Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

float4 BasicPS (Output input) : SV_TARGET {
	// simulate light (direction)
	float3 light = normalize (float3 (1, -1, 1));
	// ½÷«×
	float brightness = dot (-light, input.normal);

	return float4 (brightness, brightness, brightness, 1);
	//return float4(input.normal.xyz, 1);
}