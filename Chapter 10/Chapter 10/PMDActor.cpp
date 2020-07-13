#include "PMDActor.h"
#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include "D3DUtility.h"

#include <d3dx12.h>

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

namespace {
	string GetExtension (const string& path) {
		int idx = path.rfind ('.');
		return path.substr (idx + 1, path.length () - idx - 1);
	}

	pair<string, string> SplitFileName (const string& path, const char splitter = '*') {
		int idx = path.find (splitter);
		pair<string, string> ret;
		ret.first = path.substr (0, idx);
		ret.second = path.substr (idx + 1, path.length () - idx - 1);
		return ret;
	}

	string GetTexturePathFromModelAndTexPath (const string& modelPath, const char* texPath) {
		int pathIndex1 = modelPath.rfind ('/');
		int pathIndex2 = modelPath.rfind ('\\');
		auto pathIndex = max (pathIndex1, pathIndex2);
		auto folderPath = modelPath.substr (0, pathIndex + 1);
		return folderPath + texPath;
	}
}

PMDActor::PMDActor (const char* filePath, PMDRenderer& renderer) : _renderer (renderer), _dx12 (_renderer._dx12), _angle (0.0f) {
	_transform.world = XMMatrixIdentity ();
	LoadPMDFile (filePath);
	CreateTransformView ();
	CreateMaterialData ();
	CreateMaterialAndTextureView ();
}

PMDActor::~PMDActor () {}

HRESULT PMDActor::LoadPMDFile (const char* path) {
	// set alignment 1 byte
	#pragma pack(1)
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

	struct PMDHeader {
		float version;
		char model_name[20];
		char comment[256];
	};

	PMDHeader pmdHeader = {};

	char signature[3] = {};
	auto fp = fopen (path, "rb");

	fread (signature, sizeof (signature), 1, fp);
	fread (&pmdHeader, sizeof (PMDHeader), 1, fp);

	// PMD Vertex
	constexpr size_t PMDVERTEX_SIZE = 38;

	unsigned int vertNum;
	fread (&vertNum, sizeof (vertNum), 1, fp);
	//  char(8bits) of _vertNum * 38 amount
	vector<unsigned char> vertices (vertNum* PMDVERTEX_SIZE);
	fread (vertices.data (), vertices.size (), 1, fp);
	
	unsigned int idxNum;
	fread (&idxNum, sizeof (idxNum), 1, fp);
	vector<unsigned short> indices (idxNum);
	fread (indices.data (), indices.size () * sizeof (indices[0]), 1, fp);

	unsigned int materialNum;
	fread (&materialNum, sizeof (materialNum), 1, fp);

	vector<PMDMaterial> pmdMaterials (materialNum);
	fread (pmdMaterials.data (), pmdMaterials.size () * sizeof (PMDMaterial), 1, fp);

	fclose (fp);

	//
	// Vertex
	//
	auto hr = _dx12.Device ()->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (vertices.size ()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (_vb.ReleaseAndGetAddressOf ())
	);

	unsigned char* vertMap = nullptr;
	hr = _vb->Map (0, nullptr, (void**)&vertMap);
	copy (begin (vertices), end (vertices), vertMap);
	_vb->Unmap (0, nullptr);

	_vbView.BufferLocation = _vb->GetGPUVirtualAddress ();
	_vbView.SizeInBytes = vertices.size ();
	_vbView.StrideInBytes = PMDVERTEX_SIZE;

	//
	// Index of vertex
	//
	hr = _dx12.Device ()->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (indices.size () * sizeof (indices[0])),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (_ib.ReleaseAndGetAddressOf ())
	);

	unsigned short* idxMap = nullptr;
	hr = _ib->Map (0, nullptr, (void**)&idxMap);
	copy (begin (indices), end (indices), idxMap);
	_ib->Unmap (0, nullptr);

	_ibView.BufferLocation = _ib->GetGPUVirtualAddress ();
	_ibView.Format = DXGI_FORMAT_R16_UINT;
	_ibView.SizeInBytes = indices.size () * sizeof (indices[0]);

	//
	// Material
	//
	_materials = vector<Material> (pmdMaterials.size ());
	for (int i = 0; i < pmdMaterials.size (); ++i) {
		_materials[i].indicesNum = pmdMaterials[i].indicesNum;
		_materials[i].material.diffuse = pmdMaterials[i].diffuse;
		_materials[i].material.alpha = pmdMaterials[i].alpha;
		_materials[i].material.specular = pmdMaterials[i].specular;
		_materials[i].material.specularity = pmdMaterials[i].specularity;
		_materials[i].material.ambient = pmdMaterials[i].ambient;
		_materials[i].additional.toonIdx = pmdMaterials[i].toonIdx;
	}

	//
	// Texture & Sph & Spa
	//
	_materials.resize (materialNum);
	_textureResources.resize (materialNum);
	_sphResources.resize (materialNum);
	_spaResources.resize (materialNum);
	_toonResources.resize (materialNum);
	for (int i = 0; i < pmdMaterials.size (); i++) {
		char toonFilePath[32];
		sprintf (toonFilePath, "toon/toon%02d.bmp", pmdMaterials[i].toonIdx + 1);
		_toonResources[i] = _dx12.GetTextureByPath (toonFilePath);

		if (strlen (pmdMaterials[i].texFilePath) == 0) {
			_textureResources[i] = nullptr;
			continue;
		}

		string texFileName = pmdMaterials[i].texFilePath;
		string sphFileName = "";
		string spaFileName = "";

		if (count (texFileName.begin (), texFileName.end (), '*') > 0) {
			auto namepair = SplitFileName (texFileName);
			if (GetExtension (namepair.first) == "sph") {
				texFileName = namepair.second;
				sphFileName = namepair.first;
			} else if (GetExtension (namepair.first) == "spa") {
				texFileName = namepair.second;
				spaFileName = namepair.first;
			} else {
				texFileName = namepair.first;
				if (GetExtension (namepair.second) == "sph") {
					sphFileName = namepair.second;
				} else if (GetExtension (namepair.second) == "spa") {
					spaFileName = namepair.second;
				}
			}
		} else {
			if (GetExtension (pmdMaterials[i].texFilePath) == "sph") {
				sphFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			} else if (GetExtension (pmdMaterials[i].texFilePath) == "spa") {
				spaFileName = pmdMaterials[i].texFilePath;
				texFileName = "";
			} else {
				texFileName = pmdMaterials[i].texFilePath;
			}
		}

		if (texFileName != "") {
			auto texFilePath = GetTexturePathFromModelAndTexPath (path, texFileName.c_str ());
			_textureResources[i] = _dx12.GetTextureByPath (texFilePath.c_str ());
		}
		if (sphFileName != "") {
			auto sphFilePath = GetTexturePathFromModelAndTexPath (path, sphFileName.c_str ());
			_sphResources[i] = _dx12.GetTextureByPath (sphFilePath.c_str ());
		}
		if (spaFileName != "") {
			auto spaFilePath = GetTexturePathFromModelAndTexPath (path, spaFileName.c_str ());
			_spaResources[i] = _dx12.GetTextureByPath (spaFilePath.c_str ());
		}
	}

	return S_OK;
}

HRESULT PMDActor::CreateTransformView () {
	auto buffSize = D3D::Get256Times (sizeof (Transform));
	auto hr = _dx12.Device ()->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (buffSize),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (_transformBuffer.ReleaseAndGetAddressOf ())
	);
	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return hr;
	}

	hr = _transformBuffer->Map (0, nullptr, (void**)&_mappedTransform);
	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return hr;
	}
	*_mappedTransform = _transform;
	_transformBuffer->Unmap (0, nullptr);

	D3D12_DESCRIPTOR_HEAP_DESC transformDescHeapDesc = {};
	transformDescHeapDesc.NumDescriptors = 1;
	transformDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	transformDescHeapDesc.NodeMask = 0;

	transformDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	hr = _dx12.Device ()->CreateDescriptorHeap (&transformDescHeapDesc, IID_PPV_ARGS (_transformHeap.ReleaseAndGetAddressOf ()));
	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return hr;
	}

	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
	cbvDesc.BufferLocation = _transformBuffer->GetGPUVirtualAddress ();
	cbvDesc.SizeInBytes = buffSize;
	_dx12.Device ()->CreateConstantBufferView (&cbvDesc, _transformHeap->GetCPUDescriptorHandleForHeapStart ());

	return S_OK;
}

HRESULT PMDActor::CreateMaterialData () {
	unsigned int materialBufferSize = D3D::Get256Times (sizeof (MaterialForHlsl));
	auto hr = _dx12.Device ()->CreateCommittedResource (
		&CD3DX12_HEAP_PROPERTIES (D3D12_HEAP_TYPE_UPLOAD),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer (materialBufferSize * _materials.size ()),
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS (_materialBuffer.ReleaseAndGetAddressOf ())
	);

	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return hr;
	}

	char* mapMaterial = nullptr;
	hr = _materialBuffer->Map (0, nullptr, (void**)&mapMaterial);
	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return hr;
	}

	for (auto& m : _materials) {
		*((MaterialForHlsl*)mapMaterial) = m.material;
		mapMaterial += materialBufferSize;
	}
	_materialBuffer->Unmap (0, nullptr);

	return S_OK;
}

HRESULT PMDActor::CreateMaterialAndTextureView () {
	D3D12_DESCRIPTOR_HEAP_DESC materialDescHeapDesc = {};
	materialDescHeapDesc.NumDescriptors = _materials.size () * 5;
	materialDescHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	materialDescHeapDesc.NodeMask = 0;

	materialDescHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	auto hr = _dx12.Device ()->CreateDescriptorHeap (&materialDescHeapDesc, IID_PPV_ARGS (_materialHeap.ReleaseAndGetAddressOf ()));//Éú³É
	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return hr;
	}

	auto materialBufferSize = D3D::Get256Times(sizeof (MaterialForHlsl));

	D3D12_CONSTANT_BUFFER_VIEW_DESC matCBVDesc = {};
	matCBVDesc.BufferLocation = _materialBuffer->GetGPUVirtualAddress ();
	matCBVDesc.SizeInBytes = materialBufferSize;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = 1;

	CD3DX12_CPU_DESCRIPTOR_HANDLE matDescHeapH (_materialHeap->GetCPUDescriptorHandleForHeapStart ());
	auto inc = _dx12.Device ()->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	for (int i = 0; i < _materials.size (); ++i) {
		_dx12.Device ()->CreateConstantBufferView (&matCBVDesc, matDescHeapH);
		matDescHeapH.ptr += inc;
		matCBVDesc.BufferLocation += materialBufferSize;

		if (_textureResources[i] == nullptr) {
			srvDesc.Format = _renderer._whiteTex->GetDesc ().Format;
			_dx12.Device ()->CreateShaderResourceView (_renderer._whiteTex.Get (), &srvDesc, matDescHeapH);
		} else {
			srvDesc.Format = _textureResources[i]->GetDesc ().Format;
			_dx12.Device ()->CreateShaderResourceView (_textureResources[i].Get (), &srvDesc, matDescHeapH);
		}
		matDescHeapH.Offset (inc);

		if (_sphResources[i] == nullptr) {
			srvDesc.Format = _renderer._whiteTex->GetDesc ().Format;
			_dx12.Device ()->CreateShaderResourceView (_renderer._whiteTex.Get (), &srvDesc, matDescHeapH);
		} else {
			srvDesc.Format = _sphResources[i]->GetDesc ().Format;
			_dx12.Device ()->CreateShaderResourceView (_sphResources[i].Get (), &srvDesc, matDescHeapH);
		}
		matDescHeapH.Offset (inc);

		if (_spaResources[i] == nullptr) {
			srvDesc.Format = _renderer._blackTex->GetDesc ().Format;
			_dx12.Device ()->CreateShaderResourceView (_renderer._blackTex.Get (), &srvDesc, matDescHeapH);
		} else {
			srvDesc.Format = _spaResources[i]->GetDesc ().Format;
			_dx12.Device ()->CreateShaderResourceView (_spaResources[i].Get (), &srvDesc, matDescHeapH);
		}
		matDescHeapH.Offset (inc);

		if (_toonResources[i] == nullptr) {
			srvDesc.Format = _renderer._gradTex->GetDesc ().Format;
			_dx12.Device ()->CreateShaderResourceView (_renderer._gradTex.Get (), &srvDesc, matDescHeapH);
		} else {
			srvDesc.Format = _toonResources[i]->GetDesc ().Format;
			_dx12.Device ()->CreateShaderResourceView (_toonResources[i].Get (), &srvDesc, matDescHeapH);
		}
		matDescHeapH.ptr += inc;
	}

	return S_OK;
}

void PMDActor::Update () {
	_angle += 0.03f;
	_mappedTransform->world = XMMatrixRotationY (_angle);
}

void PMDActor::Draw () {
	_dx12.CommandList ()->IASetVertexBuffers (0, 1, &_vbView);
	_dx12.CommandList ()->IASetIndexBuffer (&_ibView);

	ID3D12DescriptorHeap* transheaps[] = {_transformHeap.Get ()};
	_dx12.CommandList ()->SetDescriptorHeaps (1, transheaps);
	_dx12.CommandList ()->SetGraphicsRootDescriptorTable (1, _transformHeap->GetGPUDescriptorHandleForHeapStart ());

	ID3D12DescriptorHeap* mdh[] = {_materialHeap.Get ()};
	
	_dx12.CommandList ()->SetDescriptorHeaps (1, mdh);

	auto materialH = _materialHeap->GetGPUDescriptorHandleForHeapStart ();
	unsigned int idxOffset = 0;

	auto cbvsrvIncSize = _dx12.Device ()->GetDescriptorHandleIncrementSize (D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV) * 5;
	for (auto& m : _materials) {
		_dx12.CommandList ()->SetGraphicsRootDescriptorTable (2, materialH);
		_dx12.CommandList ()->DrawIndexedInstanced (m.indicesNum, 1, idxOffset, 0, 0);
		materialH.ptr += cbvsrvIncSize;
		idxOffset += m.indicesNum;
	}
}