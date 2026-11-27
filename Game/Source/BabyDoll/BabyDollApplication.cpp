#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "BabyDollApplication.h"

#include "HeartComponent.h"
#include "HearingComponent.h"
#include "OrbitCameraComponent.h"
#include "PlayerMoveComponent.h"
#include "PoseAnimatorComponent.h"
#include "StitchBrainComponent.h"
#include "RearCameraComponent.h"
#include "ProximityLightComponent.h"
#include "CaughtCameraComponent.h"

#include "AssetManager.h"
#include "Component/AmbientLightComponent.h"
#include "Component/AnimationComponent.h"
#include "Component/CameraComponent.h"
#include "Component/BoxColliderComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/SkinComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "Engine.h"
#include "GameObject.h"
#include "Input/InputState.h"
#include "Input/Key.h"
#include "Mesh/StaticMesh.h"
#include "Prefab/Prefab.h"
#include "Renderer/Material.h"
#include "Renderer/RenderScene.h"
#include "World.h"

#include <imgui.h>

#include <cstdio>
#include <limits>

namespace
{
	constexpr Vector3 PlayerSpawn{ 0.0f, 0.0f, -1100.0f };
	constexpr Vector3 MonsterSpawn{ 0.0f, 0.0f, 850.0f };
	constexpr float FixedStep = 1.0f / 120.0f;

	template <typename T>
	T* Attach(GameObject& object)
	{
		std::unique_ptr<T> component = std::make_unique<T>();
		T* result = component.get();
		object.AddComponent(std::move(component));
		return result;
	}

	Vector3 ModelPoint(const Vector3& point, const Matrix4x4& matrix)
	{
		return { point.x * matrix.m[0][0] + point.y * matrix.m[1][0] + point.z * matrix.m[2][0] + matrix.m[3][0], point.x * matrix.m[0][1] + point.y * matrix.m[1][1] + point.z * matrix.m[2][1] + matrix.m[3][1], point.x * matrix.m[0][2] + point.y * matrix.m[1][2] + point.z * matrix.m[2][2] + matrix.m[3][2] };
	}
}

BabyDollApplication::BabyDollApplication() = default;
BabyDollApplication::~BabyDollApplication() = default;

GameObject* BabyDollApplication::Box(const std::string& name, const Vector3& center, const Vector3& size, const Vector3& color, GameObject* parent, bool collides)
{
	std::shared_ptr<Material> material = std::make_shared<Material>();
	material->baseColor = color;
	material->metallic = 0.05f;
	material->roughness = 0.72f;
	Prefab prefab;
	prefab.model = boxModel;
	prefab.meshMaterialOverrides = { material };
	Transform transform;
	transform.position = center;
	transform.scale = size / 100.0f;
	GameObject* object = world->InstantiatePrefab(prefab, { name, transform });
	if (object && parent) { object->SetParent(parent); }
	if (object && collides) { Attach<BoxColliderComponent>(*object); }
	return object;
}

bool BabyDollApplication::BuildRoom()
{
	const Vector3 navy{ 0.045f, 0.065f, 0.10f };
	const Vector3 brass{ 0.42f, 0.25f, 0.12f };
	for (int x = 0; x < 11; ++x)
	{
		for (int z = 0; z < 9; ++z)
		{
			const float shade = (x + z) % 2 == 0 ? 0.12f : 0.16f;
			if (!Box("Floor", { -2000.0f + x * 400.0f, -12.0f, -1600.0f + z * 400.0f }, { 398.0f, 24.0f, 398.0f }, { shade * 0.75f, shade * 0.85f, shade }, nullptr, false)) { return false; }
		}
	}
	// 엔진 물리 씬의 64바디 한도에 맞춰 바닥 타일 99개를 연속된 한 바디로 합친다.
	GameObject* floorBody = world->CreateGameObject("Continuous physics floor", {});
	if (!floorBody) { return false; }
	floorBody->GetComponent<TransformComponent>()->SetPosition({ 0.0f, -12.0f, 0.0f });
	if (!Attach<BoxColliderComponent>(*floorBody)->SetLocalHalfExtents({ 2200.0f, 12.0f, 1800.0f })) { return false; }
	const PrototypeObstacle walls[]{
		{ { -2250.0f, 0.0f, -1850.0f }, { -2200.0f, 600.0f, 1850.0f } },
		{ { 2200.0f, 0.0f, -1850.0f }, { 2250.0f, 600.0f, 1850.0f } },
		{ { -2200.0f, 0.0f, -1850.0f }, { 2200.0f, 600.0f, -1800.0f } },
		{ { -2200.0f, 0.0f, 1800.0f }, { 2200.0f, 600.0f, 1850.0f } }
	};
	for (const PrototypeObstacle& wall : walls)
	{
		space.obstacles.push_back(wall);
		Box("Gallery wall", (wall.minimum + wall.maximum) * 0.5f, wall.maximum - wall.minimum, navy);
	}
	Box("Gallery ceiling", { 0.0f, 620.0f, 0.0f }, { 4500.0f, 40.0f, 3700.0f }, navy);
	space.obstacles.push_back({ { -2250.0f, 600.0f, -1850.0f }, { 2250.0f, 640.0f, 1850.0f } });
	const auto& plinths = EscapeLayout::Exhibits;
	for (const Vector3& point : plinths)
	{
		Box("Display plinth", point + Vector3{ 0.0f, 90.0f, 0.0f }, { 220.0f, 180.0f, 220.0f }, { 0.16f, 0.13f, 0.18f });
		Box("Plinth trim", point + Vector3{ 0.0f, 185.0f, 0.0f }, { 235.0f, 10.0f, 235.0f }, brass);
		Box("Exhibit", point + Vector3{ 0.0f, 255.0f, 0.0f }, { 80.0f, 130.0f, 80.0f }, { 0.35f, 0.16f, 0.21f });
		space.obstacles.push_back({ point + Vector3{ -120.0f, 0.0f, -120.0f }, point + Vector3{ 120.0f, 320.0f, 120.0f } });
	}
	const auto& tableCenters = EscapeLayout::Tables;
	for (std::size_t index = 0; index < tables.size(); ++index)
	{
		GameObject* root = world->CreateGameObject("Hide table", {});
		if (!root) { return false; }
		root->GetComponent<TransformComponent>()->SetPosition(tableCenters[index]);
		tables[index] = Attach<HideTableComponent>(*root);
		tables[index]->center = tableCenters[index];
		tables[index]->obstacleIndex = space.obstacles.size();
		space.obstacles.push_back({ tableCenters[index] + Vector3{ -160.0f, 100.0f, -120.0f }, tableCenters[index] + Vector3{ 160.0f, 135.0f, 120.0f } });
		tableVisuals[index] = Box("Table top", { 0.0f, 117.5f, 0.0f }, { 320.0f, 35.0f, 240.0f }, { 0.19f, 0.30f, 0.30f }, root);
		for (float x : { -140.0f, 140.0f })
		{
			for (float z : { -100.0f, 100.0f }) { Box("Table leg", { x, 50.0f, z }, { 14.0f, 100.0f, 14.0f }, brass, root); }
		}
	}
	GameObject* lighting = world->CreateGameObject("Gallery lighting", {});
	if (!lighting) { return false; }
	AmbientLightComponent* ambient = Attach<AmbientLightComponent>(*lighting);
	ambient->SetColor({ 0.45f, 0.55f, 0.75f });
	ambient->SetIntensity(0.55f);
	DirectionalLightComponent* sun = Attach<DirectionalLightComponent>(*lighting);
	sun->SetColor({ 0.85f, 0.80f, 0.75f });
	sun->SetIntensity(1.6f);
	lighting->GetComponent<TransformComponent>()->SetRotation({ 55.0f, -30.0f, 0.0f });
	for (int index = 0; index < 9; ++index)
	{
		const Vector3 point{ (index % 3 - 1) * 1500.0f, 420.0f, (index / 3 - 1) * 1100.0f };
		GameObject* lightObject = world->CreateGameObject("Gallery lamp", {});
		if (!lightObject) { return false; }
		lightObject->GetComponent<TransformComponent>()->SetPosition(point);
		PointLightComponent* light = Attach<PointLightComponent>(*lightObject);
		light->SetRange(config.lampRange);
		light->SetIntensity(config.lampIntensity);
		light->SetColor(index % 2 == 0 ? Vector3{ 1.0f, 0.5f, 0.35f } : Vector3{ 0.3f, 0.8f, 1.0f });
		ProximityLightComponent* proximity = Attach<ProximityLightComponent>(*lightObject);
		proximity->SetDarkRadius(config.lampDarkRadius);
		proximity->SetFlickerTime(config.lampFlickerTime);
		proximity->SetBaseIntensity(config.lampIntensity);
		lamps.push_back(proximity);
		Box("Lamp housing", point, { 110.0f, 16.0f, 110.0f }, brass);
	}
	for (const PrototypeObstacle& wall : layout.Partitions())
	{
		space.obstacles.push_back(wall);
		if (!Box("Room partition", (wall.minimum + wall.maximum) * 0.5f, wall.maximum - wall.minimum, { 0.13f, 0.15f, 0.23f })) { return false; }
	}
	doorObstacle = space.obstacles.size();
	space.obstacles.push_back(layout.Door());
	// 플레이어/AI 충돌은 게임의 GameplaySpace에서 함께 제어한다. 열린 문은 천장 위로 올린다.
	doorVisual = Box("Locked exit gate", layout.doorPosition + Vector3{0.0f, 215.0f, 0.0f}, {50.0f, 430.0f, 590.0f}, {0.6f, 0.18f, 0.16f}, nullptr, false);
	keyVisual = world->CreateGameObject("Storage brass key", {});
	if (!keyVisual) { return false; }
	keyVisual->GetComponent<TransformComponent>()->SetPosition(layout.keyPosition + Vector3{0.0f, 100.0f, 0.0f});
	Box("Key head", {}, {50.0f, 50.0f, 15.0f}, {0.95f, 0.7f, 0.15f}, keyVisual, false);
	Box("Key shaft", {0.0f, -50.0f, 0.0f}, {12.0f, 75.0f, 15.0f}, {0.95f, 0.7f, 0.15f}, keyVisual, false);
	Box("Key tooth", {15.0f, -78.0f, 0.0f}, {30.0f, 12.0f, 15.0f}, {0.95f, 0.7f, 0.15f}, keyVisual, false);
	escapeVisual = Box("Escape beacon", layout.escapePosition + Vector3{0.0f, 2.0f, 0.0f}, {250.0f, 4.0f, 250.0f}, {0.1f, 0.85f, 0.5f}, nullptr, false);
	Box("Storage doorway left marker", {-800.0f, 220.0f, 720.0f}, {65.0f, 440.0f, 35.0f}, {0.8f, 0.55f, 0.15f}, nullptr, false);
	Box("Storage doorway right marker", {-800.0f, 220.0f, 1280.0f}, {65.0f, 440.0f, 35.0f}, {0.8f, 0.55f, 0.15f}, nullptr, false);
	return doorVisual && escapeVisual;
}

bool BabyDollApplication::BuildMonster()
{
	monsterModel = AssetManager::Get().LoadModel("Game/Content/BabyDoll/Scissorman/Scissorman.gltf");
	if (!monsterModel) { return false; }
	monsterObject = world->CreateGameObject("SCISSORMAN", {});
	if (!monsterObject) { return false; }
	brain = Attach<StitchBrainComponent>(*monsterObject);
	Prefab prefab;
	prefab.model = monsterModel;
	monsterVisual = world->InstantiatePrefab(prefab, { std::string("ScissormanVisual"), Transform{} });
	if (!monsterVisual || !monsterVisual->SetParent(monsterObject)) { return false; }
	AnimationComponent* animation = monsterVisual->GetComponent<AnimationComponent>();
	if (!animation) { return false; }
	animator = Attach<PoseAnimatorComponent>(*monsterVisual);
	if (!animator->Initialize(monsterModel, *animation) || !animator->Play("Scissorman_Idle", 0.0f)) { return false; }
	for (std::size_t index = 0; index < monsterModel->animations.size(); ++index)
	{
		if (monsterModel->animations[index]->GetName() == "Scissorman_Idle") { inspectClip = static_cast<int>(index); }
	}
	Vector3 minimum{ 1e9f, 1e9f, 1e9f };
	Vector3 maximum{ -1e9f, -1e9f, -1e9f };
	for (const GLTFSceneNode& node : monsterModel->sceneNodes)
	{
		if (!node.mesh) { continue; }
		Matrix4x4 matrix = node.localTransform.ToMatrix();
		std::optional<std::size_t> parent = node.parent;
		while (parent) { matrix = matrix * monsterModel->sceneNodes[*parent].localTransform.ToMatrix(); parent = monsterModel->sceneNodes[*parent].parent; }
		for (const MeshVertex& vertex : monsterModel->meshes[*node.mesh]->vertices)
		{
			const Vector3 point = ModelPoint(vertex.position, matrix);
			minimum = { (std::min)(minimum.x, point.x), (std::min)(minimum.y, point.y), (std::min)(minimum.z, point.z) };
			maximum = { (std::max)(maximum.x, point.x), (std::max)(maximum.y, point.y), (std::max)(maximum.z, point.z) };
		}
	}
	if (maximum.y - minimum.y < 0.001f) { return false; }
	monsterScale = 296.0f / (maximum.y - minimum.y);
	monsterFloorOffset = -minimum.y * monsterScale;
	monsterVisual->GetComponent<TransformComponent>()->SetScale({ monsterScale, monsterScale, monsterScale });
	monsterVisual->GetComponent<TransformComponent>()->SetPosition({ -(minimum.x + maximum.x) * 0.5f * monsterScale, monsterFloorOffset, -(minimum.z + maximum.z) * 0.5f * monsterScale });
	// 원본 메시 경계 대신 실제 Idle 스킨 포즈의 최저점을 바닥에 맞춘다.
	animator->Step(0.0f);
	float posedMinimumY = 1e9f;
	std::vector<GameObject*> pendingObjects{ monsterVisual };
	for (std::size_t index = 0; index < pendingObjects.size(); ++index)
	{
		GameObject* object = pendingObjects[index];
		pendingObjects.insert(pendingObjects.end(), object->GetChildren().begin(), object->GetChildren().end());
		const SkinComponent* skin = object->GetComponent<SkinComponent>();
		const StaticMeshComponent* meshComponent = object->GetComponent<StaticMeshComponent>();
		std::vector<Matrix4x4> bones;
		if (!skin || !meshComponent || !meshComponent->GetStaticMesh() || !skin->CalculateBoneMatrices(bones)) { continue; }
		const StaticMesh& mesh = *meshComponent->GetStaticMesh();
		const Matrix4x4 matrix = object->GetComponent<TransformComponent>()->GetWorldMatrix();
		for (std::size_t vertex = 0; vertex < mesh.vertices.size(); ++vertex)
		{
			Vector3 skinned;
			const SkinWeights& weights = mesh.skinWeights[vertex];
			for (std::size_t influence = 0; influence < 4; ++influence)
			{
				if (weights.weights[influence] > 0.0f) { skinned += ModelPoint(mesh.vertices[vertex].position, bones[weights.joints[influence]]) * weights.weights[influence]; }
			}
			posedMinimumY = (std::min)(posedMinimumY, ModelPoint(skinned, matrix).y);
		}
	}
	if (posedMinimumY < 1e8f)
	{
		TransformComponent* visualTransform = monsterVisual->GetComponent<TransformComponent>();
		Vector3 offset = visualTransform->GetPosition();
		offset.y -= posedMinimumY;
		visualTransform->SetPosition(offset);
	}
	return true;
}

bool BabyDollApplication::OnInitialize(Engine& inEngine)
{
	engine = &inEngine;
	if (!RegisterPrototypeLightType()) { return false; }
	if (!LoadPrototypeConfig(config)) { Log::Write(LogLevel::Error, "BabyDoll", "Invalid or missing Prototype.json"); return false; }
	if (!LoadEscapeLayout(layout)) { Log::Write(LogLevel::Error, "BabyDoll", "Invalid or missing Escape.json"); return false; }
	visualYawOffset = config.monsterVisualYawOffset;
	world = std::make_unique<World>();
	boxModel = AssetManager::Get().LoadModel("Game/Content/BabyDoll/Geometry/Box.gltf");
	ringModel = AssetManager::Get().LoadModel("Game/Content/BabyDoll/Geometry/Ring.gltf");
	if (!boxModel || !ringModel) { Log::Write(LogLevel::Error, "BabyDoll", "Geometry assets could not be loaded"); return false; }
	if (!BuildRoom()) { Log::Write(LogLevel::Error, "BabyDoll", "Room construction failed"); return false; }
	if (!BuildMonster()) { Log::Write(LogLevel::Error, "BabyDoll", "Monster construction failed"); return false; }
	playerObject = world->CreateGameObject("Player", {});
	GameObject* cameraObject = world->CreateGameObject("Prototype camera", {});
	GameObject* soundObject = world->CreateGameObject("Gameplay hearing", {});
	if (!playerObject || !cameraObject || !soundObject) { return false; }
	player = Attach<PlayerMoveComponent>(*playerObject);
	heart = Attach<HeartComponent>(*playerObject);
	rearView = Attach<RearCameraComponent>(*playerObject);
	hearing = Attach<HearingComponent>(*soundObject);
	camera = Attach<CameraComponent>(*cameraObject);
	orbit = Attach<OrbitCameraComponent>(*cameraObject);
	caughtCamera = Attach<CaughtCameraComponent>(*cameraObject);
	caughtLight = Attach<PointLightComponent>(*cameraObject);
	caughtLight->SetRange(400.0f);
	caughtLight->SetColor({ 1.0f, 0.65f, 0.52f });
	caughtLight->SetIntensity(0.0f);
	camera->SetClipDistances(5.0f, 10000.0f);
	if (!world->SetActiveCamera(camera)) { Log::Write(LogLevel::Error, "BabyDoll", "Active camera registration failed"); return false; }
	playerVisual = world->CreateGameObject("Placeholder doll", {});
	if (!playerVisual || !playerVisual->SetParent(playerObject)) { return false; }
	Box("Pink half", { -17.0f, 48.0f, 0.0f }, { 34.0f, 65.0f, 45.0f }, { 0.72f, 0.20f, 0.34f }, playerVisual, false);
	Box("Blue half", { 17.0f, 48.0f, 0.0f }, { 34.0f, 65.0f, 45.0f }, { 0.17f, 0.51f, 0.66f }, playerVisual, false);
	Box("Head", { 0.0f, 97.0f, 0.0f }, { 67.0f, 45.0f, 44.0f }, { 0.62f, 0.46f, 0.48f }, playerVisual, false);
	for (float x : { -25.0f, 25.0f }) { Box("Ear", { x, 120.0f, 0.0f }, { 18.0f, 18.0f, 20.0f }, { 0.43f, 0.28f, 0.32f }, playerVisual, false); }
	Box("Ribbon", { 0.0f, 74.0f, -25.0f }, { 35.0f, 12.0f, 6.0f }, { 0.8f, 0.08f, 0.13f }, playerVisual, false);
	carriedKeyVisual = Box("Carried brass key", {50.0f, 45.0f, 0.0f}, {15.0f, 55.0f, 12.0f}, {0.95f, 0.7f, 0.15f}, playerVisual, false);
	if (!carriedKeyVisual) { return false; }
	Prefab ring;
	ring.model = ringModel;
	std::shared_ptr<Material> ringMaterial = std::make_shared<Material>();
	ringMaterial->baseColor = { 0.25f, 0.8f, 0.8f };
	ringMaterial->alphaMode = MaterialAlphaMode::Blend;
	ringMaterial->baseColorAlpha = 0.6f;
	ringMaterial->metallic = 0.0f;
	ring.meshMaterialOverrides = { ringMaterial };
	soundRing = world->InstantiatePrefab(ring, { std::string("Sound radius"), Transform{} });
	if (!physicsProps.Initialize(*world, boxModel, space, config)) { return false; }
	ResetRun();
	physicsHealthy = world->GetPhysicsScene().Update(0.0f);
	if (!physicsHealthy) { Log::Write(LogLevel::Error, "BabyDoll", "Physics scene preparation failed"); return false; }
	inspect = false;
	paused = true;
	Present(1.0f);
	Log::Write(LogLevel::Info, "BabyDoll", "Ready: S/Period cooperative input, rear CCTV, proximity lamps, Jolt props");
	return soundRing != nullptr;
}

void BabyDollApplication::SetCaptured(bool newCaptured)
{
	captured = newCaptured;
	justCaptured = newCaptured;
	engine->SetCursorConfined(newCaptured);
	engine->SetCursorVisible(!newCaptured);
	pending = {};
	accumulator = 0.0f;
}

void BabyDollApplication::ResetRun()
{
	progress = {};
	deaths = 0;
	clearTime = 0.0f;
	soundPulseTime = 0.0f;
	player->Reset(PlayerSpawn);
	heart->Reset();
	brain->Reset(MonsterSpawn);
	rearView->Clear();
	physicsProps.Reset(*world);
	physicsProps.SyncSpace(*world, space);
	for (ProximityLightComponent* lamp : lamps) { lamp->Reset(); }
	previousPlayerPosition = PlayerSpawn;
	previousMonsterPosition = MonsterSpawn;
	previousMonsterYaw = brain->GetYaw();
	monsterAnimationSpeed = 0.0f;
	monsterLocomoting = false;
	for (HideTableComponent* table : tables) { table->Restore({}); }
	occupiedTable = -1;
	gameTime = caughtTime = exitTime = accumulator = soundRadius = footRadius = 0.0f;
	pending = {};
	ApplyTableVisuals();
	orbit->Reset(PlayerSpawn + Vector3{ 0.0f, 160.0f, 0.0f }, 0.0f, config);
	checkpoint = { PlayerSpawn, MonsterSpawn, { tables[0]->Capture(), tables[1]->Capture() }, true };
	checkpoint.progress = progress;
	ApplyProgressVisuals();
	toast = "Find the brass key in west storage, then return to the red exit gate.";
	toastTime = 6.0f;
	resetRequested = false;
}

void BabyDollApplication::ApplyProgressVisuals()
{
	space.obstacles[doorObstacle].enabled = !progress.IsDoorOpen();
	doorVisual->GetComponent<TransformComponent>()->SetPosition(layout.doorPosition + Vector3{0.0f, progress.IsDoorOpen() ? 900.0f : 215.0f, 0.0f});
	const float visible = progress.HasKey() ? 0.001f : 1.0f;
	keyVisual->GetComponent<TransformComponent>()->SetScale({visible, visible, visible});
	const float carried = progress.HasKey() ? 1.0f : 0.001f;
	carriedKeyVisual->GetComponent<TransformComponent>()->SetScale({carried, carried, carried});
}

void BabyDollApplication::RestoreCheckpoint()
{
	progress = checkpoint.progress;
	player->Reset(checkpoint.playerPosition);
	heart->Reset();
	brain->Reset(checkpoint.monsterPosition);
	rearView->Clear();
	hearing->BeginFrame();
	physicsProps.Reset(*world);
	physicsProps.SyncSpace(*world, space);
	for (auto* lamp : lamps) { lamp->Reset(); }
	for (std::size_t index = 0; index < tables.size(); ++index) { tables[index]->Restore(checkpoint.tables[index]); }
	occupiedTable = -1;
	caughtTime = exitTime = soundPulseTime = 0.0f;
	previousPlayerPosition = checkpoint.playerPosition;
	previousMonsterPosition = checkpoint.monsterPosition;
	previousMonsterYaw = brain->GetYaw();
	monsterAnimationSpeed = 0.0f;
	monsterLocomoting = false;
	ApplyTableVisuals();
	ApplyProgressVisuals();
	orbit->Reset(checkpoint.playerPosition + Vector3{0.0f, 160.0f, 0.0f}, 0.0f, config);
	toast = "Caught. Returned to the entrance checkpoint; key, gate and hiding places restored.";
	toastTime = 5.0f;
}

int BabyDollApplication::FindObjectiveInteraction() const
{
	return FindEscapeInteraction(progress, layout, space, player->GetPosition(), player->GetMode(), orbit->GetYaw(), doorObstacle);
}

int BabyDollApplication::FindInteraction() const
{
	if (player->GetMode() != PlayerMode::Moving) { return -1; }
	float best = -2.0f;
	int selected = -1;
	const Vector3 forward = PrototypeMath::Direction(orbit->GetYaw());
	for (std::size_t index = 0; index < tables.size(); ++index)
	{
		if (tables[index]->IsBroken()) { continue; }
		Vector3 toTable = tables[index]->center - player->GetPosition();
		const float distance = PrototypeMath::Length(toTable);
		if (distance > 330.0f || distance < 0.001f) { continue; }
		const float visibleDistance = space.CameraDistance(player->GetPosition() + Vector3{ 0.0f, 60.0f, 0.0f }, toTable / distance, distance, 0.0f);
		if (visibleDistance < distance - 5.0f) { continue; }
		const float score = (toTable / distance).Dot(forward);
		if (score > 0.15f && score > best) { selected = static_cast<int>(index); best = score; }
	}
	return selected;
}

void BabyDollApplication::BeginCaught()
{
	if (player->GetMode() == PlayerMode::Caught) { return; }
	caughtCamera->Begin(brain->GetPosition(), player->GetPosition(), brain->GetYaw());
	player->SetMode(PlayerMode::Caught);
	caughtTime = 0.0f;
	++deaths;
	pending = {};
	rearView->Clear();
	toastTime = 0.0f;
	animator->Play("Scissorman_SimplesAttack", 0.06f, false);
}

void BabyDollApplication::Interact()
{
	if (player->GetMode() == PlayerMode::Hidden)
	{
		if (brain->GetAction() != StitchAction::Smashing) { LeaveTable(false); }
		return;
	}
	const int objective = FindObjectiveInteraction();
	if (objective == 3) { toast = "Locked. Find the brass key in west storage."; toastTime = 4.0f; return; }
	if (objective == 1 && progress.TakeKey())
	{
		ApplyProgressVisuals();
		toast = "Brass key acquired. Return through the gold doorway to the red exit gate.";
		toastTime = 5.0f;
		return;
	}
	if (objective == 2 && progress.UnlockExit())
	{
		ApplyProgressVisuals();
		toast = "Exit unlocked. Cross the gate and reach the green beacon.";
		toastTime = 4.0f;
		return;
	}
	const int index = FindInteraction();
	if (index < 0) { return; }
	occupiedTable = index;
	tables[index]->Enter(brain->GetState() == StitchState::Chase, player->GetPosition());
	player->SetMode(PlayerMode::Hidden);
	player->Teleport(tables[index]->center);
	if (tables[index]->IsBreakableOccupancy()) { toast = "This hiding place has already been used."; toastTime = 4.0f; }
}

void BabyDollApplication::LeaveTable(bool forced)
{
	if (occupiedTable < 0) { return; }
	Vector3 destination;
	if (!tables[occupiedTable]->FindExit(orbit->GetYaw(), brain->GetPosition(), space, destination, forced))
	{
		if (forced)
		{
			// 모든 출구가 막혀도 부서진 테이블의 숨기 보호가 남지 않게 한다.
			tables[occupiedTable]->Leave();
			occupiedTable = -1;
			BeginCaught();
			return;
		}
		else
		{
		toast = "Exit blocked. Look toward another side.";
		toastTime = 3.0f;
		return;
		}
	}
	tables[occupiedTable]->Leave();
	exitStart = player->GetPosition();
	exitDestination = destination;
	exitTime = 0.0f;
	player->SetMode(PlayerMode::Exiting);
	hearing->Emit(exitStart, 50.0f, SoundKind::Foot);
	if (forced) { toast = "TABLE BROKEN - escape while it recovers!"; toastTime = 2.0f; }
}

void BabyDollApplication::ApplyTableVisuals()
{
	for (std::size_t index = 0; index < tables.size(); ++index)
	{
		const bool broken = tables[index]->IsBroken();
		space.obstacles[tables[index]->obstacleIndex].enabled = !broken;
		BoxColliderComponent* collider = tableVisuals[index]->GetComponent<BoxColliderComponent>();
		if ((broken && collider) || (!broken && !collider))
		{
			GameObject* root = tableVisuals[index]->GetParent();
			const std::vector<GameObject*> oldChildren = root->GetChildren();
			for (GameObject* child : oldChildren) { world->DestroyGameObject(child); }
			tableVisuals[index] = Box("Table top", {}, { 320.0f, 35.0f, 240.0f }, { 0.19f, 0.30f, 0.30f }, root, !broken);
			if (!tableVisuals[index]) { physicsHealthy = false; continue; }
			if (!broken)
			{
				for (float x : { -140.0f, 140.0f })
				{
					for (float z : { -100.0f, 100.0f }) { Box("Table leg", { x, 50.0f, z }, { 14.0f, 100.0f, 14.0f }, { 0.42f, 0.25f, 0.12f }, root); }
				}
			}
		}
		TransformComponent* transform = tableVisuals[index]->GetComponent<TransformComponent>();
		transform->SetPosition(broken ? Vector3{ 25.0f, 12.0f, 20.0f } : Vector3{ 0.0f, 117.5f, 0.0f });
		transform->SetRotation(broken ? EulerAngles{ 0.0f, 18.0f, 9.0f } : EulerAngles{});
	}
}

void BabyDollApplication::Simulate(const PrototypeCommand& command, float deltaTime)
{
	if (progress.stage == EscapeStage::Cleared) { return; }
	gameTime += deltaTime;
	soundPulseTime = (std::max)(0.0f, soundPulseTime - deltaTime);
	previousPlayerPosition = player->GetPosition();
	previousMonsterPosition = brain->GetPosition();
	previousMonsterYaw = brain->GetYaw();
	hearing->BeginFrame();
	if (player->GetMode() == PlayerMode::Caught)
	{
		caughtTime += deltaTime;
		if (caughtTime >= config.caughtDuration)
		{
			RestoreCheckpoint();
		}
		return;
	}
	if (command.interact) { Interact(); }
	player->Step(command, orbit->GetYaw(), deltaTime, config, space, freeMove);
	if (player->GetMode() == PlayerMode::Exiting)
	{
		exitTime += deltaTime;
		const float alpha = std::clamp(exitTime / config.hideExitTime, 0.0f, 1.0f);
		player->Teleport(Vector3::Lerp(exitStart, exitDestination, alpha * alpha * (3.0f - 2.0f * alpha)));
		if (alpha >= 1.0f) { player->SetMode(PlayerMode::Moving); occupiedTable = -1; }
	}
	const bool hidden = player->GetMode() == PlayerMode::Hidden;
	heart->Step(player->GetSpeed(), hidden, brain->GetState() == StitchState::Chase, brain->GetState() == StitchState::Suspicious, PrototypeMath::Distance(player->GetPosition(), brain->GetPosition()), deltaTime, config);
	footRadius = hidden ? 0.0f : HearingComponent::FootRadius(player->GetSpeed(), config.maxSpeed);
	hearing->Emit(player->GetPosition(), footRadius, SoundKind::Foot);
	hearing->Emit(player->GetPosition(), heart->GetSoundRadius(), SoundKind::Heart);
	if (player->HasMistake() || command.pulse) { hearing->Emit(player->GetPosition(), config.wrongSoundRadius, SoundKind::Mistake); soundPulseTime = 0.8f; }
	soundRadius = (std::max)(footRadius, heart->GetSoundRadius());
	if (aiEnabled)
	{
		StitchObservation observation;
		observation.playerPosition = player->GetPosition();
		observation.hidden = hidden;
		observation.breakableTable = occupiedTable >= 0 && tables[occupiedTable]->IsBreakableOccupancy();
		observation.heartBPM = heart->GetBPM();
		observation.sound = hearing->Hear(brain->GetPosition());
		brain->Step(observation, deltaTime, config, space);
		if (brain->GetState() == StitchState::Chase) { heart->OnDetected(); }
		if (brain->DidBreakTable() && occupiedTable >= 0)
		{
			tables[occupiedTable]->Break();
			ApplyTableVisuals();
			LeaveTable(true);
		}
		if (brain->HasCaughtPlayer() && !godMode)
		{
			BeginCaught();
		}
	}
	if (progress.TryEscape(player->GetPosition(), player->GetMode(), layout))
	{
		clearTime = gameTime;
		paused = true;
		rearView->Clear();
		// 고정 틱 처리 도중 입력 누적기를 초기화하지 않는다.
		captured = false;
		engine->SetCursorConfined(false);
		engine->SetCursorVisible(true);
	}
}

void BabyDollApplication::Present(float deltaTime)
{
	const float interpolation = paused || inspect ? 1.0f : std::clamp(accumulator / FixedStep, 0.0f, 1.0f);
	const Vector3 playerPosition = Vector3::Lerp(previousPlayerPosition, player->GetPosition(), interpolation);
	const Vector3 monsterPosition = Vector3::Lerp(previousMonsterPosition, brain->GetPosition(), interpolation);
	if (inspect || player->GetMode() == PlayerMode::Caught) { rearView->Clear(); }
	else if (progress.stage != EscapeStage::Cleared && !rearView->Step(brain->GetState() == StitchState::Chase, previewRear, playerPosition, monsterPosition, brain->GetYaw(), paused ? 0.0f : deltaTime, config, space))
	{
		toast = "Rear camera setup failed"; toastTime = 3.0f;
	}
	for (ProximityLightComponent* lamp : lamps) { lamp->Step(monsterPosition, paused ? 0.0f : deltaTime, lightsEnabled && !inspect); }
	playerObject->GetComponent<TransformComponent>()->SetPosition(playerPosition);
	playerObject->GetComponent<TransformComponent>()->SetRotation({ 0.0f, player->GetYaw(), 0.0f });
	const PlayerMode mode = player->GetMode();
	const bool low = mode == PlayerMode::Hidden || mode == PlayerMode::Exiting || mode == PlayerMode::Fallen;
	playerVisual->GetComponent<TransformComponent>()->SetScale({ 1.0f, low ? 0.35f : 1.0f, low ? 1.5f : 1.0f });
	const float bob = low ? 0.0f : std::sin(gameTime * 12.0f) * 2.0f * std::clamp(player->GetSpeed() / 300.0f, 0.0f, 1.0f);
	playerVisual->GetComponent<TransformComponent>()->SetPosition({ 0.0f, bob, 0.0f });
	monsterObject->GetComponent<TransformComponent>()->SetPosition(monsterPosition);
	const float monsterYaw = PrototypeMath::ApproachAngle(previousMonsterYaw, brain->GetYaw(), interpolation);
	monsterObject->GetComponent<TransformComponent>()->SetRotation({ 0.0f, (mode == PlayerMode::Caught ? caughtCamera->GetFacingYaw() : monsterYaw) + visualYawOffset, 0.0f });
	float playbackRate = 1.0f;
	if (!inspect && mode != PlayerMode::Caught)
	{
		monsterAnimationSpeed += (brain->GetSpeed() - monsterAnimationSpeed) * PrototypeMath::Response(10.0f, paused ? 0.0f : deltaTime);
		if (monsterAnimationSpeed > 35.0f) { monsterLocomoting = true; }
		else if (monsterAnimationSpeed < 15.0f) { monsterLocomoting = false; }
		const StitchAction action = brain->GetAction();
		const char* clip = !monsterLocomoting ? "Scissorman_Idle" : brain->GetState() == StitchState::Patrol ? "Scissorman_Walk" : "Scissorman_Run";
		if (action == StitchAction::Smashing) { clip = "Scissorman_SimplesAttack"; }
		if (action == StitchAction::Looking || action == StitchAction::Roaring) { clip = "Scissorman_ReactPlayer"; }
		const std::string_view nextClip = clip;
		const std::string_view previousClip = animator->GetClipName();
		const bool locomotion = nextClip == "Scissorman_Walk" || nextClip == "Scissorman_Run";
		const bool wasLocomotion = previousClip == "Scissorman_Walk" || previousClip == "Scissorman_Run";
		animator->Play(clip, 0.22f, true, locomotion && wasLocomotion);
		if (locomotion) { playbackRate = std::clamp(monsterAnimationSpeed / (nextClip == "Scissorman_Walk" ? config.patrolSpeed : config.chaseSpeed), 0.55f, 1.6f); }
	}
	animator->Step(paused && !inspect ? 0.0f : deltaTime, playbackRate);
	if (!progress.HasKey())
	{
		keyVisual->GetComponent<TransformComponent>()->SetPosition(layout.keyPosition + Vector3{0.0f, 105.0f + 12.0f * std::sin(gameTime * 2.0f), 0.0f});
		keyVisual->GetComponent<TransformComponent>()->SetRotation({0.0f, gameTime * 45.0f, 0.0f});
	}
	if (soundRing)
	{
		// 0 배율의 역행렬이 렌더 경로로 전달되지 않게 보이지 않을 정도의 최소 반경을 쓴다.
		const float radius = (std::max)(0.001f, inspect ? 0.0f : soundRadius / 100.0f);
		soundRing->GetComponent<TransformComponent>()->SetPosition(playerPosition + Vector3{ 0.0f, 2.0f, 0.0f });
		soundRing->GetComponent<TransformComponent>()->SetScale({ radius, 1.0f, radius });
	}
	const Vector3 target = inspect ? monsterPosition + Vector3{ 0.0f, 155.0f, 0.0f } : playerPosition + Vector3{ 0.0f, 160.0f, 0.0f };
	if (mode == PlayerMode::Caught && !inspect)
	{
		Vector3 head;
		// 공격 중 머리 높이가 달라져도 가슴 대신 얼굴을 계속 담는다.
		const std::optional<Vector3> face = (animator->TryGetNodeWorldPosition("Head2", head) || animator->TryGetNodeWorldPosition("Head", head))
			? std::optional<Vector3>{ head + Vector3{ 0.0f, 12.0f, 0.0f } } : std::nullopt;
		caughtCamera->Present(*camera, caughtTime, config, face);
	}
	else { orbit->Present(*camera, target, std::clamp(player->GetSpeed() / config.maxSpeed, 0.0f, 1.0f), deltaTime, config, space, inspect); }
	caughtLight->SetIntensity(mode == PlayerMode::Caught && !inspect ? 1.5f : 0.0f);
}

void BabyDollApplication::OnUpdate(const InputState& input, float deltaTime)
{
	if (!world || !std::isfinite(deltaTime) || deltaTime <= 0.0f) { return; }
	const float elapsed = (std::min)(deltaTime, 0.1f);
	DWORD process = 0;
	GetWindowThreadProcessId(GetForegroundWindow(), &process);
	if (captured && process != GetCurrentProcessId()) { paused = true; SetCaptured(false); }
	if (resetRequested) { ResetRun(); }
	if (previewCaughtRequested)
	{
		previewCaughtRequested = false;
		inspect = paused = false;
		BeginCaught();
		SetCaptured(true);
	}
	if (dropPropsRequested) { physicsProps.Reset(*world); physicsProps.SyncSpace(*world, space); dropPropsRequested = false; }
	const PrototypeInputFrame frame = inputAdapter.Read(input, soloControls);
	leftFootDown = frame.leftDown;
	rightFootDown = frame.rightDown;
	if (frame.togglePause && progress.stage != EscapeStage::Cleared)
	{
		if (captured) { paused = true; SetCaptured(false); }
		else if (!inspect) { paused = false; SetCaptured(true); }
	}
	if (frame.toggleDiagnostics) { showDebug = !showDebug; }
	const bool orbitInput = captured && !justCaptured;
	if (player->GetMode() != PlayerMode::Caught) { orbit->Look(orbitInput ? frame.command.mouseX : 0.0f, orbitInput ? frame.command.mouseY : 0.0f, elapsed, config); }
	if (!paused && !inspect && captured && !justCaptured)
	{
		pending.feet.insert(pending.feet.end(), frame.command.feet.begin(), frame.command.feet.end());
		pending.interact = pending.interact || frame.command.interact;
		pending.pulse = pending.pulse || frame.command.pulse;
		pending.forward = frame.command.forward;
		pending.right = frame.command.right;
		pending.run = frame.command.run;
		accumulator += elapsed;
		while (accumulator >= FixedStep)
		{
			Simulate(pending, FixedStep);
			if (!world->GetPhysicsScene().Update(FixedStep)) { physicsHealthy = false; paused = true; SetCaptured(false); break; }
			physicsProps.SyncSpace(*world, space);
			pending.feet.clear();
			pending.interact = pending.pulse = false;
			accumulator -= FixedStep;
			if (paused) { accumulator = 0.0f; break; }
		}
	}
	else { pending = {}; accumulator = 0.0f; }
	world->Tick(paused ? 0.0f : elapsed);
	Present(elapsed);
	world->DestroyPendingGameObjects();
	toastTime = (std::max)(0.0f, toastTime - elapsed);
	justCaptured = false;
}

std::optional<SceneRenderRequest> BabyDollApplication::GetRenderRequest()
{
	if (!world || !camera) { return std::nullopt; }
	CameraViewInfo view;
	if (!camera->GetCameraViewInfo(view)) { return std::nullopt; }
	return SceneRenderRequest{ world->GetRenderScene(), view, nullptr };
}

void BabyDollApplication::OnShutdown() noexcept
{
	if (engine) { engine->SetCursorVisible(true); engine->SetCursorConfined(false); }
	if (rearView) { rearView->Clear(); }
	if (world) { world->DestroyPendingGameObjects(); }
	rearView = nullptr;
	caughtCamera = nullptr;
	caughtLight = nullptr;
	lamps.clear();
	player = nullptr;
	heart = nullptr;
	hearing = nullptr;
	brain = nullptr;
	orbit = nullptr;
	animator = nullptr;
	camera = nullptr;
	tables = {};
	if (world) { world->GetRenderScene().ClearObjectDataIds(); }
	world.reset();
	monsterModel.reset();
	ringModel.reset();
	boxModel.reset();
	engine = nullptr;
}
