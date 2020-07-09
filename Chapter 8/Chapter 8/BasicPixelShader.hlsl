#include "BasicShaderHeader.hlsli"

Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
Texture2D<float4> toon : register(t3);

SamplerState smp : register(s0);
SamplerState smpToon : register(s1);

float4 main (Output input) : SV_TARGET {
	// simulate light (direction)
	float3 light = normalize (float3 (1, -1, 1));
	float3 lightColor = float3(1, 1, 1);

	float diffuseB = dot (-light, input.normal);
	float4 toonDif = toon.Sample (smpToon, float2 (0, 1.0 - diffuseB));

	float3 refLight = normalize (reflect (light, input.normal.xyz));
	float specularB = pow (saturate (dot (refLight, -input.ray)), specular.a);
	
	//float brightness = dot (lightDir, input.normal.xyz);	// range [0, 1]
	//float brightness = dot (-light, input.normal);
	float2 normalUV = (input.normal.xy + float2(1, -1)) * float2(0.5, -0.5);
	float2 sphereMapUV = input.vnormal.xy;
	sphereMapUV = (sphereMapUV + float2(1, -1))* float2(0.5, -0.5);

	float4 texColor = tex.Sample (smp, input.uv);
	//return max (diffuse * diffuse * texColor + float4 (specularB * specular.rgb, 1), float4 (ambient * texColor, 1));

	return max(saturate(
				toonDif 
				* diffuse 
				* texColor
				* sph.Sample (smp, normalUV))
				+ saturate(spa.Sample (smp, normalUV) * texColor
				+ float4 (specularB * specular.rgb, 1))
				, float4 (ambient * texColor, 1));
	// return diffuse * tex.Sample (smp, input.uv);
}