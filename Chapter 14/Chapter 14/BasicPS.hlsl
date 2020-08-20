#include "BasicHeader.hlsli"

float4 main (VS_Output input) : SV_TARGET {
	if (input.uv.x < 0.2 && input.uv.y < 0.2) {
		// depth
		float depth = texDepth.Sample (smp, input.uv * 5);
		depth = 1.0f - pow (depth, 100);
		return float4 (depth, depth, depth, 1);
	} else if (input.uv.x < 0.2 && input.uv.y < 0.4) {
		// normal
		return texNormal.Sample (smp, (input.uv - float2 (0, 0.2)) * 5);
	}

	// Deferred Shading
	//float4 normal = texNormal.Sample (smp, input.uv);
	//normal = normal * 2.0f - 1.0f;
	//float3 light = normalize (float3 (1.0f, -1.0f, 1.0f));
	//const float ambient = 0.25f;
	//float diffB = max (saturate (dot (normal.xyz, -light)), ambient);
	//return tex.Sample (smp, input.uv) * float4 (diffB, diffB, diffB, 1);
	// Deferred Shading

	return tex.Sample (smp, input.uv);
}