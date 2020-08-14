#include "DX12Wrapper.h"
#include "Application.h"

#include <cassert>
#include <d3dx12.h>

#pragma comment(lib, "DirectXTex.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

namespace {

	std::string GetTexturePathFromModelAndTexPath (const std::string& modelPath, const char* texPath) {
		int pathIndex1 = modelPath.rfind ('/');
		int pathIndex2 = modelPath.rfind ('\\');
		auto pathIndex = max (pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr (0, pathIndex + 1);
		return folderPath + texPath;
	}

	string
		GetExtension (const std::string& path) {
		int idx = path.rfind ('.');
		return path.substr (idx + 1, path.length () - idx - 1);
	}

	wstring
		GetExtension (const std::wstring& path) {
		int idx = path.rfind (L'.');
		return path.substr (idx + 1, path.length () - idx - 1);
	}

	pair<string, string>
		SplitFileName (const std::string& path, const char splitter = '*') {
		int idx = path.find (splitter);
		pair<string, string> ret;
		ret.first = path.substr (0, idx);
		ret.second = path.substr (idx + 1, path.length () - idx - 1);
		return ret;
	}

	std::wstring
		GetWideStringFromString (const std::string& str) {
		auto num1 = MultiByteToWideChar (CP_ACP,
										 MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
										 str.c_str (), -1, nullptr, 0);

		std::wstring wstr;
		wstr.resize (num1);

		auto num2 = MultiByteToWideChar (CP_ACP,
										 MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
										 str.c_str (), -1, &wstr[0], num1);

		assert (num1 == num2);
		return wstr;
	}

	void EnableDebugLayer () {
		ComPtr<ID3D12Debug> debugLayer = nullptr;
		auto result = D3D12GetDebugInterface (IID_PPV_ARGS (&debugLayer));
		debugLayer->EnableDebugLayer ();
	}

}

DX12Wrapper::DX12Wrapper (HWND hwnd) : _parallelLightVec (1, -1, 1) {
#ifdef _DEBUG
	EnableDebugLayer ();
#endif

	auto& app = Application::Instance ();
	_winSize = app.GetWindowSize ();

	if (FAILED (InitializeDXGIDevice ())) {
		assert (0);
		return;
	}

	if (FAILED (InitializeCommand ())) {
		assert (0);
		return;
	}

	if (FAILED (CreateSwapChain (hwnd))) {
		assert (0);
		return;
	}

	if (FAILED (CreateFinalRenderTargets ())) {
		assert (0);
		return;
	}

	if (FAILED (CreateSceneView ())) {
		assert (0);
		return;
	}

	CreateTextureLoaderTable ();

	if (FAILED (CreateDepthStencilView ())) {
		assert (0);
		return;
	}

	if (FAILED (_dev->CreateFence (_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (_fence.ReleaseAndGetAddressOf ())))) {
		assert (0);
		return;
	}

}

HRESULT DX12Wrapper::CreateDepthStencilView () {
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto hr = _swapchain->GetDesc1 (&desc);

	D3D12_RESOURCE_DESC resdesc = {};
	resdesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resdesc.DepthOrArraySize = 1;
	resdesc.Width = desc.Width;
	resdesc.Height = desc.Height;
	resdesc.Format = DXGI_FORMAT_D32_FLOAT;
	resdesc.SampleDesc.Count = 1;
	resdesc.SampleDesc.Quality = 0;
	resdesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
	resdesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resdesc.MipLevels = 1;
	resdesc.Alignment = 0;

	auto depthHeapProp = CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_DEFAULT);

	CD3DX12_CLEAR_VALUE depthClearValue (DXGI_FORMAT_D32_FLOAT, 1.0f, 0);

	hr = _dev->CreateCommittedResource (
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resdesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&depthClearValue,
		IID_PPV_ARGS (_depthBuffer.ReleaseAndGetAddressOf ())
	);

	if (FAILED (hr)) {
		return hr;
	}

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = _dev->CreateDescriptorHeap (&dsvHeapDesc, IID_PPV_ARGS (_dsvHeap.ReleaseAndGetAddressOf ()));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	_dev->CreateDepthStencilView (_depthBuffer.Get (), &dsvDesc, _dsvHeap->GetCPUDescriptorHandleForHeapStart ());

	return S_OK;
}

DX12Wrapper::~DX12Wrapper () {}

ComPtr<ID3D12Resource> DX12Wrapper::GetTextureByPath (const char* texpath) {
	auto it = _textureTable.find (texpath);
	if (it != _textureTable.end ()) {
		return _textureTable[texpath];
	} else {
		return ComPtr<ID3D12Resource> (CreateTextureFromFile (texpath));
	}
}

void DX12Wrapper::CreateTextureLoaderTable () {
	_loadLambdaTable["sph"] = _loadLambdaTable["spa"] = _loadLambdaTable["bmp"] = _loadLambdaTable["png"] = _loadLambdaTable["jpg"] = [](const wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromWICFile (path.c_str (), WIC_FLAGS_NONE, meta, img);
	};

	_loadLambdaTable["tga"] = [](const wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromTGAFile (path.c_str (), meta, img);
	};

	_loadLambdaTable["dds"] = [](const wstring& path, TexMetadata* meta, ScratchImage& img)->HRESULT {
		return LoadFromDDSFile (path.c_str (), DDS_FLAGS_NONE, meta, img);
	};
}

ID3D12Resource* DX12Wrapper::CreateTextureFromFile (const char* texpath) {
	string texPath = texpath;
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};
	auto wtexpath = GetWideStringFromString (texPath);
	auto ext = GetExtension (texPath);
	auto hr = _loadLambdaTable[ext] (wtexpath,
										 &metadata,
										 scratchImg);

	if (FAILED (hr)) {
		return nullptr;
	}

	auto img = scratchImg.GetImage (0, 0, 0);

	auto texHeapProp = CD3DX12_HEAP_PROPERTIES (D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D (metadata.format, metadata.width, metadata.height, metadata.arraySize, metadata.mipLevels);

	ID3D12Resource* texbuffer = nullptr;
	hr = _dev->CreateCommittedResource (
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS (&texbuffer)
	);

	if (FAILED (hr)) {
		return nullptr;
	}

	hr = texbuffer->WriteToSubresource (0,
										nullptr,
										img->pixels,
										img->rowPitch,
										img->slicePitch
	);

	if (FAILED (hr)) {
		return nullptr;
	}

	return texbuffer;
}

HRESULT DX12Wrapper::InitializeDXGIDevice () {
	UINT flagsDXGI = 0;
	flagsDXGI |= DXGI_CREATE_FACTORY_DEBUG;
	auto hr = CreateDXGIFactory2 (flagsDXGI, IID_PPV_ARGS (_dxgiFactory.ReleaseAndGetAddressOf ()));

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	if (FAILED (hr)) {
		return hr;
	}

	std::vector <IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters (i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i) {
		adapters.push_back (tmpAdapter);
	}

	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc (&adesc);
		std::wstring strDesc = adesc.Description;
		if (strDesc.find (L"NVIDIA") != std::string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}

	hr = S_FALSE;

	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (SUCCEEDED (D3D12CreateDevice (tmpAdapter, l, IID_PPV_ARGS (_dev.ReleaseAndGetAddressOf ())))) {
			featureLevel = l;
			hr = S_OK;
			break;
		}
	}

	return hr;
}

HRESULT DX12Wrapper::CreateSwapChain (const HWND& hwnd) {
	RECT rc = {};
	::GetWindowRect (hwnd, &rc);

	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = _winSize.cx;
	swapchainDesc.Height = _winSize.cy;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapchainDesc.BufferCount = 2;
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	auto hr = _dxgiFactory->CreateSwapChainForHwnd (_cmdQueue.Get (),
														hwnd,
														&swapchainDesc,
														nullptr,
														nullptr,
														(IDXGISwapChain1**)_swapchain.ReleaseAndGetAddressOf ());

	assert (SUCCEEDED (hr));

	return hr;
}

HRESULT DX12Wrapper::InitializeCommand () {
	auto hr = _dev->CreateCommandAllocator (D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS (_cmdAllocator.ReleaseAndGetAddressOf ()));
	if (FAILED (hr)) {
		assert (0);
		return hr;
	}

	hr = _dev->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator.Get (), nullptr, IID_PPV_ARGS (_cmdList.ReleaseAndGetAddressOf ()));
	if (FAILED (hr)) {
		assert (0);
		return hr;
	}

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	hr = _dev->CreateCommandQueue (&cmdQueueDesc, IID_PPV_ARGS (_cmdQueue.ReleaseAndGetAddressOf ()));

	assert (SUCCEEDED (hr));

	return hr;
}

HRESULT DX12Wrapper::CreateSceneView () {
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto hr = _swapchain->GetDesc1 (&desc);

	hr = _dev->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer ((sizeof (SceneData) + 0xff) & ~0xff),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (_sceneConstBuff.ReleaseAndGetAddressOf ())
	);

	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return hr;
	}

	_mappedSceneData = nullptr;
	hr = _sceneConstBuff->Map (0, nullptr, (void**)&_mappedSceneData);

	XMFLOAT3 eye (5, 10, -30);
	XMFLOAT3 target (0, 10, 0);
	XMFLOAT3 up (0, 1, 0);
	_mappedSceneData->view = XMMatrixLookAtLH (XMLoadFloat3 (&eye), XMLoadFloat3 (&target), XMLoadFloat3 (&up));
	_mappedSceneData->proj = XMMatrixPerspectiveFovLH (XM_PIDIV4,
													   static_cast<float>(desc.Width) / static_cast<float>(desc.Height),
													   0.1f,
													   1000.0f
	);
	_mappedSceneData->eye = eye;
	
#pragma region Chapter 13
	XMFLOAT4 planeVec (0, 1, 0, 0);
	_mappedSceneData->shadow = XMMatrixShadow (
		XMLoadFloat4 (&planeVec),
		-XMLoadFloat3 (&_parallelLightVec)
	);
#pragma endregion

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hr = _dev->CreateDescriptorHeap (&descHeapDesc, IID_PPV_ARGS (_sceneDescHeap.ReleaseAndGetAddressOf ()));

	auto heapHandle = _sceneDescHeap->GetCPUDescriptorHandleForHeapStart ();

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _sceneConstBuff->GetGPUVirtualAddress ();
	cbvDesc.SizeInBytes = _sceneConstBuff->GetDesc ().Width;
	_dev->CreateConstantBufferView (&cbvDesc, heapHandle);

	return hr;
}

HRESULT DX12Wrapper::CreateFinalRenderTargets () {
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	auto hr = _swapchain->GetDesc1 (&desc);

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = _dev->CreateDescriptorHeap (&heapDesc, IID_PPV_ARGS (_rtvHeaps.ReleaseAndGetAddressOf ()));
	if (FAILED (hr)) {
		SUCCEEDED (hr);
		return hr;
	}

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	hr = _swapchain->GetDesc (&swcDesc);
	_backBuffers.resize (swcDesc.BufferCount);

	D3D12_CPU_DESCRIPTOR_HANDLE handle = _rtvHeaps->GetCPUDescriptorHandleForHeapStart ();

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (int i = 0; i < swcDesc.BufferCount; ++i) {
		hr = _swapchain->GetBuffer (i, IID_PPV_ARGS (&_backBuffers[i]));
		assert (SUCCEEDED (hr));
		rtvDesc.Format = _backBuffers[i]->GetDesc ().Format;
		_dev->CreateRenderTargetView (_backBuffers[i], &rtvDesc, handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	_viewport.reset (new CD3DX12_VIEWPORT (_backBuffers[0]));
	_scissorRect.reset (new CD3DX12_RECT (0, 0, desc.Width, desc.Height));

	return hr;
}

ComPtr< ID3D12Device> DX12Wrapper::Device () {
	return _dev;
}

ComPtr < ID3D12GraphicsCommandList> DX12Wrapper::CommandList () {
	return _cmdList;
}

void DX12Wrapper::Update () {}

void DX12Wrapper::BeginDraw () {
	auto bbIdx = _swapchain->GetCurrentBackBufferIndex ();

	_cmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (_backBuffers[bbIdx],
						D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));


	auto rtvH = _rtvHeaps->GetCPUDescriptorHandleForHeapStart ();
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	auto dsvH = _dsvHeap->GetCPUDescriptorHandleForHeapStart ();
	_cmdList->OMSetRenderTargets (1, &rtvH, false, &dsvH);
	_cmdList->ClearDepthStencilView (dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	float clearColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
	_cmdList->ClearRenderTargetView (rtvH, clearColor, 0, nullptr);

	_cmdList->RSSetViewports (1, _viewport.get ());
	_cmdList->RSSetScissorRects (1, _scissorRect.get ());
}

void DX12Wrapper::SetScene () {
	ID3D12DescriptorHeap* sceneheaps[] = {_sceneDescHeap.Get ()};
	_cmdList->SetDescriptorHeaps (1, sceneheaps);
	_cmdList->SetGraphicsRootDescriptorTable (0, _sceneDescHeap->GetGPUDescriptorHandleForHeapStart ());
}

void DX12Wrapper::EndDraw () {
	auto bbIdx = _swapchain->GetCurrentBackBufferIndex ();
	_cmdList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (_backBuffers[bbIdx],
						D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	_cmdList->Close ();

	ID3D12CommandList* cmdlists[] = {_cmdList.Get ()};
	_cmdQueue->ExecuteCommandLists (1, cmdlists);
	_cmdQueue->Signal (_fence.Get (), ++_fenceVal);

	if (_fence->GetCompletedValue () < _fenceVal) {
		auto event = CreateEvent (nullptr, false, false, nullptr);
		_fence->SetEventOnCompletion (_fenceVal, event);
		WaitForSingleObject (event, INFINITE);
		CloseHandle (event);
	}

	_cmdAllocator->Reset ();
	_cmdList->Reset (_cmdAllocator.Get (), nullptr);
}

ComPtr < IDXGISwapChain4> DX12Wrapper::Swapchain () {
	return _swapchain;
}