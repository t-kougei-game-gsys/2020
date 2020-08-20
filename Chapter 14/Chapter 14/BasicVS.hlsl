#include "BasicHeader.hlsli"

VS_Output main(float4 pos : POSITION, float2 uv : TEXCOORD) {
	VS_Output output;
	output.sv_pos = pos;
	output.uv = uv;
	return output;
}