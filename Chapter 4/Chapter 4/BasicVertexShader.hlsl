//
// original name : main ()
// set entry name when function name has changed.
// and set shader model 5.0
//

struct Output {
	float4 pos:POSITION;
	float4 svpos:SV_POSITION;
};

// Returns float4 as coordinate of rasterizer paramater.
Output BasicVS(float4 pos : POSITION) {
	Output output;
	output.pos = pos;
	output.svpos = pos;
	return output;
}