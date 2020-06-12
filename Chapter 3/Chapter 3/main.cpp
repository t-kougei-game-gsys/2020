#include "D3DUtility.h"

#include <iostream>
#include <time.h>

using namespace D3D;

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

D3D_DESC _d3dDesc = {};

bool Display (float deltaTime) {
	//
	// step 7 : Render to rtv.
	//

	// step 7.1 : Get current back buffer index

	int bbIdx = _d3dDesc.SwapChain->GetCurrentBackBufferIndex ();

	// step 7.2 : Set current back buffer handle

	auto rtvH = _d3dDesc.RTVHeap->GetCPUDescriptorHandleForHeapStart ();
	rtvH.ptr += bbIdx * _d3dDesc.Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	// step 7.3 : Set barrier (resource : render target)

	_d3dDesc.BarrierDesc.Transition.pResource = _d3dDesc.BackBuffers[bbIdx];
	_d3dDesc.BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	_d3dDesc.BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_d3dDesc.CMDList->ResourceBarrier (1, &_d3dDesc.BarrierDesc);

	// step 7.4 : Create "SetRenderTarget" command

	_d3dDesc.CMDList->OMSetRenderTargets (1, &rtvH, false, nullptr);

	// step 7.5 : Create "ClearRenderTargetView" command

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
	_d3dDesc.CMDList->ClearRenderTargetView (rtvH, clearColor, 0, nullptr);

	// step 7.6 : Set barrier (render target -> present)

	_d3dDesc.BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_d3dDesc.BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	_d3dDesc.CMDList->ResourceBarrier (1, &_d3dDesc.BarrierDesc);

	// step 7.7 : Create "Close" command

	_d3dDesc.CMDList->Close ();

	// step 7.8 : Execute commands

	ID3D12CommandList* cmdLists[] = {_d3dDesc.CMDList};
	_d3dDesc.CMDQueue->ExecuteCommandLists (1, cmdLists);

	// step 8 : Wait for all commands to complete

	_d3dDesc.CMDQueue->Signal (_d3dDesc.Fence, ++_d3dDesc.FenceVal);
	// when gpu completed and return same value, break.
	// while (_fence->GetCompletedValue () != _fenceVal) {}
	if (_d3dDesc.Fence->GetCompletedValue () != _d3dDesc.FenceVal) {
		auto event = CreateEvent (nullptr, false, false, nullptr);
		_d3dDesc.Fence->SetEventOnCompletion (_d3dDesc.FenceVal, event);
		// wait for event
		WaitForSingleObject (event, INFINITE);
		CloseHandle (event);
	}

	//
	// step 9 : Reset & Present
	//

	_d3dDesc.CMDAllocator->Reset ();
	_d3dDesc.CMDList->Reset (_d3dDesc.CMDAllocator, nullptr);

	_d3dDesc.SwapChain->Present (1, 0);

	return true;
}

void EnableDebugLayer () {
	ID3D12Debug* debugLayer = 0;
	if (SUCCEEDED (D3D12GetDebugInterface (IID_PPV_ARGS (&debugLayer)))) {
		debugLayer->EnableDebugLayer ();
		debugLayer->Release ();
	}
}

int main () {
	EnableDebugLayer ();

	srand ((unsigned)time (NULL));

	if (!InitD3D (GetModuleHandle (nullptr), window_width, window_height, &_d3dDesc)) {
		printf_s ("ERROR : InitD3D() - FAILED\n");
		return -1;
	}

	D3D::EnterMsgLoop (Display);

	UnregisterClass (_d3dDesc.WNDClass.lpszClassName, _d3dDesc.WNDClass.hInstance);

	printf_s ("Application end.");

	return 0;
}