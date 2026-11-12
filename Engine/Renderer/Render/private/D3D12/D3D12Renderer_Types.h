#pragma once

#include "D3D12Includes.h"
#include "RenderOption.h"
#include "DataHub.h"

#include <memory>

namespace Hakko
{

using Microsoft::WRL::ComPtr;
using namespace DirectX;

#pragma region Typedefs

typedef DataHub::MeshID MeshID;

typedef DataHub::SubMeshID SubMeshID;


typedef DataHub::TexID TexID, STexID;


typedef DataHub::TexPropID CTexID;


typedef DataHub::ShaderID ShaderID;

typedef DataHub::DrawProp PSOBody;

typedef DataHub::DrawPropID PSOID;

typedef uint8_t OutputSignatureID;


typedef DataHub::RenderID RenderID;

typedef DataHub::ActionID ActionID;


constexpr uint16_t DEFAULT_RES_MESH_COUNT = RESERVED_MESH_COUNT;
constexpr uint16_t DEFAULT_RES_TEX_COUNT = RESERVED_TEXTURE_COUNT;
constexpr uint16_t DEFAULT_RES_PSOBODY_COUNT = RESERVED_DRAWPROP_COUNT;
constexpr uint16_t DEFAULT_RES_CTEX_COUNT = RESERVED_TEXPROP_COUNT;


#pragma region RenderResource

#pragma region Vertex

struct DX12_Mesh{
	ComPtr<ID3D12Resource> VB;
	ComPtr<ID3D12Resource> IB;

	DataHub::VertexType vertexType = DataHub::VertexType_Simple;
	UINT vertexCount = 0;
	UINT indexCount = 0;
};

struct DX12_SubMesh{
	UINT indexStart;
	UINT indexCount;
	INT baseVertex;
};

#pragma endregion


#pragma region Texture

enum DX12_TextureUsage{
	DX12_TextureUsage_ReadOnly = DataHub::TextureUsage_ReadOnly,
	DX12_TextureUsage_RTV = DataHub::TextureUsage_RT,
	DX12_TextureUsage_DSV = DataHub::TextureUsage_DS,
	DX12_TextureUsage_UAV,		// 폐기 예정
};


struct DX12_TextureLoaded{
    ComPtr<ID3D12Resource> Tex;
	UINT16 ArraySize;
	UINT16 MipLevels;
	DXGI_FORMAT Format;
	D3D12_RESOURCE_DIMENSION Dimension;
	bool IsCube;

	DX12_TextureUsage usage;
};
typedef DX12_TextureLoaded DX12_Texture;

struct DX12_TextureInstant{
    ComPtr<ID3D12Resource> Tex;

	DX12_TextureUsage usage;
};


struct DX12_InstantLogical{
	uint32_t firstUse;
	uint32_t lastUse;
	uint32_t ownerActionIdx;
	bool readAfterWrite;
	uint16_t physicalIdx;
};

#define TEX_INSTANT_NONE UINT16_MAX
#define TEX_INSTANT_WANT (UINT16_MAX - 1)

#define INSTANT_SLOT_MAX (RENDER_TARGET_MAX + 1)
#define INSTANT_SLOT_DS RENDER_TARGET_MAX

#pragma endregion


#pragma region ResourceRefs

struct DX12_MeshView{
	D3D12_VERTEX_BUFFER_VIEW VBV = {};
	D3D12_INDEX_BUFFER_VIEW IBV = {};		// R16_UINT
	std::vector<DX12_SubMesh> subMeshes;	// 최소 1개
};


#define CTEX_COMBINED_SIZE TEXPROP_SLOT_COUNT

struct DX12_CombinedTextureView{
	UINT RVHeapIndex;
};
typedef DX12_CombinedTextureView DX12_CTexView;

struct DX12_SingleTextureView{
	UINT RVHeapIndex;
};
typedef DX12_SingleTextureView DX12_STexView;

struct DX12_WritableTextureView{
	UINT VHeapIndex = UINT_MAX;		// 최댓값 빈값용으로 새김
};
typedef DX12_WritableTextureView DX12_WTexView;


struct DX12_WTexRef{
	DX12_TextureUsage usage = DX12_TextureUsage_ReadOnly;
	uint16_t idx = 0;
};

#pragma endregion


#pragma region PSOVariant

#define SHADER_VARIANT_NONE 0
struct DX12_CompiledShader{
	ShaderID shaderId;
	uint8_t variant;
	bool failed;
	ComPtr<IDxcBlob> blob;
};

struct DX12_RenderOutputSignature{
	UINT64 RTFormats;
	UINT64 RTBlendNDSFormat;
};
typedef DX12_RenderOutputSignature DX12_OutputSignature;

struct DX12_OutputSignatureDesc{
	DXGI_FORMAT rtFormats[RENDER_TARGET_MAX];
	DataHub::Action_Output_BlendOption rtBlendOpts[RENDER_TARGET_MAX];
	DXGI_FORMAT dsFormat;
	DataHub::Action_Output_DepthOption dsOpt;
	uint8_t sampleCount;
	DataHub::Action_OutputLayout outputLayout;
};

#define OUTSIG_RTFORMAT_BITS	8
#define OUTSIG_RTBLEND_BITS		3
#define OUTSIG_DSFORMAT_SHIFT	(OUTSIG_RTBLEND_BITS * RENDER_TARGET_MAX)
#define OUTSIG_DSOPT_SHIFT		(OUTSIG_DSFORMAT_SHIFT + 8)
#define OUTSIG_SAMPLE_SHIFT		(OUTSIG_DSOPT_SHIFT + 8)
#define OUTSIG_LAYOUT_SHIFT		(OUTSIG_SAMPLE_SHIFT + 8)


#define OUTPUT_SIGNATURE_MAX 32
#define OUTPUT_SIGNATURE_INVALID 0

// PSOID + OutputSigID -> psoVariant
struct DX12_PSOGenerator{
	PSOBody psoBody;
	ComPtr<ID3D12PipelineState> psoVariants[OUTPUT_SIGNATURE_MAX];
};

#pragma endregion

#pragma endregion


#pragma region RootSignature

enum DX12_RootParam
{
	RootParam_ObjectCB0			= 0,								// b0~b4 space3 : 물체 칸 k = ObjectCB0 + k (DRAWREQ_DATABOX_MAX 개, CBV 2 DWORD 씩)
	RootParam_CTexTable			= RootParam_ObjectCB0 + DRAWREQ_DATABOX_MAX,	// t0.. space1 : CTex 언바운드 테이블 (1)
	RootParam_StructuredTable,										// t0.. space2 : 구조버퍼 언바운드 테이블 (1, 미구현)
	RootParam_InstantSR0,											// t0~t8 space0 : 선행 출력 인스턴트, 1칸 테이블 x 9 (칸 s = InstantSR0 + s)
	RootParam_ActionCB			= RootParam_InstantSR0 + INSTANT_SLOT_MAX,	// b0 space2 : 액션 데이터 (2)
	RootParam_LayerDataCB,											// b0 space1 : 레이어 데이터 (2)
	RootParam_ConditionCB,											// b0 space0 : 카메라 등 (2)
	RootParam_LightCB,												// b1 space0 : 조명 64 (2)
	RootParam_LightMatrixCB,										// b2 space0 : 조명 행렬 64 (2)
	RootParam_LayerTexTable,										// t0 space4 : 레이어 전역 텍스처 (환경맵 큐브) 1칸 테이블 (1)
	RootParam_Constants,											// b4 space0 : 루트 상수 4 DWORD (배경 틴트 등)
	RootParam_Count,
};

#pragma endregion


#pragma region RenderData

struct DX12_RenderData{
	uint32_t CBAreaIndex;
};

#pragma endregion



#pragma region RenderAction

#pragma region RenderRequest

struct DX12_RenderRequestProxy{
	RenderID renderId;
	uint32_t requestIdx;
};

struct DX12_RenderDrawData{
	RenderID renderId;
	MeshID meshId;
	SubMeshID subMeshId;
	DX12_RenderData datas[DRAWREQ_DATABOX_MAX];
};

#pragma endregion


#pragma region Barrier

enum DX12_BarrierTarget{
	DX12_BarrierTarget_Loaded,		// id = texId
	DX12_BarrierTarget_RTInstant,	// id = 인스턴트 인덱스
	DX12_BarrierTarget_DSInstant,
};


struct DX12_ActionBarrier{
	uint16_t id;
	DX12_BarrierTarget target;
	D3D12_RESOURCE_STATES before;
	D3D12_RESOURCE_STATES after;
};

#pragma endregion

enum DX12_RenderActionType{
	DX12_RenderActionType_Draw,
	DX12_RenderActionType_Edit,
	DX12_RenderActionType_Compute,
	DX12_RenderActionType_Builtin,
};

// TODO 아직 커스텀 모드, 그래프 노드분석은 미구현
struct DX12_RenderActionNode{
	uint32_t nodeDepth;
	ActionID frontIds[RENDER_ACTION_NODE_MAX];
	ActionID actionId;
};


struct DX12_RenderAction
{
	DX12_RenderActionType actionType;
	DataHub::BuiltinActionType builtinType;
	DataHub::RenderIDType renderIdType;
	std::array<uint32_t, RENDER_ACTION_NODE_MAX> frontNodeIdx;
	
	D3D12_VIEWPORT viewport;
	D3D12_RECT scissorRect;

	OutputSignatureID outputSigId;
	DataHub::Action_OutputLayout outputLayout;

	std::array<TexID, RENDER_TARGET_MAX> rtTexIds;
	TexID dsTexId;

	std::array<uint16_t, RENDER_TARGET_MAX> rtInstantIdx;
	uint16_t dsInstantIdx;
	std::array<uint16_t, INSTANT_SLOT_MAX> srInstantIdx;
	std::array<uint16_t, INSTANT_SLOT_MAX> copySrcInstantIdx;

	bool dsReadOnly;

	std::vector<DX12_ActionBarrier> copyBarriers;
	std::vector<DX12_ActionBarrier> beginBarriers;
	std::vector<DX12_ActionBarrier> endBarriers;

	DataHub::DataBoxTID actionDataTid;
	DX12_RenderData actionRenderData;

	std::array<DataHub::DataBoxTID, DRAWREQ_DATABOX_MAX> dataTids;
};

#pragma endregion


#pragma region RenderLayer

struct DX12_RenderLayer
{
	TexID bgTexId;
	bool bgIsCube;
	float bgColor[4];
	TexID envTexId;

	DX12_RenderData conditionRenderData;
	DX12_RenderData lightRenderData;
	DX12_RenderData lightMatrixRenderData;

	DataHub::DataBoxTID layerDataTid;
	DX12_RenderData layerRenderData;

	std::vector<DX12_RenderActionNode> renderActionNodeList;
	std::vector<DX12_RenderAction> renderActionList;

	std::vector<DX12_ActionBarrier> finalBarriers;

	uint16_t rtInstantCount;
	uint16_t dsInstantCount;

	uint16_t rtInstantOffset;
	uint16_t dsInstantOffset;

	std::vector<std::vector<DX12_RenderRequestProxy>> actionRenderRequestList;
	std::vector<std::vector<DX12_RenderDrawData>> actionSortedRenderDataList;
};

#pragma endregion


#pragma region RenderFrame

struct DX12_RenderFrame
{
	std::vector<DX12_RenderLayer> layerList;
	
	uint16_t rtInstantNeedCount;
	uint16_t dsInstantNeedCount;

	bool frameInvalidated;
};
typedef DX12_RenderFrame DX12_RenderCanvas;

#pragma endregion

#pragma endregion


#pragma region Helper

inline static void DebugReport(DataHub::RenderReportKind kind, const wchar_t* msg)
{
#if RENDER_DEBUG_OUTPUT_MODE == RENDER_DEBUG_OUTPUT_ENABLE
	OutputDebugStringW(msg);
	OutputDebugStringW(L"\n");
#endif
#if RENDER_REPORT_SEND_MODE == RENDER_REPORT_SEND_ENABLE
	DataHub::RenderReportList.push_back({ kind, msg });
#endif
#ifdef _DEBUG
	if (kind == DataHub::RenderReport_Invariant)
		__debugbreak();
#endif
}

#pragma endregion

}
