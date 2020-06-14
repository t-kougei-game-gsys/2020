#include "BasicShaderHeader.hlsli"

Texture2D<float4> tex : register(t0);		//0桑スロットに�O協されたテクスチャ
SamplerState smp : register(s0);			//0桑スロットに�O協されたサンプラ

float4 main (Output input) : SV_TARGET {
	return float4(tex.Sample (smp, input.uv));
}