#pragma once

#include <DirectXMath.h>

using namespace DirectX;

struct Vertex {
	XMFLOAT3 pos;
};

struct UVVertex {
	XMFLOAT3 pos;
	XMFLOAT2 uv;
};