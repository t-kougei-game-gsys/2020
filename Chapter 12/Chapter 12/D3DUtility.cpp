#include "D3DUtility.h"

#include <iostream>
#include <sstream>

unsigned int D3D::Get256Times (unsigned int size) {
	return ((size + 0xff) & ~0xff);
}

void D3D::PrintfFloat3 (ostringstream& oss, string start, XMFLOAT3& vector, string end) {
	oss << start << "(";
	oss << vector.x << ", ";
	oss << vector.y << ", ";
	oss << vector.z << ")";
	oss << end;
}

#pragma region Chpater 12

vector<float> D3D::GetGaussianWeights (size_t count, float s) {
	vector<float> weights (count);
	float x = 0.0f;
	float total = 0.0f;
	for (auto& wgt : weights) {
		wgt = expf (-(x * x) / (2 * s * s));
		total += wgt;
		x += 1.0f;
	}

	total = total * 2.0f - 1.0f;

	for (auto& wgt : weights) {
		wgt /= total;
	}

	return weights;
}

unsigned int D3D::AligmentedValue (unsigned int size, unsigned int alignment) {
	return (size + alignment - (size % alignment));
}

#pragma endregion