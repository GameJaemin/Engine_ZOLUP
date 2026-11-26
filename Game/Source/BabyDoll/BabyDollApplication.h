#pragma once

#include "GameplaySpace.h"
#include "HideTableComponent.h"
#include "PrototypeInputAdapter.h"
#include "PhysicsProps.h"

#include "IApplication.h"
#include "ObjectID.h"

#include <array>
#include <memory>
#include <string>

class Engine;
class World;
class GameObject;
class CameraComponent;
class PlayerMoveComponent;
class HeartComponent;
class HearingComponent;
class StitchBrainComponent;
class OrbitCameraComponent;
class PoseAnimatorComponent;
class RearCameraComponent;
class ProximityLightComponent;
class Model;

struct PrototypeCheckpoint
{
	Vector3 playerPosition;
	Vector3 monsterPosition;
	std::array<HideTableSnapshot, 2> tables;
	bool valid = false;
};

/** 조립과 실행 순서만 담당한다. 컴포넌트는 World 소유이며 이 앱이 World보다 먼저 참조를 비운다. */
class BabyDollApplication final : public IApplication
{
public:
	BabyDollApplication();
	~BabyDollApplication() override;
	bool UsesGui() const noexcept override { return true; }
	bool OnInitialize(Engine& inEngine) override;
	void OnUpdate(const InputState& input, float deltaTime) override;
	void OnGui() override;
	std::optional<SceneRenderRequest> GetRenderRequest() override;
	void OnShutdown() noexcept override;

private:
	GameObject* Box(const std::string& name, const Vector3& center, const Vector3& size, const Vector3& color, GameObject* parent = nullptr, bool collides = true);
	bool BuildRoom();
	bool BuildMonster();
	void SetCaptured(bool captured);
	void ResetRun();
	void Simulate(const PrototypeCommand& command, float deltaTime);
	void Present(float deltaTime);
	void Interact();
	void LeaveTable(bool forced);
	void ApplyTableVisuals();
	void DrawHud();
	void DrawPanel();
	void DrawRearView();
	int FindInteraction() const;

private:
	Engine* engine = nullptr;
	std::unique_ptr<World> world;
	PrototypeConfig config;
	PrototypeInputAdapter inputAdapter;
	GameplaySpace space;
	PhysicsProps physicsProps;
	std::shared_ptr<const Model> boxModel;
	std::shared_ptr<const Model> ringModel;
	std::shared_ptr<const Model> monsterModel;
	GameObject* playerObject = nullptr;
	GameObject* playerVisual = nullptr;
	GameObject* monsterObject = nullptr;
	GameObject* monsterVisual = nullptr;
	GameObject* soundRing = nullptr;
	CameraComponent* camera = nullptr;
	PlayerMoveComponent* player = nullptr;
	HeartComponent* heart = nullptr;
	HearingComponent* hearing = nullptr;
	StitchBrainComponent* brain = nullptr;
	OrbitCameraComponent* orbit = nullptr;
	PoseAnimatorComponent* animator = nullptr;
	RearCameraComponent* rearView = nullptr;
	std::vector<ProximityLightComponent*> lamps;
	std::array<HideTableComponent*, 2> tables{};
	std::array<GameObject*, 2> tableVisuals{};
	PrototypeCheckpoint checkpoint;
	PrototypeCommand pending;
	Vector3 exitStart;
	Vector3 exitDestination;
	Vector3 previousPlayerPosition;
	Vector3 previousMonsterPosition;
	float previousMonsterYaw = 180.0f;
	float monsterAnimationSpeed = 0.0f;
	bool monsterLocomoting = false;
	int occupiedTable = -1;
	float accumulator = 0.0f;
	float gameTime = 0.0f;
	float exitTime = 0.0f;
	float caughtTime = 0.0f;
	float footRadius = 0.0f;
	float soundRadius = 0.0f;
	float monsterScale = 1.0f;
	float monsterFloorOffset = 0.0f;
	float visualYawOffset = 0.0f;
	float toastTime = 0.0f;
	std::string toast;
	int inspectClip = 0;
	bool inspect = true;
	bool paused = true;
	bool captured = false;
	bool justCaptured = false;
	bool freeMove = false;
	bool soloControls = true;
	bool previewRear = false;
	bool lightsEnabled = true;
	bool leftFootDown = false;
	bool rightFootDown = false;
	bool physicsHealthy = true;
	bool dropPropsRequested = false;
	bool showDebug = false;
	bool aiEnabled = true;
	bool godMode = false;
	bool resetRequested = false;
};
