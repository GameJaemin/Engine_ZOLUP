#pragma once

#include "Component/ComponentTypeRegistry.h"
#include "ObjectID.h"

#include <optional>
#include <string>
#include <utility>
#include <vector>

class World;

/**
 * 컴포넌트 하나의 상태다. 타입은 등록부의 이름이고, 속성은 등록 이름과 값을 부모 속성부터 등록 순서대로 담는다.
 * 등록하지 않은 컴포넌트 상태는 담기지 않아 복원하면 기본값이 된다.
 */
struct ComponentSnapshot
{
	std::string typeName;
	std::vector<std::pair<std::string, ComponentFieldValue>> fields;
};

/** 객체 하나의 상태다. 다른 객체를 가리키는 참조는 속성 값 안의 ObjectID로 담긴다. */
struct GameObjectSnapshot
{
	ObjectID objectID;
	std::string name;
	// 자식 순서대로 담는다. 삭제 예정 자식은 빠지며 비어 있으면 자식이 없다.
	std::vector<ObjectID> childObjectIDs;
	// 원본 객체에 붙은 순서대로 담는다. TransformComponent도 이 목록에 담긴다.
	std::vector<ComponentSnapshot> components;
};

/** World 하나의 객체 상태를 메모리에 담는다. */
struct SceneSnapshot
{
	// World 등록 순서다. 부모가 자식보다 뒤에 나올 수 있다.
	std::vector<GameObjectSnapshot> objects;
	// 활성 카메라를 가진 객체다. 무효면 활성 카메라가 없다.
	ObjectID activeCameraObjectID;
};

/**
 * 삭제 예정이 아닌 객체를 컴포넌트 속성 표로 담는다.
 * 파생 GameObject, 등록되지 않았거나 만들 수 없거나 속성이 없는 컴포넌트, 같은 타입 컴포넌트 중복,
 * 스냅샷 밖을 가리키는 컴포넌트 참조가 있으면 로그를 남기고 nullopt를 반환한다.
 * 참조는 ObjectID로 담으므로, 같은 ID를 쓰는 다른 World의 컴포넌트를 가리킨 경우는 걸러 내지 못한다.
 */
[[nodiscard]] std::optional<SceneSnapshot> CaptureWorld(const World& world);

/**
 * 객체가 없는 World에 ObjectID를 유지해 복원한다. 컴포넌트 참조 속성은 모든 컴포넌트를 붙인 뒤 연결한다.
 * 실패하면 로그를 남기고, 이번에 만든 객체를 모두 지워 빈 World로 되돌린 뒤 false를 반환한다.
 * 실패 정리에서 삭제를 바로 처리하므로 그 World의 객체 목록을 순회하는 중에는 호출하지 않는다.
 */
[[nodiscard]] bool RestoreWorld(const SceneSnapshot& snapshot, World& world);
