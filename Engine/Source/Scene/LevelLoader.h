#pragma once

#include <filesystem>
#include <optional>
#include <vector>

class GameObject;
class World;

/**
 * JSON을 읽어 일반 루트 객체 또는 프리팹 인스턴스를 월드에 추가한다.
 * 일반 객체에는 이름과 Transform이 필요하다. 프리팹 항목에서는 지정한 값만 루트의 기본값을 덮어쓴다.
 * 일반 객체는 프리팹 파일의 root와 같은 형식이며 id, components, children을 쓸 수 있다. id는 그 객체의 계층 안에서만 유효하다.
 * 프리팹 항목에는 name과 transform만 지정할 수 있다. 로드하는 동안 활성 카메라를 지정하지 않는다.
 * 성공 시 이번에 추가한 객체를 배열 순서대로 빌려준다. 빈 맵은 빈 목록을 반환한다.
 * 파일 읽기나 검증 실패 시 월드를 변경하지 않고 nullopt를 반환한다.
 * 객체 생성 실패 시 이번에 만든 객체를 모두 삭제 예정으로 표시하고 nullopt를 반환한다.
 * 반환한 포인터는 각 객체가 삭제되기 전까지 유효하다. 월드 갱신 중에는 호출하지 않는다.
 */
[[nodiscard]] std::optional<std::vector<GameObject*>> LoadLevel(const std::filesystem::path& path, World& world);
