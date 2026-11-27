#include "World.h"

#include "Component/BoxColliderComponent.h"
#include "Component/RigidBodyComponent.h"
#include "Core/Log.h"
#include "Core/Math/Transform.h"
#include "GameObject.h"
#include "ModelPrefab.h"
#include "Prefab.h"
#include "PrefabComponents.h"

#include <format>
#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
	// 프리팹 인스턴스 하나를 만드는 동안 모은다. 계층을 모두 만든 뒤 animation 대상을 연결하는 데 쓴다.
	struct PrefabInstanceLinks
	{
		// 이 인스턴스 안의 객체 id별 객체다. 비소유이며 World가 소유한다.
		std::unordered_map<std::string, GameObject*> objectsByID;
		// animation 컴포넌트를 붙일 객체와 그 설정이다. 설정은 프리팹 에셋이 소유한다.
		std::vector<std::pair<GameObject*, const PrefabAnimation*>> animations;
		// skin 컴포넌트를 붙일 객체와 그 설정이다. 포즈를 읽을 animation이 먼저 붙어야 해서 그 뒤에 붙인다.
		std::vector<std::pair<GameObject*, const PrefabSkin*>> skins;
	};

	// object는 월드에 이미 등록되어 있고 삭제 예정이 아니어야 한다.
	// 컴포넌트 설정이나 자식 생성에 실패하면 false를 반환한다. 이미 만든 자식은 object 아래에 연결되어 있다.
	bool ApplyPrefabObject(GameObject& object, const PrefabObject& data, PrefabInstanceLinks& links)
	{
		if (data.id.has_value())
		{
			links.objectsByID.emplace(*data.id, &object);
		}

		if (data.staticMesh.has_value()) { AttachPrefabStaticMesh(object, *data.staticMesh); }

		if (data.skybox.has_value()) { AttachPrefabSkybox(object, *data.skybox); }

		if (data.light.has_value()) { AttachPrefabLight(object, *data.light); }

		if (data.camera.has_value()) { AttachPrefabCamera(object, *data.camera); }

		if (data.boxCollider.has_value())
		{
			std::unique_ptr<BoxColliderComponent> collider = std::make_unique<BoxColliderComponent>();
			if (!collider->SetLocalBox(data.boxCollider->centerCm, data.boxCollider->halfExtentsCm))
			{
				Log::Write(LogLevel::Error, "Prefab", std::format("{}: invalid boxCollider settings", object.GetName()));
				return false;
			}
			object.AddComponent(std::move(collider));
		}

		if (data.rigidBody.has_value())
		{
			std::unique_ptr<RigidBodyComponent> rigidBody = std::make_unique<RigidBodyComponent>();
			rigidBody->SetSimulatePhysics(data.rigidBody->simulatePhysics);
			object.AddComponent(std::move(rigidBody));
		}

		// 대상 객체가 아직 없을 수 있어 계층을 모두 만든 뒤 붙인다.
		if (data.animation.has_value())
		{
			links.animations.emplace_back(&object, &*data.animation);
		}

		if (data.skin.has_value())
		{
			links.skins.emplace_back(&object, &*data.skin);
		}

		for (const PrefabObject& child : data.children)
		{
			GameObject* childObject = object.GetWorld().CreateGameObject(child.name, child.localTransform);
			if (childObject == nullptr) { return false; }

			// SetParent는 월드에 등록된 객체끼리만 연결할 수 있다.
			childObject->SetParent(&object);
			if (!ApplyPrefabObject(*childObject, child, links)) { return false; }
		}
		return true;
	}
}

GameObject* World::InstantiatePrefab(const Prefab& prefab, const PrefabPlacement& placement)
{
	const std::optional<std::string>& name = placement.name.has_value() ? placement.name : prefab.root.name;
	const Transform& transform = placement.transform.has_value() ? *placement.transform : prefab.root.localTransform;
	GameObject* root = CreateGameObject(name, transform);
	if (root == nullptr) { return nullptr; }

	// id 조회표는 인스턴스마다 따로 만든다. 같은 프리팹을 여러 번 놓아도 각 인스턴스 안에서만 연결된다.
	PrefabInstanceLinks links;
	if (!ApplyPrefabObject(*root, prefab.root, links))
	{
		// 이미 만든 객체는 모두 루트 아래에 있어 함께 삭제 예정이 된다.
		DestroyGameObject(root);
		return nullptr;
	}
	for (const auto& [object, animation] : links.animations)
	{
		AttachPrefabAnimation(*object, *animation, links.objectsByID);
	}
	for (const auto& [object, skin] : links.skins)
	{
		AttachPrefabSkin(*object, *skin, links.animations);
	}

	// 최상위 model이 있으면 그 노드 계층을 루트 아래에 펼친다.
	if (prefab.model != nullptr && !BuildModelHierarchy(*root, prefab.model, prefab.meshMaterialOverrides))
	{
		DestroyGameObject(root);
		return nullptr;
	}
	return root;
}

std::optional<PrefabObjects> ConvertLegacyPrefab(const Prefab& prefab)
{
	// 옛 방식으로 놓아 보고 찍기만 하는 World다. 그리지 않으며 함수가 끝나면 객체와 함께 사라진다.
	World builder;
	GameObject* root = builder.InstantiatePrefab(prefab, PrefabPlacement{});
	if (root == nullptr)
	{
		Log::Write(LogLevel::Error, "Prefab", "could not build the legacy prefab");
		return std::nullopt;
	}
	return CreatePrefabFromWorld(builder, *root);
}
