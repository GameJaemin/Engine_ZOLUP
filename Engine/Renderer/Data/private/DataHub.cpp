#include "DataHub.h"

#include <algorithm>

using namespace DirectX;
using namespace Hakko::DataHub;
using namespace std;


namespace Hakko::DataHub
{

    #pragma region Lifecycle

    bool Init()
    {

        meshIdLUT.emplace(HashString(std::wstring_view(L"__MESHDEFAULT")), 0);
        meshIdLUT.emplace(HashString(std::wstring_view(L"__MESHQUAD")), RESERVED_MESH_QUAD_ID);
        meshSubMeshCountList.assign(meshIdLUT.size(), 1);

        textureIdLUT.emplace(HashString(std::wstring_view(L"__TEXTUREDEFAULT")), RESERVED_TEX_ERROR_ID);
        textureIdLUT.emplace(HashString(std::wstring_view(L"__TEXTURETRANSPARENT")), RESERVED_TEX_TRANSPARENT_ID);
        textureIdLUT.emplace(HashString(std::wstring_view(RESERVED_TEX_NONE_DIFFUSE_NAME)), RESERVED_TEX_NONE_DIFFUSE_ID);
        textureIdLUT.emplace(HashString(std::wstring_view(RESERVED_TEX_NONE_NORMAL_NAME)), RESERVED_TEX_NONE_NORMAL_ID);
        textureIdLUT.emplace(HashString(std::wstring_view(RESERVED_TEX_NONE_ORM_NAME)), RESERVED_TEX_NONE_ORM_ID);
        textureIdLUT.emplace(HashString(std::wstring_view(RESERVED_TEX_NONE_EMISSION_NAME)), RESERVED_TEX_NONE_EMISSION_ID);
        textureIdLUT.emplace(HashString(std::wstring_view(RESERVED_TEX_NONE_SHADOW_NAME)), RESERVED_TEX_NONE_SHADOW_ID);

        csBufferIdLUT.emplace(HashString(std::wstring_view(L"__CSBUFFERDEFAULT")), 0);

        shaderIdLUT.emplace(HashString(std::wstring_view(L"__SHADERDEFAULT")), 0U);

        for (ShaderID i = 1; i < RESERVED_SHADER_WORLD_VS_ID; i++)
        {
            const std::wstring name = L"__SHADER" + std::to_wstring(i);
            shaderIdLUT.emplace(HashString(std::wstring_view(name)), i);
        }

        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_WORLD_VS_NAME)), RESERVED_SHADER_WORLD_VS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_WORLD_OPQ_PS_NAME)), RESERVED_SHADER_WORLD_OPQ_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_WORLD_TP_PS_NAME)), RESERVED_SHADER_WORLD_TP_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_PARTICLE_VS_NAME)), RESERVED_SHADER_PARTICLE_VS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_PARTICLE_PS_NAME)), RESERVED_SHADER_PARTICLE_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_SHADOW_VS_NAME)), RESERVED_SHADER_SHADOW_VS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_PP_VS_NAME)), RESERVED_SHADER_PP_VS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_SSAO_PS_NAME)), RESERVED_SHADER_SSAO_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_AOBLUR_PS_NAME)), RESERVED_SHADER_AOBLUR_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_DEFERRED_LIGHTING_PS_NAME)), RESERVED_SHADER_DEFERRED_LIGHTING_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_BLOOM_PREFILTER_PS_NAME)), RESERVED_SHADER_BLOOM_PREFILTER_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_BLOOM_DOWN_PS_NAME)), RESERVED_SHADER_BLOOM_DOWN_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_BLOOM_UP_PS_NAME)), RESERVED_SHADER_BLOOM_UP_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_BLOOM_COMBINE_PS_NAME)), RESERVED_SHADER_BLOOM_COMBINE_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_TONEMAP_PS_NAME)), RESERVED_SHADER_TONEMAP_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_FXAA_PS_NAME)), RESERVED_SHADER_FXAA_PS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_SKINNING_CS_NAME)), RESERVED_SHADER_SKINNING_CS_ID);
        shaderIdLUT.emplace(HashString(std::wstring_view(RESERVED_SHADER_PARTICLE_CS_NAME)), RESERVED_SHADER_PARTICLE_CS_ID);

        drawPropIdLUT.emplace(HashString(std::string_view("__DRAWPROPDEFAULT")), 0U);
        drawPropIdLUT.emplace(HashString(std::string_view("__DRAWPROPSKYBOX")), RESERVED_DRAWPROP_SKYBOX_ID);
        drawPropIdLUT.emplace(HashString(std::string_view("__DRAWPROPBG")), RESERVED_DRAWPROP_BG_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_OPAQUE_NAME)), RESERVED_DRAWPROP_OPAQUE_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_OPAQUE_SKINNED_NAME)), RESERVED_DRAWPROP_OPAQUE_SKINNED_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_OPAQUE_CLIP_NAME)), RESERVED_DRAWPROP_OPAQUE_CLIP_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_OPAQUE_SKINNED_CLIP_NAME)), RESERVED_DRAWPROP_OPAQUE_SKINNED_CLIP_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_TRANSLUCENT_NAME)), RESERVED_DRAWPROP_TRANSLUCENT_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_NAME)), RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_PARTICLE_NAME)), RESERVED_DRAWPROP_PARTICLE_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_SHADOW_NAME)), RESERVED_DRAWPROP_SHADOW_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_SHADOW_SKINNED_NAME)), RESERVED_DRAWPROP_SHADOW_SKINNED_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_SHADOW_CLIP_NAME)), RESERVED_DRAWPROP_SHADOW_CLIP_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_SHADOW_SKINNED_CLIP_NAME)), RESERVED_DRAWPROP_SHADOW_SKINNED_CLIP_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_SSAO_NAME)), RESERVED_DRAWPROP_SSAO_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_AOBLUR_NAME)), RESERVED_DRAWPROP_AOBLUR_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_DEFERRED_LIGHTING_NAME)), RESERVED_DRAWPROP_DEFERRED_LIGHTING_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_BLOOM_PREFILTER_NAME)), RESERVED_DRAWPROP_BLOOM_PREFILTER_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_BLOOM_DOWN_NAME)), RESERVED_DRAWPROP_BLOOM_DOWN_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_BLOOM_UP_NAME)), RESERVED_DRAWPROP_BLOOM_UP_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_BLOOM_COMBINE_NAME)), RESERVED_DRAWPROP_BLOOM_COMBINE_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_TONEMAP_NAME)), RESERVED_DRAWPROP_TONEMAP_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_FXAA_NAME)), RESERVED_DRAWPROP_FXAA_ID);

        texPropIdLUT.emplace(HashString(std::string_view("__TEXPROPDEFAULT")), 0U);


        RenderLightList.emplace_back();
        RenderShadowPresetList.emplace_back();

        
        DataBoxCustomizer::RegisterCustomDataBox<DataBox_Render3DGeneral>();
        DataBoxCustomizer::RegisterCustomDataBox<DataBox_Render3DOptional>();
        DataBoxCustomizer::RegisterCustomDataBox<DataBox_Render2DScreen>();
        DataBoxCustomizer::RegisterCustomDataBox<DataBox_RenderShadowView>();
        DataBoxCustomizer::RegisterCustomDataBox<DataBox_RenderEditCustom>();
        

        LayerList.emplace_back();
        LayerList[0].backGround = Layer_BackGround{0, {1,1,1,1}};
            

        return true;
    }

    void Finalize()
    {
        flag_updateResources = 0;
        
        blankedShadowPresetIdList = {};
        blankedLightIdList = {};

        RenderShadowPresetList = {};
        RenderLightList = {};

        DataBoxCustomizer::ClearAll();

        ReleaseMeshList = {};
        ReleaseTextureList = {};
        ReleaseCSBufferList = {};
        ReleaseDrawPropList = {};
        ReleaseTexPropList = {};

        RenderReportList = {};
        HubReportList = {};

        meshSubMeshCountList = {};

        linkCSBufferAsMeshList = {};

        shaderIdLUT = {};
        meshIdLUT = {};
        textureIdLUT = {};
        csBufferIdLUT = {};
        drawPropIdLUT = {};
        texPropIdLUT = {};

        loadShaderList = {};
        uploadMeshList = {};
        uploadTextureList = {};
        uploadCSBufferList = {};
        updateMeshList = {};
        updateTextureList = {};
        updateCSBufferList = {};
        updateDrawPropList = {};
        updateTexPropList = {};
        uploadDrawPropList = {};
        uploadTexPropList = {};

        LayerList = {};
    }

    #pragma endregion



    #pragma region DataBox

    template <>
    DataBoxID AddDataBox<DataBox_RenderLight>(const DataBox_RenderLight& dataBox)
    {
        DataBoxID idx;

        if (blankedLightIdList.size() > 0)
        {
            idx = blankedLightIdList.back();
            RenderLightList[idx] = dataBox;
            blankedLightIdList.pop_back();
        }
        else
        {
            idx = RenderLightList.size();
            RenderLightList.emplace_back(dataBox);
        }

        return idx;
    }

    template <>
    DataBoxID AddDataBox<DataBox_RenderShadowPreset>(const DataBox_RenderShadowPreset& dataBox)
    {
        DataBoxID idx;

        if (blankedShadowPresetIdList.size() > 0)
        {
            idx = blankedShadowPresetIdList.back();
            RenderShadowPresetList[idx] = dataBox;
            blankedShadowPresetIdList.pop_back();
        }
        else
        {
            idx = RenderShadowPresetList.size();
            RenderShadowPresetList.emplace_back(dataBox);
        }

        return idx;
    }


    static void RemoveLightDataBox(std::vector<DataBoxID>& blankedList, size_t listSize, DataBoxID index)
    {
        if (index == 0 || index >= listSize)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RemoveDataBox: 조명 / 그림자 프리셋 id 범위 밖 또는 예약 0번 (무시됨)");
            return;
        }

        for (DataBoxID blanked : blankedList)
        {
            if (blanked == index)
            {
                Internal::PushHubReport(HubReport_Contract, L"[Hub] RemoveDataBox: 이미 지운 조명 / 그림자 프리셋 id (무시됨)");
                return;
            }
        }

        blankedList.push_back(index);
    }

    void RemoveDataBox(DataBoxTID tid, DataBoxID index)
    {
        if (tid == DataBoxType_DataBox_RenderLight)
        {
            RemoveLightDataBox(blankedLightIdList, RenderLightList.size(), index);
            return;
        }
        else if (tid == DataBoxType_DataBox_RenderShadowPreset)
        {
            RemoveLightDataBox(blankedShadowPresetIdList, RenderShadowPresetList.size(), index);
            return;
        }

        DataBoxCustomizer::RemoveDataBox(tid, index);
    }


    template <>
    bool UpdateDataBox<DataBox_RenderLight>(DataBoxID id, const DataBox_RenderLight& dataBox)
    {
        if (id == 0 || id >= RenderLightList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] UpdateDataBox: 조명 id 범위 밖 또는 예약 0번 (무시됨)");
            return false;
        }

        RenderLightList[id] = dataBox;

        return true;
    }

    template <>
    bool UpdateDataBox<DataBox_RenderShadowPreset>(DataBoxID id, const DataBox_RenderShadowPreset& dataBox)
    {
        if (id == 0 || id >= RenderShadowPresetList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] UpdateDataBox: 그림자 프리셋 id 범위 밖 또는 예약 0번 (무시됨)");
            return false;
        }

        RenderShadowPresetList[id] = dataBox;

        return true;
    }

    #pragma endregion



    #pragma region Variables

    std::vector<RenderReport> RenderReportList;
    std::vector<HubReport> HubReportList;

    void Internal::PushHubReport(HubReportKind kind, const wchar_t* msg)
    {
        HubReportList.push_back({ kind, msg });
        if (HubReportList.size() > HUB_REPORT_MAX)
            HubReportList.erase(HubReportList.begin());
    }


    std::vector<DataBox_RenderLight> RenderLightList;
    std::vector<DataBoxID> blankedLightIdList;

    std::vector<DataBox_RenderShadowPreset> RenderShadowPresetList;
    std::vector<DataBoxID> blankedShadowPresetIdList;


    uint32_t flag_updateResources;


    std::vector<RTB_Layer> LayerList;

    #pragma endregion

}
