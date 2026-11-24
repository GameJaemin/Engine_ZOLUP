#include "GameApplication.h"

#include "Component/CameraComponent.h"
#include "Component/GravityComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Vector3.h"
#include "Engine.h"
#include "GameObject.h"
#include "Input/InputDispatcher.h"
#include "Input/InputState.h"
#include "LevelLoader.h"
#include "Paths.h"
#include "PlayerObject.h"
#include "Renderer/RenderScene.h"
#include "World.h"

#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	const CameraComponent* FindMapCamera(const std::vector<GameObject*>& levelObjects, std::string_view name)
	{
		const CameraComponent* selectedCamera = nullptr;
		for (const GameObject* object : levelObjects)
		{
			if (object->GetName() != name) { continue; }

			for (const std::unique_ptr<Component>& component : object->GetComponents())
			{
				const CameraComponent* camera = dynamic_cast<const CameraComponent*>(component.get());
				if (camera == nullptr) { continue; }
				if (selectedCamera != nullptr)
				{
					Log::Write(LogLevel::Error, "Camera", std::format("Multiple map cameras match {}", name));
					return nullptr;
				}
				selectedCamera = camera;
			}
		}

		if (selectedCamera == nullptr)
		{
			Log::Write(LogLevel::Error, "Camera", std::format("Map camera {} was not found", name));
		}

		return selectedCamera;
	}
}

GameApplication::GameApplication() = default;

GameApplication::~GameApplication() = default;

bool GameApplication::OnInitialize(Engine& inEngine)
{
	engine = &inEngine;

	gameWorld = std::make_unique<World>();
	World& world = *gameWorld;
	constexpr std::string_view startupLevelPath = "Game/Content/Maps/BistroMap.json";
	constexpr std::string_view spawnCameraName = "BistroCamera";
	std::filesystem::path levelPath;
	if (!Paths::GetAbsolutePath(startupLevelPath, levelPath))
	{
		Log::Write(LogLevel::Error, "Level", std::format("Could not resolve {} from the repository root", startupLevelPath));
		return false;
	}

	const std::optional<std::vector<GameObject*>> loadedObjects = LoadLevel(levelPath, world);
	if (!loadedObjects.has_value()) { return false; }

	// 맵의 빈 객체는 화면에 보이지 않으므로 복원된 값을 콘솔에서 확인한다.
	Log::Write(LogLevel::Info, "Level", std::format("Loaded {} objects", loadedObjects->size()));
	for (const GameObject* object : *loadedObjects)
	{
		const Transform transform = object->GetComponent<TransformComponent>()->GetTransform();
		const Vector3& position = transform.position;
		const Quaternion& rotation = transform.rotation;
		const Vector3& scale = transform.scale;
		Log::Write(LogLevel::Info, "Level", std::format("{}: position(cm) [{}, {}, {}], rotation [{}, {}, {}, {}], scale [{}, {}, {}]", object->GetName(), position.x, position.y, position.z, rotation.x, rotation.y, rotation.z, rotation.w, scale.x, scale.y, scale.z));
	}
	const CameraComponent* spawnCamera = FindMapCamera(*loadedObjects, spawnCameraName);
	if (spawnCamera == nullptr) { return false; }

	const GameObject* mapCameraOwner = spawnCamera->GetGameObject();
	const Vector3 spawnPosition = mapCameraOwner->GetComponent<TransformComponent>()->GetWorldTransform().position;
	std::unique_ptr<PlayerObject> player = std::make_unique<PlayerObject>(world);
	player->GetComponent<TransformComponent>()->SetPosition(spawnPosition);
	player->AddComponent(std::make_unique<GravityComponent>());
	GameObject* registeredPlayer = world.AddGameObject(std::move(player));
	if (registeredPlayer == nullptr || !world.SetActiveCamera(registeredPlayer->GetComponent<CameraComponent>()))
	{
		Log::Write(LogLevel::Error, "Camera", "Could not activate the player camera");
		return false;
	}

	Log::Write(LogLevel::Info, "Camera", "Active camera: player");
	return true;
}

void GameApplication::OnUpdate(const InputState& input, float deltaTime)
{
	// 키와 마우스 입력을 먼저 전달해 같은 프레임의 월드 갱신에서 사용한다.
	gameWorld->GetInputDispatcher().Dispatch(input, deltaTime);
	if (CameraComponent* camera = gameWorld->GetActiveCamera())
	{
		if (PlayerObject* player = dynamic_cast<PlayerObject*>(camera->GetGameObject()))
		{
			const RawMouseDelta mouseDelta = input.GetRawMouseDelta();
			player->AddMouseLookInput(mouseDelta.x, mouseDelta.y);
		}
	}
	gameWorld->Tick(deltaTime);
	gameWorld->DestroyPendingGameObjects();
}

std::optional<SceneRenderRequest> GameApplication::GetRenderRequest()
{
	const CameraComponent* camera = gameWorld->GetActiveCamera();
	if (camera == nullptr) { return std::nullopt; }

	CameraViewInfo cameraViewInfo;
	if (!camera->GetCameraViewInfo(cameraViewInfo)) { return std::nullopt; }

	return SceneRenderRequest{ gameWorld->GetRenderScene(), cameraViewInfo, nullptr };
}

void GameApplication::OnShutdown() noexcept
{
	if (gameWorld)
	{
		// 종료 시 DataHub가 데이터를 일괄 해제한다. 소멸자에서 ID를 반환하며 할당할 가능성을 없앤다.
		gameWorld->GetRenderScene().ClearObjectDataIds();
	}
	gameWorld.reset();

	if (engine != nullptr)
	{
		engine->SetCursorVisible(true);
		engine->SetCursorConfined(false);
	}

	engine = nullptr;
}
