#pragma once
#include "PlayerMoveComponent.h"
#include "Component/Component.h"
#include <optional>

enum class TraversalKind { Ladder, Crawl, Bar };
struct TraversalLink
{
	TraversalKind kind = TraversalKind::Ladder;
	Vector3 start;
	Vector3 end;
	Vector3 cameraPosition;
	float width = 180.0f;
	Vector3 exitOffset;
	Vector3 cameraTarget;
	float cameraFov=65.0f;
};

/** 특수 이동의 위치/발 이력/스태미나만 관리한다. 모델과 카메라는 값을 받아 표현한다. */
class TraversalComponent final : public Component
{
public:
	std::vector<TraversalLink> links;
	int Find(const Vector3& position, float yaw, const GameplaySpace* space=nullptr) const;
	bool Begin(int index, PlayerMoveComponent& player, const PrototypeConfig& config = {}, const GameplaySpace* space=nullptr);
	bool Leave(PlayerMoveComponent& player, const GameplaySpace& space);
	void Step(const PrototypeCommand& command, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space, PlayerMoveComponent& player);
	void Reset(float capacity=10.0f) { active = -1; progress = sinceFoot = stumbleTime = 0.0f; stamina=capacity; lastFoot = -1; slips = 0; }
	bool IsActive() const { return active >= 0; }
	bool IsHidden() const { return active >= 0 && links[active].kind != TraversalKind::Ladder; }
	bool UsesFixedCamera() const { return active >= 0 && (links[active].kind == TraversalKind::Crawl || (links[active].kind == TraversalKind::Ladder && reverse)); }
	const TraversalLink* GetLink() const { return active >= 0 ? &links[active] : nullptr; }
	float GetStamina() const { return stamina; }
	float GetSpeed() const { return speed; }
	bool MadeMistake() const { return mistake; }
	int GetFloor() const { return floor; }
	int GetLastFoot() const { return lastFoot; }
private:
	int active = -1;
	int lastFoot = -1;
	int slips = 0;
	int floor = 0;
	float progress = 0.0f;
	float targetProgress=0.0f;
	float stamina = 10.0f;
	float sinceFoot = 0.0f;
	float speed = 0.0f;
	bool reverse = false;
	bool mistake = false;
	float stumbleTime=0.0f;
};
