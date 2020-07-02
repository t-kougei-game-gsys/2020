#include "BasicShaderHeader.hlsli"

float4 main (Output input) : SV_TARGET {
	// simulate light (direction)
	float3 lightPos = float3 (0, 0, -5);

	// isn't targetPos - lightPos
	// because calculate theta between the lightPos and the targetPos
	float3 lightDir = normalize (lightPos - float3(0.0f, 0.0f, 0.0f));	// Light to (0, 0, 0)
	float brightness = dot (lightDir, input.normal.xyz);	// range [0, 1]
	return float4 (brightness, brightness, brightness, 1);
}