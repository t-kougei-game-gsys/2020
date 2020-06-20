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

struct PMDVertex {
	XMFLOAT3 pos;				// 頂点座標 
	XMFLOAT3 normal;			// 法線ベクトル
	XMFLOAT2 uv;				// UV座標
	unsigned short boneNo[2];	// ボーン番号
	unsigned char boneWeight;	// ボーン影響度
	unsigned char edgeFlg;		// 輪郭線フラグ
};