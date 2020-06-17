#include "BasicShaderHeader.hlsli"

//
// 7. Pixel Shader§Œ‘O∂®
//

Texture2D<float4> tex : register(t0);		// 0∑¨•π•Ì•√•»§À‘O∂®§µ§Ï§ø•∆•Ø•π•¡•„
SamplerState smp : register(s0);			// 0∑¨•π•Ì•√•»§À‘O∂®§µ§Ï§ø•µ•Û•◊•È

// -------------------------

float4 BasicPS (Output input) : SV_TARGET {
	return float4(tex.Sample (smp, input.uv));
}