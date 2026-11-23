#pragma once

#include "Prefab.h"

#include "rapidjson/document.h"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

class AssetManager;
class GameObject;
struct Material;

// 프리팹 로더 내부용이다. 컴포넌트마다 읽기, 에셋 로드, 붙이기 함수를 둔다.
// 읽기와 로드는 실패하면 원인을 로그에 남기고 false를 반환한다.

/** 저장소 루트 기준 상대 경로면 빈 문자열을, 아니면 오류 설명을 반환한다. */
[[nodiscard]] std::string_view ValidatePrefabAssetPath(const rapidjson::Value& value);

/** 객체 하나와 그 자식 계층을 읽고, 계층 안의 id와 animation 연결 대상을 검증한다. 맵의 일반 객체도 이 형식으로 읽는다. */
[[nodiscard]] bool ReadPrefabObjectDefinition(const rapidjson::Value& object, std::string_view context, PrefabObjectDefinition& outDefinition);

/** definition과 자식 계층의 에셋을 불러와 object를 채운다. */
[[nodiscard]] bool LoadPrefabObject(AssetManager& assets, const PrefabObjectDefinition& definition, std::string_view context, PrefabObject& object);

/** 유한하고 float 범위 안의 숫자면 true를 반환한다. */
[[nodiscard]] bool IsPrefabFiniteFloat(const rapidjson::Value& value);

/** baseMaterial을 복사해 기본 색 텍스처만 바꾼다. 텍스처를 읽지 못하면 nullptr를 반환한다. */
[[nodiscard]] std::shared_ptr<const Material> LoadMaterialWithBaseColorTexture(AssetManager& assets, const Material& baseMaterial, std::string_view texturePath);

// staticMesh
[[nodiscard]] bool ReadStaticMeshProperties(const rapidjson::Value& properties, std::string_view context, PrefabStaticMeshDefinition& mesh);
[[nodiscard]] bool LoadPrefabStaticMesh(AssetManager& assets, const PrefabStaticMeshDefinition& definition, std::string_view context, PrefabStaticMesh& outMesh);
void AttachPrefabStaticMesh(GameObject& object, const PrefabStaticMesh& mesh);

// skybox
[[nodiscard]] bool ReadSkyboxProperties(const rapidjson::Value& properties, std::string_view context, PrefabSkyboxDefinition& skybox);
[[nodiscard]] bool LoadPrefabSkybox(AssetManager& assets, const PrefabSkyboxDefinition& definition, std::string_view context, PrefabSkybox& outSkybox);
void AttachPrefabSkybox(GameObject& object, const PrefabSkybox& skybox);

// animation
[[nodiscard]] bool ReadAnimationProperties(const rapidjson::Value& properties, std::string_view context, PrefabAnimationDefinition& animation);
[[nodiscard]] bool LoadPrefabAnimation(AssetManager& assets, const PrefabAnimationDefinition& definition, std::string_view context, PrefabAnimation& outAnimation);
/** 계층을 모두 만든 뒤 호출한다. nodeBindings의 대상 id는 모두 objectsByID에 있어야 한다. */
void AttachPrefabAnimation(GameObject& object, const PrefabAnimation& animation, const std::unordered_map<std::string, GameObject*>& objectsByID);

// skin
[[nodiscard]] bool ReadSkinProperties(const rapidjson::Value& properties, std::string_view context, PrefabSkinDefinition& skin);
[[nodiscard]] bool LoadPrefabSkin(AssetManager& assets, const PrefabSkinDefinition& definition, std::string_view context, PrefabSkin& outSkin);
/** animations의 AnimationComponent를 모두 붙인 뒤 호출한다. 포즈는 자신이나 조상 중 같은 모델의 animation에서 읽는다. */
void AttachPrefabSkin(GameObject& object, const PrefabSkin& skin, const std::vector<std::pair<GameObject*, const PrefabAnimation*>>& animations);

// light (에셋이 없어 로드 함수를 두지 않는다)
/** directionalLight, pointLight, spotLight, ambientLight 중 하나면 종류를, 아니면 nullopt를 반환한다. */
[[nodiscard]] std::optional<PrefabLightType> FindPrefabLightType(std::string_view typeName);
/** light.type이 채워진 상태로 호출한다. */
[[nodiscard]] bool ReadLightProperties(const rapidjson::Value& properties, std::string_view context, PrefabLightDefinition& light);
void AttachPrefabLight(GameObject& object, const PrefabLightDefinition& light);

// camera (에셋이 없어 로드 함수를 두지 않는다)
[[nodiscard]] bool ReadCameraProperties(const rapidjson::Value& properties, std::string_view context, PrefabCameraDefinition& camera);
void AttachPrefabCamera(GameObject& object, const PrefabCameraDefinition& camera);
