#include "BasicShaderHeader.hlsli"

Texture2D<float4> tex:register(t0);

SamplerState smp:register(s0);

float4 main (Output input) : SV_TARGET {
	// simulate light (direction)
	float3 lightPos = float3 (-1, 1, -1);
	float3 light = normalize (float3(1, -1, 1));

	// isn't targetPos - lightPos
	// because calculate theta between the lightPos and the targetPos
	float3 lightDir = normalize (lightPos - float3(0.0f, 0.0f, 0.0f));	// Light to (0, 0, 0)
	//float brightness = dot (lightDir, input.normal.xyz);	// range [0, 1]
	float brightness = dot (-light, input.normal);
	return float4 (brightness, brightness, brightness, 1) * diffuse * tex.Sample (smp, input.uv);
	// return diffuse * tex.Sample (smp, input.uv);
}