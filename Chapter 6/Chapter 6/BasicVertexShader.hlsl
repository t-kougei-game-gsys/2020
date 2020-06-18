#include "BasicShaderHeader.hlsli"

//
// 6. Shader§ÀBuffer§ŒèÍ”√
//

cbuffer cbuff0 : register(b0) {
	matrix mat;
};

// ---------------------------

Output BasicVS (float4 pos : POSITION, float2 uv : TEXCOORD) {
	Output output;
	output.sv_pos = mul (mat, pos);
	output.uv = uv;
	return output;
}