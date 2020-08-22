#include "PMDShaderHeader.hlsli"

Texture2D<float4> tex : register(t0);
Texture2D<float4> sph : register(t1);
Texture2D<float4> spa : register(t2);
Texture2D<float4> toon : register(t3);

SamplerState smp : register(s0);
SamplerState smpToon : register(s1);

PS_Output main (Output input) {
	float3 light = normalize (float3 (1, -1, 1));
	float3 lightColor = float3(1, 1, 1);

	float diffuseB = dot (-light, input.normal);
	float4 toonDif = toon.Sample (smpToon, float2 (0, 1.0 - diffuseB));

	float3 refLight = normalize (reflect (light, input.normal.xyz));
	float specularB = pow (saturate (dot (refLight, -input.ray)), specular.a);

	float2 sphereMapUV = input.vnormal.xy;
	sphereMapUV = (sphereMapUV + float2(1, -1)) * float2(0.5, -0.5);

	float4 texCol = tex.Sample (smp, input.uv);
	float4 sphCol = sph.Sample (smp, sphereMapUV);
	float4 spaCol = spa.Sample (smp, sphereMapUV);
	float4 toonCol = toon.Sample (smpToon, float2(0, 1 - diffuseB));

	//float4 ret = saturate (
	//	toonDif *
	//	diffuse *
	//	texCol *
	//	sphCol) +
	//	saturate (spaCol * texCol + float4 (specularB * specular.rgb, 1)) +
	//	float4 (ambient * texCol * 0.5, 1);

	float4 ret = float4((spaCol + sphCol * texCol * toonCol * diffuse).rgb, diffuse.a)
		+ float4(specular.rgb * specularB, 1);

	PS_Output output;

	output.col = ret;
	output.normal.rgb = float3 ((input.normal.xyz + 1.0f) / 2.0f);
	output.normal.a = 1;
	
	float y = dot (float3 (0.299f, 0.587f, 0.114f), output.col);
	output.highLum = y > 0.995f ? output.col : 0.0;
	output.highLum.a = 1.0f;

	// Deferred Shading
	//output.col = float4 (spaCol + sphCol * texCol * diffuse);
	//output.normal.rgb = float3 ((input.normal.xyz + 1.0f) / 2.0f);
	//output.normal.a = 1;

	return output;
}