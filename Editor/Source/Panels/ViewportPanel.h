#pragma once

#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"

#include <optional>
#include <string>

class EditorSelection;
class EditorViewport;

enum class ViewportGizmoMode
{
	Move,
	Rotate,
	Scale,
};

struct ViewportPickClick
{
	// 장면 이미지의 왼쪽 위를 (0, 0)으로 하는 0 이상 1 미만의 클릭 좌표다.
	Vector2 imageUV;
	float aspectRatio;
};

class ViewportPanel
{
public:
	ViewportPanel() = default;
	ViewportPanel(const ViewportPanel&) = delete;
	ViewportPanel& operator=(const ViewportPanel&) = delete;
	ViewportPanel(ViewportPanel&&) = delete;
	ViewportPanel& operator=(ViewportPanel&&) = delete;

public:
	/**
	 * DataHub와 ImGui가 준비된 상태에서 ImGuizmo::BeginFrame 이후, ImGui::Render 전에 호출한다.
	 * 같은 프레임에 viewport.ResetRenderRequestState()를 먼저 호출한다.
	 * 생성 및 크기 변경 요청은 같은 프레임의 Sketch에서 소비해야 한다.
	 * 선택된 객체의 원점에 Move와 Rotate는 월드 축, Scale은 로컬 축 기즈모를 표시한다.
	 * Move와 Rotate의 드래그 결과는 각각 부모 기준 위치와 회전으로 저장한다.
	 * Scale 드래그는 시작 시 로컬 크기에 축별 배율을 곱해 저장한다.
	 * 카메라 조작 중이거나 현재 모드에 필요한 부모 역행렬을 구할 수 없으면 기즈모 입력을 비활성화한다.
	 * 이미지에서 프리팹 드롭이 확정되면 저장소 기준 UTF-8 상대 경로를 반환한다.
	 */
	[[nodiscard]] std::optional<std::string> Draw(EditorViewport& viewport, EditorSelection& selection);

	/** 바로 앞 Draw에서 장면 이미지 위의 우클릭 시작을 감지했고 기즈모를 조작 중이 아니면 true다. */
	bool WasImageRightClicked() const { return wasImageRightClicked; }

	/** 바로 앞 Draw의 좌클릭 결과다. 기즈모, 카메라 또는 드롭 입력에 사용한 클릭은 비워 둔다. */
	std::optional<ViewportPickClick> GetPickClick() const { return pickClick; }

public:
	// 기본 도킹 배치와 창 생성에서 같은 이름을 사용한다.
	static constexpr const char* WindowName = "Viewport";

private:
	ViewportGizmoMode gizmoMode = ViewportGizmoMode::Move;
	// 현재 Scale 드래그의 기준값이다. 다음 드래그를 시작할 때 다시 읽는다.
	Vector3 scaleAtDragStart{ 1.0f, 1.0f, 1.0f };
	bool wasImageRightClicked = false;
	std::optional<ViewportPickClick> pickClick;
};
