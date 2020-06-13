#include "D3DUtility.h"

#include <DirectXMath.h>	
#include <d3dcompiler.h>

#pragma comment (lib, "d3dcompiler.lib")

using namespace D3D;
using namespace DirectX;

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

D3D_DESC _d3dDesc = {};

ID3D12PipelineState* _pipelineState = nullptr;
ID3D12RootSignature* _rootSignature = nullptr;

D3D12_VERTEX_BUFFER_VIEW _vbView = {};
D3D12_VERTEX_BUFFER_VIEW _vbView2 = {};
D3D12_VERTEX_BUFFER_VIEW _vbView3 = {};
D3D12_VERTEX_BUFFER_VIEW _vbView4 = {};

D3D12_INDEX_BUFFER_VIEW _ibView = {};
D3D12_INDEX_BUFFER_VIEW _ibView2 = {};

D3D12_VIEWPORT _viewport = {};
D3D12_RECT _scissorRect = {};

void EnableDebugLayer () {
	ID3D12Debug* debugLayer = 0;
	if (SUCCEEDED (D3D12GetDebugInterface (IID_PPV_ARGS (&debugLayer)))) {
		debugLayer->EnableDebugLayer ();
		debugLayer->Release ();
	}
}

int _currentTriangleIndex = 0;

bool Display (float deltaTime) {
	int bbIdx = _d3dDesc.SwapChain->GetCurrentBackBufferIndex ();

	auto rtvH = _d3dDesc.RTVHeap->GetCPUDescriptorHandleForHeapStart ();
	rtvH.ptr += bbIdx * _d3dDesc.Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_d3dDesc.BarrierDesc.Transition.pResource = _d3dDesc.BackBuffers[bbIdx];
	_d3dDesc.BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	_d3dDesc.BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_d3dDesc.CMDList->ResourceBarrier (1, &_d3dDesc.BarrierDesc);

	_d3dDesc.CMDList->OMSetRenderTargets (1, &rtvH, false, nullptr);

	_d3dDesc.CMDList->ClearRenderTargetView (rtvH, BLACK, 0, nullptr);

	_d3dDesc.CMDList->SetPipelineState (_pipelineState);
	_d3dDesc.CMDList->SetGraphicsRootSignature (_rootSignature);
	_d3dDesc.CMDList->RSSetViewports (1, &_viewport);
	_d3dDesc.CMDList->RSSetScissorRects (1, &_scissorRect);

	switch (_currentTriangleIndex % 4) {
		case 0:
			_d3dDesc.CMDList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_d3dDesc.CMDList->IASetVertexBuffers (0, 1, &_vbView);
			_d3dDesc.CMDList->DrawInstanced (3, 1, 0, 0);
			break;
		case 1:
			_d3dDesc.CMDList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_d3dDesc.CMDList->IASetVertexBuffers (0, 1, &_vbView2);
			_d3dDesc.CMDList->DrawInstanced (3, 1, 0, 0);
			break;
		case 2:
			_d3dDesc.CMDList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_d3dDesc.CMDList->IASetIndexBuffer (&_ibView);
			_d3dDesc.CMDList->IASetVertexBuffers (0, 1, &_vbView3);
			_d3dDesc.CMDList->DrawIndexedInstanced (6, 1, 0, 0, 0);
			break;
		case 3:
			_d3dDesc.CMDList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_d3dDesc.CMDList->IASetIndexBuffer (&_ibView2);
			_d3dDesc.CMDList->IASetVertexBuffers (0, 1, &_vbView4);
			_d3dDesc.CMDList->DrawIndexedInstanced (12, 1, 0, 0, 0);
			break;
	}

	_d3dDesc.BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_d3dDesc.BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	_d3dDesc.CMDList->ResourceBarrier (1, &_d3dDesc.BarrierDesc);

	_d3dDesc.CMDList->Close ();

	ID3D12CommandList* cmdLists[] = {_d3dDesc.CMDList};
	_d3dDesc.CMDQueue->ExecuteCommandLists (1, cmdLists);

	_d3dDesc.CMDQueue->Signal (_d3dDesc.Fence, ++_d3dDesc.FenceVal);
	if (_d3dDesc.Fence->GetCompletedValue () != _d3dDesc.FenceVal) {
		auto event = CreateEvent (nullptr, false, false, nullptr);
		_d3dDesc.Fence->SetEventOnCompletion (_d3dDesc.FenceVal, event);
		WaitForSingleObject (event, INFINITE);
		CloseHandle (event);
	}

	_d3dDesc.CMDAllocator->Reset ();
	_d3dDesc.CMDList->Reset (_d3dDesc.CMDAllocator, nullptr);

	_d3dDesc.SwapChain->Present (1, 0);

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
			else if (wParam == VK_RIGHT)
				_currentTriangleIndex++;
			else if (wParam == VK_LEFT)
				_currentTriangleIndex = (_currentTriangleIndex + 3) % 4;
	}

	return DefWindowProc (hwnd, msg, wParam, lParam);
}

/* Create Vertex
	1. 始めに
		1.1 import DirectXMath (昔のD3DX Library)
		1.2 using DirectX namespace (XMFLOAT3)
		1.3 XMFLOAT3を作成する
	2. 頂点bufferを作成する
		2.1 ID3D12Resourceを作るために設定
			2.1.1 HEAP_PROPERTIES
			2.1.2 RESOURCE_DESC
		2.2 buffer作成
	3. 頂点データをバッファーにマップする (DX 9のLock)
	4. 頂点バッファービューの作成
	5. Indexの作成 (必須ではない)
*/
bool GenerateTriangle () {
	HRESULT hr = 0;

	//
	// 1. 始めに
	//

	// 1.3 XMFLOAT3を作成する

	XMFLOAT3 vertices[] = {
		{-1.0f, -1.0f, 0.0f},
		{-1.0f,  1.0f, 0.0f},
		{ 1.0f, -1.0f, 0.0f}
	};

	//
	// 2. 頂点buffer
	//

	// 2.1 ID3D12Resourceを作るために設定
	// 2.1.1 HEAP_PROPERTIES

	D3D12_HEAP_PROPERTIES heapProp = {};

	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// 2.1.2 RESOURCE_DESC
	
	D3D12_RESOURCE_DESC resDesc = {};

	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.Width = sizeof (vertices);
	resDesc.Height = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.MipLevels = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.SampleDesc.Count = 1;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// 2.2 buffer作成

	ID3D12Resource* vertBuff = nullptr;
	hr = _d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&vertBuff)
	);

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommittedResource() - FAILED\n");
		return false;
	}

	//
	// 3. 頂点データをバッファーにマップする (DX 9のLock)
	//

	XMFLOAT3* vertMap = nullptr;

	hr = vertBuff->Map (0, nullptr, (void**)&vertMap);
	std::copy (std::begin (vertices), std::end (vertices), vertMap);
	vertBuff->Unmap (0, nullptr);

	// これで、頂点データをバッファーにマップした
	// VertexBufferViewを作成して、どこのバッファーは頂点を表すのかをGPUに教えます。

	//
	// 4. 頂点バッファービューの作成
	//

	_vbView.BufferLocation = vertBuff->GetGPUVirtualAddress ();
	_vbView.SizeInBytes = sizeof (vertices);
	_vbView.StrideInBytes = sizeof (vertices[0]);

	// 以上は一つの頂点データ作成のDEMO

	//
	// ほかの頂点データの作成
	//

	//
	// 三角形
	//

	XMFLOAT3 vertices2[] = {
		{-0.5f, -0.7f, 0.0f},
		{ 0.0f,  0.7f, 0.0f},
		{ 0.5f, -0.7f, 0.0f}
	};

	resDesc.Width = sizeof (vertices2);

	vertBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&vertBuff)
	);

	vertMap = nullptr;

	vertBuff->Map (0, nullptr, (void**)&vertMap);
	std::copy (std::begin (vertices2), std::end (vertices2), vertMap);
	vertBuff->Unmap (0, nullptr);

	_vbView2.BufferLocation = vertBuff->GetGPUVirtualAddress ();
	_vbView2.SizeInBytes = sizeof (vertices2);
	_vbView2.StrideInBytes = sizeof (vertices2[0]);

	//
	// indexを使用する四角形
	//

	XMFLOAT3 vertices3[] = {
		{-0.4f, -0.7f, 0.0f},		// index : 0
		{-0.4f,  0.7f, 0.0f},		// index : 1
		{ 0.4f, -0.7f, 0.0f},		// index : 2
		{ 0.4f,  0.7f, 0.0f}		// index : 3
	};

	resDesc.Width = sizeof (vertices3);

	vertBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&vertBuff)
	);

	vertMap = nullptr;

	vertBuff->Map (0, nullptr, (void**)&vertMap);
	std::copy (std::begin (vertices3), std::end (vertices3), vertMap);
	vertBuff->Unmap (0, nullptr);

	_vbView3.BufferLocation = vertBuff->GetGPUVirtualAddress ();
	_vbView3.SizeInBytes = sizeof (vertices3);
	_vbView3.StrideInBytes = sizeof (vertices3[0]);

	// indexの作成

	unsigned short indices[] {
		0, 1, 2,
		2, 1, 3
	};

	ID3D12Resource* idxBuff = nullptr;

	resDesc.Width = sizeof (indices);

	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&idxBuff)
	);

	unsigned short* mappedIdx = nullptr;

	idxBuff->Map (0, nullptr, (void**)&mappedIdx);
	std::copy (std::begin (indices), std::end (indices), mappedIdx);
	idxBuff->Unmap (0, nullptr);

	_ibView.BufferLocation = idxBuff->GetGPUVirtualAddress ();
	_ibView.SizeInBytes = sizeof (indices);
	_ibView.Format = DXGI_FORMAT_R16_UINT;

	//
	// indexを使用する二つの四角形
	//

	XMFLOAT3 vertices4[] = {
		// left
		{-0.9f, -0.7f, 0.0f},
		{-0.9f,  0.7f, 0.0f},
		{-0.1f,  0.7f, 0.0f},
		{-0.1f, -0.7f, 0.0f},
		// right
		{ 0.1f, -0.7f, 0.0f},
		{ 0.1f,  0.7f, 0.0f},
		{ 0.9f,  0.7f, 0.0f},
		{ 0.9f, -0.7f, 0.0f}
	};

	resDesc.Width = sizeof (vertices4);

	vertBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&vertBuff)
	);

	vertMap = nullptr;

	vertBuff->Map (0, nullptr, (void**)&vertMap);
	std::copy (std::begin (vertices4), std::end (vertices4), vertMap);
	vertBuff->Unmap (0, nullptr);

	_vbView4.BufferLocation = vertBuff->GetGPUVirtualAddress ();
	_vbView4.SizeInBytes = sizeof (vertices4);
	_vbView4.StrideInBytes = sizeof (vertices4[0]);

	// indexの作成

	unsigned short indices2[] = {
		0, 1, 2,
		2, 0, 3,
		4, 5, 6,
		6, 4, 7
	};

	resDesc.Width = sizeof (indices2);

	idxBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&idxBuff)
	);

	mappedIdx = nullptr;
	idxBuff->Map (0, nullptr, (void**)&mappedIdx);
	std::copy (std::begin (indices2), std::end (indices2), mappedIdx);
	idxBuff->Unmap (0, nullptr);

	_ibView2.BufferLocation = idxBuff->GetGPUVirtualAddress ();
	_ibView2.SizeInBytes = sizeof (indices2);
	_ibView2.Format = DXGI_FORMAT_R16_UINT;

	return true;
}

/* GPU Setting
	1. 始めに
		1.1 import d3dcompiler.h
		1.2 link d3dcompiler.lib
	2. Shaderの作成
		2.0 errorのID3DBlobの作成
		2.1 Vertex Shader
		2.2 Pixel Shader
	3. 頂点のレイアウト
	4. Pipeline stateの作成
		4.1 Pipeline state descを作成する
		4.1.1 RootSignatureの作成
		4.1.2 Pipeline state descの本体
		4.2 Pipeline stateの作成
	5. Viewport & Scissorの設定
		5.1 Viewportの設定
		5.2 Scissorの設定
*/
bool GPUSetting () {
	HRESULT hr = 0;

	//
	// 2. Shaderの作成
	//

	// 2.0 errorのID3DBlobの作成

	ID3DBlob* errorBlob = nullptr;

	// 2.1 Vertex Shader

	ID3DBlob* vsBlob = nullptr;
	hr = D3DCompileFromFile (
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vsBlob, &errorBlob);

	if (FAILED (hr)) {
		if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND)) {
			printf_s ("ERROR : D3DCompileFromFile(Vertex Shader) - FAILED, Can't find file.\n");
			return false;
		} else {
			string errstr;
			errstr.resize (errorBlob->GetBufferSize ());

			copy_n ((char*)errorBlob->GetBufferPointer (),
					errorBlob->GetBufferSize (),
					errstr.begin ());

			errstr += "\n";

			printf_s ("%s", errstr.c_str ());
		}
	}

	// 2.2 Pixel Shader

	ID3DBlob* psBlob = nullptr;
	hr = D3DCompileFromFile (
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&psBlob, &errorBlob);

	if (FAILED (hr)) {
		if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND)) {
			printf_s ("ERROR : D3DCompileFromFile(Pixel Shader) - FAILED, Can't find file.\n");
			return false;
		} else {
			string errstr;
			errstr.resize (errorBlob->GetBufferSize ());

			copy_n ((char*)errorBlob->GetBufferPointer (),
					errorBlob->GetBufferSize (),
					errstr.begin ());

			errstr += "\n";

			printf_s ("%s", errstr.c_str ());
		}
	}

	//
	// 3. 頂点のレイアウト
	//

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION",
			0, 
			DXGI_FORMAT_R32G32B32_FLOAT, 
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 
			0
		},
	};

	//
	// 4. Pipeline stateの作成
	//

	// 4.1 Pipeline state descを作成する

	// 4.1.1 RootSignatureの作成

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* rootSigBlob = nullptr;
	hr = D3D12SerializeRootSignature (
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob
	);

	if (FAILED (hr)) {
		if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND)) {
			printf_s ("ERROR : D3D12SerializeRootSignature() - FAILED\n");
			return false;
		} else {
			string errstr;
			errstr.resize (errorBlob->GetBufferSize ());

			copy_n ((char*)errorBlob->GetBufferPointer (),
					errorBlob->GetBufferSize (),
					errstr.begin ());

			errstr += "\n";

			printf_s ("%s", errstr.c_str ());
		}
	}

	hr = _d3dDesc.Device->CreateRootSignature (
		0,
		rootSigBlob->GetBufferPointer (),
		rootSigBlob->GetBufferSize (),
		IID_PPV_ARGS(&_rootSignature)
	);

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateRootSignature() - FAILED");
		return false;
	}

	rootSigBlob->Release ();

	// 4.1.2 Pipeline state descの本体

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
	pipelineDesc.pRootSignature = _rootSignature;
	pipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer ();
	pipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize ();
	pipelineDesc.PS.pShaderBytecode = psBlob->GetBufferPointer ();
	pipelineDesc.PS.BytecodeLength = psBlob->GetBufferSize ();
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pipelineDesc.RasterizerState.MultisampleEnable = false;
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	//pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
	pipelineDesc.RasterizerState.DepthClipEnable = true;
	pipelineDesc.BlendState.AlphaToCoverageEnable = false;
	pipelineDesc.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};
	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	pipelineDesc.BlendState.RenderTarget[0] = renderTargetBlendDesc;
	pipelineDesc.InputLayout.pInputElementDescs = inputLayout;
	pipelineDesc.InputLayout.NumElements = _countof (inputLayout);
	pipelineDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	pipelineDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	pipelineDesc.NumRenderTargets = 1;
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleDesc.Quality = 0;
	
	// 4.2 Pipeline stateの作成

	hr = _d3dDesc.Device->CreateGraphicsPipelineState (&pipelineDesc, IID_PPV_ARGS (&_pipelineState));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateGraphicsPipelineState() - FAILED\n");
		return false;
	}

	//
	// 	5. Viewport & Scissorの設定
	//

	// 5.1 Viewportの設定

	_viewport.Width = window_width;
	_viewport.Height = window_height;
	_viewport.TopLeftX = 0;
	_viewport.TopLeftY = 0;
	_viewport.MaxDepth = 1.0f;
	_viewport.MinDepth = 0.0f;

	// 5.2 Scissorの設定

	_scissorRect.top = 0;
	_scissorRect.left = 0;
	_scissorRect.right = _scissorRect.left + window_width;
	_scissorRect.bottom = _scissorRect.top + window_height;

	return true;
}

bool Setup () {
	if (!GenerateTriangle ()) {
		printf_s ("ERROR : GenerateTriangle() - FAILED\n");
		return false;
	}

	if (!GPUSetting ()) {
		printf_s ("ERROR : ShaderSetting() - FAILED\n");
		return false;
	}

	return true;
}

int main () {
	EnableDebugLayer ();

	if (!InitD3D (GetModuleHandle (nullptr), window_width, window_height, &_d3dDesc)) {
		printf_s ("ERROR : InitD3D() - FAILED\n");
		return -1;
	}

	if (!Setup ()) {
		printf_s ("ERROR : Setup() - FAILED\n");
		return -1;
	}

	D3D::EnterMsgLoop (Display);

	UnregisterClass (_d3dDesc.WNDClass.lpszClassName, _d3dDesc.WNDClass.hInstance);

	printf_s ("Application end.");

	return 0;
}