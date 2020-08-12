#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <map>
#include <unordered_map>
#include <DirectXTex.h>
#include <wrl.h>
#include <string>
#include <functional>
#include <memory>

#include "D3DUtility.h"

class PMDActor;
class PMDRenderer;
class DX12Wrapper {

	struct SceneData {
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
		DirectX::XMFLOAT3 eye;
	};

	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;
	
	//
	// DXGI
	//
	ComPtr<IDXGIFactory4> _dxgiFactory = nullptr;
	ComPtr<IDXGISwapChain4> _swapchain = nullptr;
	
	//
	// DirectX 12
	//
	ComPtr<ID3D12Device> _dev = nullptr;
	ComPtr<ID3D12CommandAllocator> _cmdAllocator = nullptr;
	ComPtr<ID3D12GraphicsCommandList> _cmdList = nullptr;
	ComPtr<ID3D12CommandQueue> _cmdQueue = nullptr;

	//
	// Display
	//	
	ComPtr<ID3D12Resource> _depthBuffer = nullptr;
	std::vector<ID3D12Resource*> _backBuffers;
	ComPtr<ID3D12DescriptorHeap> _rtvHeap = nullptr;
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;
	//std::unique_ptr<D3D12_VIEWPORT> _viewport;
	//std::unique_ptr<D3D12_RECT> _scissorRect;

	//
	// Scene Data
	//	
	ComPtr<ID3D12Resource> _sceneConstBuff = nullptr;
	SceneData* _mappedSceneData;
	ComPtr<ID3D12DescriptorHeap> _sceneDescHeap = nullptr;

	//
	// Fence
	//
	ComPtr<ID3D12Fence> _fence = nullptr;
	UINT64 _fenceVal = 0;
	
	//
	// Load Texture Lambda
	//
	using LoadLambda_t = std::function<HRESULT (const std::wstring & path, DirectX::TexMetadata*, DirectX::ScratchImage&)>;
	std::map < std::string, LoadLambda_t> _loadLambdaTable;
	std::unordered_map<std::string, ComPtr<ID3D12Resource>> _textureTable;

#pragma region Chapter 12

	HWND _hwnd;
	DirectX::XMFLOAT3 _eye;
	DirectX::XMFLOAT3 _target;
	DirectX::XMFLOAT3 _up;
	float _fov = DirectX::XM_PI / 6;	// 30Åã

	ComPtr<ID3D12Resource> _peraResource;
	ComPtr<ID3D12Resource> _peraResource2;
	ComPtr<ID3D12DescriptorHeap> _peraRTVHeap;
	ComPtr<ID3D12DescriptorHeap> _peraRegisterHeap;

	ComPtr<ID3D12Resource> _peraVB;
	D3D12_VERTEX_BUFFER_VIEW _peraVBV;

	ComPtr<ID3D12PipelineState> _peraPipeline;
	ComPtr<ID3D12PipelineState> _peraPipeline2;
	ComPtr<ID3D12RootSignature> _peraRS;

	ComPtr<ID3D12Resource> _bokehParamResource;

	bool CreatePeraResourcesAndView ();
	bool CreatePeraVertex ();
	bool CreatePeraPipeline ();

	bool CreateBokehParamResource ();

	void Barrier (ID3D12Resource* p,
				  D3D12_RESOURCE_STATES before,
				  D3D12_RESOURCE_STATES after);

	void WaitForCommandQueue ();

#pragma endregion

	HRESULT	CreateFinalRenderTargets ();
	HRESULT CreateDepthStencilView ();
	HRESULT CreateSwapChain (const HWND& hwnd);
	HRESULT InitializeDXGIDevice ();
	HRESULT InitializeCommand ();
	HRESULT CreateSceneView ();
	void CreateTextureLoaderTable ();
	ID3D12Resource* CreateTextureFromFile (const char* texPath);

public:
	DX12Wrapper (HWND hwnd);
	~DX12Wrapper ();

	//void Update ();
	//void BeginDraw ();
	//void EndDraw ();

#pragma region Chapter 12
	bool Init ();

	bool PreDrawToPera1 ();
	void DrawToPera1 (std::shared_ptr<PMDRenderer> renderer);
	void PostDrawToPera1 ();
	void DrawHorizontalBokeh ();
	bool Clear ();
	void Draw (std::shared_ptr<PMDRenderer> renderer);
	void Flip ();


#pragma endregion

	ComPtr<ID3D12Resource> GetTextureByPath (const char* texPath);

	ComPtr<ID3D12Device> Device ();
	ComPtr<ID3D12GraphicsCommandList> CommandList ();
	ComPtr<IDXGISwapChain4> Swapchain ();

	// void SetScene ();
};