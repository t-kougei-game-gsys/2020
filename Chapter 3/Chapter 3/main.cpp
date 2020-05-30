#include <Windows.h>
#include <time.h>

#include "D3DUtility.h"

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

//
// 將DX的錯誤消息顯示到"輸出窗口"
//
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
	_tearingSupport = TRUE;
#endif
}

bool Setup () {

	return true;
}

bool Display (float deltaTime) {
	//
	// Step 6 : Render to rtv.
	//

	// current back buffer.
	auto bbIdx = _swapChain->GetCurrentBackBufferIndex ();

	_barrierDesc.Transition.pResource = _backBuffers[bbIdx];
	_barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	_barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_cmdList->ResourceBarrier (1, &_barrierDesc);

	auto rtvH = _rtvHeaps->GetCPUDescriptorHandleForHeapStart ();
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	/*
		OMSetRenderTargets (UINT							   numRTVDescriptors, 
							const D3D12_CPU_DESCRIPTOR_HANDLE* pRTVHandles,
							BOOL							   RTsSingleHandleToDescriptorRange,
							const D3D12_CPU_DESCRIPTOR_HANDLE* pDepthStencilDescriptor);
		numRTVDescriptors				 : Count of RTVs
		pRTVHandles						 : rtv handles head address.
		RTsSingleHandleToDescriptorRange : 
		pDepthStencilDescriptor			 : Stencil Descriptor
	*/
	_cmdList->OMSetRenderTargets (1, &rtvH, false, nullptr);

	static float duration = 1.0f;
	static float time = 0.0f;

	static float r = (double)rand () / (RAND_MAX + 1);
	static float g = (double)rand () / (RAND_MAX + 1);
	static float b = (double)rand () / (RAND_MAX + 1);

	time += deltaTime;
	if (time >= duration) {
		r = (double)rand () / (RAND_MAX + 1);
		g = (double)rand () / (RAND_MAX + 1);
		b = (double)rand () / (RAND_MAX + 1);

		time = 0.0f;
	}

	float clearColor[] = {r, g, b, 1.0f};
	_cmdList->ClearRenderTargetView (rtvH, clearColor, 0, nullptr);

	_barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	_cmdList->ResourceBarrier (1, &_barrierDesc);

	_cmdList->Close ();

	ID3D12CommandList* cmdLists[] = {_cmdList};
	_cmdQueue->ExecuteCommandLists (1, cmdLists);

	_cmdQueue->Signal (_fence, ++_fenceVal);
	// when gpu completed and return same value, break.
	// while (_fence->GetCompletedValue () != _fenceVal) {}
	if (_fence->GetCompletedValue () != _fenceVal) {
		auto event = CreateEvent (nullptr, false, false, nullptr);
		_fence->SetEventOnCompletion (_fenceVal, event);
		// wait for event
		WaitForSingleObject (event, INFINITE);
		CloseHandle (event);
	}

	_cmdAllocator->Reset ();
	_cmdList->Reset (_cmdAllocator, nullptr);

	UINT presentFlags = _tearingSupport ? DXGI_PRESENT_ALLOW_TEARING : 0;
	_swapChain->Present (0, presentFlags);

	return true;
}

LRESULT CALLBACK D3D::WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
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

//
// window size
//
const unsigned int window_width = 1280;
const unsigned int window_height = 720;

#ifdef _DEBUG
int main () {
	printf_s ("Debug Mode\n");
	EnableDebugLayer ();
#else
int WINMAIN (HINSTANCE, HINSTANCE, LPSTR, int) {
#endif

	srand ((unsigned)time (NULL));

	CheckTearingSupport ();
	if (_tearingSupport) {
		printf ("Tearing Support\n");
	} else {
		printf ("Tearing Not Support\n");
	}

	if (!D3D::InitD3D (GetModuleHandle (nullptr), window_width, window_height)) {
		printf_s ("ERROR : InitD3D() - FAILED\n");
		return -1;
	}

	D3D::EnterMsgLoop (Display);

	UnregisterClass (_w.lpszClassName, _w.hInstance);

	printf_s ("Run End\n");

	return 0;
}