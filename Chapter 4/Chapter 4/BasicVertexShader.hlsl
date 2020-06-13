struct Output {
	float4 pos : POSITION;
	float4 sv_pos : SV_POSITION;
};

Output BasicVS (float4 pos : POSITION) {
	Output output;
	output.pos = pos;
	output.sv_pos = pos;
	return output;
}