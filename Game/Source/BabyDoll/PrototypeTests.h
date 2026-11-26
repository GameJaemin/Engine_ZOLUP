#pragma once

#include "HeartComponent.h"
#include "HideTableComponent.h"
#include "OrbitCameraComponent.h"
#include "PlayerMoveComponent.h"
#include "PrototypeInputAdapter.h"
#include "StitchBrainComponent.h"

#include <cstdio>
#include <functional>

namespace PrototypeTests
{
	inline int Run()
	{
		int failures = 0;
		const std::function<void(bool, const char*)> check = [&failures](bool condition, const char* name)
		{
			std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
			if (!condition) { ++failures; }
		};
		PrototypeConfig config;
		InputState rawInput;
		rawInput.PressKey(Key::S);
		rawInput.PressKey(Key::D);
		rawInput.PressKey(Key::D);
		PrototypeInputAdapter adapter;
		const PrototypeInputFrame inputFrame = adapter.Read(rawInput);
		check(inputFrame.command.feet == std::vector<int>{ 0, 1 }, "input adapter preserves foot order and rejects key repeat");
		GameplaySpace space;
		PlayerMoveComponent player;
		player.Reset({});
		PrototypeCommand left;
		left.feet = { 0 };
		player.Step(left, 0.0f, 1.0f / 120.0f, config, space, false);
		player.Step(left, 0.0f, 1.0f / 120.0f, config, space, false);
		check(player.GetMode() == PlayerMode::Fallen && player.HasMistake(), "same foot causes one stumble");
		player.Step({}, 0.0f, 0.01f, config, space, false);
		check(!player.HasMistake(), "mistake is not emitted every frame");
		player.Reset({});
		for (int frame = 0; frame < 440; ++frame)
		{
			PrototypeCommand command;
			if (frame % 20 == 0) { command.feet.push_back(frame / 20 % 2); }
			player.Step(command, 0.0f, 1.0f / 120.0f, config, space, false);
		}
		check(player.GetMomentum() > 0.95f && player.GetMode() == PlayerMode::Moving, "alternating rhythm accelerates without stutter");
		space.obstacles.push_back({ { -200.0f, 0.0f, 100.0f }, { 200.0f, 300.0f, 200.0f } });
		const Vector3 blocked = space.Move({}, { 0.0f, 0.0f, 500.0f }, 30.0f, 120.0f);
		check(blocked.z <= 71.0f, "swept substeps prevent crossing a thin obstacle");
		const std::vector<Vector3> path = space.FindPath({}, { 0.0f, 0.0f, 600.0f }, 60.0f, 296.0f);
		bool avoids = !path.empty();
		for (const Vector3& point : path) { avoids = avoids && space.IsFree(point, 60.0f, 296.0f); }
		check(avoids, "navigation avoids geometry for monster clearance");
		check(space.CameraDistance({ 0.0f, 150.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 500.0f, 18.0f) < 100.0f, "camera retracts before intersecting geometry");
		HearingComponent hearing;
		hearing.Emit({}, 100.0f, SoundKind::Heart);
		check(hearing.Hear({ 100.0f, 500.0f, 0.0f }).has_value(), "hearing uses horizontal radius inclusively");
		check(!hearing.Hear({}, 1), "sound does not cross floors");
		hearing.Emit({}, 200.0f, SoundKind::Object, false);
		check(hearing.Hear({})->kind == SoundKind::Heart, "player sound outranks a newer object sound");
		HeartComponent heart;
		check(heart.GetSoundRadius() == 0.0f, "resting heart is inaudible");
		heart.OnDetected();
		check(std::abs(heart.GetSoundRadius() - 1500.0f) < 0.01f, "detected heart reaches maximum radius");
		heart.Step(0.0f, true, true, false, 100.0f, 1.0f, config);
		check(heart.GetBPM() == 150.0f, "heart remains at maximum while hiding during chase");
		for (int index = 0; index < 241; ++index) { heart.Step(0.0f, true, false, true, 100.0f, 1.0f / 120.0f, config); }
		check(heart.GetBPM() < 100.0f, "hidden calm crosses below audible threshold");
		HideTableComponent table;
		table.Enter(true, {});
		check(table.GetRemainingUses() == 0 && !table.IsBreakableOccupancy(), "first chase hiding consumes life without breaking");
		table.Leave();
		table.Enter(true, {});
		check(table.IsBreakableOccupancy(), "second hiding occupancy is breakable");
		const HideTableSnapshot snapshot = table.Capture();
		table.Break();
		table.Restore(snapshot);
		check(!table.IsBroken() && table.GetRemainingUses() == 0, "table snapshot restores lifetime and debris state");
		StitchBrainComponent brain;
		brain.Reset({ 0.0f, 0.0f, 500.0f });
		StitchObservation observation;
		observation.playerPosition = {};
		observation.sound = SoundStimulus{ {}, 4500.0f, SoundKind::Mistake };
		GameplaySpace flat;
		brain.Step(observation, 1.0f / 120.0f, config, flat);
		check(brain.GetState() == StitchState::Chase, "near mistake begins chase");
		observation.sound.reset();
		brain.Step(observation, 1.0f / 120.0f, config, flat);
		check(brain.GetState() == StitchState::Suspicious, "lost sound changes chase to suspicion");
		brain.Reset({ 0.0f, 0.0f, 200.0f });
		brain.ChangeState(StitchState::Chase);
		observation.hidden = true;
		observation.heartBPM = 150.0f;
		observation.sound = SoundStimulus{ {}, 1500.0f, SoundKind::Heart };
		for (int index = 0; index < 250; ++index) { brain.Step(observation, 1.0f / 120.0f, config, flat); }
		check(brain.GetState() == StitchState::Suspicious && !brain.HasCaughtPlayer(), "hidden heartbeat does not restart chase after observation");
		brain.Reset({ 130.0f, 0.0f, 0.0f });
		observation = {};
		brain.Step(observation, 1.0f / 120.0f, config, flat);
		check(brain.HasCaughtPlayer(), "patrolling monster catches an exposed player at 130cm");
		AgentMovement turning;
		turning.Reset({}, 0.0f);
		bool facesMovement = true;
		bool boundedAcceleration = true;
		bool boundedTurn = true;
		float previousSpeed = 0.0f;
		for (int index = 0; index < 1200; ++index)
		{
			const Vector3 before = turning.GetPosition();
			const float previousYaw = turning.GetYaw();
			turning.Step({ 1000.0f, 0.0f, 0.0f }, 170.0f, 1.0f / 120.0f, config, flat);
			const Vector3 displacement = turning.GetPosition() - before;
			const float distance = PrototypeMath::Length(displacement);
			if (distance > 0.01f) { facesMovement = facesMovement && (displacement / distance).Dot(PrototypeMath::Direction(turning.GetYaw())) > 0.999f; }
			boundedAcceleration = boundedAcceleration && turning.GetSpeed() - previousSpeed <= config.monsterAcceleration / 120.0f + 0.1f;
			boundedTurn = boundedTurn && std::abs(std::remainder(turning.GetYaw() - previousYaw, 360.0f)) <= config.monsterTurnRate / 120.0f + 0.001f;
			previousSpeed = turning.GetSpeed();
		}
		check(facesMovement, "agent faces its actual direction of travel instead of strafing");
		check(boundedAcceleration && boundedTurn, "agent acceleration and yaw rate remain bounded");
		check(turning.HasArrived() && PrototypeMath::Distance(turning.GetPosition(), { 1000.0f, 0.0f, 0.0f }) < 22.0f && turning.GetSpeed() < 1.0f, "agent brakes and settles at destination");
		AgentMovement detour;
		detour.Reset({ 0.0f, 0.0f, -400.0f }, 0.0f);
		bool clearDetour = true;
		for (int index = 0; index < 3000; ++index)
		{
			detour.Step({ 0.0f, 0.0f, 600.0f }, 170.0f, 1.0f / 120.0f, config, space);
			clearDetour = clearDetour && space.IsFree(detour.GetPosition(), 60.0f, 296.0f);
		}
		check(clearDetour && detour.HasArrived(), "smoothed steering rounds an obstacle and reaches its goal");
		AgentMovement agent30;
		AgentMovement agent120;
		agent30.Reset({}, 0.0f);
		agent120.Reset({}, 0.0f);
		for (int index = 0; index < 60; ++index) { agent30.Step({ 0.0f, 0.0f, 1000.0f }, 170.0f, 1.0f / 30.0f, config, flat); }
		for (int index = 0; index < 240; ++index) { agent120.Step({ 0.0f, 0.0f, 1000.0f }, 170.0f, 1.0f / 120.0f, config, flat); }
		check(PrototypeMath::Distance(agent30.GetPosition(), agent120.GetPosition()) < 4.0f, "agent travel remains consistent at 30 and 120Hz");
		GameplaySpace gallery;
		const Vector3 plinths[]{ { -1100.0f, 0.0f, -200.0f }, { 1100.0f, 0.0f, 200.0f }, { -300.0f, 0.0f, 700.0f }, { 500.0f, 0.0f, -700.0f } };
		for (const Vector3& point : plinths) { gallery.obstacles.push_back({ point + Vector3{ -120.0f, 0.0f, -120.0f }, point + Vector3{ 120.0f, 320.0f, 120.0f } }); }
		StitchBrainComponent patrol;
		patrol.Reset({ 0.0f, 0.0f, 850.0f });
		StitchObservation quiet;
		quiet.playerPosition = { 10000.0f, 0.0f, 10000.0f };
		const Vector3 patrolPoints[]{ { -1300.0f, 0.0f, 1000.0f }, { 1300.0f, 0.0f, 1000.0f }, { 1300.0f, 0.0f, -1000.0f }, { -1300.0f, 0.0f, -1000.0f } };
		int visited = 0;
		bool patrolClear = true;
		for (int index = 0; index < 10800; ++index)
		{
			patrol.Step(quiet, 1.0f / 120.0f, config, gallery);
			patrolClear = patrolClear && gallery.IsFree(patrol.GetPosition(), 60.0f, 296.0f);
			for (int point = 0; point < 4; ++point)
			{
				if (PrototypeMath::Distance(patrol.GetPosition(), patrolPoints[point]) < 35.0f) { visited |= 1 << point; }
			}
		}
		check(patrolClear && visited == 15, "monster completes its patrol circuit without collision or corner stalls");
		OrbitCameraComponent camera30;
		OrbitCameraComponent camera120;
		camera30.Reset({}, 0.0f, config);
		camera120.Reset({}, 0.0f, config);
		for (int index = 0; index < 30; ++index) { camera30.Look(index == 0 ? 100.0f : 0.0f, 0.0f, 1.0f / 30.0f, config); }
		for (int index = 0; index < 120; ++index) { camera120.Look(index == 0 ? 100.0f : 0.0f, 0.0f, 1.0f / 120.0f, config); }
		check(std::abs(camera30.GetYaw() - camera120.GetYaw()) < 0.001f, "mouse response converges equally at 30 and 120 FPS");
		std::printf("BabyDoll: %d failure(s)\n", failures);
		return failures ? 1 : 0;
	}
}
