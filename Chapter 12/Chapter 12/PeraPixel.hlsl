#include "PeraHeader.hlsli"

cbuffer PostEffect : register (b0) {
	float4 bkweights[2];
}

float4 VerticalBokehPS (Output input) : SV_TARGET {
	float w, h, level;
	tex.GetDimensions (0, w, h, level);

	float dy = 1.0f / h;
	float4 ret = float4 (0, 0, 0, 0);
	float4 col = tex.Sample (smp, input.uv);

	float2 nmTex = effectTex.Sample (smp, input.uv).xy;
	nmTex = nmTex * 2.0f - 1.0f;

	return tex.Sample (smp, input.uv + nmTex * 0.1f);

	ret += bkweights[0] * col;

	for (int i = 1; i < 8; ++i) {
		ret += bkweights[i >> 2][i % 4] * tex.Sample (smp, input.uv + float2 (0, dy * i));
		ret += bkweights[i >> 2][i % 4] * tex.Sample (smp, input.uv + float2 (0, -dy * i));
	}

	return float4 (ret.rgb, col.a);
}

float4 main (Output input) : SV_TARGET {
	float4 col = tex.Sample (smp, input.uv);
	//return float4 (input.uv, 1, 1);
	//return tex.Sample (smp, input.uv);

	//
	// モノクロ
	//
	//float Y = dot(col.rgb, float3(0.299, 0.587, 0.114));
	//return float4 (Y, Y, Y, 1);

	//
	// 色の反転
	//
	//float3 c = float3 (1.0, 1.0, 1.0) - col.rgb;
	//return float4 (c, col.a);

	//
	// 色の階調を落とす
	//
	//return float4 (col.rgb - fmod(col.rgb, 0.value5f), col.a);

	float w, h, levels;
	tex.GetDimensions (0, w, h, levels);

	float dx = 1.0f / w;
	float dy = 1.0f / h;
	float4 ret = float4 (0, 0, 0, 0);

	// int value = 2;

	//
	// ぼかし処理
	//
	//ret += tex.Sample (smp, input.uv + float2(-value * dx, -value * dy));	// 左上
	//ret += tex.Sample (smp, input.uv + float2(0, -value * dy));				// 上
	//ret += tex.Sample (smp, input.uv + float2(value * dx, -value * dy));	// 右上
	//ret += tex.Sample (smp, input.uv + float2(-value * dx, 0));				// 左
	//ret += tex.Sample (smp, input.uv);										// 自分
	//ret += tex.Sample (smp, input.uv + float2(value * dx, 0));				// 右
	//ret += tex.Sample (smp, input.uv + float2(-value * dx, value * dy));	// 左下
	//ret += tex.Sample (smp, input.uv + float2(0, value * dy));				// 下
	//ret += tex.Sample (smp, input.uv + float2(value * dx, value * dy));		// 右下

	//return ret / 9.0f;

	//
	// エンボス加工
	// 
	//ret += tex.Sample (smp, input.uv + float2 (-value * dx, -value * dy)) * 2;	// 左上
	//ret += tex.Sample (smp, input.uv + float2 (0, -value * dy));				// 上
	//ret += tex.Sample (smp, input.uv + float2 (value * dx, -value * dy)) * 0;	// 右上
	//ret += tex.Sample (smp, input.uv + float2 (-value * dx, 0));				// 左
	//ret += tex.Sample (smp, input.uv);											// 自分
	//ret += tex.Sample (smp, input.uv + float2 (value * dx, 0)) * -1;			// 右
	//ret += tex.Sample (smp, input.uv + float2 (-value * dx, value * dy)) * 0;	// 左下
	//ret += tex.Sample (smp, input.uv + float2 (0, value * dy)) * -1;			// 下
	//ret += tex.Sample (smp, input.uv + float2 (value * dx, value * dy)) * -2;	// 右下

	//float Y = dot (ret.rgb, float3(0.299, 0.587, 0.114));
	//return float4 (Y, Y, Y, 1);

	// return ret;

	//
	// シャープネス（エッジの強調）
	//
	//ret += tex.Sample (smp, input.uv + float2 (-value * dx, -value * dy)) * 0;	// 左上
	//ret += tex.Sample (smp, input.uv + float2 (0, -value * dy)) * -1;			// 上
	//ret += tex.Sample (smp, input.uv + float2 (value * dx, -value * dy)) * 0;	// 右上
	//ret += tex.Sample (smp, input.uv + float2 (-value * dx, 0)) * -1;			// 左
	//ret += tex.Sample (smp, input.uv) * 5;										// 自分
	//ret += tex.Sample (smp, input.uv + float2 (value * dx, 0)) * -1;			// 右
	//ret += tex.Sample (smp, input.uv + float2 (-value * dx, value * dy)) * 0;	// 左下
	//ret += tex.Sample (smp, input.uv + float2 (0, value * dy)) * -1;			// 下
	//ret += tex.Sample (smp, input.uv + float2 (value * dx, value * dy)) * 0;	// 右下

	//return ret;

	//
	// 簡単な輪郭線
	//
	//ret += tex.Sample (smp, input.uv + float2 (-value * dx, -value * dy)) * 0;	// 左上
	//ret += tex.Sample (smp, input.uv + float2 (0, -value * dy)) * -1;			// 上
	//ret += tex.Sample (smp, input.uv + float2 (value * dx, -value * dy)) * 0;	// 右上
	//ret += tex.Sample (smp, input.uv + float2 (-value * dx, 0)) * -1;			// 左
	//ret += tex.Sample (smp, input.uv) * 4;										// 自分
	//ret += tex.Sample (smp, input.uv + float2 (value * dx, 0)) * -1;			// 右
	//ret += tex.Sample (smp, input.uv + float2 (-value * dx, value * dy)) * 0;	// 左下
	//ret += tex.Sample (smp, input.uv + float2 (0, value * dy)) * -1;			// 下
	//ret += tex.Sample (smp, input.uv + float2 (value * dx, value * dy)) * 0;	// 右下

	//// 反転
	//float Y = dot (ret.rgb, float3 (0.299, 0.587, 0.114));

	//Y = pow (1.0f - Y, 10.0f);
	//Y = step (0.2, Y);

	//return float4 (Y, Y, Y, col.a);

	//
	// 簡単な正規分布
	//
	
	// 最上段
	//ret += tex.Sample (smp, input.uv + float2 (-2 * dx, value * dy)) * 1 / 256;
	//ret += tex.Sample (smp, input.uv + float2 (-1 * dx, value * dy)) * 4 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 0 * dx, value * dy)) * 6 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 1 * dx, value * dy)) * 4 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 2 * dx, value * dy)) * 1 / 256;

	//// 一つ上段
	//ret += tex.Sample (smp, input.uv + float2 (-2 * dx, 1 * dy)) * 4 / 256;
	//ret += tex.Sample (smp, input.uv + float2 (-1 * dx, 1 * dy)) * 16 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 0 * dx, 1 * dy)) * 24 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 1 * dx, 1 * dy)) * 16 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 2 * dx, 1 * dy)) * 4 / 256;

	//// 中段
	//ret += tex.Sample (smp, input.uv + float2 (-2 * dx, 0 * dy)) * 6 / 256;
	//ret += tex.Sample (smp, input.uv + float2 (-1 * dx, 0 * dy)) * 24 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 0 * dx, 0 * dy)) * 36 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 1 * dx, 0 * dy)) * 24 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 2 * dx, 0 * dy)) * 6 / 256;

	//// 一つ下段
	//ret += tex.Sample (smp, input.uv + float2 (-2 * dx, -1 * dy)) * 4 / 256;
	//ret += tex.Sample (smp, input.uv + float2 (-1 * dx, -1 * dy)) * 16 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 0 * dx, -1 * dy)) * 24 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 1 * dx, -1 * dy)) * 16 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 2 * dx, -1 * dy)) * 4 / 256;

	//// 最下段
	//ret += tex.Sample (smp, input.uv + float2 (-2 * dx, -value * dy)) * 1 / 256;
	//ret += tex.Sample (smp, input.uv + float2 (-1 * dx, -value * dy)) * 4 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 0 * dx, -value * dy)) * 6 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 1 * dx, -value * dy)) * 4 / 256;
	//ret += tex.Sample (smp, input.uv + float2 ( 2 * dx, -value * dy)) * 1 / 256;

	//return ret;

	//
	// 本格的な方法
	//

	ret += bkweights[0] * col;

	for (int i = 1; i < 8; ++i) {
		ret += bkweights[i >> 2][i % 4] * tex.Sample (smp, input.uv + float2( i * dx, 0));
		ret += bkweights[i >> 2][i % 4] * tex.Sample (smp, input.uv + float2(-i * dx, 0));
	}

	return float4 (ret.rgb, col.a);
}
