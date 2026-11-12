#include "D3D12Renderer.h"


namespace Hakko
{

#pragma region Helper

inline static UINT ConvertReleaseListToMask(std::vector<uint16_t>& list, std::vector<bool>& mask, uint16_t maskSize)
{
	mask.resize(maskSize, false);

	UINT skipped = 0;
	for (auto id : list)
	{
		if (id >= maskSize)
		{
			skipped++;
			continue;
		}
		mask[id] = true;
	}
	return skipped;
}


inline static DXGI_FORMAT GetDSVFormat(DXGI_FORMAT resFormat)
{
	switch (resFormat)
	{
	case DXGI_FORMAT_R24G8_TYPELESS:		return DXGI_FORMAT_D24_UNORM_S8_UINT;
	case DXGI_FORMAT_R32_TYPELESS:			return DXGI_FORMAT_D32_FLOAT;
	case DXGI_FORMAT_R32G8X24_TYPELESS:		return DXGI_FORMAT_D32_FLOAT_S8X24_UINT;
	case DXGI_FORMAT_R16_TYPELESS:			return DXGI_FORMAT_D16_UNORM;
	default:								return resFormat;
	}
}


inline bool IsEmpty(const DataHub::Layer_Viewport& v)
{
    return v.width <= 0.0f || v.height <= 0.0f;
}

inline bool IsEmpty(const DataHub::Action_Viewport& v)
{
    return v.width <= 0.0f || v.height <= 0.0f;
}

inline bool IsEmpty(const DataHub::Action_Rect& v)
{
    return v.width <= 0 || v.height <= 0;
}

inline D3D12_VIEWPORT ToD3D12Viewport(const DataHub::Layer_Viewport& v)
{
    const bool badDepth = !(v.maxZ > v.minZ);
    return D3D12_VIEWPORT{ v.originX, v.originY, v.width, v.height, badDepth ? 0.0f : v.minZ, badDepth ? 1.0f : v.maxZ };
}

inline D3D12_RECT ToD3D12Rect(const DataHub::Layer_Viewport& v)
{
    return D3D12_RECT{
        static_cast<LONG>(v.originX),
        static_cast<LONG>(v.originY),
        static_cast<LONG>(v.originX + v.width),
        static_cast<LONG>(v.originY + v.height)
    };
}

inline D3D12_VIEWPORT ToD3D12Viewport(const DataHub::Action_Viewport& v)
{
    const bool badDepth = !(v.maxZ > v.minZ);
    return D3D12_VIEWPORT{ v.originX, v.originY, v.width, v.height, badDepth ? 0.0f : v.minZ, badDepth ? 1.0f : v.maxZ };
}

inline D3D12_RECT ToD3D12Rect(const DataHub::Action_Viewport& v)
{
    return D3D12_RECT{
        static_cast<LONG>(v.originX),
        static_cast<LONG>(v.originY),
        static_cast<LONG>(v.originX + v.width),
        static_cast<LONG>(v.originY + v.height)
    };
}

inline D3D12_RECT ToD3D12Rect(const DataHub::Action_Rect& v)
{
    return D3D12_RECT{
        static_cast<LONG>(v.originX),
        static_cast<LONG>(v.originY),
        static_cast<LONG>(v.originX + v.width),
        static_cast<LONG>(v.originY + v.height)
    };
}


#pragma region Action

inline static void ResolveActionViewport(const DataHub::RTB_Action& src, const DataHub::Layer_Viewport& layerVp,
	D3D12_VIEWPORT& outVp, D3D12_RECT& outRect)
{
	const bool vpEmpty = IsEmpty(src.viewport);

	outVp = vpEmpty ? ToD3D12Viewport(layerVp) : ToD3D12Viewport(src.viewport);

	if (!IsEmpty(src.rect))
		outRect = ToD3D12Rect(src.rect);
	else
		outRect = vpEmpty ? ToD3D12Rect(layerVp) : ToD3D12Rect(src.viewport);
}

inline static void FillRenderActionBase(DX12_RenderAction& action, DX12_RenderActionType type, const DataHub::RTB_Action& src,
	const DataHub::Layer_Viewport& layerVp)
{

	action.actionType = type;
	action.builtinType = (DataHub::BuiltinActionType)0;
	action.renderIdType = DataHub::RenderIdType_None;
	action.frontNodeIdx.fill(0);

	ResolveActionViewport(src, layerVp, action.viewport, action.scissorRect);

	action.outputSigId = OUTPUT_SIGNATURE_INVALID;

	action.rtTexIds.fill(0);
	action.dsTexId = 0;
	action.rtInstantIdx.fill(TEX_INSTANT_NONE);
	action.dsInstantIdx = TEX_INSTANT_NONE;
	action.srInstantIdx.fill(TEX_INSTANT_NONE);
	action.copySrcInstantIdx.fill(TEX_INSTANT_NONE);

	for (uint8_t i = 0; i < RENDER_TARGET_MAX; i++)
	{
		if (src.output.useRT[i])
		{
			action.rtTexIds[i] = src.output.RTTex[i];
			if (src.output.RTTex[i] == 0)
				action.rtInstantIdx[i] = TEX_INSTANT_WANT;
		}
	}
	if (src.output.useDS)
	{
		action.dsTexId = src.output.DSTex;
		if (src.output.DSTex == 0)
			action.dsInstantIdx = TEX_INSTANT_WANT;
	}

	action.dsReadOnly = src.output.depthOpt == DataHub::Action_Output_DepthOption_ForceDepthRead;

	action.outputLayout = src.output.outputLayout;
	{
		uint8_t liveMask = 0;
		for (uint8_t r = 0; r < RENDER_TARGET_MAX; r++)
		{
			if (src.output.useRT[r])
				liveMask |= (uint8_t)(1 << r);
		}

		const uint8_t needMask = DataHub::OutputLayoutTargetMask(action.outputLayout);
		bool ok = (needMask & liveMask) == needMask;

		if (action.outputLayout == DataHub::OutputLayout_DepthOnly && !src.output.useDS)
			ok = false;

		if (!ok)
		{
			DebugReport(DataHub::RenderReport_Contract, L"[Read] 출력 레이아웃이 요구하는 칸이 안 켜져 있음 (None 으로 강등)");
			action.outputLayout = DataHub::OutputLayout_None;
		}
	}

	action.actionDataTid = src.actionDataBoxTypeId;
	if (action.actionDataTid != 0 && DataHub::DataBoxCustomizer::GetProvider(action.actionDataTid) == nullptr)
	{
		DebugReport(DataHub::RenderReport_Contract, L"[Read] 액션 데이터 tid 에 프로바이더 없음 (무시됨)");
		action.actionDataTid = 0;
	}
	action.actionRenderData = {};

	action.dataTids.fill(0);
}

inline static void FillRenderAction(DX12_RenderAction& action, const DataHub::RTB_ActionDraw& src, const DataHub::Layer_Viewport& layerVp)
{
	FillRenderActionBase(action, DX12_RenderActionType_Draw, src, layerVp);
	action.renderIdType = src.renderIdType;
	action.dataTids = src.dataBoxTypeIds;

	for (size_t k = 0; k < DRAWREQ_DATABOX_MAX; k++)
	{
		if (action.dataTids[k] != 0 && DataHub::DataBoxCustomizer::GetProvider(action.dataTids[k]) == nullptr)
		{
			DebugReport(DataHub::RenderReport_Contract, L"[Read] 드로우 데이터 칸 tid 에 프로바이더 없음 (칸 무시됨)");
			action.dataTids[k] = 0;
		}
	}
}

inline static void FillRenderAction(DX12_RenderAction& action, const DataHub::RTB_ActionEdit& src, const DataHub::Layer_Viewport& layerVp)
{
	FillRenderActionBase(action, DX12_RenderActionType_Edit, src, layerVp);
}

inline static void FillRenderAction(DX12_RenderAction& action, const DataHub::RTB_ActionCompute& src, const DataHub::Layer_Viewport& layerVp)
{
	FillRenderActionBase(action, DX12_RenderActionType_Compute, src, layerVp);
	// 미구현 (컴퓨트 경로): 액션 자리만, 디스패치 안함
	DebugReport(DataHub::RenderReport_Contract, L"[Read] 컴퓨트 액션 미구현 (무시)");
}

inline static void FillRenderAction(DX12_RenderAction& action, const DataHub::RTB_ActionBuiltin& src, const DataHub::Layer_Viewport& layerVp)
{
	FillRenderActionBase(action, DX12_RenderActionType_Builtin, src, layerVp);
	action.builtinType = src.builtinType;
}

#pragma endregion


#pragma region Barrier

static const D3D12_RESOURCE_STATES WRITE_STATE_MASK =
	D3D12_RESOURCE_STATE_RENDER_TARGET | D3D12_RESOURCE_STATE_UNORDERED_ACCESS |
	D3D12_RESOURCE_STATE_DEPTH_WRITE | D3D12_RESOURCE_STATE_COPY_DEST |
	D3D12_RESOURCE_STATE_STREAM_OUT | D3D12_RESOURCE_STATE_RESOLVE_DEST;

inline static D3D12_RESOURCE_STATES GetLoadedHomeState(DX12_TextureUsage usage)
{
	switch (usage)
	{
	case DX12_TextureUsage_DSV:
		return D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	case DX12_TextureUsage_UAV:
		return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	default:
		return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
	}
}

inline static D3D12_RESOURCE_STATES GetInstantBaseState(bool isDS)
{
	if (isDS)
		return D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}

inline static D3D12_RESOURCE_STATES GetInstantReadState(DX12_RenderActionType type, bool isDS)
{
	if (type == DX12_RenderActionType_Compute)
		return D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE;

	if (isDS)
		return D3D12_RESOURCE_STATE_DEPTH_READ | D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;

	return D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
}


inline static bool IsStateCovered(D3D12_RESOURCE_STATES before, D3D12_RESOURCE_STATES after)
{
	if (before == after)
		return true;

	return (before & WRITE_STATE_MASK) == 0 && (before & after) == after;
}


inline static void TransitionInstant(std::vector<DX12_ActionBarrier>& barriers, DX12_BarrierTarget target, uint16_t instIdx,
	D3D12_RESOURCE_STATES& state, D3D12_RESOURCE_STATES after)
{
	if (IsStateCovered(state, after))
		return;

	barriers.push_back(DX12_ActionBarrier{instIdx, target, state, after});
	state = after;
}

inline static void TransitionLoaded(DX12_RenderAction& action, TexID texId, const DX12_Texture& tex, D3D12_RESOURCE_STATES use)
{
	const D3D12_RESOURCE_STATES home = GetLoadedHomeState(tex.usage);

	if (IsStateCovered(home, use))
		return;

	action.beginBarriers.push_back(DX12_ActionBarrier{texId, DX12_BarrierTarget_Loaded, home, use});
	action.endBarriers.push_back(DX12_ActionBarrier{texId, DX12_BarrierTarget_Loaded, use, home});
}


inline static bool IsFrontAction(const DX12_RenderAction& action, uint32_t idx)
{
	for (uint32_t f = 0; f < RENDER_ACTION_NODE_MAX; f++)
	{
		if (action.frontNodeIdx[f] != 0 && action.frontNodeIdx[f] == idx)
			return true;
	}
	return false;
}

#pragma endregion


#pragma region DrawRequest

inline static void ReadActionDrawRequests(const DataHub::RTB_ActionDraw& src, DX12_RenderLayer& layer, uint32_t node)
{
	const size_t reqCount = src.drawReqList.size();

	auto& requestList = layer.actionRenderRequestList[node];

	requestList.resize(reqCount);

	for (size_t i = 0; i < reqCount; i++)
	{
		requestList[i].renderId = src.drawReqList[i].renderId;
		requestList[i].requestIdx = (uint32_t)i;
	}

	layer.actionSortedRenderDataList[node].resize(reqCount);
}

inline static void ReadActionEditRequest(const DataHub::RTB_ActionEdit& src, DX12_RenderLayer& layer, uint32_t node)
{
	auto& requestList = layer.actionRenderRequestList[node];

	requestList.resize(1);

	requestList[0].renderId = ((RenderID)src.drawProp << 48) | ((RenderID)src.texProp << 32) ;
	requestList[0].requestIdx = 0;

	layer.actionSortedRenderDataList[node].assign(1, {});
}

#pragma endregion

#pragma endregion



#pragma region ReadMessage

void D3D12Renderer::ReadTransferBox()
{
	// --- 패스 / 리소스 관련 요청 (프레임 별) ---
	ReadUploadRequest();

	// --- 그리기 관련 요청 (레이어/패스 별) ---
	ReadLayerOption();

	if (renderFrames[cpuFrameIndex].frameInvalidated)
	{
		DebugReport(DataHub::RenderReport_Contract, L"[Frame] 무효 프레임 (이번 프레임 요청 전부 버림)");
		return;
	}


	ReadActionNode();
}


void D3D12Renderer::ReadLayerOption()
{
	using namespace DataHub;

	auto& renderFrame = renderFrames[cpuFrameIndex];

	for (size_t i = 0; i < layerCount; i++)
	{
		auto bgTexIdNew = LayerList[i].backGround.bgTexId;
		const auto& color = LayerList[i].backGround.color;

        if (releaseTexIdMask[bgTexIdNew])
        {
            DebugReport(DataHub::RenderReport_Contract, L"[Layer] 삭제된 텍스처를 배경으로 지정 (무시됨)");
            bgTexIdNew = 0;
        }
	    else if (bgTexIdNew != 0)
		{
			MarkOrBuildSingleTexture(bgTexIdNew);
            renderFrame.layerList[i].bgIsCube = textureList[bgTexIdNew].IsCube;

			renderFrame.layerList[i].bgColor[0] = color[0];
			renderFrame.layerList[i].bgColor[1] = color[1];
			renderFrame.layerList[i].bgColor[2] = color[2];
			renderFrame.layerList[i].bgColor[3] = color[3];
		}

		renderFrame.layerList[i].bgTexId = bgTexIdNew;


		auto envTexIdNew = LayerList[i].envTexId;

		if (releaseTexIdMask[envTexIdNew])
		{
			DebugReport(DataHub::RenderReport_Contract, L"[Layer] 삭제된 텍스처를 환경맵으로 지정 (무시됨)");
			envTexIdNew = 0;
		}
		else if (envTexIdNew != 0)
		{
			if (!textureList[envTexIdNew].IsCube)
			{
				DebugReport(DataHub::RenderReport_Contract, L"[Layer] 환경맵이 큐브맵이 아님 (무시됨)");
				envTexIdNew = 0;
			}
			else
				MarkOrBuildSingleTexture(envTexIdNew);
		}

		renderFrame.layerList[i].envTexId = envTexIdNew;


		renderFrame.layerList[i].layerDataTid = LayerList[i].layerDataBoxTid;
		if (renderFrame.layerList[i].layerDataTid != 0 && DataBoxCustomizer::GetProvider(renderFrame.layerList[i].layerDataTid) == nullptr)
		{
			DebugReport(DataHub::RenderReport_Contract, L"[Read] 레이어 데이터 tid 에 프로바이더 없음 (무시됨)");
			renderFrame.layerList[i].layerDataTid = 0;
		}
	}
	
}

void D3D12Renderer::ReadActionNode()
{
	using namespace DataHub;

	auto& renderFrame = renderFrames[cpuFrameIndex];

	
    #if RENDER_PIPELINE_MODE == RENDER_USE_PIPELINE_BUILTIN

	renderFrame.rtInstantNeedCount = 0;
	renderFrame.dsInstantNeedCount = 0;

	const size_t BUILTIN_ACTION_NODE_MAX = RENDER_LIGHT_MAX + 24;

	for (size_t l = 0; l < layerCount; l++)
	{
		auto& layerDesc = LayerList[l];
		auto& layer = renderFrame.layerList[l];

		Layer_Viewport layerVp = layerDesc.viewport;
		if (IsEmpty(layerVp))
			layerVp = Layer_Viewport{ 0.0f, 0.0f, (float)bbWidth, (float)bbHeight };

		auto& nodeList = layer.renderActionNodeList;
		auto& actionList = layer.renderActionList;
		auto& requestList = layer.actionRenderRequestList;
		auto& sortedList = layer.actionSortedRenderDataList;

		nodeList.resize(BUILTIN_ACTION_NODE_MAX);
		actionList.resize(BUILTIN_ACTION_NODE_MAX);
		requestList.resize(BUILTIN_ACTION_NODE_MAX);
		sortedList.resize(BUILTIN_ACTION_NODE_MAX);

		uint32_t n = 0;
		ActionID id = 0;


		nodeList[n] = DX12_RenderActionNode{};
		{
			DX12_RenderAction& base = actionList[n];
			base = {};
			base.rtInstantIdx.fill(TEX_INSTANT_NONE);
			base.dsInstantIdx = TEX_INSTANT_NONE;
			base.srInstantIdx.fill(TEX_INSTANT_NONE);
			base.copySrcInstantIdx.fill(TEX_INSTANT_NONE);
			base.outputSigId = OUTPUT_SIGNATURE_INVALID;
		}
		requestList[n].clear();
		sortedList[n].clear();
		n++;	id++;

		nodeList[n] = DX12_RenderActionNode{1, {0}, id};
		FillRenderAction(actionList[n], layerDesc.actionEarlyZ, layerVp);
		ReadActionDrawRequests(layerDesc.actionEarlyZ, layer, n);
		n++;	id++;

		nodeList[n] = DX12_RenderActionNode{1, {0}, id};
		FillRenderAction(actionList[n], layerDesc.actionGlobalSDW, layerVp);
		ReadActionDrawRequests(layerDesc.actionGlobalSDW, layer, n);
		n++;	id++;

		nodeList[n] = DX12_RenderActionNode{1, {0}, id};
		FillRenderAction(actionList[n], layerDesc.actionLocalSDWs[0], layerVp);
		ReadActionDrawRequests(layerDesc.actionLocalSDWs[0], layer, n);
		actionList[n].frontNodeIdx[0] = 0;
		n++;	id++;

		for (uint32_t i = 1; i < RENDER_LIGHT_MAX; i++)
		{
			nodeList[n] = DX12_RenderActionNode{1+i, {id-1,}, id};
			FillRenderAction(actionList[n], layerDesc.actionLocalSDWs[i], layerVp);
			ReadActionDrawRequests(layerDesc.actionLocalSDWs[i], layer, n);
			actionList[n].frontNodeIdx[0] = n - 1;
			n++;	id++;
		}

		nodeList[n] = DX12_RenderActionNode{RENDER_LIGHT_MAX+1, {1,2,2+RENDER_LIGHT_MAX,}, id};
		FillRenderAction(actionList[n], layerDesc.actionOPQ, layerVp);
		ReadActionDrawRequests(layerDesc.actionOPQ, layer, n);
		actionList[n].frontNodeIdx[0] = 1;
		actionList[n].frontNodeIdx[1] = 2;
		actionList[n].frontNodeIdx[2] = n - 1;
		n++;	id++;

		nodeList[n] = DX12_RenderActionNode{RENDER_LIGHT_MAX+2, {id-1,}, id};
		FillRenderAction(actionList[n], layerDesc.actionBackground, layerVp);
		requestList[n].clear();
		sortedList[n].clear();
		actionList[n].frontNodeIdx[0] = n - 1;
		n++;	id++;

		nodeList[n] = DX12_RenderActionNode{RENDER_LIGHT_MAX+3, {id-1,}, id};
		FillRenderAction(actionList[n], layerDesc.actionTP, layerVp);
		ReadActionDrawRequests(layerDesc.actionTP, layer, n);
		actionList[n].frontNodeIdx[0] = n - 1;
		n++;	id++;

		for (uint32_t i = 0; i < 16; i++)
		{
			const RTB_ActionEdit& pp = layerDesc.actionPPs[i];
			if (pp.drawProp == 0)
			{
				id++;
				continue;
			}

			nodeList[n] = DX12_RenderActionNode{RENDER_LIGHT_MAX+4+i, {id-1,}, id};
			FillRenderAction(actionList[n], pp, layerVp);
			ReadActionEditRequest(pp, layer, n);
			actionList[n].frontNodeIdx[0] = n - 1;
			n++;	id++;
		}

		nodeList[n] = DX12_RenderActionNode{RENDER_LIGHT_MAX+20, {id-1,}, id};
		FillRenderAction(actionList[n], layerDesc.actionSC, layerVp);
		ReadActionDrawRequests(layerDesc.actionSC, layer, n);
		actionList[n].frontNodeIdx[0] = n - 1;
		n++;	id++;

		nodeList[n] = DX12_RenderActionNode{RENDER_LIGHT_MAX+21, {id-1,}, id};
		FillRenderAction(actionList[n], layerDesc.actionDebug, layerVp);
		ReadActionDrawRequests(layerDesc.actionDebug, layer, n);
		actionList[n].frontNodeIdx[0] = n - 1;
		n++;	id++;

		nodeList.resize(n);
		actionList.resize(n);
		requestList.resize(n);
		sortedList.resize(n);


		BuildActionGraph(layer, layerDesc);


		layer.rtInstantOffset = 0;
		layer.dsInstantOffset = 0;

		if (renderFrame.rtInstantNeedCount < layer.rtInstantCount)
			renderFrame.rtInstantNeedCount = layer.rtInstantCount;
		if (renderFrame.dsInstantNeedCount < layer.dsInstantCount)
			renderFrame.dsInstantNeedCount = layer.dsInstantCount;
	}

	
	ResizeTextureInstant();
	
	
    #elif RENDER_PIPELINE_MODE == RENDER_USE_PIPELINE_CUSTOM

	#endif
}

void D3D12Renderer::ReadUploadRequest()
{
	using namespace DataHub;


	if (flag_updateResources_Upload)
	{
		UploadMesh();
		UploadTexture();
		GetShaderInfo();

		flag_updateResources_Upload = false;
	}
	
	if (flag_updateProps_FindOrBuild)
	{
		BuildPSOBody();
		BuildCombinedTexture();

		flag_updateProps_FindOrBuild = false;
	}
}

void D3D12Renderer::ReadReleaseRequest()
{
	using namespace DataHub;

	auto& renderFrame = renderFrames[cpuFrameIndex];


	if (flag_updateResources_Release)
	{
		if (ConvertReleaseListToMask(ReleaseMeshList, releaseMeshIdMask, (uint16_t)meshList.size()) > 0)
			DebugReport(RenderReport_Contract, L"[Release] meshId 범위 밖 (업로드 전 해제?), 무시");
		if (ConvertReleaseListToMask(ReleaseTextureList, releaseTexIdMask, (uint16_t)textureList.size()) > 0)
			DebugReport(RenderReport_Contract, L"[Release] texId 범위 밖 (업로드 전 해제?), 무시");
		meshReleaseCount = FRAME_COUNT;
		texReleaseCount = FRAME_COUNT;

		ReleaseMeshList.clear();
		ReleaseTextureList.clear();

		flag_updateResources_Release = false;
	}

	if (flag_updateProps_Release)
	{
		if (ConvertReleaseListToMask(ReleaseDrawPropList, releasePSOIdMask, (uint16_t)psoGeneratorList.size()) > 0)
			DebugReport(RenderReport_Contract, L"[Release] drawPropId 범위 밖 (빌드 전 해제?), 무시");
		if (ConvertReleaseListToMask(ReleaseTexPropList, releaseCTexIdMask, (uint16_t)combinedTextureList.size()) > 0)
			DebugReport(RenderReport_Contract, L"[Release] texPropId 범위 밖 (빌드 전 해제?), 무시");
		psoReleaseCount = FRAME_COUNT;
		cTexReleaseCount = FRAME_COUNT;

		ReleaseDrawPropList.clear();
		ReleaseTexPropList.clear();

		flag_updateProps_Release = false;
	}
}


#pragma region Helper

DX12_OutputSignature D3D12Renderer::MakeOutputSignature(const DX12_RenderAction& action, const DataHub::Action_Output& output)
{
	using namespace DataHub;

	DX12_OutputSignatureDesc desc = {};

	for (uint8_t r = 0; r < RENDER_TARGET_MAX; r++)
	{
		DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;

		if (action.rtTexIds[r] != 0)
		{
			if (action.rtTexIds[r] < textureList.size())
				format = textureList[action.rtTexIds[r]].Format;
		}
		else if (action.rtInstantIdx[r] != TEX_INSTANT_NONE)
		{
			format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		}

		desc.rtFormats[r] = format;

		if (format != DXGI_FORMAT_UNKNOWN)
			desc.rtBlendOpts[r] = output.blendOpt[r];
		else
			desc.rtBlendOpts[r] = Action_Output_BlendOption_ByBlendType;
	}

	DXGI_FORMAT dsFormat = DXGI_FORMAT_UNKNOWN;

	if (action.dsTexId != 0)
	{
		if (action.dsTexId < textureList.size())
			dsFormat = GetDSVFormat(textureList[action.dsTexId].Format);
	}
	else if (action.dsInstantIdx != TEX_INSTANT_NONE)
	{
		dsFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	}

	desc.dsFormat = dsFormat;

	if (dsFormat != DXGI_FORMAT_UNKNOWN)
		desc.dsOpt = output.depthOpt;
	else
		desc.dsOpt = Action_Output_DepthOption_ByDepthUsage;

	desc.sampleCount = 1;

	desc.outputLayout = action.outputLayout;

	return PackOutputSignature(desc);
}


const DataHub::RTB_Action& D3D12Renderer::GetActionSource(const DataHub::RTB_Layer& layerDesc, ActionID id)
{
#if RENDER_PIPELINE_MODE == RENDER_USE_PIPELINE_BUILTIN
	if (id == 1)
		return layerDesc.actionEarlyZ;
	if (id == 2)
		return layerDesc.actionGlobalSDW;
	if (id < 3 + RENDER_LIGHT_MAX)
		return layerDesc.actionLocalSDWs[id - 3];
	if (id == 3 + RENDER_LIGHT_MAX)
		return layerDesc.actionOPQ;
	if (id == 4 + RENDER_LIGHT_MAX)
		return layerDesc.actionBackground;
	if (id == 5 + RENDER_LIGHT_MAX)
		return layerDesc.actionTP;
	if (id < 6 + RENDER_LIGHT_MAX + 16)
		return layerDesc.actionPPs[id - (6 + RENDER_LIGHT_MAX)];
	if (id == 6 + RENDER_LIGHT_MAX + 16)
		return layerDesc.actionSC;
	return layerDesc.actionDebug;
#else
	const auto& v = layerDesc.actionList[id];
	if (const auto* draw = std::get_if<DataHub::RTB_ActionDraw>(&v))
		return *draw;
	if (const auto* edit = std::get_if<DataHub::RTB_ActionEdit>(&v))
		return *edit;
	if (const auto* builtin = std::get_if<DataHub::RTB_ActionBuiltin>(&v))
		return *builtin;
	return std::get<DataHub::RTB_ActionCompute>(v);
#endif
}


void D3D12Renderer::BuildActionGraph(DX12_RenderLayer& layer, const DataHub::RTB_Layer& layerDesc)
{
	using namespace DataHub;

	auto& actionList = layer.renderActionList;
	const uint32_t actionCount = (uint32_t)actionList.size();

	if (layer.renderActionNodeList.size() != actionCount)
		DebugReport(DataHub::RenderReport_Invariant, L"[Graph] 노드 리스트랑 액션 리스트 갯수 다름");

	rtInstantLogicalList.clear();
	dsInstantLogicalList.clear();


	#pragma region AssignInstant

	for (uint32_t i = 1; i < actionCount; i++)
	{
		DX12_RenderAction& action = actionList[i];
		const bool isCompute = action.actionType == DX12_RenderActionType_Compute;

		if (isCompute)
		{
			for (uint8_t r = 0; r < RENDER_TARGET_MAX; r++)
			{
				if (action.rtInstantIdx[r] == TEX_INSTANT_WANT)
				{
					DebugReport(DataHub::RenderReport_Contract, L"[Graph] 컴퓨트 액션 인스턴트 출력 미지원 (무시)");
					action.rtInstantIdx[r] = TEX_INSTANT_NONE;
				}
			}

			if (action.dsTexId != 0 || action.dsInstantIdx != TEX_INSTANT_NONE)
			{
				DebugReport(DataHub::RenderReport_Contract, L"[Graph] 컴퓨트 액션에 DS 지정됨 (무시)");
				action.dsTexId = 0;
				action.dsInstantIdx = TEX_INSTANT_NONE;
			}
		}

		uint16_t outClaim[INSTANT_SLOT_MAX];
		uint16_t srClaim[INSTANT_SLOT_MAX];
		uint16_t copySrc[INSTANT_SLOT_MAX];
		uint16_t inheritClaim[INSTANT_SLOT_MAX];
		bool srInherited[INSTANT_SLOT_MAX];

		for (uint8_t s = 0; s < INSTANT_SLOT_MAX; s++)
		{
			outClaim[s] = TEX_INSTANT_NONE;
			srClaim[s] = TEX_INSTANT_NONE;
			copySrc[s] = TEX_INSTANT_NONE;
			inheritClaim[s] = TEX_INSTANT_NONE;
			srInherited[s] = false;
		}


		for (uint32_t f = 0; f < RENDER_ACTION_NODE_MAX; f++)
		{
			const uint32_t frontIdx = action.frontNodeIdx[f];

			if (frontIdx == 0)
				continue;

			if (frontIdx >= i)
			{
				DebugReport(DataHub::RenderReport_Contract, L"[Graph] 선행 노드가 정렬상 뒤에 있음 / 자기 자신 (무시)");
				continue;
			}

			const DX12_RenderAction& front = actionList[frontIdx];
			const Action_Input& input = GetActionSource(layerDesc, layer.renderActionNodeList[i].actionId).inputs[f];

			for (uint8_t s = 0; s < INSTANT_SLOT_MAX; s++)
			{
				const bool isDS = s == INSTANT_SLOT_DS;
				auto& logicalList = isDS ? dsInstantLogicalList : rtInstantLogicalList;

				const Action_InputType type = isDS ? input.DSInputType : input.RTInputType[s];

				if ((type & Action_InputType_ClearSR) == 0 && front.srInstantIdx[s] != TEX_INSTANT_NONE && inheritClaim[s] == TEX_INSTANT_NONE)
					inheritClaim[s] = front.srInstantIdx[s];

				const TexID frontTexId = isDS ? front.dsTexId : front.rtTexIds[s];
				const uint16_t logicalIdx = isDS ? front.dsInstantIdx : front.rtInstantIdx[s];

				if (frontTexId != 0 || logicalIdx == TEX_INSTANT_NONE)
					continue;

				if (logicalIdx >= logicalList.size())
				{
					DebugReport(DataHub::RenderReport_Invariant, L"[Graph] 선행 출력 인스턴트 인덱스 범위 밖");
					continue;
				}

				const bool wantCopy = (type & Action_InputType_Copy) != 0;
				const bool wantRead = wantCopy || (type & Action_InputType_Convert) != 0;
				const bool detach = (type & Action_InputType_Detach) != 0;
				const bool wantOut = !detach && (!wantRead || wantCopy || (isDS && action.dsReadOnly));

				if (wantRead)
				{
					if (logicalList[logicalIdx].ownerActionIdx != frontIdx)
						DebugReport(DataHub::RenderReport_Contract, L"[Graph] 선행 출력에 다른 액션이 먼저 이어 그림 (형제 노드 순서에 따라 결과 달라짐)");

					if (srClaim[s] == TEX_INSTANT_NONE)
					{
						if (wantCopy)
						{
							srClaim[s] = (uint16_t)logicalList.size();
							copySrc[s] = logicalIdx;
							logicalList.push_back(DX12_InstantLogical{i, i, i, false, TEX_INSTANT_NONE});
						}
						else
						{
							srClaim[s] = logicalIdx;
						}
					}
					else if (srClaim[s] != logicalIdx && copySrc[s] != logicalIdx)
					{
						DebugReport(DataHub::RenderReport_Contract, L"[Graph] 여러 선행 출력이 같은 SR 칸으로 들어옴 (앞쪽 선행 노드 우선)");
					}
				}

				if (wantOut)
				{
					if (outClaim[s] == TEX_INSTANT_NONE)
						outClaim[s] = logicalIdx;
					else if (outClaim[s] != logicalIdx)
						DebugReport(DataHub::RenderReport_Contract, L"[Graph] 여러 선행 출력이 같은 출력 칸으로 들어옴 (앞쪽 선행 노드 우선)");
				}
			}
		}

		for (uint8_t s = 0; s < INSTANT_SLOT_MAX; s++)
		{
			if (srClaim[s] == TEX_INSTANT_NONE && inheritClaim[s] != TEX_INSTANT_NONE)
			{
				srClaim[s] = inheritClaim[s];
				srInherited[s] = true;
			}
		}


		for (uint8_t s = 0; s < INSTANT_SLOT_MAX; s++)
		{
			const bool isDS = s == INSTANT_SLOT_DS;
			const bool readOnly = isDS && action.dsReadOnly;
			auto& logicalList = isDS ? dsInstantLogicalList : rtInstantLogicalList;

			const TexID texId = isDS ? action.dsTexId : action.rtTexIds[s];
			uint16_t& outIdx = isDS ? action.dsInstantIdx : action.rtInstantIdx[s];

			if (texId != 0)
			{
				if (outClaim[s] != TEX_INSTANT_NONE)
					DebugReport(DataHub::RenderReport_Contract, L"[Graph] 로디드 출력이라 선행 인스턴트 이어 그리기 무시");
				continue;
			}

			if (outIdx != TEX_INSTANT_WANT)
				continue;

			if (outClaim[s] != TEX_INSTANT_NONE)
			{
				const uint16_t logicalIdx = outClaim[s];
				const DX12_InstantLogical& logical = logicalList[logicalIdx];

				if (srClaim[s] == logicalIdx && !readOnly)
				{
					if (!srInherited[s])
						DebugReport(DataHub::RenderReport_Contract, L"[Graph] 같은 인스턴트를 읽으면서 씀 (읽기 제외, 읽기 유지하려면 Copy 로)");
					srClaim[s] = TEX_INSTANT_NONE;
					srInherited[s] = false;
				}

				if (!IsFrontAction(action, logical.ownerActionIdx))
					DebugReport(DataHub::RenderReport_Contract, L"[Graph] 선행 노드 출력이 아닌 인스턴트에 이어 그림 (다른 액션이 이미 이어받음)");
				else if (logical.readAfterWrite && !readOnly)
					DebugReport(DataHub::RenderReport_Contract, L"[Graph] 다른 액션이 읽은 인스턴트에 이어 그림 (형제 노드 순서에 따라 결과 달라짐)");

				outIdx = logicalIdx;
			}
			else
			{
				if (readOnly)
					DebugReport(DataHub::RenderReport_Contract, L"[Graph] 읽기전용 DS 인데 이어받을 DS 없음");

				outIdx = (uint16_t)logicalList.size();
				logicalList.push_back(DX12_InstantLogical{i, i, i, false, TEX_INSTANT_NONE});
			}
		}


		for (uint8_t s = 0; s < INSTANT_SLOT_MAX; s++)
		{
			const bool isDS = s == INSTANT_SLOT_DS;
			auto& logicalList = isDS ? dsInstantLogicalList : rtInstantLogicalList;

			action.srInstantIdx[s] = srClaim[s];
			action.copySrcInstantIdx[s] = copySrc[s];

			if (srClaim[s] != TEX_INSTANT_NONE)
			{
				logicalList[srClaim[s]].readAfterWrite = true;
				logicalList[srClaim[s]].lastUse = i;
			}

			if (copySrc[s] != TEX_INSTANT_NONE)
			{
				logicalList[copySrc[s]].readAfterWrite = true;
				logicalList[copySrc[s]].lastUse = i;
			}

			const uint16_t outIdx = isDS ? action.dsInstantIdx : action.rtInstantIdx[s];

			if (outIdx != TEX_INSTANT_NONE)
			{
				logicalList[outIdx].lastUse = i;
				logicalList[outIdx].ownerActionIdx = i;
				if (!(isDS && action.dsReadOnly))
					logicalList[outIdx].readAfterWrite = false;
			}
		}
	}

	#pragma endregion


	#pragma region AllocateInstant

	for (uint8_t k = 0; k < 2; k++)
	{
		auto& logicalList = (k == 0) ? rtInstantLogicalList : dsInstantLogicalList;
		const uint32_t logicalCount = (uint32_t)logicalList.size();

		uint16_t physicalCount = 0;
		uint32_t cursor = 0;
		instantFreeList.clear();

		for (uint32_t i = 1; i < actionCount; i++)
		{
			while (cursor < logicalCount && logicalList[cursor].firstUse == i)
			{
				if (!instantFreeList.empty())
				{
					logicalList[cursor].physicalIdx = instantFreeList.back();
					instantFreeList.pop_back();
				}
				else
				{
					logicalList[cursor].physicalIdx = physicalCount++;
				}

				cursor++;
			}

			for (uint32_t l = 0; l < cursor; l++)
			{
				if (logicalList[l].lastUse == i)
					instantFreeList.push_back(logicalList[l].physicalIdx);
			}
		}

		if (k == 0)
			layer.rtInstantCount = physicalCount;
		else
			layer.dsInstantCount = physicalCount;
	}

	for (uint32_t i = 1; i < actionCount; i++)
	{
		DX12_RenderAction& action = actionList[i];

		for (uint8_t r = 0; r < RENDER_TARGET_MAX; r++)
		{
			if (action.rtInstantIdx[r] != TEX_INSTANT_NONE)
				action.rtInstantIdx[r] = rtInstantLogicalList[action.rtInstantIdx[r]].physicalIdx;

			if (action.srInstantIdx[r] != TEX_INSTANT_NONE)
				action.srInstantIdx[r] = rtInstantLogicalList[action.srInstantIdx[r]].physicalIdx;

			if (action.copySrcInstantIdx[r] != TEX_INSTANT_NONE)
				action.copySrcInstantIdx[r] = rtInstantLogicalList[action.copySrcInstantIdx[r]].physicalIdx;
		}

		if (action.dsInstantIdx != TEX_INSTANT_NONE)
			action.dsInstantIdx = dsInstantLogicalList[action.dsInstantIdx].physicalIdx;

		if (action.srInstantIdx[INSTANT_SLOT_DS] != TEX_INSTANT_NONE)
			action.srInstantIdx[INSTANT_SLOT_DS] = dsInstantLogicalList[action.srInstantIdx[INSTANT_SLOT_DS]].physicalIdx;

		if (action.copySrcInstantIdx[INSTANT_SLOT_DS] != TEX_INSTANT_NONE)
			action.copySrcInstantIdx[INSTANT_SLOT_DS] = dsInstantLogicalList[action.copySrcInstantIdx[INSTANT_SLOT_DS]].physicalIdx;
	}

	#pragma endregion


	#pragma region Barrier

	rtInstantSimStates.assign(layer.rtInstantCount, GetInstantBaseState(false));
	dsInstantSimStates.assign(layer.dsInstantCount, GetInstantBaseState(true));

	for (uint32_t i = 1; i < actionCount; i++)
	{
		BuildActionBarriers(actionList[i]);

		if (actionList[i].actionType == DX12_RenderActionType_Compute)
		{
			actionList[i].outputSigId = OUTPUT_SIGNATURE_INVALID;
		}
		else
		{
			bool drawable = actionList[i].dsTexId == 0;
			bool hole = false;

			for (uint8_t r = 0; r < RENDER_TARGET_MAX; r++)
			{
				if (actionList[i].rtTexIds[r] != 0)
					drawable = false;

				if (actionList[i].rtInstantIdx[r] == TEX_INSTANT_NONE)
					hole = true;
				else if (hole)
					drawable = false;
			}

			if (!drawable)
			{
				DebugReport(DataHub::RenderReport_Contract, L"[Graph] 로디드 RT/DS 출력 (미지원) 이거나 RT 출력 칸이 연속이 아님 (액션 스킵)");
				actionList[i].outputSigId = OUTPUT_SIGNATURE_INVALID;
			}
			else
			{
				const Action_Output& output = GetActionSource(layerDesc, layer.renderActionNodeList[i].actionId).output;
				actionList[i].outputSigId = GetOutputSignatureId(MakeOutputSignature(actionList[i], output));
			}
		}
	}

	layer.finalBarriers.clear();

	for (uint16_t p = 0; p < layer.rtInstantCount; p++)
	{
		if (rtInstantSimStates[p] != GetInstantBaseState(false))
			layer.finalBarriers.push_back(DX12_ActionBarrier{p, DX12_BarrierTarget_RTInstant, rtInstantSimStates[p], GetInstantBaseState(false)});
	}

	for (uint16_t p = 0; p < layer.dsInstantCount; p++)
	{
		if (dsInstantSimStates[p] != GetInstantBaseState(true))
			layer.finalBarriers.push_back(DX12_ActionBarrier{p, DX12_BarrierTarget_DSInstant, dsInstantSimStates[p], GetInstantBaseState(true)});
	}

	#pragma endregion
}

void D3D12Renderer::BuildActionBarriers(DX12_RenderAction& action)
{
	const bool isCompute = action.actionType == DX12_RenderActionType_Compute;

	action.copyBarriers.clear();
	action.beginBarriers.clear();
	action.endBarriers.clear();


	#pragma region Copy

	for (uint8_t s = 0; s < INSTANT_SLOT_MAX; s++)
	{
		const uint16_t srcIdx = action.copySrcInstantIdx[s];

		if (srcIdx == TEX_INSTANT_NONE)
			continue;

		const uint16_t dstIdx = action.srInstantIdx[s];
		const bool isDS = s == INSTANT_SLOT_DS;
		auto& simStates = isDS ? dsInstantSimStates : rtInstantSimStates;
		const DX12_BarrierTarget target = isDS ? DX12_BarrierTarget_DSInstant : DX12_BarrierTarget_RTInstant;

		if (dstIdx == TEX_INSTANT_NONE || srcIdx >= simStates.size() || dstIdx >= simStates.size())
		{
			DebugReport(DataHub::RenderReport_Invariant, L"[Barrier] 복제 인스턴트 인덱스 이상 (복제 취소)");
			action.copySrcInstantIdx[s] = TEX_INSTANT_NONE;
			action.srInstantIdx[s] = TEX_INSTANT_NONE;
			continue;
		}

		TransitionInstant(action.copyBarriers, target, srcIdx, simStates[srcIdx], D3D12_RESOURCE_STATE_COPY_SOURCE);
		TransitionInstant(action.copyBarriers, target, dstIdx, simStates[dstIdx], D3D12_RESOURCE_STATE_COPY_DEST);
	}

	#pragma endregion


	#pragma region InputSR

	for (uint8_t s = 0; s < INSTANT_SLOT_MAX; s++)
	{
		const uint16_t instIdx = action.srInstantIdx[s];

		if (instIdx == TEX_INSTANT_NONE)
			continue;

		const bool isDS = s == INSTANT_SLOT_DS;
		auto& simStates = isDS ? dsInstantSimStates : rtInstantSimStates;

		if (instIdx >= simStates.size())
		{
			DebugReport(DataHub::RenderReport_Invariant, L"[Barrier] 읽기 인스턴트 인덱스 범위 밖");
			continue;
		}

		TransitionInstant(action.beginBarriers, isDS ? DX12_BarrierTarget_DSInstant : DX12_BarrierTarget_RTInstant,
			instIdx, simStates[instIdx], GetInstantReadState(action.actionType, isDS));
	}

	#pragma endregion


	#pragma region OutputRT

	for (uint8_t r = 0; r < RENDER_TARGET_MAX; r++)
	{
		const TexID texId = action.rtTexIds[r];
		const uint16_t instIdx = action.rtInstantIdx[r];

		if (texId != 0)
		{
			const DX12_TextureUsage needUsage = isCompute ? DX12_TextureUsage_UAV : DX12_TextureUsage_RTV;
			bool valid = true;

			if (texId >= textureList.size() || (texId < releaseTexIdMask.size() && releaseTexIdMask[texId]))
			{
				DebugReport(DataHub::RenderReport_Contract, L"[Barrier] 출력 RT 텍스처 없음 / 해제 예정");
				valid = false;
			}
			else if (textureList[texId].usage != needUsage)
			{
				DebugReport(DataHub::RenderReport_Contract, L"[Barrier] 출력 RT 텍스처 usage 불일치");
				valid = false;
			}
			else
			{
				for (uint8_t i = 0; i < r; i++)
				{
					if (action.rtTexIds[i] == texId)
					{
						DebugReport(DataHub::RenderReport_Contract, L"[Barrier] 같은 텍스처를 RT 두 슬롯에 지정");
						valid = false;
						break;
					}
				}
			}

			if (!valid)
			{
				action.rtTexIds[r] = 0;
				continue;
			}

			TransitionLoaded(action, texId, textureList[texId],
				isCompute ? D3D12_RESOURCE_STATE_UNORDERED_ACCESS : D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
		else if (instIdx != TEX_INSTANT_NONE)
		{
			if (instIdx >= rtInstantSimStates.size())
			{
				DebugReport(DataHub::RenderReport_Invariant, L"[Barrier] 출력 RT 인스턴트 인덱스 범위 밖");
				action.rtInstantIdx[r] = TEX_INSTANT_NONE;
				continue;
			}

			TransitionInstant(action.beginBarriers, DX12_BarrierTarget_RTInstant, instIdx,
				rtInstantSimStates[instIdx], D3D12_RESOURCE_STATE_RENDER_TARGET);
		}
	}

	#pragma endregion


	#pragma region OutputDS

	const TexID dsTexId = action.dsTexId;
	const uint16_t dsInstIdx = action.dsInstantIdx;
	const D3D12_RESOURCE_STATES dsUse = action.dsReadOnly ? D3D12_RESOURCE_STATE_DEPTH_READ : D3D12_RESOURCE_STATE_DEPTH_WRITE;

	if (dsTexId != 0)
	{
		bool valid = true;

		if (dsTexId >= textureList.size() || (dsTexId < releaseTexIdMask.size() && releaseTexIdMask[dsTexId]))
		{
			DebugReport(DataHub::RenderReport_Contract, L"[Barrier] 출력 DS 텍스처 없음 / 해제 예정");
			valid = false;
		}
		else if (textureList[dsTexId].usage != DX12_TextureUsage_DSV)
		{
			DebugReport(DataHub::RenderReport_Contract, L"[Barrier] 출력 DS 텍스처 usage 불일치");
			valid = false;
		}

		if (valid)
			TransitionLoaded(action, dsTexId, textureList[dsTexId], dsUse);
		else
			action.dsTexId = 0;
	}
	else if (dsInstIdx != TEX_INSTANT_NONE)
	{
		if (dsInstIdx >= dsInstantSimStates.size())
		{
			DebugReport(DataHub::RenderReport_Invariant, L"[Barrier] 출력 DS 인스턴트 인덱스 범위 밖");
			action.dsInstantIdx = TEX_INSTANT_NONE;
		}
		else
		{
			TransitionInstant(action.beginBarriers, DX12_BarrierTarget_DSInstant, dsInstIdx,
				dsInstantSimStates[dsInstIdx], dsUse);
		}
	}

	#pragma endregion
}


void D3D12Renderer::RecordActionBarriers(ID3D12GraphicsCommandList* cmdList, const DX12_RenderLayer& layer,
	const vector<DX12_ActionBarrier>& barriers, UINT frameIdx)
{
	if (barriers.empty())
		return;

	const auto& rtInstants = rtTextureInstantLists[frameIdx];
	const auto& dsInstants = dsTextureInstantLists[frameIdx];

	barrierScratch.clear();

	for (const DX12_ActionBarrier& barrier : barriers)
	{
		ID3D12Resource* res;

		switch (barrier.target)
		{
		case DX12_BarrierTarget_Loaded:		res = textureList[barrier.id].Tex.Get();							break;
		case DX12_BarrierTarget_RTInstant:	res = rtInstants[barrier.id + layer.rtInstantOffset].Tex.Get();	break;
		default:							res = dsInstants[barrier.id + layer.dsInstantOffset].Tex.Get();	break;
		}

		barrierScratch.push_back(CD3DX12_RESOURCE_BARRIER::Transition(res, barrier.before, barrier.after));
	}

	cmdList->ResourceBarrier((UINT)barrierScratch.size(), barrierScratch.data());
}

#pragma endregion

#pragma endregion

}
