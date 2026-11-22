#pragma once

#include <filesystem>
#include <optional>
#include <vector>

class GameObject;
class World;

/**
 * JSON을 읽어 일반 루트 객체 또는 프리팹 인스턴스를 월드에 추가한다.
 * 일반 객체에는 이름과 Transform이 필요하다. 프리팹 항목에서는 지정한 값만 루트의 기본값을 덮어쓴다.
 * 일반 객체의 components 배열은 directionalLight, pointLight, spotLight, ambientLight, camera를 지원한다.
 * prefab과 components는 함께 쓸 수 없다. color, intensity, enabled를 생략하면 컴포넌트 기본값을 쓴다.
 * pointLight와 spotLight에는 rangeCm이 필요하다. spotLight에는 전체 각도(도)인 outerConeDegrees도 필요하다.
 * camera의 projection은 perspective 또는 orthographic이다. 각각 verticalFovDegrees 또는 orthographicHeightCm이 필요하다.
 * nearClipCm과 farClipCm을 생략하면 카메라 기본값을 쓴다. 로드하는 동안 활성 카메라를 지정하지 않는다.
 * 성공 시 이번에 추가한 객체를 배열 순서대로 빌려준다. 빈 맵은 빈 목록을 반환한다.
 * 파일 읽기나 검증 실패 시 월드를 변경하지 않고 nullopt를 반환한다.
 * 반환한 포인터는 각 객체가 삭제되기 전까지 유효하다. 월드 갱신 중에는 호출하지 않는다.
 */
[[nodiscard]] std::optional<std::vector<GameObject*>> LoadLevel(const std::filesystem::path& path, World& world);
