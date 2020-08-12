#include "PeraHeader.hlsli"

Output main (float4 pos : POSITION, float2 uv : TEXCOORD) {
	Output output;
	output.sv_pos = pos;
	output.uv = uv;
	return output;
}