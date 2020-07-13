#pragma once

#include<d3d12.h>
#include<vector>
#include<wrl.h>
#include<memory>

class DX12Wrapper;
class PMDActor;
class PMDRenderer {
	friend PMDActor;
	
private:
	DX12Wrapper& _dx12;
	template<typename T>
	using ComPtr = Microsoft::WRL::ComPtr<T>;

	ComPtr<ID3D12PipelineState> _pipeline = nullptr;
	ComPtr<ID3D12RootSignature> _rootSig = nullptr;

	ComPtr<ID3D12Resource> _whiteTex = nullptr;
	ComPtr<ID3D12Resource> _blackTex = nullptr;
	ComPtr<ID3D12Resource> _gradTex = nullptr;

	ID3D12Resource* CreateDefaultTexture (size_t width, size_t height);
	ID3D12Resource* CreateWhiteTexture ();
	ID3D12Resource* CreateBlackTexture ();
	ID3D12Resource* CreateGrayGradationTexture ();

	HRESULT CreateGraphicsPipelineForPMD ();
	HRESULT CreateRootSignature ();
	
	bool CheckShaderCompileResult (HRESULT hr, ID3DBlob* error = nullptr);

public:
	PMDRenderer (DX12Wrapper& dx12);
	~PMDRenderer ();
	void Update ();
	void Draw ();
	ID3D12PipelineState* GetPipelineState ();
	ID3D12RootSignature* GetRootSignature ();
};

