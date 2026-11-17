#pragma once

#include "DataHub.h"

// 데이터허브 사용에 도움을 주는 조작 헬퍼들입니다
// 선택 기능 헤더


namespace Hakko::DataHub
{
    
    #pragma region Interface

    template <std::derived_from<RTB> T>
    void SendRTB(const T& RTB, uint8_t layer = 0U) = delete;
    
    template <std::derived_from<RTB> T>
    void SendRTB(const T& RTB, ActionID action, uint8_t layer = 0U) = delete;


    // (미구현) 커스텀 파이프라인용 액션 노드 추가
    template <std::derived_from<RTB_Action> T>
    bool AddActionNode(const T& Action, ActionID frontNode, uint8_t layer = 0U)
    {
        Internal::PushHubReport(HubReport_Contract, L"[Hub] AddActionNode: 커스텀 파이프라인 미구현");
        return false;
    }


    // SendRTB 로 전송
    #pragma region LayerCommand

    struct RenderTransferBox_SetRenderCondition : public RTB
    {
        RenderCondition condition;
    };
    typedef RenderTransferBox_SetRenderCondition RTB_SetRenderCondition, RTB_SetCondition;


    struct RenderTransferBox_SetRenderCamera : public RTB
    {
        DirectX::XMMATRIX view;
        DirectX::XMMATRIX proj;
        DirectX::XMMATRIX viewProj;
        DirectX::XMFLOAT3 eyePos;
    };
    typedef RenderTransferBox_SetRenderCamera RTB_SetRenderCamera, RTB_SetCamera;


    struct RenderTransferBox_SetRenderGlobalLight : public RTB
    {
        DirectX::XMFLOAT4 ambient;
        DirectX::XMFLOAT4 color;
        DirectX::XMFLOAT3 direction;
    };
    typedef RenderTransferBox_SetRenderGlobalLight RTB_SetRenderGlobalLight, RTB_SetGlobalLight;


    struct RenderTransferBox_SetRenderTime : public RTB
    {
        float totalTime;
    };
    typedef RenderTransferBox_SetRenderTime RTB_SetRenderTime, RTB_SetTime;



    struct RenderTransferBox_SetRenderLight : public RTB
    {
        DataBoxID light;
        uint16_t slot;
    };
    typedef RenderTransferBox_SetRenderLight RTB_SetRenderLight, RTB_SetLight;



    // 레이어 그림자 프리셋 칸에 프리셋 세팅, 칸 0 = 레이어 기본 프리셋
    struct RenderTransferBox_SetRenderShadowPreset : public RTB
    {
        DataBoxID shadowPreset;
        uint16_t slot;
    };
    typedef RenderTransferBox_SetRenderShadowPreset RTB_SetRenderShadowPreset, RTB_SetShadowPreset;

    #pragma endregion


    void SetLayerBackground(TexID texId, uint8_t layer = 0U);       // 0 이면 안 그림

    void SetLayerBackgroundColor(float r, float g, float b, float a, uint8_t layer = 0U);

    void SetLayerEnvMap(TexID texId, uint8_t layer = 0U);           // 큐브맵만, 0 이면 없음

    void SetLayerViewport(const Layer_Viewport& viewport, uint8_t layer = 0U);


    // 포워드 / 디퍼드 전환 (디퍼드면 기본 제공 라이팅 셰이더도 세팅)
    // 해당 프레임 요청을 보내기 전에 호출해서 토글해야 함 (불투명 요청이 파이프라인 종류를 따라 OPQ / GBuffer 액션으로 자동 입력)
    void SetLayerPipelineType(Layer_PipelineType type, uint8_t layer = 0U);

    // 포워드 뎁스 프리패스 켜고 끄기 (포워드에서 끄면 SSAO도 무효)
    // 해당 프레임 요청을 보내기 전에 호출해서 토글해야 함 (불투명 요청이 EarlyZ 에 자동으로 같이 들어감)
    void SetLayerEarlyZ(bool enable, uint8_t layer = 0U);


    // SendRTB 로 전송: 빌트인 Edit 액션에 기본 제공 셰이더를 꽂고 (노드가 켜짐) 액션 데이터를 채움
    #pragma region ActionCommand

    enum Tonemap_Mode
    {
        Tonemap_Mode_ACES,
        Tonemap_Mode_Reinhard,
        Tonemap_Mode_None,      // 곡선 없음 (0~1 로 자르기만)
    };

    struct RenderTransferBox_SetActionTonemap : public RTB
    {
        bool enable = true;
        uint8_t ppSlot = 0;         // actionPPs 칸 번호

        float exposureEV = 0.0f;    // 노출, 스톱 단위 (+1 = 2배, -1 = 절반)
        Tonemap_Mode mode = Tonemap_Mode_ACES;
        float gainR = 1.0f;         // 화이트 밸런스 배율
        float gainG = 1.0f;
        float gainB = 1.0f;
        float contrast = 1.0f;
        float saturation = 1.0f;    // 0 = 흑백
        float gamma = 1.0f;
    };
    typedef RenderTransferBox_SetActionTonemap RTB_SetActionTonemap, RTB_SetTonemap;


    struct RenderTransferBox_SetActionFXAA : public RTB
    {
        bool enable = true;
        uint8_t ppSlot = 0;                 // actionPPs 칸 번호

        float edgeThresholdScale = 1.0f;
        float spanMaxScale = 1.0f;
    };
    typedef RenderTransferBox_SetActionFXAA RTB_SetActionFXAA, RTB_SetFXAA;


    // actionAO 에 꽂음
    struct RenderTransferBox_SetActionSSAO : public RTB
    {
        bool enable = true;

        float radiusScale = 1.0f;
        float intensity = 1.0f;
        float biasScale = 1.0f;
    };
    typedef RenderTransferBox_SetActionSSAO RTB_SetActionSSAO, RTB_SetSSAO;


    // actionAOBlur 에 꽂음 (SSAO 결과를 읽으니 SSAO 와 같이)
    struct RenderTransferBox_SetActionAOBlur : public RTB
    {
        bool enable = true;

        float depthThresholdScale = 1.0f;
    };
    typedef RenderTransferBox_SetActionAOBlur RTB_SetActionAOBlur, RTB_SetAOBlur;


    // 블룸 세트: 줄인 RT 텍스처들과 그걸 읽는 texProp 묶음 (단계 i 의 크기 = 기준 크기의 1 / 2^(i + 1))
    // 크기는 만들 때 고정 (창 크기가 바뀌어도 돌긴 함, 해상도만 안 맞음)
    #define RENDER_BLOOM_LEVEL_MAX 6

    struct BloomChain
    {
        std::string_view name;      // 텍스처 / texProp 이름의 앞부분 (뒤에 단계 번호가 붙음)
        uint32_t levels = 0;        // 줄이는 단계 수
        uint32_t width = 0;         // 기준 (화면) 크기
        uint32_t height = 0;
    };

    // 텍스처 / texProp 을 만들어 등록 (이름 = name + 단계 번호, 업로드 RTB 는 서비스가 같은 프레임에), 실패하면 levels 0
    BloomChain CreateBloomChain(const char* name, uint32_t width, uint32_t height, uint32_t levels = 3);

    // actionPPs[ppSlot] 부터 단계 수 x 2 칸 차지, 톤맵보다 앞 칸에 둘 것
    // 끄면 차지한 칸의 출력 설정도 기본으로 되돌림
    struct RenderTransferBox_SetActionBloom : public RTB
    {
        bool enable = true;
        uint8_t ppSlot = 0;             // 첫 칸

        BloomChain chain;

        float thresholdScale = 1.0f;
        float kneeScale = 1.0f;
        float radiusScale = 1.0f;
        float intensity = 1.0f;
    };
    typedef RenderTransferBox_SetActionBloom RTB_SetActionBloom, RTB_SetBloom;

    #pragma endregion


    // 그림자 한 장을 그리는 쪽 (섀도우 액션) 과 읽는 쪽 (캐스케이드 / 그림자 프리셋) 에 한번에 세팅
    // 아틀라스는 정사각 격자, 타일 번호 = 격자 칸 번호 = 섀도우 액션 칸 번호 (왼쪽 위가 0, 가로로 먼저)
    // 캐스터 요청은 그 액션에 SendRTB(요청, 액션), 자세한 규칙과 제한은 가이드 참조
    #pragma region ShadowCommand

    struct ShadowAtlas
    {
        std::wstring_view name; // DS 텍스처 이름 (CreateShadowAtlas 로 만들거나 직접)
        uint32_t size = 0;      // 한 변 픽셀
        uint32_t grid = 1;      // 한 변을 몇 칸으로 나누는지 (2 = 타일 4장)
    };

    // DS 텍스처를 만들어 등록 (업로드 RTB 는 서비스가 같은 프레임에), 실패하면 size 0
    ShadowAtlas CreateShadowAtlas(const wchar_t* name, uint32_t size, uint32_t grid = 1);


    // 전역조명 캐스케이드 한 장: actionGlobalSDWs[cascade] + 레이어 컨디션의 캐스케이드 칸 + globalShadowTexId
    struct RenderTransferBox_SetShadowCascade : public RTB
    {
        bool enable = true;
        uint8_t cascade = 0;                // 캐스케이드 번호 = 타일 번호

        ShadowAtlas atlas;

        DirectX::XMMATRIX lightViewProj;    // 조명 뷰프로젝션 (다른 행렬처럼 전치해서)
        float splitEnd = 0.0f;              // 이 캐스케이드가 덮는 뷰 깊이의 끝
    };
    typedef RenderTransferBox_SetShadowCascade RTB_SetShadowCascade;


    // 로컬 조명 그림자 한 장: actionLocalSDWs[tile] + 레이어 프리셋 칸에 꽂힌 프리셋의 칸 하나 + localShadowTexId
    // 프리셋 칸이 비어 있으면 프리셋을 새로 만들어 꽂음 (해제는 서비스가)
    struct RenderTransferBox_SetShadowLocal : public RTB
    {
        bool enable = true;
        uint16_t tile = 0;                  // 타일 번호

        ShadowAtlas atlas;

        DirectX::XMMATRIX lightViewProj;
        uint16_t lightSlot = 0;             // 이 그림자를 내는 조명칸 (0 은 빈 자리라 불가)
        uint16_t presetSlot = 0;            // 레이어 프리셋 칸 (0 = 기본 프리셋)
        uint8_t entry = 0;                  // 프리셋 안의 칸
    };
    typedef RenderTransferBox_SetShadowLocal RTB_SetShadowLocal;

    #pragma endregion


    // 기본 제공 파티클 (가산 블렌드 사각형, 방출 모양 3종): 시뮬은 컴퓨트 선계산 칸, 그리기는 반투명 요청
    #pragma region ParticleCommand

    #define RENDER_PARTICLE_STRIDE 64       // 파티클 하나의 바이트 수 (defaultParticle_CS 의 Particle 과 손으로 맞춤)
    #define RENDER_PARTICLE_GROUP 64        // 스레드 그룹 하나가 도는 파티클 수 (버퍼 칸 수는 이 배수)

    struct ParticleBuffer
    {
        std::wstring_view name;
        uint32_t count = 0;         // 파티클 수
    };

    // 파티클 버퍼를 만들어 등록 (count 는 그룹 배수로 올림, 업로드 RTB 는 서비스가 같은 프레임에), 실패하면 count 0
    ParticleBuffer CreateParticleBuffer(const wchar_t* name, uint32_t count);

    enum Particle_Shape
    {
        Particle_Shape_Cone,        // 한 점에서 emitDir 축 둘레로 coneAngle 만큼 벌어져 나감
        Particle_Shape_Sphere,      // 반지름 (shapeSize.x) 안의 무작위 자리에서 바깥쪽으로
        Particle_Shape_Box,         // 상자 (반 변 길이 shapeSize) 안의 무작위 자리에서 emitDir 방향으로
    };

    // SendRTB 로 전송: actionPreComputes[computeSlot] 에 파티클 시뮬을 꽂음
    // 시간 두 칸은 매 프레임 채워서 다시 보낼 것 (dt 0 이면 멈춤)
    struct RenderTransferBox_SetActionParticle : public RTB
    {
        bool enable = true;
        uint8_t computeSlot = 0;        // actionPreComputes 칸 번호

        ParticleBuffer buffer;

        float dt = 0.0f;                // 이번 프레임에 흐른 시간 (초)
        float time = 0.0f;              // 누적 시간 (난수 시드용, 프레임마다 달라지기만 하면 됨)
        float emitRatio = 1.0f;         // 버퍼 칸 중 순환시킬 비율 (0 = 방출 멈춤, 살아 있던 건 수명대로 사라짐)

        Particle_Shape shape = Particle_Shape_Cone;
        DirectX::XMFLOAT3 emitPos = {0.0f, 0.0f, 0.0f};         // 방출 위치 (월드)
        DirectX::XMFLOAT3 emitDir = {0.0f, 1.0f, 0.0f};         // 방출 방향
        float coneAngle = 0.34f;                                // 축에서 벌어지는 최대 각도 (라디안), Cone 용
        DirectX::XMFLOAT3 shapeSize = {0.0f, 0.0f, 0.0f};       // Sphere: x = 반지름 / Box: 반 변 길이

        DirectX::XMFLOAT3 accel = {0.0f, -3.0f, 0.0f};          // 가속도 (중력 포함)
        float drag = 0.0f;              // 저항: 초당 속도가 줄어드는 비율
        float speedMin = 1.8f;          // 태어날 때의 속도 범위 (초당 이동량)
        float speedMax = 3.0f;
        float lifeMin = 1.0f;           // 수명 범위 (초)
        float lifeMax = 2.0f;
        float sizeMin = 0.02f;          // 태어날 때의 크기 범위 (사각형 반 변 길이)
        float sizeMax = 0.04f;
        float sizeEnd = 1.0f;           // 수명이 끝날 때의 크기 배율 (1 = 그대로)
        DirectX::XMFLOAT3 colorStart = {4.0f, 1.6f, 0.4f};      // 태어날 때의 색 (HDR)
        DirectX::XMFLOAT3 colorEnd = {0.0f, 0.0f, 0.0f};        // 수명이 끝날 때의 색 (가산 블렌드라 검정 = 사라짐)
    };
    typedef RenderTransferBox_SetActionParticle RTB_SetActionParticle, RTB_SetParticle;

    // (반투명)파티클 그리기 요청 (파티클 drawProp + 사각형 메쉬 + 인스턴스 수 + 버퍼)
    // drawProp 0 = 디폴트 가산 블렌드, texProp 0 = 디폴트 둥근 점으로 (0번 칸에 텍스처를 넣은 texProp 을 넘기면 그 텍스처로)
    // 요청에 든 버퍼 id 는 만든 시점 것 (다른 요청처럼 보내기 전에 다시 만들거나 id 를 다시 얻을 것)
    RTB_DrawTP MakeParticleRequest(const ParticleBuffer& buffer, DrawPropID drawProp = 0, TexPropID texProp = 0);

    #pragma endregion


    // 기본 제공 컴퓨트 스키닝: 스킨드 정점을 컴퓨트 선계산 칸에서 미리 변형, 그릴 땐 스태틱 메쉬로 그리기
    #pragma region SkinningCommand

    #define RENDER_SKINNING_GROUP 64                        // 스레드 그룹 하나가 도는 정점 수 (defaultSkinning_CS 와 맞춤)
    #define RENDER_SKINNING_SOURCE_SUFFIX L"|SkinSource"    // 원본 버퍼 이름을 안 넘겼을 때의 기본 이름 접미사

    struct SkinnedMesh
    {
        std::wstring_view name;
        std::wstring_view sourceName;   // 원본 버퍼 이름, 비어 있으면 기본 이름
        uint32_t count = 0;             // 그룹 배수로 올린 정점 수
    };

    // 스킨드 메쉬를 받아 원본 버퍼 / 결과 버퍼 / 결과를 가리키는 메쉬를 만들어 등록 (업로드 RTB 는 서비스가 같은 프레임에), 실패하면 count 0
    // 메쉬와 결과 버퍼는 name, 원본 버퍼는 sourceName 으로 등록됨 (안 넘기면 기본 이름). 셋 중 하나라도 이름이 겹치면 아무것도 안 만듦
    SkinnedMesh CreateSkinnedMesh(const wchar_t* name, Mesh&& mesh, const wchar_t* sourceName = nullptr);

    // 원본 버퍼의 실제 이름 (넘긴 이름 또는 기본 이름)
    std::wstring GetSkinnedMeshSourceName(const SkinnedMesh& skinned);

    // 원본 정점을 통째로 바꿈 (모프 등). 정점 수는 만들 때와 같을 것 (업데이트 RTB 는 서비스가 같은 프레임에)
    bool UpdateSkinnedMeshVertices(const SkinnedMesh& skinned, const std::vector<VertexSkinned>& vertices);

    // SendRTB 로 전송: actionPreComputes[computeSlot] 에 스키닝을 꽂음 (메쉬 하나가 한 칸)
    // 본 행렬은 서비스가 boneDataId 의 박스 값을 고치면 됨 (재전송 필요 x, 드로우 요청이 아니므로)
    struct RenderTransferBox_SetActionSkinning : public RTB
    {
        bool enable = true;
        uint8_t computeSlot = 0;        // actionPreComputes 칸 번호

        SkinnedMesh mesh;
        DataBoxID boneDataId = 0;       // DtB_3DOptional (본 행렬)
    };
    typedef RenderTransferBox_SetActionSkinning RTB_SetActionSkinning, RTB_SetSkinning;

    #pragma endregion


    // 일반 드로우 요청: 빌트인 요청을 RTB_DrawRequest 로 바꿔서 남는 데이터 칸을 채우거나, 원하는 액션에 직접 보낼 때
    // 액션은 레이어의 필드로 지정 (예: LayerList[0].actionLocalSDWs[3]), 이 경로는 자동 분배 (EarlyZ / GBuffer) 가 없음
    #pragma region RawDrawRequest

    RTB_DrawRequest MakeDrawRequest(const RTB_DrawOPQ& RTB);

    RTB_DrawRequest MakeDrawRequest(const RTB_DrawTP& RTB);

    RTB_DrawRequest MakeDrawRequest(const RTB_DrawSDW& RTB);

    RTB_DrawRequest MakeDrawRequest(const RTB_DrawSC& RTB);


    // 드로우 요청에서 그림자 요청 만들기
    // 기본 제공 drawProp 은 알아서 섀도우 drawProp 으로 변환
    // 직접 만든 drawProp 이면 섀도우용 drawProp 을 넘길 것
    RTB_DrawSDW MakeShadowRequest(const RTB_DrawOPQ& RTB, DrawPropID shadowDrawProp = 0);

    RTB_DrawSDW MakeShadowRequest(const RTB_DrawTP& RTB, DrawPropID shadowDrawProp = 0);


    void SendRTB(const RTB_DrawRequest& RTB, RTB_ActionDraw& action);

    // 섀도우 요청은 섀도우 액션에만 (다른 종류 액션이면 리포트하고 버림)
    void SendRTB(const RTB_DrawSDW& RTB, RTB_ActionDraw& action);


    // 요청의 dataBoxIds[slot] 이 가리킬 데이터 타입 세팅 (그 액션의 요청 전부에 적용)
    void SetActionDataType(RTB_ActionDraw& action, uint8_t slot, DataBoxTID tid);

    // 액션 출력을 레이아웃에 자동 맞춤, 추가 칸은 한번 더 직접 넣기
    // 텍스처 / 블렌드 / 클리어 설정은 그대로, None 은 레이아웃 값만 바꿈 (Draw 이외 용)
    void SetActionOutputLayout(RTB_ActionDraw& action, Action_OutputLayout layout);

    #pragma endregion


    #pragma region MakeDrawRequest

    // 익히 말하는... 머터리얼
    // 간편 사용 용도, 빌트인 3종 (Opaque / Translucent / Screen) 이 쓰는 속성만 받음
    // 세부 조정은 CreateDrawProp / CreateTexProp 직접 하세요
    struct DrawOption
    {
        DrawProp_CullType cullType = DrawProp_CullType_Back;
        DrawProp_BlendType blendType = DrawProp_BlendType_None;     // Opaque 요청은 None / AlphaClip 만, Translucent 요청은 AlphaClip 불가 (None 이면 Alpha 로)
        bool alphaClipOrTranslucent = false;                        // Opaque 요청에서 켜면 blendType 을 AlphaClip 으로 (컷오프는 DataBox_Render3DGeneral.alphaCutoff)

        VertexType vertexType = VertexType_Static;
        ShaderID VSId = 0;      // 0 이면 기본 제공 셰이더 (Default_World_VS) 스크린은 기본 제공이 없어서 필수 <- 아닐 예정
        ShaderID PSId = 0;      // 0 이면 기본 제공 셰이더 (Opaque = Default_World_OPQ_PS / Translucent = Default_World_TP_PS)

        TexID diffuseTexId = RESERVED_TEX_NONE_DIFFUSE_ID;
        TexID normalTexId = RESERVED_TEX_NONE_NORMAL_ID;
        TexID ORMTexId = RESERVED_TEX_NONE_ORM_ID;
        TexID emissionTexId = RESERVED_TEX_NONE_EMISSION_ID;
        TexID flexibleTexId[TEXPROP_SLOT_COUNT - 4] = {0};
    };

    // 그림자는 기본 제공 VS 로 그리는 Static / Skinned 정점만 (섀도우는 뒷면 컬링 고정)
    // 그 외 사용은 리포트, 섀도우 요청의 drawProp 을 직접 채울 것
    bool MakeDrawRequestAndProps(
        DrawOption option, 
        const char* drawPropName, const char* texPropName, 
        RTB_DrawOPQ& rtb,
        MeshID meshId = 0U, DataBoxID generalId = 0U, DataBoxID optionalId = 0U,
        RTB_DrawSDW* shadowRtb = nullptr
    );

    bool MakeDrawRequestAndProps(
        DrawOption option, 
        const char* drawPropName, const char* texPropName, 
        RTB_DrawTP& rtb,
        MeshID meshId = 0U, DataBoxID generalId = 0U, DataBoxID optionalId = 0U,
        RTB_DrawSDW* shadowRtb = nullptr
    );

    bool MakeDrawRequestAndProps(
        DrawOption option, 
        const char* drawPropName, 
        RTB_DrawSC& rtb,
        TexID texId, DataBoxID screenId = 0U
    );

    #pragma endregion


    #pragma region ClearRequests

    // 드로우 요청 비우기 (요청 목록은 서비스가 비우는 계약)
    // 빌트인 파이프라인: SendRTB 로 넣은 요청 종류별로
    void ClearOpaqueDrawRequests(uint8_t layer = 0U);

    void ClearTranslucentDrawRequests(uint8_t layer = 0U);

    void ClearScreenDrawRequests(uint8_t layer = 0U);

    void ClearShadowDrawRequests(uint8_t layer = 0U);       // 섀도우 액션 전부

    // 액션을 직접 지정 (SendRTB(요청, 액션) 으로 넣은 것)
    void ClearDrawRequests(RTB_ActionDraw& action);


    // (미구현) 커스텀 파이프라인용: 액션 id 로 지정
    void ClearDrawRequests(ActionID action, uint8_t layer = 0U);

    #pragma endregion


    // 리소스 이름 바꾸기
    // 없는 이름이거나, 새 이름이 이미 있거나, 예약 리소스면 실패
    #pragma region RenameResource

    bool RenameMesh(std::wstring_view oldName, std::wstring_view newName);

    bool RenameTexture(std::wstring_view oldName, std::wstring_view newName);

    bool RenameComputeBuffer(std::wstring_view oldName, std::wstring_view newName);

    bool RenameDrawProp(std::string_view oldName, std::string_view newName);

    bool RenameTexProp(std::string_view oldName, std::string_view newName);

    #pragma endregion

    #pragma endregion
    


#pragma region Template

template <>
void DataHub::SendRTB<DataHub::RTB_UpdateResources>(const RTB_UpdateResources& RTB, uint8_t layer);


template <>
void DataHub::SendRTB<DataHub::RTB_SetCondition>(const RTB_SetCondition& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetCamera>(const RTB_SetCamera& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetGlobalLight>(const RTB_SetGlobalLight& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetTime>(const RTB_SetTime& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetLight>(const RTB_SetLight& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetShadowPreset>(const RTB_SetShadowPreset& RTB, uint8_t layer);


template <>
void DataHub::SendRTB<DataHub::RTB_SetTonemap>(const RTB_SetTonemap& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetFXAA>(const RTB_SetFXAA& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetSSAO>(const RTB_SetSSAO& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetAOBlur>(const RTB_SetAOBlur& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetBloom>(const RTB_SetBloom& RTB, uint8_t layer);


template <>
void DataHub::SendRTB<DataHub::RTB_SetShadowCascade>(const RTB_SetShadowCascade& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetShadowLocal>(const RTB_SetShadowLocal& RTB, uint8_t layer);


template <>
void DataHub::SendRTB<DataHub::RTB_SetParticle>(const RTB_SetParticle& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_SetSkinning>(const RTB_SetSkinning& RTB, uint8_t layer);


template <>
void DataHub::SendRTB<DataHub::RTB_DrawOPQ>(const RTB_DrawOPQ& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_DrawTP>(const RTB_DrawTP& RTB, uint8_t layer);

template <>
void DataHub::SendRTB<DataHub::RTB_DrawSC>(const RTB_DrawSC& RTB, uint8_t layer);

#pragma endregion

}
