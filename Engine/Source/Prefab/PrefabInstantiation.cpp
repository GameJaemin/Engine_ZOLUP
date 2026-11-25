#include "World.h"

#include "AssetManager.h"
#include "Component/AnimationComponent.h"
#include "Component/BoxColliderComponent.h"
#include "Component/RigidBodyComponent.h"
#include "Component/SkinComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "Core/Math/Transform.h"
#include "GLTFLoader.h"
#include "GameObject.h"
#include "Mesh/Skin.h"
#include "Prefab.h"
#include "PrefabComponents.h"

#include <cstddef>
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

	// 프리팹 최상위 model의 노드 계층을 root 아래에 생성한다. 모델의 인덱스와 계층은 로더가 검증한다.
	// 노드 객체 생성에 실패하면 이번에 만든 노드를 삭제 예정으로 표시하고 false를 반환한다.
	bool InstantiatePrefabModelHierarchy(GameObject& root, const Prefab& prefab)
	{
		if (prefab.model == nullptr) { return true; }

		World& world = root.GetWorld();
		const Model& model = *prefab.model;

		// 뼈 노드는 GameObject가 아니라 포즈 배열의 칸으로 다룬다. 뼈 아래의 노드도 연결할 부모 GameObject가 없어 만들지 않는다.
		std::vector<bool> isJoint(model.sceneNodes.size(), false);
		for (const std::shared_ptr<const Skin>& skin : model.skins)
		{
			for (const std::size_t joint : skin->joints) { isJoint[joint] = true; }
		}

		std::vector<bool> skipsObject(model.sceneNodes.size(), false);
		std::size_t skippedNonJointCount = 0;
		for (std::size_t i = 0; i < model.sceneNodes.size(); ++i)
		{
			// 자신이나 조상 중에 뼈가 있으면 만들지 않는다.
			std::optional<std::size_t> current = i;
			while (current.has_value() && !isJoint[*current]) { current = model.sceneNodes[*current].parent; }
			skipsObject[i] = current.has_value();
			if (skipsObject[i] && !isJoint[i]) { ++skippedNonJointCount; }
		}
		if (skippedNonJointCount > 0)
		{
			Log::Write(LogLevel::Warning, "Level", std::format("{} nodes under skin joints are not created as GameObjects", skippedNonJointCount));
		}

		std::vector<GameObject*> nodeObjects;
		nodeObjects.reserve(model.sceneNodes.size());

		for (std::size_t i = 0; i < model.sceneNodes.size(); ++i)
		{
			const GLTFSceneNode& sceneNode = model.sceneNodes[i];
			if (skipsObject[i])
			{
				// 노드 번호와 칸 번호를 맞추려고 GameObject가 없는 칸도 남긴다.
				nodeObjects.push_back(nullptr);
				continue;
			}

			std::unique_ptr<GameObject> newNodeObject = std::make_unique<GameObject>(world);
			newNodeObject->GetComponent<TransformComponent>()->SetTransform(sceneNode.localTransform);

			if (sceneNode.mesh.has_value())
			{
				const std::size_t meshIndex = *sceneNode.mesh;
				std::unique_ptr<StaticMeshComponent> mesh = std::make_unique<StaticMeshComponent>();
				mesh->SetStaticMesh(model.meshes[meshIndex]);

				std::shared_ptr<const Material> material = model.meshMaterials[meshIndex];
				const std::vector<std::shared_ptr<const Material>>& overrides = prefab.meshMaterialOverrides;
				if (!overrides.empty() && overrides[meshIndex] != nullptr)
				{
					material = overrides[meshIndex];
				}
				mesh->SetMaterial(std::move(material));
				newNodeObject->AddComponent(std::move(mesh));
			}

			GameObject* nodeObject = world.AddGameObject(std::move(newNodeObject));
			if (nodeObject == nullptr)
			{
				// 아직 루트 아래에 연결하기 전이라 루트 삭제로 함께 지워지지 않는다.
				for (GameObject* createdObject : nodeObjects)
				{
					if (createdObject != nullptr) { world.DestroyGameObject(createdObject); }
				}
				return false;
			}
			nodeObjects.push_back(nodeObject);
		}

		// SetParent는 월드에 등록된 객체끼리만 연결할 수 있다.
		for (std::size_t i = 0; i < model.sceneNodes.size(); ++i)
		{
			// GameObject를 만든 노드의 부모 노드에는 항상 GameObject가 있다. 부모가 뼈였다면 이 노드도 건너뛰었다.
			if (nodeObjects[i] == nullptr) { continue; }

			const std::optional<std::size_t>& parentIndex = model.sceneNodes[i].parent;
			GameObject* parent = parentIndex.has_value() ? nodeObjects[*parentIndex] : &root;
			nodeObjects[i]->SetParent(parent);
		}

		// 클립이 있으면 재생 컴포넌트를 붙이고 노드 연결까지만 한다. 무엇을 재생할지는 게임 코드가 Play로 정한다.
		// 스킨 연결이 포즈를 읽도록 주소를 남긴다. 클립이 없으면 nullptr이며 스킨은 원래 값을 쓴다.
		const AnimationComponent* animation = nullptr;
		if (!model.animations.empty())
		{
			std::vector<TransformComponent*> nodeTransforms;
			nodeTransforms.reserve(nodeObjects.size());
			for (GameObject* nodeObject : nodeObjects)
			{
				// GameObject가 없는 노드(뼈)는 nullptr로 둔다. 재생 컴포넌트는 그 노드 값을 포즈에만 적는다.
				nodeTransforms.push_back(nodeObject != nullptr ? nodeObject->GetComponent<TransformComponent>() : nullptr);
			}

			std::vector<Transform> restPose;
			restPose.reserve(model.sceneNodes.size());
			for (const GLTFSceneNode& sceneNode : model.sceneNodes)
			{
				restPose.push_back(sceneNode.localTransform);
			}

			std::unique_ptr<AnimationComponent> animationComponent = std::make_unique<AnimationComponent>();
			animationComponent->SetNodeTransforms(std::move(nodeTransforms));
			animationComponent->SetRestPose(std::move(restPose));
			animation = animationComponent.get();
			root.AddComponent(std::move(animationComponent));
		}

		// 스킨이 붙은 메시 노드의 GameObject에 스킨 연결을 붙인다.
		for (std::size_t i = 0; i < model.sceneNodes.size(); ++i)
		{
			if (nodeObjects[i] == nullptr || !model.sceneNodes[i].skin.has_value()) { continue; }

			std::unique_ptr<SkinComponent> skin = std::make_unique<SkinComponent>();
			skin->SetSkin(prefab.model, i, animation);
			nodeObjects[i]->AddComponent(std::move(skin));
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

	if (!InstantiatePrefabModelHierarchy(*root, prefab))
	{
		DestroyGameObject(root);
		return nullptr;
	}
	return root;
}
