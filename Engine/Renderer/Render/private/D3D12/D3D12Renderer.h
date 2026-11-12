#pragma once

#include "RenderOption.h"

#include "D3D12Renderer_Types.h"

#include <cstdint>
#include <memory>


namespace Hakko
{

struct RenderLimits
{
	uint32_t rvDescriptors = 262144;
	uint32_t rtvDescriptors = 1024;
	uint32_t dsvDescriptors = 1024;
	uint32_t cbAreaInitNum = 1024;
	uint32_t cbAreaShrinkFreq = 180;
	uint32_t cbAreaShrinkThreshold = 4;
};

struct RenderTuning
{
	uint32_t resCollectFreq = 4096;
	uint32_t texCollectThreshold = 4096;
};

class D3D12Renderer
{
private:
	template <typename T> using vector = std::vector<T>;
	template <typename T, size_t N> using array = std::array<T, N>;
	template <typename T1, typename T2> using unordered_map = std::unordered_map<T1, T2>;
	template <typename T> using unique_ptr = std::unique_ptr<T>;
	template <typename T1, typename T2> using pair = std::pair<T1, T2>;


#pragma region Constructor
public:
	D3D12Renderer(const RenderLimits& limits);
	~D3D12Renderer() = default;

#pragma endregion
	

#pragma region Variable

	#pragma region Constant
private:
	static const UINT 		FRAME_COUNT = RENDER_BACKBUFFER_MODE;


	const RenderLimits		requestedLimits;


	UINT64					RES_COLLECT_FREQ;
	UINT16					STEX_COLLECT_THRESHOLD;

	const UINT 				RTV_HEAP_NUM_DESCS;
	const UINT 				DSV_HEAP_NUM_DESCS;
	const UINT 				RV_HEAP_NUM_DESCS;


	const UINT 				CB_AREA_INIT_SIZE_DEFAULT;
	const UINT				CB_AREA_SHRINK_FREQ;
	const UINT				CB_AREA_SHRINK_THRESHOLD;

	const UINT 				CB_AREA_SIZE_CONDITION = RENDER_LAYER_MAX * (UINT)(sizeof(DataHub::RenderCondition) / 256);
	const UINT 				CB_AREA_SIZE_LIGHT = RENDER_LIGHT_MAX;


	static const PSOID		SKYBOX_PASS_PSOID = RESERVED_DRAWPROP_SKYBOX_ID;
	static const PSOID		BG_PASS_PSOID = RESERVED_DRAWPROP_BG_ID;


	const CD3DX12_HEAP_PROPERTIES 	DEFAULT_HEAP_PROPS;
	const CD3DX12_HEAP_PROPERTIES 	UPLOAD_HEAP_PROPS;

	#pragma endregion


	#pragma region RenderCore
private:
	HWND								hWnd;

	bool								vsync;
	bool								tearingSupported;
	bool								suspended;
	std::wstring						defaultShaderDir;

	ComPtr<IDXGISwapChain3> 			swapChain;

	ComPtr<ID3D12CommandQueue> 			renderCMDQueue;
	ComPtr<ID3D12CommandAllocator> 		renderCMDAllocators[FRAME_COUNT];
	ComPtr<ID3D12GraphicsCommandList> 	renderCMDList_Master;


	ComPtr<IDxcUtils>					dxcUtils;
	ComPtr<IDxcCompiler3>				dxcCompiler;
	ComPtr<IDxcIncludeHandler>			dxcIncludeHandler;
	D3D_SHADER_MODEL					shaderModel = D3D_SHADER_MODEL_6_0;


	class RTVHeapAllocator;
	typedef RTVHeapAllocator RTVHeap;

	class DSVHeapAllocator;
	typedef DSVHeapAllocator DSVHeap;

	class RVHeapAllocator;
	typedef RVHeapAllocator RVHeap;


	std::unique_ptr<RTVHeap> 			rtvHeap;
	std::unique_ptr<DSVHeap> 			dsvHeap;
	std::unique_ptr<RVHeap> 			rvHeap;

	#pragma endregion


	#pragma region FrameResource
private:	
	UINT 								bbWidth;
	UINT 								bbHeight;

	ComPtr<ID3D12Resource> 				backBuffers[FRAME_COUNT];
	D3D12_CPU_DESCRIPTOR_HANDLE 		bbvCPUHandles[FRAME_COUNT];

	UINT 								backBufferIndex;
	UINT 								cpuFrameIndex;
	UINT64								currentFrameCount;

	ComPtr<ID3D12Fence> 				fence;
	HANDLE 								fenceEvent;
	UINT64 								currentFenceValue;
	UINT64 								frameFenceValues[FRAME_COUNT];
	

	DX12_RenderFrame 					renderFrames[FRAME_COUNT];
	UINT8								layerCount;

	#pragma endregion


	#pragma region RenderResource
private:
	ComPtr<ID3D12RootSignature> 			defaultRenderRootSig;	// 파라미터 배치는 DX12_RootParam, 셰이더 쪽은 common/registers.hlsli 참조

	
	vector<DataHub::ShaderInfo> 			shaderList;
	vector<DX12_CompiledShader>				compiledShaderList;

	DX12_OutputSignature					outputSigIds[OUTPUT_SIGNATURE_MAX];
	array<bool, RENDER_TARGET_MAX>			outputSigBlendableMasks[OUTPUT_SIGNATURE_MAX];
	uint8_t									outputSigCount = 1;

	ComPtr<ID3D12PipelineState>				compositePSO;
	
	uint8_t									formatBlendableCache[256] = {};
	vector<DX12_PSOGenerator>				psoGeneratorList;

	vector<DX12_Mesh>						meshList;
	vector<DX12_MeshView>					meshViewList;


    vector<DX12_Texture> 					textureList;
	vector<vector<pair<CTexID, uint8_t>>> 	texRefList_CTex;
	vector<bool> 							texRefMask_STex;
	vector<DX12_WTexRef>					texRefList_WTex;
	
	vector<DX12_TextureInstant> 			rtTextureInstantLists[FRAME_COUNT];
	vector<DX12_TextureInstant> 			dsTextureInstantLists[FRAME_COUNT];

	
	vector<DX12_CombinedTextureView> 		combinedTextureList;
	vector<DX12_SingleTextureView> 			singleTextureList;
	vector<UINT16>							frameMarkList_STex;
	vector<DX12_WritableTextureView>		writableTextureList;
	vector<uint16_t>						blankedIdxList_WTex;

	vector<DX12_SingleTextureView> 			rtSTexInstantLists[FRAME_COUNT];
	vector<DX12_WritableTextureView>		rtvInstantLists[FRAME_COUNT];
	vector<DX12_SingleTextureView> 			dsSTexInstantLists[FRAME_COUNT];
	vector<DX12_WritableTextureView>		dsvInstantLists[FRAME_COUNT];
	vector<DX12_WritableTextureView>		dsvReadOnlyInstantLists[FRAME_COUNT];


	vector<DX12_InstantLogical>				rtInstantLogicalList;
	vector<DX12_InstantLogical>				dsInstantLogicalList;
	vector<uint16_t>						instantFreeList;
	vector<D3D12_RESOURCE_STATES>			rtInstantSimStates;
	vector<D3D12_RESOURCE_STATES>			dsInstantSimStates;
	vector<D3D12_RESOURCE_BARRIER>			barrierScratch;


	vector<bool>							releaseMeshIdMask;
	int 									meshReleaseCount;
	vector<bool>							releaseTexIdMask;
	int 									texReleaseCount;
	vector<bool>							releasePSOIdMask;
	int 									psoReleaseCount;
	vector<bool>							releaseCTexIdMask;
	int 									cTexReleaseCount;


	vector<bool>							instantClearedRT;
	vector<bool>							instantClearedDS;
	
	#pragma endregion


	#pragma region RenderData
private:
	template <typename T> class CBAreaAllocator;
	template <typename T> using CBArea = CBAreaAllocator<T>;

	class CBAreaRaw;
	

	unique_ptr<CBArea<DataHub::RenderCondition>>	cbArea_Condition[FRAME_COUNT];
	vector<CBArea<DataHub::DataBox_RenderLight>>	cbArea_Light[FRAME_COUNT];
	
	vector<CBAreaRaw>								cbArea_DataBox[FRAME_COUNT];
	vector<CBAreaRaw>								cbArea_LightMatrix;


	vector<ComPtr<ID3D12Resource>>					retiredCBAreas;
	vector<uint32_t>								tidCounts;

	vector<vector<uint32_t>>						dataSlotLUT;
	vector<vector<uint32_t>>						dataSlotStamp;
	
	vector<DX12_RenderRequestProxy>					sortScratch;

	#pragma endregion

#pragma endregion


#pragma region Methods

	#pragma region Lifecycle
public:
	bool Init(HWND hwnd, int width, int height);
	void Finalize();

	void OnResize(int width, int height);
	void SetVSync(bool enable);
	void SetTuning(const RenderTuning& tuning);

	void Sketch();
	void Render();

	// Sketch 하위 동작
private:
	void SortRenderId();
	void GetConditionData();
	void GetDataFromSortedId();
	void UploadRenderData();

	#pragma endregion


	#pragma region FrameControl
private:
	void WaitGPUProcess();
	void SignalFence();

	#pragma endregion


	#pragma region LayerControl
private:
	bool InitCanvasLayer();

	void UpdateCanvasLayer();

	#pragma endregion


	#pragma region ResourceControl
private:
	bool InitResource();

	bool InitShaderCompiler();
	bool InitGraphicResource();
	void PreUploadResource();

	void InvalidateView();
	void CollectManagedResource();

	void ScheduledReleaseResource();


	void UploadMesh();	
	void UploadTexture();
	void GetShaderInfo();

	void BuildPSOBody();
	void BuildPSOVariant(PSOID psoId, OutputSignatureID sigId);
	void BuildCombinedTexture();


	void ReleaseMesh(vector<bool>& releaseResIdMask);
	void ReleaseTexture(vector<bool>& releaseResIdMask);

	void ReleasePSOBody(vector<bool>& releaseResIdMask);
	void ReleaseCombinedTexture(vector<bool>& releaseResIdMask);


	void ResizeTextureInstant();


	void MarkOrBuildSingleTexture(TexID texId);

	bool CompileShader(ShaderID shaderId, uint8_t variant, ComPtr<IDxcBlob>& outBlob);
	IDxcBlob* GetCompiledShader(ShaderID shaderId, uint8_t variant);

	static uint8_t ShaderVariantVS(DataHub::VertexType vertexType);
	static uint8_t ShaderVariantPS(DataHub::Action_OutputLayout layout);

	static DX12_OutputSignature PackOutputSignature(const DX12_OutputSignatureDesc& desc);
	static DX12_OutputSignatureDesc UnpackOutputSignature(const DX12_OutputSignature& sig);

	OutputSignatureID GetOutputSignatureId(DX12_OutputSignature outputSig);
	
	bool IsFormatBlendable(DXGI_FORMAT format);

	#pragma endregion


	#pragma region ReadMessage
private:
	void ReadTransferBox();

	void ReadLayerOption();
	void ReadActionNode();

	void ReadUploadRequest();
	void ReadReleaseRequest();


	DX12_OutputSignature MakeOutputSignature(const DX12_RenderAction& action, const DataHub::Action_Output& output);

	static const DataHub::RTB_Action& GetActionSource(const DataHub::RTB_Layer& layerDesc, ActionID id);
	
	void BuildActionGraph(DX12_RenderLayer& layer, const DataHub::RTB_Layer& layerDesc);
	void BuildActionBarriers(DX12_RenderAction& action);

	void RecordActionBarriers(ID3D12GraphicsCommandList* cmdList, const DX12_RenderLayer& layer, const vector<DX12_ActionBarrier>& barriers, UINT frameIdx);

	#pragma endregion

#pragma endregion


#pragma region Allocator
private:
	class DescriptorHeapAllocator;

#pragma endregion

};

}


#include "D3D12Renderer_Allocator.h"
