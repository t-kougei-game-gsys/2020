#include "D3DUtility.h"

/* Process
	Create window
	Init DX
		step 1 : Create DXGIFactory
		step 2 : Create D3D12Device
			step 2.1 : Find all adapters
			step 2.2 : Find available feature level and create device
			step 2.3 : Create Device
		step 3 : Create GraphicsCommandList & CommandAllocator & CommandQueue
			step 3.1 : Create CommandAllocator
			step 3.3 : Create CommandList
			step 3.3 : Create CommandQueue
		step 4 : Create Swapchain
		Step 5 : step 5 : Create DescriptorHeap (Render Target View)
			step 5.1 : Create DescriptorHeap
			step 5.2 : Connect buffer of swapchain to render target view
		step 6 : Create Fence & Barrier
			step 6.1 : Create Fence
			step 6.2 : Create Barrier
		Step 7 : Render
			step 7.1 : Get current back buffer
			step 7.2 : Set current back buffer handle
			step 7.3 : Set barrier (resource : render target)
			step 7.4 : Create "SetRenderTarget" command
			step 7.5 : Create "ClearRenderTargetView" command
			step 7.6 : Set barrier (render target -> present)
			step 7.7 : Create "Close" command
			step 7.8 : Execute commands
		step 8 : Wait for all commands to complete
		step 9 : Reset & Present
	Debug
*/
bool D3D::InitD3D (HINSTANCE hInstance, int width, int height, D3D_DESC* pDesc) {
	//
	// Create window.
	//

	WNDCLASSEX w = {};
	w.cbSize = sizeof (WNDCLASSEX);
	w.lpszClassName = _T ("DX12");
	w.hInstance = hInstance;
	w.lpfnWndProc = D3D::WndProc;
	if (!RegisterClassEx (&w)) {
		printf_s ("ERROR : RegisterClass() - FAILED");
		return false;
	}

	HWND hwnd = 0;
	hwnd = CreateWindow (w.lpszClassName,
						 _T ("DX 12"),
						 WS_OVERLAPPEDWINDOW,
						 CW_USEDEFAULT,
						 CW_USEDEFAULT,
						 width,
						 height,
						 nullptr,
						 nullptr,
						 hInstance,
						 nullptr);

	if (!hwnd) {
		printf_s ("ERROR - CreateWindow() - FAILED");
		return false;
	}

	ShowWindow (hwnd, SW_SHOW);

	//
	// Init DX
	//

	HRESULT hr = 0;

	IDXGIFactory6* dxgiFactory = nullptr;
	ID3D12Device* dev = nullptr;
	ID3D12CommandAllocator* cmdAllocator;
	ID3D12GraphicsCommandList* cmdList;
	ID3D12CommandQueue* cmdQueue;
	IDXGISwapChain4* swapChain = nullptr;
	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	ID3D12Fence* fence = nullptr;
	UINT64 fenceVal = 0;
	D3D12_RESOURCE_BARRIER barrierDesc;

	std::vector<ID3D12Resource*> backBuffers;

	//
	// step 1 : Create DXGIFactory
	//

#ifdef _DEBUG
	// CreateDXGIFactory2 : Can use debug mode.
	hr = CreateDXGIFactory2 (DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS (&dxgiFactory));
#else
	hr = CreateDXGIFactory1 (IID_PPV_ARGS (&_dxgiFactory));
#endif

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateDXGIFactory() - FAILED\n");
		return false;
	}

	//
	// step 2 : Create D3D12Device
	//

	// step 2.1 : Find all adapters.

	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = 0;
	for (int i = 0; dxgiFactory->EnumAdapters (i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
		adapters.push_back (tmpAdapter);
	}

	// Find NVDIA adapter.

	for (auto adpt : adapters) {
		DXGI_ADAPTER_DESC desc = {};
		adpt->GetDesc (&desc);

		// wstring : unicode text.
		std::wstring strDesc = desc.Description;

		// L : Converted to unicode text.
		if (strDesc.find (L"NVIDIA") != std::string::npos) {
			tmpAdapter = adpt;
			break;
		}
	}

	// step 2.2 : Find available feature level and create device.

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	// step 2.3 : Create Device

	D3D_FEATURE_LEVEL featureLevel;
	for (auto level : levels) {
		if (D3D12CreateDevice (tmpAdapter, level, IID_PPV_ARGS (&dev)) == S_OK) {
			featureLevel = level;
			break;
		}
	}

	if (0 == dev) {
		printf_s ("ERROR : D3D12CreateDevice() - FAILED\n");
		return false;
	}

	//
	// step 3 : Create GraphicsCommandList & CommandAllocator & CommandQueue 
	//

	// step 3.1 : Create CommandAllocator

	hr = dev->CreateCommandAllocator (D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS (&cmdAllocator));
	if (FAILED (hr)) {
		printf_s ("ERROR : CreateAllocator() - FAILED\n");
		return false;
	}

	// step 3.2 : Create CommandList

	hr = dev->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT, cmdAllocator, nullptr, IID_PPV_ARGS (&cmdList));
	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommandList() - FAILED\n");
		return false;
	}

	// step 3.3 : Create CommandQueue

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hr = dev->CreateCommandQueue (&cmdQueueDesc, IID_PPV_ARGS (&cmdQueue));
	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommandQueue() - FAILED\n");
		return false;
	}

	//
	// step 4 : Create Swapchain
	//

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	// allow tearing.
	// swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	hr = dxgiFactory->CreateSwapChainForHwnd (cmdQueue,
											  hwnd,
											  &swapChainDesc,
											  nullptr,
											  nullptr,
											  (IDXGISwapChain1**)&swapChain);

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateSwapChain() - FAILED\n");
		return false;
	}

	//
	// step 5 : Create DescriptorHeap (Render Target View)
	//

	// step 5.1 : Create DescriptorHeap

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;		// render target view
	heapDesc.NodeMask = 0;								// when 2 gpu...
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = dev->CreateDescriptorHeap (&heapDesc, IID_PPV_ARGS (&rtvHeaps));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateDescriptorHeap() - FAILED\n");
		return -1;
	}

	// step 5.2 : Connect buffer of swapchain to render target view

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	swapChain->GetDesc (&swcDesc);
	// 直接に前のswapChainDescを使用してもいい
	backBuffers = std::vector<ID3D12Resource*> (swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart ();

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (int i = 0; i < swcDesc.BufferCount; ++i) {
		hr = swapChain->GetBuffer (i, IID_PPV_ARGS (&backBuffers[i]));

		if (FAILED (hr)) {
			printf_s ("ERROR : GetBuffer(%d) - FAILED\n", i);
			continue;
		}

		dev->CreateRenderTargetView (backBuffers[i], &rtvDesc, handle);
		handle.ptr += dev->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	if (FAILED (hr)) {
		printf_s ("ERROR : GetBuffer() - FAILED\n");
		return -1;
	}

	//
	// step 6 : Create Fence & Barrier
	//

	// step 6.1 : Create Fence

	hr = dev->CreateFence (fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (&fence));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateFence() - FAILED\n");
		return false;
	}

	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	// step 6.3 : in main.cpp->Display method

	(*pDesc).WNDClass = w;
	(*pDesc).Device = dev;
	(*pDesc).SwapChain = swapChain;
	(*pDesc).CMDList = cmdList;
	(*pDesc).CMDQueue = cmdQueue;
	(*pDesc).CMDAllocator = cmdAllocator;
	(*pDesc).RTVHeap = rtvHeaps;
	(*pDesc).Fence = fence;
	(*pDesc).FenceVal = fenceVal;
	(*pDesc).BarrierDesc = barrierDesc;
	(*pDesc).BackBuffers = backBuffers;

	return true;
}

void DisplayFPS (float deltaTime) {
	static float lastTime = (float)timeGetTime ();
	static DWORD frameCnt = 0;
	static float timeElapsed = 0.0f;
	static float fps = 0.0f;

	frameCnt++;
	timeElapsed += deltaTime;
	if (timeElapsed >= 1.0f) {
		fps = (float)frameCnt / timeElapsed;
		printf_s ("FPS : %.1f\n", fps);
		timeElapsed = 0.0f;
		frameCnt = 0;
	}
}

int D3D::EnterMsgLoop (bool (*ptr_display)(float deltaTime)) {
	MSG msg;
	ZeroMemory (&msg, sizeof (MSG));

	static float lastTime = (float)timeGetTime ();

	while (msg.message != WM_QUIT) {
		if (PeekMessage (&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		} else {
			float currTime = (float)timeGetTime ();
			float deltaTime = (currTime - lastTime) * 0.001f;

			DisplayFPS (deltaTime);

			ptr_display (deltaTime);

			lastTime = currTime;
		}
	}

	return msg.wParam;
}