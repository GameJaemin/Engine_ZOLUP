#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <string_view>
#include <variant>

#include <DirectXMath.h>

// 데이터허브 내 자료형 정의 (리소스 타입 / 데이터박스)


namespace Hakko::DataHub
{

    #pragma region Typedef

    typedef uint32_t HashedString;


    typedef uint32_t DataBoxID;
    typedef DataBoxID DtBID;


    typedef uint64_t RenderID;

    enum RenderIDType
    {
        RenderIdType_None,          // ActionEdit 에서 활용
        RenderIdType_Opaque,
        RenderIdType_Translucent,
        RenderIdType_Shadow,
        RenderIdType_Screen,
    };
    

    #pragma region Vertex

    // 정점 타입 (Simple ⊂ Static ⊂ Skinned)
    enum VertexType
    {
        VertexType_Simple = 0,
        VertexType_Static,
        VertexType_Skinned,
    };


    struct VertexSimple
    {
        DirectX::XMFLOAT3 Pos;
    };

    struct VertexStatic
    {
        DirectX::XMFLOAT3 Pos;
        DirectX::XMFLOAT2 UV;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT3 Tangent;
        DirectX::XMFLOAT4 Color;
    };

    struct VertexSkinned
    {
        DirectX::XMFLOAT3 Pos;
        DirectX::XMFLOAT2 UV;
        DirectX::XMFLOAT3 Normal;
        DirectX::XMFLOAT3 Tangent;
        DirectX::XMFLOAT4 Color;
        uint16_t BoneIdx[4];
        float BoneWeight[4];
    };

    // 접두 호환 잠금
    static_assert(offsetof(VertexStatic, Pos) == offsetof(VertexSimple, Pos), "VertexStatic 은 VertexSimple 을 접두로 가져야 함");
    static_assert(offsetof(VertexSkinned, Pos) == offsetof(VertexStatic, Pos)
               && offsetof(VertexSkinned, UV) == offsetof(VertexStatic, UV)
               && offsetof(VertexSkinned, Normal) == offsetof(VertexStatic, Normal)
               && offsetof(VertexSkinned, Tangent) == offsetof(VertexStatic, Tangent)
               && offsetof(VertexSkinned, Color) == offsetof(VertexStatic, Color), "VertexSkinned 는 VertexStatic 을 접두로 가져야 함");
    static_assert(offsetof(VertexSkinned, BoneIdx) == sizeof(VertexStatic), "VertexSkinned 의 추가 필드는 VertexStatic 바로 뒤여야 함");

    typedef uint32_t VIndex;

    #define SKIN_BONE_MAX 512


    struct SubMesh
    {
        uint32_t indexStart;
        uint32_t indexCount;
        uint32_t baseVertex;
    };

    typedef uint16_t SubMeshID;

    
    struct Mesh
    {
        VertexType vertexType;

        std::vector<VertexSimple> verticesSimple;
        std::vector<VertexStatic> verticesStatic;
        std::vector<VertexSkinned> verticesSkinned;

        std::vector<VIndex> indices;
        std::vector<SubMesh> subMeshes;     // 비어있으면 전체 서브메쉬 0
    };
    
    typedef uint16_t MeshID;

    #pragma endregion


    #pragma region Texture

    // 파일의 압축포맷, 확장자
    enum TextureFileFormat
    {
        TextureFileFormat_None,
        TextureFileFormat_DDS,
    };

    enum TextureType
    {
        TextureType_Default,
        TextureType_Diffuse,
        TextureType_CubeMap,
        TextureType_LUT,
        TextureType_VolumeMap,
    };

    // 텍스처의 압축포맷
    enum class TextureReadFormat
    {
        Unknown,

        // 컬러 (비압축)
        R8_UNorm,
        RG8_UNorm,
        RGBA8_UNorm,
        RGBA8_SRGB,
        BGRA8_UNorm,
        BGRA8_SRGB,
        R10G10B10A2_UNorm,
        R11G11B10_Float,
        R16_Float,
        RG16_Float,
        RGBA16_Float,
        R32_Float,
        RG32_Float,
        RGBA32_Float,

        // 블록 압축 (파일 텍스처용)
        BC1_UNorm,  BC1_SRGB,
        BC2_UNorm,  BC2_SRGB,
        BC3_UNorm,  BC3_SRGB,
        BC4_UNorm,  BC4_SNorm,
        BC5_UNorm,  BC5_SNorm,
        BC6H_UF16,  BC6H_SF16,
        BC7_UNorm,  BC7_SRGB,

        // 깊이/스텐실
        D16_UNorm,
        D24_UNorm_S8_UInt,
        D32_Float,
        D32_Float_S8X24_UInt,
    };

    struct TextureData {
        TextureReadFormat format;
        TextureType textureType;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t mipLevels = 1;
        uint32_t arraySize = 1;
    };

    struct SubresourceData {
        size_t offset;
        uint32_t rowPitch;
        uint32_t slicePitch;
    };

    enum TextureUsage
    {
        TextureUsage_ReadOnly,
        TextureUsage_RT,
        TextureUsage_DS,
    };


    struct Texture {
        TextureData data;
        std::vector<SubresourceData> subresources;
        std::vector<std::byte> rawData;
        TextureUsage usage;
    };

    typedef uint16_t TexID;

    #pragma endregion


    #pragma region Shader

    enum ShaderType
    {
        ShaderType_VS,
        ShaderType_PS,
    };

	typedef std::pair<std::wstring, ShaderType> ShaderInfo;

    typedef uint32_t ShaderID;

    #pragma endregion


    // 미구현 (컴퓨트 자원)
    #pragma region ComputeResource

    enum ComputeDimension
    {
        ComputeDimension_Buffer,
        ComputeDimension_Texture1D,
        ComputeDimension_Texture2D,
        ComputeDimension_Texture3D,
    };

    struct ComputeResource
    {
        ComputeDimension dimension = ComputeDimension_Buffer;
        TextureReadFormat format = TextureReadFormat::Unknown;
        uint32_t stride = 0;
        uint32_t width = 1;
        uint32_t height = 1;
        uint32_t depth = 1;
        uint32_t count = 0;
    };
    typedef ComputeResource CSRes;

    typedef uint16_t CSResID;

    #pragma endregion


    #pragma region Prop

    // (RTB)Action.dsOpt = ForceDepthRead 설정 시 쓰기 강제 꺼짐
    // DS 없는 출력 시 무시
    enum DrawProp_DepthUsage
    {
        DrawProp_DepthUsage_Opaque,         // 깊이 LESS / 쓰기 있음 (기본값)
        DrawProp_DepthUsage_Translucent,    // 깊이 LESS / 쓰기 없음
        DrawProp_DepthUsage_Screen,         // 깊이 테스트 / 쓰기 모두 끔
        DrawProp_DepthUsage_Background,     // 깊이 LESS_EQUAL / 쓰기 없음
        DrawProp_DepthUsage_Equal,          // 미구현: 깊이 EQUAL / 쓰기 없음
        DrawProp_DepthUsage_Always,         // 미구현: 깊이 ALWAYS / 쓰기 있음
    };

    enum DrawProp_CullType
    {
        DrawProp_CullType_Back,
        DrawProp_CullType_Front,
        DrawProp_CullType_None,
    };

    // RT 없는 출력 시 무시
    enum DrawProp_BlendType
    {
        DrawProp_BlendType_None,
        DrawProp_BlendType_Alpha,
        DrawProp_BlendType_Addtive,
        DrawProp_BlendType_Multiply,
    };

    // 미구현
    enum DrawProp_FillType
    {
        DrawProp_FillType_Solid,
        DrawProp_FillType_Wireframe,
    };

    // 미구현
    enum DrawProp_DepthBias
    {
        DrawProp_DepthBias_None,
        DrawProp_DepthBias_ShadowCaster,
        DrawProp_DepthBias_Decal,
    };

    // 미구현
    struct DrawProp_StencilOption
    {
        bool useStencil;
    };

    enum DrawProp_Topology
    {
        DrawProp_Topology_TriangleList,
        DrawProp_Topology_TriangleStrip,
        DrawProp_Topology_LineList,
        DrawProp_Topology_LineStrip,
        DrawProp_Topology_PointList,
    };

    struct DrawProp
    {
        DrawProp_DepthUsage depthUsage;
        DrawProp_CullType cullType;
        DrawProp_BlendType blendType;
        DrawProp_FillType fillType;         // 미구현
        DrawProp_DepthBias depthBias;       // 미구현
        DrawProp_StencilOption stecilOpt;
        DrawProp_Topology topology;

        VertexType vertexType = VertexType_Static;   // 기본값 Static
        ShaderID VSId;
        ShaderID PSId;
    };

    typedef uint16_t DrawPropID;


    // customizable (< 256)
    #define TEXPROP_SLOT_COUNT 8

    struct TextureProp
    {
        TexID texId[TEXPROP_SLOT_COUNT] = {0};
        uint16_t texMipLevels[TEXPROP_SLOT_COUNT] = {0};
    };
    typedef TextureProp TexProp;

    typedef uint16_t TexPropID;

    #pragma endregion

    #pragma endregion



    #pragma region Report

    enum HubReportKind
    {
        HubReport_Contract,
        HubReport_Limit,
        HubReport_Load,
        HubReport_Internal,
    };

    struct HubReport
    {
        HubReportKind kind;
        const wchar_t* msg;
    };

    constexpr size_t HUB_REPORT_MAX = 256;
    extern std::vector<HubReport> HubReportList;

    namespace Internal
    {
        void PushHubReport(HubReportKind kind, const wchar_t* msg);
    }


    enum RenderReportKind
    {
        RenderReport_Invariant,
        RenderReport_Contract,
        RenderReport_Limit,
        RenderReport_Init,
    };

    struct RenderReport
    {
        RenderReportKind kind;
        const wchar_t* msg;
    };

    extern std::vector<RenderReport> RenderReportList;

    #pragma endregion

    

    #pragma region DataBox

    struct DataBox
    {
    protected:
        DataBox() = default;
    };
    typedef DataBox DtB;

    enum DataBoxType
    {
        DataBoxType_None = 0,
        DataBoxType_DataBox_RenderCondition = 1,
        DataBoxType_DataBox_RenderLight = 2,
        DataBoxType_DataBox_RenderLightMatrix = 3,
        DataBoxType_DataBox_Render3DGeneral = 4,
        DataBoxType_DataBox_Render3DOptional = 5,
        DataBoxType_DataBox_Render2DScreen = 6,

        DataBoxType_CustomizableBegin = 7,

        DataBoxType_DtB_Condition = DataBoxType_DataBox_RenderCondition,
        DataBoxType_DtB_Light = DataBoxType_DataBox_RenderLight,
        DataBoxType_DtB_LightMatrix = DataBoxType_DataBox_RenderLightMatrix,
        DataBoxType_DtB_3DGeneral = DataBoxType_DataBox_Render3DGeneral,
        DataBoxType_DtB_3DOptional = DataBoxType_DataBox_Render3DOptional,
        DataBoxType_DtB_2DScreen = DataBoxType_DataBox_Render2DScreen,
    };

    typedef uint32_t DataBoxTID;
    typedef DataBoxTID DtBTID;


    // 렌더러용 커스텀 베이스 (DataBoxCustomizer를 이용하세요)
    struct alignas(256) DataBox_Custom : public DataBox
    {
    protected:
        DataBox_Custom() = default;
    };
    typedef DataBox_Custom DtB_Custom;


    #pragma region RenderData

    
    // 특수화된 데이터 (커스터마이즈 불가)
    #pragma region ConditionData

    struct alignas(256) DataBox_RenderCondition : DataBox
    {
        DirectX::XMMATRIX camera_View;
        DirectX::XMMATRIX camera_Proj;
        DirectX::XMMATRIX camera_ViewProj;
        DirectX::XMFLOAT3 camera_EyePos;
        float global_TotalTime;
        DirectX::XMFLOAT4 global_LightAmbient;
        DirectX::XMFLOAT4 global_LightColor;
        DirectX::XMFLOAT3 global_LightDirection;
        DirectX::XMMATRIX global_LightViewProj;
    };
    typedef DataBox_RenderCondition RenderCondition, DtB_Condition;


    struct DataBox_RenderLight : DataBox
    {
        uint32_t type;
        DirectX::XMFLOAT3 position;
        
        float range;
        DirectX::XMFLOAT3 color;
        
        DirectX::XMFLOAT3 direction;
        float spotAngle;
    };
    typedef DataBox_RenderLight DtB_Light;

    extern std::vector<DataBox_RenderLight> RenderLightList;
    extern std::vector<DataBoxID> blankedLightIdList;


    struct DataBox_RenderLightMatrix : DataBox
    {
        DirectX::XMMATRIX lightViewProj;
    };
    typedef DataBox_RenderLightMatrix DtB_LightMatrix;
    
    extern std::vector<DataBox_RenderLightMatrix> RenderLightMatrixList;
    extern std::vector<DataBoxID> blankedLightMatrixIdList;

    #pragma endregion


    // 빌트인 파이프라인이랑 쓰기 좋은 기본제공 데이터타입
    #pragma region BuiltinData

    struct alignas(256) DataBox_Render3DGeneral : DataBox_Custom
    {
        DirectX::XMMATRIX worldMat;
        DirectX::XMFLOAT4 blendColor;
        float roughness;
        float metalic;
        float _[2];
        uint32_t affectLights[8];
    };
    typedef DataBox_Render3DGeneral DtB_3DGeneral;

    struct alignas(256) DataBox_Render3DOptional : DataBox_Custom
    {
        DirectX::XMMATRIX boneTransforms[SKIN_BONE_MAX];
    };
    typedef DataBox_Render3DOptional DtB_3DOptional;


    // 스크린 쿼드 사용, 좌표는 뷰포트 비율 0~1, 원점 좌상단
    // 렌더러 스크린 드로우 경로 미구현
    struct alignas(256) DataBox_Render2DScreen : DataBox_Custom
    {
        DirectX::XMFLOAT4 rect;
        DirectX::XMFLOAT4 uvRect;
        DirectX::XMFLOAT2 pivot;        // 회전 중심 (rect 내부 비율)
        float rotation;                 // 라디안, 시계 방향
        float _;
        DirectX::XMFLOAT4 blendColor;
    };
    typedef DataBox_Render2DScreen DtB_2DScreen;

    #pragma endregion

    #pragma endregion

    #pragma endregion

}
