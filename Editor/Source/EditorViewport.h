#pragma once

#include "EditorCamera.h"
#include "EditorCameraController.h"
#include "Renderer/SceneRenderRequest.h"
#include "Renderer/TextureRenderTarget2D.h"

#include <cstdint>
#include <optional>

class InputState;
class RenderScene;

class EditorViewport
{
public:
	/** 입력과 카메라를 갱신하고, 이번 갱신에서 조작이 끝났으면 true를 반환한다. deltaTime은 초 단위다. */
	bool UpdateCamera(const InputState& input, float deltaTime);
	/** UpdateCamera 이후 같은 프레임의 이미지 우클릭을 전달한다. 조작이 시작되면 true를 반환한다. */
	bool TryBeginCameraControl(bool wasImageRightClicked);
	/** 조작 상태를 초기화하고, 조작 중이었다면 true를 반환한다. */
	bool StopCameraControl() noexcept;
	/** 장면 이미지에서 시작한 우클릭 카메라 조작이 계속되고 있으면 true다. */
	bool IsCameraControlActive() const { return isViewportCameraActive; }
	/** 장면 렌더 요청에 쓰는 카메라 시점의 복사본을 반환한다. */
	CameraViewInfo GetCameraViewInfo() const { return camera.GetCameraViewInfo(); }

private:
	EditorCamera camera;
	EditorCameraController cameraController;

	// 이번 입력 갱신에서 받은 우클릭 시작을 같은 프레임의 이미지 클릭과 대조한다.
	bool hasViewportCameraPress = false;
	// 장면 이미지 위에서 오른쪽 버튼을 누른 뒤 뗄 때까지 카메라 입력을 허용한다.
	bool isViewportCameraActive = false;

public:
	/** 패널을 그리기 전에 호출해 이번 프레임의 렌더 요청 여부를 초기화한다. */
	void ResetRenderRequestState();
	/** DataHub 초기화 후 양수인 픽셀 크기로 호출한다. true는 준비 요청 접수를 뜻한다. */
	bool PrepareRenderTarget(std::uint32_t width, std::uint32_t height);
	/** 뷰포트 수명 동안 빌린다. 초기화 전에도 반환한다. */
	const TextureRenderTarget2D& GetRenderTarget() const { return renderTarget; }
	/** 이번 프레임의 타깃 준비 요청이 접수된 경우에만 장면과 타깃을 빌려 요청을 만든다. */
	std::optional<SceneRenderRequest> BuildRenderRequest(RenderScene& scene) const;
	/** DataHub 종료 전에 호출한다. 해제 요청이 실패하면 타깃 상태를 유지한다. */
	bool ShutdownRenderTarget();

private:
	// 현재 단일 에디터 뷰포트가 사용하는 등록 이름이다.
	static constexpr const wchar_t* RenderTargetName = L"Editor.SceneViewport";

	TextureRenderTarget2D renderTarget;
	bool shouldRenderScene = false;
};
