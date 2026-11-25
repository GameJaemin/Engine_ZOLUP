#pragma once

#include "ThirdPersonCharacter.h"

#include "Physics/JoltRuntime.h"

#include <cmath>
#include <cstdio>
#include <memory>

namespace ThirdPersonTests
{
	inline void AddFloor(ThirdPersonScene& scene)
	{
		scene.AddTriangle({ -3000.0f, 0.0f, -3000.0f }, { 3000.0f, 0.0f, -3000.0f }, { 3000.0f, 0.0f, 3000.0f });
		scene.AddTriangle({ -3000.0f, 0.0f, -3000.0f }, { 3000.0f, 0.0f, 3000.0f }, { -3000.0f, 0.0f, 3000.0f });
	}

	inline void Simulate(ThirdPersonMotor& motor, const ThirdPersonInput& input, const ThirdPersonScene& scene, int frames, float frameTime)
	{
		for (int frame = 0; frame < frames; ++frame)
		{
			motor.Update(input, frameTime, scene);
		}
	}

	/** 렌더러 초기화 없이 이동/충돌과 실제 캐릭터 생성-해제 경로를 검사한다. */
	inline int Run()
	{
		int failures = 0;
		const auto check = [&failures](bool result, const char* name)
		{
			std::printf("%s %s\n", result ? "PASS" : "FAIL", name);
			if (!result)
			{
				++failures;
			}
		};
		ThirdPersonScene flat;
		AddFloor(flat);
		flat.Finalize();
		ThirdPersonInput walk;
		walk.forward = 1.0f;
		ThirdPersonMotor straight;
		straight.Reset({ 0.0f, 0.1f, 0.0f }, 0.0f);
		Simulate(straight, walk, flat, 120, 1.0f / 60.0f);
		check(straight.GetPosition().z > 300.0f && std::abs(straight.GetPosition().y) < 0.2f && straight.IsGrounded(), "walk remains on floor");
		ThirdPersonInput diagonal = walk;
		diagonal.right = 1.0f;
		ThirdPersonMotor diagonalMotor;
		diagonalMotor.Reset({ 0.0f, 0.1f, 0.0f }, 0.0f);
		Simulate(diagonalMotor, diagonal, flat, 120, 1.0f / 60.0f);
		check(std::abs(ThirdPersonMath::Length(diagonalMotor.GetPosition()) - ThirdPersonMath::Length(straight.GetPosition())) < 0.5f, "diagonal speed is normalized");
		ThirdPersonMotor running;
		running.Reset({ 0.0f, 0.1f, 0.0f }, 0.0f);
		ThirdPersonInput sprint = walk;
		sprint.running = true;
		Simulate(running, sprint, flat, 120, 1.0f / 60.0f);
		check(running.GetPosition().z > straight.GetPosition().z * 2.0f && running.GetSpeed() > 440.0f, "Shift selects running speed");
		Simulate(running, {}, flat, 60, 1.0f / 60.0f);
		check(running.GetSpeed() < 0.1f, "release brakes to idle");
		ThirdPersonMotor slowFrame;
		ThirdPersonMotor fastFrame;
		slowFrame.Reset({ 0.0f, 0.1f, 0.0f }, 0.0f);
		fastFrame.Reset({ 0.0f, 0.1f, 0.0f }, 0.0f);
		Simulate(slowFrame, walk, flat, 60, 1.0f / 30.0f);
		Simulate(fastFrame, walk, flat, 288, 1.0f / 144.0f);
		check(std::abs(slowFrame.GetPosition().z - fastFrame.GetPosition().z) < 0.5f, "30/144 fps movement agrees");
		ThirdPersonInput look;
		look.mouseX = 600.0f;
		look.mouseY = 10000.0f;
		ThirdPersonMotor orbit;
		orbit.Reset({ 0.0f, 0.1f, 0.0f }, 0.0f);
		orbit.Update(look, 1.0f / 60.0f, flat);
		check(std::abs(orbit.GetView().yaw - 90.0f) < 0.001f && orbit.GetView().pitch == 65.0f && orbit.GetBodyYaw() == 0.0f, "mouse orbits independently and clamps pitch");
		orbit.Update(walk, 1.0f / 60.0f, flat);
		check(orbit.GetPosition().x > 0.0f && std::abs(orbit.GetPosition().z) < 0.01f, "movement follows camera yaw");
		ThirdPersonScene blocked;
		AddFloor(blocked);
		blocked.AddTriangle({ -1000.0f, 0.0f, 250.0f }, { 1000.0f, 0.0f, 250.0f }, { 1000.0f, 500.0f, 250.0f });
		blocked.AddTriangle({ -1000.0f, 0.0f, 250.0f }, { 1000.0f, 500.0f, 250.0f }, { -1000.0f, 500.0f, 250.0f });
		blocked.Finalize();
		ThirdPersonMotor wall;
		wall.Reset({ 0.0f, 0.1f, 0.0f }, 0.0f);
		Simulate(wall, sprint, blocked, 180, 1.0f / 60.0f);
		check(wall.GetPosition().z <= 224.1f && wall.GetPosition().z > 220.0f && wall.GetSpeed() < 1.0f, "capsule cannot run through thin wall");
		const float cameraLength = blocked.CameraDistance({ 0.0f, 120.0f, 0.0f }, { 0.0f, 0.0f, 1.0f }, 330.0f, 16.0f);
		check(cameraLength > 230.0f && cameraLength < 235.0f, "camera retracts before wall");
		ThirdPersonMotor falling;
		falling.Reset({ 0.0f, 300.0f, 0.0f }, 0.0f);
		Simulate(falling, {}, flat, 120, 1.0f / 60.0f);
		check(falling.IsGrounded() && std::abs(falling.GetPosition().y) < 0.2f, "gravity lands on actual floor");

		// Engine::Run을 거치지 않으므로 World보다 먼저 Jolt를 초기화하고 더 늦게 종료한다.
		JoltRuntime joltRuntime;
		if (!joltRuntime.Initialize()) { return 1; }

		// 실제 AssetManager/World/AnimationComponent를 쓰되 GPU 준비는 요청하지 않는다.
		AssetManager assets;
		World world;
		GameObject* floor = world.CreateGameObject("TestFloor", {});
		std::shared_ptr<StaticMesh> mesh = std::make_shared<StaticMesh>("TestFloor", "");
		mesh->vertices.resize(4);
		mesh->vertices[0].position = { -3000.0f, 0.0f, -3000.0f };
		mesh->vertices[1].position = { 3000.0f, 0.0f, -3000.0f };
		mesh->vertices[2].position = { 3000.0f, 0.0f, 3000.0f };
		mesh->vertices[3].position = { -3000.0f, 0.0f, 3000.0f };
		mesh->indices = { 0, 1, 2, 0, 2, 3 };
		std::unique_ptr<StaticMeshComponent> component = std::make_unique<StaticMeshComponent>();
		component->SetStaticMesh(mesh);
		floor->AddComponent(std::move(component));
		GameObject* previous = world.CreateGameObject("PreviousCamera", {});
		previous->AddComponent(std::make_unique<CameraComponent>());
		world.SetActiveCamera(previous->GetComponent<CameraComponent>());
		ThirdPersonCharacter feature;
		const bool attached = feature.Attach(world, { floor }, { 0.0f, 170.0f, 0.0f }, 0.0f);
		check(attached, "real XBot loads and attaches without renderer");
		if (attached)
		{
			InputState input;
			input.PressKey(Key::W);
			input.PressKey(Key::Shift);
			for (int frame = 0; frame < 60; ++frame)
			{
				world.Tick(1.0f / 60.0f);
				feature.Update(input, 1.0f / 60.0f, true);
				input.ClearFrameEvents();
			}
			check(feature.GetMotor().GetPosition().z > 340.0f && world.GetActiveCamera() != previous->GetComponent<CameraComponent>(), "input drives live character and independent camera");
			feature.Detach();
			world.DestroyPendingGameObjects();
			check(world.GetActiveCamera() == previous->GetComponent<CameraComponent>(), "detach restores previous camera and destroys objects");
			check(feature.Attach(world, { floor }, { 0.0f, 170.0f, 0.0f }, 0.0f), "feature can attach again");
			feature.Detach();
			world.DestroyPendingGameObjects();
		}
		std::printf("THIRD_PERSON_TESTS failures=%d\n", failures);
		return failures == 0 ? 0 : 1;
	}
}
