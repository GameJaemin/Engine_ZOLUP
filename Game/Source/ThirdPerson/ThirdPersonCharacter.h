#pragma once

#include "ThirdPersonAnimator.h"

#include "AssetManager.h"
#include "Component/CameraComponent.h"
#include "Core/Log.h"
#include "Input/InputState.h"
#include "Input/Key.h"
#include "ObjectID.h"
#include "Prefab/Prefab.h"
#include "World.h"

#include <cmath>
#include <format>
#include <limits>
#include <memory>
#include <vector>

/**
 * 컨텐츠 측 3인칭 기능의 연결 단위다. World가 객체를 소유하고 이 클래스는 ID만 보관한다.
 * World보다 먼저 파괴하거나 Detach해야 한다. 코드와 문서를 폴더째 옮겨 재사용한다.
 */
class ThirdPersonCharacter
{
public:
	ThirdPersonCharacter() = default;
	~ThirdPersonCharacter()
	{
		Detach();
	}

	ThirdPersonCharacter(const ThirdPersonCharacter&) = delete;
	ThirdPersonCharacter& operator=(const ThirdPersonCharacter&) = delete;

	/** levelRoots는 스폰 전에 로드한 정적 맵이다. scene은 맵 배치가 바뀌면 다시 구성한다. */
	bool Attach(World& targetWorld, const std::vector<GameObject*>& levelRoots, const Vector3& spawnHint, float initialYaw, const ThirdPersonConfig& newConfig = {})
	{
		Detach();
		config = newConfig;
		if (config.walkSpeed <= 0.0f || config.runSpeed <= config.walkSpeed || config.radius <= 0.0f || config.height < 2.0f * config.radius) { return false; }
		const std::shared_ptr<const Model> model = AssetManager::Get().LoadModel(config.modelPath);
		animator = ThirdPersonAnimator{};
		if (model == nullptr || !animator.Initialize(model, config))
		{
			Log::Write(LogLevel::Error, "ThirdPerson", "Character model must contain Idle, Walk and Run clips");
			return false;
		}
		scene.Build(levelRoots);
		if (!FindSpawn(spawnHint, home))
		{
			Log::Write(LogLevel::Error, "ThirdPerson", "No walkable floor with capsule clearance near the spawn camera");
			return false;
		}
		world = &targetWorld;
		if (const CameraComponent* previous = world->GetActiveCamera())
		{
			previousCameraID = previous->GetGameObject()->GetObjectID();
		}
		GameObject* root = world->CreateGameObject("ThirdPersonCharacter", {});
		if (root == nullptr) { return FailAttach(); }
		rootID = root->GetObjectID();
		Prefab prefab;
		prefab.model = model;
		prefab.root.name = "XBotVisual";
		GameObject* visual = world->InstantiatePrefab(prefab, {});
		if (visual == nullptr) { return FailAttach(); }
		visualID = visual->GetObjectID();
		if (!visual->SetParent(root)) { return FailAttach(); }
		AnimationComponent* animation = visual->GetComponent<AnimationComponent>();
		if (animation == nullptr) { return FailAttach(); }
		// 원본 바인드 메시의 최저점을 캐릭터 발바닥 원점에 맞춘다.
		float minimumY = (std::numeric_limits<float>::max)();
		for (std::size_t node = 0; node < model->sceneNodes.size(); ++node)
		{
			if (!model->sceneNodes[node].mesh.has_value()) { continue; }
			Matrix4x4 matrix = model->sceneNodes[node].localTransform.ToMatrix();
			std::optional<std::size_t> parent = model->sceneNodes[node].parent;
			while (parent.has_value())
			{
				matrix = matrix * model->sceneNodes[*parent].localTransform.ToMatrix();
				parent = model->sceneNodes[*parent].parent;
			}
			for (const MeshVertex& vertex : model->meshes[*model->sceneNodes[node].mesh]->vertices)
			{
				minimumY = (std::min)(minimumY, ThirdPersonMath::TransformPoint(vertex.position, matrix).y);
			}
		}
		if (!std::isfinite(minimumY) || minimumY == (std::numeric_limits<float>::max)()) { return FailAttach(); }
		visual->GetComponent<TransformComponent>()->SetPosition({ 0.0f, -minimumY, 0.0f });
		visual->GetComponent<TransformComponent>()->SetRotation({ 0.0f, config.visualYawOffset, 0.0f });
		GameObject* cameraObject = world->CreateGameObject("ThirdPersonCamera", {});
		if (cameraObject == nullptr) { return FailAttach(); }
		cameraID = cameraObject->GetObjectID();
		std::unique_ptr<CameraComponent> newCamera = std::make_unique<CameraComponent>();
		CameraComponent* camera = newCamera.get();
		if (!camera->SetVerticalFovDegrees(config.cameraFov) || !camera->SetClipDistances(5.0f, 10000.0f)) { return FailAttach(); }
		cameraObject->AddComponent(std::move(newCamera));
		if (!world->SetActiveCamera(camera)) { return FailAttach(); }
		motor = ThirdPersonMotor(config);
		spawnYaw = initialYaw;
		Reset();
		animator.Update(*animation, 0.0f, 0.0f);
		Log::Write(LogLevel::Info, "ThirdPerson", std::format("Ready: {} collision triangles, spawn [{:.1f}, {:.1f}, {:.1f}] cm. WASD walk, Shift run, mouse orbit, Escape release, click capture, R reset.", scene.GetTriangleCount(), home.x, home.y, home.z));
		return true;
	}

	void Detach() noexcept
	{
		if (world == nullptr) { return; }
		GameObject* camera = world->FindGameObject(cameraID);
		if (camera != nullptr && world->GetActiveCamera() == camera->GetComponent<CameraComponent>())
		{
			GameObject* previous = world->FindGameObject(previousCameraID);
			world->SetActiveCamera(previous != nullptr ? previous->GetComponent<CameraComponent>() : nullptr);
		}
		for (const ObjectID& id : { rootID, visualID, cameraID })
		{
			if (GameObject* object = world->FindGameObject(id))
			{
				world->DestroyGameObject(object);
			}
		}
		world = nullptr;
		rootID = visualID = cameraID = previousCameraID = {};
	}

	/** GameApplication이 World::Tick 뒤에 한 번 호출하며 입력은 그 프레임만 빌린다. */
	void Update(const InputState& input, float deltaTime, bool acceptsInput)
	{
		if (world == nullptr) { return; }
		GameObject* root = world->FindGameObject(rootID);
		GameObject* visual = world->FindGameObject(visualID);
		GameObject* camera = world->FindGameObject(cameraID);
		if (root == nullptr || visual == nullptr || camera == nullptr)
		{
			Detach();
			return;
		}
		ThirdPersonInput command;
		if (acceptsInput)
		{
			command.right = static_cast<float>(input.IsKeyDown(Key::D)) - static_cast<float>(input.IsKeyDown(Key::A));
			command.forward = static_cast<float>(input.IsKeyDown(Key::W)) - static_cast<float>(input.IsKeyDown(Key::S));
			command.running = input.IsKeyDown(Key::Shift);
			const RawMouseDelta mouse = input.GetRawMouseDelta();
			command.mouseX = static_cast<float>(mouse.x);
			command.mouseY = static_cast<float>(mouse.y);
			for (const KeyEvent& event : input.GetFrameEvents())
			{
				if (event.key == Key::R && event.type == KeyEventType::Pressed)
				{
					Reset();
				}
			}
		}
		motor.Update(command, deltaTime, scene);
		if (motor.GetPosition().y < home.y - 2000.0f)
		{
			Reset();
		}
		root->GetComponent<TransformComponent>()->SetPosition(motor.GetPosition());
		root->GetComponent<TransformComponent>()->SetRotation({ 0.0f, motor.GetBodyYaw(), 0.0f });
		if (AnimationComponent* animation = visual->GetComponent<AnimationComponent>())
		{
			animator.Update(*animation, motor.GetSpeed(), deltaTime);
		}
		UpdateCamera(*camera, deltaTime);
	}

	const ThirdPersonMotor& GetMotor() const { return motor; }

private:
	bool FailAttach()
	{
		Detach();
		return false;
	}

	bool FindSpawn(const Vector3& hint, Vector3& result) const
	{
		for (int ring = 0; ring <= 8; ++ring)
		{
			const int samples = ring == 0 ? 1 : 16;
			for (int sample = 0; sample < samples; ++sample)
			{
				const float angle = 6.283185307f * static_cast<float>(sample) / static_cast<float>(samples);
				Vector3 point = hint + Vector3{ std::sin(angle) * ring * 45.0f, 0.0f, std::cos(angle) * ring * 45.0f };
				float floor = 0.0f;
				if (!scene.FindGround(point, hint.y - 25.0f, hint.y - 800.0f, floor)) { continue; }
				point.y = floor + 0.1f;
				if (ThirdPersonMath::Length(scene.GetPenetration(point, config.radius + 1.0f, config.height)) > 0.1f) { continue; }
				result = point;
				return true;
			}
		}
		return false;
	}

	void Reset()
	{
		motor.Reset(home, spawnYaw);
		cameraPivot = home + Vector3{ 0.0f, config.cameraHeight, 0.0f };
		cameraLength = config.cameraDistance;
		if (GameObject* root = world->FindGameObject(rootID))
		{
			root->GetComponent<TransformComponent>()->SetPosition(home);
		}
		if (GameObject* camera = world->FindGameObject(cameraID))
		{
			UpdateCamera(*camera, 0.0f);
		}
	}

	void UpdateCamera(GameObject& camera, float deltaTime)
	{
		const float elapsed = std::clamp(deltaTime, 0.0f, 0.1f);
		const Vector3 desiredPivot = motor.GetPosition() + Vector3{ 0.0f, config.cameraHeight, 0.0f };
		cameraPivot = Vector3::Lerp(cameraPivot, desiredPivot, 1.0f - std::exp(-18.0f * elapsed));
		const EulerAngles view = motor.GetView();
		const float yaw = Math::DegreesToRadians(view.yaw);
		const float pitch = Math::DegreesToRadians(view.pitch);
		const Vector3 backward{ -std::sin(yaw) * std::cos(pitch), std::sin(pitch), -std::cos(yaw) * std::cos(pitch) };
		const float available = scene.CameraDistance(cameraPivot, backward, config.cameraDistance, 16.0f);
		cameraLength = available < cameraLength ? available : cameraLength + (available - cameraLength) * (1.0f - std::exp(-5.0f * elapsed));
		TransformComponent* transform = camera.GetComponent<TransformComponent>();
		transform->SetPosition(cameraPivot + backward * cameraLength);
		transform->SetRotation(view);
	}

private:
	World* world = nullptr;
	ObjectID rootID;
	ObjectID visualID;
	ObjectID cameraID;
	ObjectID previousCameraID;
	ThirdPersonConfig config;
	ThirdPersonScene scene;
	ThirdPersonMotor motor;
	ThirdPersonAnimator animator;
	Vector3 home;
	Vector3 cameraPivot;
	float cameraLength = 0.0f;
	float spawnYaw = 0.0f;
};
