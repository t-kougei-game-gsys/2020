#include "BasicHeader.hlsli"

float4 Get5x5GaussianBlur (Texture2D<float4> t, SamplerState smp, float2 uv, float dx, float dy, float4 rect) {
	float4 ret = t.Sample (smp, uv);

	float l1 = -dx, l2 = -2 * dx;
	float r1 = dx, r2 = 2 * dx;
	float u1 = -dy, u2 = -2 * dy;
	float d1 = dy, d2 = 2 * dy;
	l1 = max (uv.x + l1, rect.x) - uv.x;
	l2 = max (uv.x + l2, rect.x) - uv.x;
	r1 = min (uv.x + r1, rect.z - dx) - uv.x;
	r2 = min (uv.x + r2, rect.z - dx) - uv.x;

	u1 = max (uv.y + u1, rect.y) - uv.y;
	u2 = max (uv.y + u2, rect.y) - uv.y;
	d1 = min (uv.y + d1, rect.w - dy) - uv.y;
	d2 = min (uv.y + d2, rect.w - dy) - uv.y;

	return float4(
		(
			  t.Sample (smp, uv + float2(l2, u2)).rgb
			+ t.Sample (smp, uv + float2(l1, u2)).rgb * 4
			+ t.Sample (smp, uv + float2(0, u2)).rgb * 6
			+ t.Sample (smp, uv + float2(r1, u2)).rgb * 4
			+ t.Sample (smp, uv + float2(r2, u2)).rgb

			+ t.Sample (smp, uv + float2(l2, u1)).rgb * 4
			+ t.Sample (smp, uv + float2(l1, u1)).rgb * 16
			+ t.Sample (smp, uv + float2(0, u1)).rgb * 24
			+ t.Sample (smp, uv + float2(r1, u1)).rgb * 16
			+ t.Sample (smp, uv + float2(r2, u1)).rgb * 4

			+ t.Sample (smp, uv + float2(l2, 0)).rgb * 6
			+ t.Sample (smp, uv + float2(l1, 0)).rgb * 24
			+ ret.rgb * 36
			+ t.Sample (smp, uv + float2(r1, 0)).rgb * 24
			+ t.Sample (smp, uv + float2(r2, 0)).rgb * 6

			+ t.Sample (smp, uv + float2(l2, d1)).rgb * 4
			+ t.Sample (smp, uv + float2(l1, d1)).rgb * 16
			+ t.Sample (smp, uv + float2(0, d1)).rgb * 24
			+ t.Sample (smp, uv + float2(r1, d1)).rgb * 16
			+ t.Sample (smp, uv + float2(r2, d1)).rgb * 4

			+ t.Sample (smp, uv + float2(l2, d2)).rgb
			+ t.Sample (smp, uv + float2(l1, d2)).rgb * 4
			+ t.Sample (smp, uv + float2(0, d2)).rgb * 6
			+ t.Sample (smp, uv + float2(r1, d2)).rgb * 4
			+ t.Sample (smp, uv + float2(r2, d2)).rgb
		) / 256.0f, ret.a
	);
}

float4 main (VS_Output input) : SV_TARGET {
	// return texHighLum.Sample (smp, input.uv);

	if (input.uv.x < 0.2 && input.uv.y < 0.2) {
		// depth
		float depth = texDepth.Sample (smp, input.uv * 5);
		depth = 1.0f - pow (depth, 100);
		return float4 (depth, depth, depth, 1);
	} else if (input.uv.x < 0.2 && input.uv.y < 0.4) {
		// normal
		return texNormal.Sample (smp, (input.uv - float2 (0, 0.2)) * 5);
	} else if (input.uv.x < 0.2 && input.uv.y < 0.6) {
		// high luminance
		return texHighLum.Sample (smp, (input.uv - float2 (0, 0.4)) * 5);
	} else if (input.uv.x < 0.2 && input.uv.y < 0.8) {
		return texShrink.Sample (smp, (input.uv - float2(0, 0.8)) * 5);
	}

	// Deferred Shading
	//float4 normal = texNormal.Sample (smp, input.uv);
	//normal = normal * 2.0f - 1.0f;
	//float3 light = normalize (float3 (1.0f, -1.0f, 1.0f));
	//const float ambient = 0.25f;
	//float diffB = max (saturate (dot (normal.xyz, -light)), ambient);
	//return tex.Sample (smp, input.uv) * float4 (diffB, diffB, diffB, 1);
	// Deferred Shading

	float w, h, miplevels;
	tex.GetDimensions (0, w, h, miplevels);
	float dx = 1.0 / w;
	float dy = 1.0 / h;

	float4 bloomAccum = float4 (0, 0, 0, 0);
	float2 uvSize = float2(1, 0.5);
	float2 uvOfst = float2(0, 0);

	for (int i = 0; i < 8; ++i) {
		bloomAccum += Get5x5GaussianBlur (texShrink, smp, input.uv * uvSize + uvOfst, dx, dy, float4(uvOfst, uvOfst + uvSize));
		uvOfst.y += uvSize.y;
		uvSize *= 0.5f;
	}

	return tex.Sample (smp, input.uv) + Get5x5GaussianBlur (texHighLum, smp, input.uv, dx, dy, float4(uvOfst, uvOfst + uvSize)) + saturate (bloomAccum);
}

float4 BlurPS (VS_Output input) : SV_Target {
	float w, h, miplevels;
	tex.GetDimensions (0, w, h, miplevels);
	return Get5x5GaussianBlur (tex, smp, input.uv, 1.0 / w, 1.0 / h, float4 (0, 0, 1, 1));
}