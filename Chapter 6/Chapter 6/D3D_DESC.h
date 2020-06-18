#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>

struct D3D_DESC {
	WNDCLASSEX WNDClass;
	ID3D12Device* Device;
	IDXGISwapChain4* SwapChain;
	ID3D12DescriptorHeap* RTVHeap;
	ID3D12CommandAllocator* CMDAllocator;
	ID3D12GraphicsCommandList* CMDList;
	ID3D12CommandQueue* CMDQueue;
	ID3D12Fence* Fence;
	UINT64 FenceVal;
	D3D12_RESOURCE_BARRIER BarrierDesc;
	std::vector<ID3D12Resource*> BackBuffers;
};