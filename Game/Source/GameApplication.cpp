#include "GameApplication.h"

#include "AssetManager.h"
#include "Component/AnimationComponent.h"
#include "Component/CameraComponent.h"
#include "Component/StaticMeshComponent.h"
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
#include "Prefab.h"
#include "Renderer/RenderScene.h"
#include "World.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	/**
	 * model의 배치표대로 객체들을 만들어, position(cm)에 놓은 빈 루트 객체 아래에 붙인다.
	 * 메시가 있는 노드에는 overrideMaterial을 쓰고, nullptr면 모델의 메시별 기본 재질을 쓴다. 루트 객체를 반환한다.
	 */
	GameObject* AddModel(World& world, const Model& model, const Vector3& position, const std::shared_ptr<const Material>& overrideMaterial)
	{
		// 루트는 놓을 위치를 맡고, 노드 객체는 파일에 적힌 배치를 그대로 유지한다.
		std::unique_ptr<GameObject> rootObject = std::make_unique<GameObject>(world);
		rootObject->GetComponent<TransformComponent>()->SetPosition(position);
		rootObject->GetComponent<TransformComponent>()->SetScale({3, 3, 3});
		GameObject* root = world.AddGameObject(std::move(rootObject));


		// 배치표 한 줄마다 객체를 만들어 등록한다. nodeObjects의 번호는 배치표 번호와 같다.
		std::vector<GameObject*> nodeObjects;
		nodeObjects.reserve(model.sceneNodes.size());
		for (const GLTFSceneNode& sceneNode : model.sceneNodes)
		{
			std::unique_ptr<GameObject> nodeObject = std::make_unique<GameObject>(world);
			nodeObject->GetComponent<TransformComponent>()->SetTransform(sceneNode.localTransform);

			if (sceneNode.mesh.has_value())
			{
				const std::size_t meshIndex = sceneNode.mesh.value();

				std::unique_ptr<StaticMeshComponent> mesh = std::make_unique<StaticMeshComponent>();
				mesh->SetStaticMesh(model.meshes[meshIndex]);
				mesh->SetMaterial(overrideMaterial != nullptr ? overrideMaterial : model.meshMaterials[meshIndex]);
				nodeObject->AddComponent(std::move(mesh));
			}

			nodeObjects.push_back(world.AddGameObject(std::move(nodeObject)));
		}

		// SetParent는 등록된 객체끼리만 이을 수 있어 모두 등록한 뒤에 잇는다.
		// 최상위 노드는 루트에 붙인다. 순환은 로더가 이미 걸렀다.
		for (std::size_t i = 0; i < model.sceneNodes.size(); ++i)
		{
			const std::optional<std::size_t>& parentIndex = model.sceneNodes[i].parent;
			GameObject* parent = parentIndex.has_value() ? nodeObjects[parentIndex.value()] : root;
			nodeObjects[i]->SetParent(parent);
		}

		return root;
	}

	/** 객체 계층에서 처음 만나는 animation 컴포넌트의 모델이다. 없으면 nullptr이다. */
	const Model* FindAnimationModel(const PrefabObject& object)
	{
		if (object.animation.has_value()) { return object.animation->model.get(); }

		for (const PrefabObject& child : object.children)
		{
			if (const Model* model = FindAnimationModel(child)) { return model; }
		}
		return nullptr;
	}

	/** 맵에서 objectName인 GameObject를 찾아 prefabPath 프리팹의 첫 클립을 반복 재생한다. 찾지 못하면 경고만 남긴다. */
	void PlayFirstClip(const std::vector<GameObject*>& levelObjects, std::string_view objectName, std::string_view prefabPath)
	{
		// 맵을 읽을 때 같은 경로로 불러 캐시에 있으므로, 노드 목록을 만든 것과 같은 Model을 돌려받는다.
		const std::shared_ptr<const Prefab> prefab = AssetManager::Get().LoadPrefab(prefabPath);

		// 기존 형식은 최상위 model, 새 형식은 animation 컴포넌트의 model이 클립을 가진다.
		const Model* model = nullptr;
		if (prefab != nullptr)
		{
			model = prefab->model != nullptr ? prefab->model.get() : FindAnimationModel(prefab->root);
		}
		if (model == nullptr || model->animations.empty())
		{
			Log::Write(LogLevel::Warning, "Animation", std::format("{} has no animation clip", prefabPath));
			return;
		}

		for (GameObject* object : levelObjects)
		{
			if (object->GetName() != objectName) { continue; }

			AnimationComponent* animation = object->GetComponent<AnimationComponent>();
			if (animation == nullptr) { break; }
			animation->Play(model->animations[0], true);
			return;
		}
		Log::Write(LogLevel::Warning, "Animation", std::format("{} with AnimationComponent was not found in the level", objectName));
	}

	bool ActivateMapCamera(World& world, const std::vector<GameObject*>& levelObjects, std::string_view name)
	{
		CameraComponent* selectedCamera = nullptr;
		for (GameObject* object : levelObjects)
		{
			if (object->GetName() != name) { continue; }

			for (const std::unique_ptr<Component>& component : object->GetComponents())
			{
				CameraComponent* camera = dynamic_cast<CameraComponent*>(component.get());
				if (camera == nullptr) { continue; }
				if (selectedCamera != nullptr)
				{
					Log::Write(LogLevel::Error, "Camera", std::format("Multiple map cameras match {}", name));
					return false;
				}
				selectedCamera = camera;
			}
		}

		if (selectedCamera == nullptr)
		{
			Log::Write(LogLevel::Error, "Camera", std::format("Map camera {} was not found", name));
			return false;
		}
		if (!world.SetActiveCamera(selectedCamera))
		{
			Log::Write(LogLevel::Error, "Camera", std::format("Could not activate map camera {}", name));
			return false;
		}

		Log::Write(LogLevel::Info, "Camera", std::format("Active map camera: {}", name));
		return true;
	}
}

GameApplication::GameApplication(std::string_view initialMapCameraName)
	: mapCameraName(initialMapCameraName)
{
}

GameApplication::~GameApplication() = default;

bool GameApplication::OnInitialize(Engine& inEngine)
{
	engine = &inEngine;

	gameWorld = std::make_unique<World>();
	World& world = *gameWorld;
	std::filesystem::path levelPath;
	//if (!Paths::GetAbsolutePath("Game/Content/Maps/BistroMap.json", levelPath))
	if (!Paths::GetAbsolutePath("Game/Content/Maps/DevMap.json", levelPath))
	{
		Log::Write(LogLevel::Error, "Level", "Could not resolve Game/Content/Maps/DevMap.json from the repository root");
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
	PlayFirstClip(*loadedObjects, "AnimatedBox", "Game/Content/Prefabs/BoxAnimated.prefab.json");
	PlayFirstClip(*loadedObjects, "RiggedSimple", "Game/Content/Prefabs/RiggedSimple.prefab.json");

	if (!mapCameraName.empty()) { return ActivateMapCamera(world, *loadedObjects, mapCameraName); }

	GameObject* player = world.AddGameObject(std::make_unique<PlayerObject>(world));
	// World에 등록된 객체의 카메라만 지정할 수 있으므로 AddGameObject 뒤에 지정한다.
	if (player != nullptr) { world.SetActiveCamera(player->GetComponent<CameraComponent>()); }

	engine->SetCursorConfined(true);
	engine->SetCursorVisible(false);
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
