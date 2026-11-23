#pragma once

#include "Core/Math/Vector3.h"
#include "DataHub_Types.h"

#include <array>
#include <optional>
#include <unordered_map>
#include <vector>

class LightComponent;
class SkyboxComponent;
class StaticMeshComponent;
struct CameraViewInfo;
struct Matrix4x4;
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

/**
 * 한 World의 렌더링 대상 컴포넌트 목록을 관리한다. 메인 스레드에서 사용하며 World가 소유한다.
 * 정적 메시, 조명 및 스카이박스 컴포넌트는 부착 시 등록하고 소멸 시 스스로 해제한다.
 */
class RenderScene
{
	// 생성과 소멸
public:
	/** 활성 DataHub에서 이 장면의 점광원과 스포트라이트 데이터를 반환한다. 반복 호출할 수 있다. */
	void ReleaseLightData();

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
	/** Sketch 전에 등록 순서상 큐브맵 준비에 처음 성공한 활성 후보를 적용한다. 후보가 없으면 단색 배경을 사용한다. */
	void PrepareBackground() const;

	// 비소유이며 등록 순서를 유지한다. 각 컴포넌트는 소멸 시 목록에서 자신을 제거한다.
	std::vector<SkyboxComponent*> skyboxComponents;

	// Hakko 프레임 준비
public:
	/**
	 * DataHub 초기화 후 호출한다. viewport는 중간 렌더 타깃 안에서 그릴 픽셀 영역이다.
	 * projectionAspectRatio는 최종 출력 대상의 너비를 높이로 나눈 값이다.
	 * 영역 크기가 양수가 아니거나, 투영 비율이 유한한 양수가 아니거나,
	 * 카메라 투영 계산에 실패하면 SkipFrame을 반환한다.
	 * shadowAtlas는 엔진이 등록한 방향광 깊이 텍스처다. 호출 전에 캐스케이드 0번을 끈 상태여야 한다.
	 * 조명이나 리소스 및 그리기 요청 준비에 실패하면 Failed를 반환한다.
	 * Ready를 반환한 프레임은 Sketch에서 준비한 요청을 소비해야 한다.
	 */
	FramePreparationResult PrepareFrame(const CameraViewInfo& cameraViewInfo, const ViewportRect& viewport, float projectionAspectRatio, const Hakko::DataHub::ShadowAtlas& shadowAtlas);

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
	 * DataHub 초기화 후 Sketch 전에 호출해 기본 레이어의 카메라 행렬과 월드 위치를 갱신한다.
	 * 행렬은 현재 Hakko 셰이더의 열 단위 저장에 맞춰 전치해 전달한다.
	 * aspectRatio는 최종 출력 대상의 너비를 높이로 나눈 값이다.
	 * 투영 계산에 실패하면 기존 값을 유지하고 false를 반환한다.
	 */
	bool PrepareCamera(const CameraViewInfo& cameraViewInfo, const Matrix4x4& viewMatrix, float aspectRatio);

	// Hakko 전역 조명 준비
	/**
	 * DataHub 초기화 후 Sketch 전에 기본 레이어의 방향광과 환경광을 갱신한다.
	 * 등록 순서상 처음 방향 조회에 성공한 활성 방향광과 첫 활성 환경광을 각각 사용한다.
	 * 사용할 조명이 없는 종류는 해당 색상의 RGB를 0으로 설정한다. 선택한 방향광의 방향을 반환한다.
	 */
	std::optional<Vector3> PrepareGlobalLight();

	/** splitEnd는 화면 카메라 공간의 최대 그림자 깊이다. 단위는 cm다. */
	bool PrepareDirectionalShadow(const CameraViewInfo& cameraViewInfo, const Vector3& lightDirection, float splitEnd, const Hakko::DataHub::ShadowAtlas& shadowAtlas);

	// Hakko 점광원 준비
	/**
	 * DataHub 초기화 후 Sketch 전에 등록 순서대로 유효한 활성 점광원을 최대 4개 연결한다.
	 * 남은 슬롯에는 빈 조명을 연결한다. 조명 데이터 등록이나 갱신 실패 시 false를 반환한다.
	 */
	bool PreparePointLights();

	// 0은 빈 조명이고 슬롯 2는 기존 스포트라이트에 사용한다.
	static constexpr std::array<uint16_t, 4> PointLightSlots{ 1, 3, 4, 5 };
	std::array<Hakko::DataHub::DataBoxID, PointLightSlots.size()> pointLightDataIds{};

	// Hakko 스포트라이트 준비
	/**
	 * DataHub 초기화 후 Sketch 전에 기본 레이어의 슬롯 2에 첫 유효한 활성 스포트라이트를 연결한다.
	 * 사용할 스포트라이트가 없으면 빈 조명을 연결한다. 조명 데이터 등록이나 갱신 실패 시 false를 반환한다.
	 */
	bool PrepareSpotLight();

	// 0은 Hakko의 빈 조명 ID다. 첫 등록 후에는 같은 ID를 갱신하며 DataHub 종료 시 정리된다.
	Hakko::DataHub::DataBoxID spotLightDataId = 0;

	// Hakko 그리기 요청 준비
	/**
	 * DataHub 초기화와 메시 준비 성공 후 Sketch 전에 호출한다. 기본 레이어의 불투명 및 투명 요청 목록을 채운다.
	 * 메시와 TransformComponent가 있는 대상의 객체 데이터를 갱신하고 첫 서브메시의 그리기 요청을 추가한다.
	 * 메시 ID는 에셋 경로로 매번 조회하며 재질의 텍스처 조합을 준비해 요청에 연결한다.
	 * 투명 요청은 메시 경계 상자 중심의 카메라 공간 Z를 near부터 far 범위의 16비트 정렬 키로 변환한다.
	 * 방향광 그림자가 활성화되면 불투명 요청을 캐스케이드 0번에도 제출한다.
	 * 준비 실패 시 불투명, 투명 및 그림자 요청 목록을 비우고 false를 반환한다.
	 * 요청 생성부터 렌더러의 요청 소비까지 메시, 텍스처와 조합 ID를 재배치하거나 DataHub를 종료하면 안 된다.
	 * 새 텍스처와 조합의 업로드 요청은 같은 프레임에 렌더러가 처리해야 한다.
	 * 경로 변환과 할당 실패 예외는 전파한다.
	 * 재질 미지정은 오류용 조합 0을 사용한다. 색상 텍스처가 없는 재질은 기본 흰색에 재질 색상 계수를 적용한다.
	 */
	bool PrepareDrawRequests(const CameraViewInfo& cameraViewInfo, const Matrix4x4& viewMatrix, bool hasDirectionalShadow);

public:
	/**
	 * 종료 시 장면 제출을 중단한 뒤 객체 데이터 ID와 뼈 행렬 데이터 ID와 메시 경계 상자 중심 캐시를 비운다. 이후 이 장면을 제출하지 않는다.
	 * 실제 객체 데이터는 DataHub에 남으며 GPU 작업 완료 뒤 DataHub 종료에서 정리한다.
	 */
	void ClearObjectDataIds();

private:
	// 컴포넌트별 객체 데이터 ID다. 키는 비소유이며 컴포넌트 제거 시 항목도 제거해야 한다.
	std::unordered_map<const StaticMeshComponent*, Hakko::DataHub::DataBoxID> objectDataIds;
	// 스킨 메시 컴포넌트별 뼈 행렬 데이터 ID다. 키는 비소유이며 컴포넌트 제거 시 항목도 제거해야 한다.
	std::unordered_map<const StaticMeshComponent*, Hakko::DataHub::DataBoxID> boneDataIds;
	// 투명 정렬에 쓰는 메시 로컬 좌표다. 등록된 메시 ID가 유효한 동안 재사용한다.
	std::unordered_map<Hakko::DataHub::MeshID, Vector3> translucentBoundsCenters;
};
