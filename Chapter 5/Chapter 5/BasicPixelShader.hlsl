#include "BasicShaderHeader.hlsli"

Texture2D<float4> tex : register(t0);		//0·¬¥¹¥í¥Ã¥È¤ËÔO¶¨¤µ¤ì¤¿¥Æ¥¯¥¹¥Á¥ã
SamplerState smp : register(s0);			//0·¬¥¹¥í¥Ã¥È¤ËÔO¶¨¤µ¤ì¤¿¥µ¥ó¥×¥é

float4 main (Output input) : SV_TARGET {
	return float4(tex.Sample (smp, input.uv));
}