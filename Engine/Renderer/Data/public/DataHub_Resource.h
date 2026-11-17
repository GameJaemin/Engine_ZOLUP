#pragma once

#include "DataHub_Types.h"


namespace Hakko::DataHub
{

    #pragma region ResourceControl

    #pragma region ReservedResource

    #define RESERVED_MESH_COUNT 2
    #define RESERVED_TEXTURE_COUNT 7
    #define RESERVED_CSBUFFER_COUNT 1
    #define RESERVED_SHADER_COUNT 23
    #define RESERVED_DRAWPROP_COUNT 23
    #define RESERVED_TEXPROP_COUNT 1


    #define RESERVED_MESH_QUAD_ID 1

    #define RESERVED_TEX_ERROR_ID 0
    #define RESERVED_TEX_TRANSPARENT_ID 1
    #define RESERVED_TEX_NONE_DIFFUSE_ID 2
    #define RESERVED_TEX_NONE_NORMAL_ID 3
    #define RESERVED_TEX_NONE_ORM_ID 4
    #define RESERVED_TEX_NONE_EMISSION_ID 5
    #define RESERVED_TEX_NONE_SHADOW_ID 6

    #define RESERVED_TEX_NONE_DIFFUSE_NAME L"None_Diffuse"
    #define RESERVED_TEX_NONE_NORMAL_NAME L"None_Normal"
    #define RESERVED_TEX_NONE_ORM_NAME L"None_ORM"
    #define RESERVED_TEX_NONE_EMISSION_NAME L"None_Emission"
    #define RESERVED_TEX_NONE_SHADOW_NAME L"None_Shadow"


    #define RESERVED_SHADER_WORLD_VS_ID 5
    #define RESERVED_SHADER_WORLD_OPQ_PS_ID 6
    #define RESERVED_SHADER_WORLD_TP_PS_ID 7
    #define RESERVED_SHADER_PARTICLE_VS_ID 8
    #define RESERVED_SHADER_PARTICLE_PS_ID 9
    #define RESERVED_SHADER_SHADOW_VS_ID 10
    #define RESERVED_SHADER_PP_VS_ID 11
    #define RESERVED_SHADER_SSAO_PS_ID 12
    #define RESERVED_SHADER_AOBLUR_PS_ID 13
    #define RESERVED_SHADER_DEFERRED_LIGHTING_PS_ID 14
    #define RESERVED_SHADER_BLOOM_PREFILTER_PS_ID 15
    #define RESERVED_SHADER_BLOOM_DOWN_PS_ID 16
    #define RESERVED_SHADER_BLOOM_UP_PS_ID 17
    #define RESERVED_SHADER_BLOOM_COMBINE_PS_ID 18
    #define RESERVED_SHADER_TONEMAP_PS_ID 19
    #define RESERVED_SHADER_FXAA_PS_ID 20
    #define RESERVED_SHADER_SKINNING_CS_ID 21
    #define RESERVED_SHADER_PARTICLE_CS_ID 22

    #define RESERVED_SHADER_WORLD_VS_NAME L"Default_World_VS"
    #define RESERVED_SHADER_WORLD_OPQ_PS_NAME L"Default_World_OPQ_PS"
    #define RESERVED_SHADER_WORLD_TP_PS_NAME L"Default_World_TP_PS"
    #define RESERVED_SHADER_PARTICLE_VS_NAME L"Default_Particle_VS"
    #define RESERVED_SHADER_PARTICLE_PS_NAME L"Default_Particle_PS"
    #define RESERVED_SHADER_SHADOW_VS_NAME L"Default_Shadow_VS"
    #define RESERVED_SHADER_PP_VS_NAME L"Default_PP_VS"
    #define RESERVED_SHADER_SSAO_PS_NAME L"Default_SSAO_PS"
    #define RESERVED_SHADER_AOBLUR_PS_NAME L"Default_AOBlur_PS"
    #define RESERVED_SHADER_DEFERRED_LIGHTING_PS_NAME L"Default_DeferredLighting_PS"
    #define RESERVED_SHADER_BLOOM_PREFILTER_PS_NAME L"Default_BloomPrefilter_PS"
    #define RESERVED_SHADER_BLOOM_DOWN_PS_NAME L"Default_BloomDown_PS"
    #define RESERVED_SHADER_BLOOM_UP_PS_NAME L"Default_BloomUp_PS"
    #define RESERVED_SHADER_BLOOM_COMBINE_PS_NAME L"Default_BloomCombine_PS"
    #define RESERVED_SHADER_TONEMAP_PS_NAME L"Default_Tonemap_PS"
    #define RESERVED_SHADER_FXAA_PS_NAME L"Default_FXAA_PS"
    #define RESERVED_SHADER_SKINNING_CS_NAME L"Default_Skinning_CS"
    #define RESERVED_SHADER_PARTICLE_CS_NAME L"Default_Particle_CS"


    #define RESERVED_DRAWPROP_SKYBOX_ID 1
    #define RESERVED_DRAWPROP_BG_ID 2
    #define RESERVED_DRAWPROP_OPAQUE_ID 3
    #define RESERVED_DRAWPROP_OPAQUE_SKINNED_ID 4
    #define RESERVED_DRAWPROP_OPAQUE_CLIP_ID 5
    #define RESERVED_DRAWPROP_OPAQUE_SKINNED_CLIP_ID 6
    #define RESERVED_DRAWPROP_TRANSLUCENT_ID 7
    #define RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_ID 8
    #define RESERVED_DRAWPROP_PARTICLE_ID 9
    #define RESERVED_DRAWPROP_SHADOW_ID 10
    #define RESERVED_DRAWPROP_SHADOW_SKINNED_ID 11
    #define RESERVED_DRAWPROP_SHADOW_CLIP_ID 12
    #define RESERVED_DRAWPROP_SHADOW_SKINNED_CLIP_ID 13
    #define RESERVED_DRAWPROP_SSAO_ID 14
    #define RESERVED_DRAWPROP_AOBLUR_ID 15
    #define RESERVED_DRAWPROP_DEFERRED_LIGHTING_ID 16
    #define RESERVED_DRAWPROP_BLOOM_PREFILTER_ID 17
    #define RESERVED_DRAWPROP_BLOOM_DOWN_ID 18
    #define RESERVED_DRAWPROP_BLOOM_UP_ID 19
    #define RESERVED_DRAWPROP_BLOOM_COMBINE_ID 20
    #define RESERVED_DRAWPROP_TONEMAP_ID 21
    #define RESERVED_DRAWPROP_FXAA_ID 22

    #define RESERVED_DRAWPROP_OPAQUE_NAME "Default_Opaque"
    #define RESERVED_DRAWPROP_OPAQUE_SKINNED_NAME "Default_Opaque_Skinned"
    #define RESERVED_DRAWPROP_OPAQUE_CLIP_NAME "Default_Opaque_Clip"
    #define RESERVED_DRAWPROP_OPAQUE_SKINNED_CLIP_NAME "Default_Opaque_Skinned_Clip"
    #define RESERVED_DRAWPROP_TRANSLUCENT_NAME "Default_Translucent"
    #define RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_NAME "Default_Translucent_Skinned"
    #define RESERVED_DRAWPROP_PARTICLE_NAME "Default_Particle"
    #define RESERVED_DRAWPROP_SHADOW_NAME "Default_Shadow"
    #define RESERVED_DRAWPROP_SHADOW_SKINNED_NAME "Default_Shadow_Skinned"
    #define RESERVED_DRAWPROP_SHADOW_CLIP_NAME "Default_Shadow_Clip"
    #define RESERVED_DRAWPROP_SHADOW_SKINNED_CLIP_NAME "Default_Shadow_Skinned_Clip"
    #define RESERVED_DRAWPROP_SSAO_NAME "Default_SSAO"
    #define RESERVED_DRAWPROP_AOBLUR_NAME "Default_AOBlur"
    #define RESERVED_DRAWPROP_DEFERRED_LIGHTING_NAME "Default_DeferredLighting"
    #define RESERVED_DRAWPROP_BLOOM_PREFILTER_NAME "Default_BloomPrefilter"
    #define RESERVED_DRAWPROP_BLOOM_DOWN_NAME "Default_BloomDown"
    #define RESERVED_DRAWPROP_BLOOM_UP_NAME "Default_BloomUp"
    #define RESERVED_DRAWPROP_BLOOM_COMBINE_NAME "Default_BloomCombine"
    #define RESERVED_DRAWPROP_TONEMAP_NAME "Default_Tonemap"
    #define RESERVED_DRAWPROP_FXAA_NAME "Default_FXAA"

    #pragma endregion

    
    ShaderID RegisterShader(const wchar_t* path, ShaderType shaderType);

    // Create / Update / Link 는 만든 프레임에 해당 RTB (RTBCMD_Upload / RTBCMD_Update) 를 보내야 함
    // 미루면 그 사이에 실행된 해제 때문에 대기 중인 id 가 어긋남... (적어도 해제를 동일 프레임에 하면 문제 x)
    MeshID CreateMesh(const wchar_t* name, Mesh&& mesh);
    TexID CreateTexture(const wchar_t* name, Texture&& tex);
    CSBufferID CreateComputeBuffer(const wchar_t* name, const ComputeBuffer& desc);
    DrawPropID CreateDrawProp(const char* name, DrawProp prop);
    TexPropID CreateTexProp(const char* name, TextureProp prop);

    MeshID LinkComputeBufferAsMesh(const wchar_t* csBufferName, const wchar_t* meshName, 
                                    std::vector<VIndex>&& indices, std::vector<SubMesh>&& subMeshes);


    // 셰이더는 현재 내리기 지원 안함
    // 해제가 없어서 그냥 아이디 저장해도 무방하긴 함
    ShaderID GetShaderId(std::wstring_view name);

    MeshID GetMeshId(std::wstring_view name);
    TexID GetTexId(std::wstring_view name);
    CSBufferID GetComputeBufferId(std::wstring_view name);
    DrawPropID GetDrawPropId(std::string_view name);
    TexPropID GetTexPropId(std::string_view name);

    uint16_t GetSubMeshCount(MeshID id);

    
    bool UpdateMesh(std::wstring_view name, Mesh&& mesh);
    bool UpdateTexture(std::wstring_view name, Texture&& tex);
    bool UpdateComputeBuffer(std::wstring_view name, const ComputeBuffer& desc);
    bool UpdateDrawProp(std::string_view name, DrawProp prop);
    bool UpdateTexProp(std::string_view name, TextureProp prop);


    bool ReleaseMesh(std::wstring_view name);
    bool ReleaseTexture(std::wstring_view name);
    bool ReleaseComputeBuffer(std::wstring_view name);
    bool ReleaseDrawProp(std::string_view name);
    bool ReleaseTexProp(std::string_view name);


    extern std::vector<ShaderInfo> loadShaderList;

    extern std::vector<Mesh> uploadMeshList;
    extern std::vector<Texture> uploadTextureList;
    extern std::vector<ComputeBuffer> uploadCSBufferList;
    extern std::vector<DrawProp> uploadDrawPropList;
    extern std::vector<TextureProp> uploadTexPropList;

    extern std::vector<uint16_t> meshSubMeshCountList;
    
    extern std::vector<CSMeshLinkData> linkCSBufferAsMeshList;


    extern std::unordered_map<HashedString, ShaderID> shaderIdLUT;
    
    extern std::unordered_map<HashedString, MeshID> meshIdLUT;
    extern std::unordered_map<HashedString, TexID> textureIdLUT;
    extern std::unordered_map<HashedString, CSBufferID> csBufferIdLUT;
    extern std::unordered_map<HashedString, DrawPropID> drawPropIdLUT;
    extern std::unordered_map<HashedString, TexPropID> texPropIdLUT;


    extern std::vector<std::pair<MeshID, Mesh>> updateMeshList;
    extern std::vector<std::pair<TexID, Texture>> updateTextureList;
    extern std::vector<std::pair<CSBufferID, ComputeBuffer>> updateCSBufferList;    
    extern std::vector<std::pair<DrawPropID, DrawProp>> updateDrawPropList;
    extern std::vector<std::pair<TexPropID, TextureProp>> updateTexPropList;


    extern std::vector<MeshID> ReleaseMeshList;
    extern std::vector<TexID> ReleaseTextureList;
    extern std::vector<CSBufferID> ReleaseCSBufferList;
    extern std::vector<DrawPropID> ReleaseDrawPropList;
    extern std::vector<TexPropID> ReleaseTexPropList;

    #pragma endregion

}
