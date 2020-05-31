// pos : The calculated position in vertex shader
// return color of the render target

struct Input {
	float4 pos:POSITION;
	float4 svpos:SV_POSITION;
};

float4 BasicPS (Input input) : SV_TARGET {
	//return float4(1, 1, 1, 1);
	return float4((float2(0 ,1) + input.pos.xy) * 0.5f, 1, 1);
}