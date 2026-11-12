#pragma once

#include "DataHub_Types.h"


namespace Hakko::DataHub
{

    #pragma region ResourceControl

    #define RESERVED_MESH_COUNT 2
    #define RESERVED_TEXTURE_COUNT 5
    #define RESERVED_DRAWPROP_COUNT 7
    #define RESERVED_TEXPROP_COUNT 1

    #define RESERVED_MESH_QUAD_ID 1

    #define RESERVED_TEX_ERROR_ID 0
    #define RESERVED_TEX_NONE_DIFFUSE_ID 1
    #define RESERVED_TEX_NONE_NORMAL_ID 2
    #define RESERVED_TEX_NONE_ORM_ID 3
    #define RESERVED_TEX_NONE_EMISSION_ID 4

    #define RESERVED_TEX_NONE_DIFFUSE_NAME L"None_Diffuse"
    #define RESERVED_TEX_NONE_NORMAL_NAME L"None_Normal"
    #define RESERVED_TEX_NONE_ORM_NAME L"None_ORM"
    #define RESERVED_TEX_NONE_EMISSION_NAME L"None_Emission"

    #define RESERVED_DRAWPROP_SKYBOX_ID 1
    #define RESERVED_DRAWPROP_BG_ID 2
    #define RESERVED_DRAWPROP_OPAQUE_ID 3
    #define RESERVED_DRAWPROP_TRANSLUCENT_ID 4
    #define RESERVED_DRAWPROP_OPAQUE_SKINNED_ID 5
    #define RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_ID 6
    
    #define RESERVED_DRAWPROP_OPAQUE_NAME "Default_Opaque"
    #define RESERVED_DRAWPROP_TRANSLUCENT_NAME "Default_Translucent"
    #define RESERVED_DRAWPROP_OPAQUE_SKINNED_NAME "Default_Opaque_Skinned"
    #define RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_NAME "Default_Translucent_Skinned"


    MeshID CreateMesh(const wchar_t* name, Mesh&& data);
    TexID CreateTexture(const wchar_t* name, Texture&& tex);
    ShaderID RegisterShader(const wchar_t* path, ShaderType shaderType);
    CSResID CreateComputeResource(const wchar_t* name, const ComputeResource& desc);    // 미구현
    DrawPropID AddDrawProp(const char* name, DrawProp prop);
    TexPropID AddTextureProp(const char* name, TextureProp prop);


    MeshID GetMeshId(std::wstring_view name);
    TexID GetTexId(std::wstring_view name);
    ShaderID GetShaderId(std::wstring_view name);
    CSResID GetComputeResourceId(std::wstring_view name);   // 미구현
    DrawPropID GetDrawPropId(std::string_view name);
    TexPropID GetTexPropId(std::string_view name);

    uint16_t GetSubMeshCount(MeshID id);

    // 미구현
    bool UpdateMesh(std::wstring_view name);
    bool UpdateTexture(std::wstring_view name);
    bool UpdateComputeResource(std::wstring_view name);

    bool UpdateDrawProp(std::string_view name);
    bool UpdateTexProp(std::string_view name);


    bool ReleaseMesh(std::wstring_view name);
    bool ReleaseTexture(std::wstring_view name);

    bool ReleaseDrawProp(std::string_view name);
    bool ReleaseTexProp(std::string_view name);



    extern std::vector<Mesh> uploadMeshList;
    extern std::vector<Texture> uploadTextureList;
    extern std::vector<ShaderInfo> loadShaderList;
    extern std::vector<DrawProp> drawPropList;
    extern std::vector<TextureProp> texPropList;

    extern std::vector<uint16_t> meshSubMeshCountList;

    
    extern std::unordered_map<HashedString, MeshID> meshIdLUT;
    extern std::unordered_map<HashedString, TexID> textureIdLUT;
    extern std::unordered_map<HashedString, ShaderID> shaderIdLUT;
    extern std::unordered_map<HashedString, DrawPropID> drawPropIdLUT;
    extern std::unordered_map<HashedString, TexPropID> texPropIdLUT;


    extern std::vector<MeshID> ReleaseMeshList;
    extern std::vector<TexID> ReleaseTextureList;
    // 셰이더는 현재 내리기 지원 안함
    extern std::vector<DrawPropID> ReleaseDrawPropList;
    extern std::vector<TexPropID> ReleaseTexPropList;

    #pragma endregion

}
