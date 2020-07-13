#pragma once

#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl.h>
#include <vector>
#include <string>

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
	Transform* _mappedTransform = nullptr;
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
};

