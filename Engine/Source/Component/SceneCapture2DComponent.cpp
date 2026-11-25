#include "SceneCapture2DComponent.h"

#include "CameraComponent.h"
#include "CameraViewInfo.h"
#include "Core/Log.h"
#include "GameObject.h"
#include "Renderer/RenderScene.h"
#include "World.h"

#include <format>
#include <string>
#include <utility>

SceneCapture2DComponent::~SceneCapture2DComponent()
{
	// 타깃을 해제하기 전에 목록에서 빼서 해제 중인 타깃으로 요청을 만들지 않게 한다.
	if (registeredRenderScene != nullptr)
	{
		registeredRenderScene->RemoveSceneCaptureComponent(this);
	}

	if (!renderTarget.Shutdown())
	{
		Log::Write(LogLevel::Error, "SceneCapture", "Failed to request render target release");
	}
}

void SceneCapture2DComponent::OnRegister()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr) { return; }

	RenderScene& scene = owner->GetWorld().GetRenderScene();
	scene.AddSceneCaptureComponent(this);
	registeredRenderScene = &scene;

	// Edit World와 Play World가 같은 ObjectID를 쓰므로 저장하지 않는 실행 중 번호로 이름을 만든다.
	// 해제 직후 같은 이름의 재등록은 보장되지 않으므로 번호는 다시 쓰지 않는다.
	static std::uint32_t nextTargetNumber = 1;
	std::wstring targetName = std::format(L"SceneCapture2D.{}", nextTargetNumber++);
	if (!renderTarget.Initialize(std::move(targetName), width, height))
	{
		Log::Write(LogLevel::Error, "SceneCapture", "Failed to request render target creation");
	}
}

bool SceneCapture2DComponent::SetWidth(std::uint32_t newWidth)
{
	if (newWidth == 0 || newWidth > MaxResolution) { return false; }
	if (!renderTarget.GetName().empty() && !renderTarget.Resize(newWidth, height)) { return false; }

	width = newWidth;
	return true;
}

bool SceneCapture2DComponent::SetHeight(std::uint32_t newHeight)
{
	if (newHeight == 0 || newHeight > MaxResolution) { return false; }
	if (!renderTarget.GetName().empty() && !renderTarget.Resize(width, newHeight)) { return false; }

	height = newHeight;
	return true;
}

bool SceneCapture2DComponent::GetCaptureViewInfo(CameraViewInfo& outCameraViewInfo) const
{
	const GameObject* owner = GetGameObject();
	if (owner == nullptr) { return false; }

	const CameraComponent* camera = owner->GetComponent<CameraComponent>();
	if (camera == nullptr) { return false; }

	return camera->GetCameraViewInfo(outCameraViewInfo);
}

std::optional<SceneRenderRequest> SceneCapture2DComponent::BuildRenderRequest()
{
	GameObject* owner = GetGameObject();
	if (owner == nullptr || owner->IsPendingDestroy()) { return std::nullopt; }
	if (renderTarget.GetName().empty()) { return std::nullopt; }

	CameraViewInfo cameraViewInfo;
	if (!GetCaptureViewInfo(cameraViewInfo)) { return std::nullopt; }

	return SceneRenderRequest{ owner->GetWorld().GetRenderScene(), cameraViewInfo, &renderTarget };
}
