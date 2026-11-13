#pragma once

#include <filesystem>
#include <optional>
#include <vector>

class GameObject;
class World;

/**
 * JSON에서 이름과 Transform을 읽어 루트 객체를 만들고 월드에 추가한다.
 * 성공 시 이번에 추가한 객체를 배열 순서대로 빌려준다. 빈 맵은 빈 목록을 반환한다.
 * 파일 읽기나 검증 실패 시 월드를 변경하지 않고 nullopt를 반환한다.
 * 반환한 포인터는 각 객체가 삭제되기 전까지 유효하다. 월드 갱신 중에는 호출하지 않는다.
 */
[[nodiscard]] std::optional<std::vector<GameObject*>> LoadLevel(const std::filesystem::path& path, World& world);
