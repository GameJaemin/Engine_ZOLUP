#include "VoxelDollComponent.h"

#include "AssetManager.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "Prefab/Prefab.h"
#include "World.h"

namespace
{
	constexpr std::array<Vector3, 6> Origins{
		Vector3{ 0.0f, 60.0f, 0.0f }, Vector3{ 0.0f, 92.0f, 0.0f },
		Vector3{ -31.0f, 76.0f, 0.0f }, Vector3{ 31.0f, 76.0f, 0.0f },
		Vector3{ -13.0f, 28.0f, 0.0f }, Vector3{ 13.0f, 28.0f, 0.0f }
	};
}

bool VoxelDollComponent::Initialize()
{
	GameObject* root = GetGameObject();
	if (!root || parts[0].IsValid()) { return false; }
	constexpr const char* names[]{ "Body", "Head", "LeftArm", "RightArm", "LeftLeg", "RightLeg" };
	for (std::size_t index = 0; index < parts.size(); ++index)
	{
		Prefab prefab;
		prefab.model = AssetManager::Get().LoadModel(std::string("Game/Content/BabyDoll/VoxelDoll/") + names[index] + ".gltf");
		if (!prefab.model) { return false; }
		Transform transform;
		transform.position = Origins[index];
		GameObject* part = root->GetWorld().InstantiatePrefab(prefab, { std::string("Voxel ") + names[index], transform });
		if (!part || !part->SetParent(root)) { return false; }
		parts[index] = part->GetObjectID();
	}
	return true;
}

void VoxelDollComponent::Reset(const Vector3& position)
{
	previousPosition = position;
	phase = elapsed = movement = crouch = fall = lean = previousYaw = 0.0f;
	initializedPosition = true;
	previousFoot=-1; stepPulse=actionTime=0.0f; wakeTime=1.0f;
	crawlAmount=0.0f;
}

GameObject* VoxelDollComponent::GetHandAnchor(int hand)
{
	if(!GetGameObject()||hand<0||hand>1) {return nullptr;}
	GameObject* object=GetGameObject()->GetWorld().FindGameObject(parts[hand+2]);
	return object&&!object->IsPendingDestroy()?object:nullptr;
}

void VoxelDollComponent::Present(const Vector3& position, float speed, float bodyYaw, float viewYaw, PlayerMode mode,
	float deltaTime, const PrototypeConfig& config, float fear, int lastFoot, bool crawlConstrained)
{
	if (!GetGameObject()) { return; }
	const bool moving = mode == PlayerMode::Moving || mode==PlayerMode::Ladder || mode==PlayerMode::Crawling;
	const float sprint = std::clamp((speed - 300.0f) / (std::max)(1.0f, config.maxSpeed - 300.0f), 0.0f, 1.0f);
	const float distance = initializedPosition ? PrototypeMath::Length(position - previousPosition) : 0.0f;
	previousPosition = position;
	initializedPosition = true;
	if (deltaTime > 0.0f)
	{
		elapsed += deltaTime;
		actionTime=(std::max)(0.0f,actionTime-deltaTime);
		wakeTime=(std::max)(0.0f,wakeTime-deltaTime);
		stepPulse*=std::exp(-deltaTime*10.0f);
		if(lastFoot>=0 && lastFoot!=previousFoot) {stepPulse=lastFoot==0?1.0f:-1.0f;}
		previousFoot=lastFoot;
		// 실제 전진 거리로 보폭을 맞춘다. 벽에 막히거나 정지하면 제자리 달리기를 하지 않는다.
		const float stride = config.dollStrideLength * (1.0f + sprint * 0.7f);
		if (moving && distance < 100.0f) { phase = std::fmod(phase + distance / stride * 6.2831853f, 6.2831853f); }
		const float response = PrototypeMath::Response(12.0f, deltaTime);
		crawlAmount+=((crawlConstrained||mode==PlayerMode::Crawling?1.0f:0.0f)-crawlAmount)*response;
		movement += ((moving ? std::clamp(speed / config.minimumWalkSpeed, 0.0f, 1.0f) : 0.0f) - movement) * response;
		crouch += ((mode == PlayerMode::Hidden || mode == PlayerMode::Exiting || mode==PlayerMode::Crawling || crawlConstrained ? 1.0f : 0.0f) - crouch) * response;
		fall += ((mode == PlayerMode::Fallen && !crawlConstrained ? 1.0f : 0.0f) - fall) * response;
		const float turn = std::remainder(bodyYaw - previousYaw, 360.0f) / deltaTime;
		lean += (std::clamp(-turn / 180.0f, -1.0f, 1.0f) * config.dollTurnLean * movement - lean) * response;
	}
	previousYaw = bodyYaw;
	const float jog=std::clamp((speed-200.0f)/280.0f,0.0f,1.0f);
	const float gait = (std::sin(phase)*movement+stepPulse*(1.0f-jog)*0.35f) * (1.0f-fall);
	const float scared=std::clamp(fear,0.0f,1.0f)*(1.0f-movement);
	TransformComponent* root = GetGameObject()->GetComponent<TransformComponent>();
	root->SetPosition({ 0.0f, std::abs(std::sin(phase)) * config.dollBodyBob * movement + fall * 12.0f + crawlAmount*26.0f, 0.0f });
	root->SetRotation({ crouch * 57.0f + crawlAmount*21.0f + (std::max)(fall * 82.0f,wakeTime*60.0f)*(1.0f-crawlAmount) + sprint * 7.0f, 0.0f, lean+std::sin(elapsed*19.0f)*scared*1.2f });
	root->SetScale({ 1.0f, 1.0f - crouch * 0.43f + crawlAmount*0.23f, 1.0f });
	for (std::size_t index = 0; index < parts.size(); ++index)
	{
		GameObject* part = GetGameObject()->GetWorld().FindGameObject(parts[index]);
		if (!part || part->IsPendingDestroy()) { continue; }
		TransformComponent* transform = part->GetComponent<TransformComponent>();
		Vector3 origin = Origins[index];
		EulerAngles rotation{};
		if (index == 0) { origin.y += std::sin(elapsed * 2.0f) * 0.65f * (1.0f - movement); rotation.roll = gait * 2.0f; }
		if (index == 1)
		{
			rotation.yaw = std::clamp(std::remainder(viewYaw - bodyYaw, 360.0f), -35.0f, 35.0f) * (1.0f - fall);
			rotation.roll = -gait * 2.5f;
			rotation.pitch = -sprint * 5.0f - crouch * 15.0f;
			rotation.pitch-=scared*8.0f;
		}
		if (index == 2 || index == 3)
		{
			const float side = index == 2 ? 1.0f : -1.0f;
			rotation.pitch = -side * gait * config.dollStepSwing * 0.75f - crouch * 55.0f - fall * 100.0f;
			rotation.roll = side * (6.0f + sprint * 8.0f);
			if(mode==PlayerMode::Hanging) {rotation.pitch=-175.0f;}
			if(mode==PlayerMode::Ladder) {rotation.pitch=-100.0f+gait*side*30.0f;}
			if(actionTime>0.0f && index==static_cast<std::size_t>(actionHand+2)) {rotation.pitch-=std::sin(actionTime/0.45f*3.1415927f)*65.0f;}
		}
		if (index == 4 || index == 5)
		{
			const float side = index == 4 ? 1.0f : -1.0f;
			rotation.pitch = side * gait * config.dollStepSwing - crouch * 30.0f;
			origin.y += (std::max)(0.0f, side * gait) * 4.0f;
		}
		transform->SetPosition(origin);
		transform->SetRotation(rotation);
	}
}
