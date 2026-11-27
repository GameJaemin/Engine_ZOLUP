#pragma once

#include "PrototypeTypes.h"

#include "Component/Component.h"

#include <cstdint>
#include <optional>

struct SoundStimulus
{
	Vector3 position;
	float radius = 0.0f;
	SoundKind kind = SoundKind::Object;
	int floor = 0;
	bool player = true;
	std::uint64_t sequence = 0;
	bool crossFloor = false;
	bool ignoreRadius = false;
	int priority = 0;
	std::optional<StitchState> reaction;
};

/** 매 틱 BeginFrame -> Emit -> Hear 순서로 사용한다. 실제 음원 재생과 독립이다. */
class HearingComponent final : public Component
{
public:
	void BeginFrame() { stimuli.clear(); }
	void Emit(const Vector3& position, float radius, SoundKind kind, bool player = true, int floor = 0,
		bool crossFloor = false, bool ignoreRadius = false, int priority = 0, std::optional<StitchState> reaction = std::nullopt);
	std::optional<SoundStimulus> Hear(const Vector3& listener, int floor = 0) const;
	static float FootRadius(float speed, float maximumSpeed, const PrototypeConfig* tuning=nullptr);
	std::uint64_t GetSequence() const { return sequence; }
	void RestoreSequence(std::uint64_t value) { sequence=value; stimuli.clear(); }

private:
	std::vector<SoundStimulus> stimuli;
	std::uint64_t sequence = 0;
};
