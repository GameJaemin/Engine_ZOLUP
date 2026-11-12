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

        meshIdLUT.emplace(HashString(std::wstring_view(L"__MESHDEFAULT")), 0U);
        meshIdLUT.emplace(HashString(std::wstring_view(L"__MESHQUAD")), RESERVED_MESH_QUAD_ID);
        meshSubMeshCountList.assign(meshIdLUT.size(), 1);

        textureIdLUT.emplace(HashString(std::wstring_view(L"__TEXTUREDEFAULT")), RESERVED_TEX_ERROR_ID);
        textureIdLUT.emplace(HashString(std::wstring_view(RESERVED_TEX_NONE_DIFFUSE_NAME)), RESERVED_TEX_NONE_DIFFUSE_ID);
        textureIdLUT.emplace(HashString(std::wstring_view(RESERVED_TEX_NONE_NORMAL_NAME)), RESERVED_TEX_NONE_NORMAL_ID);
        textureIdLUT.emplace(HashString(std::wstring_view(RESERVED_TEX_NONE_ORM_NAME)), RESERVED_TEX_NONE_ORM_ID);
        textureIdLUT.emplace(HashString(std::wstring_view(RESERVED_TEX_NONE_EMISSION_NAME)), RESERVED_TEX_NONE_EMISSION_ID);

        drawPropIdLUT.emplace(HashString(std::string_view("__DRAWPROPDEFAULT")), 0U);
        drawPropIdLUT.emplace(HashString(std::string_view("__DRAWPROPSKYBOX")), RESERVED_DRAWPROP_SKYBOX_ID);
        drawPropIdLUT.emplace(HashString(std::string_view("__DRAWPROPBG")), RESERVED_DRAWPROP_BG_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_OPAQUE_NAME)), RESERVED_DRAWPROP_OPAQUE_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_TRANSLUCENT_NAME)), RESERVED_DRAWPROP_TRANSLUCENT_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_OPAQUE_SKINNED_NAME)), RESERVED_DRAWPROP_OPAQUE_SKINNED_ID);
        drawPropIdLUT.emplace(HashString(std::string_view(RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_NAME)), RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_ID);
        
        drawPropList.assign(drawPropIdLUT.size(), {});

        texPropIdLUT.emplace(HashString(std::string_view("__TEXPROPDEFAULT")), 0U);
        texPropList.assign(texPropIdLUT.size(), {});


        RenderLightList.emplace_back();
        RenderLightMatrixList.emplace_back();

        
        DataBoxCustomizer::RegisterCustomDataBox<DataBox_Render3DGeneral>();
        DataBoxCustomizer::RegisterCustomDataBox<DataBox_Render3DOptional>();
        DataBoxCustomizer::RegisterCustomDataBox<DataBox_Render2DScreen>();

        DataBoxCustomizer::AddDataBox<DataBox_Render3DGeneral>(DataBox_Render3DGeneral());
        DataBoxCustomizer::AddDataBox<DataBox_Render3DOptional>(DataBox_Render3DOptional());
        DataBoxCustomizer::AddDataBox<DataBox_Render2DScreen>(DataBox_Render2DScreen());
        

        LayerList.emplace_back();
        LayerList[0].backGround = Layer_BackGround{0, {1,1,1,1}};
            

        return true;
    }

    void Finalize()
    {
        flag_updateResources_Upload = false;
        flag_updateResources_Release = false;

        flag_updateProps_FindOrBuild = false;
        flag_updateProps_Release = false;
        
        blankedLightMatrixIdList = {};
        blankedLightIdList = {};

        RenderLightMatrixList = {};
        RenderLightList = {};

        DataBoxCustomizer::ClearAll();

        ReleaseMeshList = {};
        ReleaseTextureList = {};
        ReleaseDrawPropList = {};
        ReleaseTexPropList = {};
        RenderReportList = {};
        HubReportList = {};

        shaderIdLUT = {};
        meshIdLUT = {};
        meshSubMeshCountList = {};
        textureIdLUT = {};
        drawPropIdLUT = {};
        texPropIdLUT = {};

        loadShaderList = {};
        uploadMeshList = {};
        uploadTextureList = {};
        drawPropList = {};
        texPropList = {};

        LayerList = {};
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

    std::vector<DataBox_RenderLightMatrix> RenderLightMatrixList;
    std::vector<DataBoxID> blankedLightMatrixIdList;


    bool flag_updateResources_Upload;
    bool flag_updateResources_Release;

    bool flag_updateProps_FindOrBuild;
    bool flag_updateProps_Release;


    std::vector<RTB_Layer> LayerList;

    #pragma endregion

}
