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
#include "VoxelDollComponent.h"
#include "TraversalComponent.h"
#include "HandInventoryComponent.h"
#include "ObjectSoundComponent.h"
#include "StitchVisualComponent.h"

#include "AssetManager.h"
#include "Component/AmbientLightComponent.h"
#include "Component/AnimationComponent.h"
#include "Component/CameraComponent.h"
#include "Component/BoxColliderComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/SkyboxComponent.h"
#include "Component/SpotLightComponent.h"
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
#include "Texture/Texture.h"
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
	MaterialParameters parameters;
	const float gray=0.26f+0.5f*(color.x*0.21f+color.y*0.72f+color.z*0.07f);
	parameters.baseColor = name.find("Key")!=std::string::npos ? color : Vector3{gray,gray,gray};
	parameters.metallic = 0.0f;
	parameters.roughness = 0.95f;
	const std::shared_ptr<const Material> material = std::make_shared<const Material>(name, std::string(), std::move(parameters));
	Prefab prefab;
	prefab.model = boxModel;
	prefab.meshMaterialOverrides = { material };
	Transform transform;
	transform.position = center;
	transform.scale = size / 100.0f;
	GameObject* object = world->InstantiatePrefab(prefab, { name, transform });
	if (object && parent) { object->SetParent(parent); }
	if (object && collides) { Attach<BoxColliderComponent>(*object); }
	if(object&&(name.find("ceiling")!=std::string::npos||name=="Upper floor around ladder opening"||(name=="Research room shell"&&size.y<100.0f)))
	{
		reviewRoofs.push_back({object->GetObjectID(),center});
	}
	return object;
}

bool BabyDollApplication::BuildRoom()
{
	const auto backdrop = AssetManager::Get().LoadTexture("Game/Content/BabyDoll/Materials/GrayboxBackdrop.png", TextureFormat::RGBA8SRGB);
	GameObject* background = world->CreateGameObject("Graybox backdrop", {});
	if (!backdrop || !background) { return false; }
	Attach<SkyboxComponent>(*background)->SetFaces({backdrop, backdrop, backdrop, backdrop, backdrop, backdrop});
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
	for(int x=-2000;x<=2000;x+=200) {Box("200cm floor grid",{static_cast<float>(x),0.6f,0.0f},{1.5f,0.8f,3600.0f},{0.46f,0.46f,0.46f},nullptr,false);}
	for(int z=-1600;z<=1600;z+=200) {Box("200cm floor grid",{0.0f,0.6f,static_cast<float>(z)},{4400.0f,0.8f,1.5f},{0.46f,0.46f,0.46f},nullptr,false);}
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
	const PrototypeObstacle ceilings[]{
		{{-2250.0f, 600.0f, -1850.0f}, {1250.0f, 638.0f, 1850.0f}},
		{{1750.0f, 600.0f, -1850.0f}, {2250.0f, 638.0f, 1850.0f}},
		{{1250.0f, 600.0f, -1850.0f}, {1750.0f, 638.0f, -700.0f}},
		{{1250.0f, 600.0f, 600.0f}, {1750.0f, 638.0f, 1850.0f}}
	};
	for (const auto& slab : ceilings)
	{
		Box("Gallery ceiling", (slab.minimum + slab.maximum) * 0.5f, slab.maximum - slab.minimum, navy, nullptr, false);
		space.obstacles.push_back(slab);
	}
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
	sceneAmbient=ambient;
	ambient->SetColor({ 0.65f, 0.65f, 0.65f });
	ambient->SetIntensity(config.ambientIntensity);
	DirectionalLightComponent* sun = Attach<DirectionalLightComponent>(*lighting);
	sceneFill=sun;
	sun->SetColor({ 1.0f, 1.0f, 1.0f });
	sun->SetIntensity(config.directionalIntensity);
	lighting->GetComponent<TransformComponent>()->SetRotation({ 55.0f, -30.0f, 0.0f });
	// 재질과 발광 텍스처는 처음 한 번 만들고 모든 전등이 상태별로 공유한다.
	std::array<std::array<std::shared_ptr<const Material>, 3>, 2> lampMaterials;
	const char* materialPaths[2][3]{
		{ "LampOff", "LampWarmDim", "LampWarmOn" },
		{ "LampOff", "LampCoolDim", "LampCoolOn" }
	};
	for (int color = 0; color < 2; ++color)
	{
		for (int level = 0; level < 3; ++level)
		{
			lampMaterials[color][level] = AssetManager::Get().LoadMaterial(std::string("Game/Content/BabyDoll/Materials/") + materialPaths[color][level] + ".material.json");
			if (!lampMaterials[color][level]) { return false; }
		}
	}
	const auto partitions = layout.Partitions();
	// Only partitions and the gate separate playable spaces. The perimeter encloses the entire gallery.
	std::vector<PrototypeObstacle> lightWalls = partitions;
	lightWalls.insert(lightWalls.end(), GrayboxLayout::UpperRoomShell.begin(), GrayboxLayout::UpperRoomShell.end());
	lightWalls.push_back(layout.Door());
	const Vector3 lightColor{1.0f, 0.93f, 0.82f};
	for (const Vector3& point : GrayboxLayout::Lamps)
	{
		GameObject* lightObject = world->CreateGameObject("Gallery lamp", {});
		if (!lightObject) { return false; }
		lightObject->GetComponent<TransformComponent>()->SetPosition(point);
		const auto placement = GrayboxLayout::FitGalleryPointLight(point, config.lampRange, lightWalls);
		GameObject* emitter = world->CreateGameObject("Gallery lamp emitter", {});
		if (!emitter || !emitter->SetParent(lightObject)) { return false; }
		emitter->GetComponent<TransformComponent>()->SetPosition(placement.position - point);
		auto* light = Attach<PointLightComponent>(*emitter);
		if (!light->SetRange(placement.range)) { return false; }
		light->SetColor(lightColor);
		ProximityLightComponent* proximity = Attach<ProximityLightComponent>(*emitter);
		proximity->SetDarkRadius(config.lampDarkRadius);
		proximity->SetFlickerTime(config.lampFlickerTime);
		proximity->SetBaseIntensity(config.lampIntensity);
		lamps.push_back(proximity);
		if (!Box("Lamp housing", { 0.0f, 41.0f, 0.0f }, { 150.0f, 18.0f, 90.0f }, { 0.09f, 0.1f, 0.12f }, lightObject)) { return false; }
		GameObject* diffuser = Box("Emissive lamp diffuser", { 0.0f, 29.0f, 0.0f }, { 130.0f, 6.0f, 70.0f }, {}, lightObject, false);
		if (!diffuser || !proximity->BindEmissiveVisual(*diffuser, lampMaterials[0])) { return false; }
	}
	for (const PrototypeObstacle& wall : partitions)
	{
		space.obstacles.push_back(wall);
		if (!Box("Room partition", (wall.minimum + wall.maximum) * 0.5f, wall.maximum - wall.minimum, { 0.42f, 0.42f, 0.42f },nullptr,false)) { return false; }
		Box("Partition cap",{(wall.minimum.x+wall.maximum.x)*0.5f,wall.maximum.y+2.0f,(wall.minimum.z+wall.maximum.z)*0.5f},
			{wall.maximum.x-wall.minimum.x+4.0f,4.0f,wall.maximum.z-wall.minimum.z+4.0f},{0.72f,0.72f,0.72f},nullptr,false);
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
	GameObject* frontLight = world->CreateGameObject("Stitch red front light", {});
	if (!frontLight) { return false; }
	monsterFrontLight = Attach<SpotLightComponent>(*frontLight);
	monsterFrontLight->SetColor({1.0f, 0.018f, 0.025f});
	monsterFrontLight->SetRange(730.0f);
	monsterFrontLight->SetOuterConeAngleDegrees(38.0f);
	monsterFrontLight->SetIntensity(2.1f);
	GameObject* frontFill = world->CreateGameObject("Stitch red close fill", {});
	if (!frontFill) { return false; }
	monsterFrontFill = Attach<PointLightComponent>(*frontFill);
	monsterFrontFill->SetColor({1.0f, 0.018f, 0.025f});
	monsterFrontFill->SetRange(235.0f);
	monsterFrontFill->SetIntensity(0.65f);
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
	stitchVisualObject = world->CreateGameObject("Voxel Stitches visual",{});
	if(!stitchVisualObject || !stitchVisualObject->SetParent(monsterObject)) {return false;}
	stitchVisual = Attach<StitchVisualComponent>(*stitchVisualObject);
	if(!stitchVisual->Initialize(boxModel)) {return false;}
	playerObject = world->CreateGameObject("Player", {});
	GameObject* cameraObject = world->CreateGameObject("Prototype camera", {});
	GameObject* soundObject = world->CreateGameObject("Gameplay hearing", {});
	if (!playerObject || !cameraObject || !soundObject) { return false; }
	GameObject* playerFillObject = world->CreateGameObject("Player local fill", {});
	if (!playerFillObject || !playerFillObject->SetParent(playerObject)) { return false; }
	playerFillLight = Attach<PointLightComponent>(*playerFillObject);
	playerFillLight->SetColor({0.82f, 0.89f, 1.0f});
	playerFillLight->SetIntensity(0.16f);
	playerFillLight->SetRange(180.0f);
	player = Attach<PlayerMoveComponent>(*playerObject);
	traversal = Attach<TraversalComponent>(*playerObject);
	inventory = Attach<HandInventoryComponent>(*playerObject);
	objectSounds = Attach<ObjectSoundComponent>(*soundObject);
	if (!objectSounds->Load()) { return false; }
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
	playerVisual = world->CreateGameObject("Voxel teddy visual", {});
	if (!playerVisual || !playerVisual->SetParent(playerObject)) { return false; }
	doll = Attach<VoxelDollComponent>(*playerVisual);
	if (!doll->Initialize()) { Log::Write(LogLevel::Error, "BabyDoll", "Voxel teddy assets could not be loaded"); return false; }
	carriedKeyVisual = world->CreateGameObject("Carried brass key",{});
	if (!carriedKeyVisual||!carriedKeyVisual->SetParent(playerVisual)) { return false; }
	Box("Held brass shaft",{0.0f,0.0f,0.0f},{8.0f,35.0f,8.0f},{0.95f,0.7f,0.15f},carriedKeyVisual,false);
	Box("Held brass head",{0.0f,24.0f,0.0f},{25.0f,20.0f,8.0f},{0.95f,0.7f,0.15f},carriedKeyVisual,false);
	secondCarriedKey = world->CreateGameObject("Carried silver key",{});
	if(!secondCarriedKey||!secondCarriedKey->SetParent(playerVisual)) {return false;}
	Box("Held silver shaft",{0.0f,0.0f,0.0f},{8.0f,35.0f,8.0f},{0.65f,0.8f,0.9f},secondCarriedKey,false);
	Box("Held silver head",{0.0f,24.0f,0.0f},{25.0f,20.0f,8.0f},{0.65f,0.8f,0.9f},secondCarriedKey,false);
	if (!secondCarriedKey || !BuildChapter()) { return false; }
	Prefab ring;
	ring.model = ringModel;
	const auto ringMaterial = AssetManager::Get().LoadMaterial("Game/Content/BabyDoll/Materials/SoundRing.material.json");
	if (!ringMaterial) { return false; }
	ring.meshMaterialOverrides = { ringMaterial };
	soundRing = world->InstantiatePrefab(ring, { std::string("Sound radius"), Transform{} });
	if (!physicsProps.Initialize(*world, boxModel, space, config)) { return false; }
	ResetRun(false);
	LoadCheckpoint();
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
	if (newCaptured) { grayboxReview = false; }
	captured = newCaptured;
	justCaptured = newCaptured;
	engine->SetCursorConfined(newCaptured);
	engine->SetCursorVisible(!newCaptured);
	pending = {};
	accumulator = 0.0f;
}

void BabyDollApplication::ResetRun(bool clearSaved)
{
	chaseViewWeight = 0.0f;
	if (clearSaved) { ClearCheckpointFile(); }
	progress = {};
	inventory->Restore({});
	traversal->Reset(config.hangDuration);
	objectSounds->Reset();
	exitDoor->Restore({});
	upperCheckpointSaved = false;
	deaths = 0;
	clearTime = 0.0f;
	soundPulseTime = 0.0f;
	player->Reset(PlayerSpawn);
	heart->Reset(config);
	brain->Reset(MonsterSpawn);
	stitchVisual->Reset();
	rearView->Clear();
	physicsProps.Reset(*world);
	physicsProps.SyncSpace(*world, space);
	for (ProximityLightComponent* lamp : lamps) { lamp->Reset(); }
	previousPlayerPosition = PlayerSpawn;
	previousPlayerYaw = player->GetYaw();
	doll->Reset(PlayerSpawn);
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
	checkpoint.hands = inventory->Capture();
	checkpoint.door = exitDoor->state;
	checkpoint.monster = brain->Capture();
	checkpoint.props = physicsProps.Capture(*world);
	checkpoint.heart = heart->Capture();
	checkpoint.soundSequence = hearing->GetSequence();
	ApplyProgressVisuals();
	toast.clear();
	toastTime = 0.0f;
	resetRequested = false;
}

void BabyDollApplication::ApplyProgressVisuals()
{
	space.obstacles[doorObstacle].enabled = !exitDoor->IsPassable();
	doorVisual->GetComponent<TransformComponent>()->SetPosition(layout.doorPosition + Vector3{0.0f, 215.0f + exitDoor->state.opening * 650.0f, 0.0f});
	const float visible = progress.HasKey() ? 0.001f : 1.0f;
	keyVisual->GetComponent<TransformComponent>()->SetScale({visible, visible, visible});
	const float carried = progress.HasKey() ? 1.0f : 0.001f;
	carriedKeyVisual->GetComponent<TransformComponent>()->SetScale({carried, carried, carried});
	const int brassHand = inventory->Get(0) == 1 ? 0 : 1;
	if(GameObject* hand=doll->GetHandAnchor(brassHand)) {carriedKeyVisual->SetParent(hand);}
	carriedKeyVisual->GetComponent<TransformComponent>()->SetPosition({0.0f,-32.0f,12.0f});
	const bool silver = inventory->Get(0) == 2 || inventory->Get(1) == 2;
	if (secondKeyVisual) { secondKeyVisual->GetComponent<TransformComponent>()->SetScale(silver ? Vector3{0.001f,0.001f,0.001f} : Vector3{1.0f,1.0f,1.0f}); }
	if (secondCarriedKey)
	{
		secondCarriedKey->GetComponent<TransformComponent>()->SetScale(silver ? Vector3{1.0f,1.0f,1.0f} : Vector3{0.001f,0.001f,0.001f});
		if(GameObject* hand=doll->GetHandAnchor(inventory->Get(0)==2?0:1)) {secondCarriedKey->SetParent(hand);}
		secondCarriedKey->GetComponent<TransformComponent>()->SetPosition({0.0f,-32.0f,12.0f});
	}
}

void BabyDollApplication::RestoreCheckpoint()
{
	chaseViewWeight = 0.0f;
	progress = checkpoint.progress;
	inventory->Restore(checkpoint.hands);
	exitDoor->Restore(checkpoint.door);
	traversal->Reset(config.hangDuration);
	objectSounds->Reset();
	player->Reset(checkpoint.playerPosition);
	player->RestoreFoot(checkpoint.playerLastFoot);
	// 진행도는 복원하지만 재시작 순간의 추격/심박은 새로 시작한다.
	heart->Reset(config);
	stitchVisual->Reset();
	rearView->Clear();
	hearing->RestoreSequence(checkpoint.soundSequence);
	physicsProps.Restore(*world,checkpoint.props);
	physicsProps.SyncSpace(*world, space);
	for (auto* lamp : lamps) { lamp->Reset(); }
	for (std::size_t index = 0; index < tables.size(); ++index) { tables[index]->Restore(checkpoint.tables[index]); }
	occupiedTable = -1;
	caughtTime = exitTime = soundPulseTime = 0.0f;
	footRadius = soundRadius = 0.0f;
	pending = {};
	checkpointRequested = false;
	previousPlayerPosition = checkpoint.playerPosition;
	previousPlayerYaw = player->GetYaw();
	doll->Reset(checkpoint.playerPosition);
	monsterAnimationSpeed = 0.0f;
	monsterLocomoting = false;
	ApplyTableVisuals();
	ApplyProgressVisuals();
	const bool safeRestart = brain->RestartPatrol(checkpoint.monsterPosition, checkpoint.playerPosition, config, space);
	previousMonsterPosition = brain->GetPosition();
	previousMonsterYaw = brain->GetYaw();
	orbit->Reset(checkpoint.playerPosition + Vector3{0.0f, 160.0f, 0.0f}, 0.0f, config);
	toast = "Checkpoint restored.";
	toastTime = 5.0f;
	if (!safeRestart)
	{
		// 지형이 바뀌어 안전한 위치가 없으면 추격 상태로 강제 재시작하지 않는다.
		brain->Reset(checkpoint.monsterPosition);
		previousMonsterPosition = brain->GetPosition();
		previousMonsterYaw = brain->GetYaw();
		paused = true;
		SetCaptured(false);
		toast = "Checkpoint restart blocked: no safe monster patrol position.";
	}
}

int BabyDollApplication::FindObjectiveInteraction() const
{
	if(player->GetMode()!=PlayerMode::Moving) {return 0;}
	const int selected=FindEscapeInteraction(progress, layout, space, player->GetPosition(), player->GetMode(), orbit->GetYaw(), doorObstacle);
	if(selected==1) {return 1;}
	const auto bounds=layout.Door();
	const Vector3 position=player->GetPosition();
	const Vector3 nearest{std::clamp(position.x,bounds.minimum.x,bounds.maximum.x),0.0f,std::clamp(position.z,bounds.minimum.z,bounds.maximum.z)};
	const Vector3 delta=nearest-position;
	const float distance=PrototypeMath::Distance(position,nearest);
	if(std::abs(position.y)>80.0f||distance>layout.interactionDistance) {return 0;}
	if(distance>30.0f&&(delta/distance).Dot(PrototypeMath::Direction(orbit->GetYaw()))<0.15f) {return 0;}
	if(distance>1.0f&&space.CameraDistance(position+Vector3{0.0f,65.0f,0.0f},delta/distance,distance,0.0f,doorObstacle)<distance-5.0f) {return 0;}
	return exitDoor->state.locked?3:exitDoor->state.targetOpen?4:2;
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
	if(useVoxelStitch)
	{
		// 촉수 바깥의 구도가 벽에 막히면 가까운 방향부터 빈 구도를 찾는다.
		const Vector3 origin=brain->GetPosition()+Vector3{0.0f,config.caughtTentacleAimHeight,0.0f};
		const float required=35.0f+config.caughtTentacleDistance+18.0f;
		const float initialYaw=caughtCamera->GetFacingYaw();
		float bestYaw=initialYaw, bestClearance=-1.0f;
		for(float offset:{0.0f,30.0f,-30.0f,60.0f,-60.0f,90.0f,-90.0f,135.0f,-135.0f,180.0f})
		{
			const float yaw=initialYaw+offset;
			const float clearance=space.CameraDistance(origin,PrototypeMath::Direction(yaw),required,12.0f);
			if(clearance>bestClearance) {bestClearance=clearance;bestYaw=yaw;}
			if(clearance>=required-0.01f) {break;}
		}
		caughtCamera->Begin(brain->GetPosition(),brain->GetPosition()+PrototypeMath::Direction(bestYaw)*100.0f,bestYaw);
		stitchVisual->RevealForCapture();
		caughtCamera->BlendFrom(*camera);
	}
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
	previousPlayerYaw = player->GetYaw();
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
	for (int hand : command.hands) { UseHand(hand); }
	if (command.hands.empty() && command.interact) { UseHand(1); }
	if(!traversal->IsActive()) {player->Step(command, orbit->GetYaw(), deltaTime, config, space, freeMove);}
	traversal->Step(command, deltaTime, config, space, *player);
	if (player->GetMode() == PlayerMode::Exiting)
	{
		exitTime += deltaTime;
		const float alpha = std::clamp(exitTime / config.hideExitTime, 0.0f, 1.0f);
		player->Teleport(Vector3::Lerp(exitStart, exitDestination, alpha * alpha * (3.0f - 2.0f * alpha)));
		if (alpha >= 1.0f) { player->SetMode(PlayerMode::Moving); occupiedTable = -1; }
	}
	// Entering the elevator ends this tick before the pursuer can catch up to the player.
	if (progress.TryEscapeElevator(player->GetPosition(), player->GetMode()))
	{
		clearTime = gameTime;
		paused = true;
		rearView->Clear();
		// 고정 틱 처리 도중 입력 누적기를 초기화하지 않는다.
		captured = false;
		engine->SetCursorConfined(false);
		engine->SetCursorVisible(true);
		return;
	}
	const bool hidden = player->GetMode() == PlayerMode::Hidden || traversal->IsHidden();
	const int playerFloor = static_cast<int>(std::floor((player->GetPosition().y + 1.0f) / 640.0f));
	const int monsterFloor = static_cast<int>(std::floor((brain->GetPosition().y + 1.0f) / 640.0f));
	heart->Step(traversal->IsActive()?traversal->GetSpeed():player->GetSpeed(), hidden, brain->GetState() == StitchState::Chase, brain->GetState() == StitchState::Suspicious,
		playerFloor==monsterFloor?PrototypeMath::Distance(player->GetPosition(),brain->GetPosition()):100000.0f,deltaTime,config);
	footRadius = hidden ? 0.0f : HearingComponent::FootRadius(traversal->IsActive() ? traversal->GetSpeed() : player->GetSpeed(), config.maxSpeed, &config);
	hearing->Emit(player->GetPosition(), footRadius, SoundKind::Foot, true, playerFloor);
	hearing->Emit(player->GetPosition(), heart->GetSoundRadius(), SoundKind::Heart, true, playerFloor);
	if (player->HasMistake() || traversal->MadeMistake() || command.pulse) { hearing->Emit(player->GetPosition(), config.wrongSoundRadius, SoundKind::Mistake, true, playerFloor); soundPulseTime = 0.8f; }
	physicsProps.EmitPlayerBumps(*world,player->GetPosition(),player->GetAttemptedSpeed(),*objectSounds);
	objectSounds->Step(deltaTime, *hearing);
	const bool sameDoorFloor = std::abs(brain->GetPosition().y - layout.doorPosition.y) < 100.0f;
	const bool monsterAtDoor = sameDoorFloor && PrototypeMath::Distance(brain->GetPosition(), layout.doorPosition) < 370.0f;
	if (monsterAtDoor) { exitDoor->OpenForMonster(brain->GetState() == StitchState::Patrol); }
	exitDoor->Step(deltaTime, PrototypeMath::Distance(player->GetPosition(), layout.doorPosition) < 100.0f || monsterAtDoor);
	ApplyProgressVisuals();
	soundRadius = (std::max)(footRadius, heart->GetSoundRadius());
	if (aiEnabled)
	{
		StitchObservation observation;
		observation.playerPosition = player->GetPosition();
		observation.hidden = hidden;
		observation.breakableTable = occupiedTable >= 0 && tables[occupiedTable]->IsBreakableOccupancy();
		observation.heartBPM = heart->GetBPM();
		observation.sound = hearing->Hear(brain->GetPosition(), monsterFloor);
		observation.inCrawl = traversal->GetLink()&&traversal->GetLink()->kind==TraversalKind::Crawl;
		observation.onLadder = player->GetMode() == PlayerMode::Ladder;
		GameplaySpace navigation = space;
		navigation.obstacles[doorObstacle].enabled = false;
		if (monsterAtDoor && !exitDoor->IsPassable()) { brain->Hold(deltaTime, config, space); }
		else { brain->Step(observation, deltaTime, config, navigation); }
		if (brain->GetState() == StitchState::Chase) { heart->OnDetected(); }
		if (brain->DidBreakTable() && occupiedTable >= 0)
		{
			tables[occupiedTable]->Break();
			objectSounds->Trigger("table" + std::to_string(occupiedTable), "table", "Break", tables[occupiedTable]->center);
			ApplyTableVisuals();
			LeaveTable(true);
		}
		if (brain->HasCaughtPlayer() && !godMode)
		{
			BeginCaught();
		}
	}
	if (checkpointRequested && player->GetMode() == PlayerMode::Moving) { SaveCheckpoint(); checkpointRequested = false; }
	if (!upperCheckpointSaved && playerFloor == 1 && player->GetMode() == PlayerMode::Moving) { upperCheckpointSaved = true; SaveCheckpoint(); }
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
	const float playerYaw = player->GetMode()==PlayerMode::Ladder ? player->GetYaw() : PrototypeMath::ApproachAngle(previousPlayerYaw, player->GetYaw(), interpolation);
	playerObject->GetComponent<TransformComponent>()->SetRotation({ 0.0f, playerYaw, 0.0f });
	const PlayerMode mode = player->GetMode();
	const bool lowPose = mode == PlayerMode::Hidden || mode == PlayerMode::Exiting
		|| (traversal->GetLink() && traversal->GetLink()->kind == TraversalKind::Crawl);
	auto* playerFillTransform = playerFillLight->GetGameObject()->GetComponent<TransformComponent>();
	playerFillTransform->SetPosition({0.0f, lowPose ? 86.0f : 138.0f, 0.0f});
	playerFillLight->SetRange(lowPose ? 125.0f : 180.0f);
	playerFillLight->SetEnabled(!inspect && !grayboxReview && mode != PlayerMode::Caught);
	doll->Present(playerPosition, traversal->IsActive()?traversal->GetSpeed():player->GetSpeed(), playerYaw, traversal->UsesFixedCamera()?playerYaw:orbit->GetYaw(), mode, paused ? 0.0f : deltaTime, config,
		std::clamp((heart->GetBPM()-90.0f)/30.0f,0.0f,1.0f),player->GetLastFoot(),traversal->GetLink()&&traversal->GetLink()->kind==TraversalKind::Crawl);
	monsterObject->GetComponent<TransformComponent>()->SetPosition(monsterPosition);
	const float monsterYaw = PrototypeMath::ApproachAngle(previousMonsterYaw, brain->GetYaw(), interpolation);
	const bool chasing = brain->GetState() == StitchState::Chase;
	const bool showThreat = !inspect && !grayboxReview && mode != PlayerMode::Caught && progress.stage != EscapeStage::Cleared;
	const float chaseTarget = chasing && showThreat ? 1.0f : 0.0f;
	chaseViewWeight += (chaseTarget - chaseViewWeight) * PrototypeMath::Response(chaseTarget > chaseViewWeight ? 10.0f : 4.0f, deltaTime);
	auto* frontLightTransform = monsterFrontLight->GetGameObject()->GetComponent<TransformComponent>();
	frontLightTransform->SetPosition(monsterPosition + PrototypeMath::Direction(monsterYaw) * 105.0f + Vector3{0.0f, 165.0f, 0.0f});
	frontLightTransform->SetRotation({22.0f, monsterYaw, 0.0f});
	monsterFrontLight->SetEnabled(showThreat);
	const float presentedMonsterYaw = mode == PlayerMode::Caught ? caughtCamera->GetFacingYaw() : monsterYaw;
	auto* frontFillTransform = monsterFrontFill->GetGameObject()->GetComponent<TransformComponent>();
	frontFillTransform->SetPosition(monsterPosition + PrototypeMath::Direction(presentedMonsterYaw) * 105.0f + Vector3{0.0f, 155.0f, 0.0f});
	monsterFrontFill->SetEnabled(showThreat || (mode == PlayerMode::Caught && !inspect && !grayboxReview));
	monsterObject->GetComponent<TransformComponent>()->SetRotation({ 0.0f, presentedMonsterYaw + visualYawOffset, 0.0f });
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
	const bool voxelMonster=useVoxelStitch&&!inspect;
	const float importedScale=voxelMonster?0.001f:monsterScale;
	monsterVisual->GetComponent<TransformComponent>()->SetScale({importedScale,importedScale,importedScale});
	const float stitchScale=voxelMonster?1.0f:0.001f;
	stitchVisualObject->GetComponent<TransformComponent>()->SetScale({stitchScale,stitchScale,stitchScale});
	stitchVisualObject->GetComponent<TransformComponent>()->SetRotation({0.0f,-visualYawOffset,0.0f});
	if (mode != PlayerMode::Caught || !voxelMonster)
	{
		stitchVisual->Present(brain->GetState(),brain->GetAction(),PrototypeMath::Distance(playerPosition,monsterPosition),brain->GetSpeed(),paused?0.0f:deltaTime);
	}
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
		// 곰돌이는 촉수와 벌어진 몸을 담고, 가져온 모델은 애니메이션 머리를 추적한다.
		const std::optional<Vector3> face = voxelMonster ? std::optional<Vector3>{monsterPosition+Vector3{0.0f,config.caughtTentacleAimHeight,0.0f}+PrototypeMath::Direction(caughtCamera->GetFacingYaw())*35.0f} : (animator->TryGetNodeWorldPosition("Head2", head) || animator->TryGetNodeWorldPosition("Head", head))
			? std::optional<Vector3>{ head + Vector3{ 0.0f, 12.0f, 0.0f } } : std::nullopt;
		const std::optional<CaughtCameraFraming> framing = voxelMonster ? std::optional<CaughtCameraFraming>{
			CaughtCameraFraming{config.caughtTentacleDistance,config.caughtTentacleMinimumDistance,config.caughtTentacleFov,4.0f}} : std::nullopt;
		caughtCamera->Present(*camera, caughtTime, config, face, framing);
		if (voxelMonster)
		{
			stitchVisual->PresentCapture(caughtTime);
		}
	}
	else
	{
		orbit->Present(*camera, target, std::clamp(player->GetSpeed() / config.maxSpeed, 0.0f, 1.0f), deltaTime, config, space, inspect);
		const auto shot = !inspect ? traversal->GetCameraShot(playerPosition) : std::nullopt;
		orbit->BlendFixed(*camera, shot ? std::optional<Vector3>{shot->position} : std::nullopt,
			shot?shot->target:playerPosition, paused ? 0.0f : deltaTime,shot?shot->fov:65.0f,&space,shot?shot->framingHeight:0.0f);
	}
	if(grayboxReview)
	{
		auto* transform=camera->GetGameObject()->GetComponent<TransformComponent>();
		transform->SetPosition({-2900.0f,4100.0f,-3200.0f});
		transform->SetRotation({46.0f,42.0f,0.0f});
		camera->SetVerticalFovDegrees(58.0f);
	}
	for(const auto& roof:reviewRoofs)
	{
		if(GameObject* object=world->FindGameObject(roof.first)) {object->GetComponent<TransformComponent>()->SetPosition(roof.second+(grayboxReview?Vector3{0.0f,-10000.0f,0.0f}:Vector3{}));}
	}
	if(sceneAmbient) {sceneAmbient->SetIntensity(grayboxReview?0.8f:config.ambientIntensity);}
	if(sceneFill) {sceneFill->SetIntensity(grayboxReview?0.8f:config.directionalIntensity);}
	caughtLight->SetIntensity(mode == PlayerMode::Caught && !inspect ? 1.5f : 0.0f);
	caughtLight->SetRange(voxelMonster?900.0f:400.0f);
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
		else if (!inspect) { grayboxReview=false; paused = false; SetCaptured(true); }
	}
	if (frame.toggleDiagnostics) { showDebug = !showDebug; }
	const bool orbitInput = captured && !justCaptured;
	if (player->GetMode() != PlayerMode::Caught && !traversal->UsesFixedCamera()) { orbit->Look(orbitInput ? frame.command.mouseX : 0.0f, orbitInput ? frame.command.mouseY : 0.0f, elapsed, config); }
	if (!paused && !inspect && captured && !justCaptured)
	{
		pending.feet.insert(pending.feet.end(), frame.command.feet.begin(), frame.command.feet.end());
		pending.hands.insert(pending.hands.end(), frame.command.hands.begin(), frame.command.hands.end());
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
			pending.hands.clear();
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
	// Main layer uses the room lights. Default environment IBL otherwise lights the room even with all lamps off.
	Hakko::DataHub::SetLayerEnvIntensity(grayboxReview || inspect ? 0.65f : 0.0f);
	SceneRenderRequest request{ world->GetRenderScene(), view, nullptr };
	if (!inspect && !grayboxReview && player->GetMode() == PlayerMode::Caught)
	{
		request.postProcessPasses.push_back({
			"Game/Content/BabyDoll/Shaders/ChaseView_PS.hlsl",
			{0.0f, 0.82f * PrototypeMath::Response(15.0f, caughtTime), 0.0f}
		});
	}
	else if (!inspect && !grayboxReview && progress.stage != EscapeStage::Cleared && chaseViewWeight > 0.005f)
	{
		request.postProcessPasses.push_back({
			"Game/Content/BabyDoll/Shaders/ChaseView_PS.hlsl",
			{0.055f * chaseViewWeight, 0.76f * chaseViewWeight, std::floor(gameTime * 24.0f)}
		});
	}
	return request;
}

void BabyDollApplication::OnShutdown() noexcept
{
	if (engine) { engine->SetCursorVisible(true); engine->SetCursorConfined(false); }
	if (rearView) { rearView->Clear(); }
	if (world) { world->DestroyPendingGameObjects(); }
	rearView = nullptr;
	caughtCamera = nullptr;
	doll = nullptr;
	traversal=nullptr; inventory=nullptr; objectSounds=nullptr; exitDoor=nullptr; stitchVisual=nullptr; stitchVisualObject=nullptr;
	caughtLight = nullptr;
	monsterFrontLight = nullptr;
	monsterFrontFill = nullptr;
	playerFillLight = nullptr;
	lamps.clear();
	sceneAmbient=nullptr; sceneFill=nullptr; reviewRoofs.clear();
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
