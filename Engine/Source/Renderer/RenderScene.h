#pragma once

#include "Core/Math/Vector3.h"
#include "DataHub_RTB.h"
#include "DataHub_Types.h"

#include <DirectXCollision.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

class LightComponent;
class SceneCapture2DComponent;
class SkyboxComponent;
class StaticMeshComponent;
struct CameraViewInfo;
struct Matrix4x4;
struct SceneRenderRequest;
struct ViewportRect;

namespace Hakko::DataHub
{
	struct ShadowAtlas;
}

enum class FramePreparationResult
{
	Ready,
	SkipFrame,
	Failed
};

struct RenderSceneLayerLightData
{
	// 슬롯 0은 빈 조명이다. 인덱스 i의 ID와 조명은 슬롯 i + 1에 대응한다.
	std::array<Hakko::DataHub::DataBoxID, RENDER_LIGHT_MAX - 1> ids{};
	std::vector<Hakko::DataHub::DataBox_RenderLight> lights;
};

/**
 * 한 World의 렌더링 대상 컴포넌트 목록을 관리한다. 메인 스레드에서 사용하며 World가 소유한다.
 * 정적 메시, 조명 및 스카이박스 컴포넌트는 부착 시 등록하고 소멸 시 스스로 해제한다.
 */
class RenderScene
{
	// 생성과 소멸
public:
	RenderScene() = default;

	RenderScene(const RenderScene&) = delete;
	RenderScene& operator=(const RenderScene&) = delete;
	RenderScene(RenderScene&&) = delete;
	RenderScene& operator=(RenderScene&&) = delete;

	// 정적 메시
public:
	/**
	 * 렌더링 대상으로 추가한다. nullptr이거나 이미 추가한 대상은 무시한다.
	 * 대상은 소멸 전에 RemoveStaticMeshComponent로 제거되어야 한다. 할당 실패 예외는 전파한다.
	 */
	void AddStaticMeshComponent(StaticMeshComponent* component);

	/** 렌더링 대상에서 제거하고 등록한 객체 데이터 ID를 반납한다. 추가하지 않은 대상은 무시한다. */
	void RemoveStaticMeshComponent(StaticMeshComponent* component);

private:
	// 비소유이며 추가한 순서를 유지한다.
	std::vector<StaticMeshComponent*> staticMeshComponents;

	// 조명
public:
	/**
	 * nullptr와 중복 등록은 무시한다. 할당 실패 예외는 전파한다.
	 * 대상은 소멸 시 RemoveLightComponent로 제거되어야 한다.
	 */
	void AddLightComponent(LightComponent* component);

	/** 등록되지 않은 대상은 무시한다. */
	void RemoveLightComponent(LightComponent* component);

private:
	// 비소유이며 등록 순서를 유지한다.
	std::vector<LightComponent*> lightComponents;

	// 배경
public:
	/** 배경 후보로 추가한다. nullptr와 중복 등록은 무시한다. 할당 실패 예외는 전파한다. */
	void AddSkyboxComponent(SkyboxComponent* component);

	/** 배경 후보에서 제거한다. 등록되지 않은 대상은 무시한다. */
	void RemoveSkyboxComponent(SkyboxComponent* component);

private:
	/** Sketch 전에 큐브맵 준비에 처음 성공한 활성 후보를 등록 순서대로 찾아 지정한 레이어에 적용한다. 후보가 없으면 단색 배경을 사용한다. */
	void PrepareBackground(std::uint8_t layer) const;

	// 비소유이며 등록 순서를 유지한다. 각 컴포넌트는 소멸 시 목록에서 자신을 제거한다.
	std::vector<SkyboxComponent*> skyboxComponents;

	// 캡처
public:
	/** 캡처 후보로 추가한다. nullptr와 중복 등록은 무시한다. 할당 실패 예외는 전파한다. */
	void AddSceneCaptureComponent(SceneCapture2DComponent* component);

	/** 캡처 후보에서 제거한다. 등록되지 않은 대상은 무시한다. */
	void RemoveSceneCaptureComponent(SceneCapture2DComponent* component);

	/** 준비된 캡처의 요청을 등록 순서대로 outRequests에 추가한다. 요청은 같은 프레임에 소비한다. */
	void CollectCaptureRequests(std::vector<SceneRenderRequest>& outRequests);

private:
	// 비소유이며 등록 순서를 유지한다. 각 컴포넌트는 소멸 시 목록에서 자신을 제거한다.
	std::vector<SceneCapture2DComponent*> sceneCaptureComponents;

	// Hakko 프레임 준비
public:
	/**
	 * DataHub 초기화 후 지정한 레이어의 장면을 준비한다. viewport는 중간 렌더 타깃 안에서 그릴 픽셀 영역이다.
	 * projectionAspectRatio는 최종 출력 대상의 너비를 높이로 나눈 값이다.
	 * shadowAtlas는 레이어 0에서 사용하는 등록된 방향광 깊이 텍스처다. 레이어 0을 준비하기 전에 캐스케이드 0을 꺼 둔다.
	 * 영역 크기가 양수가 아니거나, 투영 비율이 유한한 양수가 아니거나,
	 * 카메라 투영 계산에 실패하면 SkipFrame을 반환한다.
	 * 레이어가 등록되지 않았거나 조명, 리소스 또는 그리기 요청 준비에 실패하면 Failed를 반환한다.
	 * Ready를 반환한 프레임은 Sketch에서 준비한 요청을 소비해야 한다.
	 */
	FramePreparationResult PrepareFrame(const CameraViewInfo& cameraViewInfo, const ViewportRect& viewport, float projectionAspectRatio, const Hakko::DataHub::ShadowAtlas& shadowAtlas, std::uint8_t layer);

	// Hakko 메시 등록
private:
	/**
	 * DataHub 초기화 후 호출한다. 등록된 컴포넌트의 메시를 Hakko에 등록하고 이미 등록한 메시는 재사용한다.
	 * 빈 메시 참조는 건너뛰며 에셋 경로가 비어 있거나 등록에 실패하면 false를 반환한다.
	 * 같은 에셋 경로는 같은 메시로 취급하며 이름 조회로 등록 여부를 판단한다.
	 * 실패 전에 등록한 메시는 유지한다. 경로 변환과 할당 실패 예외는 전파한다.
	 */
	bool PrepareStaticMeshes();

	// Hakko 카메라 준비
	/**
	 * DataHub 초기화 후 Sketch 전에 호출해 지정한 레이어의 카메라 행렬과 월드 위치를 갱신한다.
	 * 행렬은 현재 Hakko 셰이더의 열 단위 저장에 맞춰 전치해 전달한다.
	 * aspectRatio는 최종 출력 대상의 너비를 높이로 나눈 값이다.
	 * 투영 계산에 실패하면 기존 값을 유지하고 false를 반환한다.
	 */
	bool PrepareCamera(const CameraViewInfo& cameraViewInfo, const Matrix4x4& viewMatrix, float aspectRatio, std::uint8_t layer);

	// Hakko 전역 조명 준비
	/**
	 * DataHub 초기화 후 Sketch 전에 지정한 레이어의 방향광과 환경광을 갱신한다.
	 * 등록 순서상 처음 방향 조회에 성공한 활성 방향광과 첫 활성 환경광을 각각 사용한다.
	 * 사용할 조명이 없는 종류는 해당 색상의 RGB를 0으로 설정한다. 선택한 방향광이 있으면 그 방향을 반환한다.
	 */
	std::optional<Vector3> PrepareGlobalLight(std::uint8_t layer);

	/** 선택한 방향광으로 기본 레이어의 캐스케이드 0을 준비한다. splitEnd는 화면 카메라 공간의 깊이 끝이며 단위는 cm다. */
	bool PrepareDirectionalShadow(const CameraViewInfo& cameraViewInfo, const Vector3& lightDirection, float splitEnd, const Hakko::DataHub::ShadowAtlas& shadowAtlas);

	/**
	 * Point와 Spot을 지정한 레이어의 1번부터 63번까지 공유 슬롯에 연결한다.
	 * 후보가 슬롯보다 많으면 카메라 시야에 영향을 줄 수 있는 조명과 예상 기여도가 높은 조명을 우선한다.
	 * 남는 슬롯은 비운다. 조명 데이터 등록이나 갱신 실패 시 false를 반환한다.
	 */
	bool PrepareLocalLights(const CameraViewInfo& cameraViewInfo, const Matrix4x4& viewMatrix, float aspectRatio, std::uint8_t layer);

private:
	// 모든 레이어의 요청을 준비한 뒤 Sketch가 읽으므로 레이어마다 등록 ID를 따로 보관한다.
	std::array<RenderSceneLayerLightData, RENDER_LAYER_MAX> localLightsByLayer;

private:
	// Hakko 그리기 요청 준비
	/**
	 * DataHub 초기화와 메시 준비 성공 후 Sketch 전에 호출한다. 등록된 레이어의 불투명 및 투명 요청 목록을 채운다.
	 * 메시와 TransformComponent가 있는 대상의 객체 데이터를 갱신하고 첫 서브메시의 그리기 요청을 추가한다.
	 * 메시 ID는 에셋 경로로 매번 조회하며 재질의 텍스처 조합을 준비해 요청에 연결한다.
	 * 투명 및 포워드 불투명 요청은 경계 상자에 대한 기여도가 큰 로컬 조명을 최대 8개 사용한다.
	 * 투명 요청은 메시 경계 상자 중심의 카메라 공간 Z를 near부터 far 범위의 16비트 정렬 키로 변환한다.
	 * 지정한 레이어에 방향광 그림자가 활성화되면 불투명 요청을 그 레이어의 캐스케이드 0에도 제출한다.
	 * 준비 실패 시 지정한 레이어의 불투명, 투명 및 그림자 요청 목록을 비우고 false를 반환한다.
	 * 요청 생성부터 렌더러의 요청 소비까지 메시, 텍스처와 조합 ID를 재배치하거나 DataHub를 종료하면 안 된다.
	 * 새 텍스처와 조합의 업로드 요청은 같은 프레임에 렌더러가 처리해야 한다.
	 * 경로 변환과 할당 실패 예외는 전파한다.
	 * 재질 미지정은 오류용 조합 0을 사용한다. 색상 텍스처가 없는 재질은 기본 흰색에 재질 색상 계수를 적용한다.
	 */
	bool PrepareDrawRequests(const CameraViewInfo& cameraViewInfo, const Matrix4x4& viewMatrix, bool hasDirectionalShadow, std::uint8_t layer);

public:
	/** 이 장면을 교체할 때 활성 DataHub의 로컬 조명 슬롯을 비우고 재사용하던 ID를 반환한다. */
	void ReleaseLightData();

	/**
	 * 종료 시 장면 제출을 중단한 뒤 객체 및 뼈 행렬 데이터 ID와 메시 경계 상자 캐시를 비운다. 이후 이 장면을 제출하지 않는다.
	 * 실제 객체 데이터는 DataHub에 남으며 GPU 작업 완료 뒤 DataHub 종료에서 정리한다.
	 */
	void ClearObjectDataIds();

private:
	// 각 레이어의 객체 데이터에 그 레이어의 조명 슬롯을 저장한다. 컴포넌트 키는 비소유다.
	std::array<std::unordered_map<const StaticMeshComponent*, Hakko::DataHub::DataBoxID>, RENDER_LAYER_MAX> objectDataIdsByLayer;
	// 스킨 메시 컴포넌트별 뼈 행렬 데이터 ID다. 키는 비소유이며 컴포넌트 제거 시 항목도 제거해야 한다.
	std::unordered_map<const StaticMeshComponent*, Hakko::DataHub::DataBoxID> boneDataIds;
	// 조명 선택 및 투명 정렬에 쓰는 메시 로컬 경계 상자다. 스킨 메시는 원본 정점 기준이다.
	std::unordered_map<Hakko::DataHub::MeshID, DirectX::BoundingBox> meshBounds;
};
