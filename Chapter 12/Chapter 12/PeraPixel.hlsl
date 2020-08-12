#include "PeraHeader.hlsli"

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

	int value = 2;

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
	ret += tex.Sample (smp, input.uv + float2 (-value * dx, -value * dy)) * 0;	// 左上
	ret += tex.Sample (smp, input.uv + float2 (0, -value * dy)) * -1;			// 上
	ret += tex.Sample (smp, input.uv + float2 (value * dx, -value * dy)) * 0;	// 右上
	ret += tex.Sample (smp, input.uv + float2 (-value * dx, 0)) * -1;			// 左
	ret += tex.Sample (smp, input.uv) * 4;										// 自分
	ret += tex.Sample (smp, input.uv + float2 (value * dx, 0)) * -1;			// 右
	ret += tex.Sample (smp, input.uv + float2 (-value * dx, value * dy)) * 0;	// 左下
	ret += tex.Sample (smp, input.uv + float2 (0, value * dy)) * -1;			// 下
	ret += tex.Sample (smp, input.uv + float2 (value * dx, value * dy)) * 0;	// 右下

	// 反転
	float Y = dot (ret.rgb, float3 (0.299, 0.587, 0.114));

	Y = pow (1.0f - Y, 10.0f);
	Y = step (0.2, Y);

	return float4 (Y, Y, Y, col.a);
}
