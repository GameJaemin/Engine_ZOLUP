#pragma once

#include "HeartComponent.h"
#include "HideTableComponent.h"
#include "OrbitCameraComponent.h"
#include "PlayerMoveComponent.h"
#include "PrototypeInputAdapter.h"
#include "StitchBrainComponent.h"
#include "ProximityLightComponent.h"
#include "EscapeScenarioTests.h"
#include "CaughtCameraComponent.h"

#include "PoseAnimatorComponent.h"
#include "VoxelDollComponent.h"
#include "ChapterTests.h"
#include "AssetManager.h"
#include "Component/AnimationComponent.h"
#include "Component/StaticMeshComponent.h"
#include "Renderer/Material.h"
#include "Component/CameraComponent.h"

#include "Component/BoxColliderComponent.h"
#include "Component/ComponentTypeRegistry.h"
#include "Component/EngineComponentTypes.h"
#include "Component/PointLightComponent.h"
#include "Component/RigidBodyComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "Physics/JoltRuntime.h"
#include "Scene/SceneSnapshot.h"
#include "World.h"

#include <cstdio>
#include <functional>

namespace PrototypeTests
{
	inline int Run()
	{
		std::setvbuf(stdout,nullptr,_IONBF,0);
		int failures = 0;
		const std::function<void(bool, const char*)> check = [&failures](bool condition, const char* name)
		{
			std::printf("%s %s\n", condition ? "PASS" : "FAIL", name);
			if (!condition) { ++failures; }
		};
		PrototypeConfig config;
		RunChapterTests(check);
		InputState rawInput;
		rawInput.PressKey(Key::S);
		rawInput.PressKey(Key::Period);
		rawInput.PressKey(Key::Period);
		PrototypeInputAdapter adapter;
		const PrototypeInputFrame inputFrame = adapter.Read(rawInput);
		check(inputFrame.command.feet == std::vector<int>{ 0, 1 }, "input adapter preserves foot order and rejects key repeat");
		check(inputFrame.leftDown && inputFrame.rightDown, "co-op key guide receives held S and Period state");
		InputState soloInput;
		soloInput.PressKey(Key::D);
		check(adapter.Read(soloInput).command.feet.empty() && adapter.Read(soloInput, true).command.feet == std::vector<int>{ 1 }, "D is only a right foot in the explicit solo profile");
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
		player.Step(left, 0.0f, 1.0f / 120.0f, config, space, false);
		for (int frame = 0; frame < 240; ++frame) { player.Step({}, 0.0f, 1.0f / 120.0f, config, space, false); }
		check(player.GetLastFoot() == 0 && player.GetSpeed() < 0.1f, "stopping clears speed but remembers the last foot");
		player.Step(left, 0.0f, 1.0f / 120.0f, config, space, false);
		check(player.GetMode() == PlayerMode::Fallen && player.HasMistake(), "same foot after a long pause still causes a stumble");
		player.Reset({});
		player.Step(left, 0.0f, 1.0f / 120.0f, config, space, false);
		for (int frame = 0; frame < 240; ++frame) { player.Step({}, 0.0f, 1.0f / 120.0f, config, space, false); }
		PrototypeCommand right;
		right.feet = { 1 };
		player.Step(right, 0.0f, 1.0f / 120.0f, config, space, false);
		check(player.GetMode() == PlayerMode::Moving && player.GetLastFoot() == 1 && player.GetMomentum() < 0.1f,
			"opposite foot after a pause starts a fresh acceleration combo");
		player.Reset({});
		for (int frame = 0; frame < 440; ++frame)
		{
			PrototypeCommand command;
			if (frame % 20 == 0) { command.feet.push_back(frame / 20 % 2); }
			player.Step(command, 0.0f, 1.0f / 120.0f, config, space, false);
		}
		check(player.GetMomentum() > 0.95f && player.GetMode() == PlayerMode::Moving, "alternating rhythm accelerates without stutter");
		for (int intervalFrames : { 36, 60, 84 })
		{
			player.Reset({});
			bool keepsWalking = true;
			for (int frame = 0; frame < 360; ++frame)
			{
				PrototypeCommand command;
				if (frame % intervalFrames == 0) { command.feet.push_back(frame / intervalFrames % 2); }
				player.Step(command, 0.0f, 1.0f / 120.0f, config, space, false);
				if (frame >= 120) { keepsWalking = keepsWalking && player.GetSpeed() >= config.minimumWalkSpeed - 0.1f; }
			}
			check(keepsWalking && player.GetPosition().z > 500.0f && player.GetMode() == PlayerMode::Moving,
				"slow alternating feet keep minimum walking speed at 0.3, 0.5 or 0.7 second intervals");
		}
		for (int frame = 0; frame < 240; ++frame) { player.Step({}, 0.0f, 1.0f / 120.0f, config, space, false); }
		check(player.GetSpeed() < 0.1f, "walking floor releases after foot input stops");
		PrototypeConfig walkConfig = config;
		walkConfig.minimumWalkSpeed = 260.0f;
		player.Reset({});
		for (int frame = 0; frame < 180; ++frame)
		{
			PrototypeCommand command;
			if (frame % 60 == 0) { command.feet.push_back(frame / 60 % 2); }
			player.Step(command, 0.0f, 1.0f / 120.0f, walkConfig, space, false);
		}
		check(std::abs(player.GetSpeed() - walkConfig.minimumWalkSpeed) < 0.1f, "minimum walking speed follows tuning independently of sprint momentum");
		player.SetMode(PlayerMode::Hidden);
		const Vector3 hiddenPosition = player.GetPosition();
		player.Step(left, 0.0f, 1.0f / 120.0f, config, space, false);
		check(player.GetSpeed() == 0.0f && PrototypeMath::Distance(hiddenPosition, player.GetPosition()) == 0.0f,
			"minimum walking speed does not move a hidden player");
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
		// Space 소리는 한 틱만 들린다. 숨기와 심박 진정이 마지막 소리 위치 조사를 취소하면 안 된다.
		for (float hiddenBPM : { 72.0f, 150.0f })
		{
			brain.Reset({ 0.0f, 0.0f, 1500.0f });
			StitchObservation noise;
			noise.sound = SoundStimulus{ {}, 4500.0f, SoundKind::Mistake };
			brain.Step(noise, 1.0f / 120.0f, config, flat);
			noise.sound.reset();
			noise.hidden = true;
			noise.heartBPM = hiddenBPM;
			// 소리 후 다른 곳에 숨더라도 AI는 실제 플레이어 위치 대신 들은 지점을 확인한다.
			noise.playerPosition = { 600.0f, 0.0f, 0.0f };
			bool reachedNoise = false;
			bool keptNoiseDestination = true;
			for (int index = 0; index < 1440 && !reachedNoise; ++index)
			{
				brain.Step(noise, 1.0f / 120.0f, config, flat);
				reachedNoise = PrototypeMath::Distance(brain.GetPosition(), {}) < 25.0f;
				keptNoiseDestination = keptNoiseDestination && PrototypeMath::Distance(brain.GetDestination(), {}) < 0.01f;
			}
			check(reachedNoise && keptNoiseDestination, hiddenBPM < 100.0f
				? "distant Space noise is investigated even after immediate quiet hiding"
				: "distant Space noise is investigated before listening to a hidden player");
			for (int index = 0; index < 180; ++index) { brain.Step(noise, 1.0f / 120.0f, config, flat); }
			check(hiddenBPM < 100.0f ? brain.GetState() == StitchState::Patrol : brain.GetState() == StitchState::Suspicious,
				"hidden heart state is evaluated after investigating the remembered noise");
		}
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
		const auto& patrolPoints = StitchBrainComponent::PatrolRoute;
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
		RearViewState rear;
		rear.Step(true, 0.0f, 5.0f);
		check(rear.visible && rear.remaining == 5.0f, "CCTV opens immediately on chase");
		rear.Step(false, 4.9f, 5.0f);
		check(rear.visible, "CCTV survives the chase-end hold interval");
		rear.Step(false, 0.11f, 5.0f);
		check(!rear.visible, "CCTV closes after five seconds without chase");
		check(rear.GetPush(1500.0f, config) == 0.0f && rear.GetPush(250.0f, config) == 1.0f, "rear camera framing respects far and near thresholds");
		JoltRuntime runtime;
		if (!runtime.Initialize()) { check(false, "Jolt runtime initializes"); return 1; }
		ComponentTypeRegistry registry;
		const bool registered = RegisterEngineComponentTypes(registry) && RegisterPrototypeLightType();
		check(registered, "game lamp type registers beside engine component types");
		if (!registered) { return 1; }
		{
			World cameraWorld;
			GameObject* object = cameraWorld.CreateGameObject("Capture camera test", {});
			std::unique_ptr<CameraComponent> cameraOwner = std::make_unique<CameraComponent>();
			CameraComponent* output = cameraOwner.get();
			object->AddComponent(std::move(cameraOwner));
			GameplaySpace cameraSpace;
			OrbitCameraComponent walkCamera;
			walkCamera.Reset({ 0.0f, 160.0f, 0.0f }, 0.0f, config);
			for (int index = 0; index < 120; ++index)
			{
				walkCamera.Present(*output, { 0.0f, 160.0f, 0.0f }, 200.0f / config.maxSpeed, 1.0f / 120.0f, config, cameraSpace, false);
			}
			check(std::abs(walkCamera.GetFov() - config.cameraFov) < 0.01f, "ordinary walking keeps a stable camera field of view");
			for (int index = 0; index < 360; ++index)
			{
				walkCamera.Present(*output, { 0.0f, 160.0f, 0.0f }, 1.0f, 1.0f / 120.0f, config, cameraSpace, false);
			}
			check(walkCamera.GetFov() > config.runningCameraFov - 0.1f, "sprinting smoothly opens the camera field of view");
			cameraSpace.obstacles.push_back({ { 45.0f, 0.0f, -700.0f }, { 70.0f, 300.0f, 100.0f } });
			walkCamera.Reset({ 0.0f, 160.0f, 0.0f }, 0.0f, config);
			walkCamera.Present(*output, { 0.0f, 160.0f, 0.0f }, 0.0f, 1.0f / 120.0f, config, cameraSpace, false);
			check(walkCamera.GetPosition().x < 28.0f, "camera shoulder retracts before entering a side wall");
			GameObject* dollObject = cameraWorld.CreateGameObject("Procedural doll test", {});
			auto dollOwner = std::make_unique<VoxelDollComponent>();
			VoxelDollComponent* dollPose = dollOwner.get();
			dollObject->AddComponent(std::move(dollOwner));
			dollPose->Reset({});
			dollPose->Present({ 0.0f, 0.0f, 20.0f }, 200.0f, 0.0f, 0.0f, PlayerMode::Moving, 0.1f, config);
			const float gaitPhase = dollPose->GetPhase();
			dollPose->Present({ 0.0f, 0.0f, 20.0f }, 0.0f, 0.0f, 0.0f, PlayerMode::Moving, 0.1f, config);
			check(gaitPhase > 0.0f && dollPose->GetPhase() == gaitPhase, "voxel gait follows actual distance and stops at a blocked position");
			dollPose->Present({ 0.0f, 0.0f, 20.0f }, 200.0f, 0.0f, 0.0f, PlayerMode::Moving, 0.0f, config);
			check(dollPose->GetPhase() == gaitPhase, "paused voxel animation holds its phase");
			dollPose->Present({0.0f,0.0f,20.0f},100.0f,90.0f,90.0f,PlayerMode::Crawling,1.0f,config,0.0f,0,true);
			check(dollObject->GetComponent<TransformComponent>()->GetPosition().y>25.0f,"crawl pose lifts the horizontal doll clear of the floor");
			dollPose->Present({0.0f,0.0f,20.0f},0.0f,90.0f,90.0f,PlayerMode::Fallen,1.0f,config,0.0f,0,true);
			check(dollObject->GetComponent<TransformComponent>()->GetPosition().y>25.0f&&dollObject->GetComponent<TransformComponent>()->GetScale().y>0.79f,
				"crawl stumble retains the low lifted pose instead of burying or standing the doll");
			dollPose->Reset({0.0f,0.0f,20.0f});
			dollPose->Present({ 0.0f, 0.0f, 20.0f }, 0.0f, 0.0f, 0.0f, PlayerMode::Fallen, 0.2f, config);
			check(dollObject->GetComponent<TransformComponent>()->GetRotation().pitch > 60.0f, "stumbling changes the doll pose instead of stretching its mesh");
			CaughtCameraComponent shot;
			shot.Begin({}, { 0.0f, 0.0f, 100.0f }, 180.0f);
			shot.Present(*output, 0.0f, config);
			const Vector3 start = object->GetComponent<TransformComponent>()->GetPosition();
			check(start.z > 0.0f && std::abs(start.y - config.caughtFaceHeight) < 0.01f && std::abs(shot.GetFacingYaw()) < 0.01f,
				"capture camera faces the monster from the victim side at face height");
			shot.Present(*output, 0.3f, config);
			const Vector3 pushed = object->GetComponent<TransformComponent>()->GetPosition();
			check(pushed.z < start.z && output->GetVerticalFovDegrees() < config.caughtFov,
				"capture shot pushes toward the face and tightens its field of view");
			shot.Present(*output, 0.3f, config);
			check(PrototypeMath::Length(pushed - object->GetComponent<TransformComponent>()->GetPosition()) < 0.0001f,
				"capture shake does not drift when time is paused");
			check(CaughtCameraComponent::GetFade(0.8f, config) == 0.0f
				&& CaughtCameraComponent::GetFade(config.caughtDuration - 0.05f, config) == 1.0f,
				"capture keeps the scare visible then holds full black before restoring");
			shot.Begin({}, {}, 90.0f);
			check(std::abs(shot.GetFacingYaw() - 90.0f) < 0.01f, "overlapping capture positions use a finite fallback direction");
			shot.Begin({}, { 0.0f, 0.0f, 100.0f }, 0.0f);
			shot.Present(*output, 0.0f, config, Vector3{ 20.0f, 300.0f, 5.0f });
			check(std::abs(object->GetComponent<TransformComponent>()->GetPosition().y - 300.0f) < 0.01f,
				"capture framing follows the animated head instead of a fixed chest height");
			bool outsideTentacles=true, wideFraming=true, centered=true;
			const CaughtCameraFraming tentacleShot{config.caughtTentacleDistance,config.caughtTentacleMinimumDistance,config.caughtTentacleFov,4.0f};
			for(float yaw:{0.0f,90.0f,-135.0f})
			{
				const Vector3 forward=PrototypeMath::Direction(yaw);
				const Vector3 focus=Vector3{0.0f,config.caughtTentacleAimHeight,0.0f}+forward*35.0f;
				shot.Begin({},forward*100.0f,yaw);
				for(int frame=0;frame<=22;++frame)
				{
					const float elapsed=frame*0.05f;
					shot.Present(*output,elapsed,config,focus,tentacleShot);
					const Vector3 delta=object->GetComponent<TransformComponent>()->GetPosition()-focus;
					outsideTentacles=outsideTentacles&&delta.Dot(forward)>=config.caughtTentacleMinimumDistance-0.01f;
					wideFraming=wideFraming&&output->GetVerticalFovDegrees()>=config.caughtTentacleFov-4.01f;
					const auto rotation=object->GetComponent<TransformComponent>()->GetRotation();
					centered=centered&&std::abs(std::remainder(rotation.yaw-yaw-180.0f,360.0f))<6.0f
						&&std::abs(rotation.pitch)<6.0f&&std::abs(rotation.roll)<=config.caughtShakeAngle
						&&std::abs(delta.Dot(forward)-config.caughtTentacleDistance)<0.01f;
				}
			}
			check(outsideTentacles,"bear jumpscare keeps its camera beyond the extended tentacles for every facing direction");
			check(wideFraming,"bear jumpscare preserves room for the open torso and tentacles throughout the shake");
			check(centered,"strong shake keeps the bear near the center at a fixed distance across camera directions");
			const Vector3 entry{250.0f,160.0f,-300.0f};
			object->GetComponent<TransformComponent>()->SetPosition(entry);
			object->GetComponent<TransformComponent>()->SetRotation({12.0f,170.0f,0.0f});
			output->SetVerticalFovDegrees(80.0f);
			shot.Begin({}, {0.0f,0.0f,100.0f}, 0.0f);
			shot.BlendFrom(*output);
			shot.Present(*output,0.0f,config,Vector3{0.0f,205.0f,35.0f},tentacleShot);
			check(PrototypeMath::Length(object->GetComponent<TransformComponent>()->GetPosition()-entry)<0.001f
				&& output->GetVerticalFovDegrees()==80.0f,
				"capture begins at the current gameplay camera without a cut");
			shot.Present(*output,config.caughtFadeStart*0.95f,config,Vector3{0.0f,205.0f,35.0f},tentacleShot);
			const Vector3 grabPosition=object->GetComponent<TransformComponent>()->GetPosition();
			const float grabRoll=object->GetComponent<TransformComponent>()->GetRotation().roll;
			shot.Present(*output,config.caughtFadeStart*0.95f,config,Vector3{0.0f,205.0f,35.0f},tentacleShot);
			check(PrototypeMath::Length(grabPosition-object->GetComponent<TransformComponent>()->GetPosition())<0.001f
				&& std::abs(grabRoll)<=config.caughtShakeAngle && std::abs(grabPosition.z-455.0f)<0.01f,
				"camera shake holds its distance and repeats the same pose while paused");
			GameObject* rig = cameraWorld.CreateGameObject("Head pose test", {});
			rig->GetComponent<TransformComponent>()->SetPosition({ 10.0f, 20.0f, 30.0f });
			std::shared_ptr<Model> rigModel = std::make_shared<Model>("Capture test rig", "");
			rigModel->sceneNodes.resize(2);
			rigModel->sceneNodes[0].localTransform.position = { 0.0f, 100.0f, 0.0f };
			rigModel->sceneNodes[1].name = "Head";
			rigModel->sceneNodes[1].parent = 0;
			rigModel->sceneNodes[1].localTransform.position = { 0.0f, 50.0f, 0.0f };
			rigModel->animations.push_back(std::make_shared<AnimationClip>("Test", ""));
			AnimationComponent* rigAnimation = static_cast<AnimationComponent*>(rig->AddComponent(std::make_unique<AnimationComponent>()));
			PoseAnimatorComponent* rigAnimator = static_cast<PoseAnimatorComponent*>(rig->AddComponent(std::make_unique<PoseAnimatorComponent>()));
			Vector3 head;
			check(rigAnimator->Initialize(rigModel, *rigAnimation) && rigAnimator->TryGetNodeWorldPosition("Head", head)
				&& PrototypeMath::Length(head - Vector3{ 10.0f, 170.0f, 30.0f }) < 0.01f,
				"head targeting composes bone parents and the visual world transform");
			check(!rigAnimator->TryGetNodeWorldPosition("Missing", head), "models without a head node use the capture height fallback");
		}
		{
			World original;
			World restored;
			GameObject* floor = original.CreateGameObject("Physics floor", {});
			floor->GetComponent<TransformComponent>()->SetPosition({ 0.0f, -10.0f, 0.0f });
			std::unique_ptr<BoxColliderComponent> floorBox = std::make_unique<BoxColliderComponent>();
			check(floorBox->SetLocalHalfExtents({ 1000.0f, 10.0f, 1000.0f }), "physics floor dimensions accepted");
			floor->AddComponent(std::move(floorBox));
			GameObject* crate = original.CreateGameObject("Physics crate", {});
			crate->GetComponent<TransformComponent>()->SetPosition({ 0.0f, 300.0f, 0.0f });
			crate->GetComponent<TransformComponent>()->SetRotation({ 0.0f, 25.0f, 12.0f });
			std::unique_ptr<BoxColliderComponent> crateBox = std::make_unique<BoxColliderComponent>();
			check(crateBox->SetLocalHalfExtents({ 40.0f, 40.0f, 40.0f }), "rotated dynamic box dimensions accepted");
			crate->AddComponent(std::move(crateBox));
			crate->AddComponent(std::make_unique<RigidBodyComponent>());
			bool physicsPassed = true;
			for (int tick = 0; tick < 240; ++tick) { physicsPassed = original.GetPhysicsScene().Update(1.0f / 60.0f) && physicsPassed; }
			const float crateHeight = crate->GetComponent<TransformComponent>()->GetPosition().y;
			check(physicsPassed && crateHeight >= 38.0f && crateHeight < 75.0f, "Jolt crate falls and rests on its static floor");
			GameObject* lampObject = original.CreateGameObject("Proximity lamp", {});
			lampObject->AddComponent(std::make_unique<PointLightComponent>());
			std::unique_ptr<ProximityLightComponent> lampOwner = std::make_unique<ProximityLightComponent>();
			ProximityLightComponent* lamp = lampOwner.get();
			lampObject->AddComponent(std::move(lampOwner));
			check(lamp->SetDarkRadius(500.0f) && lamp->SetFlickerTime(0.4f) && lamp->SetBaseIntensity(3.0f), "lamp tuning accepts valid settings");
			lamp->Step({}, 0.2f, true);
			check(!lamp->IsDark(), "near lamp flickers before extinguishing");
			lamp->Step({}, 0.3f, true);
			check(lamp->IsDark() && lamp->GetOutputIntensity() == 0.0f, "near lamp turns off after flicker time");
			lamp->Step({ 1000.0f, 0.0f, 0.0f }, 0.0f, true);
			check(!lamp->IsDark() && lamp->GetOutputIntensity() == 3.0f, "lamp relights immediately outside the radius");
			check(!lamp->SetFlickerTime(0.0f) && lamp->GetFlickerTime() == 0.4f, "invalid lamp edit preserves its previous setting");
			const std::optional<SceneSnapshot> snapshot = CaptureWorld(original);
			const bool cloned = snapshot.has_value() && RestoreWorld(*snapshot, restored);
			check(cloned, "engine snapshot clones physics props and registered game lamp");
			if (cloned)
			{
				const GameObject* clone = restored.FindGameObject(lampObject->GetObjectID());
				const ProximityLightComponent* copiedLamp = clone ? clone->GetComponent<ProximityLightComponent>() : nullptr;
				check(copiedLamp && copiedLamp->GetDarkRadius() == 500.0f && copiedLamp->GetBaseIntensity() == 3.0f, "registered game fields survive snapshot restoration");
			}
			GameObject* lens = original.CreateGameObject("Lamp lens", {});
			StaticMeshComponent* lensMesh = static_cast<StaticMeshComponent*>(lens->AddComponent(std::make_unique<StaticMeshComponent>()));
			std::array<std::shared_ptr<const Material>, 3> materials;
			for (int level = 0; level < 3; ++level) { materials[level] = std::make_shared<const Material>("Lamp test", "", MaterialParameters{}); }
			check(lamp->BindEmissiveVisual(*lens, materials) && lensMesh->GetMaterial() == materials[2], "lamp lens binds with its lit material");
			lamp->Step({}, 0.05f, true);
			check(lensMesh->GetMaterial() == materials[1] && std::abs(lampObject->GetComponent<PointLightComponent>()->GetIntensity() - 0.36f) < 0.001f,
				"lamp emission and point light enter the same dim flicker phase");
			lamp->Step({}, 0.1f, true);
			check(lensMesh->GetMaterial() == materials[2] && lampObject->GetComponent<PointLightComponent>()->GetIntensity() == 3.0f,
				"lamp emission and point light brighten on the same tick");
			lamp->Step({}, 0.3f, true);
			check(lensMesh->GetMaterial() == materials[0] && lampObject->GetComponent<PointLightComponent>()->GetIntensity() == 0.0f,
				"lamp emission and point light extinguish together");
			lamp->Reset();
			check(lensMesh->GetMaterial() == materials[2] && lampObject->GetComponent<PointLightComponent>()->GetIntensity() == 3.0f,
				"checkpoint lamp reset immediately restores both outputs");
			original.DestroyGameObject(lens);
			original.DestroyPendingGameObjects();
			lamp->Step({}, 0.05f, true);
			check(!lamp->IsDark(), "removed lamp meshes do not leave dangling component pointers");
		}
		RunEscapeScenarioTests(check);
		std::printf("BabyDoll: %d failure(s)\n", failures);
		return failures ? 1 : 0;
	}
}
