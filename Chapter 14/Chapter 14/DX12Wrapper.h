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
#include <array>

class DX12Wrapper {

	struct SceneData {
		DirectX::XMMATRIX view;
		DirectX::XMMATRIX proj;
		DirectX::XMFLOAT3 eye;
	};

	SIZE _winSize;
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
	ComPtr<ID3D12DescriptorHeap> _rtvHeaps = nullptr;
	ComPtr<ID3D12DescriptorHeap> _dsvHeap = nullptr;
	std::unique_ptr<D3D12_VIEWPORT> _viewport;
	std::unique_ptr<D3D12_RECT> _scissorRect;

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

	HRESULT	CreateFinalRenderTargets ();
	HRESULT CreateDepthStencilView ();
	HRESULT CreateSwapChain (const HWND& hwnd);
	HRESULT InitializeDXGIDevice ();
	HRESULT InitializeCommand ();
	HRESULT CreateSceneView ();
	void CreateTextureLoaderTable ();
	ID3D12Resource* CreateTextureFromFile (const char* texPath);

#pragma region Chapater 14

	std::array<ComPtr<ID3D12Resource>, 2> _peraResources;
	ComPtr<ID3D12DescriptorHeap> _peraRTVHeap;
	ComPtr<ID3D12DescriptorHeap> _peraSRVHeap;

	ComPtr<ID3D12Resource> _vb;
	D3D12_VERTEX_BUFFER_VIEW _vbv;

	ComPtr<ID3D12RootSignature> _rs;
	ComPtr<ID3D12PipelineState> _pp;
	ComPtr<ID3D12PipelineState> _blurPP;

	std::array<ComPtr<ID3D12Resource>, 2> _bloomBuffers;
	ComPtr<ID3D12Resource> _dofBuffer;

	void CreateRTVAndSRVHeap ();
	void CreateVertex ();
	void CreatePipeline ();
	
	void CreateBloomResources ();
	void CreateBlurForDOFBuffer ();

#pragma endregion

public:
	DX12Wrapper (HWND hwnd);
	~DX12Wrapper ();

	void Update ();
	void EndDraw ();

	ComPtr<ID3D12Resource> GetTextureByPath (const char* texPath);

	ComPtr<ID3D12Device> Device ();
	ComPtr<ID3D12GraphicsCommandList> CommandList ();
	ComPtr<IDXGISwapChain4> Swapchain ();

	void SetScene ();

#pragma region Chapter 14

	void DrawShrinkTextureForBlur ();

	void PrePeraDraw ();
	void PostPeraDraw ();

	void PreDraw ();
	void Draw ();

#pragma endregion

};