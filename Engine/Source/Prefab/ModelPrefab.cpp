#include "ModelPrefab.h"

#include "AssetManager.h"
#include "Component/AnimationComponent.h"
#include "Component/SkinComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "Core/Math/Transform.h"
#include "GLTF/GLTFLoader.h"
#include "GameObject.h"
#include "Core/FileHelper.h"
#include "Mesh/Skin.h"
#include "Scene/SnapshotJson.h"
#include "World.h"

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <format>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
	/** 로그에 적을 경로 글자를 만든다. */
	std::string PathToText(const std::filesystem::path& path)
	{
		const std::u8string text = path.generic_u8string();
		return std::string(text.begin(), text.end());
	}

	/** \n 줄바꿈을 \r\n으로 바꾼다. 저장소의 .editorconfig가 CRLF를 정한다. */
	std::string ToCrlf(std::string_view text)
	{
		std::string result;
		result.reserve(text.size() + text.size() / 16);
		for (const char character : text)
		{
			if (character == '\n')
			{
				result += '\r';
			}
			result += character;
		}
		return result;
	}

	void LogPrefabFileError(std::string_view context, std::string_view reason)
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: {}", context, reason));
	}
}

GameObject* InstantiatePrefab(World& world, const PrefabObjects& prefab, const PrefabPlacement& placement)
{
	// 1) 원본은 캐시에 남겨 두고 사본을 고친다. 같은 Prefab을 여러 번 놓을 수 있어야 하기 때문이다.
	std::vector<GameObjectSnapshot> objects = prefab.objects;

	// 2) 사본의 ID를 새로 뽑는다. 짝 표로 새 루트 ID를 찾는다.
	const std::unordered_map<ObjectID, ObjectID> newIDs = RegenerateObjectIDs(objects);
	const auto foundRoot = newIDs.find(prefab.rootObjectID);
	if (foundRoot == newIDs.end())
	{
		Log::Write(LogLevel::Error, "Prefab", "prefab root is not in its objects");
		return nullptr;
	}

	// 3) 복원한다. 실패하면 RestoreGameObjects가 이번에 만든 객체만 삭제 예정으로 표시한다.
	std::vector<GameObject*> created;
	if (!RestoreGameObjects(objects, world, created)) { return nullptr; }

	// 4) 루트에 배치 값을 적용한다. 루트는 부모가 없으므로 로컬 Transform이 곧 월드 Transform이다.
	GameObject* root = world.FindGameObject(foundRoot->second);
	if (placement.name.has_value())
	{
		root->SetName(*placement.name);
	}
	if (placement.transform.has_value())
	{
		root->GetComponent<TransformComponent>()->SetTransform(*placement.transform);
	}
	return root;
}

bool BuildModelHierarchy(GameObject& root, const std::shared_ptr<const Model>& modelAsset, const std::vector<std::shared_ptr<const Material>>& materialOverrides)
{
	if (modelAsset == nullptr) { return false; }

	World& world = root.GetWorld();
	const Model& model = *modelAsset;

	// 뼈 노드는 GameObject가 아니라 포즈 배열의 칸으로 다룬다. 뼈 아래의 노드도 연결할 부모 GameObject가 없어 만들지 않는다.
	std::vector<bool> isJoint(model.sceneNodes.size(), false);
	for (const std::shared_ptr<const Skin>& skin : model.skins)
	{
		for (const std::size_t joint : skin->joints)
		{
			isJoint[joint] = true;
		}
	}

	std::vector<bool> skipsObject(model.sceneNodes.size(), false);
	std::size_t skippedNonJointCount = 0;
	for (std::size_t i = 0; i < model.sceneNodes.size(); ++i)
	{
		// 자신이나 조상 중에 뼈가 있으면 만들지 않는다.
		std::optional<std::size_t> current = i;
		while (current.has_value() && !isJoint[*current])
		{
			current = model.sceneNodes[*current].parent;
		}
		skipsObject[i] = current.has_value();
		if (skipsObject[i] && !isJoint[i])
		{
			++skippedNonJointCount;
		}
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
			if (!materialOverrides.empty() && materialOverrides[meshIndex] != nullptr)
			{
				material = materialOverrides[meshIndex];
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
				if (createdObject != nullptr)
				{
					world.DestroyGameObject(createdObject);
				}
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

		// 원래 자세는 SetModel이 모델 배치표에서 채운다. SetModel이 노드 연결을 비우므로 연결은 그 뒤에 넣는다.
		std::unique_ptr<AnimationComponent> animationComponent = std::make_unique<AnimationComponent>();
		animationComponent->SetModel(modelAsset);
		// 목록을 모델 노드 수로 만들었으므로 성공해야 한다. 실패하면 호출자가 루트와 함께 만든 노드를 지운다.
		if (!animationComponent->SetNodeTransforms(std::move(nodeTransforms)))
		{
			Log::Write(LogLevel::Error, "Level", std::format("{}: animation node list does not match the model", root.GetName()));
			return false;
		}
		animation = animationComponent.get();
		root.AddComponent(std::move(animationComponent));
	}

	// 스킨이 붙은 메시 노드의 GameObject에 스킨 연결을 붙인다.
	for (std::size_t i = 0; i < model.sceneNodes.size(); ++i)
	{
		if (nodeObjects[i] == nullptr || !model.sceneNodes[i].skin.has_value()) { continue; }

		std::unique_ptr<SkinComponent> skin = std::make_unique<SkinComponent>();
		skin->SetSkin(modelAsset, i, animation);
		nodeObjects[i]->AddComponent(std::move(skin));
	}
	return true;
}

std::optional<PrefabObjects> CreatePrefabFromWorld(const World& world, const GameObject& root)
{
	// CaptureWorld가 실패 원인을 로그로 남긴다.
	std::optional<SceneSnapshot> snapshot = CaptureWorld(world);
	if (!snapshot.has_value()) { return std::nullopt; }
	return PrefabObjects{ std::move(snapshot->objects), root.GetObjectID() };
}

std::optional<PrefabObjects> CreateModelPrefab(const std::shared_ptr<const Model>& model, const std::vector<std::shared_ptr<const Material>>& materialOverrides)
{
	if (model == nullptr)
	{
		Log::Write(LogLevel::Error, "Prefab", "cannot create a prefab without a model");
		return std::nullopt;
	}
	// BuildModelHierarchy가 메시 번호로 바로 꺼내므로 길이를 먼저 확인한다.
	if (!materialOverrides.empty() && materialOverrides.size() != model->meshes.size())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: material override count does not match mesh count", model->GetName()));
		return std::nullopt;
	}

	// 객체를 만들어 찍기만 하는 World다. 그리지 않으며 함수가 끝나면 객체와 함께 사라진다.
	World builder;
	GameObject* root = builder.CreateGameObject(model->GetName(), Transform{});
	if (root == nullptr || !BuildModelHierarchy(*root, model, materialOverrides))
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: could not build the model hierarchy", model->GetName()));
		return std::nullopt;
	}

	return CreatePrefabFromWorld(builder, *root);
}

bool SavePrefabFile(const std::filesystem::path& path, const PrefabObjects& prefab, AssetManager& assets)
{
	const std::string context = PathToText(path);

	// JSON 트리를 먼저 모두 만든다. 에셋 참조를 저장할 수 없으면 파일을 건드리기 전에 실패한다.
	rapidjson::Document document(rapidjson::kObjectType);
	rapidjson::Document::AllocatorType& allocator = document.GetAllocator();
	const std::string rootText = prefab.rootObjectID.ToString();
	document.AddMember("rootObject", rapidjson::Value(rootText.c_str(), static_cast<rapidjson::SizeType>(rootText.size()), allocator), allocator);
	rapidjson::Value objects;
	if (!WriteGameObjects(prefab.objects, assets, allocator, objects, context)) { return false; }
	document.AddMember("objects", objects, allocator);

	// 트리를 들여쓴 글자로 바꾼다. 기존 JSON 파일처럼 탭 하나로 들여쓴다.
	rapidjson::StringBuffer buffer;
	rapidjson::PrettyWriter<rapidjson::StringBuffer> writer(buffer);
	writer.SetIndent('\t', 1);
	// 배열 원소 사이의 줄바꿈만 없앤다. [0.0, 0.0, 0.0] 같은 수 배열이 한 줄이 되고, 배열 안 객체의 속성은 그대로 줄을 나눠 쓴다.
	writer.SetFormatOptions(rapidjson::kFormatSingleLineArray);
	document.Accept(writer);

	// PrettyWriter는 \n으로 줄을 바꾸므로 CRLF로 바꾸고, .editorconfig대로 끝에 줄바꿈을 하나 둔다.
	std::string text = ToCrlf(std::string_view(buffer.GetString(), buffer.GetSize()));
	text += "\r\n";
	if (!FileHelper::WriteAllText(path, text))
	{
		LogPrefabFileError(context, "could not write the file");
		return false;
	}
	return true;
}

std::optional<PrefabObjects> LoadPrefabFile(const std::filesystem::path& path, AssetManager& assets)
{
	const std::string context = PathToText(path);
	std::vector<std::byte> bytes;
	if (!FileHelper::ReadAllBytes(path, bytes))
	{
		LogPrefabFileError(context, "could not read the file");
		return std::nullopt;
	}

	// RapidJSON이 원시 NUL 바이트를 입력 끝으로 처리할 수 있어, 옛 Prefab 로더처럼 빈 파일과 NUL이 든 파일을 먼저 거절한다.
	if (bytes.empty() || std::find(bytes.begin(), bytes.end(), std::byte{ 0 }) != bytes.end())
	{
		LogPrefabFileError(context, "file is empty or contains a raw NUL byte");
		return std::nullopt;
	}

	// 기존 로더와 같이 UTF-8이 아닌 글자가 있으면 파싱을 실패시킨다.
	rapidjson::Document document;
	document.Parse<rapidjson::kParseValidateEncodingFlag>(reinterpret_cast<const char*>(bytes.data()), bytes.size());
	if (document.HasParseError())
	{
		LogPrefabFileError(context, std::format("invalid JSON at byte {}: {}", document.GetErrorOffset(), rapidjson::GetParseError_En(document.GetParseError())));
		return std::nullopt;
	}

	// 맨 위는 rootObject와 objects 두 항목만 있어야 한다. 항목이 둘이고 둘 다 찾으면 다른 이름은 끼어 있을 수 없다.
	// 형식 번호는 두지 않는다. 구조가 바뀌면 옛 파일은 이 검사들로 실패하고, 파일을 한꺼번에 다시 저장해 맞춘다.
	if (!document.IsObject() || document.MemberCount() != 2)
	{
		LogPrefabFileError(context, "top level must be an object with only rootObject and objects");
		return std::nullopt;
	}
	const auto rootJson = document.FindMember("rootObject");
	const auto objectsJson = document.FindMember("objects");
	if (rootJson == document.MemberEnd() || objectsJson == document.MemberEnd())
	{
		LogPrefabFileError(context, "top level must have rootObject and objects");
		return std::nullopt;
	}

	const std::optional<ObjectID> rootObjectID = rootJson->value.IsString() ? ObjectID::Parse(std::string_view(rootJson->value.GetString(), rootJson->value.GetStringLength())) : std::nullopt;
	if (!rootObjectID.has_value())
	{
		LogPrefabFileError(context, "rootObject must be a valid ObjectID string");
		return std::nullopt;
	}

	std::optional<std::vector<GameObjectSnapshot>> objects = ReadGameObjects(objectsJson->value, assets, context);
	if (!objects.has_value()) { return std::nullopt; }

	// 루트는 목록 안에 있어야 하고 누구의 자식도 아니어야 한다. 놓을 때 루트의 로컬 Transform을 월드 Transform으로 쓰기 때문이다.
	// ID 중복은 ReadGameObjects가, 목록 밖을 가리키는 자식이나 참조는 놓을 때 복원이 거절한다.
	const bool hasRoot = std::any_of(objects->begin(), objects->end(), [&rootObjectID](const GameObjectSnapshot& object) { return object.objectID == *rootObjectID; });
	if (!hasRoot)
	{
		LogPrefabFileError(context, "rootObject is not in objects");
		return std::nullopt;
	}
	for (const GameObjectSnapshot& object : *objects)
	{
		const bool isParentOfRoot = std::find(object.childObjectIDs.begin(), object.childObjectIDs.end(), *rootObjectID) != object.childObjectIDs.end();
		if (isParentOfRoot)
		{
			LogPrefabFileError(context, std::format("rootObject is a child of {}", object.name));
			return std::nullopt;
		}
	}

	return PrefabObjects{ std::move(*objects), *rootObjectID };
}
