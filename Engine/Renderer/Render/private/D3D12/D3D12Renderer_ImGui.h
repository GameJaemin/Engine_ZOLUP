#pragma once


struct ImTextureData;


namespace Hakko
{

#pragma region ImGuiTypes

#define IMGUI_TEXID_ID_MASK			0xFFFFull
#define IMGUI_TEXID_KIND_SHIFT		16
#define IMGUI_TEXID_KIND_MASK		0xFFull
#define IMGUI_TEXID_OPTION_SHIFT	24
#define IMGUI_TEXID_OPTION_MASK		0xFFull

enum DX12_ImGuiTexKind{
	DX12_ImGuiTexKind_Hub = 0,
	DX12_ImGuiTexKind_Internal = 1,
};

#define IMGUI_DRAW_OPTION_LINEAR_TO_GAMMA	0x1

struct DX12_ImGuiTexture{
	DX12_Texture texture;
	UINT RVHeapIndex = UINT_MAX;
};

struct DX12_ImGuiDraw{
	D3D12_RECT scissorRect;
	UINT64 texHandle;
	UINT drawOption;
	UINT indexStart;
	UINT indexCount;
	INT baseVertex;
};

struct DX12_ImGuiFrame{
	ComPtr<ID3D12Resource> VB;
	ComPtr<ID3D12Resource> IB;
	UINT8* VBMapped = nullptr;
	UINT8* IBMapped = nullptr;
	UINT VBCapacity = 0;
	UINT IBCapacity = 0;

	D3D12_VERTEX_BUFFER_VIEW VBV = {};
	D3D12_INDEX_BUFFER_VIEW IBV = {};
	D3D12_VIEWPORT viewport = {};
	float projection[4] = {};

	std::vector<DX12_ImGuiDraw> drawList;
};

#pragma endregion



#pragma region ImGuiModule

class D3D12Renderer::ImGuiModule{
public:
	ImGuiModule(D3D12Renderer& renderer);

public:
	bool Init();
	void Finalize();
	void Resize();

	void Prepare();
	void Record(ID3D12GraphicsCommandList* cmdList, UINT frameIdx);

private:
	bool CreateTexture(ImTextureData* tex);
	void DestroyTexture(ImTextureData* tex);
	UINT64 ResolveTexture(UINT64 texKey, UINT& drawOption);

private:
	D3D12Renderer&						renderer;

	ComPtr<ID3D12RootSignature>			rootSig;
	ComPtr<ID3D12PipelineState>			pso;

	UINT								bbRTVIndices[FRAME_COUNT] = {};

	vector<DX12_ImGuiTexture>			textureSlotList;
	DX12_ImGuiFrame						frames[FRAME_COUNT];
};

#pragma endregion

}
