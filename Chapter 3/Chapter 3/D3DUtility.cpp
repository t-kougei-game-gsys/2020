#include "D3DUtility.h"

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;
IDXGISwapChain4* _swapChain = nullptr;
ID3D12DescriptorHeap* _rtvHeaps = nullptr;
ID3D12Fence* _fence = nullptr;
UINT64 _fenceVal = 0;
D3D12_RESOURCE_BARRIER _barrierDesc = {};

std::vector<ID3D12Resource*> _backBuffers;

WNDCLASSEX _w;

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

bool D3D::InitD3D (HINSTANCE hInstance, int width, int height) {
	//
	// Register window class.
	//

	_w.cbSize = sizeof (WNDCLASSEX);
	_w.lpszClassName = _T("DX12");
	_w.hInstance = hInstance;
	_w.lpfnWndProc = D3D::WndProc;
	if (!RegisterClassEx (&_w)) {
		printf_s ("ERROR : RegisterClass() - FAILED");
		return false;
	}

	//
	// Create window.
	//
	HWND hwnd = 0;
	hwnd = CreateWindow (_w.lpszClassName,
						_T("DX 12"),
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
	// DX Init
	//

	//
	// step 1 : Create DXGIFactory and find DXGIAdapter
	//

	HRESULT hr = 0;

#ifdef _DEBUG
	// CreateDXGIFactory2 : Can use debug mode.
	hr = CreateDXGIFactory2 (DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS (&_dxgiFactory));
#else
	hr = CreateDXGIFactory1 (IID_PPV_ARGS (&_dxgiFactory));
#endif

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateDXGIFactory() - FAILED\n");
		return false;
	}

	// Find all adapters.
	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = 0;
	for (int i = 0; _dxgiFactory->EnumAdapters (i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
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

	//
	// step 2 : Find available featrue level and create device.
	//

	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	D3D_FEATURE_LEVEL featureLevel;
	for (auto level : levels) {
		if (D3D12CreateDevice (tmpAdapter, level, IID_PPV_ARGS (&_dev)) == S_OK) {
			featureLevel = level;
			break;
		}
	}

	if (0 == _dev) {
		printf_s ("ERROR : D3D12CreateDevice() - FAILED\n");
		return false;
	}

	//
	// step 3 : Create GraphicsCommandList & CommandAllocator & CommandQueue & Swapchain
	//

	hr = _dev->CreateCommandAllocator (D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS (&_cmdAllocator));
	if (FAILED (hr)) {
		printf_s ("ERROR : CreateAllocator() - FAILED\n");
		return false;
	}

	hr = _dev->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS (&_cmdList));
	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommandList() - FAILED\n");
		return false;
	}

	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hr = _dev->CreateCommandQueue (&cmdQueueDesc, IID_PPV_ARGS (&_cmdQueue));
	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommandQueue() - FAILED\n");
		return false;
	}

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
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	hr = _dxgiFactory->CreateSwapChainForHwnd (_cmdQueue,
											   hwnd,
											   &swapChainDesc,
											   nullptr,
											   nullptr,
											   (IDXGISwapChain1**)&_swapChain);

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateSwapChain() - FAILED\n");
		return false;
	}

	//
	// Step 4 : Create DescriptorHeap & Descriptor & View
	//

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;		// render target view
	heapDesc.NodeMask = 0;								// when 2 gpu...
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	hr = _dev->CreateDescriptorHeap (&heapDesc, IID_PPV_ARGS (&_rtvHeaps));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateDescriptorHeap() - FAILED\n");
		return -1;
	}

	// Connect swapchain to render target view

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	_swapChain->GetDesc (&swcDesc);
	_backBuffers = std::vector<ID3D12Resource*> (swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = _rtvHeaps->GetCPUDescriptorHandleForHeapStart ();
	for (int i = 0; i < swcDesc.BufferCount; ++i) {
		hr = _swapChain->GetBuffer (i, IID_PPV_ARGS (&_backBuffers[i]));

		if (FAILED (hr)) {
			printf_s ("ERROR : GetBuffer(%d) - FAILED\n", i);
			continue;
		}

		_dev->CreateRenderTargetView (_backBuffers[i], nullptr, handle);
		handle.ptr += _dev->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}

	if (FAILED (hr)) {
		printf_s ("ERROR : GetBuffer() - FAILED\n");
		return -1;
	}

	//
	// Step 5 : ErrorŒê
	//

	hr = _dev->CreateFence (_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (&_fence));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateFence() - FAILED\n");
		return false;
	}

	_barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	_barrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
	_barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;

	return true;
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