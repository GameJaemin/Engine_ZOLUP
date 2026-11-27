#include "StitchVisualComponent.h"
#include "AssetManager.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "World.h"
#include "Prefab/Prefab.h"
#include "Renderer/Material.h"


bool StitchVisualComponent::Initialize(std::shared_ptr<const Model> box)
{
	if(!GetGameObject()||!box) {return false;}
	World& world=GetGameObject()->GetWorld();
	const char* names[]{"StitchTorsoLeft","StitchTorsoRight","StitchHeadLeft","StitchHeadRight","LeftArm","RightArm","LeftLeg","RightLeg"};
	for(std::size_t index=0;index<parts.size();++index)
	{
		Prefab prefab; prefab.model=AssetManager::Get().LoadModel(std::string("Game/Content/BabyDoll/VoxelDoll/")+names[index]+".gltf");
		if(!prefab.model) {return false;}
		GameObject* object=world.InstantiatePrefab(prefab,{std::string("Stitch ")+names[index],{}});
		if(!object||!object->SetParent(GetGameObject())) {return false;}
		parts[index]=object->GetObjectID();
	}
	const auto cube=[&](const char* name,const Vector3& color)
	{
		MaterialParameters p; p.baseColor=color; p.metallic=0.0f; p.roughness=0.8f;
		Prefab prefab; prefab.model=box; prefab.meshMaterialOverrides={std::make_shared<const Material>(name,std::string(),std::move(p))};
		GameObject* object=world.InstantiatePrefab(prefab,{std::string(name),{}});
		if(!object||!object->SetParent(GetGameObject())) {return ObjectID{};}
		return object->GetObjectID();
	};
	mouth=cube("Opened inner mouth",{0.055f,0.008f,0.015f});
	for(auto& id:tentacles) {id=cube("Tentacle segment",{0.3f,0.035f,0.075f}); if(!id.IsValid()) {return false;}}
	for(auto& id:teeth) {id=cube("Inner fang",{0.8f,0.7f,0.5f}); if(!id.IsValid()) {return false;}}
	return mouth.IsValid();
}

void StitchVisualComponent::PresentCapture(float elapsed)
{
	if (!GetGameObject()) { return; }
	opening = 1.0f;
	// 촉수는 원래 길이를 유지한다. 잡힘 중에는 경과 시간만으로 같은 자세를 재현한다.
	time = (std::max)(0.0f, elapsed);
	Present(StitchState::Chase, StitchAction::Waiting, 0.0f, 0.0f, 0.0f);
}
void StitchVisualComponent::Present(StitchState state,StitchAction action,float distance,float speed,float deltaTime)
{
	if(!GetGameObject()) {return;}
	time+=deltaTime; phase=std::fmod(phase+speed*deltaTime/260.0f*6.2831853f,6.2831853f);
	float target=opening;
	if(state==StitchState::Patrol) {target=0.0f;}
	else if(state==StitchState::Chase) {target=std::clamp((1500.0f-distance)/850.0f,0.0f,1.0f);}
	opening+=std::clamp(target-opening,-0.55f*deltaTime,0.9f*deltaTime);
	const float visible=(std::max)(0.001f,opening);
	const float gait=std::sin(phase)*std::clamp(speed/170.0f,0.0f,1.0f);
	World& world=GetGameObject()->GetWorld();
	const auto pose=[&](ObjectID id,const Vector3& p,const EulerAngles& r,const Vector3& scale)
	{
		GameObject* object=world.FindGameObject(id); if(!object||object->IsPendingDestroy()) {return;}
		TransformComponent* transform=object->GetComponent<TransformComponent>(); transform->SetPosition(p); transform->SetRotation(r); transform->SetScale(scale);
	};
	for(int index=0;index<4;++index)
	{
		const float side=index%2==0?-1.0f:1.0f;
		const bool head=index>=2;
		const float look=action==StitchAction::Looking?std::sin(time*2.2f)*18.0f:0.0f;
		pose(parts[index],{side*opening*(head?32.0f:45.0f),std::abs(gait)*3.0f,0.0f},
			{head&&action==StitchAction::Roaring?-18.0f:0.0f,head?look:0.0f,side*opening*(head?7.0f:3.0f)}, {1.0f,1.0f,1.0f});
	}
	for(int index=4;index<8;++index)
	{
		const float side=index%2==0?-1.0f:1.0f;
		const bool arm=index<6;
		const float attack=action==StitchAction::Smashing?(0.5f+0.5f*std::sin(time*8.0f))*85.0f:action==StitchAction::Roaring?65.0f:0.0f;
		pose(parts[index],{side*(arm?94.0f:38.0f),arm?185.0f:78.0f,0.0f},
			{side*gait*(arm?-25.0f:30.0f)-attack,0.0f,arm?side*10.0f:0.0f},{2.7f,2.7f,2.7f});
	}
	pose(mouth,{0.0f,192.0f,15.0f},{},{opening*1.15f+0.001f,1.55f*visible,0.35f*visible});
	for(int index=0;index<10;++index)
	{
		const float row=index<5?-1.0f:1.0f;
		pose(teeth[index],{(index%5-2)*18.0f*opening,192.0f+row*62.0f,40.0f},{0.0f,0.0f,row*12.0f},{0.12f*visible,0.30f*visible,0.18f*visible});
	}
	for(int arm=0;arm<6;++arm)
	{
		const float angle=(arm-2.5f)*0.5f;
		Vector3 start{0.0f,165.0f+(arm%2)*35.0f,35.0f};
		for(int segment=0;segment<3;++segment)
		{
			const float wave=std::sin(time*3.0f+arm+segment*0.6f);
			Vector3 direction{std::sin(angle+wave*0.13f),wave*0.3f,std::cos(angle+wave*0.13f)};
			direction=direction/(std::max)(PrototypeMath::Length(direction),0.001f);
			const float length=(80.0f-segment*12.0f)*visible;
			const Vector3 center=start+direction*(length*0.5f);
			const EulerAngles rotation{std::atan2(-direction.y,std::sqrt(direction.x*direction.x+direction.z*direction.z))*57.2957795f,std::atan2(direction.x,direction.z)*57.2957795f,0.0f};
			pose(tentacles[arm*3+segment],center,rotation,{(0.16f-segment*0.035f)*visible,(0.16f-segment*0.035f)*visible,length/100.0f});
			start=start+direction*length;
		}
	}
}
