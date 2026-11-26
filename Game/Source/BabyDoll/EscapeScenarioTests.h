#pragma once

#include "EscapeScenario.h"
#include "PlayerMoveComponent.h"
#include "StitchBrainComponent.h"
#include "HideTableComponent.h"

#include <functional>

inline void RunEscapeScenarioTests(const std::function<void(bool, const char*)>& check)
{
	PrototypeConfig config;
	EscapeLayout layout;
	check(LoadEscapeLayout(layout), "escape layout loads from game JSON");
	EscapeProgress progress;
	check(!progress.UnlockExit() && !progress.TryEscape(layout.escapePosition, PlayerMode::Moving, layout), "exit cannot unlock or clear before acquiring the key");
	GameplaySpace room;
	room.obstacles = layout.Partitions();
	const std::size_t gate = room.obstacles.size();
	room.obstacles.push_back(layout.Door());
	for (const auto& point : EscapeLayout::Exhibits) { room.obstacles.push_back({point + Vector3{-120.0f, 0.0f, -120.0f}, point + Vector3{120.0f, 320.0f, 120.0f}}); }
	for (const auto& point : EscapeLayout::Tables) { room.obstacles.push_back({point + Vector3{-160.0f, 100.0f, -120.0f}, point + Vector3{160.0f, 135.0f, 120.0f}}); }
	StitchBrainComponent patrol;
	patrol.Reset({0.0f, 0.0f, 850.0f});
	StitchObservation quiet;
	quiet.playerPosition = {10000.0f, 0.0f, 10000.0f};
	int visited = 0;
	for (int i = 0; i < 24000; ++i)
	{
		patrol.Step(quiet, 1.0f / 120.0f, config, room);
		for (int point = 0; point < 4; ++point) { if (PrototypeMath::Distance(patrol.GetPosition(), StitchBrainComponent::PatrolRoute[point]) < 35.0f) { visited |= 1 << point; } }
	}
	check(visited == 15, "monster completes its patrol in the full room while the exit is locked");
	PlayerMoveComponent walker;
	walker.Reset({600.0f, 0.0f, -1100.0f});
	PrototypeCommand walk;
	walk.forward = 1.0f;
	for (int i = 0; i < 600; ++i) { walker.Step(walk, 90.0f, 1.0f / 120.0f, config, room, true); }
	check(walker.GetPosition().x < layout.doorPosition.x - config.playerRadius, "locked gate blocks actual player movement");
	int nextFoot = 0;
	const auto walkTo = [&](const Vector3& goal)
	{
		const std::vector<Vector3> route = room.FindPath(walker.GetPosition(), goal, config.playerRadius + 5.0f, config.playerHeight);
		if (route.empty()) { return false; }
		std::size_t point = 0;
		for (int frame = 0; frame < 18000 && point < route.size(); ++frame)
		{
			const Vector3 delta = route[point] - walker.GetPosition();
			if (PrototypeMath::Length(delta) < 45.0f) { ++point; continue; }
			PrototypeCommand rhythm;
			if (frame % 20 == 0) { rhythm.feet.push_back(nextFoot); nextFoot = 1 - nextFoot; }
			walker.Step(rhythm, std::atan2(delta.x, delta.z) * 57.2957795f, 1.0f / 120.0f, config, room, false);
			if (!room.IsFree(walker.GetPosition(), config.playerRadius, config.playerHeight)) { return false; }
		}
		return PrototypeMath::Distance(walker.GetPosition(), goal) < 80.0f;
	};
	walker.Reset({0.0f, 0.0f, -1100.0f});
	check(walkTo(layout.keyPosition), "player can navigate from entrance through storage doorway to the key");
	check(progress.TakeKey() && !progress.TakeKey(), "key pickup advances once and cannot be duplicated");
	const Vector3 angledContact = layout.doorPosition + Vector3{-60.0f, 0.0f, 190.0f};
	check(room.IsFree(angledContact, config.playerRadius, config.playerHeight)
		&& FindEscapeInteraction(progress, layout, room, angledContact, PlayerMode::Moving, 90.0f, gate) == 2,
		"key carrier running against the gate at an angle can still unlock it");
	const Vector3 edgeContact = layout.doorPosition + Vector3{-60.0f, 0.0f, 250.0f};
	check(FindEscapeInteraction(progress, layout, room, edgeContact, PlayerMode::Moving, 90.0f, gate) == 2,
		"wide gate can be unlocked near its edge instead of only at its center");
	check(FindEscapeInteraction(progress, layout, room, edgeContact, PlayerMode::Moving, -90.0f, gate) == 0,
		"gate interaction still requires facing the door");
	check(FindEscapeInteraction(progress, layout, room, edgeContact, PlayerMode::Caught, 90.0f, gate) == 0,
		"caught player cannot unlock the exit");
	GameplaySpace occludedDoor = room;
	occludedDoor.obstacles.push_back({{710.0f, 0.0f, -1250.0f}, {720.0f, 200.0f, -950.0f}});
	check(FindEscapeInteraction(progress, layout, occludedDoor, {650.0f, 0.0f, -1100.0f}, PlayerMode::Moving, 90.0f, gate) == 0,
		"ignoring the target gate does not allow interaction through another wall");
	EscapeProgress withoutKey;
	check(FindEscapeInteraction(withoutKey, layout, room, edgeContact, PlayerMode::Moving, 90.0f, gate) == 3,
		"gate edge still reports locked when no key is carried");
	StitchBrainComponent pursuer;
	pursuer.Reset({400.0f, 0.0f, -1100.0f});
	pursuer.ChangeState(StitchState::Chase);
	check(pursuer.GetState() == StitchState::Chase && FindEscapeInteraction(progress, layout, room, edgeContact, PlayerMode::Moving, 90.0f, gate) == 2,
		"being chased does not block key use at the gate");
	check(walkTo(layout.doorPosition + Vector3{-160.0f, 0.0f, 0.0f}), "key carrier can return from storage to the locked gate");
	check(progress.UnlockExit(), "carried key unlocks the exit");
	room.obstacles[gate].enabled = !progress.IsDoorOpen();
	check(walkTo(layout.escapePosition), "opened exit provides a collision-safe route to the escape beacon");
	check(!progress.TryEscape(layout.escapePosition, PlayerMode::Caught, layout), "capture wins over escape on the same tick");
	check(progress.TryEscape(walker.GetPosition(), walker.GetMode(), layout) && !progress.TryEscape(walker.GetPosition(), walker.GetMode(), layout), "complete play route clears exactly once");
	const EscapeProgress entranceCheckpoint;
	progress = entranceCheckpoint;
	room.obstacles[gate].enabled = !progress.IsDoorOpen();
	check(!progress.HasKey() && !progress.IsDoorOpen() && room.obstacles[gate].enabled, "entrance checkpoint restores key and locked gate after a failed run");
	// 추격 중 접근할 수 없는 숨기 지점은 수명 없는 테이블이어도 원격 파괴하지 않는다.
	GameplaySpace blocked;
	blocked.obstacles.push_back({{-500.0f, 0.0f, -500.0f}, {500.0f, 400.0f, 500.0f}});
	StitchBrainComponent brain;
	brain.Reset({0.0f, 0.0f, 1400.0f});
	StitchObservation hidden;
	hidden.hidden = hidden.breakableTable = true;
	hidden.heartBPM = 150.0f;
	hidden.sound = SoundStimulus{{}, 1500.0f, SoundKind::Heart};
	bool smashed = false;
	for (int i = 0; i < 2400; ++i) { brain.Step(hidden, 1.0f / 120.0f, config, blocked); smashed = smashed || brain.GetAction() == StitchAction::Smashing || brain.DidBreakTable(); }
	check(!smashed && brain.GetState() == StitchState::Suspicious, "inaccessible hiding spot times out to suspicion without remote table destruction");
	GameplaySpace flat;
	brain.Reset({0.0f, 0.0f, 500.0f});
	StitchObservation noise;
	noise.playerPosition = {0.0f, 0.0f, -1000.0f};
	noise.sound = SoundStimulus{{0.0f, 0.0f, 500.0f}, 4500.0f, SoundKind::Mistake};
	brain.Step(noise, 1.0f / 120.0f, config, flat);
	noise.sound.reset();
	for (int i = 0; i < 1200 && brain.GetAction() != StitchAction::Roaring; ++i) { brain.Step(noise, 1.0f / 120.0f, config, flat); }
	check(brain.GetAction() == StitchAction::Roaring, "investigation without further noise reaches the roar phase");
	const Vector3 nextSound{1000.0f, 0.0f, 500.0f};
	noise.sound = SoundStimulus{nextSound, 4500.0f, SoundKind::Mistake, 0, true, 1};
	brain.Step(noise, 1.0f / 120.0f, config, flat);
	noise.sound.reset();
	for (int i = 0; i < 300; ++i) { brain.Step(noise, 1.0f / 120.0f, config, flat); }
	check(PrototypeMath::Distance(brain.GetDestination(), nextSound) < 0.01f, "one-shot noise heard during roaring is investigated afterwards");
	HideTableComponent brokenTable;
	brokenTable.Enter(true, {0.0f, 0.0f, -230.0f});
	brokenTable.Break();
	GameplaySpace exitsBlocked;
	exitsBlocked.obstacles = {
		{{-50.0f, 0.0f, 180.0f}, {50.0f, 200.0f, 280.0f}},
		{{-50.0f, 0.0f, -280.0f}, {50.0f, 200.0f, -180.0f}}
	};
	Vector3 landing;
	check(!brokenTable.FindExit(0.0f, {0.0f, 0.0f, 100.0f}, exitsBlocked, landing), "ordinary table exit respects blocked preferred and fallback directions");
	check(brokenTable.FindExit(0.0f, {0.0f, 0.0f, 100.0f}, exitsBlocked, landing, true) && PrototypeMath::Length(landing) < 0.01f,
		"destroyed table can release the player in place when all exit directions are blocked");
	exitsBlocked.obstacles.push_back({{-100.0f, 0.0f, -100.0f}, {100.0f, 200.0f, 100.0f}});
	check(!brokenTable.FindExit(0.0f, {0.0f, 0.0f, 100.0f}, exitsBlocked, landing, true), "fully obstructed destroyed table rejects unsafe landing for checkpoint recovery");
}
