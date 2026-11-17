// 렌더러 computeRootSig 와 1:1 (DX12_RootParamCompute / D3D12Renderer_ResCtrl.cpp)
// 컴퓨트 셰이더는 registers.hlsli 대신 이걸 include (그래픽스와 t / u 칸 배치가 달라서 같이 못 씀)
#ifndef REGISTERS_COMPUTE_HLSLI
#define REGISTERS_COMPUTE_HLSLI

// =====================================================================
// b, space1 / space2 : 레이어 데이터 / 액션 데이터 (그래픽스와 같은 자리, 내용은 사용 측 정의)
// 미구현 레이어 고정칸 (b0 space0, registers.hlsli 의 ConditionCB) 은 묶여 있지만 여기 선언은 아직 없음
// =====================================================================
#define LAYER_DATA_CB   register(b0, space1)
#define ACTION_DATA_CB  register(b0, space2)

// =====================================================================
// 액션의 reads[k] / writes[k] (k = 0~3) 가 오는 자리. 타입은 사용 측이 선언
// 예) StructuredBuffer<MyData> g_In : CS_READ_BUFFER(0);     RWStructuredBuffer<MyData> g_Out : CS_WRITE_BUFFER(0);
// 버퍼는 범위 검사 없이 주소로 묶임: 버퍼 밖을 읽고 쓰지 않는 건 셰이더와 디스패치 수를 넣는 쪽 책임
// =====================================================================
#define CS_READ_BUFFER(k)   register(t##k, space0)
#define CS_WRITE_BUFFER(k)  register(u##k, space0)
#define CS_READ_TEX(k)      register(t##k, space1)
#define CS_WRITE_TEX(k)     register(u##k, space1)

// =====================================================================
// t, space2 : 선행 출력 인스턴트 (RT 0~7 = g_SR0~7, DS = g_SR8), 액션 입력 칸 그대로
// =====================================================================
Texture2D g_SR0 : register(t0, space2);
Texture2D g_SR1 : register(t1, space2);
Texture2D g_SR2 : register(t2, space2);
Texture2D g_SR3 : register(t3, space2);
Texture2D g_SR4 : register(t4, space2);
Texture2D g_SR5 : register(t5, space2);
Texture2D g_SR6 : register(t6, space2);
Texture2D g_SR7 : register(t7, space2);
Texture2D g_SR8 : register(t8, space2);

// =====================================================================
// s : 정적 샘플러 (그래픽스와 같음)
// =====================================================================
SamplerState           g_SamplerAnisoWrap   : register(s0);
SamplerState           g_SamplerLinearClamp : register(s1);
SamplerState           g_SamplerPointClamp  : register(s2);
SamplerState           g_SamplerLinearWrap  : register(s3);
SamplerState           g_SamplerPointWrap   : register(s4);
SamplerComparisonState g_SamplerShadow      : register(s5);
SamplerState           g_SamplerAnisoClamp  : register(s6);

#endif
