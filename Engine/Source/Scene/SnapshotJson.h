#pragma once

#include "Component/ComponentTypeRegistry.h"
#include "SceneSnapshot.h"

#include "rapidjson/document.h"

#include <optional>
#include <string_view>
#include <vector>

class AssetManager;

/**
 * value를 JSON 값으로 바꿔 outJson에 담는다. 에셋 참조는 경로 정보로, 컴포넌트 참조는 대상 객체 ID로 적고, 가리키지 않으면 null이다.
 * 에셋 참조를 저장할 수 없으면 context와 이유를 로그로 남기고 false를 반환한다.
 */
bool WriteFieldValue(const ComponentFieldValue& value, AssetManager& assets, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& outJson, std::string_view context);

/**
 * json을 field의 값 견본과 같은 종류로 읽는다. 목록이면 원소를 원소 견본으로 읽는다.
 * 종류나 길이가 맞지 않거나, 유한하지 않은 수이거나, 에셋을 얻지 못하면 context와 이유를 로그로 남기고 nullopt를 반환한다.
 */
std::optional<ComponentFieldValue> ReadFieldValue(const rapidjson::Value& json, const ComponentFieldInfo& field, AssetManager& assets, std::string_view context);

/**
 * objects를 JSON 배열로 쓴다. 객체마다 id, name, 자식이 있으면 children, components를 적고, 속성은 담긴 순서대로 적는다.
 * 에셋 참조를 저장할 수 없으면 context와 이유를 로그로 남기고 false를 반환한다.
 */
bool WriteGameObjects(const std::vector<GameObjectSnapshot>& objects, AssetManager& assets, rapidjson::Document::AllocatorType& allocator, rapidjson::Value& outJson, std::string_view context);

/**
 * JSON 배열을 객체 목록으로 읽는다. 컴포넌트 타입과 속성은 등록부 이름으로 찾는다.
 * 모르는 속성과 저장 제외 속성은 경고를 남기고 건너뛰며, 파일에 없는 속성은 담지 않아 복원하면 기본값이 된다.
 * 모양이 틀리거나, ID를 읽을 수 없거나, 같은 ID가 두 번 나오거나, 모르는 컴포넌트 타입이거나, 한 객체에 같은 컴포넌트 타입이나 같은 속성이 두 번 나오거나,
 * 값을 읽지 못하면 로그를 남기고 nullopt를 반환한다. 목록 밖을 가리키는 자식이나 참조는 여기서 검사하지 않고 복원(RestoreGameObjects)이 거절한다.
 */
std::optional<std::vector<GameObjectSnapshot>> ReadGameObjects(const rapidjson::Value& json, AssetManager& assets, std::string_view context);
