#include "SceneSnapshot.h"

#include "Component/AmbientLightComponent.h"
#include "Component/AnimationComponent.h"
#include "Component/CameraComponent.h"
#include "Component/Component.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/LightComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/SkinComponent.h"
#include "Component/SkyboxComponent.h"
#include "Component/SpotLightComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "GameObject.h"
#include "World.h"

#include <format>
#include <memory>
#include <typeinfo>
#include <unordered_set>
#include <utility>
#include <vector>

namespace
{
	// 컴포넌트를 가진 객체의 ID다. nullptr면 무효 ID다.
	ObjectID GetOwnerID(const Component* component)
	{
		if (component == nullptr || component->GetGameObject() == nullptr) { return ObjectID{}; }
		return component->GetGameObject()->GetObjectID();
	}

	AnimationSnapshot CaptureAnimation(const AnimationComponent& animation)
	{
		AnimationSnapshot data;
		data.nodeObjectIDs.reserve(animation.GetNodeTransforms().size());
		for (const TransformComponent* node : animation.GetNodeTransforms())
		{
			data.nodeObjectIDs.push_back(GetOwnerID(node));
		}
		data.restPose = animation.GetRestPose();
		return data;
	}

	// 노드 객체는 world에 모두 등록되어 있어야 한다. 노드 객체를 찾지 못하면 nullptr를 반환한다.
	std::unique_ptr<AnimationComponent> CreateAnimation(const AnimationSnapshot& animation, World& world)
	{
		std::vector<TransformComponent*> nodeTransforms;
		nodeTransforms.reserve(animation.nodeObjectIDs.size());
		for (const ObjectID& nodeObjectID : animation.nodeObjectIDs)
		{
			if (!nodeObjectID.IsValid())
			{
				nodeTransforms.push_back(nullptr);
				continue;
			}
			GameObject* node = world.FindGameObject(nodeObjectID);
			if (node == nullptr) { return nullptr; }
			nodeTransforms.push_back(node->GetComponent<TransformComponent>());
		}

		std::unique_ptr<AnimationComponent> component = std::make_unique<AnimationComponent>();
		component->SetNodeTransforms(std::move(nodeTransforms));
		component->SetRestPose(animation.restPose);
		return component;
	}

	SkinSnapshot CaptureSkin(const SkinComponent& skin)
	{
		return SkinSnapshot{ .model = skin.GetModel(), .nodeIndex = skin.GetNodeIndex(), .animationObjectID = GetOwnerID(skin.GetAnimation()) };
	}

	// Animation을 모두 붙인 뒤 호출한다. 가리키는 객체에 Animation이 없으면 nullptr를 반환한다.
	std::unique_ptr<SkinComponent> CreateSkin(const SkinSnapshot& skin, World& world)
	{
		const AnimationComponent* animation = nullptr;
		if (skin.animationObjectID.IsValid())
		{
			GameObject* owner = world.FindGameObject(skin.animationObjectID);
			animation = owner != nullptr ? owner->GetComponent<AnimationComponent>() : nullptr;
			if (animation == nullptr) { return nullptr; }
		}

		std::unique_ptr<SkinComponent> component = std::make_unique<SkinComponent>();
		component->SetSkin(skin.model, skin.nodeIndex, animation);
		return component;
	}

	// 참조 대상이 스냅샷 안에 있는지 확인한다. 삭제 예정이라 담지 않은 객체를 가리키면 false를 반환한다.
	bool ValidateReferences(const SceneSnapshot& snapshot)
	{
		std::unordered_set<ObjectID> objectIDs;
		std::unordered_set<ObjectID> animationOwnerIDs;
		for (const GameObjectSnapshot& data : snapshot.objects)
		{
			objectIDs.insert(data.objectID);
			if (data.animation.has_value()) { animationOwnerIDs.insert(data.objectID); }
		}

		for (const GameObjectSnapshot& data : snapshot.objects)
		{
			if (data.animation.has_value())
			{
				for (const ObjectID& nodeObjectID : data.animation->nodeObjectIDs)
				{
					if (nodeObjectID.IsValid() && !objectIDs.contains(nodeObjectID))
					{
						Log::Write(LogLevel::Error, "Snapshot", std::format("{}: animation node {} is not in the snapshot", data.name, nodeObjectID.ToString()));
						return false;
					}
				}
			}
			if (data.skin.has_value() && data.skin->animationObjectID.IsValid() && !animationOwnerIDs.contains(data.skin->animationObjectID))
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: skin animation {} is not in the snapshot", data.name, data.skin->animationObjectID.ToString()));
				return false;
			}
		}
		return true;
	}

	StaticMeshSnapshot CaptureStaticMesh(const StaticMeshComponent& mesh)
	{
		return StaticMeshSnapshot{ .mesh = mesh.GetStaticMesh(), .material = mesh.GetMaterial() };
	}

	// 재질 값이 허용 범위를 벗어나면 nullptr를 반환한다.
	std::unique_ptr<StaticMeshComponent> CreateStaticMesh(const StaticMeshSnapshot& mesh)
	{
		std::unique_ptr<StaticMeshComponent> component = std::make_unique<StaticMeshComponent>();
		component->SetStaticMesh(mesh.mesh);
		if (!component->SetMaterial(mesh.material)) { return nullptr; }
		return component;
	}

	SkyboxSnapshot CaptureSkybox(const SkyboxComponent& skybox)
	{
		return SkyboxSnapshot{ .faces = skybox.GetFaces(), .enabled = skybox.IsEnabled() };
	}

	std::unique_ptr<SkyboxComponent> CreateSkybox(const SkyboxSnapshot& skybox)
	{
		std::unique_ptr<SkyboxComponent> component = std::make_unique<SkyboxComponent>();
		component->SetFaces(skybox.faces);
		component->SetEnabled(skybox.enabled);
		return component;
	}

	// 조명 4종 중 하나면 값을 담아 반환하고, 아니면 nullopt를 반환한다.
	std::optional<LightSnapshot> CaptureLight(const Component& component)
	{
		LightSnapshot light{};
		const std::type_info& type = typeid(component);
		if (type == typeid(PointLightComponent))
		{
			light.type = LightSnapshotType::Point;
			light.rangeCm = static_cast<const PointLightComponent&>(component).GetRange();
		}
		else if (type == typeid(SpotLightComponent))
		{
			const SpotLightComponent& spotLight = static_cast<const SpotLightComponent&>(component);
			light.type = LightSnapshotType::Spot;
			light.rangeCm = spotLight.GetRange();
			light.outerConeAngleDegrees = spotLight.GetOuterConeAngleDegrees();
		}
		else if (type == typeid(DirectionalLightComponent)) { light.type = LightSnapshotType::Directional; }
		else if (type == typeid(AmbientLightComponent)) { light.type = LightSnapshotType::Ambient; }
		else { return std::nullopt; }

		const LightComponent& common = static_cast<const LightComponent&>(component);
		light.color = common.GetColor();
		light.intensity = common.GetIntensity();
		light.enabled = common.IsEnabled();
		return light;
	}

	// 값이 컴포넌트의 허용 범위를 벗어나면 nullptr를 반환한다.
	std::unique_ptr<LightComponent> CreateLight(const LightSnapshot& light)
	{
		std::unique_ptr<LightComponent> component;
		switch (light.type)
		{
		case LightSnapshotType::Point:
		{
			std::unique_ptr<PointLightComponent> pointLight = std::make_unique<PointLightComponent>();
			if (!pointLight->SetRange(light.rangeCm)) { return nullptr; }
			component = std::move(pointLight);
			break;
		}
		case LightSnapshotType::Spot:
		{
			std::unique_ptr<SpotLightComponent> spotLight = std::make_unique<SpotLightComponent>();
			if (!spotLight->SetRange(light.rangeCm) || !spotLight->SetOuterConeAngleDegrees(light.outerConeAngleDegrees)) { return nullptr; }
			component = std::move(spotLight);
			break;
		}
		case LightSnapshotType::Directional:
			component = std::make_unique<DirectionalLightComponent>();
			break;
		case LightSnapshotType::Ambient:
			component = std::make_unique<AmbientLightComponent>();
			break;
		default:
			return nullptr;
		}

		if (!component->SetColor(light.color) || !component->SetIntensity(light.intensity)) { return nullptr; }
		component->SetEnabled(light.enabled);
		return component;
	}

	CameraSnapshot CaptureCamera(const CameraComponent& camera)
	{
		return CameraSnapshot{
			.projectionMode = camera.GetProjectionMode(),
			.verticalFovDegrees = camera.GetVerticalFovDegrees(),
			.orthographicHeight = camera.GetOrthographicHeight(),
			.nearClipDistance = camera.GetNearClipDistance(),
			.farClipDistance = camera.GetFarClipDistance(),
		};
	}

	// 값이 컴포넌트의 허용 범위를 벗어나면 nullptr를 반환한다.
	std::unique_ptr<CameraComponent> CreateCamera(const CameraSnapshot& camera)
	{
		std::unique_ptr<CameraComponent> component = std::make_unique<CameraComponent>();
		component->SetProjectionMode(camera.projectionMode);
		if (!component->SetVerticalFovDegrees(camera.verticalFovDegrees)
			|| !component->SetOrthographicHeight(camera.orthographicHeight)
			|| !component->SetClipDistances(camera.nearClipDistance, camera.farClipDistance))
		{
			return nullptr;
		}
		return component;
	}

	// object와 자손을 부모 먼저 순서로 담는다. 지원하지 않는 타입을 만나면 false를 반환한다.
	bool CaptureGameObject(const GameObject& object, const ObjectID& parentID, SceneSnapshot& snapshot)
	{
		// 파생 타입은 생성자가 붙이는 컴포넌트와 자체 상태가 있어 기본 객체로 바꿔 담지 않는다.
		if (typeid(object) != typeid(GameObject))
		{
			Log::Write(LogLevel::Error, "Snapshot", std::format("{}: derived GameObject type is not supported", object.GetName()));
			return false;
		}

		GameObjectSnapshot data{ .objectID = object.GetObjectID(), .name = object.GetName(), .parentID = parentID };
		bool hasTransform = false;
		for (const std::unique_ptr<Component>& component : object.GetComponents())
		{
			// 컴포넌트 종류별 저장은 여기에 모은다. 이후 단계에서 종류마다 한 칸씩 추가한다.
			if (typeid(*component) == typeid(TransformComponent))
			{
				if (hasTransform)
				{
					Log::Write(LogLevel::Error, "Snapshot", std::format("{}: duplicate TransformComponent", object.GetName()));
					return false;
				}
				hasTransform = true;
				data.localTransform = static_cast<const TransformComponent&>(*component).GetTransform();
				continue;
			}
			if (typeid(*component) == typeid(StaticMeshComponent))
			{
				if (data.staticMesh.has_value())
				{
					Log::Write(LogLevel::Error, "Snapshot", std::format("{}: duplicate StaticMeshComponent", object.GetName()));
					return false;
				}
				data.staticMesh = CaptureStaticMesh(static_cast<const StaticMeshComponent&>(*component));
				continue;
			}
			if (typeid(*component) == typeid(SkyboxComponent))
			{
				if (data.skybox.has_value())
				{
					Log::Write(LogLevel::Error, "Snapshot", std::format("{}: duplicate SkyboxComponent", object.GetName()));
					return false;
				}
				data.skybox = CaptureSkybox(static_cast<const SkyboxComponent&>(*component));
				continue;
			}
			if (std::optional<LightSnapshot> light = CaptureLight(*component))
			{
				if (data.light.has_value())
				{
					Log::Write(LogLevel::Error, "Snapshot", std::format("{}: duplicate light component", object.GetName()));
					return false;
				}
				data.light = *light;
				continue;
			}
			if (typeid(*component) == typeid(CameraComponent))
			{
				if (data.camera.has_value())
				{
					Log::Write(LogLevel::Error, "Snapshot", std::format("{}: duplicate CameraComponent", object.GetName()));
					return false;
				}
				data.camera = CaptureCamera(static_cast<const CameraComponent&>(*component));
				continue;
			}
			if (typeid(*component) == typeid(AnimationComponent))
			{
				if (data.animation.has_value())
				{
					Log::Write(LogLevel::Error, "Snapshot", std::format("{}: duplicate AnimationComponent", object.GetName()));
					return false;
				}
				data.animation = CaptureAnimation(static_cast<const AnimationComponent&>(*component));
				continue;
			}
			if (typeid(*component) == typeid(SkinComponent))
			{
				if (data.skin.has_value())
				{
					Log::Write(LogLevel::Error, "Snapshot", std::format("{}: duplicate SkinComponent", object.GetName()));
					return false;
				}
				data.skin = CaptureSkin(static_cast<const SkinComponent&>(*component));
				continue;
			}

			Log::Write(LogLevel::Error, "Snapshot", std::format("{}: unsupported component {}", object.GetName(), typeid(*component).name()));
			return false;
		}
		snapshot.objects.push_back(std::move(data));

		for (const GameObject* child : object.GetChildren())
		{
			if (child->IsPendingDestroy()) { continue; }
			if (!CaptureGameObject(*child, object.GetObjectID(), snapshot)) { return false; }
		}
		return true;
	}
}

std::optional<SceneSnapshot> CaptureWorld(const World& world)
{
	SceneSnapshot snapshot;
	for (const std::unique_ptr<GameObject>& object : world.GetGameObjects())
	{
		// 자식은 부모를 담을 때 함께 담는다.
		if (object->IsPendingDestroy() || object->GetParent() != nullptr) { continue; }
		if (!CaptureGameObject(*object, ObjectID{}, snapshot)) { return std::nullopt; }
	}
	if (!ValidateReferences(snapshot)) { return std::nullopt; }

	if (const CameraComponent* camera = world.GetActiveCamera())
	{
		const GameObject* owner = camera->GetGameObject();
		// 삭제 예정 객체는 담지 않으므로 그 카메라도 활성으로 담지 않는다.
		if (!owner->IsPendingDestroy()) { snapshot.activeCameraObjectID = owner->GetObjectID(); }
	}
	return snapshot;
}

bool RestoreWorld(const SceneSnapshot& snapshot, World& world)
{
	if (!world.GetGameObjects().empty())
	{
		Log::Write(LogLevel::Error, "Snapshot", "Restore target world is not empty");
		return false;
	}

	// 1) 모든 객체를 만들고 저장된 ID로 등록한다. 계층 연결은 등록된 객체끼리만 가능하다.
	for (const GameObjectSnapshot& data : snapshot.objects)
	{
		std::unique_ptr<GameObject> object = std::make_unique<GameObject>(world);
		object->SetName(data.name);
		// 생성자가 붙인 TransformComponent에 값을 채운다.
		object->GetComponent<TransformComponent>()->SetTransform(data.localTransform);
		if (world.AddGameObject(std::move(object), data.objectID) == nullptr)
		{
			Log::Write(LogLevel::Error, "Snapshot", std::format("{}: could not register ObjectID {}", data.name, data.objectID.ToString()));
			return false;
		}
	}

	// 2) 부모가 먼저 나오므로 배열 순서대로 연결하면 자식 순서도 유지된다.
	for (const GameObjectSnapshot& data : snapshot.objects)
	{
		if (!data.parentID.IsValid()) { continue; }

		GameObject* parent = world.FindGameObject(data.parentID);
		if (parent == nullptr || !world.FindGameObject(data.objectID)->SetParent(parent))
		{
			Log::Write(LogLevel::Error, "Snapshot", std::format("{}: could not attach to parent {}", data.name, data.parentID.ToString()));
			return false;
		}
	}

	// 3) 컴포넌트를 붙인다. 순서는 프리팹과 같이 StaticMesh, Skybox, 조명, 카메라 순으로 고정한다.
	for (const GameObjectSnapshot& data : snapshot.objects)
	{
		GameObject& object = *world.FindGameObject(data.objectID);
		if (data.staticMesh.has_value())
		{
			std::unique_ptr<StaticMeshComponent> mesh = CreateStaticMesh(*data.staticMesh);
			if (mesh == nullptr)
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: invalid material values", data.name));
				return false;
			}
			object.AddComponent(std::move(mesh));
		}
		if (data.skybox.has_value())
		{
			object.AddComponent(CreateSkybox(*data.skybox));
		}
		if (data.light.has_value())
		{
			std::unique_ptr<LightComponent> light = CreateLight(*data.light);
			if (light == nullptr)
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: invalid light values", data.name));
				return false;
			}
			object.AddComponent(std::move(light));
		}
		if (data.camera.has_value())
		{
			std::unique_ptr<CameraComponent> camera = CreateCamera(*data.camera);
			if (camera == nullptr)
			{
				Log::Write(LogLevel::Error, "Snapshot", std::format("{}: invalid camera values", data.name));
				return false;
			}
			object.AddComponent(std::move(camera));
		}
	}

	// 다른 객체를 가리키는 컴포넌트는 프리팹과 같이 Animation, Skin 순으로 붙인다. Skin이 가리키는 Animation이 먼저 있어야 한다.
	for (const GameObjectSnapshot& data : snapshot.objects)
	{
		if (!data.animation.has_value()) { continue; }

		std::unique_ptr<AnimationComponent> animation = CreateAnimation(*data.animation, world);
		if (animation == nullptr)
		{
			Log::Write(LogLevel::Error, "Snapshot", std::format("{}: animation node object was not found", data.name));
			return false;
		}
		world.FindGameObject(data.objectID)->AddComponent(std::move(animation));
	}
	for (const GameObjectSnapshot& data : snapshot.objects)
	{
		if (!data.skin.has_value()) { continue; }

		std::unique_ptr<SkinComponent> skin = CreateSkin(*data.skin, world);
		if (skin == nullptr)
		{
			Log::Write(LogLevel::Error, "Snapshot", std::format("{}: skin animation object was not found", data.name));
			return false;
		}
		world.FindGameObject(data.objectID)->AddComponent(std::move(skin));
	}

	// 4) World 상태
	if (snapshot.activeCameraObjectID.IsValid())
	{
		GameObject* owner = world.FindGameObject(snapshot.activeCameraObjectID);
		CameraComponent* camera = owner != nullptr ? owner->GetComponent<CameraComponent>() : nullptr;
		if (camera == nullptr || !world.SetActiveCamera(camera))
		{
			Log::Write(LogLevel::Error, "Snapshot", std::format("could not activate camera of {}", snapshot.activeCameraObjectID.ToString()));
			return false;
		}
	}
	return true;
}
