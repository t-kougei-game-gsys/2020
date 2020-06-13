struct Input {
	float4 pos : POSITION;
};

float4 BasicPS (Input input) : SV_TARGET {
	return float4((float2(0, 1) + input.pos.xy) * 0.8f, 0.7f, 1.0f);
}