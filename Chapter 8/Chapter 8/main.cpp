#include "D3DUtility.h"

#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <vector>

#pragma comment (lib, "d3dcompiler.lib")

using namespace D3D;
using namespace DirectX;

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

ID3D12RootSignature* _rootSignature;
ID3D12PipelineState* _pipelineState;
D3D12_VIEWPORT _viewport;
D3D12_RECT _scissorRect;
ID3D12DescriptorHeap* _dsvHeap;
ID3D12DescriptorHeap* _basicDescHeap;

D3D_DESC _d3dDesc = {};

D3D12_VERTEX_BUFFER_VIEW _vbView = {};
D3D12_INDEX_BUFFER_VIEW _ibView = {};

unsigned int _vertNum = 0;
unsigned int _idxNum = 0;

struct MatricesData {
	XMMATRIX world;
	XMMATRIX viewproj;
};

MatricesData* _matrixData;



float WindowRatio () {
	return (float)window_width / (float)window_height;
}

bool Display (float deltaTime) {
	int bbIdx = _d3dDesc.SwapChain->GetCurrentBackBufferIndex ();

	auto rtvH = _d3dDesc.RTVHeap->GetCPUDescriptorHandleForHeapStart ();
	rtvH.ptr += bbIdx * _d3dDesc.Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	_d3dDesc.BarrierDesc.Transition.pResource = _d3dDesc.BackBuffers[bbIdx];
	_d3dDesc.BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	_d3dDesc.BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	_d3dDesc.CMDList->ResourceBarrier (1, &_d3dDesc.BarrierDesc);

	auto dsvH = _dsvHeap->GetCPUDescriptorHandleForHeapStart ();

	_d3dDesc.CMDList->OMSetRenderTargets (1, &rtvH, false, &dsvH);

	_d3dDesc.CMDList->ClearRenderTargetView (rtvH, BLACK, 0, nullptr);
	_d3dDesc.CMDList->ClearDepthStencilView (dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	_d3dDesc.CMDList->SetPipelineState (_pipelineState);
	_d3dDesc.CMDList->SetGraphicsRootSignature (_rootSignature);
	_d3dDesc.CMDList->RSSetViewports (1, &_viewport);
	_d3dDesc.CMDList->RSSetScissorRects (1, &_scissorRect);

	_d3dDesc.CMDList->SetDescriptorHeaps (1, &_basicDescHeap);
	auto handle = _basicDescHeap->GetGPUDescriptorHandleForHeapStart ();
	_d3dDesc.CMDList->SetGraphicsRootDescriptorTable (0, handle);

	//_d3dDesc.CMDList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
	_d3dDesc.CMDList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_d3dDesc.CMDList->IASetVertexBuffers (0, 1, &_vbView);
	_d3dDesc.CMDList->IASetIndexBuffer (&_ibView);
	//_d3dDesc.CMDList->DrawInstanced (_vertNum, 1, 0, 0);
	_d3dDesc.CMDList->DrawIndexedInstanced (_idxNum, 1, 0, 0, 0);

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

#pragma region PMD

struct PMDHeader {
	float version;
	char model_name[20];
	char comment[256];
};

bool ReadPMD () {
	PMDHeader pmdHeader = {};

	char signature[3] = {};
	auto fp = fopen ("Model/Miku.pmd", "rb");
	
	// PMD Dataの最初3 Byteは"PMD"がありますので、まずは読み込んで、必要ではないByteを消す
	fread (signature, sizeof (signature), 1, fp);
	fread (&pmdHeader, sizeof (PMDHeader), 1, fp);
	
	// constexpr
	// C++ 11以上のfeature
	// constの上位版
	constexpr size_t PMDVERTEX_SIZE = 38;

	fread (&_vertNum, sizeof (_vertNum), 1, fp);
	vector<unsigned char> vertices (_vertNum * PMDVERTEX_SIZE);
	fread (vertices.data (), vertices.size (), 1, fp);

	fread (&_idxNum, sizeof (_idxNum), 1, fp);
	vector<unsigned short> indices (_idxNum);
	fread (indices.data (), indices.size () * sizeof (indices[0]), 1, fp);

	fclose (fp);

	ID3D12Resource* vertBuff = nullptr;
/*
	_d3dDesc.Device->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (vertices.size ()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&vertBuff)
	);
*/

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	D3D12_RESOURCE_DESC	resDesc = {};
	resDesc.Width = vertices.size ();
	resDesc.Height = 1;
	resDesc.Format = DXGI_FORMAT_UNKNOWN;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	resDesc.MipLevels = 1;
	resDesc.DepthOrArraySize = 1;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	resDesc.SampleDesc.Count = 1;
	
	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&vertBuff)
	);

	unsigned char* vertMap = nullptr;
	vertBuff->Map (0, nullptr, (void**)&vertMap);
	copy (begin (vertices), end (vertices), vertMap);
	vertBuff->Unmap (0, nullptr);

	_vbView.BufferLocation = vertBuff->GetGPUVirtualAddress ();
	_vbView.SizeInBytes = vertices.size ();
	_vbView.StrideInBytes = PMDVERTEX_SIZE;



	ID3D12Resource* idxBuff = nullptr;
	resDesc.Width = indices.size () * sizeof (indices[0]);

	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&idxBuff)
	);

	unsigned short* idxMap = nullptr;
	idxBuff->Map (0, nullptr, (void**)&idxMap);
	copy (begin (indices), end (indices), idxMap);
	idxBuff->Unmap (0, nullptr);

	_ibView.BufferLocation = idxBuff->GetGPUVirtualAddress ();
	_ibView.SizeInBytes = indices.size () * sizeof (indices[0]);
	_ibView.Format = DXGI_FORMAT_R16_UINT;

	return true;
}

#pragma endregion

void CreateDepthBuffer () {
	D3D12_RESOURCE_DESC depthResDesc = {};
	depthResDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthResDesc.Width = window_width;
	depthResDesc.Height = window_height;
	depthResDesc.DepthOrArraySize = 1;
	depthResDesc.Format = DXGI_FORMAT_D32_FLOAT;
	depthResDesc.SampleDesc.Count = 1;
	depthResDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_HEAP_PROPERTIES depthHeapProp = {};
	depthHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	depthHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	depthHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	
	D3D12_CLEAR_VALUE depthClearValue = {};
	depthClearValue.DepthStencil.Depth = 1.0;
	depthClearValue.Format = DXGI_FORMAT_D32_FLOAT;

	ID3D12Resource* depthBuffer = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&depthHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&depthResDesc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,	
		&depthClearValue,
		IID_PPV_ARGS (&depthBuffer)
	);

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	_d3dDesc.Device->CreateDescriptorHeap (&dsvHeapDesc, IID_PPV_ARGS (&_dsvHeap));

	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;

	_d3dDesc.Device->CreateDepthStencilView (depthBuffer, &dsvDesc, _dsvHeap->GetCPUDescriptorHandleForHeapStart ());
}

void SetCamera () {
	XMMATRIX worldMat = XMMatrixIdentity ();
	
	XMFLOAT3 eye (0, 20, -25);
	XMFLOAT3 target (0, 10, 0);
	XMFLOAT3 up (0, 1, 0);

	XMMATRIX viewMat = XMMatrixLookAtLH (
		XMLoadFloat3 (&eye),
		XMLoadFloat3 (&target),
		XMLoadFloat3 (&up)
	);

	XMMATRIX projMat = XMMatrixPerspectiveFovLH (
		XM_PIDIV2,		// fov
		WindowRatio (),
		1.0f,
		100.0f
	);
	
	ID3D12Resource* cbvBuffer = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (Get256Times (sizeof (MatricesData))),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&cbvBuffer)
	);

	cbvBuffer->Map (0, nullptr, (void**)&_matrixData);
	_matrixData->world = worldMat;
	_matrixData->viewproj = viewMat * projMat;
	cbvBuffer->Unmap (0, nullptr);

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	_d3dDesc.Device->CreateDescriptorHeap (&descHeapDesc, IID_PPV_ARGS (&_basicDescHeap));

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = cbvBuffer->GetGPUVirtualAddress ();
	cbvDesc.SizeInBytes = cbvBuffer->GetDesc ().Width;

	auto handle = _basicDescHeap->GetCPUDescriptorHandleForHeapStart ();

	_d3dDesc.Device->CreateConstantBufferView (&cbvDesc, handle);
}

bool GPUSetting () {
	// Read Shader

	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;

	D3DCompileFromFile (
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vsBlob,
		nullptr
 	);

	D3DCompileFromFile (
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&psBlob,
		nullptr
	);

	// Input layout

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"BONE_NO", 0, DXGI_FORMAT_R16G16_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"WEIGHT", 0, DXGI_FORMAT_R8_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"EDGE_FLG", 0, DXGI_FORMAT_R8_UINT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
	};

	// D3D12_ROOT_SIGNATURE

	D3D12_ROOT_SIGNATURE_DESC rootSigDesc = {};
	rootSigDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// DESCRIPTOR_RANGE

	D3D12_DESCRIPTOR_RANGE descRange = {};
	descRange.NumDescriptors = 1;
	descRange.BaseShaderRegister = 0;
	descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ROOT_PARAMETER
	
	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.DescriptorTable.pDescriptorRanges = &descRange;
	rootParam.DescriptorTable.NumDescriptorRanges = 1;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootSigDesc.NumParameters = 1;
	rootSigDesc.pParameters = &rootParam;

	// Create

	ID3DBlob* rootSigBlob = nullptr;
	D3D12SerializeRootSignature (
		&rootSigDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		nullptr
	);

	_d3dDesc.Device->CreateRootSignature (
		0,
		rootSigBlob->GetBufferPointer (),
		rootSigBlob->GetBufferSize (),
		IID_PPV_ARGS (&_rootSignature)
	);

	// Pipeline

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
	pipelineDesc.pRootSignature = _rootSignature;
	pipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer ();
	pipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize ();
	pipelineDesc.PS.pShaderBytecode = psBlob->GetBufferPointer ();
	pipelineDesc.PS.BytecodeLength = psBlob->GetBufferSize ();
	//pipelineDesc.HS.BytecodeLength = 0;
	//pipelineDesc.HS.pShaderBytecode = nullptr;
	//pipelineDesc.DS.BytecodeLength = 0;
	//pipelineDesc.DS.pShaderBytecode = nullptr;
	//pipelineDesc.GS.BytecodeLength = 0;
	//pipelineDesc.GS.pShaderBytecode = nullptr;
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pipelineDesc.RasterizerState.MultisampleEnable = false;
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	pipelineDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
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

	pipelineDesc.DepthStencilState.DepthEnable = true;
	pipelineDesc.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	pipelineDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	pipelineDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	_d3dDesc.Device->CreateGraphicsPipelineState (&pipelineDesc, IID_PPV_ARGS (&_pipelineState));

	// Viewport

	_viewport.Width = window_width;
	_viewport.Height = window_height;
	_viewport.TopLeftX = 0;
	_viewport.TopLeftY = 0;
	_viewport.MaxDepth = 1.0f;
	_viewport.MinDepth = 0.0f;

	// Scissor

	_scissorRect.top = 0;
	_scissorRect.left = 0;
	_scissorRect.right = _scissorRect.left + window_width;
	_scissorRect.bottom = _scissorRect.top + window_height;

	return true;
}

bool Setup () {

	if (!ReadPMD ()) {
		return false;
	}

	SetCamera ();
	CreateDepthBuffer ();

	if (!GPUSetting ()) {
		printf_s ("ERROR : GPUSetting() - FAILED\n");
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