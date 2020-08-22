#include "PMDRenderer.h"
#include <d3dx12.h>
#include <cassert>
#include <d3dcompiler.h>
#include "Dx12Wrapper.h"
#include <string>
#include <algorithm>

using namespace std;

namespace {
	void PrintErrorBlob (ID3DBlob* blob) {
		assert (blob);
		string err;
		err.resize (blob->GetBufferSize ());
		copy_n ((char*)blob->GetBufferPointer (), err.size (), err.begin ());
	}
}

PMDRenderer::PMDRenderer (DX12Wrapper& dx12) :_dx12 (dx12) {
	assert (SUCCEEDED (CreateRootSignature ()));
	assert (SUCCEEDED (CreateGraphicsPipelineForPMD ()));
	_whiteTex = CreateWhiteTexture ();
	_blackTex = CreateBlackTexture ();
	_gradTex = CreateGrayGradationTexture ();
}

PMDRenderer::~PMDRenderer () {}

void PMDRenderer::Update () {}

void PMDRenderer::Draw () {}

ID3D12Resource* PMDRenderer::CreateDefaultTexture (size_t width, size_t height) {
	auto resDesc = CD3DX12_RESOURCE_DESC::Tex2D (DXGI_FORMAT_R8G8B8A8_UNORM, width, height);
	auto texHeapProp = CD3DX12_HEAP_PROPERTIES (D3D12_CPU_PAGE_PROPERTY_WRITE_BACK, D3D12_MEMORY_POOL_L0);
	ID3D12Resource* buffer = nullptr;
	auto hr = _dx12.Device ()->CreateCommittedResource (
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS (&buffer)
	);

	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return nullptr;
	}

	return buffer;
}

ID3D12Resource* PMDRenderer::CreateWhiteTexture () {
	ID3D12Resource* whiteBuff = CreateDefaultTexture (4, 4);

	std::vector<unsigned char> data (4 * 4 * 4);
	std::fill (data.begin (), data.end (), 0xff);

	auto result = whiteBuff->WriteToSubresource (0, nullptr, data.data (), 4 * 4, data.size ());

	assert (SUCCEEDED (result));

	return whiteBuff;
}

ID3D12Resource* PMDRenderer::CreateBlackTexture () {
	ID3D12Resource* blackBuff = CreateDefaultTexture (4, 4);
	std::vector<unsigned char> data (4 * 4 * 4);
	std::fill (data.begin (), data.end (), 0x00);

	auto result = blackBuff->WriteToSubresource (0, nullptr, data.data (), 4 * 4, data.size ());

	assert (SUCCEEDED (result));

	return blackBuff;
}

ID3D12Resource* PMDRenderer::CreateGrayGradationTexture () {
	ID3D12Resource* gradBuff = CreateDefaultTexture (4, 256);

	std::vector<unsigned int> data (4 * 256);
	auto it = data.begin ();
	unsigned int c = 0xff;
	for (; it != data.end (); it += 4) {
		auto col = (0xff << 24) | RGB (c, c, c);
		std::fill (it, it + 4, col);
		--c;
	}

	auto result = gradBuff->WriteToSubresource (0, nullptr, data.data (), 4 * sizeof (unsigned int), sizeof (unsigned int) * data.size ());
	
	assert (SUCCEEDED (result));

	return gradBuff;
}

bool PMDRenderer::CheckShaderCompileResult (HRESULT hr, ID3DBlob* error) {
	if (FAILED (hr)) {
		if (hr == HRESULT_FROM_WIN32 (ERROR_FILE_NOT_FOUND)) {
			OutputDebugStringA ("File isn't exist.");
		} else {
			std::string errstr;
			errstr.resize (error->GetBufferSize ());
			std::copy_n ((char*)error->GetBufferPointer (), error->GetBufferSize (), errstr.begin ());
			errstr += "\n";
			OutputDebugStringA (errstr.c_str ());
		}
		return false;
	} else {
		return true;
	}
}

HRESULT PMDRenderer::CreateGraphicsPipelineForPMD () {
	ComPtr<ID3DBlob> vsBlob = nullptr;
	ComPtr<ID3DBlob> psBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto hr = D3DCompileFromFile (L"PMDVertexShader.hlsl",
									  nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
									  "main", "vs_5_0",
									  D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
									  0, &vsBlob, &errorBlob);

	if (!CheckShaderCompileResult (hr, errorBlob.Get ())) {
		assert (0);
		return hr;
	}

	hr = D3DCompileFromFile (L"PMDPixelShader.hlsl",
								 nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE,
								 "main", "ps_5_0",
								 D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
								 0, &psBlob, &errorBlob);

	if (!CheckShaderCompileResult (hr, errorBlob.Get ())) {
		assert (0);
		return hr;
	}

	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
		{
			"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
			D3D12_APPEND_ALIGNED_ELEMENT,
			D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0
		},
		{
			"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0,
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};
	gpipeline.pRootSignature = _rootSig.Get ();
	gpipeline.VS = CD3DX12_SHADER_BYTECODE (vsBlob.Get ());
	gpipeline.PS = CD3DX12_SHADER_BYTECODE (psBlob.Get ());
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	gpipeline.BlendState = CD3DX12_BLEND_DESC (D3D12_DEFAULT);
	gpipeline.RasterizerState = CD3DX12_RASTERIZER_DESC (D3D12_DEFAULT);
	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
	gpipeline.DepthStencilState.DepthEnable = true;
	gpipeline.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	gpipeline.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	gpipeline.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	gpipeline.DepthStencilState.StencilEnable = false;
	gpipeline.InputLayout.pInputElementDescs = inputLayout;
	gpipeline.InputLayout.NumElements = _countof (inputLayout);
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	// Chapter 14
	gpipeline.NumRenderTargets = 3;
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpipeline.RTVFormats[1] = DXGI_FORMAT_R8G8B8A8_UNORM;
	gpipeline.RTVFormats[2] = DXGI_FORMAT_R8G8B8A8_UNORM;

	gpipeline.SampleDesc.Count = 1;
	gpipeline.SampleDesc.Quality = 0;
	hr = _dx12.Device ()->CreateGraphicsPipelineState (&gpipeline, IID_PPV_ARGS (_pipeline.ReleaseAndGetAddressOf ()));

	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
	}

	return hr;
}

HRESULT PMDRenderer::CreateRootSignature () {
	CD3DX12_DESCRIPTOR_RANGE  descTblRanges[4] = {};
	descTblRanges[0].Init (D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
	descTblRanges[1].Init (D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);
	descTblRanges[2].Init (D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 2);
	descTblRanges[3].Init (D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 4, 0);

	CD3DX12_ROOT_PARAMETER rootParams[3] = {};
	rootParams[0].InitAsDescriptorTable (1, &descTblRanges[0]);
	rootParams[1].InitAsDescriptorTable (1, &descTblRanges[1]);
	rootParams[2].InitAsDescriptorTable (2, &descTblRanges[2]);

	CD3DX12_STATIC_SAMPLER_DESC samplerDescs[2] = {};
	samplerDescs[0].Init (0);
	samplerDescs[1].Init (1, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_CLAMP, D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Init (3, rootParams, 2, samplerDescs, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> rootSigBlob = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;

	auto hr = D3D12SerializeRootSignature (&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSigBlob, &errorBlob);
	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return hr;
	}

	hr = _dx12.Device ()->CreateRootSignature (0, rootSigBlob->GetBufferPointer (), rootSigBlob->GetBufferSize (), IID_PPV_ARGS (_rootSig.ReleaseAndGetAddressOf ()));
	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return hr;
	}

	return hr;
}

ID3D12PipelineState* PMDRenderer::GetPipelineState () {
	return _pipeline.Get ();
}

ID3D12RootSignature* PMDRenderer::GetRootSignature () {
	return _rootSig.Get ();
}

#pragma region Chapter 14

void PMDRenderer::PreDraw () {
	auto cmdlist = _dx12.CommandList ();
	cmdlist->SetPipelineState (_pipeline.Get ());
	cmdlist->SetGraphicsRootSignature (_rootSig.Get ());
	cmdlist->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
}

#pragma endregion