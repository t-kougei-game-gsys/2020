#include "DrawDSVHeader.hlsli"

float4 main(VS_Output input) : SV_TARGET {
	float dep = pow (tex.Sample (smp, input.uv), 20);
	return float4 (dep, dep, dep, 1);
	//return tex.Sample (smp, input.uv);
}