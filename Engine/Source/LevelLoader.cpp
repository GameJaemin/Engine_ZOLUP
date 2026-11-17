#include "LevelLoader.h"

#include "AssetManager.h"
#include "Component/AnimationComponent.h"
#include "Component/SkinComponent.h"
#include "Component/SkyboxComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/FileHelper.h"
#include "Core/Log.h"
#include "Core/Math/Transform.h"
#include "GameObject.h"
#include "Mesh/Skin.h"
#include "Prefab.h"
#include "World.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <filesystem>
#include <format>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	bool IsFloatArray(const rapidjson::Value& value, rapidjson::SizeType size)
	{
		if (!value.IsArray() || value.Size() != size) { return false; }

		// Transform은 float를 사용하므로 변환 전에 double로 읽어 범위를 확인한다.
		constexpr double MaxFloat = (std::numeric_limits<float>::max)();
		for (const rapidjson::Value& entry : value.GetArray())
		{
			if (!entry.IsNumber()) { return false; }

			const double number = entry.GetDouble();
			if (!std::isfinite(number) || std::abs(number) > MaxFloat) { return false; }
		}

		return true;
	}

	// 배열 인덱스 뒤에 붙일 오류를 반환한다. 빈 문자열이면 항목이 유효하다.
	std::string_view ValidateObject(const rapidjson::Value& object)
	{
		if (!object.IsObject()) { return " must be an object"; }

		const auto name = object.FindMember("name");
		if (name == object.MemberEnd() || !name->value.IsString()) { return ".name must be a string"; }

		const auto transform = object.FindMember("transform");
		if (transform == object.MemberEnd() || !transform->value.IsObject()) { return ".transform must be an object"; }

		const rapidjson::Value& fields = transform->value;
		const auto position = fields.FindMember("position");
		if (position == fields.MemberEnd() || !IsFloatArray(position->value, 3)) { return ".transform.position must contain three finite floats"; }

		const auto rotation = fields.FindMember("rotation");
		if (rotation == fields.MemberEnd() || !IsFloatArray(rotation->value, 4)) { return ".transform.rotation must contain four finite floats"; }

		// Transform은 단위 쿼터니언을 요구한다. 손으로 입력한 값의 반올림 오차는 허용한다.
		constexpr double UnitLengthTolerance = 0.0001;
		double lengthSquared = 0.0;
		for (const rapidjson::Value& entry : rotation->value.GetArray())
		{
			const double component = entry.GetDouble();
			lengthSquared += component * component;
		}
		if (std::abs(std::sqrt(lengthSquared) - 1.0) > UnitLengthTolerance) { return ".transform.rotation must be a unit quaternion"; }

		const auto scale = fields.FindMember("scale");
		if (scale == fields.MemberEnd() || !IsFloatArray(scale->value, 3)) { return ".transform.scale must contain three finite floats"; }

		const auto prefab = object.FindMember("prefab");
		if (prefab != object.MemberEnd())
		{
			if (!prefab->value.IsString()) { return ".prefab must be a string"; }

			const std::string_view prefabPath(prefab->value.GetString(), prefab->value.GetStringLength());
			if (prefabPath.empty() || prefabPath.find('\0') != std::string_view::npos)
			{
				return ".prefab must be non-empty and contain no NUL";
			}

			const std::filesystem::path path(std::u8string(prefabPath.begin(), prefabPath.end()));
			const std::filesystem::path normalizedPath = path.lexically_normal();
			const bool goesOutsideRoot = normalizedPath.begin() != normalizedPath.end() && *normalizedPath.begin() == "..";
			if (path.has_root_path() || goesOutsideRoot) { return ".prefab must be a repository-relative path"; }
		}

		return {};
	}

	// root는 같은 월드에 등록되어 있어야 한다. 모델의 인덱스와 계층은 로더가 검증한다.
	void ApplyPrefab(GameObject& root, const Prefab& prefab)
	{
		if (prefab.skybox.has_value())
		{
			const PrefabSkybox& source = *prefab.skybox;
			std::unique_ptr<SkyboxComponent> skybox = std::make_unique<SkyboxComponent>();
			skybox->SetFaces(source.faces);
			skybox->SetEnabled(source.enabled);
			root.AddComponent(std::move(skybox));
		}

		if (prefab.model == nullptr) { return; }

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

			std::unique_ptr<GameObject> nodeObject = std::make_unique<GameObject>(world);
			nodeObject->GetComponent<TransformComponent>()->SetTransform(sceneNode.localTransform);

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
				nodeObject->AddComponent(std::move(mesh));
			}

			nodeObjects.push_back(world.AddGameObject(std::move(nodeObject)));
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
	}
}

std::optional<std::vector<GameObject*>> LoadLevel(const std::filesystem::path& path, World& world)
{
	const std::u8string utf8Path = path.u8string();
	const std::string_view pathText(reinterpret_cast<const char*>(utf8Path.data()), utf8Path.size());

	// ----------
	// 파일 읽기와 JSON 파싱
	// ----------
	std::vector<std::byte> bytes;
	if (!FileHelper::ReadAllBytes(path, bytes))
	{
		Log::Write(LogLevel::Error, "Level", std::format("{}: could not read file", pathText));
		return std::nullopt;
	}

	// 원시 NUL 바이트가 있으면 파서가 그 위치를 입력 끝으로 볼 수 있어 미리 거부한다.
	if (bytes.empty() || std::ranges::find(bytes, std::byte{0}) != bytes.end())
	{
		Log::Write(LogLevel::Error, "Level", std::format("{}: file is empty or contains a raw NUL byte", pathText));
		return std::nullopt;
	}

	rapidjson::Document document;
	document.Parse<rapidjson::kParseValidateEncodingFlag>(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	if (document.HasParseError())
	{
		Log::Write(LogLevel::Error, "Level", std::format("{}: invalid JSON at byte {}: {}", pathText, document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError())));
		return std::nullopt;
	}

	// ----------
	// 맵 구조와 객체 데이터 검증
	// ----------
	if (!document.IsObject())
	{
		Log::Write(LogLevel::Error, "Level", std::format("{}: JSON root must be an object", pathText));
		return std::nullopt;
	}

	const auto objects = document.FindMember("objects");
	if (objects == document.MemberEnd() || !objects->value.IsArray())
	{
		Log::Write(LogLevel::Error, "Level", std::format("{}: objects must be an array", pathText));
		return std::nullopt;
	}

	// 뒤쪽 항목의 검증 실패로 일부 객체만 등록되는 일을 막기 위해 배열 전체를 먼저 검증한다.
	const rapidjson::Value::Array entries = objects->value.GetArray();
	for (rapidjson::SizeType i = 0; i < entries.Size(); ++i)
	{
		const std::string_view error = ValidateObject(entries[i]);
		if (!error.empty())
		{
			Log::Write(LogLevel::Error, "Level", std::format("{}: objects[{}]{}", pathText, i, error));
			return std::nullopt;
		}
	}

	// ----------
	// 참조된 프리팹 로드
	// ----------
	std::vector<std::shared_ptr<const Prefab>> prefabs(entries.Size());
	for (rapidjson::SizeType i = 0; i < entries.Size(); ++i)
	{
		const rapidjson::Value& entry = entries[i];
		const auto prefab = entry.FindMember("prefab");
		if (prefab == entry.MemberEnd()) { continue; }

		const std::string_view prefabPath(prefab->value.GetString(), prefab->value.GetStringLength());
		prefabs[i] = AssetManager::Get().LoadPrefab(prefabPath);
		if (prefabs[i] == nullptr)
		{
			Log::Write(LogLevel::Error, "Level", std::format("{}: objects[{}].prefab could not load {}", pathText, i, prefabPath));
			return std::nullopt;
		}
	}

	// ----------
	// 월드에 루트 객체 복원
	// ----------
	std::vector<GameObject*> loadedObjects;
	loadedObjects.reserve(entries.Size());
	for (rapidjson::SizeType i = 0; i < entries.Size(); ++i)
	{
		const rapidjson::Value& entry = entries[i];
		// 위에서 필수 필드의 타입, 배열 길이와 숫자 범위를 검사했다.
		const rapidjson::Value& name = entry["name"];
		const rapidjson::Value& fields = entry["transform"];
		const rapidjson::Value::ConstArray position = fields["position"].GetArray();
		const rapidjson::Value::ConstArray rotation = fields["rotation"].GetArray();
		const rapidjson::Value::ConstArray scale = fields["scale"].GetArray();
		const Transform transform{
			.position = { position[0].GetFloat(), position[1].GetFloat(), position[2].GetFloat() },
			.rotation = { rotation[0].GetFloat(), rotation[1].GetFloat(), rotation[2].GetFloat(), rotation[3].GetFloat() },
			.scale = { scale[0].GetFloat(), scale[1].GetFloat(), scale[2].GetFloat() }
		};

		std::unique_ptr<GameObject> object = std::make_unique<GameObject>(world);
		object->SetName(std::string(name.GetString(), name.GetStringLength()));
		object->GetComponent<TransformComponent>()->SetTransform(transform);
		// World가 소유권을 가져가고 호출부에는 빌린 포인터를 돌려준다.
		GameObject* root = world.AddGameObject(std::move(object));
		if (prefabs[i] != nullptr)
		{
			ApplyPrefab(*root, *prefabs[i]);
		}
		loadedObjects.push_back(root);
	}
	return loadedObjects;
}
