#pragma once

#include <string>
#include <DirectXMath.h>
#include <sstream>
#include <vector>

using namespace std;
using namespace DirectX;

namespace D3D {
	unsigned int Get256Times (unsigned int size);

	void PrintfFloat3 (ostringstream& oss, string start, XMFLOAT3& vector, string end);

	//
	// Color
	//
	const float BLACK[] = {0.0f, 0.0f, 0.0f, 1.0f};
	const float WHITE[] = {1.0f, 1.0f, 1.0f, 1.0f};
	const float RED[] = {1.0f, 0.0f, 0.0f, 1.0f};
	const float GREEN[] = {0.0f, 1.0f, 0.0f, 1.0f};
	const float BLUE[] = {0.0f, 0.0f, 1.0f, 1.0f};

	extern vector<float> GetGaussianWeights (size_t count, float s);

	// アライメント数値を返す
	extern unsigned int AligmentedValue (unsigned int size, unsigned int alignment = 16);
}