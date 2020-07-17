#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <vector>
#include <string>
#include <map>
#include <unordered_map>

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
	std::vector<DirectX::XMMATRIX> _boneMatrices;
	struct BoneNode {
		int boneIdx;
		DirectX::XMFLOAT3 startPos;
		std::vector<BoneNode*> children;
	};
	std::map<std::string, BoneNode> _boneNodeTable;

	//
	// Animation
	//
	struct KeyFrame {
		unsigned int frameNo;
		DirectX::XMVECTOR quaternion;
		DirectX::XMFLOAT2 p1, p2;

		KeyFrame (unsigned int fno, DirectX::XMVECTOR& q, const DirectX::XMFLOAT2& ip1, const DirectX::XMFLOAT2& ip2) : 
				frameNo (fno), quaternion (q), p1 (ip1), p2 (ip2) {}
	};
	std::unordered_map<std::string, std::vector<KeyFrame>> _keyFrameDatas;
	DWORD _startTime;
	unsigned int _duration = 0;

	void MotionUpdate ();
	float GetYFromXOnBezier (float x, const DirectX::XMFLOAT2& a, const DirectX::XMFLOAT2& b, uint8_t n);

	void RecursiveMatrixMultipy (BoneNode* node, DirectX::XMMATRIX& mat);

	HRESULT CreateMaterialData ();
	HRESULT CreateMaterialAndTextureView ();
	HRESULT CreateTransformView ();
	HRESULT LoadPMDFile (const char* path);

public:
	PMDActor (const char* filePath, PMDRenderer& renderer);
	~PMDActor ();
	
	PMDActor* Clone ();
	
	void Update ();
	void Draw ();

	void LoadVMDFile (const char* filePath, const char* name);
	void PlayAnimation ();
};

