#include "BasicHeader.hlsli"

float4 main (VS_Output input) : SV_TARGET {
	if (input.uv.x < 0.2 && input.uv.y < 0.2) {
		// normal
		return texNormal.Sample (smp, (input.uv - float2 (0, 0.4)) * 5);
	}

	return tex.Sample (smp, input.uv);
}