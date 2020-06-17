#include "D3DUtility.h"
#include "Vertex.h"

#include <d3dx12.h>
#include <time.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "DirectXTex.lib")

using namespace D3D;

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

ID3D12RootSignature* _rootSignature;
ID3D12PipelineState* _pipelineState;
ID3D12DescriptorHeap* _texHeap;
ID3D12DescriptorHeap* _texHeap2;
ID3D12DescriptorHeap* _texHeap3;

D3D12_VERTEX_BUFFER_VIEW _vbView = {};
D3D12_INDEX_BUFFER_VIEW _ibView = {};

D3D12_VIEWPORT _viewport = {};
D3D12_RECT _scissorRect = {};

D3D_DESC _d3dDesc = {};

struct TexRGBA {
	unsigned char R, G, B, A;
};

int _currentTexIdx = 0;

bool Display (float deltaTime) {
	int bbIdx = _d3dDesc.SwapChain->GetCurrentBackBufferIndex ();

	auto rtvH = _d3dDesc.RTVHeap->GetCPUDescriptorHandleForHeapStart ();
	rtvH.ptr += bbIdx * _d3dDesc.Device->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	 _d3dDesc.BarrierDesc.Transition.pResource = _d3dDesc.BackBuffers[bbIdx];
	 _d3dDesc.BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
	 _d3dDesc.BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
	 _d3dDesc.CMDList->ResourceBarrier (1, &_d3dDesc.BarrierDesc);

	//
	// d3dx12 Barrier
	//

	//_d3dDesc.CMDList->ResourceBarrier (1, &CD3DX12_RESOURCE_BARRIER::Transition (_d3dDesc.BackBuffers[bbIdx],
	//																			D3D12_RESOURCE_STATE_PRESENT,
	//																			D3D12_RESOURCE_STATE_RENDER_TARGET)
	//);

	_d3dDesc.CMDList->OMSetRenderTargets (1, &rtvH, false, nullptr);

	_d3dDesc.CMDList->ClearRenderTargetView (rtvH, BLACK, 0, nullptr);

	_d3dDesc.CMDList->SetPipelineState (_pipelineState);
	_d3dDesc.CMDList->SetGraphicsRootSignature (_rootSignature);
	_d3dDesc.CMDList->RSSetViewports (1, &_viewport);
	_d3dDesc.CMDList->RSSetScissorRects (1, &_scissorRect);

	//
	// 6. 描画するときの設定
	//

	switch (_currentTexIdx) {
		case 0:
			_d3dDesc.CMDList->SetDescriptorHeaps (1, &_texHeap);
			_d3dDesc.CMDList->SetGraphicsRootDescriptorTable (0, _texHeap->GetGPUDescriptorHandleForHeapStart ());
			break;
		case 1:
			_d3dDesc.CMDList->SetDescriptorHeaps (1, &_texHeap2);
			_d3dDesc.CMDList->SetGraphicsRootDescriptorTable (0, _texHeap2->GetGPUDescriptorHandleForHeapStart ());
			break;
		case 2:
			_d3dDesc.CMDList->SetDescriptorHeaps (1, &_texHeap3);
			_d3dDesc.CMDList->SetGraphicsRootDescriptorTable (0, _texHeap3->GetGPUDescriptorHandleForHeapStart ());
			break;
	}

	// ---------------------

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
			else if (wParam == VK_RIGHT)
				_currentTexIdx = (_currentTexIdx + 1) % 3;
			else if (wParam == VK_LEFT)
				_currentTexIdx = (_currentTexIdx + 2) % 3;
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

/* Create Texture Process
	1. 頂点情報にUV情報を追加する
	2. 頂点のLayoutの設定 (GPUSetting)
	3. Shaderの書き換える (Shader)
	4. Textureの作成
		4.0 Import DirectXTex
		4.1 データの準備
		4.2 Bufferの作成
			4.2.1 Texture Buffer (ID3DResource)
			4.2.2 Bufferへ生データを転送する
		4.3 ShaderResourceViewの作成 (TextureBuffの引用)
			4.3.1 Create Descriptor Heap
			4.3.2 Create View
		4.4 Gammaの補正 (Only Read Texture)
	5. RootSignatureDesc
		5.1 ROOT_PARAMETER
			5.1.1　DESCRIPTOR_RANGEの作成
			5.1.2　ROOT_PARAMETERの作成
			5.1.3　RootSignatureDescの設定
		5.2 Samplerの設定
			5.2.1 D3D12_STATIC_SAMPLER_DESC
			5.2.2 RootSignatureDescの設定
	6. 描画するときの設定
	7. Pixel Shaderの設定
*/

//
// 1. 頂点情報にUV情報を追加する
//
bool CreateVertex () {
	HRESULT hr = 0;

	UVVertex vertices[] = {
		{{-0.5f, -0.9f, 0.0f}, {0.0f, 1.0f}},
		{{-0.5f,  0.9f, 0.0f}, {0.0f, 0.0f}},
		{{ 0.5f, -0.9f, 0.0f}, {1.0f, 1.0f}},
		{{ 0.5f,  0.9f, 0.0f}, {1.0f, 0.0f}},
	};

	//
	// Create Vertex_Buffer_View
	//

	// Heap property

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_UPLOAD;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;

	// Resource description

	D3D12_RESOURCE_DESC vertResDesc = {};
	vertResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	vertResDesc.Width = sizeof (vertices);
	vertResDesc.Height = 1;
	vertResDesc.DepthOrArraySize = 1;
	vertResDesc.MipLevels = 1;
	vertResDesc.Format = DXGI_FORMAT_UNKNOWN;
	vertResDesc.SampleDesc.Count = 1;
	vertResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	vertResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	ID3D12Resource* vertBuff = nullptr;
	hr = _d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&vertResDesc,
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

	//
	// Create Index Resource
	//

	unsigned short indices[] = {
		0, 1, 2,
		2, 1, 3
	};

	vertResDesc.Width = sizeof (indices);

	ID3D12Resource* idxBuff = nullptr;
	hr = _d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&vertResDesc,
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
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = sizeof (indices);

	return true;
}

bool NoiseTexture () {
	//
	// 4. Textureの作成
	//

	// 4.1 データの準備

	vector<TexRGBA> textureData (256 * 256);

	for (auto& rgba : textureData) {
		rgba.R = rand () % 256;
		rgba.G = rand () % 256;
		rgba.B = rand () % 256;
		rgba.A = 255;
	}

	// 4.2 Bufferの作成

	// 4.2.1 Texture Buffer (ID3DResource)

	D3D12_HEAP_PROPERTIES heapProp = {};
	heapProp.Type = D3D12_HEAP_TYPE_CUSTOM;
	heapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	heapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	heapProp.CreationNodeMask = 0;
	heapProp.VisibleNodeMask = 0;

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

	ID3D12Resource* texBuff;
	_d3dDesc.Device->CreateCommittedResource (
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,
		nullptr,
		IID_PPV_ARGS (&texBuff)
	);

	// 4.2.2 Bufferへ生データを転送する

	// CPU側のバッファー
	texBuff->WriteToSubresource (
		0,
		nullptr,
		textureData.data (),
		sizeof (TexRGBA) * 256,
		sizeof (TexRGBA) * textureData.size ()
	);

	//
	// 4.3 ShaderResourceViewの作成 (TextureBuffの引用)
	//

	// 4.3.1 Create Descriptor Heap

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	_d3dDesc.Device->CreateDescriptorHeap (&descHeapDesc, IID_PPV_ARGS (&_texHeap));

	// 4.3.2 Create View

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

	return true;
}

bool ReadTexture () {
	//
	// 4. Textureの作成 (Read Texture)
	//

	// 4.0 Import DirectXTex

	// 4.1 データの準備
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	LoadFromWICFile (L"img/textest.png", WIC_FLAGS_NONE, &metadata, scratchImg);

	const Image* img = scratchImg.GetImage (0, 0, 0);

	// 4.2 Bufferの作成

	// 4.2.1 Texture Buffer (ID3DResource)

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

	// 4.2.2 Bufferへ生データを転送する

	// CPU側のバッファー
	texBuff->WriteToSubresource (0, nullptr, img->pixels, img->rowPitch, img->slicePitch);

	// 4.3 ShaderResourceViewの作成 (TextureBuffの引用)y

	// 4.3.1 Create Descriptor Heap

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	_d3dDesc.Device->CreateDescriptorHeap (&descHeapDesc, IID_PPV_ARGS (&_texHeap2));

	// 4.3.2 Create View

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	_d3dDesc.Device->CreateShaderResourceView (
		texBuff,
		&srvDesc,
		_texHeap2->GetCPUDescriptorHandleForHeapStart ()
	);

	// 4.4 Gammaの補正 (Only Read Texture), in D3DUtility.cpp InitD3D

	return true;
}

/* Create Texture Process 2
	1. 頂点情報にUV情報を追加する
	2. Layoutの設定 (GPUSetting)
	3. Shaderの書き換える (Shader)
	4. Textureの作成
		4.0 Import DirectXTex
		4.1 データの準備
		4.2 Bufferの作成
			4.2.1 Upload Buffer
			4.2.2 Resource Buffer
			4.3.2 Upload Bufferへ生データをMAPする
		4.3 ShaderResourceViewの作成 (TextureBuffの引用)
			4.3.1 Create Descriptor Heap
			4.3.2 Create View
	5. Upload BufferのデータをResource Bufferに転送するためにの設定
		5.1 Copy元の設定
		5.2 Copy先の設定
		5.3 CommandListの設定
	6. 描画するときの設定
	7. Pixel Shaderの設定
*/
bool ReadTexture2 () {
	//
	// 4. Textureの作成 (Read Texture 2)
	//

	// 4.0 Import DirectXTex

	// 4.1 データの準備
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	LoadFromWICFile (L"img/textest.png", WIC_FLAGS_NONE, &metadata, scratchImg);

	const Image* img = scratchImg.GetImage (0, 0, 0);

	// 4.2 Bufferの作成

	// 4.2.1 Upload Buffer

	D3D12_HEAP_PROPERTIES uploadHeapProp = {};
	uploadHeapProp.Type = D3D12_HEAP_TYPE_UPLOAD;;
	uploadHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	uploadHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	uploadHeapProp.CreationNodeMask = 0;
	uploadHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC uploadResDesc = {};
	// 画像のbufferではないので、UNKNOWNだけでいい
	uploadResDesc.Format = DXGI_FORMAT_UNKNOWN;
	uploadResDesc.Width = img->slicePitch;
	uploadResDesc.Height = 1;
	uploadResDesc.DepthOrArraySize = 1;
	uploadResDesc.SampleDesc.Count = 1;
	uploadResDesc.SampleDesc.Quality = 0;
	uploadResDesc.MipLevels = 1;
	uploadResDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	uploadResDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	uploadResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* uploadBuff;
	_d3dDesc.Device->CreateCommittedResource (
		&uploadHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&uploadResDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&uploadBuff)
	);

	// 4.2.2 Resource Buffer

	D3D12_HEAP_PROPERTIES texHeapProp = {};
	texHeapProp.Type = D3D12_HEAP_TYPE_DEFAULT;
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;

	D3D12_RESOURCE_DESC texResDesc = {};
	texResDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texResDesc.Width = metadata.width;
	texResDesc.Height = metadata.height;
	texResDesc.DepthOrArraySize = metadata.arraySize;
	texResDesc.SampleDesc.Count = 1;
	texResDesc.SampleDesc.Quality = 0;
	texResDesc.Format = metadata.format;
	texResDesc.MipLevels = metadata.mipLevels;
	texResDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION> (metadata.dimension);
	texResDesc.Flags = D3D12_RESOURCE_FLAG_NONE;

	ID3D12Resource* texBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&texResDesc,
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS (&texBuff)
	);

	// 4.3.2 Upload Bufferへ生データをMAPする

	uint8_t* mapForImg = nullptr;	// image->pixelsと同じ型にする
	uploadBuff->Map (0, nullptr, (void**)&mapForImg);
	copy_n (img->pixels, img->slicePitch, mapForImg);
	uploadBuff->Unmap (0, nullptr);

	// 4.3 ShaderResourceViewの作成 (TextureBuffの引用)

	// 4.3.1 Create Descriptor Heap

	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};
	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	descHeapDesc.NodeMask = 0;
	descHeapDesc.NumDescriptors = 1;
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;

	ID3D12DescriptorHeap* texDescHeap = nullptr;
	_d3dDesc.Device->CreateDescriptorHeap (&descHeapDesc, IID_PPV_ARGS (&_texHeap3));

	// 4.3.2 Create View

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = metadata.format;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	_d3dDesc.Device->CreateShaderResourceView (texBuff,
											   &srvDesc,
											   _texHeap3->GetCPUDescriptorHandleForHeapStart ()
	);

	//
	// 5. Upload BufferのデータをResource Bufferに転送するためにの設定
	//

	D3D12_TEXTURE_COPY_LOCATION src = {};

	// 5.1 Copy元の設定

	src.pResource = uploadBuff;
	src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
	src.PlacedFootprint.Offset = 0;
	src.PlacedFootprint.Footprint.Width = metadata.width;
	src.PlacedFootprint.Footprint.Height = metadata.height;
	src.PlacedFootprint.Footprint.Depth = metadata.depth;
	src.PlacedFootprint.Footprint.RowPitch = img->rowPitch;
	src.PlacedFootprint.Footprint.Format = img->format;

	D3D12_TEXTURE_COPY_LOCATION dst = {};

	// 5.2 Copy先の設定

	dst.pResource = texBuff;
	dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
	dst.SubresourceIndex = 0;

	// 5.3 CommandListの設定

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

	return true;
}

bool D3DXVertex () {
	UVVertex vertices[] = {
		{{-0.5f, -0.9f, 0.0f}, {0.0f, 1.0f}},
		{{-0.5f,  0.9f, 0.0f}, {0.0f, 0.0f}},
		{{ 0.5f, -0.9f, 0.0f}, {1.0f, 1.0f}},
		{{ 0.5f,  0.9f, 0.0f}, {1.0f, 0.0f}},
	};

	ID3D12Resource* vertBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (sizeof (vertices)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&vertBuff)
	);

	UVVertex* vertMap = nullptr;
	vertBuff->Map (0, nullptr, (void**)&vertMap);
	copy (begin (vertices), end (vertices), vertMap);
	vertBuff->Unmap (0, nullptr);

	_vbView.BufferLocation = vertBuff->GetGPUVirtualAddress ();
	_vbView.SizeInBytes = sizeof (vertices);
	_vbView.StrideInBytes = sizeof (vertices[0]);

	//
	// Create Index Resource
	//

	unsigned short indices[] = {
		0, 1, 2,
		2, 1, 3
	};

	ID3D12Resource* idxBuff = nullptr;
	_d3dDesc.Device->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (sizeof (indices)),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (&idxBuff)
	);

	unsigned short* idxMap = nullptr;
	idxBuff->Map (0, nullptr, (void**)&idxMap);
	copy (begin (indices), end (indices), idxMap);
	idxBuff->Unmap (0, nullptr);

	_ibView.BufferLocation = idxBuff->GetGPUVirtualAddress ();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = sizeof (indices);

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

	//
	// 2. 頂点のLayoutの設定 (GPUSetting)
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

	//
	// 	5. RootSignatureDesc
	//

	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// 5.1 ROOT_PARAMETER

	// 5.1.1　DESCRIPTOR_RANGEの作成

	D3D12_DESCRIPTOR_RANGE descRange = {};
	descRange.NumDescriptors = 1;
	descRange.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	descRange.BaseShaderRegister = 0;
	descRange.OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	// 5.1.2　ROOT_PARAMETERの作成

	D3D12_ROOT_PARAMETER rootParam = {};
	rootParam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParam.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;
	rootParam.DescriptorTable.pDescriptorRanges = &descRange;
	rootParam.DescriptorTable.NumDescriptorRanges = 1;

	// 5.1.3　RootSignatureDescの設定

	rootSignatureDesc.pParameters = &rootParam;
	rootSignatureDesc.NumParameters = 1;

	// 5.2 Samplerの設定

	// 5.2.1 D3D12_STATIC_SAMPLER_DESC

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

	// 5.2.2 RootSignatureDescの設定

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;

	// ---------------------------------

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

	/*if (!CreateVertex ()) {
		printf_s ("ERROR : CreateVertex() - FAILED");
		return false;
	}*/

	if (!D3DXVertex ()) {
		printf_s ("ERROR : D3DXVertex() - FAILED");
		return false;
	}

	NoiseTexture ();

	ReadTexture ();

	ReadTexture2 ();

	if (!GPUSetting ()) {
		printf_s ("ERROR : GPUSetting() - FAILED");
		return false;
	}

	return true;
}

int main () {
	srand ((unsigned)time (NULL));

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