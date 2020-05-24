#include <Windows.h>
#include <tchar.h>
#include <vector>

//
// dx header
//
//
// application -> dx12 -> dxgi -> hardware
// application -> dxgi -> hardware
//
#include <d3d12.h>		
#include <dxgi1_6.h>	// The API of graphics board.

//
// Link to library
//
#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "winmm.lib")

#ifdef _DEBUG
#include<iostream>
#endif

void DebugOutputFormatString (const char* format, ...) {
#ifdef _DEBUG
	va_list valist;
	va_start (valist, format);
	printf_s (format, valist);
	va_end (valist);
#endif
}


LRESULT WindowProcedure (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_DESTROY:
			PostQuitMessage (0);
			break;
		case WM_KEYDOWN:
			if (wParam == VK_ESCAPE)
				DestroyWindow (hwnd);
	}
	return DefWindowProc (hwnd, msg, wParam, lParam);
}

void EnableDebugLayer () {
	ID3D12Debug* debugLayer = 0;
	if (SUCCEEDED (D3D12GetDebugInterface (IID_PPV_ARGS (&debugLayer)))) {
		debugLayer->EnableDebugLayer ();
		debugLayer->Release ();
	}
}

bool _tearingSupport = false;

//
// Check tearing feature.
//
void CheckTearingSupport () {
#ifndef PIXSUPPORT
	IDXGIFactory6* factory;
	HRESULT hr = CreateDXGIFactory1 (IID_PPV_ARGS (&factory));
	BOOL allowTearing = false;
	if (SUCCEEDED (hr)) {
		hr = factory->CheckFeatureSupport (DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof (allowTearing));
	}

	_tearingSupport = SUCCEEDED (hr) && allowTearing;
#else
	m_tearingSupport = TRUE;
#endif
}

//
// dx用
//
ID3D12Device* _dev = 0;
IDXGIFactory6* _dxgiFactory = 0;
ID3D12CommandAllocator* _cmdAllocator = 0;
ID3D12GraphicsCommandList* _cmdList = 0;
ID3D12CommandQueue* _cmdQueue = 0;
IDXGISwapChain4* _swapChain = 0;

//
// window size
//
const unsigned int window_width = 1280;
const unsigned int window_height = 720;

#ifdef _DEBUG
int main () {
	printf_s ("Debug Mode\n");
#else
int WINMAIN (HINSTANCE, HINSTANCE, LPSTR, int) {
#endif
	CheckTearingSupport ();
	if (_tearingSupport) {
		printf ("Tearing Support\n");
	} else {
		printf ("Tearing Not Support\n");
	}

	//
	// Create a window.
	//
	WNDCLASSEX w = {};

	w.cbSize = sizeof (WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;
	w.lpszClassName = _T ("DX12Sample");
	w.hInstance = GetModuleHandle (nullptr);		// get handle

	// register
	RegisterClassEx (&w);

	RECT wrc = {0, 0, window_width, window_height};

	AdjustWindowRect (&wrc, WS_OVERLAPPEDWINDOW, false);

	// create
	HWND hwnd = CreateWindow (w.lpszClassName,
							  _T ("DX12テスト"),
							  WS_OVERLAPPEDWINDOW,
							  CW_USEDEFAULT,
							  CW_USEDEFAULT,
							  wrc.right - wrc.left,
							  wrc.bottom - wrc.top,
							  nullptr,
							  nullptr,
							  w.hInstance,
							  nullptr);

#ifdef _DEBUG
//デバッグレイヤーをオンに
	EnableDebugLayer ();
#endif

	//
	// Initialize dx.
	//

	HRESULT hr = 0;

	// Create DXGIFactory

#ifdef _DEBUG
	hr = CreateDXGIFactory2 (DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS (&_dxgiFactory));
#else
	hr = CreateDXGIFactory1 (IID_PPV_ARGS (&_dxgiFactory));
#endif

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateDXGIFactory() - FAILED\n");
		return -1;
	}

	// Get adpters.

	std::vector<IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = 0;
	for (int i = 0; _dxgiFactory->EnumAdapters (i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
		adapters.push_back (tmpAdapter);
	}
	tmpAdapter = 0;		// reset
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

	// Find the available feature level and create device.

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

	// Can't create device.
	if (0 == _dev) {
		printf_s ("ERROR : D3D12CreateDevice() - FAILED\n");
		return -1;
	}

	// Create CommandAllocator

	/* CommandAllocator
		CommandList memory.
	*/
	hr = _dev->CreateCommandAllocator (D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS (&_cmdAllocator));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateAllocator() - FAILED\n");
		return -1;
	}

	// Create GraphicsCommandList

	/* GraphicsCommandList
		GPUに命令するためのメソッドを持つインターフェイス
		DrawInstanced ()
		DrawIndexedInstanced ()
		ClearRenderTargetView ()
		Close ()
	*/
	hr = _dev->CreateCommandList (0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, 0, IID_PPV_ARGS (&_cmdList));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommandList() - FAILED\n");
		return -1;
	}

	// Create CommandQueue

	/* CommandQueue
		GPUの命令Queue
	*/
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	hr = _dev->CreateCommandQueue (&cmdQueueDesc, IID_PPV_ARGS (&_cmdQueue));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommandQueue() - FAILED\n");
		return -1;
	}

	// Create SwapChain

	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

	swapChainDesc.Width = window_width;
	swapChainDesc.Height = window_height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.Stereo = false;
	swapChainDesc.SampleDesc.Count = 1;
	swapChainDesc.SampleDesc.Quality = 0;
	swapChainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapChainDesc.BufferCount = 2;
	swapChainDesc.Scaling = DXGI_SCALING_STRETCH;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING;

	hr = _dxgiFactory->CreateSwapChainForHwnd (_cmdQueue,
											   hwnd,
											   &swapChainDesc,
											   nullptr,
											   nullptr,
											   (IDXGISwapChain1**)&_swapChain);

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateSwapChain() - FAILED\n");
		return -1;
	}

	// Create DescriptorHeap

	/*
		用於保存各種view
	*/

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;		// render target view
	heapDesc.NodeMask = 0;								// when 2 gpu...
	heapDesc.NumDescriptors = 2;
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ID3D12DescriptorHeap* rtvHeaps = nullptr;

	hr = _dev->CreateDescriptorHeap (&heapDesc, IID_PPV_ARGS (&rtvHeaps));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateDescriptorHeap() - FAILED\n");
		return -1;
	}

	// Connect swapchain to render target view

	/*
		將swapChina中的buffer與RenderTargetView做關連
		然後可以在DirectX pipeline中使用.
	*/

	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	_swapChain->GetDesc (&swcDesc);
	std::vector<ID3D12Resource*> _backBuffers (swcDesc.BufferCount);
	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart ();
	for (int i = 0; i < swcDesc.BufferCount; i++) {
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
	// Create fence.
	//

	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	hr = _dev->CreateFence (_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS (&_fence));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateFence() - FAILED\n");
		return -1;
	}

	//
	// Show the window.
	//
	ShowWindow (hwnd, SW_SHOW);
	UpdateWindow (hwnd);

	//
	// MSG loop
	//
	MSG msg;
	ZeroMemory (&msg, sizeof (MSG));

	static float lastTime = (float)timeGetTime ();
	static DWORD frameCnt = 0;
	static float timeElapsed = 0.0f;
	static float fps = 0.0f;

	while (msg.message != WM_QUIT) {
		if (PeekMessage (&msg, 0, 0, 0, PM_REMOVE)) {
			TranslateMessage (&msg);
			DispatchMessage (&msg);
		} else {
			float currTime = (float)timeGetTime ();
			float deltaTime = (currTime - lastTime) * 0.001f;

			// fps
			frameCnt++;
			timeElapsed += deltaTime;
			if (timeElapsed >= 1.0f) {
				fps = (float)frameCnt / timeElapsed;

				printf_s ("FPS : %.1f\n", fps);

				timeElapsed = 0.0f;
				frameCnt = 0;
			}

			// DirectX Logic

			auto bbIdx = _swapChain->GetCurrentBackBufferIndex ();

			D3D12_RESOURCE_BARRIER BarrierDesc = {};
			BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
			BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
			BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			_cmdList->ResourceBarrier (1, &BarrierDesc);

			auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart ();
			rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
			_cmdList->OMSetRenderTargets (1, &rtvH, false, nullptr);

			float clearColor[] = {1.0f, 1.0f, 0.0f, 1.0f};	// yellow
			_cmdList->ClearRenderTargetView (rtvH, clearColor, 0, nullptr);

			BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			_cmdList->ResourceBarrier (1, &BarrierDesc);

			_cmdList->Close ();

			ID3D12CommandList* cmdLists[] = {_cmdList};

			_cmdQueue->ExecuteCommandLists (1, cmdLists);

			// check
			_cmdQueue->Signal (_fence, ++_fenceVal);
			if (_fence->GetCompletedValue () != _fenceVal) {
				auto event = CreateEvent (0, false, false, 0);
				_fence->SetEventOnCompletion (_fenceVal, event);
				WaitForSingleObject (event, INFINITE);
				CloseHandle (event);
			}

			// clear command queue
			_cmdAllocator->Reset ();
			_cmdList->Reset (_cmdAllocator, 0);

			// 第一參數
			// 1->垂直同步
			// 0->即時執行
			UINT presentFlags = _tearingSupport ? DXGI_PRESENT_ALLOW_TEARING : 0;
			_swapChain->Present (0, presentFlags);

			lastTime = currTime;
		}
	}

	UnregisterClass (w.lpszClassName, w.hInstance);

	return 0;
}