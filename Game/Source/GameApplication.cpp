#include "GameApplication.h"

#include "AssetManager.h"
#include "Component/AmbientLightComponent.h"
#include "Component/CameraComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "Core/Math/EulerAngles.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Vector3.h"
#include "GameObject.h"
#include "Input/InputDispatcher.h"
#include "LevelLoader.h"
#include "Paths.h"
#include "PlayerObject.h"
#include "Renderer/RenderScene.h"
#include "World.h"

#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

namespace
{
	/** 위에서 아래로 비스듬히 빛을 보내는 방향광을 World에 추가한다. */
	void AddDirectionalLight(World& world)
	{
		std::unique_ptr<GameObject> lightObject = std::make_unique<GameObject>(world);

		TransformComponent* transform = lightObject->GetComponent<TransformComponent>();
		transform->SetRotation(EulerAngles{ .pitch = 45.0f, .yaw = -80.0f, .roll = 0.0f });

		std::unique_ptr<DirectionalLightComponent> light = std::make_unique<DirectionalLightComponent>();
		light->SetColor(Vector3{ 1.0f, 1.0f, 1.0f });
		light->SetIntensity(1.0f);

		lightObject->AddComponent(std::move(light));
		world.AddGameObject(std::move(lightObject));
	}

	/** DevMap 주전자 앞에 주황색 점광원을 배치한다. */
	void AddPointLight(World& world)
	{
		std::unique_ptr<GameObject> lightObject = std::make_unique<GameObject>(world);
		TransformComponent* transform = lightObject->GetComponent<TransformComponent>();
		transform->SetPosition({ 940.0f, 580.0f, 2880.0f });

		std::unique_ptr<PointLightComponent> light = std::make_unique<PointLightComponent>();
		light->SetColor({ 1.0f, 0.35f, 0.1f });
		light->SetIntensity(2.0f);
		light->SetRange(300.0f);

		lightObject->AddComponent(std::move(light));
		world.AddGameObject(std::move(lightObject));
	}

	/** DevMap에 약한 흰색 환경광을 추가한다. */
	void AddAmbientLight(World& world)
	{
		std::unique_ptr<GameObject> lightObject = std::make_unique<GameObject>(world);
		std::unique_ptr<AmbientLightComponent> light = std::make_unique<AmbientLightComponent>();
		light->SetIntensity(0.1f);

		lightObject->AddComponent(std::move(light));
		world.AddGameObject(std::move(lightObject));
	}

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
}

GameApplication::GameApplication() = default;

GameApplication::~GameApplication() = default;

bool GameApplication::OnInitialize()
{
	gameWorld = std::make_unique<World>();
	World& world = *gameWorld;
	std::filesystem::path levelPath;
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

	GameObject* player = world.AddGameObject(std::make_unique<PlayerObject>(world));
	// World에 등록된 객체의 카메라만 지정할 수 있으므로 AddGameObject 뒤에 지정한다.
	if (player != nullptr) { world.SetActiveCamera(player->GetComponent<CameraComponent>()); }

	AddDirectionalLight(world);
	AddPointLight(world);
	AddAmbientLight(world);

	return true;
}

void GameApplication::OnUpdate(const InputState& input, float deltaTime)
{
	// 입력 바인딩을 먼저 실행한 뒤 월드를 갱신한다.
	gameWorld->GetInputDispatcher().Dispatch(input, deltaTime);
	gameWorld->Tick(deltaTime);
}

std::optional<SceneView> GameApplication::GetSceneView()
{
	const CameraComponent* camera = gameWorld->GetActiveCamera();
	if (camera == nullptr) { return std::nullopt; }

	CameraViewInfo cameraViewInfo;
	if (!camera->GetCameraViewInfo(cameraViewInfo)) { return std::nullopt; }

	return SceneView{ gameWorld->GetRenderScene(), cameraViewInfo };
}

void GameApplication::OnShutdown() noexcept
{
	if (gameWorld)
	{
		// 종료 시 DataHub가 데이터를 일괄 해제한다. 소멸자에서 ID를 반환하며 할당할 가능성을 없앤다.
		gameWorld->GetRenderScene().ClearObjectDataIds();
	}
	gameWorld.reset();
}
