#include "DataHub.h"
#include "DataHub_Resource.h"


namespace Hakko::DataHub
{

    #pragma region Resource

    #pragma region Variables

    std::vector<Mesh> uploadMeshList;
    std::vector<Texture> uploadTextureList;    
    std::vector<ShaderInfo> loadShaderList;
    std::vector<DrawProp> drawPropList;
    std::vector<TextureProp> texPropList;

    std::vector<uint16_t> meshSubMeshCountList;


    std::unordered_map<HashedString, MeshID> meshIdLUT;
    std::unordered_map<HashedString, TexID> textureIdLUT;
    std::unordered_map<HashedString, ShaderID> shaderIdLUT;
    std::unordered_map<HashedString, DrawPropID> drawPropIdLUT;
    std::unordered_map<HashedString, TexPropID> texPropIdLUT;


    std::vector<MeshID> ReleaseMeshList;
    std::vector<TexID> ReleaseTextureList;

    std::vector<DrawPropID> ReleaseDrawPropList;
    std::vector<TexPropID> ReleaseTexPropList;

    #pragma endregion


    static bool ValidateSubMeshes(size_t vertexCount, const std::vector<VIndex>& indices, const std::vector<SubMesh>& subMeshes)
    {
        if (subMeshes.empty())
        {
            uint32_t maxIdx = 0;
            for (size_t i = 0; i < indices.size(); i++)
                if (indices[i] > maxIdx) maxIdx = indices[i];
            return maxIdx < vertexCount;
        }

        for (size_t s = 0; s < subMeshes.size(); s++)
        {
            const SubMesh& sub = subMeshes[s];
            const size_t end = (size_t)sub.indexStart + sub.indexCount;

            if (end > indices.size())
                return false;

            uint32_t maxIdx = 0;
            for (size_t i = sub.indexStart; i < end; i++)
                if (indices[i] > maxIdx) maxIdx = indices[i];

            if ((size_t)sub.baseVertex + maxIdx >= vertexCount)
                return false;
        }

        return true;
    }


    MeshID CreateMesh(const wchar_t* name, Mesh&& data)
    {
        size_t vertexCount = 0;
        switch (data.vertexType)
        {
        case VertexType_Simple:  vertexCount = data.verticesSimple.size();  break;
        case VertexType_Static:  vertexCount = data.verticesStatic.size();  break;
        case VertexType_Skinned: vertexCount = data.verticesSkinned.size(); break;
        }

        if (vertexCount == 0 || data.indices.empty() || !ValidateSubMeshes(vertexCount, data.indices, data.subMeshes))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] CreateMesh: 빈 데이터 / 서브메쉬 범위 밖");
            return 0;
        }

        auto key = HashString(std::wstring_view(name));

        if (meshIdLUT.contains(key))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] CreateMesh: 이미 있는 이름");
            return 0;
        }

        if (meshIdLUT.size() >= 0xFFFF)
        {
            Internal::PushHubReport(HubReport_Limit, L"[Hub] CreateMesh: 메쉬 id 소진 (16비트)");
            return 0;
        }

        MeshID id = static_cast<MeshID>(meshIdLUT.size());
        meshIdLUT.emplace(key, id);
        meshSubMeshCountList.push_back(data.subMeshes.empty() ? (uint16_t)1 : (uint16_t)data.subMeshes.size());
        uploadMeshList.push_back(std::move(data));

        return id;
    }

    TexID CreateTexture(const wchar_t* name, Texture&& tex)
    {
        auto key = HashString(std::wstring_view(name));

        if (textureIdLUT.contains(key))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] CreateTexture: 이미 있는 이름");
            return 0;
        }

        if (textureIdLUT.size() >= 0xFFFF)
        {
            Internal::PushHubReport(HubReport_Limit, L"[Hub] CreateTexture: 텍스처 id 소진 (16비트)");
            return 0;
        }

        TexID id = static_cast<TexID>(textureIdLUT.size());
        textureIdLUT.emplace(key, id);
        uploadTextureList.push_back(std::move(tex));

        return id;
    }

    ShaderID RegisterShader(const wchar_t* path, ShaderType shaderType)
    {
        if (shaderType != ShaderType_VS && shaderType != ShaderType_PS)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RegisterShader: 셰이더 타입 잘못됨");
            return 0;
        }

        auto key = HashString(std::wstring_view(path));

        if (shaderIdLUT.contains(key))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RegisterShader: 이미 있는 경로");
            return 0;
        }

        ShaderID id = static_cast<ShaderID>(shaderIdLUT.size());
        shaderIdLUT.emplace(key, id);
        loadShaderList.emplace_back(path, shaderType);

        return id;
    }

    CSResID CreateComputeResource(const wchar_t* name, const ComputeResource& desc)
    {
        // 미구현 (컴퓨트 경로)
        Internal::PushHubReport(HubReport_Contract, L"[Hub] CreateComputeResource: 컴퓨트 자원 미구현");
        return 0;
    }

    DrawPropID AddDrawProp(const char *name, DrawProp prop)
    {
        std::string_view sName(name);
        auto key = HashString(sName);

        
        if (drawPropIdLUT.size() >= 0xFFFF)
        {
            Internal::PushHubReport(HubReport_Limit, L"[Hub] AddDrawProp: id 소진 (16비트)");
            return 0;
        }

        if (prop.VSId >= shaderIdLUT.size() || prop.PSId >= shaderIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] AddDrawProp: 셰이더 id 범위 밖");
            return 0;
        }

        DrawPropID id = static_cast<DrawPropID>(drawPropIdLUT.size());

        auto result = drawPropIdLUT.try_emplace(key, id);

        if (!result.second)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] AddDrawProp: 이미 있는 이름");
            return 0;
        }

            
        drawPropList.emplace_back(prop);

        return id;
    }

    TexPropID AddTextureProp(const char *name, TextureProp prop)
    {
        std::string_view sName(name);
        auto key = HashString(sName);

        
        if (texPropIdLUT.size() >= 0xFFFF)
        {
            Internal::PushHubReport(HubReport_Limit, L"[Hub] AddTextureProp: id 소진 (16비트)");
            return 0;
        }

        for (size_t i = 0; i < TEXPROP_SLOT_COUNT; i++)
        {
            if (prop.texId[i] >= textureIdLUT.size())
            {
                Internal::PushHubReport(HubReport_Contract, L"[Hub] AddTextureProp: 텍스처 id 범위 밖 (0 으로)");
                prop.texId[i] = 0;
            }
        }

        TexPropID id = static_cast<TexPropID>(texPropIdLUT.size());

        auto result = texPropIdLUT.try_emplace(key, id);

        if (!result.second)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] AddTextureProp: 이미 있는 이름");
            return 0;
        }


        texPropList.emplace_back(prop);

        return id;
    }


    MeshID GetMeshId(std::wstring_view name)
    {
        auto key = HashString(name);

        if (!meshIdLUT.contains(key))
            return 0;
            
        return meshIdLUT[key];
    }

    TexID GetTexId(std::wstring_view name)
    {
        auto key = HashString(name);

        if (!textureIdLUT.contains(key))
            return 0;

        return textureIdLUT[key];
    }

    ShaderID GetShaderId(std::wstring_view name)
    {
        auto key = HashString(name);

        if (!shaderIdLUT.contains(key))
            return 0;

        return shaderIdLUT[key];
    }

    CSResID GetComputeResourceId(std::wstring_view name)
    {
        // 미구현
        Internal::PushHubReport(HubReport_Contract, L"[Hub] GetComputeResourceId: 컴퓨트 리소스 미구현");
        return 0;
    }

    DrawPropID GetDrawPropId(std::string_view name)
    {
        auto key = HashString(name);

        if (!drawPropIdLUT.contains(key))
            return 0;

        return drawPropIdLUT[key];
    }

    TexPropID GetTexPropId(std::string_view name)
    {
        auto key = HashString(name);

        if (!texPropIdLUT.contains(key))
            return 0;

        return texPropIdLUT[key];
    }


    uint16_t GetSubMeshCount(MeshID id)
    {
        if (id >= meshSubMeshCountList.size())
            return 0;

        return meshSubMeshCountList[id];
    }


    bool UpdateMesh(std::wstring_view name)
    {
        // 미구현
        Internal::PushHubReport(HubReport_Contract, L"[Hub] UpdateMesh: 업데이트 리소스 미구현");
        return 0;
    }

    bool UpdateTexture(std::wstring_view name)
    {
        // 미구현
        Internal::PushHubReport(HubReport_Contract, L"[Hub] UpdateTexture: 업데이트 리소스 미구현");
        return 0;
    }

    bool UpdateComputeResource(std::wstring_view name)
    {
        // 미구현
        Internal::PushHubReport(HubReport_Contract, L"[Hub] UpdateComputeResource: 업데이트 리소스 미구현");
        return 0;
    }

    bool UpdateDrawProp(std::string_view name)
    {
        // 미구현
        Internal::PushHubReport(HubReport_Contract, L"[Hub] UpdateDrawProp: 업데이트 리소스 미구현");
        return 0;
    }

    bool UpdateTexProp(std::string_view name)
    {
        // 미구현
        Internal::PushHubReport(HubReport_Contract, L"[Hub] UpdateTexProp: 업데이트 리소스 미구현");
        return 0;
    }


    bool ReleaseMesh(std::wstring_view filename)
    {
        auto key = HashString(filename);

        if (!meshIdLUT.contains(key))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ReleaseMesh: 없는 이름");
            return false;
        }

        auto id = meshIdLUT[key];
        if (id < RESERVED_MESH_COUNT)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ReleaseMesh: 예약 메쉬 해제 시도");
            return false;
        }

        ReleaseMeshList.push_back(id);

        return true;
    }

    bool ReleaseTexture(std::wstring_view filename)
    {
        auto key = HashString(filename);

        if (!textureIdLUT.contains(key))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ReleaseTexture: 없는 이름");
            return false;
        }

        auto id = textureIdLUT[key];
        if (id < RESERVED_TEXTURE_COUNT)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ReleaseTexture: 예약 텍스처 해제 시도");
            return false;
        }

        ReleaseTextureList.push_back(id);

        return true;
    }


    bool ReleaseDrawProp(std::string_view name)
    {
        auto key = HashString(name);

        if (!drawPropIdLUT.contains(key))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ReleaseDrawProp: 없는 이름");
            return false;
        }

        auto id = drawPropIdLUT[key];
        if (id < RESERVED_DRAWPROP_COUNT)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ReleaseDrawProp: 빌트인 drawProp 해제 시도");
            return false;
        }

        ReleaseDrawPropList.push_back(id);

        return true;
    }
    
    bool ReleaseTexProp(std::string_view name)
    {
        auto key = HashString(name);

        if (!texPropIdLUT.contains(key))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ReleaseTexProp: 없는 이름");
            return false;
        }

        auto id = texPropIdLUT[key];
        if (id < RESERVED_TEXPROP_COUNT)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ReleaseTexProp: 기본 texProp 해제 시도");
            return false;
        }

        ReleaseTexPropList.push_back(id);

        return true;
    }

    #pragma endregion

}
