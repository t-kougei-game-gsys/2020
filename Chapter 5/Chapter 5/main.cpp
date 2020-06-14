#include "D3DUtility.h"
#include "Vertex.h"

#include <DirectXTex.h>
#include <vector>
#include <d3dcompiler.h>

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "DirectXTex.lib")

using namespace D3D;

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

D3D_DESC _d3dDesc = {};

ID3D12RootSignature* _rootSignature = nullptr;
ID3D12PipelineState* _pipelineState = nullptr;
ID3D12DescriptorHeap* _texHeap = nullptr;

D3D12_VIEWPORT _viewport = {};
D3D12_RECT _scissorRect = {};

D3D12_VERTEX_BUFFER_VIEW _vbView = {};
D3D12_INDEX_BUFFER_VIEW _ibView = {};

size_t
AlignmentedSize (size_t size, size_t alignment) {
	return size + alignment - size % alignment;
}

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

	_d3dDesc.CMDList->SetDescriptorHeaps (1, &_texHeap);
	_d3dDesc.CMDList->SetGraphicsRootDescriptorTable (0, _texHeap->GetGPUDescriptorHandleForHeapStart ());

	_d3dDesc.CMDList->RSSetViewports (1, &_viewport);
	_d3dDesc.CMDList->RSSetScissorRects (1, &_scissorRect);
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

void NoiseTexture ();
void ReadTexture ();
void ReadTexture2 ();
int _textureIndex = 0;

void ChangeTexture () {
	switch (_textureIndex % 3) {
		case 0:
			NoiseTexture ();
			break;
		case 1:
			ReadTexture ();
			break;
		case 2:
			ReadTexture2 ();
			break;
	}
}

LRESULT CALLBACK D3D::WndProc (HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg) {
		case WM_DESTROY:
			PostQuitMessage (0);
			break;
		case WM_KEYDOWN:
			switch (wParam) {
				case VK_ESCAPE:
					DestroyWindow (hwnd);
					break;
				case VK_RIGHT:
					_textureIndex++;
					ChangeTexture ();
					break;
				case VK_LEFT:
					_textureIndex = (_textureIndex + 2) % 3;
					ChangeTexture ();
					break;
			}
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

	UVVertex vertices[] = {
		{{-0.5f, -0.9f, 0.0f} ,{0.0f, 1.0f}},
		{{-0.5f,  0.9f, 0.0f} ,{0.0f, 0.0f}},
		{{ 0.5f, -0.9f, 0.0f} ,{1.0f, 1.0f}},
		{{ 0.5f,  0.9f, 0.0f} ,{1.0f, 0.0f}},
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

	UVVertex* vertMap = nullptr;
	vertBuff->Map (0, nullptr, (void**)&vertMap);
	std::copy (std::begin (vertices), std::end (vertices), vertMap);
	vertBuff->Unmap (0, nullptr);

	_vbView.BufferLocation = vertBuff->GetGPUVirtualAddress ();
	_vbView.SizeInBytes = sizeof (vertices);
	_vbView.StrideInBytes = sizeof (vertices[0]);

	unsigned short indices[] = {
		0, 1, 2,
		2, 1, 3
	};

	resDesc.Width = sizeof (indices);

	ID3D12Resource* idxBuff = nullptr;

	hr = _d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&idxBuff)
	);

	if (FAILED (hr)) {
		printf_s ("ERROR : CreateCommittedResource() - FAILED\n");
		return false;
	}

	unsigned short* idxMap = nullptr;
	idxBuff->Map (0, nullptr, (void**)&idxMap);
	std::copy (std::begin (indices), std::end (indices), idxMap);
	idxBuff->Unmap (0, nullptr);

	_ibView.BufferLocation = idxBuff->GetGPUVirtualAddress ();
	_ibView.SizeInBytes = sizeof (indices);
	_ibView.Format = DXGI_FORMAT_R16_UINT;

	return true;
}

bool GPUSetting () {
	ID3DBlob* errorBlob = nullptr;
	ID3DBlob* vsBlob = nullptr;
	ID3DBlob* psBlob = nullptr;

	D3DCompileFromFile (
		L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", "vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&vsBlob, &errorBlob
	);

	D3DCompileFromFile (
		L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main", "ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&psBlob, &errorBlob
	);

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
			"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,
			0, D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		}
	};

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	//
	// 5. RootSignatureの作成
	//

	// 5.1 D3D_ROOT_PARAMETERの作成

	// 5.1.1 D3D12_DESCRIPTOR_RANGEの作成

	D3D12_DESCRIPTOR_RANGE descTblRange = {};
	descTblRange.NumDescriptors = 1;
	descTblRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descTblRange.BaseShaderRegister = 0;
	descTblRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 5.1.2 D3D_ROOT_PARAMETERの作成

	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam.DescriptorTable.pDescriptorRanges = &descTblRange;
	rootParam.DescriptorTable.NumDescriptorRanges = 1;

	// 5.2 RootSignatureDescのpParamters & NumParametersを設定する
	rootSignatureDesc.pParameters = &rootParam;
	rootSignatureDesc.NumParameters = 1;

	//
	// 6. Samplerの設定
	//

	// 6.1 D3D12_STATIC_SAMPLER_DESCの作成

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

	// 6.2 RootSignatureDescのpStaticSamplers & NumStaticSamplersを設定する

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	ID3DBlob* rootSigBlob = nullptr;
	D3D12SerializeRootSignature (
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob
	);

	_d3dDesc.Device->CreateRootSignature (
		0,
		rootSigBlob->GetBufferPointer (),
		rootSigBlob->GetBufferSize (),
		IID_PPV_ARGS (&_rootSignature)
	);

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

	_d3dDesc.Device->CreateGraphicsPipelineState (&pipelineDesc, IID_PPV_ARGS (&_pipelineState));

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

#pragma region Noise Texture

struct TexRGBA {
	unsigned char R, G, B, A;
};

/* Create Texture
	1. Texture Dataの作成
	2. Texture Buffer (ID3DResource)
		2.1 HEAP_PROPERTIESの作成
		2.2 RESOURCE_DESCの作成
		2.3 Buffer作成
	3. Bufferの転送
	4. ShaderResourceViewの作成
		4.1　DescriptorHeapの作成
		4.2　ShaderResourceViewの作成
	5. RootSignatureの作成
		5.1 D3D_ROOT_PARAMETERの作成
			5.1.1 D3D12_DESCRIPTOR_RANGEの作成
			5.1.2 D3D_ROOT_PARAMETERの作成
		5.2 RootSignatureDescのpParamters & NumParametersを設定する
	6. Samplerの設定
		6.1 D3D12_STATIC_SAMPLER_DESCの作成
		6.2 RootSignatureDescのpStaticSamplers & NumStaticSamplersを設定する
*/

void NoiseTexture () {
	//
	// 1. Texture Dataの作成
	//

	std::vector<TexRGBA> textureData (256 * 256);

	for (auto& rgba : textureData) {
		rgba.R = rand () % 256;
		rgba.G = rand () % 256;
		rgba.B = rand () % 256;
		rgba.G = 255;
	}

	//
	// 	2. Texture Buffer (ID3DResource)
	// 

	// 2.1 HEAP_PROPERTIESの作成

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapProp.CreationNodeMask = 0;
	heapProp.VisibleNodeMask = 0;

	// 2.2 RESOURCE_DESCの作成

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.Width = 256;
	resDesc.Height = 256;
	resDesc.DepthOrArraySize = 1;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	// 2.3 Buffer作成

	ID3D12Resource* texBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS (&texBuff)
	);

	//
	// 3. Bufferの転送
	//

	texBuff->WriteToSubresource (
		0,
		nullptr,
		textureData.data (),
		sizeof (TexRGBA) * 256,
		sizeof (TexRGBA) * textureData.size ()
	);

	//
	// 4. ShaderResourceViewの作成
	//

	// 4.1　DescriptorHeapの作成

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	
	_d3dDesc.Device->CreateDescriptorHeap (&descHeapDesc, IID_PPV_ARGS (&_texHeap));

	// 4.2　ShaderResourceViewの作成

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	_d3dDesc.Device->CreateShaderResourceView (
		texBuff,
		&srvDesc,
		_texHeap->GetCPUDescriptorHandleForHeapStart ()
	);
}

#pragma endregion

#pragma region Textureを読み込む

void ReadTexture () {
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	LoadFromWICFile (
		L"img/textest.png", WIC_FLAGS_NONE,
		&metadata, scratchImg
	);

	auto img = scratchImg.GetImage (0, 0, 0);

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapProp.CreationNodeMask = 0;
	heapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	resDesc.Width = metadata.width;
	resDesc.Height = metadata.height;
	resDesc.DepthOrArraySize = metadata.arraySize;
	resDesc.SampleDesc.Count = 1;
	resDesc.Format = metadata.format;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = metadata.mipLevels;
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION> (metadata.dimension);
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* texBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS (&texBuff)
	);

	texBuff->WriteToSubresource (
		0,
		nullptr,
		img->pixels,
		img->rowPitch,
		img->slicePitch
	);

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	_d3dDesc.Device->CreateDescriptorHeap (&descHeapDesc, IID_PPV_ARGS (&_texHeap));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Format = metadata.format;

	_d3dDesc.Device->CreateShaderResourceView (
		texBuff,
		&srvDesc,
		_texHeap->GetCPUDescriptorHandleForHeapStart ()
	);
}

void ReadTexture2 () {
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	LoadFromWICFile (
		L"img/textest.png", WIC_FLAGS_NONE,
		&metadata, scratchImg
	);

	auto img = scratchImg.GetImage (0, 0, 0);

	D3D12_HEAP_PROPERTIES uploadHeapProp = {};
	uploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	uploadHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProp.CreationNodeMask = 0;
	uploadHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC uploadResDesc = {};
	uploadResDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadResDesc.Width = AlignmentedSize (img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT) * img->height;
	uploadResDesc.Height = 1;
	uploadResDesc.DepthOrArraySize = 1;
	uploadResDesc.MipLevels = 1;
	uploadResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	uploadResDesc.SampleDesc.Count = 1;
	uploadResDesc.SampleDesc.Quality = 0;
	uploadResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* uploadBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&uploadHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&uploadResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&uploadBuff)
	);

	D3D12_HEAP_PROPERTIES texheapProp = {};
	texheapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	texheapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	texheapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	texheapProp.CreationNodeMask = 0;
	texheapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Width = metadata.width;
	resDesc.Height = metadata.height;
	resDesc.DepthOrArraySize = metadata.arraySize;
	resDesc.SampleDesc.Count = 1;
	resDesc.Format = metadata.format;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = metadata.mipLevels;
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION> (metadata.dimension);
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* texBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&texheapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS (&texBuff)
	);

	uint8_t* mapforImg = nullptr;
	uploadBuff->Map (0, nullptr, (void**)&mapforImg);
	auto srcAddress = img->pixels;
	auto rowPitch = AlignmentedSize (img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	for (int y = 0; y < img->height; ++y) {
		std::copy_n (srcAddress, rowPitch, mapforImg);
		srcAddress += img->rowPitch;
		mapforImg += rowPitch;
	}
	uploadBuff->Unmap (0, nullptr);

	D3D12_TEXTURE_COPY_LOCATION src = {}, dst = {};
	dst.pResource = texBuff;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	src.pResource = uploadBuff;
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint = {};
	UINT nrow;
	UINT64 rowsize, size;
	auto desc = texBuff->GetDesc ();
	_d3dDesc.Device->GetCopyableFootprints (&desc, 0, 1, 0, &footprint, &nrow, &rowsize, &size);
	src.PlacedFootprint = footprint;
	src.PlacedFootprint.Offset = 0;
	src.PlacedFootprint.Footprint.Width = metadata.width;
	src.PlacedFootprint.Footprint.Height = metadata.height;
	src.PlacedFootprint.Footprint.Depth = metadata.depth;
	src.PlacedFootprint.Footprint.RowPitch = AlignmentedSize (img->rowPitch, D3D12_TEXTURE_DATA_PITCH_ALIGNMENT);
	src.PlacedFootprint.Footprint.Format = img->format;

	{
		_d3dDesc.CMDList->CopyTextureRegion (&dst, 0, 0, 0, &src, nullptr);

		_d3dDesc.BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		_d3dDesc.BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		_d3dDesc.BarrierDesc.Transition.pResource = texBuff;
		_d3dDesc.BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		_d3dDesc.BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
		_d3dDesc.BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

		_d3dDesc.CMDList->ResourceBarrier (1, &_d3dDesc.BarrierDesc);
		_d3dDesc.CMDList->Close ();

		ID3D12CommandList* cmdlists[] = {_d3dDesc.CMDList};
		_d3dDesc.CMDQueue->ExecuteCommandLists (1, cmdlists);

		_d3dDesc.CMDQueue->Signal (_d3dDesc.Fence, ++_d3dDesc.FenceVal);

		if (_d3dDesc.Fence->GetCompletedValue () != _d3dDesc.FenceVal) {
			auto event = CreateEvent (nullptr, false, false, nullptr);
			_d3dDesc.Fence->SetEventOnCompletion (_d3dDesc.FenceVal, event);
			WaitForSingleObject (event, INFINITE);
			CloseHandle (event);
		}
		_d3dDesc.CMDAllocator->Reset ();
		_d3dDesc.CMDList->Reset (_d3dDesc.CMDAllocator, nullptr);
	}
	
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	
	HRESULT hr;
	_texHeap = nullptr;
	ID3D12DescriptorHeap* texDescHeap = nullptr;
	hr = _d3dDesc.Device->CreateDescriptorHeap (&descHeapDesc, IID_PPV_ARGS (&_texHeap));
	if (FAILED (hr)) {
		printf_s ("ERROR : CreateDescriptorHeap () - FAILED");
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	_d3dDesc.Device->CreateShaderResourceView (texBuff,
											   &srvDesc, 
											   _texHeap->GetCPUDescriptorHandleForHeapStart ()
	);

}

#pragma endregion


bool Setup () {

	if (!CreateVertex ()) {
		printf_s ("ERROR : CreateVertex() - FAILED\n");
		return false;
	}

	NoiseTexture ();

	// ReadTexture ();

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