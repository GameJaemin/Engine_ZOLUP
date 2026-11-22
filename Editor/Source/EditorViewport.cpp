#include "EditorViewport.h"

#include "Input/InputState.h"
#include "Input/Key.h"

bool EditorViewport::UpdateCamera(const InputState& input, float deltaTime)
{
	const bool wasViewportCameraActive = isViewportCameraActive;
	hasViewportCameraPress = false;

	for (const KeyEvent& event : input.GetFrameEvents())
	{
		if (event.key != Key::MouseRight) { continue; }

		hasViewportCameraPress = event.type == KeyEventType::Pressed;
		isViewportCameraActive = false;
	}

	const bool isRightButtonDown = input.IsKeyDown(Key::MouseRight);
	hasViewportCameraPress = hasViewportCameraPress && isRightButtonDown;
	isViewportCameraActive = isViewportCameraActive && isRightButtonDown;

	cameraController.Update(camera, input, deltaTime, isViewportCameraActive);
	return wasViewportCameraActive && !isViewportCameraActive;
}

bool EditorViewport::TryBeginCameraControl(bool wasImageRightClicked)
{
	if (!hasViewportCameraPress || !wasImageRightClicked) { return false; }

	isViewportCameraActive = true;
	return true;
}

bool EditorViewport::StopCameraControl() noexcept
{
	const bool wasViewportCameraActive = isViewportCameraActive;
	hasViewportCameraPress = false;
	isViewportCameraActive = false;
	return wasViewportCameraActive;
}

void EditorViewport::ResetRenderRequestState()
{
	shouldRenderScene = false;
}

bool EditorViewport::PrepareRenderTarget(std::uint32_t width, std::uint32_t height)
{
	if (renderTarget.GetName().empty())
	{
		shouldRenderScene = renderTarget.Initialize(RenderTargetName, width, height);
	}
	else
	{
		shouldRenderScene = renderTarget.Resize(width, height);
	}

	return shouldRenderScene;
}

std::optional<SceneRenderRequest> EditorViewport::BuildRenderRequest(RenderScene& scene) const
{
	if (!shouldRenderScene) { return std::nullopt; }

	return SceneRenderRequest{ scene, camera.GetCameraViewInfo(), &renderTarget };
}

bool EditorViewport::ShutdownRenderTarget()
{
	shouldRenderScene = false;
	return renderTarget.Shutdown();
}
