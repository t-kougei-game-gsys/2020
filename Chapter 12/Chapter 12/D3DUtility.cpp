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