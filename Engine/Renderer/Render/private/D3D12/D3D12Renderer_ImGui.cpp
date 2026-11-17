#include "D3D12Renderer.h"

#if RENDER_IMGUI_MODE == RENDER_IMGUI_ENABLE

#include "D3D12Core.h"

#include "imgui.h"


namespace Hakko
{

#pragma region Helper

static bool CompileImGuiShader(IDxcUtils* utils, IDxcCompiler3* compiler, IDxcIncludeHandler* includeHandler,
	const std::wstring& path, const wchar_t* entry, const wchar_t* profile, ComPtr<IDxcBlob>& outBlob)
{
	ComPtr<IDxcBlobEncoding> source;
	if (FAILED(utils->LoadFile(path.c_str(), nullptr, &source)))
	{
		DebugReport(DataHub::RenderReport_Init, L"[ImGui] 셰이더 파일 로드 실패");
		return false;
	}

	DxcBuffer sourceBuffer = { source->GetBufferPointer(), source->GetBufferSize(), DXC_CP_ACP };

	std::vector<LPCWSTR> args;
	args.push_back(L"-E");	args.push_back(entry);
	args.push_back(L"-T");	args.push_back(profile);
	#ifdef _DEBUG
	args.push_back(L"-Zi");
	args.push_back(L"-Od");
	args.push_back(L"-Qembed_debug");
	#else
	args.push_back(L"-O3");
	#endif

	ComPtr<IDxcResult> result;
	HRESULT hr = compiler->Compile(&sourceBuffer, args.data(), (UINT32)args.size(), includeHandler, IID_PPV_ARGS(&result));

	HRESULT status = E_FAIL;
	if (SUCCEEDED(hr) && result != nullptr)
		result->GetStatus(&status);

	if (FAILED(status))
	{
		DebugReport(DataHub::RenderReport_Init, L"[ImGui] 셰이더 컴파일 실패");
		#ifdef _DEBUG
		ComPtr<IDxcBlobUtf8> errors;
		if (result != nullptr && SUCCEEDED(result->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&errors), nullptr)) && errors != nullptr && errors->GetStringLength() > 0)
		{
			OutputDebugStringA(errors->GetStringPointer());
			OutputDebugStringA("\n");
		}
		#endif
		return false;
	}

	return SUCCEEDED(result->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&outBlob), nullptr)) && outBlob != nullptr;
}

static bool IsLinearColorFormat(DXGI_FORMAT format)
{
	switch (format)
	{
	case DXGI_FORMAT_R8G8B8A8_UNORM_SRGB:
	case DXGI_FORMAT_B8G8R8A8_UNORM_SRGB:
	case DXGI_FORMAT_BC1_UNORM_SRGB:
	case DXGI_FORMAT_BC2_UNORM_SRGB:
	case DXGI_FORMAT_BC3_UNORM_SRGB:
	case DXGI_FORMAT_BC7_UNORM_SRGB:
	case DXGI_FORMAT_R11G11B10_FLOAT:
	case DXGI_FORMAT_R16_FLOAT:
	case DXGI_FORMAT_R16G16_FLOAT:
	case DXGI_FORMAT_R16G16B16A16_FLOAT:
	case DXGI_FORMAT_R32_FLOAT:
	case DXGI_FORMAT_R32G32_FLOAT:
	case DXGI_FORMAT_R32G32B32A32_FLOAT:
	case DXGI_FORMAT_R9G9B9E5_SHAREDEXP:
	case DXGI_FORMAT_BC6H_UF16:
	case DXGI_FORMAT_BC6H_SF16:
		return true;
	default:
		return false;
	}
}

#pragma endregion



#pragma region ImGui

D3D12Renderer::ImGuiModule::ImGuiModule(D3D12Renderer& renderer) : renderer(renderer)
{
}

bool D3D12Renderer::ImGuiModule::Init()
{
	ID3D12Device* device = D3D12Core::getInstance().GetDevice();

	#pragma region RootSignature

	CD3DX12_DESCRIPTOR_RANGE1 texRange;
	texRange.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0U, D3D12_DESCRIPTOR_RANGE_FLAG_DATA_VOLATILE);

	CD3DX12_ROOT_PARAMETER1 rootParams[3];
	rootParams[0].InitAsConstants(4, 0, 0U, D3D12_SHADER_VISIBILITY_VERTEX);
	rootParams[1].InitAsDescriptorTable(1, &texRange, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParams[2].InitAsConstants(1, 1, 0U, D3D12_SHADER_VISIBILITY_PIXEL);

	D3D12_STATIC_SAMPLER_DESC sampler = {};
	sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
	sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
	sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
	sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
	sampler.MinLOD = 0.0f;
	sampler.MaxLOD = D3D12_FLOAT32_MAX;
	sampler.ShaderRegister = 0;
	sampler.RegisterSpace = 0;
	sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	if (FAILED(device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData))))
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSigDesc;
	rootSigDesc.Init_1_1(_countof(rootParams), rootParams, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	ComPtr<ID3DBlob> rootSigRaw;
	ComPtr<ID3DBlob> error;

	if (FAILED(D3DX12SerializeVersionedRootSignature(&rootSigDesc, featureData.HighestVersion, &rootSigRaw, &error))
		|| FAILED(device->CreateRootSignature(0, rootSigRaw->GetBufferPointer(), rootSigRaw->GetBufferSize(), IID_PPV_ARGS(&rootSig))))
	{
		DebugReport(DataHub::RenderReport_Init, L"[ImGui] 루트 시그니처 생성 실패");
		return false;
	}

	#pragma endregion


	#pragma region PSO

	ComPtr<IDxcBlob> vsBlob;
	ComPtr<IDxcBlob> psBlob;

	if (!CompileImGuiShader(renderer.dxcUtils.Get(), renderer.dxcCompiler.Get(), renderer.dxcIncludeHandler.Get(), renderer.defaultShaderDir + L"/imgui_VS.hlsl", L"VSMain", L"vs_6_0", vsBlob)
		|| !CompileImGuiShader(renderer.dxcUtils.Get(), renderer.dxcCompiler.Get(), renderer.dxcIncludeHandler.Get(), renderer.defaultShaderDir + L"/imgui_PS.hlsl", L"PSMain", L"ps_6_0", psBlob))
		return false;

	const D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	{
		{ "POSITION",	0, DXGI_FORMAT_R32G32_FLOAT,	0, (UINT)offsetof(ImDrawVert, pos),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD",	0, DXGI_FORMAT_R32G32_FLOAT,	0, (UINT)offsetof(ImDrawVert, uv),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR",		0, DXGI_FORMAT_R8G8B8A8_UNORM,	0, (UINT)offsetof(ImDrawVert, col),	D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSig.Get();
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };
	psoDesc.InputLayout = { inputLayout, _countof(inputLayout) };
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_UNKNOWN;
	psoDesc.SampleDesc.Count = 1;
	psoDesc.SampleMask = UINT_MAX;

	psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	psoDesc.BlendState.RenderTarget[0].BlendEnable = TRUE;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;

	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	psoDesc.DepthStencilState.DepthEnable = FALSE;
	psoDesc.DepthStencilState.StencilEnable = FALSE;

	if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso))))
	{
		DebugReport(DataHub::RenderReport_Init, L"[ImGui] PSO 생성 실패");
		return false;
	}

	#pragma endregion


	#pragma region BackBufferRTV

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (UINT n = 0; n < FRAME_COUNT; n++)
	{
		if (!renderer.rtvHeap->SetRTV(renderer.backBuffers[n].Get(), &rtvDesc, bbRTVIndices[n]))
		{
			DebugReport(DataHub::RenderReport_Init, L"[ImGui] 백버퍼 RTV 자리 없음");
			return false;
		}
	}

	#pragma endregion


	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGuiIO& io = ImGui::GetIO();
		io.BackendRendererName = "Hakko_D3D12";
		io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;
	}

	return true;
}

void D3D12Renderer::ImGuiModule::Finalize()
{
	if (ImGui::GetCurrentContext() != nullptr)
	{
		ImGuiPlatformIO& platformIO = ImGui::GetPlatformIO();
		for (int i = 0; i < platformIO.Textures.Size; i++)
		{
			ImTextureData* tex = platformIO.Textures[i];
			if (tex->BackendUserData == nullptr)
				continue;

			tex->BackendUserData = nullptr;
			tex->SetTexID(ImTextureID_Invalid);
			tex->SetStatus(ImTextureStatus_Destroyed);
		}

		ImGuiIO& io = ImGui::GetIO();
		io.BackendRendererName = nullptr;
		io.BackendFlags &= ~(ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures);
	}

	textureSlotList.clear();

	for (UINT n = 0; n < FRAME_COUNT; n++)
		frames[n] = {};

	pso.Reset();
	rootSig.Reset();
}

void D3D12Renderer::ImGuiModule::Resize()
{
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	for (UINT n = 0; n < FRAME_COUNT; n++)
		renderer.rtvHeap->UpdateRTV(bbRTVIndices[n], renderer.backBuffers[n].Get(), &rtvDesc);
}


void D3D12Renderer::ImGuiModule::Prepare()
{
	DX12_ImGuiFrame& frame = frames[renderer.cpuFrameIndex];
	frame.drawList.clear();

	if (ImGui::GetCurrentContext() == nullptr)
		return;

	ImGuiIO& io = ImGui::GetIO();
	io.BackendRendererName = "Hakko_D3D12";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset | ImGuiBackendFlags_RendererHasTextures;

	ImDrawData* drawData = ImGui::GetDrawData();
	if (drawData == nullptr)
		return;


	#pragma region Texture

	if (drawData->Textures != nullptr)
	{
		for (int i = 0; i < drawData->Textures->Size; i++)
		{
			ImTextureData* tex = (*drawData->Textures)[i];

			if (tex->Status == ImTextureStatus_WantCreate || tex->Status == ImTextureStatus_WantUpdates)
				CreateTexture(tex);
			else if (tex->Status == ImTextureStatus_WantDestroy)
				DestroyTexture(tex);
		}
	}

	#pragma endregion


	const float fbWidth = drawData->DisplaySize.x * drawData->FramebufferScale.x;
	const float fbHeight = drawData->DisplaySize.y * drawData->FramebufferScale.y;

	if (fbWidth <= 0.0f || fbHeight <= 0.0f || drawData->TotalVtxCount <= 0 || drawData->TotalIdxCount <= 0)
		return;


	#pragma region Buffer

	ID3D12Device* device = D3D12Core::getInstance().GetDevice();
	const CD3DX12_RANGE readRange(0, 0);

	const UINT vbBytes = (UINT)drawData->TotalVtxCount * (UINT)sizeof(ImDrawVert);
	const UINT ibBytes = (UINT)drawData->TotalIdxCount * (UINT)sizeof(ImDrawIdx);

	if (vbBytes > frame.VBCapacity)
	{
		const UINT newCapacity = vbBytes + vbBytes / 2;
		const auto desc = CD3DX12_RESOURCE_DESC::Buffer(newCapacity);

		frame.VB.Reset();
		frame.VBMapped = nullptr;
		frame.VBCapacity = 0;

		if (FAILED(device->CreateCommittedResource(&renderer.UPLOAD_HEAP_PROPS, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&frame.VB)))
			|| FAILED(frame.VB->Map(0, &readRange, (void**)&frame.VBMapped)))
		{
			DebugReport(DataHub::RenderReport_Limit, L"[ImGui] 정점 버퍼 할당 실패, 이번 프레임 ImGui 안 그림");
			frame.VB.Reset();
			frame.VBMapped = nullptr;
			return;
		}
		frame.VBCapacity = newCapacity;
	}

	if (ibBytes > frame.IBCapacity)
	{
		const UINT newCapacity = ibBytes + ibBytes / 2;
		const auto desc = CD3DX12_RESOURCE_DESC::Buffer(newCapacity);

		frame.IB.Reset();
		frame.IBMapped = nullptr;
		frame.IBCapacity = 0;

		if (FAILED(device->CreateCommittedResource(&renderer.UPLOAD_HEAP_PROPS, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&frame.IB)))
			|| FAILED(frame.IB->Map(0, &readRange, (void**)&frame.IBMapped)))
		{
			DebugReport(DataHub::RenderReport_Limit, L"[ImGui] 인덱스 버퍼 할당 실패, 이번 프레임 ImGui 안 그림");
			frame.IB.Reset();
			frame.IBMapped = nullptr;
			return;
		}
		frame.IBCapacity = newCapacity;
	}

	frame.VBV.BufferLocation = frame.VB->GetGPUVirtualAddress();
	frame.VBV.SizeInBytes = vbBytes;
	frame.VBV.StrideInBytes = (UINT)sizeof(ImDrawVert);

	frame.IBV.BufferLocation = frame.IB->GetGPUVirtualAddress();
	frame.IBV.SizeInBytes = ibBytes;
	frame.IBV.Format = sizeof(ImDrawIdx) == 2 ? DXGI_FORMAT_R16_UINT : DXGI_FORMAT_R32_UINT;

	#pragma endregion


	#pragma region Projection

	const float L = drawData->DisplayPos.x;
	const float R = drawData->DisplayPos.x + drawData->DisplaySize.x;
	const float T = drawData->DisplayPos.y;
	const float B = drawData->DisplayPos.y + drawData->DisplaySize.y;

	frame.projection[0] = 2.0f / (R - L);
	frame.projection[1] = 2.0f / (T - B);
	frame.projection[2] = (R + L) / (L - R);
	frame.projection[3] = (T + B) / (B - T);

	frame.viewport = { 0.0f, 0.0f, fbWidth, fbHeight, 0.0f, 1.0f };

	#pragma endregion


	#pragma region DrawList

	ImDrawVert* vtxDst = (ImDrawVert*)frame.VBMapped;
	ImDrawIdx* idxDst = (ImDrawIdx*)frame.IBMapped;

	const ImVec2 clipOffset = drawData->DisplayPos;
	const ImVec2 clipScale = drawData->FramebufferScale;

	UINT globalVtx = 0;
	UINT globalIdx = 0;

	UINT64 prevTexKey = ImTextureID_Invalid;
	UINT64 prevTexHandle = 0;
	UINT prevDrawOption = 0;

	for (int n = 0; n < drawData->CmdListsCount; n++)
	{
		const ImDrawList* list = drawData->CmdLists[n];

		memcpy(vtxDst + globalVtx, list->VtxBuffer.Data, (size_t)list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idxDst + globalIdx, list->IdxBuffer.Data, (size_t)list->IdxBuffer.Size * sizeof(ImDrawIdx));

		for (int c = 0; c < list->CmdBuffer.Size; c++)
		{
			const ImDrawCmd& cmd = list->CmdBuffer[c];

			if (cmd.UserCallback != nullptr)
			{
				DebugReport(DataHub::RenderReport_Contract, L"[ImGui] 드로우 콜백은 지원 안 함 (무시)");
				continue;
			}

			float x1 = (cmd.ClipRect.x - clipOffset.x) * clipScale.x;
			float y1 = (cmd.ClipRect.y - clipOffset.y) * clipScale.y;
			float x2 = (cmd.ClipRect.z - clipOffset.x) * clipScale.x;
			float y2 = (cmd.ClipRect.w - clipOffset.y) * clipScale.y;

			if (x1 < 0.0f)		x1 = 0.0f;
			if (y1 < 0.0f)		y1 = 0.0f;
			if (x2 > fbWidth)	x2 = fbWidth;
			if (y2 > fbHeight)	y2 = fbHeight;

			if (x2 <= x1 || y2 <= y1 || cmd.ElemCount == 0)
				continue;

			const UINT64 texKey = (UINT64)(cmd.TexRef._TexData != nullptr ? cmd.TexRef._TexData->TexID : cmd.TexRef._TexID);
			if (texKey == ImTextureID_Invalid)
				continue;

			if (texKey != prevTexKey)
			{
				prevTexKey = texKey;
				prevTexHandle = ResolveTexture(texKey, prevDrawOption);
			}

			DX12_ImGuiDraw draw;
			draw.scissorRect = { (LONG)x1, (LONG)y1, (LONG)x2, (LONG)y2 };
			draw.texHandle = prevTexHandle;
			draw.drawOption = prevDrawOption;
			draw.indexStart = cmd.IdxOffset + globalIdx;
			draw.indexCount = cmd.ElemCount;
			draw.baseVertex = (INT)(cmd.VtxOffset + globalVtx);

			frame.drawList.push_back(draw);
		}

		globalVtx += (UINT)list->VtxBuffer.Size;
		globalIdx += (UINT)list->IdxBuffer.Size;
	}

	#pragma endregion
}

void D3D12Renderer::ImGuiModule::Record(ID3D12GraphicsCommandList* cmdList, UINT frameIdx)
{
	const DX12_ImGuiFrame& frame = frames[frameIdx];

	if (frame.drawList.empty())
		return;

	const D3D12_CPU_DESCRIPTOR_HANDLE rtv = renderer.rtvHeap->GetCPUHandle(bbRTVIndices[renderer.backBufferIndex]);
	cmdList->OMSetRenderTargets(1, &rtv, FALSE, nullptr);
	cmdList->RSSetViewports(1, &frame.viewport);

	cmdList->SetGraphicsRootSignature(rootSig.Get());
	cmdList->SetPipelineState(pso.Get());
	cmdList->SetGraphicsRoot32BitConstants(0, 4, frame.projection, 0);

	cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	cmdList->IASetVertexBuffers(0, 1, &frame.VBV);
	cmdList->IASetIndexBuffer(&frame.IBV);

	UINT64 boundTex = 0;
	UINT boundDrawOption = 0;
	cmdList->SetGraphicsRoot32BitConstant(2, boundDrawOption, 0);

	for (size_t i = 0; i < frame.drawList.size(); i++)
	{
		const DX12_ImGuiDraw& draw = frame.drawList[i];

		cmdList->RSSetScissorRects(1, &draw.scissorRect);

		if (draw.texHandle != boundTex)
		{
			const D3D12_GPU_DESCRIPTOR_HANDLE handle = { draw.texHandle };
			cmdList->SetGraphicsRootDescriptorTable(1, handle);
			boundTex = draw.texHandle;
		}

		if (draw.drawOption != boundDrawOption)
		{
			cmdList->SetGraphicsRoot32BitConstant(2, draw.drawOption, 0);
			boundDrawOption = draw.drawOption;
		}

		cmdList->DrawIndexedInstanced(draw.indexCount, 1, draw.indexStart, draw.baseVertex, 0);
	}
}


bool D3D12Renderer::ImGuiModule::CreateTexture(ImTextureData* tex)
{
	using namespace DataHub;

	const UINT width = (UINT)tex->Width;
	const UINT height = (UINT)tex->Height;
	const UINT pitch = width * 4;

	Texture src = {};
	src.data.format = TextureReadFormat::RGBA8_UNorm;
	src.data.textureType = TextureType_2D;
	src.data.width = width;
	src.data.height = height;
	src.rawData.resize((size_t)pitch * height);
	src.subresources.push_back({ 0, pitch, pitch * height });

	if (tex->Format == ImTextureFormat_RGBA32)
	{
		memcpy(src.rawData.data(), tex->GetPixels(), src.rawData.size());
	}
	else
	{
		const unsigned char* alpha = (const unsigned char*)tex->GetPixels();
		for (size_t p = 0; p < (size_t)width * height; p++)
		{
			src.rawData[p * 4 + 0] = (std::byte)255;
			src.rawData[p * 4 + 1] = (std::byte)255;
			src.rawData[p * 4 + 2] = (std::byte)255;
			src.rawData[p * 4 + 3] = (std::byte)alpha[p];
		}
	}


	DX12_ImGuiTexture created;
	vector<ComPtr<ID3D12Resource>> uploadBufferList;

	renderer.BeginUploadCommands();
	const bool texOk = renderer.CreateTextureResource(src, created.texture, uploadBufferList);
	renderer.FlushUploadCommands();

	if (!texOk)
	{
		DebugReport(RenderReport_Contract, L"[ImGui] 텍스처 생성 실패");
		return false;
	}

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = created.texture.Format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1;

	if (!renderer.rvHeap->SetSRV(created.texture.Tex.Get(), &srvDesc, created.RVHeapIndex))
	{
		DebugReport(RenderReport_Limit, L"[ImGui] 텍스처 뷰 자리 없음 (디스크립터 힙 한도)");
		return false;
	}


	const UINT retireSlot = (renderer.cpuFrameIndex + FRAME_COUNT - 1) % FRAME_COUNT;
	size_t slot = (size_t)tex->BackendUserData;

	if (slot != 0)
	{
		DX12_ImGuiTexture& old = textureSlotList[slot - 1];
		renderer.retiredTextureLists[retireSlot].push_back(std::move(old.texture));
		renderer.retiredSTexViewLists[retireSlot].push_back({ old.RVHeapIndex });
	}
	else
	{
		for (size_t i = 0; i < textureSlotList.size(); i++)
		{
			if (textureSlotList[i].RVHeapIndex == UINT_MAX)
			{
				slot = i + 1;
				break;
			}
		}

		if (slot == 0)
		{
			textureSlotList.push_back({});
			slot = textureSlotList.size();
		}
	}

	const UINT rvIndex = created.RVHeapIndex;
	textureSlotList[slot - 1] = std::move(created);

	tex->BackendUserData = (void*)slot;
	tex->SetTexID((ImTextureID)(((UINT64)DX12_ImGuiTexKind_Internal << IMGUI_TEXID_KIND_SHIFT) | (UINT64)(slot - 1)));
	tex->SetStatus(ImTextureStatus_OK);

	return true;
}

void D3D12Renderer::ImGuiModule::DestroyTexture(ImTextureData* tex)
{
	const size_t slot = (size_t)tex->BackendUserData;

	if (slot != 0 && slot <= textureSlotList.size())
	{
		const UINT retireSlot = (renderer.cpuFrameIndex + FRAME_COUNT - 1) % FRAME_COUNT;
		DX12_ImGuiTexture& old = textureSlotList[slot - 1];

		renderer.retiredTextureLists[retireSlot].push_back(std::move(old.texture));
		renderer.retiredSTexViewLists[retireSlot].push_back({ old.RVHeapIndex });
		old = {};
	}

	tex->BackendUserData = nullptr;
	tex->SetTexID(ImTextureID_Invalid);
	tex->SetStatus(ImTextureStatus_Destroyed);
}

UINT64 D3D12Renderer::ImGuiModule::ResolveTexture(UINT64 texKey, UINT& drawOption)
{
	using namespace DataHub;

	const UINT64 errorHandle = renderer.rvHeap->GetGPUHandle(renderer.singleTextureList[0].RVHeapIndex).ptr;
	const UINT kind = (UINT)((texKey >> IMGUI_TEXID_KIND_SHIFT) & IMGUI_TEXID_KIND_MASK);
	const UINT16 id = (UINT16)(texKey & IMGUI_TEXID_ID_MASK);

	drawOption = 0;

	switch (kind)
	{
	case DX12_ImGuiTexKind_Hub:
	{
		if (id >= renderer.textureList.size())
		{
			DebugReport(RenderReport_Contract, L"[ImGui] Image 의 texId 범위 밖 (에러 텍스처로)");
			return errorHandle;
		}

		#if RENDER_DEBUG_CHECK
		if (id < renderer.releaseTexIdMask.size() && renderer.releaseTexIdMask[id])
		{
			DebugReport(RenderReport_Contract, L"[ImGui] Image 의 텍스처가 해제 예정 (에러 텍스처로)");
			return errorHandle;
		}
		#endif

		const DX12_Texture& texture = renderer.textureList[id];

		if (texture.Dimension != D3D12_RESOURCE_DIMENSION_TEXTURE2D || texture.IsCube || texture.ArraySize != 1)
		{
			DebugReport(RenderReport_Contract, L"[ImGui] Image 는 2D 텍스처만 (에러 텍스처로)");
			return errorHandle;
		}

		if (!renderer.MarkOrBuildSingleTexture(id))
		{
			DebugReport(RenderReport_Limit, L"[ImGui] Image 텍스처 뷰 자리 없음 (에러 텍스처로)");
			return errorHandle;
		}

		if (IsLinearColorFormat(texture.Format))
			drawOption |= IMGUI_DRAW_OPTION_LINEAR_TO_GAMMA;

		return renderer.rvHeap->GetGPUHandle(renderer.singleTextureList[id].RVHeapIndex).ptr;
	}

	case DX12_ImGuiTexKind_Internal:
		if (id < textureSlotList.size() && textureSlotList[id].RVHeapIndex != UINT_MAX)
			return renderer.rvHeap->GetGPUHandle(textureSlotList[id].RVHeapIndex).ptr;

		DebugReport(RenderReport_Invariant, L"[ImGui] 내부 텍스처 칸이 비어 있음");
		return errorHandle;

	default:
		DebugReport(RenderReport_Contract, L"[ImGui] 모르는 텍스처 종류 (에러 텍스처로)");
		return errorHandle;
	}
}

#pragma endregion

}

#endif
