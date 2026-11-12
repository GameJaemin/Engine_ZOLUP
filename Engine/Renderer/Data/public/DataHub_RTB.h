#pragma once

#include "RenderOption.h"
#include "DataHub_Types.h"

// RenderTransferBox: 렌더러로 전송할 요청 단위 (드로우 리퀘스트 / 액션 / 레이어)
// Canvas(RT) - Layer(Pipeline) - Action(Pass) - DrawRequest(Data) 순 포함구조


namespace Hakko::DataHub
{

    #pragma region RenderTransferBox

    struct RenderTransferBox
    {
    protected:
        RenderTransferBox() = default;
    };
    typedef RenderTransferBox RTB;


    #pragma region DrawRequest

    #define DRAWREQ_DATABOX_MAX 5

    struct RenderTransferBox_DrawRequest : public RTB
    {
        RenderID renderId;      // RenderIDProvider 확장 클래스를 이용하세요
        MeshID meshId;          // renderId 안의 메쉬 비트는 정렬용, 조회는 여기서
        SubMeshID subMeshId;

        DataBoxID dataBoxIds[DRAWREQ_DATABOX_MAX];
    };
    typedef RenderTransferBox_DrawRequest RTB_DrawRequest;
    

    #pragma region BuiltInRequests

    struct RenderTransferBox_DrawRequestShadow : public RTB
    {
        // 알파(1) psoBody(16) 텍스처조합(16) 메쉬(16) 뎁스(8) 잉여(7)
        bool alphaClip;
        DrawPropID drawProp;
        TexPropID texProp;
        MeshID meshId;
        uint8_t depth;
        uint8_t iid;

        SubMeshID subMeshId;
        DataBoxID generalId;
        DataBoxID optionalId;
    };
    typedef RenderTransferBox_DrawRequestShadow RTB_DrawRequestShadow, RTB_DrawSDW;


    struct RenderTransferBox_DrawRequestOpaque : public RTB
    {
        // 알파(1) 레이어(7) psoBody(16) 텍스처조합(16) 메쉬(16) 뎁스(8)
        bool alphaClip;
        uint8_t layer;
        DrawPropID drawProp;
        TexPropID texProp;
        MeshID meshId;
        uint8_t depth;

        SubMeshID subMeshId;
        DataBoxID generalId;
        DataBoxID optionalId;
    };
    typedef RenderTransferBox_DrawRequestOpaque RTB_DrawRequestOpaque, RTB_DrawOPQ;

    
    struct RenderTransferBox_DrawRequestTranslucent : public RTB
    {
        // 뎁스(16) psoBody(16) 텍스처조합(16) 잉여(16)
        uint16_t depth;
        DrawPropID drawProp;
        TexPropID texProp;
        uint16_t iid;

        MeshID meshId;
        SubMeshID subMeshId;
        DataBoxID generalId;
        DataBoxID optionalId;
    };
    typedef RenderTransferBox_DrawRequestTranslucent RTB_DrawRequestTranslucent, RTB_DrawTP;

    
    struct RenderTransferBox_DrawRequestScreen : public RTB
    {
        // 레이어(16) 반투명(1) 텍스처(16) psoBody(16) 잉여(15)
        uint16_t layer;
        bool translucent;
        TexID texture;
        DrawPropID drawProp;
        uint16_t iid;

        DataBoxID screenId;
    };
    typedef RenderTransferBox_DrawRequestScreen RTB_DrawRequestScreen, RTB_DrawSC;

    #pragma endregion

    #pragma endregion



    #pragma region ResourceCommand

    enum RTBCMD_UpdateResource
    {
        RTBCMD_UpdateResource_Upload,
        RTBCMD_UpdateResource_Release,
    };

    struct RenderTransferBox_UpdateResource : public RTB
    {
        RTBCMD_UpdateResource command;
    };
    typedef RenderTransferBox_UpdateResource RTB_UpdateResource;


    enum RTBCMD_UpdateProp
    {
        RTBCMD_UpdateProp_FindOrBuild,
        RTBCMD_UpdateProp_Release,
    };

    struct RenderTransferBox_UpdateProp : public RTB
    {
        RTBCMD_UpdateProp command;
    };
    typedef RenderTransferBox_UpdateProp RTB_UpdateProp;


    extern bool flag_updateResources_Upload;
    extern bool flag_updateProps_FindOrBuild;

    extern bool flag_updateResources_Release;
    extern bool flag_updateProps_Release;

    #pragma endregion



    #pragma region Action

    struct Action_Viewport
    {
        float originX;
        float originY;
        float width;
        float height;
        float minZ = 0.0f;
        float maxZ = 1.0f;
    };

    struct Action_Rect
    {
        int32_t originX;
        int32_t originY;
        int32_t width;
        int32_t height;
    };


    #pragma region IOSignature

    // 선행 액션에서 받은 임시 출력 자원의 사용방식
    enum Action_InputType
    {
        Action_InputType_NoConvert = 0,      // 그대로 이어그리기
        Action_InputType_Convert = 0x01,     // 읽기용 변환 (복사 우선)
        Action_InputType_Copy = 0x02,        // 읽기용 복사
        Action_InputType_Detach = 0x04,      // 임시 자원 폐기 (변환, 복사 우선) 후 새 자원으로
        
        Action_InputType_ClearSR = 0x08,     // 이전 임시 SR 폐기

        Action_Input_DrawDefault = Action_InputType_NoConvert,
        Action_Input_EditDefault = Action_InputType_Convert,
    };


    #define RENDER_TARGET_MAX 8

    struct Action_Input
    {
        std::array<Action_InputType, RENDER_TARGET_MAX> RTInputType;
        Action_InputType DSInputType;
    };


    enum Action_Output_BlendOption
    {
        Action_Output_BlendOption_ByBlendType,

        Action_Output_BlendOption_ForceNone,
        Action_Output_BlendOption_ForceAlpha,
        Action_Output_BlendOption_ForceAddtive,
        Action_Output_BlendOption_ForceMultiply,
    };

    enum Action_Output_DepthOption
    {
        Action_Output_DepthOption_ByDepthUsage,

        Action_Output_DepthOption_ForceDepthRead,
    };

    // 출력 레이아웃 (의미 정보): PS가 어느 칸에 뭘 쓰는지, 셰이더 변형 컴파일용
    enum Action_OutputLayout
    {
        OutputLayout_None = 0,
        OutputLayout_DepthOnly,
        OutputLayout_Color,
        OutputLayout_ColorId,
        OutputLayout_GBuffer,

        OutputLayout_Count,
    };

    inline uint8_t OutputLayoutTargetMask(Action_OutputLayout layout)
    {
        switch (layout)
        {
        case OutputLayout_Color:    return 0b0001;
        case OutputLayout_ColorId:  return 0b0011;
        case OutputLayout_GBuffer:  return 0b1111;
        default:                    return 0;
        }
    }

    // 액션에서 사용할 출력 정보
    struct Action_Output
    {
        std::array<bool, RENDER_TARGET_MAX> useRT = {0};
        std::array<TexID, RENDER_TARGET_MAX> RTTex = {0};
        std::array<Action_Output_BlendOption, RENDER_TARGET_MAX> blendOpt = {(Action_Output_BlendOption)0};

        bool useDS = 0;
        TexID DSTex = 0;
        Action_Output_DepthOption depthOpt = {(Action_Output_DepthOption)0};

        Action_OutputLayout outputLayout = OutputLayout_None;
    };

    #pragma endregion


    #pragma region ActionVariant

    #define RENDER_ACTION_NODE_MAX 8    

    struct RenderTransferBox_Action : public RTB
    {
    public:
        std::array<Action_Input, RENDER_ACTION_NODE_MAX> inputs;
        Action_Output output;

        Action_Viewport viewport;
        Action_Rect rect;

        DataBoxTID actionDataBoxTypeId;
        DataBoxID actionDataBoxId;

    protected:
        RenderTransferBox_Action(Action_InputType defaultInput)
        {
            for (auto& node : inputs)
            {
                node.RTInputType.fill(defaultInput);
                node.DSInputType = defaultInput;
            }
            viewport = {};  rect = {};
            actionDataBoxTypeId = 0;    actionDataBoxId = 0;
        }
    };
    typedef RenderTransferBox_Action RTB_Action;    

    typedef uint32_t ActionID;

    
    struct RenderTransferBox_ActionDraw : public RTB_Action
    {
        RenderIDType renderIdType;
        std::array<DataBoxTID, DRAWREQ_DATABOX_MAX> dataBoxTypeIds;
        std::vector<RTB_DrawRequest> drawReqList;

        RenderTransferBox_ActionDraw() : RenderTransferBox_Action(Action_Input_DrawDefault)
        {
            output.useRT[0] = true;
            output.useDS = true;
            output.outputLayout = OutputLayout_Color;
            dataBoxTypeIds.fill(0);
        }
    };
    typedef RenderTransferBox_ActionDraw RTB_ActionDraw;


    struct RenderTransferBox_ActionEdit : public RTB_Action
    {
        DrawPropID drawProp;
        TexPropID texProp;

        RenderTransferBox_ActionEdit() : RenderTransferBox_Action(Action_Input_EditDefault)
        {
            drawProp = 0;   texProp = 0;
        }
    };
    typedef RenderTransferBox_ActionEdit RTB_ActionEdit;
    

    struct RenderTransferBox_ActionCompute : public RTB_Action
    {
        // 미구현 (컴퓨트 경로)
        ShaderID csId;
        uint32_t dispatchX;
        uint32_t dispatchY;
        uint32_t dispatchZ;

        RenderTransferBox_ActionCompute() : RenderTransferBox_Action(Action_Input_EditDefault)
        {
            csId = 0;
            dispatchX = dispatchY = dispatchZ = 0;
        }
    };
    typedef RenderTransferBox_ActionCompute RTB_ActionCompute;


    enum BuiltinActionType
    {
        BuiltinAction_Background = 0,
    };

    struct RenderTransferBox_ActionBuiltin : public RTB_Action
    {
        BuiltinActionType builtinType;

        RenderTransferBox_ActionBuiltin() : RenderTransferBox_Action(Action_Input_DrawDefault)
        {
            output.useRT[0] = true;
            output.useDS = true;
            output.outputLayout = OutputLayout_Color;
            builtinType = BuiltinAction_Background;
        }
    };
    typedef RenderTransferBox_ActionBuiltin RTB_ActionBuiltin;

    #pragma endregion

    #pragma endregion



    #pragma region Layer

    #define RENDER_LIGHT_MAX 64
    #define RENDER_LAYER_MAX 8
    
    struct Layer_BackGround
    {
        TexID bgTexId;
        float color[4];
    };

    struct Layer_Viewport
    {
        float originX;
        float originY;
        float width;
        float height;
        float minZ = 0.0f;
        float maxZ = 1.0f;
    };


    #pragma region LayerCommand

    struct RenderTransferBox_SetRenderCondition : public RTB
    {
        RenderCondition condition;
    };
    typedef RenderTransferBox_SetRenderCondition RTB_SetRenderCondition, RTB_SetCondition;
    

    struct RenderTransferBox_SetRenderLight : public RTB
    {
        DataBoxID light;
        uint16_t slot;
    };
    typedef RenderTransferBox_SetRenderLight RTB_SetRenderLight, RTB_SetLight;
    

    struct RenderTransferBox_SetRenderLightMatrix : public RTB
    {
        DataBoxID lightMatrix;
        uint16_t slot;
    };
    typedef RenderTransferBox_SetRenderLightMatrix RTB_SetRenderLightMatrix, RTB_SetLightMatrix;

    #pragma endregion
    

    // 매크로는 RenderOption.h 에 정의되어 있습니다
    #pragma region LayerPipelineVariant

    #if RENDER_PIPELINE_MODE == RENDER_USE_PIPELINE_BUILTIN

    enum Layer_PipelineType
    {
        Layer_PipelineType_Forward,
        Layer_PipelineType_Defered,
    };


    struct RenderTransferBox_LayerBuiltIn : public RTB
    {
        Layer_PipelineType pipelineType;

        Layer_BackGround backGround;
        TexID envTexId;
        Layer_Viewport viewport;
        

        RenderCondition condition;
        std::array<DataBoxID, RENDER_LIGHT_MAX> lights;
        std::array<DataBoxID, RENDER_LIGHT_MAX> lightMatrixes;

        DataBoxTID layerDataBoxTid;
        DataBoxID layerDataBoxId;


        RTB_ActionDraw actionEarlyZ;                    // 포워드용
        RTB_ActionDraw actionGBuffer;                   // 디퍼드용

        RTB_ActionDraw actionGlobalSDW;                 // 현재 제한적으로 지원 (로드된 텍스처에 쓰기가 불가)
        std::array<RTB_ActionDraw, RENDER_LIGHT_MAX> actionLocalSDWs;   // 동일

        RTB_ActionDraw actionOPQ;
        RTB_ActionBuiltin actionBackground;             // OPQ 뒤 고정 (스카이박스 / 배경판)
        RTB_ActionDraw actionTP;

        std::array<RTB_ActionEdit, 16> actionPPs;

        RTB_ActionDraw actionSC;

        RTB_ActionDraw actionDebug;


        RenderTransferBox_LayerBuiltIn();
    };
    typedef RenderTransferBox_LayerBuiltIn RenderTransferBox_Layer;

    inline RenderTransferBox_LayerBuiltIn::RenderTransferBox_LayerBuiltIn() : 
            backGround({0, {1.0f, 1.0f, 1.0f, 1.0f}}), envTexId(0), viewport(), 
            condition(), lights(), lightMatrixes(),
            layerDataBoxTid(0), layerDataBoxId(0),
            actionLocalSDWs({}), actionPPs({})
    {
        actionEarlyZ.renderIdType = RenderIdType_Opaque;
        actionEarlyZ.output.outputLayout = OutputLayout_DepthOnly;
        actionEarlyZ.dataBoxTypeIds[0] = DataBoxType_DataBox_Render3DGeneral;
        actionEarlyZ.dataBoxTypeIds[1] = DataBoxType_DataBox_Render3DOptional;

        actionGBuffer.output.outputLayout = OutputLayout_GBuffer;

        actionGlobalSDW.output.useRT[0] = false;
        // actionGlobalSDW.output.useDS = true;
        actionGlobalSDW.renderIdType = RenderIdType_Shadow;
        actionGlobalSDW.output.outputLayout = OutputLayout_DepthOnly;
        actionGlobalSDW.dataBoxTypeIds[0] = DataBoxType_DataBox_Render3DGeneral;
        actionGlobalSDW.dataBoxTypeIds[1] = DataBoxType_DataBox_Render3DOptional;

        for (auto& action : actionLocalSDWs)
        {
            action.output.useRT[0] = false;
            // action.output.useDS = true;
            action.renderIdType = RenderIdType_Shadow;
            action.output.outputLayout = OutputLayout_DepthOnly;
            action.dataBoxTypeIds[0] = DataBoxType_DataBox_Render3DGeneral;
            action.dataBoxTypeIds[1] = DataBoxType_DataBox_Render3DOptional;
        }

        actionOPQ.inputs[0].RTInputType[0] = Action_InputType_Detach;
        actionOPQ.inputs[1].RTInputType[0] = Action_InputType_Detach;
        actionOPQ.inputs[1].DSInputType = Action_InputType_Detach;
        actionOPQ.inputs[2].DSInputType = Action_InputType_Detach;
        actionOPQ.renderIdType = RenderIdType_Opaque;
        actionOPQ.dataBoxTypeIds[0] = DataBoxType_DataBox_Render3DGeneral;
        actionOPQ.dataBoxTypeIds[1] = DataBoxType_DataBox_Render3DOptional;

        // actionTP.inputs[0].RTInputType[0] = Action_InputType_NoConvert;
        actionTP.renderIdType = RenderIdType_Translucent;
        actionTP.dataBoxTypeIds[0] = DataBoxType_DataBox_Render3DGeneral;
        actionTP.dataBoxTypeIds[1] = DataBoxType_DataBox_Render3DOptional;

        // 출력 전부 false면 노드 스킵 (PP 기본)

        // actionSC.inputs[0].RTInputType[0] = Action_InputType_NoConvert;
        actionSC.renderIdType = RenderIdType_Screen;
        actionSC.dataBoxTypeIds[0] = DataBoxType_DataBox_Render2DScreen;
    }
    

    #elif RENDER_PIPELINE_MODE == RENDER_USE_PIPELINE_CUSTOM
    
    struct RenderTransferBox_LayerCustom : public RTB
    {
        Layer_BackGround backGround;
        TexID envTexId;
        Layer_Viewport viewport;
        
        RenderCondition condition;
        std::array<DataBoxID, RENDER_LIGHT_MAX> lights;
        std::array<DataBoxId, RENDER_LIGHT_MAX> lightMatrixes;

        DataBoxTID layerDataBoxTid;
        DataBoxID layerDataBoxId;


        std::vector<std::array<ActionID, RENDER_ACTION_NODE_MAX>> actionNodeList;       // 아래 리스트와 일대일대응, 본인의 선행 노드를 가리킴
        std::vector<std::variant<RTB_ActionDraw, RTB_ActionEdit, RTB_ActionCompute, RTB_ActionBuiltin>> actionList;    // 0번 빈칸 (Base)

        RenderTransferBox_LayerCustom() : 
            backGround({0, {1.0f, 1.0f, 1.0f, 1.0f}}), envTexId(0), viewport(), 
            condition(), lights(), lightMatrixes(),
            layerDataBoxTid(0), layerDataBoxId(0), actionNodeList(1), actionList(1) {}
    };
    typedef RenderTransferBox_LayerCustom RenderTransferBox_Layer;

    #endif

    #pragma endregion


    typedef RenderTransferBox_Layer RTB_Layer;

    extern std::vector<RTB_Layer> LayerList;

    #pragma endregion

    #pragma endregion

}
