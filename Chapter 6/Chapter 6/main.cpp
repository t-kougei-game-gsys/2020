#include "D3DUtility.h"
#include "Vertex.h"

#include <d3dx12.h>
#include <DirectXTex.h>
#include <DirectXMath.h>
#include <d3dcompiler.h>

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "DirectXTex.lib")

using namespace D3D;
using namespace DirectX;

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

D3D_DESC _d3dDesc = {};

D3D12_VERTEX_BUFFER_VIEW _vbView = {};
D3D12_INDEX_BUFFER_VIEW _ibView = {};

ID3D12RootSignature* _rootSignature;
ID3D12PipelineState* _pipelineState;

ID3D12DescriptorHeap* _basicDescHeap = nullptr;

D3D12_VIEWPORT _viewport = {};
D3D12_RECT _scissorRect = {};

XMMATRIX* _matrix;

XMMATRIX _viewMat;
XMMATRIX _projMat;

bool Display (float deltaTime) {
	int bbIdx = _d3dDesc.SwapChain->GetCurrentBackBufferIndex ();

	static float angle = 0.0f;
	angle += 0.1f;
	auto worldMat = XMMatrixRotationY (angle);
	*_matrix = worldMat * _viewMat * _projMat;

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

	_d3dDesc.CMDList->SetDescriptorHeaps (1, &_basicDescHeap);

	//
	// 7. Drawの設定
	//

	//auto heapHandle = _basicDescHeap->GetGPUDescriptorHandleForHeapStart ();
	//heapHandle.ptr += _d3dDesc.Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	_d3dDesc.CMDList->SetGraphicsRootDescriptorTable (0, _basicDescHeap->GetGPUDescriptorHandleForHeapStart ());

	_d3dDesc.CMDList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_d3dDesc.CMDList->IASetVertexBuffers (0, 1, &_vbView);
	_d3dDesc.CMDList->IASetIndexBuffer (&_ibView);
	_d3dDesc.CMDList->DrawIndexedInstanced (6, 1, 0, 0, 0);

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

bool CreateVertex () {
	HRESULT hr = 0;

	/*UVVertex vertices[] = {
		{{   0.0f, 100.0f, 0.0f}, {0.0f, 1.0f}},
		{{   0.0f,   0.0f, 0.0f}, {0.0f, 0.0f}},
		{{ 100.0f, 100.0f, 0.0f}, {1.0f, 1.0f}},
		{{ 100.0f,   0.0f, 0.0f}, {1.0f, 0.0f}},
	};*/

	//UVVertex vertices[] = {
	//	{{-0.5f, -0.9f, 0.0f}, {0.0f, 1.0f}},
	//	{{-0.5f,  0.9f, 0.0f}, {0.0f, 0.0f}},
	//	{{ 0.5f, -0.9f, 0.0f}, {1.0f, 1.0f}},
	//	{{ 0.5f,  0.9f, 0.0f}, {1.0f, 0.0f}},
	//};

	UVVertex vertices[] = {
		{{-1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},
		{{-1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},
		{{ 1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},
		{{ 1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}},
	};

	ID3D12Resource* vertBuff = nullptr;
	hr = _d3dDesc.Device->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (sizeof (vertices)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&vertBuff)
	);

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommittedResource(Vertex) - FAILED");
		return false;
	}

	UVVertex* vertMap = nullptr;
	vertBuff->Map (0, nullptr, (void**)&vertMap);
	copy (begin (vertices), end (vertices), vertMap);
	vertBuff->Unmap (0, nullptr);

	_vbView.BufferLocation = vertBuff->GetGPUVirtualAddress ();
	_vbView.SizeInBytes = sizeof (vertices);
	_vbView.StrideInBytes = sizeof (vertices[0]);

	unsigned short indices[] = {
		0, 1, 2,
		2, 1, 3
	};

	ID3D12Resource* idxBuff = nullptr;
	hr = _d3dDesc.Device->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (sizeof (indices)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&idxBuff)
	);

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommittedResource(Index) - FAILED");
		return false;
	}

	unsigned short* idxMap = nullptr;
	idxBuff->Map (0, nullptr, (void**)&idxMap);
	copy (begin (indices), end (indices), idxMap);
	idxBuff->Unmap (0, nullptr);

	_ibView.BufferLocation = idxBuff->GetGPUVirtualAddress ();
	_ibView.SizeInBytes = sizeof (indices);
	_ibView.Format = DXGI_FORMAT_R16_UINT;

	return true;
}

/* Matrix Transformation to shader
	1. Create Matrixs (XMMATRIX*)
	2. Create constant buffer (ID3DResource)
		2.1 D3D12の方法
		2.2 D3DX12の方法
	3. Map
	4. Constant Buffer View (CBV)
		4.1 Descriptorの作成
		4.2 Viewの作成
	5. RootSignatureの設定 : in GPUSetting()
		5.1 DESCRIPTOR_RANGEの設定
		5.2 ROOT_PARAMETERの設定
	6. ShaderにBufferの応用 (Vertex Shader)
	7. Drawの設定 : in display()
*/
bool TextureTransformation () {
	//
	// Create Texture
	//

	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	LoadFromWICFile (L"img/textest.png", WIC_FLAGS_NONE, &metadata, scratchImg);

	auto img = scratchImg.GetImage (0, 0, 0);

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;
	resDesc.Width = metadata.width;
	resDesc.Height = metadata.height;
	resDesc.DepthOrArraySize = metadata.arraySize;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = metadata.mipLevels;
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION> (metadata.dimension);
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* texBuff;
	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS (&texBuff)
	);

	texBuff->WriteToSubresource (0, nullptr, img->pixels, img->rowPitch, img->slicePitch);

	//
	// 1. Create Matrixs (XMMATRIX*)
	//

	// world matrix 

	auto matrix = XMMatrixIdentity ();

	//
	// 2D
	//
	
	//matrix.r[0].m128_f32[0] = 2.0f / window_width;
	//matrix.r[1].m128_f32[1] = -2.0f / window_height;
	//matrix.r[3].m128_f32[0] = -1.0f;
	//matrix.r[3].m128_f32[1] = 1.0f;

	//
	// 3D
	//

	// World Matrix

	auto worldMat = XMMatrixRotationY (XM_PIDIV4);

	// View Matrix

	XMFLOAT3 eye (0, 0, -5);
	XMFLOAT3 target (0, 0, 0);
	XMFLOAT3 up (0, 1, 0);
	_viewMat  = XMMatrixLookAtLH (XMLoadFloat3(&eye), XMLoadFloat3 (&target), XMLoadFloat3 (&up));

	// Projection Matrix

	_projMat = XMMatrixPerspectiveFovLH (
		XM_PIDIV2,									// fov
		(float)window_width / (float)window_height,	// ratio
		1.0f,										// near
		1000.0f										// far
	);

	//
	// 2. Create constant buffer (ID3DResource)
	//

	ID3D12Resource* constantBuff = nullptr;

	// 2.1 D3D12の方法
	
	//D3D12_HEAP_PROPERTIES heapProp2 = {};
	//heapProp2.Type = D3D12_HEAP_TYPE_UPLOAD;
	//heapProp2.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	//heapProp2.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	//D3D12_RESOURCE_DESC resDesc2 = {};
	//resDesc2.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//resDesc2.Width = ((sizeof(XMMATRIX) + 0xff) & ~0xff);
	//resDesc2.Height = 1;
	//resDesc2.DepthOrArraySize = 1;
	//resDesc2.MipLevels = 1;
	//resDesc2.Format = DXGI_FORMAT_UNKNOWN;
	//resDesc2.SampleDesc.Count = 1;
	//resDesc2.Flags = D3D12_RESOURCE_FLAG_NONE;
	//resDesc2.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	//_d3dDesc.Device->CreateCommittedResource (
	//	&heapProp2,
	//	D3D12_HEAP_FLAG_NONE,
	//	&resDesc2,
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	nullptr,
	//	IID_PPV_ARGS (&constantBuff)
	//);

	// 2.2 D3DX12の方法

	_d3dDesc.Device->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (((sizeof(XMMATRIX) + 0xff) & ~0xff)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&constantBuff)
	);

	//
	// 3. Map
	//

	constantBuff->Map (0, nullptr, (void**)&_matrix);
	*_matrix = matrix;
	constantBuff->Unmap (0, nullptr);

	//
	// 4. Constant Buffer View (CBV)
	//
	
	// 4.1 Descriptorの作成

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 2;		// 二つのVIEWがあります。
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	_d3dDesc.Device->CreateDescriptorHeap (&descHeapDesc, IID_PPV_ARGS (&_basicDescHeap));

	// _d3dDesc.Device->CreateDescriptorHeap (&descHeapDesc2, IID_PPV_ARGS (&_basicDescHeap));

	// 4.2 Viewの作成

	// Texture (Shader Resource View)

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	auto basicHeapHandle = _basicDescHeap->GetCPUDescriptorHandleForHeapStart ();

	_d3dDesc.Device->CreateShaderResourceView (texBuff, &srvDesc, basicHeapHandle);

	// Matrix (Constant Buffer View)

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = constantBuff->GetGPUVirtualAddress ();
	cbvDesc.SizeInBytes = constantBuff->GetDesc ().Width;

	basicHeapHandle.ptr += _d3dDesc.Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	_d3dDesc.Device->CreateConstantBufferView (&cbvDesc, basicHeapHandle);

	// 5. RootSignatureの設定, in GPUSetting

	return true;
}

bool GPUSetting () {
	HRESULT hr = 0;

	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;

	hr = D3DCompileFromFile (
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vsBlob, &errorBlob
	);

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

	D3DCompileFromFile (
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&psBlob, &errorBlob
	);

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
		{
			"TEXCOORD",
			0,
			DXGI_FORMAT_R32G32_FLOAT,
			0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
			0
		}
	};

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//
	// 5. RootSignatureの設定
	//

	// 5.1 DESCRIPTOR_RANGEの設定

	D3D12_DESCRIPTOR_RANGE descRange[2] = {};

	// テクスチャ用レジスター[0]

	descRange[0].NumDescriptors = 1;
	descRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descRange[0].BaseShaderRegister = 1;							// 0 slot
	descRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// Constant Buffer用レジスター[1]

	descRange[1].NumDescriptors = 1;								// only one descriptor
	descRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;		// constant buffer view
	descRange[1].BaseShaderRegister = 1;							// 0 slot
	descRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// レジスター番号の重複も問題ない
	// 種別が違う場合、レジスターは別の場所にある

	// 5.2 ROOT_PARAMETERの設定

	// 一緒に使う

	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.DescriptorTable.pDescriptorRanges = descRange;
	rootParam.DescriptorTable.NumDescriptorRanges = 2;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	/* 分別で設定
	D3D12_ROOT_PARAMETER rootParam[2] = {};

	// テクスチャ用

	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam[0].DescriptorTable.pDescriptorRanges = &descRange[0];
	rootParam[0].DescriptorTable.NumDescriptorRanges = 1;

	// Constant Buffer用

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;
	rootParam[1].DescriptorTable.pDescriptorRanges = &descRange[1];
	rootParam[1].DescriptorTable.NumDescriptorRanges = 1;
	*/

	rootSignatureDesc.pParameters = &rootParam;
	rootSignatureDesc.NumParameters = 1;

	// ----------------------------------------------------------------------------

	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};
	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	//samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;
	samplerDesc.MinLOD = 0.0f;
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

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

	hr = _d3dDesc.Device->CreateRootSignature (
		0,
		rootSigBlob->GetBufferPointer (),
		rootSigBlob->GetBufferSize (),
		IID_PPV_ARGS (&_rootSignature)
	);

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateRootSignature() - FAILED\n");
		return false;
	}

	rootSigBlob->Release ();

	D3D12_GRAPHICS_PIPELINE_STATE_DESC pipelineDesc = {};
	pipelineDesc.pRootSignature = _rootSignature;
	pipelineDesc.VS.pShaderBytecode = vsBlob->GetBufferPointer ();
	pipelineDesc.VS.BytecodeLength = vsBlob->GetBufferSize ();
	pipelineDesc.PS.pShaderBytecode = psBlob->GetBufferPointer ();
	pipelineDesc.PS.BytecodeLength = psBlob->GetBufferSize ();
	pipelineDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	pipelineDesc.RasterizerState.MultisampleEnable = false;
	pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	//pipelineDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT;
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
	pipelineDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	pipelineDesc.SampleDesc.Count = 1;
	pipelineDesc.SampleDesc.Quality = 0;

	hr = _d3dDesc.Device->CreateGraphicsPipelineState (&pipelineDesc, IID_PPV_ARGS (&_pipelineState));

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateGraphicsPipelineState() - FAILED\n");
		return false;
	}

	_viewport.Width = window_width;
	_viewport.Height = window_height;
	_viewport.TopLeftX = 0;
	_viewport.TopLeftY = 0;
	_viewport.MaxDepth = 1.0f;
	_viewport.MinDepth = 0.0f;

	_scissorRect.top = 0;
	_scissorRect.left = 0;
	_scissorRect.right = _scissorRect.left + window_width;
	_scissorRect.bottom = _scissorRect.top + window_height;

	return true;
}

bool Setup () {

	if (!CreateVertex ()) {
		printf_s ("ERROR : CreateVertex() - FAILED\n");
		return false;
	}

	if (!TextureTransformation ()) {
		printf_s ("ERROR : CreateBuffer() - FAILED\n");
		return false;
	}

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