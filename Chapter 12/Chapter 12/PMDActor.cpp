#include "PMDActor.h"
#include "PMDRenderer.h"
#include "Dx12Wrapper.h"
#include "D3DUtility.h"

#include <sstream>
#include <iostream>
#include <d3dx12.h>
#include <array>

using namespace Microsoft::WRL;
using namespace std;
using namespace DirectX;

constexpr float epsilon = 0.0005f;

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

	// turn z-axis to lookAt
	XMMATRIX LookAtMatrix (const XMVECTOR& lookAt, XMFLOAT3& up, XMFLOAT3& right) {
		XMVECTOR vz = lookAt;
		XMVECTOR vy = XMVector3Normalize (XMLoadFloat3 (&up));
		XMVECTOR vx = XMVector3Normalize (XMVector3Cross (vy, vz));
		vy = XMVector3Normalize (XMVector3Cross (vz, vx));

		// LookAtとupが同じ方向を向いてたらright基準で作り直す
		if (abs (XMVector3Dot (vy, vz).m128_f32[0]) == 1.0f) {
			vx = XMVector3Normalize (XMLoadFloat3 (&right));
			vy = XMVector3Normalize (XMVector3Cross (vz, vx));
			vx = XMVector3Normalize (XMVector3Cross (vy, vz));
		}

		XMMATRIX ret = XMMatrixIdentity ();
		ret.r[0] = vx;
		ret.r[1] = vy;
		ret.r[2] = vz;
		return ret;
	}

	XMMATRIX LookAtMatrix (const XMVECTOR& origin, const XMVECTOR& lookAt, XMFLOAT3& up, XMFLOAT3& right) {
		return XMMatrixTranspose (LookAtMatrix (origin, up, right)) * LookAtMatrix (lookAt, up, right);
	}

	enum class BoneType {
		Rotation,
		RotAndMove,
		IK,
		Undefined,
		IKChild,
		RotationChild,
		IKDestination,
		Invisible
	};
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

	#pragma pack(1)
	struct PMDBone {
		char boneName[20];
		unsigned short parentNo;
		unsigned short nextNo;
		unsigned char type;
		unsigned short ikBoneNo;
		XMFLOAT3 pos;
	};
	#pragma pack()

	PMDHeader pmdHeader = {};

	char signature[3] = {};
	auto fp = fopen (path, "rb");
	if (fp == nullptr) {
		assert (0);
		return ERROR_FILE_NOT_FOUND;
	}

	fread (signature, sizeof (signature), 1, fp);
	fread (&pmdHeader, sizeof (PMDHeader), 1, fp);

	// PMD Vertex
	constexpr size_t PMDVERTEX_SIZE = 38;

	unsigned int vertNum;
	fread (&vertNum, sizeof (vertNum), 1, fp);
	// printf_s ("Vertex Count: %i\n", vertNum);
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

	// bone
	unsigned short boneNum = 0;
	fread (&boneNum, sizeof (boneNum), 1, fp);
	// printf_s ("Bone Count: %i\n", boneNum);

	vector<PMDBone> pmdBones(boneNum);
	fread (pmdBones.data (), sizeof (PMDBone), boneNum, fp);
	// break point: can't display japanese, but can printf
	// printf_s ("BoneName: %s\n", pmdBones[0].boneName);

	//
	// IK Load
	//
	uint16_t ikNum = 0;
	fread (&ikNum, sizeof (ikNum), 1, fp);

	_ikData.resize (ikNum);
	for (auto& ik : _ikData) {
		fread (&ik.boneIdx, sizeof (ik.boneIdx), 1, fp);
		fread (&ik.targetIdx, sizeof (ik.targetIdx), 1, fp);

		uint8_t chainLen = 0;
		fread (&chainLen, sizeof (chainLen), 1, fp);

		ik.nodeIdxes.resize (chainLen);
		fread (&ik.iterations, sizeof (ik.iterations), 1, fp);
		fread (&ik.limit, sizeof (ik.limit), 1, fp);

		if (chainLen == 0) {
			continue;
		}

		fread (ik.nodeIdxes.data (), sizeof (ik.nodeIdxes[0]), chainLen, fp);
	}

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

	//
	// Bone
	//
	_kneeIdxes.clear ();
	_boneNameArray.resize (pmdBones.size ());
	_boneNodeAddressArray.resize (pmdBones.size ());
	for (int i = 0; i < pmdBones.size (); i++) {
		auto& pb = pmdBones[i];
		auto& node = _boneNodeTable[pb.boneName];
		node.boneIdx = i;
		node.startPos = pb.pos;
		node.boneType = pb.type;
		node.parentBone = pb.parentNo;
		node.ikParentBone = pb.ikBoneNo;

		string boneName = pb.boneName;

		_boneNameArray[i] = boneName;
		_boneNodeAddressArray[i] = &node;

		// the index of the knee of the bone
		if (boneName.find ("ひざ") != string::npos) {
			_kneeIdxes.emplace_back (i);
		}
	}

	for (auto& pb : pmdBones) {
		if (pb.parentNo >= pmdBones.size ()) {
			continue;
		}
		
		auto parentName = _boneNameArray[pb.parentNo];
		_boneNodeTable[parentName].children.emplace_back (&_boneNodeTable[pb.boneName]);
	}

	// Bone matrix
	_boneMatrices.resize (pmdBones.size ());
	std::fill (_boneMatrices.begin (), _boneMatrices.end (), XMMatrixIdentity ());		// init

#pragma region Animation Debug
	//auto node = _boneNodeTable["左腕"];
	//// Can read bone
	////printf_s ("Bone Index: %i\n", node.boneIdx);
	////printf_s ("Bone children: %i\n", node.children[0]->boneIdx);
	//auto& pos = node.startPos;
	//// 原理は Scale * Rotation * Positionのように同じだ
	//// 回転するとき、原点はBONEの頂点の位置ではなく、まず原点に移動させて、そして回転させて、最後元の位置に戻す。
	//auto armMat = XMMatrixTranslation (-pos.x, -pos.y, -pos.z) 
	//			* XMMatrixRotationZ (XM_PIDIV2)
	//			* XMMatrixTranslation (pos.x, pos.y, pos.z);
	// RecursiveMatrixMultipy (&node, armMat);
	//_boneMatrices [node.boneIdx] = armMat;

	//node = _boneNodeTable["左ひじ];
	//pos = node.startPos;
	//auto elbowMat = XMMatrixTranslation (-pos.x, -pos.y, -pos.z)
	//	* XMMatrixRotationZ (-XM_PIDIV2)
	//	* XMMatrixTranslation (pos.x, pos.y, pos.z);
	//RecursiveMatrixMultipy (&node, elbowMat);
	//_boneMatrices[node.boneIdx] = elbowMat;

	//XMMATRIX identity = XMMatrixIdentity ();
	//RecursiveMatrixMultipy (&_boneNodeTable["･ｻ･ｿｩ`"], identity);
	// copy (_boneMatrices.begin (), _boneMatrices.end (), _mappedMatrices + 1);
#pragma endregion Animation Debug

	//
	// IK Debug
	//
	auto getNameFromIdx = [&](uint16_t idx)->string {
		auto it = find_if (_boneNodeTable.begin (), _boneNodeTable.end (), [idx](const pair<string, BoneNode>& obj) {
			return obj.second.boneIdx == idx;
		});

		if (it != _boneNodeTable.end ()) {
			return it->first;
		} else {
			return "";
		}
	};

	for (auto& ik : _ikData) {
		ostringstream oss;

		auto& node = _boneNodeAddressArray[ik.boneIdx];	
		oss << "IKボーン番号 = " << ik.boneIdx << ":" << getNameFromIdx (ik.boneIdx) << ", ";
		D3D::PrintfFloat3 (oss, "", node->startPos, "\n");

		node = _boneNodeAddressArray[ik.targetIdx];
		oss << "IKターゲット番号 = " << ik.targetIdx << ":" << getNameFromIdx (ik.targetIdx) << ", ";
		D3D::PrintfFloat3 (oss, "", node->startPos, "\n");

		for (auto& node : ik.nodeIdxes) {
			auto& n = _boneNodeAddressArray[node];
			oss << "\tノードボーン = " << node << ":" << getNameFromIdx (node) << ", ";
			D3D::PrintfFloat3 (oss, "", n->startPos, "\n");
		}

		//OutputDebugString (oss.str().c_str ());
		cout << oss.str () << endl;
	}

	return S_OK;
}

void PMDActor::LoadVMDFile (const char* filePath, const char* name) {
	auto fp = fopen (filePath, "rb");
	fseek (fp, 50, SEEK_SET);

	// keyframe count
	unsigned int vmdMotionNum = 0;
	fread (&vmdMotionNum, sizeof (vmdMotionNum), 1, fp);

	struct VMDMotion {
		char boneName [15];

		// アライメント

		unsigned int frameNo;
		XMFLOAT3 location;
		XMFLOAT4 quaternion;
		unsigned char bezier[64];
	};

	// Load vmd data.
	vector<VMDMotion> vmdMotions (vmdMotionNum);
	for (auto& motion : vmdMotions) {
		fread (motion.boneName, sizeof (motion.boneName), 1, fp);
		fread (&motion.frameNo, sizeof (motion.frameNo)
							+ sizeof (motion.location)
							+ sizeof (motion.quaternion)
							+ sizeof (motion.bezier)
							, 1, fp);
	}

#pragma pack(1)
	//表情データ(頂点モーフデータ)
	struct VMDMorph {
		char name[15];
		uint32_t frameNo;
		float weight;
	};
#pragma pack()
	
	uint32_t morphCount = 0;
	fread (&morphCount, sizeof (morphCount), 1, fp);
	vector<VMDMorph> morphs (morphCount);
	fread (morphs.data (), sizeof (VMDMorph), morphCount, fp);

#pragma pack(1)
	//カメラ
	struct VMDCamera {
		uint32_t frameNo;
		float distance;
		XMFLOAT3 pos;
		XMFLOAT3 eulerAngle;
		uint8_t Interpolation[24];
		uint32_t fov;
		uint8_t persFlg;
	};
#pragma pack()

	uint32_t vmdCameraCount = 0;
	fread (&vmdCameraCount, sizeof (vmdCameraCount), 1, fp);
	vector<VMDCamera> cameraData (vmdCameraCount);
	fread (cameraData. data(), sizeof (VMDCamera), vmdCameraCount, fp);

	// ライト照明データ
	struct VMDLight {
		uint32_t frameNo;
		XMFLOAT3 rgb;
		XMFLOAT3 vec;
	};

	uint32_t vmdLightCount = 0;
	fread (&vmdLightCount, sizeof (vmdLightCount), 1, fp);
	vector<VMDLight> lights (vmdLightCount);
	fread (lights.data(), sizeof (VMDLight), vmdLightCount, fp);

#pragma pack(1)
	// セルフ影データ
	struct VMDSelfShadow {
		uint32_t frameNo;
		uint8_t mode;
		float distance;
	};
#pragma pack()

	uint32_t selfShadowCount = 0;
	fread (&selfShadowCount, sizeof (selfShadowCount), 1, fp);
	vector<VMDSelfShadow> selfShadowData (selfShadowCount);
	fread (selfShadowData.data (), sizeof (VMDSelfShadow), selfShadowCount, fp);

	uint32_t ikSwitchCount = 0;
	fread (&ikSwitchCount, sizeof (ikSwitchCount), 1, fp);

	_ikEnableData.resize (ikSwitchCount);
	for (auto& ikEnable : _ikEnableData) {
		fread (&ikEnable.frameNo, sizeof (ikEnable.frameNo), 1, fp);

		uint8_t visibleFlg = 0;
		fread (&visibleFlg, sizeof (visibleFlg), 1, fp);

		uint32_t ikBoneCount = 0;
		fread (&ikBoneCount, sizeof (ikBoneCount), 1, fp);

		for (int i = 0; i < ikBoneCount; i++) {
			char ikBoneName[20];
			fread (ikBoneName, _countof (ikBoneName), 1, fp);

			uint8_t flg = 0;
			fread (&flg, sizeof (flg), 1, fp);
			ikEnable.ikEnableTable[ikBoneName] = flg;
		}
	}
	fclose (fp);

	// Change to use data.
	for (auto& vmdMotion : vmdMotions) {
		XMVECTOR quaternion = XMLoadFloat4 (&vmdMotion.quaternion);
		_keyFrameDatas[vmdMotion.boneName].emplace_back (KeyFrame (vmdMotion.frameNo, quaternion, vmdMotion.location
																	, XMFLOAT2 ((float)vmdMotion.bezier[3] / 127.0f, (float)vmdMotion.bezier[7] / 127.0f)
																	, XMFLOAT2 ((float)vmdMotion.bezier[11] / 127.0f, (float)vmdMotion.bezier[15] / 127.0f))
																	);
		_duration = std::max<unsigned int> (_duration, vmdMotion.frameNo);
	}
	// printf_s ("Max FrameNo : %i\n", _duration);

	// sort
	for (auto& keyFrame : _keyFrameDatas) {
		sort (keyFrame.second.begin (), keyFrame.second.end (), [](const KeyFrame& l, const KeyFrame& r) {
			return l.frameNo <= r.frameNo;
		});
	}

	// calculate bone transformation
	for (auto& keyFrame : _keyFrameDatas) {
		// get bone node
		auto itBoneNode = _boneNodeTable.find (keyFrame.first);
		if (itBoneNode == _boneNodeTable.end ()) {
			continue;
		}
		auto node = itBoneNode->second;
		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation (-pos.x, -pos.y, -pos.z)
					* XMMatrixRotationQuaternion (keyFrame.second[0].quaternion)
					* XMMatrixTranslation (pos.x, pos.y, pos.z);
		_boneMatrices[node.boneIdx] = mat;
	}

	XMMATRIX mat = XMMatrixIdentity ();
	RecursiveMatrixMultipy (&_boneNodeTable["センター"], mat);
	copy (_boneMatrices.begin (), _boneMatrices.end (), _mappedMatrices + 1);
}

void PMDActor::PlayAnimation () {
	_startTime = timeGetTime ();
}

void PMDActor::MotionUpdate () {
	auto elapsedTime = timeGetTime() - _startTime;
	// 30 frame per second
	unsigned int frameNo = 30 * (elapsedTime / 1000.0f);

	if (frameNo > _duration) {
		_startTime = timeGetTime ();
		frameNo = 0;
	}

	fill (_boneMatrices.begin (), _boneMatrices.end (), XMMatrixIdentity ());
	
	for (auto& keyFrame : _keyFrameDatas) {
		auto& boneName = keyFrame.first;
		auto itBoneNode = _boneNodeTable.find (boneName);
		if (itBoneNode == _boneNodeTable.end ()) {
			continue;
		}
		auto node = itBoneNode->second;


		auto motions = keyFrame.second;
		auto rit = find_if (motions.rbegin (), motions.rend (), [frameNo](const KeyFrame& motion) {
			return motion.frameNo <= frameNo;
		});

		if (rit == motions.rend ()) {
			continue;
		}

		XMMATRIX rotation = XMMatrixIdentity ();
		XMVECTOR offset = XMLoadFloat3 (&rit->offset);
		auto it = rit.base ();
		if (it != motions.end ()) {
			auto t = static_cast<float> (frameNo - rit->frameNo) / static_cast <float> (it->frameNo - rit->frameNo);
			t = GetYFromXOnBezier (t, it->p1, it->p2, 12);
			rotation = XMMatrixRotationQuaternion (XMQuaternionSlerp (rit->quaternion, it->quaternion, t));
			offset = XMVectorLerp (offset, XMLoadFloat3 (&it->offset), t);
		} else {
			rotation = XMMatrixRotationQuaternion (rit->quaternion);
		}

		auto& pos = node.startPos;
		auto mat = XMMatrixTranslation (-pos.x, -pos.y, -pos.z)
					* rotation
					* XMMatrixTranslation (pos.x, pos.y, pos.z);
		_boneMatrices [node.boneIdx] = mat * XMMatrixTranslationFromVector (offset);
	}
	XMMATRIX mat = XMMatrixIdentity ();
	RecursiveMatrixMultipy (&_boneNodeTable ["センター"], mat);
	
	IKSolve (frameNo);

	copy (_boneMatrices.begin (), _boneMatrices.end (), _mappedMatrices + 1);
}

HRESULT PMDActor::CreateTransformView () {
	auto buffSize = D3D::Get256Times (sizeof (XMMATRIX) * (1 + _boneMatrices.size ()));
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

	hr = _transformBuffer->Map (0, nullptr, (void**)&_mappedMatrices);
	if (FAILED (hr)) {
		assert (SUCCEEDED (hr));
		return hr;
	}
	_mappedMatrices[0] = _transform.world;
	copy (_boneMatrices.begin (), _boneMatrices.end (), _mappedMatrices + 1);
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
	auto hr = _dx12.Device ()->CreateDescriptorHeap (&materialDescHeapDesc, IID_PPV_ARGS (_materialHeap.ReleaseAndGetAddressOf ()));//ﾉ嵭ﾉ
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
	_angle += 0.001f;
	_mappedMatrices[0] = XMMatrixRotationY (_angle);

	MotionUpdate ();
}

void PMDActor::Draw () {
	_dx12.CommandList ()->IASetVertexBuffers (0, 1, &_vbView);
	_dx12.CommandList ()->IASetIndexBuffer (&_ibView);
	_dx12.CommandList ()->IASetPrimitiveTopology (D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

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

void PMDActor::RecursiveMatrixMultipy (BoneNode* node, const XMMATRIX& mat, bool flg) {
	_boneMatrices[node->boneIdx] *= mat;
	for (auto& cnode : node->children) {
		XMMATRIX m = _boneMatrices[node->boneIdx];
		RecursiveMatrixMultipy (cnode, m);
	}
}

float PMDActor::GetYFromXOnBezier (float x, const XMFLOAT2& a, const XMFLOAT2& b, uint8_t n) {
	if (a.x == a.y && b.x == b.y)
		return x;

	float t = x;
	const float k0 = 1 + 3 * a.x - 3 * b.x;
	const float k1 = 3 * b.x - 6 * a.x;
	const float k2 = 3 * a.x;

	for (int i = 0; i < n; ++i) {
		auto ft = k0 * t * t * t + k1 * t * t + k2 * t - x;
		if (ft <= epsilon && ft >= -epsilon)
			break;

		t -= ft / 2;
	}

	auto r = 1 - t;
	return t * t * t + 3 * t * t * r * b.y + 3 * t * r * r * a.y;
}

void PMDActor::SolveCCDIK (const PMDIK& ik) {
	auto targetBoneNode = _boneNodeAddressArray[ik.boneIdx];
	auto targetOriginPos = XMLoadFloat3 (&targetBoneNode->startPos);

	auto parentMat = _boneMatrices[_boneNodeAddressArray[ik.boneIdx]->ikParentBone];

	XMVECTOR det;
	auto invParentMat = XMMatrixInverse (&det, parentMat);
	auto targetNextPos = XMVector3Transform (targetOriginPos, _boneMatrices[ik.boneIdx] * invParentMat);

	auto endPos = XMLoadFloat3 (&_boneNodeAddressArray[ik.targetIdx]->startPos);
	
	vector<XMVECTOR> bonePositions;
	for (auto& cidx : ik.nodeIdxes) {
		bonePositions.push_back (XMLoadFloat3 (&_boneNodeAddressArray[cidx]->startPos));
	}

	vector<XMMATRIX> mats (bonePositions.size ());
	fill (mats.begin (), mats.end (), XMMatrixIdentity ());

	auto ikLimit = ik.limit * XM_PI;

	for (int c = 0; c < ik.iterations; ++c) {
		if (XMVector3Length (XMVectorSubtract (endPos, targetNextPos)).m128_f32[0] <= epsilon) {
			break;
		}

		for (int bidx = 0; bidx < bonePositions.size (); ++bidx) {
			const auto& pos = bonePositions[bidx];

			auto vecToEnd = XMVectorSubtract (endPos, pos);
			auto vecToTarget = XMVectorSubtract (targetNextPos, pos);

			vecToEnd = XMVector3Normalize (vecToEnd);
			vecToTarget = XMVector3Normalize (vecToTarget);
	
			if (XMVector3Length (XMVectorSubtract (vecToEnd, vecToTarget)).m128_f32[0] <= epsilon) {
				continue;
			}

			auto cross = XMVector3Normalize (XMVector3Cross (vecToEnd, vecToTarget));

			float angle = XMVector3AngleBetweenVectors (vecToEnd, vecToTarget).m128_f32[0];

			angle = min (angle, ikLimit);
		
			XMMATRIX rot = XMMatrixRotationAxis (cross, angle);

			auto mat = XMMatrixTranslationFromVector (-pos) * rot * XMMatrixTranslationFromVector (pos);
			mats[bidx] *= mat;

			for (auto idx = bidx - 1; idx >= 0; --idx) {
				bonePositions[idx] = XMVector3Transform (bonePositions[idx], mat);
			}
		
			endPos = XMVector3Transform (endPos, mat);

			if (XMVector3Length (XMVectorSubtract (endPos, targetNextPos)).m128_f32[0] <= epsilon) {
				break;
			}
		}
	}
	int idx = 0;
	for (auto& cidx : ik.nodeIdxes) {
		_boneMatrices[cidx] = mats[idx];
		++idx;
	}
	auto node = _boneNodeAddressArray[ik.nodeIdxes.back ()];
	RecursiveMatrixMultipy (node, parentMat, true);
}

void PMDActor::SolveCosineIK (const PMDIK& ik) {
	vector<XMVECTOR> positions;
	array<float, 2> edgeLens;

	auto& targetNode = _boneNodeAddressArray[ik.boneIdx];
	auto targetPos = XMVector3Transform (XMLoadFloat3 (&targetNode->startPos), _boneMatrices[ik.boneIdx]);

	ostringstream oss;
	oss << "IKボーン番号 = " << targetNode->boneIdx << ":" << _boneNameArray[ik.boneIdx] << ", ";
	D3D::PrintfFloat3 (oss, "", targetNode->startPos, "\n");

	// end node
	auto endNode = _boneNodeAddressArray [ik.targetIdx];

	// get position of bones
	positions.emplace_back (XMLoadFloat3 (&endNode->startPos));
	for (auto& chainBoneIdx : ik.nodeIdxes) {
		auto boneNode = _boneNodeAddressArray [chainBoneIdx];
		positions.emplace_back (XMLoadFloat3 (&boneNode->startPos));
	}

	reverse (positions.begin (), positions.end ());

	// line length
	edgeLens[0] = XMVector3Length (XMVectorSubtract (positions[1], positions[0])).m128_f32[0];
	edgeLens[1] = XMVector3Length (XMVectorSubtract (positions[2], positions[1])).m128_f32[0];

	// don't calculate center point 
	positions[0] = XMVector3Transform (positions[0], _boneMatrices[ik.nodeIdxes[1]]);
	//positions[1]
	positions[2] = XMVector3Transform (positions[2], _boneMatrices[ik.boneIdx]);

	auto linearVec = XMVectorSubtract (positions[2], positions[0]);
	
	float A = XMVector3Length (linearVec).m128_f32[0];
	float B = edgeLens[0];
	float C = edgeLens[1];

	linearVec = XMVector3Normalize (linearVec);

	// root -> centeral
	float theta1 = acosf ((A * A + B * B - C * C) / (2 * A * B));
	// central -> target
	float theta2 = acosf ((B * B + C * C - A * A) / (2 * B * C));

	XMVECTOR axis;
	if (find (_kneeIdxes.begin (), _kneeIdxes.end (), ik.nodeIdxes[0]) == _kneeIdxes.end ()) {
		// can't find
		auto vm = XMVector3Normalize (XMVectorSubtract (positions[2], positions[0]));
		auto vt = XMVector3Normalize (XMVectorSubtract (targetPos, positions[0]));
		axis = XMVector3Cross (vt, vm);
	} else {
		auto right = XMFLOAT3 (1, 0, 0);
		axis = XMLoadFloat3 (&right);
	}

	auto mat1 = XMMatrixTranslationFromVector (-positions[0]);
	mat1 *= XMMatrixRotationAxis (axis, theta1);
	mat1 *= XMMatrixTranslationFromVector (positions[0]);

	auto mat2 = XMMatrixTranslationFromVector (-positions[1]);
	mat2 *= XMMatrixRotationAxis (axis, theta2 - XM_PI);
	mat2 *= XMMatrixTranslationFromVector (positions[1]);

	_boneMatrices[ik.nodeIdxes[1]] *= mat1;
	_boneMatrices[ik.nodeIdxes[0]] = mat2 * _boneMatrices[ik.nodeIdxes[1]];
	_boneMatrices[ik.targetIdx] = _boneMatrices[ik.nodeIdxes[0]];
}

void PMDActor::SolveLookAt (const PMDIK& ik) {
	auto rootNode = _boneNodeAddressArray[ik.nodeIdxes[0]];
	auto targetNode = _boneNodeAddressArray[ik.targetIdx];

	auto opos1 = XMLoadFloat3 (&rootNode->startPos);
	auto tpos1 = XMLoadFloat3 (&targetNode->startPos);

	auto opos2 = XMVector3TransformCoord (opos1, _boneMatrices[ik.nodeIdxes[0]]);
	auto tpos2 = XMVector3TransformCoord (tpos1, _boneMatrices[ik.boneIdx]);

	auto originVec = XMVectorSubtract (tpos1, opos1);
	auto targetVec = XMVectorSubtract (tpos2, opos2);

	originVec = XMVector3Normalize (originVec);
	targetVec = XMVector3Normalize (targetVec);

	XMFLOAT3 up (0, 1, 0);
	XMFLOAT3 right (1, 0, 0);
	XMMATRIX mat = XMMatrixTranslationFromVector (-opos2) * LookAtMatrix (originVec, targetVec, up, right) * XMMatrixTranslationFromVector (opos2);

	_boneMatrices[ik.nodeIdxes[0]] = mat;
}

void PMDActor::IKSolve (int frameNo) {
	auto it = find_if (_ikEnableData.rbegin (), _ikEnableData.rend (), [frameNo](const VMDIKEnable& ikEnable) {
		return ikEnable.frameNo <= frameNo;
	});

	for (auto& ik : _ikData) {
		if (it != _ikEnableData.rend ()) {
			auto ikEnableIt = it->ikEnableTable.find (_boneNameArray[ik.boneIdx]);
			if (ikEnableIt != it->ikEnableTable.end ()) {
				if (!ikEnableIt->second) {
					continue;
				}
			}
		}

		auto childrenNodesCount = ik.nodeIdxes.size ();

		switch (childrenNodesCount) {
			case 0:		// 1 point (IK必要がない)
				assert (0);
				continue;
			case 1:		// 2 point
				SolveLookAt (ik);
				break;
			case 2:		// 3 point
				SolveCosineIK (ik);
				break;
			default:	// 4 point or above
				SolveCCDIK (ik);
				break;
		}
	}
}