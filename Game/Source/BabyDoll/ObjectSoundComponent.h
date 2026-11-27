#pragma once
#include "HearingComponent.h"
#include "Component/Component.h"
#include <string>

struct ObjectSoundRule
{
	std::string objectID;
	std::string event;
	float radius = 0.0f;
	float minImpactSpeed = 0.0f;
	float duration = 0.0f;
	float cooldown = 0.0f;
	StitchState reaction = StitchState::Suspicious;
	bool ignore = false;
	bool crossFloor = false;
	bool ignoreRadius = false;
	int priority = 0;
	std::string sfxID;
};

/** 실제 음원 없이도 사물 이벤트의 수명/우선순위/층 예외를 검증할 수 있다. */
class ObjectSoundComponent final : public Component
{
public:
	bool Load();
	std::vector<ObjectSoundRule> rules;
	void Trigger(const std::string& instance, const std::string& kind, const std::string& event, const Vector3& position, float impactSpeed = 0.0f);
	void Step(float deltaTime, HearingComponent& hearing);
	void Reset() { active.clear(); cooldowns.clear(); }
private:
	struct Active { ObjectSoundRule rule; Vector3 position; float remaining; };
	struct Cooldown { std::string key; float remaining; };
	std::vector<Active> active;
	std::vector<Cooldown> cooldowns;
};
