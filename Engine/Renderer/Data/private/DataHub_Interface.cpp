#include "DataHub_Interface.h"

#include <utility>
#include <string>
#include <cstring>


namespace Hakko::DataHub
{

    #pragma region Interface

    template <>
    void DataHub::SendRTB<DataHub::RTB_UpdateResources>(const RTB_UpdateResources& RTB, uint8_t layer)
    {
        flag_updateResources |= RTB.command;
    }

    #pragma region SetLayerOption

    void SetLayerBackground(TexID texId, uint8_t layer)
    {
        if (layer >= LayerList.size() || texId >= textureIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetLayerBackground: 레이어 / 텍스처 id 범위 밖 (무시됨)");
            return;
        }

        LayerList[layer].backGround.bgTexId = texId;
    }

    void SetLayerBackgroundColor(float r, float g, float b, float a, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetLayerBackgroundColor: 레이어 범위 밖 (무시됨)");
            return;
        }

        float* color = LayerList[layer].backGround.color;
        color[0] = r;   color[1] = g;   color[2] = b;   color[3] = a;
    }

    void SetLayerEnvMap(TexID texId, uint8_t layer)
    {
        if (layer >= LayerList.size() || texId >= textureIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetLayerEnvMap: 레이어 / 텍스처 id 범위 밖 (무시됨)");
            return;
        }

        LayerList[layer].envTexId = texId;
    }

    void SetLayerViewport(const Layer_Viewport& viewport, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetLayerViewport: 레이어 범위 밖 (무시됨)");
            return;
        }

        LayerList[layer].viewport = viewport;
    }

    void SetLayerPipelineType(Layer_PipelineType type, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetLayerPipelineType: 레이어 범위 밖 (무시됨)");
            return;
        }

        LayerList[layer].pipelineType = type;

        if (type == Layer_PipelineType_Deferred && LayerList[layer].actionLighting.drawProp == 0)
            LayerList[layer].actionLighting.drawProp = RESERVED_DRAWPROP_DEFERRED_LIGHTING_ID;
    }

    void SetLayerEarlyZ(bool enable, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetLayerEarlyZ: 레이어 범위 밖 (무시됨)");
            return;
        }

        LayerList[layer].actionEarlyZ.output.useDS = enable;
    }

    #pragma endregion


    #pragma region SetLayerData

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetCondition>(const RTB_SetCondition& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        LayerList[layer].condition = RTB.condition;
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetCamera>(const RTB_SetCamera& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        RenderCondition& condition = LayerList[layer].condition;
        condition.camera_View = RTB.view;
        condition.camera_Proj = RTB.proj;
        condition.camera_ViewProj = RTB.viewProj;
        condition.camera_EyePos = RTB.eyePos;
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetGlobalLight>(const RTB_SetGlobalLight& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        RenderCondition& condition = LayerList[layer].condition;
        condition.global_LightAmbient = RTB.ambient;
        condition.global_LightColor = RTB.color;
        condition.global_LightDirection = RTB.direction;
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetTime>(const RTB_SetTime& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        LayerList[layer].condition.global_TotalTime = RTB.totalTime;
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetLight>(const RTB_SetLight& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.slot >= RENDER_LIGHT_MAX || RTB.light >= RenderLightList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetLight: 슬롯 / 조명 id 범위 밖");
            return;
        }

        LayerList[layer].lights[RTB.slot] = RTB.light;
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetShadowPreset>(const RTB_SetShadowPreset& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.slot >= RENDER_SHADOW_PRESET_MAX || RTB.shadowPreset >= RenderShadowPresetList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetShadowPreset: 슬롯 / 프리셋 id 범위 밖");
            return;
        }

        LayerList[layer].shadowPresets[RTB.slot] = RTB.shadowPreset;
    }

    #pragma endregion


    #pragma region SetActionData

    static void SetEditActionData(RTB_ActionEdit& action, DrawPropID drawProp, const DtB_EditCustom& data)
    {
        action.drawProp = drawProp;

        if (action.actionDataBoxId == 0)
            action.actionDataBoxId = AddDataBox(data);
        else
            UpdateDataBox(action.actionDataBoxId, data);
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetTonemap>(const RTB_SetTonemap& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.ppSlot >= RENDER_BUILTIN_PP_MAX)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetTonemap: PP 칸 범위 밖");
            return;
        }

        RTB_ActionEdit& action = LayerList[layer].actionPPs[RTB.ppSlot];
        if (!RTB.enable)
        {
            action.drawProp = 0;
            return;
        }

        DtB_EditCustom data = DtB_EditCustom();
        data.values[0] = RTB.exposureEV;
        data.values[1] = (float)RTB.mode;
        data.values[2] = RTB.gainR;
        data.values[3] = RTB.gainG;
        data.values[4] = RTB.gainB;
        data.values[5] = RTB.contrast;
        data.values[6] = RTB.saturation;
        data.values[7] = RTB.gamma;

        SetEditActionData(action, RESERVED_DRAWPROP_TONEMAP_ID, data);
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetFXAA>(const RTB_SetFXAA& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.ppSlot >= RENDER_BUILTIN_PP_MAX)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetFXAA: PP 칸 범위 밖");
            return;
        }

        RTB_ActionEdit& action = LayerList[layer].actionPPs[RTB.ppSlot];
        if (!RTB.enable)
        {
            action.drawProp = 0;
            return;
        }

        DtB_EditCustom data = DtB_EditCustom();
        data.values[0] = RTB.edgeThresholdScale;
        data.values[1] = RTB.spanMaxScale;

        SetEditActionData(action, RESERVED_DRAWPROP_FXAA_ID, data);
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetSSAO>(const RTB_SetSSAO& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        RTB_ActionEdit& action = LayerList[layer].actionAO;
        if (!RTB.enable)
        {
            action.drawProp = 0;
            return;
        }

        DtB_EditCustom data = DtB_EditCustom();
        data.values[0] = RTB.radiusScale;
        data.values[1] = RTB.intensity;
        data.values[2] = RTB.biasScale;

        SetEditActionData(action, RESERVED_DRAWPROP_SSAO_ID, data);
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetAOBlur>(const RTB_SetAOBlur& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        RTB_ActionEdit& action = LayerList[layer].actionAOBlur;
        if (!RTB.enable)
        {
            action.drawProp = 0;
            return;
        }

        DtB_EditCustom data = DtB_EditCustom();
        data.values[0] = RTB.depthThresholdScale;

        SetEditActionData(action, RESERVED_DRAWPROP_AOBLUR_ID, data);
    }

    BloomChain CreateBloomChain(const char* name, uint32_t width, uint32_t height, uint32_t levels)
    {
        BloomChain chain;

        if (name == nullptr || width == 0 || height == 0 || levels == 0 || levels > RENDER_BLOOM_LEVEL_MAX)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] CreateBloomChain: 이름 / 크기 / 단계 수가 잘못됨");
            return chain;
        }

        for (uint32_t i = 0; i < levels; i++)
        {
            const std::string propName = std::string(name) + std::to_string(i);
            const std::wstring texName(propName.begin(), propName.end());

            uint32_t w = width >> (i + 1);
            uint32_t h = height >> (i + 1);
            if (w == 0)
                w = 1;
            if (h == 0)
                h = 1;

            Texture tex = {};
            tex.data.format = TextureReadFormat::RGBA16_Float;
            tex.data.textureType = TextureType_2D;
            tex.data.width = w;
            tex.data.height = h;
            tex.usage = TextureUsage_RT;

            const TexID texId = CreateTexture(texName.c_str(), std::move(tex));

            TexProp prop = {};
            prop.texId[0] = texId;

            const TexPropID texPropId = CreateTexProp(propName.c_str(), prop);

            if (texId == 0 || texPropId == 0)
            {
                Internal::PushHubReport(HubReport_Contract, L"[Hub] CreateBloomChain: 텍스처 / texProp 생성 실패");
                return BloomChain();
            }
        }

        chain.name = name;
        chain.levels = levels;
        chain.width = width;
        chain.height = height;

        return chain;
    }

    static void SetBloomNode(RTB_ActionEdit& action, DrawPropID drawProp, TexPropID readProp, const BloomChain& chain, uint32_t target, TexID targetTex, bool additive)
    {
        action.drawProp = drawProp;
        action.texProp = readProp;
        action.output.blendOpt[0] = additive ? Action_Output_BlendOption_ForceAddtive : Action_Output_BlendOption_ByBlendType;

        if (target >= chain.levels)
        {
            action.output.RTTex[0] = 0;
            action.viewport = Action_Viewport{};
            return;
        }

        uint32_t w = chain.width >> (target + 1);
        uint32_t h = chain.height >> (target + 1);
        if (w == 0)
            w = 1;
        if (h == 0)
            h = 1;

        action.output.RTTex[0] = targetTex;
        action.viewport = Action_Viewport{0.0f, 0.0f, (float)w, (float)h};
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetBloom>(const RTB_SetBloom& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        const uint32_t levels = RTB.chain.levels;
        const uint32_t first = RTB.ppSlot;

        if (levels == 0 || levels > RENDER_BLOOM_LEVEL_MAX || first + levels * 2 > RENDER_BUILTIN_PP_MAX)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetBloom: 블룸 세트가 비었거나 PP 칸을 넘음 (무시됨)");
            return;
        }

        auto& pps = LayerList[layer].actionPPs;
        const uint32_t last = first + levels * 2 - 1;

        if (!RTB.enable)
        {
            for (uint32_t i = first; i <= last; i++)
                SetBloomNode(pps[i], 0, 0, RTB.chain, levels, 0, false);
            return;
        }

        TexID texIds[RENDER_BLOOM_LEVEL_MAX] = {0};
        TexPropID texPropIds[RENDER_BLOOM_LEVEL_MAX] = {0};

        for (uint32_t i = 0; i < levels; i++)
        {
            const std::string propName = std::string(RTB.chain.name) + std::to_string(i);
            const std::wstring texName(propName.begin(), propName.end());

            texIds[i] = GetTexId(texName);
            texPropIds[i] = GetTexPropId(propName);

            if (texIds[i] == 0 || texPropIds[i] == 0)
            {
                Internal::PushHubReport(HubReport_Contract, L"[Hub] SetBloom: 블룸 세트의 텍스처 / texProp 이 없음 (무시됨)");
                return;
            }
        }

        DtB_EditCustom prefilterData = DtB_EditCustom();
        prefilterData.values[0] = RTB.thresholdScale;
        prefilterData.values[1] = RTB.kneeScale;

        DtB_EditCustom upData = DtB_EditCustom();
        upData.values[0] = RTB.radiusScale;

        DtB_EditCustom combineData = DtB_EditCustom();
        combineData.values[0] = RTB.intensity;
        combineData.values[1] = RTB.radiusScale;

        SetBloomNode(pps[first], RESERVED_DRAWPROP_BLOOM_PREFILTER_ID, 0, RTB.chain, 0, texIds[0], false);
        SetEditActionData(pps[first], RESERVED_DRAWPROP_BLOOM_PREFILTER_ID, prefilterData);

        for (uint32_t i = 1; i < levels; i++)
            SetBloomNode(pps[first + i], RESERVED_DRAWPROP_BLOOM_DOWN_ID, texPropIds[i - 1], RTB.chain, i, texIds[i], false);

        for (uint32_t i = levels - 1; i >= 1; i--)
        {
            RTB_ActionEdit& up = pps[first + levels + (levels - 1 - i)];
            SetBloomNode(up, RESERVED_DRAWPROP_BLOOM_UP_ID, texPropIds[i], RTB.chain, i - 1, texIds[i - 1], true);
            SetEditActionData(up, RESERVED_DRAWPROP_BLOOM_UP_ID, upData);
        }

        SetBloomNode(pps[last], RESERVED_DRAWPROP_BLOOM_COMBINE_ID, texPropIds[0], RTB.chain, levels, 0, false);
        SetEditActionData(pps[last], RESERVED_DRAWPROP_BLOOM_COMBINE_ID, combineData);
    }

    #pragma endregion


    #pragma region SetShadowData

    ShadowAtlas CreateShadowAtlas(const wchar_t* name, uint32_t size, uint32_t grid)
    {
        Texture tex = {};
        tex.data.format = TextureReadFormat::D32_Float;
        tex.data.textureType = TextureType_2D;
        tex.data.width = size;
        tex.data.height = size;
        tex.usage = TextureUsage_DS;

        ShadowAtlas atlas;
        if (CreateTexture(name, std::move(tex)) == 0)
            return atlas;

        atlas.name = name;
        atlas.size = size;
        atlas.grid = grid;

        return atlas;
    }

    static bool CheckShadowTile(const ShadowAtlas& atlas, TexID texId, uint32_t tile)
    {
        if (texId == 0 || atlas.size == 0 || atlas.grid == 0)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetShadow: 아틀라스 텍스처 / 크기 / 격자가 비어 있음 (무시됨)");
            return false;
        }
        if (tile >= atlas.grid * atlas.grid)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetShadow: 타일 번호가 아틀라스 격자 칸 수를 넘음 (무시됨)");
            return false;
        }

        return true;
    }

    static void SetShadowActionTile(RTB_ActionDraw& action, const ShadowAtlas& atlas, TexID texId, uint32_t tile, const DirectX::XMMATRIX& lightViewProj)
    {
        const float tileSize = (float)(atlas.size / atlas.grid);

        action.output.useDS = true;
        action.output.DSTex = texId;
        action.output.dsLoadOpt = Action_Output_LoadOption_Clear;
        action.viewport = Action_Viewport{(float)(tile % atlas.grid) * tileSize, (float)(tile / atlas.grid) * tileSize, tileSize, tileSize};

        DtB_ShadowView view = DtB_ShadowView();
        view.viewProj = lightViewProj;

        if (action.actionDataBoxId == 0)
            action.actionDataBoxId = AddDataBox(view);
        else
            UpdateDataBox(action.actionDataBoxId, view);
    }

    static DirectX::XMFLOAT4 ShadowTileUV(const ShadowAtlas& atlas, uint32_t tile)
    {
        const float scale = 1.0f / (float)atlas.grid;

        return DirectX::XMFLOAT4{scale, scale, (float)(tile % atlas.grid) * scale, (float)(tile / atlas.grid) * scale};
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetShadowCascade>(const RTB_SetShadowCascade& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.cascade >= RENDER_BUILTIN_CSM_MAX)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetShadowCascade: 캐스케이드 번호 범위 밖 (무시됨)");
            return;
        }

        RTB_ActionDraw& action = LayerList[layer].actionGlobalSDWs[RTB.cascade];
        RenderCondition& condition = LayerList[layer].condition;
        float* splits = &condition.global_LightCascadeSplit.x;

        if (!RTB.enable)
        {
            action.output.useDS = false;
            splits[RTB.cascade] = 0.0f;
            return;
        }

        const TexID texId = GetTexId(RTB.atlas.name);

        if (!CheckShadowTile(RTB.atlas, texId, RTB.cascade))
            return;

        SetShadowActionTile(action, RTB.atlas, texId, RTB.cascade, RTB.lightViewProj);

        condition.global_LightViewProj[RTB.cascade] = RTB.lightViewProj;
        condition.global_LightCascadeTile[RTB.cascade] = ShadowTileUV(RTB.atlas, RTB.cascade);
        splits[RTB.cascade] = RTB.splitEnd;

        LayerList[layer].globalShadowTexId = texId;
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetShadowLocal>(const RTB_SetShadowLocal& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.tile >= RENDER_BUILTIN_LOCAL_SDW_MAX || RTB.presetSlot >= RENDER_SHADOW_PRESET_MAX || RTB.entry >= RENDER_SHADOW_PRESET_LOCAL_MAX)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetShadowLocal: 타일 / 프리셋 칸 / 프리셋 안의 칸 범위 밖 (무시됨)");
            return;
        }

        RTB_ActionDraw& action = LayerList[layer].actionLocalSDWs[RTB.tile];
        DataBoxID& presetId = LayerList[layer].shadowPresets[RTB.presetSlot];

        if (!RTB.enable)
        {
            action.output.useDS = false;
            if (presetId != 0 && presetId < RenderShadowPresetList.size())
                RenderShadowPresetList[presetId].locals[RTB.entry] = ShadowPreset_Entry();
            return;
        }

        if (RTB.lightSlot == 0 || RTB.lightSlot >= RENDER_LIGHT_MAX)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetShadowLocal: 조명칸 범위 밖 (0 은 빈 자리, 무시됨)");
            return;
        }

        const TexID texId = GetTexId(RTB.atlas.name);

        if (!CheckShadowTile(RTB.atlas, texId, RTB.tile))
            return;

        if (presetId == 0)
            presetId = AddDataBox(DtB_ShadowPreset());
        if (presetId == 0 || presetId >= RenderShadowPresetList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetShadowLocal: 프리셋 칸의 프리셋 id 가 잘못됨 (무시됨)");
            return;
        }

        SetShadowActionTile(action, RTB.atlas, texId, RTB.tile, RTB.lightViewProj);

        ShadowPreset_Entry& presetEntry = RenderShadowPresetList[presetId].locals[RTB.entry];
        presetEntry.lightViewProj = RTB.lightViewProj;
        presetEntry.shadowTile = ShadowTileUV(RTB.atlas, RTB.tile);
        presetEntry.lightSlot = RTB.lightSlot;

        LayerList[layer].localShadowTexId = texId;
    }

    #pragma endregion


    #pragma region SetParticleData

    ParticleBuffer CreateParticleBuffer(const wchar_t* name, uint32_t count)
    {
        ParticleBuffer buffer;

        const uint32_t rounded = (count + RENDER_PARTICLE_GROUP - 1) / RENDER_PARTICLE_GROUP * RENDER_PARTICLE_GROUP;
        if (name == nullptr || count == 0 || rounded > 0xFFFF)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] CreateParticleBuffer: 이름 / 파티클 수가 잘못됨 (최대 65472)");
            return buffer;
        }

        ComputeBuffer desc;
        desc.stride = RENDER_PARTICLE_STRIDE;
        desc.count = rounded;
        desc.data.resize((size_t)desc.stride * desc.count);

        if (CreateComputeBuffer(name, desc) == 0)
            return buffer;

        buffer.name = name;
        buffer.count = rounded;

        return buffer;
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetParticle>(const RTB_SetParticle& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.computeSlot >= RENDER_BUILTIN_PRECOMPUTE_MAX)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetParticle: 컴퓨트 선계산 칸 범위 밖 (무시됨)");
            return;
        }

        RTB_ActionCompute& action = LayerList[layer].actionPreComputes[RTB.computeSlot];

        if (!RTB.enable)
        {
            action.csId = 0;
            return;
        }

        const CSBufferID bufferId = GetComputeBufferId(RTB.buffer.name);

        if (bufferId == 0 || RTB.buffer.count == 0 || RTB.buffer.count % RENDER_PARTICLE_GROUP != 0)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetParticle: 파티클 버퍼가 비었거나 칸 수가 그룹 배수가 아님 (무시됨)");
            return;
        }

        DtB_EditCustom data = DtB_EditCustom();
        data.values[0] = RTB.dt;
        data.values[1] = RTB.time;
        data.values[2] = RTB.emitRatio;
        data.values[3] = (float)RTB.shape;
        data.values[4] = RTB.emitPos.x;
        data.values[5] = RTB.emitPos.y;
        data.values[6] = RTB.emitPos.z;
        data.values[7] = RTB.coneAngle;
        data.values[8] = RTB.emitDir.x;
        data.values[9] = RTB.emitDir.y;
        data.values[10] = RTB.emitDir.z;
        data.values[11] = RTB.drag;
        data.values[12] = RTB.shapeSize.x;
        data.values[13] = RTB.shapeSize.y;
        data.values[14] = RTB.shapeSize.z;
        data.values[16] = RTB.accel.x;
        data.values[17] = RTB.accel.y;
        data.values[18] = RTB.accel.z;
        data.values[20] = RTB.speedMin;
        data.values[21] = RTB.speedMax;
        data.values[22] = RTB.lifeMin;
        data.values[23] = RTB.lifeMax;
        data.values[24] = RTB.sizeMin;
        data.values[25] = RTB.sizeMax;
        data.values[26] = RTB.sizeEnd;
        data.values[28] = RTB.colorStart.x;
        data.values[29] = RTB.colorStart.y;
        data.values[30] = RTB.colorStart.z;
        data.values[32] = RTB.colorEnd.x;
        data.values[33] = RTB.colorEnd.y;
        data.values[34] = RTB.colorEnd.z;

        action.csId = RESERVED_SHADER_PARTICLE_CS_ID;
        action.dispatchX = RTB.buffer.count / RENDER_PARTICLE_GROUP;
        action.dispatchY = 1;
        action.dispatchZ = 1;
        action.writes.fill({});
        action.reads.fill({});
        action.writes[0] = ActionCompute_Binding{ActionCompute_Binding_Type_Buffer, bufferId};

        if (action.actionDataBoxTypeId != DataBoxType_DtB_EditCustom)
        {
            action.actionDataBoxTypeId = DataBoxType_DtB_EditCustom;
            action.actionDataBoxId = 0;
        }

        if (action.actionDataBoxId == 0)
            action.actionDataBoxId = AddDataBox(data);
        else
            UpdateDataBox(action.actionDataBoxId, data);
    }

    RTB_DrawTP MakeParticleRequest(const ParticleBuffer& buffer, DrawPropID drawProp, TexPropID texProp)
    {
        RTB_DrawTP rtb = {};
        rtb.drawProp = drawProp != 0 ? drawProp : RESERVED_DRAWPROP_PARTICLE_ID;
        rtb.texProp = texProp;
        rtb.instanceCount = (uint16_t)buffer.count;
        rtb.meshId = RESERVED_MESH_QUAD_ID;
        rtb.bufferId = GetComputeBufferId(buffer.name);

        return rtb;
    }

    #pragma endregion


    #pragma region SetSkinningData

    static void BuildSkinSource(const std::vector<VertexSkinned>& vertices, ComputeBuffer& out)
    {
        const uint32_t vertexCount = (uint32_t)vertices.size();

        out = ComputeBuffer();
        out.stride = sizeof(VertexSkinned);
        out.count = (vertexCount + RENDER_SKINNING_GROUP - 1) / RENDER_SKINNING_GROUP * RENDER_SKINNING_GROUP;
        out.data.resize((size_t)out.stride * out.count);
        memcpy(out.data.data(), vertices.data(), (size_t)vertexCount * sizeof(VertexSkinned));
    }

    SkinnedMesh CreateSkinnedMesh(const wchar_t* name, Mesh&& mesh, const wchar_t* sourceName)
    {
        SkinnedMesh skinned;

        if (name == nullptr || mesh.vertexType != VertexType_Skinned || mesh.verticesSkinned.empty() || mesh.indices.empty())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] CreateSkinnedMesh: 이름이 없거나 스킨드 메쉬가 아님 / 정점, 인덱스가 비어있음");
            return skinned;
        }

        std::wstring srcName = std::wstring(name) + RENDER_SKINNING_SOURCE_SUFFIX;
        if (sourceName != nullptr)
            srcName = sourceName;

        if (srcName == name
            || meshIdLUT.contains(HashString(std::wstring_view(name)))
            || csBufferIdLUT.contains(HashString(std::wstring_view(name)))
            || csBufferIdLUT.contains(HashString(std::wstring_view(srcName))))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] CreateSkinnedMesh: 이미 있는 이름 (메쉬 / 결과 버퍼 / 원본 버퍼)");
            return skinned;
        }

        ComputeBuffer src;
        BuildSkinSource(mesh.verticesSkinned, src);

        ComputeBuffer dst;
        dst.stride = sizeof(VertexStatic);
        dst.count = src.count;

        if (CreateComputeBuffer(srcName.c_str(), src) == 0)
            return skinned;
        if (CreateComputeBuffer(name, dst) == 0)
            return skinned;
        if (LinkComputeBufferAsMesh(name, name, std::move(mesh.indices), std::move(mesh.subMeshes)) == 0)
            return skinned;

        skinned.name = name;
        if (sourceName != nullptr)
            skinned.sourceName = sourceName;
        skinned.count = dst.count;

        return skinned;
    }

    std::wstring GetSkinnedMeshSourceName(const SkinnedMesh& skinned)
    {
        if (!skinned.sourceName.empty())
            return std::wstring(skinned.sourceName);

        return std::wstring(skinned.name) + RENDER_SKINNING_SOURCE_SUFFIX;
    }

    bool UpdateSkinnedMeshVertices(const SkinnedMesh& skinned, const std::vector<VertexSkinned>& vertices)
    {
        if (vertices.empty())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] UpdateSkinnedMeshVertices: 정점이 비어있음 (무시됨)");
            return false;
        }

        ComputeBuffer src;
        BuildSkinSource(vertices, src);

        if (src.count != skinned.count)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] UpdateSkinnedMeshVertices: 정점 수가 만들 때와 다름 (무시됨)");
            return false;
        }

        return UpdateComputeBuffer(GetSkinnedMeshSourceName(skinned), src);
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_SetSkinning>(const RTB_SetSkinning& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.computeSlot >= RENDER_BUILTIN_PRECOMPUTE_MAX)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetSkinning: 컴퓨트 선계산 칸 범위 밖 (무시됨)");
            return;
        }

        RTB_ActionCompute& action = LayerList[layer].actionPreComputes[RTB.computeSlot];

        if (!RTB.enable)
        {
            action.csId = 0;
            return;
        }

        const CSBufferID srcId = GetComputeBufferId(GetSkinnedMeshSourceName(RTB.mesh));
        const CSBufferID dstId = GetComputeBufferId(RTB.mesh.name);

        if (srcId == 0 || dstId == 0 || RTB.mesh.count == 0 || RTB.mesh.count % RENDER_SKINNING_GROUP != 0)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetSkinning: 스킨드 메쉬의 버퍼가 없거나 정점 수가 그룹 배수가 아님 (무시됨)");
            return;
        }

        action.csId = RESERVED_SHADER_SKINNING_CS_ID;
        action.dispatchX = RTB.mesh.count / RENDER_SKINNING_GROUP;
        action.dispatchY = 1;
        action.dispatchZ = 1;
        action.writes.fill({});
        action.reads.fill({});
        action.reads[0] = ActionCompute_Binding{ ActionCompute_Binding_Type_Buffer, srcId };
        action.writes[0] = ActionCompute_Binding{ ActionCompute_Binding_Type_Buffer, dstId };
        action.actionDataBoxTypeId = DataBoxType_DataBox_Render3DOptional;
        action.actionDataBoxId = RTB.boneDataId;
    }

    #pragma endregion


    #pragma region RawDrawRequest

    RTB_DrawRequest MakeDrawRequest(const RTB_DrawOPQ& RTB)
    {
        RenderIdOpaque idp;
        idp.alphaClip = RTB.alphaClip;  idp.layer = RTB.layer;  idp.drawProp = RTB.drawProp;
        idp.texProp = RTB.texProp;      idp.meshId = RTB.meshId;  idp.depth = RTB.depth;

        RTB_DrawRequest req = {};
        req.renderId = idp;
        req.meshId = RTB.meshId;
        req.subMeshId = RTB.subMeshId;
        req.dataBoxIds[0] = RTB.generalId;
        req.dataBoxIds[1] = RTB.optionalId;

        return req;
    }

    RTB_DrawRequest MakeDrawRequest(const RTB_DrawTP& RTB)
    {
        RenderIdTranslucent idp;
        idp.depth = RTB.depth;  idp.drawProp = RTB.drawProp;  idp.texProp = RTB.texProp;  idp.instanceCount = RTB.instanceCount;

        RTB_DrawRequest req = {};
        req.renderId = idp;
        req.meshId = RTB.meshId;
        req.subMeshId = RTB.subMeshId;
        req.dataBoxIds[0] = RTB.generalId;
        req.dataBoxIds[1] = RTB.optionalId;
        req.dataBoxIds[2] = RTB.bufferId;

        return req;
    }

    RTB_DrawRequest MakeDrawRequest(const RTB_DrawSDW& RTB)
    {
        RenderIdShadow idp;
        idp.alphaClip = RTB.alphaClip;  idp.drawProp = RTB.drawProp;  idp.texProp = RTB.texProp;
        idp.meshId = RTB.meshId;        idp.depth = RTB.depth;  idp.iid = RTB.iid;

        RTB_DrawRequest req = {};
        req.renderId = idp;
        req.meshId = RTB.meshId;
        req.subMeshId = RTB.subMeshId;
        req.dataBoxIds[0] = RTB.generalId;
        req.dataBoxIds[1] = RTB.optionalId;

        return req;
    }

    RTB_DrawRequest MakeDrawRequest(const RTB_DrawSC& RTB)
    {
        RenderIdScreen idp;
        idp.layer = RTB.layer;  idp.translucent = RTB.translucent;  idp.texture = RTB.texture;
        idp.drawProp = RTB.drawProp;  idp.iid = RTB.iid;

        RTB_DrawRequest req = {};
        req.renderId = idp;
        req.meshId = 1;
        req.subMeshId = 0;
        req.dataBoxIds[0] = RTB.screenId;

        return req;
    }

    static DrawPropID ShadowDrawPropOf(DrawPropID drawProp)
    {
        switch (drawProp)
        {
        case RESERVED_DRAWPROP_OPAQUE_ID:               return RESERVED_DRAWPROP_SHADOW_ID;
        case RESERVED_DRAWPROP_OPAQUE_SKINNED_ID:       return RESERVED_DRAWPROP_SHADOW_SKINNED_ID;
        case RESERVED_DRAWPROP_OPAQUE_CLIP_ID:          return RESERVED_DRAWPROP_SHADOW_CLIP_ID;
        case RESERVED_DRAWPROP_OPAQUE_SKINNED_CLIP_ID:  return RESERVED_DRAWPROP_SHADOW_SKINNED_CLIP_ID;
        case RESERVED_DRAWPROP_TRANSLUCENT_ID:          return RESERVED_DRAWPROP_SHADOW_ID;
        case RESERVED_DRAWPROP_TRANSLUCENT_SKINNED_ID:  return RESERVED_DRAWPROP_SHADOW_SKINNED_ID;
        default:                                        return 0;
        }
    }

    RTB_DrawSDW MakeShadowRequest(const RTB_DrawOPQ& RTB, DrawPropID shadowDrawProp)
    {
        if (shadowDrawProp == 0)
            shadowDrawProp = ShadowDrawPropOf(RTB.drawProp);
        if (shadowDrawProp == 0)
            Internal::PushHubReport(HubReport_Contract, L"[Hub] MakeShadowRequest: 짝 섀도우가 없는 drawProp (섀도우 drawProp 을 직접 넘길 것, 요청은 안 그려짐)");

        RTB_DrawSDW sdw = {};
        sdw.alphaClip = RTB.alphaClip;
        sdw.drawProp = shadowDrawProp;
        sdw.texProp = RTB.texProp;
        sdw.meshId = RTB.meshId;
        sdw.subMeshId = RTB.subMeshId;
        sdw.generalId = RTB.generalId;
        sdw.optionalId = RTB.optionalId;

        return sdw;
    }

    RTB_DrawSDW MakeShadowRequest(const RTB_DrawTP& RTB, DrawPropID shadowDrawProp)
    {
        if (shadowDrawProp == 0)
            shadowDrawProp = ShadowDrawPropOf(RTB.drawProp);
        if (shadowDrawProp == 0)
            Internal::PushHubReport(HubReport_Contract, L"[Hub] MakeShadowRequest: 짝 섀도우가 없는 drawProp (섀도우 drawProp 을 직접 넘길 것, 요청은 안 그려짐)");

        RTB_DrawSDW sdw = {};
        sdw.alphaClip = (shadowDrawProp == RESERVED_DRAWPROP_SHADOW_CLIP_ID || shadowDrawProp == RESERVED_DRAWPROP_SHADOW_SKINNED_CLIP_ID);
        sdw.drawProp = shadowDrawProp;
        sdw.texProp = RTB.texProp;
        sdw.meshId = RTB.meshId;
        sdw.subMeshId = RTB.subMeshId;
        sdw.generalId = RTB.generalId;
        sdw.optionalId = RTB.optionalId;

        return sdw;
    }

    void SendRTB(const RTB_DrawRequest& RTB, RTB_ActionDraw& action)
    {
        if (RTB.meshId >= meshIdLUT.size() || RTB.subMeshId >= meshSubMeshCountList[RTB.meshId])
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: 메쉬 / 서브메쉬 id 범위 밖 (요청 버림)");
            return;
        }

        action.drawReqList.push_back(RTB);
    }

    void SendRTB(const RTB_DrawSDW& RTB, RTB_ActionDraw& action)
    {
        if (action.renderIdType != RenderIdType_Shadow)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest(SDW): 섀도우 액션이 아님 (요청 버림)");
            return;
        }

        if (RTB.meshId >= meshIdLUT.size() || RTB.subMeshId >= meshSubMeshCountList[RTB.meshId])
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: 메쉬 / 서브메쉬 id 범위 밖 (요청 버림)");
            return;
        }
        if (RTB.drawProp >= drawPropIdLUT.size() || RTB.texProp >= texPropIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: drawProp / texProp id 범위 밖 (요청 버림)");
            return;
        }

        action.drawReqList.push_back(MakeDrawRequest(RTB));
    }

    void SetActionDataType(RTB_ActionDraw& action, uint8_t slot, DataBoxTID tid)
    {
        if (slot >= DRAWREQ_DATABOX_MAX)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetActionDataType: 데이터 칸 범위 밖 (무시됨)");
            return;
        }

        action.dataBoxTypeIds[slot] = tid;
    }

    void SetActionOutputLayout(RTB_ActionDraw& action, Action_OutputLayout layout)
    {
        if (layout >= OutputLayout_Count)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SetActionOutputLayout: 없는 레이아웃 (무시됨)");
            return;
        }

        action.output.outputLayout = layout;

        if (layout == OutputLayout_None)
            return;

        const uint8_t mask = OutputLayoutTargetMask(layout);
        for (uint8_t r = 0; r < RENDER_TARGET_MAX; r++)
            action.output.useRT[r] = ((mask >> r) & 1) != 0;

        if (layout == OutputLayout_DepthOnly)
            action.output.useDS = true;
    }

    #pragma endregion


    #pragma region SendDrawRequest
    
    template <>
    void DataHub::SendRTB<DataHub::RTB_DrawOPQ>(const RTB_DrawOPQ& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.meshId >= meshIdLUT.size() || RTB.subMeshId >= meshSubMeshCountList[RTB.meshId])
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: 메쉬 / 서브메쉬 id 범위 밖 (요청 버림)");
            return;
        }
        if (RTB.drawProp >= drawPropIdLUT.size() || RTB.texProp >= texPropIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: drawProp / texProp id 범위 밖 (요청 버림)");
            return;
        }

        RTB_DrawRequest req = MakeDrawRequest(RTB);

        if (LayerList[layer].pipelineType == Layer_PipelineType_Deferred)
        {
            LayerList[layer].actionGBuffer.drawReqList.push_back(req);
            return;
        }

        if (LayerList[layer].actionEarlyZ.output.useDS)
            LayerList[layer].actionEarlyZ.drawReqList.push_back(req);

        LayerList[layer].actionOPQ.drawReqList.push_back(req);
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_DrawTP>(const RTB_DrawTP& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.meshId >= meshIdLUT.size() || RTB.subMeshId >= meshSubMeshCountList[RTB.meshId])
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: 메쉬 / 서브메쉬 id 범위 밖 (요청 버림)");
            return;
        }
        if (RTB.drawProp >= drawPropIdLUT.size() || RTB.texProp >= texPropIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest: drawProp / texProp id 범위 밖 (요청 버림)");
            return;
        }

        RTB_DrawRequest req = MakeDrawRequest(RTB);
        LayerList[layer].actionTP.drawReqList.push_back(req);
    }

    template <>
    void DataHub::SendRTB<DataHub::RTB_DrawSC>(const RTB_DrawSC& RTB, uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] SendRTB: 레이어 범위 밖");
            return;
        }

        if (RTB.drawProp >= drawPropIdLUT.size() || RTB.texture >= textureIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] DrawRequest(SC): drawProp / 텍스처 id 범위 밖 (요청 버림)");
            return;
        }

        RTB_DrawRequest req = MakeDrawRequest(RTB);
        LayerList[layer].actionSC.drawReqList.push_back(req);
    }

    #pragma endregion


    #pragma region MakeDrawRequest

    #pragma region Helper

    static DrawProp MakeDrawProp(const DrawOption& option, DrawProp_DepthUsage depthUsage, ShaderID defaultPSId)
    {
        DrawProp dp = {};
        dp.depthUsage = depthUsage;
        dp.cullType = option.cullType;
        dp.blendType = option.blendType;
        dp.fillType = DrawProp_FillType_Solid;
        dp.depthBias = DrawProp_DepthBias_None;
        dp.stecilOpt.useStencil = false;
        dp.topology = DrawProp_Topology_TriangleList;
        dp.vertexType = option.vertexType;
        dp.VSId = option.VSId != 0 ? option.VSId : RESERVED_SHADER_WORLD_VS_ID;
        dp.PSId = option.PSId != 0 ? option.PSId : defaultPSId;

        return dp;
    }

    static TextureProp MakeTexProp(const DrawOption& option)
    {
        TextureProp tp = {};
        tp.texId[0] = option.diffuseTexId;
        tp.texId[1] = option.normalTexId;
        tp.texId[2] = option.ORMTexId;
        tp.texId[3] = option.emissionTexId;
        for (size_t i = 4; i < TEXPROP_SLOT_COUNT; i++)
            tp.texId[i] = option.flexibleTexId[i - 4];

        return tp;
    }

    static void FillShadowRequest(RTB_DrawSDW& sdw, const DrawOption& option, bool alphaClip, TexPropID texProp, MeshID meshId, DataBoxID generalId, DataBoxID optionalId)
    {
        DrawPropID shadowDrawProp = 0;

        if (option.VSId == 0 && option.vertexType == VertexType_Static)
            shadowDrawProp = alphaClip ? RESERVED_DRAWPROP_SHADOW_CLIP_ID : RESERVED_DRAWPROP_SHADOW_ID;
        if (option.VSId == 0 && option.vertexType == VertexType_Skinned)
            shadowDrawProp = alphaClip ? RESERVED_DRAWPROP_SHADOW_SKINNED_CLIP_ID : RESERVED_DRAWPROP_SHADOW_SKINNED_ID;

        if (shadowDrawProp == 0)
            Internal::PushHubReport(HubReport_Contract, L"[Hub] MakeDrawRequestAndProps: 이 옵션은 기본 제공 섀도우가 없음 (섀도우 요청의 drawProp 을 직접 채울 것)");

        sdw.alphaClip = alphaClip;
        sdw.drawProp = shadowDrawProp;
        sdw.texProp = texProp;
        sdw.meshId = meshId;
        sdw.depth = 0U;
        sdw.iid = 0U;
        sdw.subMeshId = 0U;
        sdw.generalId = generalId;
        sdw.optionalId = optionalId;
    }

    static bool RegisterProps(
        const char* drawPropName, const DrawProp& dp, DrawPropID& dpId,
        const char* texPropName, const TextureProp* tp, TexPropID& tpId)
    {
        if (dp.VSId >= shaderIdLUT.size() || dp.PSId >= shaderIdLUT.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] MakeDrawRequestAndProps: 셰이더 id 범위 밖");
            return false;
        }

        const auto dpKey = HashString(std::string_view(drawPropName));
        const auto tpKey = tp != nullptr ? HashString(std::string_view(texPropName)) : 0U;

        if (drawPropIdLUT.contains(dpKey) || (tp != nullptr && texPropIdLUT.contains(tpKey)))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] MakeDrawRequestAndProps: 이미 있는 이름");
            return false;
        }

        if (drawPropIdLUT.size() >= 0xFFFF || (tp != nullptr && texPropIdLUT.size() >= 0xFFFF))
        {
            Internal::PushHubReport(HubReport_Limit, L"[Hub] MakeDrawRequestAndProps: id 소진 (16비트)");
            return false;
        }

        dpId = static_cast<DrawPropID>(drawPropIdLUT.size());
        drawPropIdLUT.emplace(dpKey, dpId);
        uploadDrawPropList.emplace_back(dp);

        if (tp != nullptr)
        {
            tpId = static_cast<TexPropID>(texPropIdLUT.size());
            texPropIdLUT.emplace(tpKey, tpId);
            uploadTexPropList.emplace_back(*tp);
        }

        return true;
    }

    #pragma endregion


    bool MakeDrawRequestAndProps(
        DrawOption option,
        const char* drawPropName, const char* texPropName,
        RTB_DrawOPQ& rtb,
        MeshID meshId, DataBoxID generalId, DataBoxID optionalId, RTB_DrawSDW* shadowRtb)
    {
        if (option.blendType != DrawProp_BlendType_None && option.blendType != DrawProp_BlendType_AlphaClip)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] MakeDrawRequestAndProps(OPQ): 블렌드 옵션은 Translucent 요청으로");
            return false;
        }

        const bool alphaClip = option.alphaClipOrTranslucent || option.blendType == DrawProp_BlendType_AlphaClip;

        DrawProp dp = MakeDrawProp(option, DrawProp_DepthUsage_Opaque, RESERVED_SHADER_WORLD_OPQ_PS_ID);
        if (alphaClip)
            dp.blendType = DrawProp_BlendType_AlphaClip;

        const TextureProp tp = MakeTexProp(option);

        DrawPropID dpId = 0;
        TexPropID tpId = 0;
        if (!RegisterProps(drawPropName, dp, dpId, texPropName, &tp, tpId))
            return false;


        rtb.alphaClip = alphaClip;
        rtb.layer = 0U;
        rtb.drawProp = dpId;
        rtb.texProp = tpId;
        rtb.meshId = meshId;
        rtb.depth = 0U;
        rtb.subMeshId = 0U;
        rtb.generalId = generalId;
        rtb.optionalId = optionalId;

        if (shadowRtb != nullptr)
            FillShadowRequest(*shadowRtb, option, alphaClip, tpId, meshId, generalId, optionalId);


        return true;
    }

    bool MakeDrawRequestAndProps(
        DrawOption option,
        const char *drawPropName, const char *texPropName,
        RTB_DrawTP &rtb,
        MeshID meshId, DataBoxID generalId, DataBoxID optionalId, RTB_DrawSDW* shadowRtb)
    {
        if (option.blendType == DrawProp_BlendType_AlphaClip)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] MakeDrawRequestAndProps(TP): 알파 클립은 Opaque 요청으로");
            return false;
        }

        DrawProp dp = MakeDrawProp(option, DrawProp_DepthUsage_Translucent, RESERVED_SHADER_WORLD_TP_PS_ID);
        if (dp.blendType == DrawProp_BlendType_None)
            dp.blendType = DrawProp_BlendType_Alpha;

        const TextureProp tp = MakeTexProp(option);

        DrawPropID dpId = 0;
        TexPropID tpId = 0;
        if (!RegisterProps(drawPropName, dp, dpId, texPropName, &tp, tpId))
            return false;


        rtb.depth = 0U;
        rtb.drawProp = dpId;
        rtb.texProp = tpId;
        rtb.instanceCount = 0U;
        rtb.meshId = meshId;
        rtb.subMeshId = 0U;
        rtb.generalId = generalId;
        rtb.optionalId = optionalId;
        rtb.bufferId = 0U;

        if (shadowRtb != nullptr)
            FillShadowRequest(*shadowRtb, option, false, tpId, meshId, generalId, optionalId);


        return true;
    }

    bool MakeDrawRequestAndProps(
        DrawOption option,
        const char* drawPropName,
        RTB_DrawSC& rtb,
        TexID texId, DataBoxID screenId)
    {
        if (option.VSId == 0 || option.PSId == 0)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] MakeDrawRequestAndProps(SC): 스크린은 기본 제공 셰이더가 없음 (VSId / PSId 필수)");
            return false;
        }

        DrawProp dp = MakeDrawProp(option, DrawProp_DepthUsage_Screen, 0);
        dp.vertexType = VertexType_Simple;

        DrawPropID dpId = 0;
        TexPropID tpId = 0;
        if (!RegisterProps(drawPropName, dp, dpId, nullptr, nullptr, tpId))
            return false;


        rtb.layer = 0U;
        rtb.translucent = option.alphaClipOrTranslucent;
        rtb.texture = texId;
        rtb.drawProp = dpId;
        rtb.iid = 0U;
        rtb.screenId = screenId;


        return true;
    }

    #pragma endregion


    #pragma region ClearRequests

    void ClearOpaqueDrawRequests(uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ClearDrawRequests: 레이어 범위 밖");
            return;
        }

        LayerList[layer].actionOPQ.drawReqList.clear();
        LayerList[layer].actionGBuffer.drawReqList.clear();
        LayerList[layer].actionEarlyZ.drawReqList.clear();
    }

    void ClearTranslucentDrawRequests(uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ClearDrawRequests: 레이어 범위 밖");
            return;
        }

        LayerList[layer].actionTP.drawReqList.clear();
    }

    void ClearScreenDrawRequests(uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ClearDrawRequests: 레이어 범위 밖");
            return;
        }

        LayerList[layer].actionSC.drawReqList.clear();
    }

    void ClearShadowDrawRequests(uint8_t layer)
    {
        if (layer >= LayerList.size())
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] ClearDrawRequests: 레이어 범위 밖");
            return;
        }

        for (RTB_ActionDraw& action : LayerList[layer].actionGlobalSDWs)
            action.drawReqList.clear();
        for (RTB_ActionDraw& action : LayerList[layer].actionLocalSDWs)
            action.drawReqList.clear();
    }

    void ClearDrawRequests(RTB_ActionDraw& action)
    {
        action.drawReqList.clear();
    }


    void ClearDrawRequests(ActionID action, uint8_t layer)
    {
        Internal::PushHubReport(HubReport_Contract, L"[Hub] ClearDrawRequests: 액션 id 지정은 커스텀 파이프라인용 (미구현), 빌트인은 Clear~DrawRequests 사용");
    }

    #pragma endregion


    #pragma region RenameResource

    bool RenameMesh(std::wstring_view oldName, std::wstring_view newName)
    {
        const auto oldKey = HashString(oldName);
        const auto newKey = HashString(newName);

        if (!meshIdLUT.contains(oldKey) || meshIdLUT.contains(newKey))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RenameMesh: 없는 이름이거나 새 이름이 이미 있음");
            return false;
        }

        const auto id = meshIdLUT[oldKey];
        if (id < RESERVED_MESH_COUNT)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RenameMesh: 예약 메쉬 는 이름을 못 바꿈");
            return false;
        }

        meshIdLUT.erase(oldKey);
        meshIdLUT.emplace(newKey, id);

        return true;
    }

    bool RenameTexture(std::wstring_view oldName, std::wstring_view newName)
    {
        const auto oldKey = HashString(oldName);
        const auto newKey = HashString(newName);

        if (!textureIdLUT.contains(oldKey) || textureIdLUT.contains(newKey))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RenameTexture: 없는 이름이거나 새 이름이 이미 있음");
            return false;
        }

        const auto id = textureIdLUT[oldKey];
        if (id < RESERVED_TEXTURE_COUNT)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RenameTexture: 예약 텍스처 는 이름을 못 바꿈");
            return false;
        }

        textureIdLUT.erase(oldKey);
        textureIdLUT.emplace(newKey, id);

        return true;
    }

    bool RenameComputeBuffer(std::wstring_view oldName, std::wstring_view newName)
    {
        const auto oldKey = HashString(oldName);
        const auto newKey = HashString(newName);

        if (!csBufferIdLUT.contains(oldKey) || csBufferIdLUT.contains(newKey))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RenameComputeBuffer: 없는 이름이거나 새 이름이 이미 있음");
            return false;
        }

        const auto id = csBufferIdLUT[oldKey];
        if (id < RESERVED_CSBUFFER_COUNT)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RenameComputeBuffer: 예약 컴퓨트 버퍼 는 이름을 못 바꿈");
            return false;
        }

        csBufferIdLUT.erase(oldKey);
        csBufferIdLUT.emplace(newKey, id);

        return true;
    }

    bool RenameDrawProp(std::string_view oldName, std::string_view newName)
    {
        const auto oldKey = HashString(oldName);
        const auto newKey = HashString(newName);

        if (!drawPropIdLUT.contains(oldKey) || drawPropIdLUT.contains(newKey))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RenameDrawProp: 없는 이름이거나 새 이름이 이미 있음");
            return false;
        }

        const auto id = drawPropIdLUT[oldKey];
        if (id < RESERVED_DRAWPROP_COUNT)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RenameDrawProp: 예약 drawProp 는 이름을 못 바꿈");
            return false;
        }

        drawPropIdLUT.erase(oldKey);
        drawPropIdLUT.emplace(newKey, id);

        return true;
    }

    bool RenameTexProp(std::string_view oldName, std::string_view newName)
    {
        const auto oldKey = HashString(oldName);
        const auto newKey = HashString(newName);

        if (!texPropIdLUT.contains(oldKey) || texPropIdLUT.contains(newKey))
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RenameTexProp: 없는 이름이거나 새 이름이 이미 있음");
            return false;
        }

        const auto id = texPropIdLUT[oldKey];
        if (id < RESERVED_TEXPROP_COUNT)
        {
            Internal::PushHubReport(HubReport_Contract, L"[Hub] RenameTexProp: 예약 texProp 는 이름을 못 바꿈");
            return false;
        }

        texPropIdLUT.erase(oldKey);
        texPropIdLUT.emplace(newKey, id);

        return true;
    }

    #pragma endregion

    #pragma endregion

    

}
