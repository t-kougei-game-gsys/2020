#pragma once

#include <string>
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
#include <dxgi1_6.h>	// The API of graphics board. The other one dxgi1_5.h etc..

//
// Link to library
//
#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "dxgi.lib")
#pragma comment (lib, "winmm.lib")

using namespace std;

namespace D3D {
	bool InitD3D (HINSTANCE hInstance, int width, int height);

	LRESULT CALLBACK WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

	int EnterMsgLoop (bool (*ptr_display)(float deltaTime));
}

extern ID3D12Device* _dev;
extern IDXGIFactory6* _dxgiFactory;
extern ID3D12CommandAllocator* _cmdAllocator;
extern ID3D12GraphicsCommandList* _cmdList;
extern ID3D12CommandQueue* _cmdQueue;
extern IDXGISwapChain4* _swapChain;
extern ID3D12DescriptorHeap* _rtvHeaps;
extern ID3D12Fence* _fence;
extern UINT64 _fenceVal;
extern D3D12_RESOURCE_BARRIER _barrierDesc;

extern std::vector<ID3D12Resource*> _backBuffers;

extern WNDCLASSEX _w;