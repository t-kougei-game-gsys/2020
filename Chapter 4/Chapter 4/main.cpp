#include <Windows.h>
#include <DirectXMath.h>	

#include "D3DUtility.h"

//
// Compiler as shader.
//
#include <d3dcompiler.h>
#pragma comment (lib, "d3dcompiler.lib")

#ifdef _DEBUG
#include<iostream>
#endif

using namespace DirectX;

//
//
//
D3D12_VERTEX_BUFFER_VIEW _vbView = {};
D3D12_VERTEX_BUFFER_VIEW _vbView2 = {};
D3D12_VERTEX_BUFFER_VIEW _vbView3 = {};
D3D12_VERTEX_BUFFER_VIEW _vbView4 = {};
D3D12_VERTEX_BUFFER_VIEW _vbView5 = {};
D3D12_INDEX_BUFFER_VIEW _ibView = {};
D3D12_INDEX_BUFFER_VIEW _ibView2 = {};
ID3D12RootSignature* _rootSignature = nullptr;
ID3D12PipelineState* _pipelineState = nullptr;
D3D12_VIEWPORT _viewport = {};
D3D12_RECT _scissorrect = {};

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

int _currentTriangleIndex = 0;
void NextTriangle () {
	_currentTriangleIndex++;
	if (_currentTriangleIndex > 5) {
		_currentTriangleIndex = 1;
	}
}

void PrevTriangle () {
	_currentTriangleIndex--;
	if (_currentTriangleIndex < 1) {
		_currentTriangleIndex = 5;
	}
}

bool Display (float deltaTime) {
	auto bbIdx = _swapChain->GetCurrentBackBufferIndex ();

	_barrierDesc.Transition.pResource = _backBuffers[bbIdx];
	_barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	_barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_cmdList->ResourceBarrier (1, &_barrierDesc);

	_cmdList->SetPipelineState (_pipelineState);

	auto rtvH = _rtvHeaps->GetCPUDescriptorHandleForHeapStart ();
	rtvH.ptr += bbIdx * _dev->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	_cmdList->OMSetRenderTargets (1, &rtvH, false, nullptr);

	//float clearColor[] = {1.0f, 1.0f, 1.0f, 1.0f};
	float clearColor[] = {0, 0, 0, 0};
	_cmdList->ClearRenderTargetView (rtvH, clearColor, 0, nullptr);

	_cmdList->RSSetViewports (1, &_viewport);
	_cmdList->RSSetScissorRects (1, &_scissorrect);
	_cmdList->SetGraphicsRootSignature (_rootSignature);
	
	switch (_currentTriangleIndex % 5) {
		case 0:
			_cmdList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_cmdList->IASetVertexBuffers (0, 1, &_vbView);
			_cmdList->DrawInstanced (3, 1, 0, 0);
			break;
		case 1:
			_cmdList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_cmdList->IASetVertexBuffers (0, 1, &_vbView2);
			_cmdList->DrawInstanced (3, 1, 0, 0);
			break;
		case 2:
			_cmdList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
			_cmdList->IASetVertexBuffers (0, 1, &_vbView3);
			_cmdList->DrawInstanced (4, 1, 0, 0);
			break;
		case 3:
			_cmdList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_cmdList->IASetVertexBuffers (0, 1, &_vbView4);
			_cmdList->IASetIndexBuffer (&_ibView);
			// index : 6個
			_cmdList->DrawIndexedInstanced (6, 1, 0, 0, 0);
			break;
		case 4:
			_cmdList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			_cmdList->IASetVertexBuffers (0, 1, &_vbView5);
			_cmdList->IASetIndexBuffer (&_ibView2);
			// index : 12個
			_cmdList->DrawIndexedInstanced (12, 1, 0, 0, 0);
			break;
		default:
			printf_s ("ERROR : CurrentTriangleIndex out of bounds");
			break;
	}

	_barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
	_cmdList->ResourceBarrier (1, &_barrierDesc);

	_cmdList->Close ();

	ID3D12CommandList* cmdLists[] = {_cmdList};
	_cmdQueue->ExecuteCommandLists (1, cmdLists);

	_cmdQueue->Signal (_fence, ++_fenceVal);
	if (_fence->GetCompletedValue () != _fenceVal) {
		auto event = CreateEvent (nullptr, false, false, nullptr);
		_fence->SetEventOnCompletion (_fenceVal, event);
		WaitForSingleObject (event, INFINITE);
		CloseHandle (event);
	}

	_cmdAllocator->Reset ();
	_cmdList->Reset (_cmdAllocator, nullptr);

	_swapChain->Present (1, 0);

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
				//NextTriangle ();
			else if (wParam == VK_LEFT)
				_currentTriangleIndex = (_currentTriangleIndex + 4) % 5;
				//PrevTriangle ();
	}

	return DefWindowProc (hwnd, msg, wParam, lParam);
}

//
// window size
//
const unsigned int window_width = 1280;
const unsigned int window_height = 720;

bool GenerateTriangle () {
	HRESULT hr = 0;

	XMFLOAT3 vertices[] = {
		{-1.0f, -1.0f, 0.0f},
		{-1.0f,  1.0f, 0.0f},
		{ 1.0f, -1.0f, 0.0f}
	};

	XMFLOAT3 vertices2[] = {
		{-0.5f, -0.7f, 0.0f},
		{ 0.0f,  0.7f, 0.0f},
		{ 0.5f, -0.7f, 0.0f}
	};

	XMFLOAT3 vertices3[] = {
		{-0.4f, -0.7f, 0.0f},
		{-0.4f,  0.7f, 0.0f},
		{ 0.4f, -0.7f, 0.0f},
		{ 0.4f,  0.7f, 0.0f}
	};

	XMFLOAT3 vertices4[] = {
		{-0.4f, -0.7f, 0.0f},
		{-0.4f,  0.7f, 0.0f},
		{ 0.4f, -0.7f, 0.0f},
		{ 0.4f,  0.7f, 0.0f}
	};

	XMFLOAT3 vertices5[] = {
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

	unsigned short indices[] = {
		0, 1, 2,
		2, 1, 3
	};

	unsigned short indices2[] = {
		0, 1, 2,
		2, 0, 3,
		4, 5, 6,
		6, 4, 7
	};

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

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

	// gpu memory
	ID3D12Resource* vertBuff = nullptr;
	ID3D12Resource* vertBuff2 = nullptr;
	ID3D12Resource* vertBuff3 = nullptr;
	ID3D12Resource* vertBuff4 = nullptr;
	ID3D12Resource* vertBuff5 = nullptr;

	hr = _dev->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,		// only read
		nullptr,
		IID_PPV_ARGS (&vertBuff)
	);

	resDesc.Width = sizeof (vertices2);
	hr = _dev->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,		// only read
		nullptr,
		IID_PPV_ARGS (&vertBuff2)
	);

	resDesc.Width = sizeof (vertices3);
	hr = _dev->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,		// only read
		nullptr,
		IID_PPV_ARGS (&vertBuff3)
	);

	resDesc.Width = sizeof (vertices4);
	hr = _dev->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,		// only read
		nullptr,
		IID_PPV_ARGS (&vertBuff4)
	);

	resDesc.Width = sizeof (vertices5);
	hr = _dev->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,		// only read
		nullptr,
		IID_PPV_ARGS (&vertBuff5)
	);

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommittedResource() - FAILED\n");
		return false;
	}

	XMFLOAT3* vertMap = nullptr;

	// DX 9 -> IDirect3DVertexBuffer9::Lock ();
	vertBuff->Map (0, nullptr, (void**)&vertMap);
	std::copy (std::begin (vertices), std::end (vertices), vertMap);
	vertBuff->Unmap (0, nullptr);

	vertBuff2->Map (0, nullptr, (void**)&vertMap);
	std::copy (std::begin (vertices2), std::end (vertices2), vertMap);
	vertBuff2->Unmap (0, nullptr);

	vertBuff3->Map (0, nullptr, (void**)&vertMap);
	std::copy (std::begin (vertices3), std::end (vertices3), vertMap);
	vertBuff3->Unmap (0, nullptr);

	vertBuff4->Map (0, nullptr, (void**)&vertMap);
	std::copy (std::begin (vertices4), std::end (vertices4), vertMap);
	vertBuff4->Unmap (0, nullptr);

	vertBuff5->Map (0, nullptr, (void**)&vertMap);
	std::copy (std::begin (vertices5), std::end (vertices5), vertMap);
	vertBuff5->Unmap (0, nullptr);

	_vbView.BufferLocation = vertBuff->GetGPUVirtualAddress ();
	_vbView.SizeInBytes = sizeof (vertices);
	_vbView.StrideInBytes = sizeof (vertices[0]);

	_vbView2.BufferLocation = vertBuff2->GetGPUVirtualAddress ();
	_vbView2.SizeInBytes = sizeof (vertices2);
	_vbView2.StrideInBytes = sizeof (vertices2[0]);

	_vbView3.BufferLocation = vertBuff3->GetGPUVirtualAddress ();
	_vbView3.SizeInBytes = sizeof (vertices3);
	_vbView3.StrideInBytes = sizeof (vertices3[0]);

	_vbView4.BufferLocation = vertBuff4->GetGPUVirtualAddress ();
	_vbView4.SizeInBytes = sizeof (vertices4);
	_vbView4.StrideInBytes = sizeof (vertices4[0]);

	_vbView5.BufferLocation = vertBuff5->GetGPUVirtualAddress ();
	_vbView5.SizeInBytes = sizeof (vertices5);
	_vbView5.StrideInBytes = sizeof (vertices5[0]);

	// index
	ID3D12Resource* idxBuff = nullptr;
	ID3D12Resource* idxBuff2 = nullptr;

	resDesc.Width = sizeof (indices);
	hr = _dev->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,		// only read
		nullptr,
		IID_PPV_ARGS (&idxBuff)
	);

	resDesc.Width = sizeof (indices2);
	hr = _dev->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,		// only read
		nullptr,
		IID_PPV_ARGS (&idxBuff2)
	);

	unsigned short* mappedIdx = nullptr;
	idxBuff->Map (0, nullptr, (void**)&mappedIdx);
	std::copy (std::begin (indices), std::end (indices), mappedIdx);
	idxBuff->Unmap (0, nullptr);

	idxBuff2->Map (0, nullptr, (void**)&mappedIdx);
	std::copy (std::begin (indices2), std::end (indices2), mappedIdx);
	idxBuff2->Unmap (0, nullptr);

	_ibView.BufferLocation = idxBuff->GetGPUVirtualAddress ();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = sizeof (indices);

	_ibView2.BufferLocation = idxBuff2->GetGPUVirtualAddress ();
	_ibView2.Format = DXGI_FORMAT_R16_UINT;
	_ibView2.SizeInBytes = sizeof (indices2);

	return true;
}

bool Setup () {
	HRESULT hr = 0;

	/* flow
		1. vertex data
		2. vertex shader & pixel shader
		3. vertex layout : 解釋這一塊數據代表甚麼, 例如vertices is position data.
		4. root signature
		5. pipeline state
	*/

	//
	// Generate vertex buffer
	//

	GenerateTriangle ();

	//
	// Setup shader
	//

	ID3DBlob* vsBlob = nullptr;		// vertex shader
	ID3DBlob* psBlob = nullptr;		// pixel shader
	ID3DBlob* errorBlob = nullptr;
	hr = D3DCompileFromFile (
		L"BasicVertexShader.hlsl",								// shader file name
		nullptr,												// define
		D3D_COMPILE_STANDARD_FILE_INCLUDE,						// include
		"BasicVS", "vs_5_0",									// entry function name, shader version
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,		// flags (debug mode | skip optimize)
		0,														// adress of shader code
		&vsBlob, &errorBlob										// blob point & send error inf to errorBlob when vsBlob has broken
	);

	if (FAILED (hr)) {
		if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND)) {
			printf_s ("(Vertex Shader)File is not found.");
			return -1;
		} else {
			std::string errStr;
			errStr.resize (errorBlob->GetBufferSize ());

			std::copy_n ((char*)errorBlob->GetBufferPointer (), errorBlob->GetBufferSize (), errStr.begin ());
			errStr += "\n";

			printf_s ("(Vertex Shader)%s", errStr.c_str ());
		}
		return -1;
	}

	hr = D3DCompileFromFile (
		L"BasicPixelShader.hlsl",								// shader file name
		nullptr,												// define
		D3D_COMPILE_STANDARD_FILE_INCLUDE,						// include
		"BasicPS", "ps_5_0",									// entry function name, shader version
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,		// flags (debug mode | skip optimize)
		0,														// adress of shader code
		&psBlob, &errorBlob										// blob point & send error inf to errorBlob when psBlob has broken
	);

	if (FAILED (hr)) {
		if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND)) {
			printf_s ("(Pixel Shader)File is not found.");
			return -1;
		} else {
			std::string errStr;
			errStr.resize (errorBlob->GetBufferSize ());

			std::copy_n ((char*)errorBlob->GetBufferPointer (), errorBlob->GetBufferSize (), errStr.begin ());
			errStr += "\n";

			printf_s ("(Pixel Shader)%s", errStr.c_str ());
		}
		return -1;
	}

	//
	// layout of vertex
	//

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION",										// SemanticName : 語義詞, POSITION代表座標
			0,												// SemanticIndex
			DXGI_FORMAT_R32G32B32_FLOAT,					// Format
			0,												// InputSlot
			D3D12_APPEND_ALIGNED_ELEMENT,					// AlignedByteOffset : offset address of data
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,		// InputSlotClass
			0												// InstanceDataStepRate 
		},
	};

	//
	// Pipeline state
	//

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = nullptr;

	// shader set
	gpipeline.VS.pShaderBytecode = vsBlob->GetBufferPointer ();
	gpipeline.VS.BytecodeLength = vsBlob->GetBufferSize ();
	gpipeline.PS.pShaderBytecode = psBlob->GetBufferPointer ();
	gpipeline.PS.BytecodeLength = psBlob->GetBufferSize ();

	// sample mask
	// rasterizer state

	// 0xffffffff
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;

	gpipeline.RasterizerState.MultisampleEnable = false;
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	gpipeline.RasterizerState.DepthClipEnable = true;
	gpipeline.RasterizerState.FrontCounterClockwise = false;
	gpipeline.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	gpipeline.RasterizerState.DepthBiasClamp = D3D12_DEFAULT_DEPTH_BIAS_CLAMP;
	gpipeline.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	gpipeline.RasterizerState.AntialiasedLineEnable = false;
	gpipeline.RasterizerState.ForcedSampleCount = 0;
	gpipeline.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	// blend
	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};

	renderTargetBlendDesc.BlendEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	renderTargetBlendDesc.LogicOpEnable = false;

	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;

	// input layout
	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof (inputLayout);

	//
	gpipeline.DepthStencilState.DepthEnable = false;
	gpipeline.DepthStencilState.StencilEnable = false;

	//
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;

	//
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// render target
	gpipeline.NumRenderTargets = 1;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	// root signature
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
		printf_s ("ERROR : D3D12SerializeRootSignature() - FAILED\n");
		return false;
	}

	hr = _dev->CreateRootSignature (
		0,
		rootSigBlob->GetBufferPointer (),
		rootSigBlob->GetBufferSize (),
		IID_PPV_ARGS (&_rootSignature)
	);

	rootSigBlob->Release ();

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateRootSignature() - FAILED\n");
		return false;
	}

	gpipeline.pRootSignature = _rootSignature;

	//
	hr = _dev->CreateGraphicsPipelineState (&gpipeline, IID_PPV_ARGS (&_pipelineState));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateGraphicsPipelineState() - FAILED\n");
		return false;
	}

	//
	// viewport
	//

	_viewport.Width = window_width;
	_viewport.Height = window_height;
	_viewport.TopLeftX = 0.0f;
	_viewport.TopLeftY = 0.0f;
	_viewport.MaxDepth = 1.0f;
	_viewport.MinDepth = 0.0f;

	//
	// scissor rect
	//

	_scissorrect.top = 0;
	_scissorrect.left = 0;
	_scissorrect.right = _scissorrect.left + window_width;
	_scissorrect.bottom = _scissorrect.top + window_height;

	// viewport & scissor rect 之後會傳到GraphicsCommandList的RSSetViewports和RsSetScissorRect中使用

	return true;
}

#ifdef _DEBUG
int main () {
	printf_s ("Debug Mode\n");
	EnableDebugLayer ();
#else
int WINMAIN (HINSTANCE, HINSTANCE, LPSTR, int) {
#endif

	if (!D3D::InitD3D (GetModuleHandle (nullptr), window_width, window_height)) {
		printf_s ("ERROR : InitD3D() - FAILED\n");
		return -1;
	}

	if (!Setup ()) {
		printf_s ("ERROR : Setup() - FAILED\n");
		return -1;
	}

	D3D::EnterMsgLoop (Display);

	UnregisterClass (_w.lpszClassName, _w.hInstance);

	printf_s ("Run End\n");

	return 0;
}