#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>

using namespace DirectX;
using namespace std;

class PMDRenderer;
class DX12Wrapper;
class PMDActor {
	friend PMDRenderer;

private:

	struct MaterialForHlsl {
		DirectX::XMFLOAT3 diffuse;
		float alpha;
		DirectX::XMFLOAT3 specular;
		float specularity;
		DirectX::XMFLOAT3 ambient;
	};

	struct AdditionalMaterial {
		std::string texPath;
		int toonIdx;
		bool edgeFlg;
	};
	
	struct Material {
		unsigned int indicesNum;
		MaterialForHlsl material;
		AdditionalMaterial additional;
	};

	struct Transform {
		DirectX::XMMATRIX world;

		void* operator new (size_t size) {
			return _aligned_malloc (size, 16);
		}
	};

	struct PMDIK {
		uint16_t boneIdx;					// bone index
		uint16_t targetIdx;					// target bone index
		// uint8_t chainLen;				// length
		uint16_t iterations;				// execute count
		float limit;						// angle limit
		std::vector<uint16_t> nodeIdxes;	// bones
	};

	PMDRenderer& _renderer;
	DX12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	//
	// Vertex
	//
	ComPtr<ID3D12Resource> _vb = nullptr;
	ComPtr<ID3D12Resource> _ib = nullptr;
	D3D12_VERTEX_BUFFER_VIEW _vbView = {};
	D3D12_INDEX_BUFFER_VIEW _ibView = {};

	//
	// Transformation
	//
	ComPtr<ID3D12Resource> _transformMatBuffer = nullptr;
	ComPtr<ID3D12DescriptorHeap> _transformHeap = nullptr;
	Transform _transform;
	DirectX::XMMATRIX* _mappedMatrices = nullptr;
	ComPtr<ID3D12Resource> _transformBuffer = nullptr;
	float _angle;

	//
	// Material
	//
	std::vector<Material> _materials;
	ComPtr<ID3D12Resource> _materialBuffer = nullptr;
	std::vector<ComPtr<ID3D12Resource>> _textureResources;
	std::vector<ComPtr<ID3D12Resource>> _sphResources;
	std::vector<ComPtr<ID3D12Resource>> _spaResources;
	std::vector<ComPtr<ID3D12Resource>> _toonResources;
	ComPtr<ID3D12DescriptorHeap> _materialHeap = nullptr;
	
	//
	// Bone
	//
	std::vector<DirectX::XMMATRIX> _boneMatrices;	// KeyFrame animation matrix
	struct BoneNode {
		uint32_t boneIdx;
		uint32_t boneType;
		uint32_t parentBone;
		uint32_t ikParentBone;
		XMFLOAT3 startPos;
		vector<BoneNode*> children;
	};
	map<string, BoneNode> _boneNodeTable;
	vector<string> _boneNameArray;
	vector<BoneNode*> _boneNodeAddressArray;
	std::vector<uint32_t> _kneeIdxes;

	//
	// Animation
	//
	struct KeyFrame {
		unsigned int frameNo;
		XMFLOAT2 p1, p2;		// bezier point
		XMVECTOR quaternion;	// rotation
		XMFLOAT3 offset;		// translation

		KeyFrame (unsigned int fno, XMVECTOR& q, XMFLOAT3& ofst, const XMFLOAT2& ip1, const XMFLOAT2& ip2) : 
				frameNo (fno), quaternion (q), offset (ofst), p1 (ip1), p2 (ip2) {}
	};
	unordered_map<string, vector<KeyFrame>> _keyFrameDatas;
	DWORD _startTime;
	unsigned int _duration = 0;

	//
	// IK
	//
	vector<PMDIK> _ikData;
	
	struct VMDIKEnable {
		uint32_t frameNo;
		unordered_map<string, bool> ikEnableTable;
	};
	std::vector<VMDIKEnable> _ikEnableData;

	// CCD-IK
	// @param ik IK Object
	void SolveCCDIK (const PMDIK& ik);

	// ó]å∑íËóùIK
	// @param ik IK Object
	void SolveCosineIK (const PMDIK& ik);

	// LookAt
	// @param ik IK Object
	void SolveLookAt (const PMDIK& ik);

	void IKSolve (int frameNo);

	void MotionUpdate ();
	float GetYFromXOnBezier (float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);

	void RecursiveMatrixMultipy (BoneNode* node, const XMMATRIX& mat, bool flg = false);

	HRESULT CreateMaterialData ();
	HRESULT CreateMaterialAndTextureView ();
	HRESULT CreateTransformView ();
	HRESULT LoadPMDFile (const char* path);

#pragma region Chapter 13

	unsigned int _idxNum;

#pragma endregion

public:
	PMDActor (const char* filePath, PMDRenderer& renderer);
	~PMDActor ();
	
	PMDActor* Clone ();
	
	void Update ();
	void Draw (bool isShadow);

	void LoadVMDFile (const char* filePath, const char* name);
	void PlayAnimation ();
};