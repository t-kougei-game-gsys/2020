#include "BasicShaderHeader.hlsli"

// Texture2D<float4> tex : register(t0);
SamplerState smp : register(s0);

cbuffer cbuff1 : register (b1) {
	float lightAngle;
};

float4 BasicPS (Output input) : SV_TARGET {	
	printf ("how to printf???? \n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n\n");

	// simulate light (direction)
	
	float radius = 1.0f;
	float x = cos (lightAngle) / radius;
	float y = sin (lightAngle) / radius;

	float3 lightPos = float3 (x, y, 1);
	float3 lightDir = normalize (float3(0.0f, 0.0f, 0.0f) - lightPos);	// Light to (0, 0, 0)
	// ½÷«×
	//float brightness = dot (-light, input.normal);
	float brightness = dot (lightDir, input.normal);	// range [0, 1]
	//brightness = abs (brightness);
	//brightness = -brightness;
	//float3 color = float3(5, 5, 5);
	//return float4 (color.xyz * brightness, 1);
	return float4 (brightness, brightness, brightness, 1);
	//return float4(input.normal.xyz, 1);
}