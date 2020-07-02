#include "D3DUtility.h"

#include <iostream>
#include <DirectXMath.h>
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <vector>
#include <DirectXTex.h>

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "DirectXTex.lib")

using namespace D3D;
using namespace DirectX;

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

//
// Shader Material Data
//
struct MaterialForHlsl {
	XMFLOAT3 diffuse;
	float alpha;
	XMFLOAT3 specular;
	float specularity;
	XMFLOAT3 ambient;
};

struct AdditionalMaterial {
	string texPath;
	int toonIdx;
	bool edgeFlg;
};

struct Material {
	unsigned int indicesNum;
	MaterialForHlsl material;
	AdditionalMaterial additional;
};

ID3D12RootSignature* _rootSignature;
ID3D12PipelineState* _pipelineState;
D3D12_VIEWPORT _viewport;
D3D12_RECT _scissorRect;
ID3D12DescriptorHeap* _dsvHeap;
ID3D12DescriptorHeap* _basicDescHeap;
ID3D12DescriptorHeap* _materialDescHeap = nullptr;

vector<Material> _materials = {};
vector<ID3D12Resource*> _textureResources = {};

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



string GetTexturePathFromModelAndTexPath (const string& modelPath, char* texPath) {
	int pathIndex1 = modelPath.rfind ('/');
	int pathIndex2 = modelPath.rfind ('\\');
	auto pathIndex = max (pathIndex1, pathIndex2);
	auto folderPath = modelPath.substr (0, pathIndex + 1);
	return folderPath + texPath;
}

wstring GetWideStringFromString (const string& str) {
	auto num1 = MultiByteToWideChar (
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str(),
		-1,
		nullptr,
		0
	);

	wstring wstr;

	// cout << "str: " << wstr.c_str () << endl;

	wstr.resize (num1);

	auto num2 = MultiByteToWideChar (
		CP_ACP,
		MB_PRECOMPOSED | MB_ERR_INVALID_CHARS,
		str.c_str (),
		-1,
		&wstr[0],
		num1
	);

	//cout << "str: " << wstr.c_str () << endl;

	assert (num1 == num2);
	return wstr;
}

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

	_d3dDesc.CMDList->ClearRenderTargetView (rtvH, WHITE, 0, nullptr);
	_d3dDesc.CMDList->ClearDepthStencilView (dsvH, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

	_d3dDesc.CMDList->SetPipelineState (_pipelineState);
	_d3dDesc.CMDList->SetGraphicsRootSignature (_rootSignature);
	_d3dDesc.CMDList->RSSetViewports (1, &_viewport);
	_d3dDesc.CMDList->RSSetScissorRects (1, &_scissorRect);
	
	_d3dDesc.CMDList->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_d3dDesc.CMDList->IASetVertexBuffers (0, 1, &_vbView);
	_d3dDesc.CMDList->IASetIndexBuffer (&_ibView);

	_d3dDesc.CMDList->SetDescriptorHeaps (1, &_basicDescHeap);
	auto handle = _basicDescHeap->GetGPUDescriptorHandleForHeapStart ();
	_d3dDesc.CMDList->SetGraphicsRootDescriptorTable (0, handle);		// set in ROOT_PARAMETER[0]

	_d3dDesc.CMDList->SetDescriptorHeaps (1, &_materialDescHeap);
	auto materialH = _materialDescHeap->GetGPUDescriptorHandleForHeapStart ();
	auto cbvsrvIncSize = _d3dDesc.Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 2;
	unsigned int idxOffset = 0;
	for (auto& m : _materials) {
		_d3dDesc.CMDList->SetGraphicsRootDescriptorTable (1, materialH);	// set in ROOT_PARAMETER[1]
		_d3dDesc.CMDList->DrawIndexedInstanced (m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
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

//
// PMD Material
//
#pragma pack(1)		// アライメントは発生しない
struct PMDMaterial {
	XMFLOAT3 diffuse;
	float alpha;
	float specularity;
	XMFLOAT3 specular;
	XMFLOAT3 ambient;
	unsigned char toonIdx;
	unsigned char edgeFlg;

	unsigned int indicesNum;
	char texFilePath[20];
};		// 70 byte
#pragma pack()

ID3D12Resource* LoadTextureFromFile (string& texPath) {
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	// cout << "Texture Path: " << GetWideStringFromString (texPath).c_str () << endl;

	LoadFromWICFile (
		GetWideStringFromString (texPath).c_str (),
		// L"Model/Miku.pmd",
		WIC_FLAGS_NONE,
		&metadata,
		scratchImg
	);

	auto img = scratchImg.GetImage (0, 0, 0);

	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.Format = metadata.format;
	resDesc.Width = metadata.width;
	resDesc.Height = metadata.height;
	resDesc.DepthOrArraySize = metadata.arraySize;
	resDesc.SampleDesc.Count = 1;
	resDesc.SampleDesc.Quality = 0;
	resDesc.MipLevels = metadata.mipLevels;
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION> (metadata.dimension);
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* texBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&texHeapProp,
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

	return texBuff;
}	

bool ReadPMD () {
	PMDHeader pmdHeader = {};

	char signature[3] = {};
	string strModelPath = "Model/Miku.pmd";
	auto fp = fopen (strModelPath.c_str (), "rb");
	
	// cout << "String Model Path: " << strModelPath.c_str () << endl;

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
	


	// Material
	unsigned int materialNum;
	fread (&materialNum, sizeof (materialNum), 1, fp);
	// printf_s ("Material Count: %d\n", materialNum);

	vector<PMDMaterial> pmdMaterials (materialNum);
	fread (pmdMaterials.data (), pmdMaterials.size () * sizeof (PMDMaterial), 1, fp);
	// Material
	

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



	// read material
	_materials = vector<Material> (pmdMaterials.size ());
	for (int i = 0; i < pmdMaterials.size (); ++i) {
		_materials[i].indicesNum = pmdMaterials[i].indicesNum;
		_materials[i].material.diffuse = pmdMaterials[i].diffuse;
		_materials[i].material.alpha = pmdMaterials[i].alpha;
		_materials[i].material.specular = pmdMaterials[i].specular;
		_materials[i].material.specularity = pmdMaterials[i].specularity;
		_materials[i].material.ambient = pmdMaterials[i].ambient;
	}

	// create buffer
	auto materialBuffSize = Get256Times (sizeof (MaterialForHlsl));
	ID3D12Resource* materialBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (materialBuffSize * materialNum),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&materialBuff)
	);
	
	char* mapMaterial = nullptr;
	materialBuff->Map (0, nullptr, (void**)&mapMaterial);
	for (auto& m : _materials) {
		*((MaterialForHlsl*)mapMaterial) = m.material;
		mapMaterial += materialBuffSize;
	}
	materialBuff->Unmap (0, nullptr);



	// texture
	_textureResources = vector<ID3D12Resource*> (materialNum);
	for (int i = 0; i < pmdMaterials.size (); i++) {
		if (strlen (pmdMaterials[i].texFilePath) == 0) {
			_textureResources[i] = nullptr;
			continue;
		}

		auto texFilePath = GetTexturePathFromModelAndTexPath (strModelPath, pmdMaterials[i].texFilePath);
		// printf_s ("Texture File Path:%s\n", strModelPath);
		// cout << "Texture File Path: " << texFilePath << endl;
		_textureResources[i] = LoadTextureFromFile (texFilePath);
	}



	// create descriptor heap & view
	D3D12_DESCRIPTOR_HEAP_DESC matDescHeapDesc = {};
	// matDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	matDescHeapDesc.NumDescriptors = materialNum * 2;
	matDescHeapDesc.NodeMask = 0;
	matDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	_d3dDesc.Device->CreateDescriptorHeap (&matDescHeapDesc, IID_PPV_ARGS (&_materialDescHeap));

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = materialBuff->GetGPUVirtualAddress ();
	matCBVDesc.SizeInBytes = materialBuffSize;

	auto matDescHeapH = _materialDescHeap->GetCPUDescriptorHandleForHeapStart ();
	auto inc = _d3dDesc.Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (int i = 0; i < materialNum; ++i) {
		_d3dDesc.Device->CreateConstantBufferView (&matCBVDesc, matDescHeapH);

		matDescHeapH.ptr += inc;
		matCBVDesc.BufferLocation += materialBuffSize;

		if (_textureResources[i] != nullptr) {
			srvDesc.Format = _textureResources[i]->GetDesc().Format;
		}

		_d3dDesc.Device->CreateShaderResourceView (
			_textureResources[i],
			&srvDesc,
			matDescHeapH
		);

		matDescHeapH.ptr += inc;
	}




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

	D3D12_DESCRIPTOR_RANGE descRange[3] = {};
	descRange[0].NumDescriptors = 1;
	descRange[0].BaseShaderRegister = 0;
	descRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;
	
	descRange[1].NumDescriptors = 1;		// いくつかがあるが、一度に使うのは一つ
	descRange[1].BaseShaderRegister = 1;
	descRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;
	descRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	descRange[2].NumDescriptors = 1;
	descRange[2].BaseShaderRegister = 0;
	descRange[2].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descRange[2].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// ROOT_PARAMETER
	
	D3D12_ROOT_PARAMETER rootParam[2] = {};
	rootParam[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[0].DescriptorTable.pDescriptorRanges = &descRange[0];
	rootParam[0].DescriptorTable.NumDescriptorRanges = 1;
	rootParam[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;

	rootParam[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam[1].DescriptorTable.pDescriptorRanges = &descRange[1];
	rootParam[1].DescriptorTable.NumDescriptorRanges = 2;
	rootParam[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	rootSigDesc.NumParameters = 2;
	rootSigDesc.pParameters = rootParam;

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

	rootSigDesc.pStaticSamplers = &samplerDesc;
	rootSigDesc.NumStaticSamplers = 1;

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