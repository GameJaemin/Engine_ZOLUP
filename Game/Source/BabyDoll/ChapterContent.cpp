#include "BabyDollApplication.h"
#include "TraversalComponent.h"
#include "HandInventoryComponent.h"
#include "ObjectSoundComponent.h"
#include "PlayerMoveComponent.h"
#include "StitchBrainComponent.h"
#include "OrbitCameraComponent.h"
#include "Component/TransformComponent.h"
#include "GameObject.h"
#include "World.h"
#include "VoxelDollComponent.h"

bool BabyDollApplication::BuildChapter()
{
	// 기능 검증용 임시 배치다. 정식 방 위치나 퍼즐 조건을 확정하지 않는다.
	space.groundPatches = {
		{{1050.0f,0.0f,280.0f},{1620.0f,0.0f,1650.0f},640.0f,640.0f,0},
		{{1780.0f,0.0f,280.0f},{2050.0f,0.0f,1650.0f},640.0f,640.0f,0},
		{{1620.0f,0.0f,280.0f},{1780.0f,0.0f,320.0f},640.0f,640.0f,0},
		{{1620.0f,0.0f,500.0f},{1780.0f,0.0f,1650.0f},640.0f,640.0f,0},
		{{1350.0f,0.0f,-1000.0f},{1650.0f,0.0f,280.0f},0.0f,640.0f,32}
	};
	for(const auto& patch:space.groundPatches)
	{
		if(patch.steps>0) {continue;}
		const Vector3 minimum{patch.minimum.x,600.0f,patch.minimum.z};
		const Vector3 maximum{patch.maximum.x,640.0f,patch.maximum.z};
		Box("Upper floor around ladder opening",(minimum+maximum)*0.5f,maximum-minimum,{0.18f,0.17f,0.23f},nullptr,false);
		space.obstacles.push_back({minimum,maximum,true,true});
	}
	for (int step = 0; step < 32; ++step)
	{
		const float top = (step + 1) * 20.0f;
		Box("Stair tread", {1500.0f,top-10.0f,-980.0f+step*40.0f}, {300.0f,20.0f,40.0f}, {0.22f,0.24f,0.29f}, nullptr, false);
		space.obstacles.push_back({{1350.0f,top-20.0f,-1000.0f+step*40.0f},{1650.0f,top,-960.0f+step*40.0f},true,true});
	}
	const PrototypeObstacle roomWalls[]{
		{{1000.0f,640.0f,280.0f},{1050.0f,1240.0f,1700.0f}},
		{{2050.0f,640.0f,280.0f},{2100.0f,1240.0f,1700.0f}},
		{{1050.0f,640.0f,1650.0f},{2050.0f,1240.0f,1700.0f}},
		{{1050.0f,1240.0f,280.0f},{2050.0f,1280.0f,1700.0f}}
	};
	for (const auto& wall : roomWalls)
	{
		space.obstacles.push_back(wall);
		Box("Research room shell", (wall.minimum+wall.maximum)*0.5f, wall.maximum-wall.minimum, {0.09f,0.12f,0.17f}, nullptr, false);
	}
	traversal->links = {
		{TraversalKind::Ladder,{1700.0f,0.0f,400.0f},{1700.0f,640.0f,400.0f},{1730.0f,700.0f,750.0f},180.0f,{0.0f,0.0f,150.0f},{1700.0f,700.0f,400.0f},80.0f},
		{TraversalKind::Crawl,{-650.0f,0.0f,1100.0f},{250.0f,0.0f,1100.0f},{-900.0f,35.0f,1100.0f},180.0f,{}, {-200.0f,35.0f,1100.0f},30.0f},
		{TraversalKind::Bar,{250.0f,0.0f,-1250.0f},{250.0f,330.0f,-1250.0f},{}}
	};
	for (float x : {1650.0f,1750.0f}) { Box("Ladder rail",{x,320.0f,440.0f},{12.0f,680.0f,12.0f},{0.75f,0.55f,0.2f},nullptr,false); }
	for (int step=0; step<16; ++step) { Box("Ladder rung",{1700.0f,step*42.0f,440.0f},{110.0f,8.0f,12.0f},{0.75f,0.55f,0.2f},nullptr,false); }
	Box("Hanging bar",{250.0f,450.0f,-1250.0f},{230.0f,12.0f,12.0f},{0.75f,0.55f,0.2f},nullptr,false);
	const PrototypeObstacle tunnel[]{
		{{-550.0f,140.0f,1000.0f},{150.0f,180.0f,1200.0f}},
		{{-550.0f,0.0f,970.0f},{150.0f,180.0f,1000.0f}},
		{{-550.0f,0.0f,1200.0f},{150.0f,180.0f,1230.0f}}
	};
	space.obstacles.push_back({{-550.0f,0.0f,1000.0f},{150.0f,140.0f,1200.0f},true,false,75.0f});
	for (const auto& wall : tunnel)
	{
		space.obstacles.push_back(wall);
		Box("Doll passage",(wall.minimum+wall.maximum)*0.5f,wall.maximum-wall.minimum,{0.16f,0.29f,0.3f},nullptr,false);
	}
	auto doorOwner = std::make_unique<DoorComponent>();
	exitDoor = doorOwner.get();
	exitDoor->position = layout.doorPosition;
	doorVisual->AddComponent(std::move(doorOwner));
	secondKeyVisual = world->CreateGameObject("Silver key",{});
	if(!secondKeyVisual) {return false;}
	secondKeyVisual->GetComponent<TransformComponent>()->SetPosition({1200.0f,740.0f,1200.0f});
	Box("Silver shaft",{},{10.0f,40.0f,10.0f},{0.65f,0.8f,0.9f},secondKeyVisual,false);
	Box("Silver head",{0.0f,27.0f,0.0f},{30.0f,25.0f,10.0f},{0.65f,0.8f,0.9f},secondKeyVisual,false);
	Box("Research desk",{1200.0f,680.0f,1200.0f},{160.0f,65.0f,110.0f},{0.3f,0.18f,0.15f},nullptr,false);
	Box("Elevator floor",elevatorPosition+Vector3{0.0f,2.0f,0.0f},{260.0f,4.0f,260.0f},{0.3f,0.65f,0.5f},nullptr,false);
	for (float x : {-140.0f,140.0f}) { Box("Elevator frame",elevatorPosition+Vector3{x,180.0f,100.0f},{20.0f,360.0f,220.0f},{0.35f,0.25f,0.12f},nullptr,false); }
	Box("Elevator header",elevatorPosition+Vector3{0.0f,360.0f,100.0f},{300.0f,20.0f,220.0f},{0.35f,0.25f,0.12f},nullptr,false);
	escapeVisual->GetComponent<TransformComponent>()->SetScale({0.001f,0.001f,0.001f});
	return secondKeyVisual != nullptr;
}

void BabyDollApplication::TeleportForTest(const Vector3& position)
{
	traversal->Reset(config.hangDuration); player->Reset(position); previousPlayerPosition=position; previousPlayerYaw=0.0f;
	doll->Reset(position); orbit->Reset(position+Vector3{0.0f,160.0f,0.0f},0.0f,config);
	inspect=paused=false; SetCaptured(true);
}

void BabyDollApplication::UseHand(int hand)
{
	doll->UseHand(hand);
	if (traversal->IsActive())
	{
		if(traversal->GetLink()->kind!=TraversalKind::Bar||hand==0)
		{
			if(!traversal->Leave(*player,space)&&traversal->GetLink()->kind==TraversalKind::Crawl) {toast="Keep crawling to an exit before standing up."; toastTime=2.0f;}
		}
		return;
	}
	if (player->GetMode() == PlayerMode::Hidden) { if (hand == 1) { Interact(); } return; }
	if (player->GetMode() != PlayerMode::Moving) { return; }
	const int link = traversal->Find(player->GetPosition(),orbit->GetYaw(),&space);
	if (link >= 0 && (traversal->links[link].kind!=TraversalKind::Bar || hand==0) && traversal->Begin(link,*player,config,&space)) { return; }
	const Vector3 silverPosition{1200.0f,640.0f,1200.0f};
	if (!inventory->Contains(2) && std::abs(player->GetPosition().y - silverPosition.y) < 80.0f && PrototypeMath::Distance(player->GetPosition(),silverPosition) < 150.0f)
	{
		if (inventory->Take(2,hand)) { ApplyProgressVisuals(); checkpointRequested = true; }
		return;
	}
	const int objective = FindObjectiveInteraction();
	if (objective == 1)
	{
		if (inventory->Take(1,hand) && progress.TakeKey()) { ApplyProgressVisuals(); checkpointRequested = true; }
		return;
	}
	const auto bounds = layout.Door();
	const Vector3 nearest{std::clamp(player->GetPosition().x,bounds.minimum.x,bounds.maximum.x),0.0f,std::clamp(player->GetPosition().z,bounds.minimum.z,bounds.maximum.z)};
	const Vector3 delta = nearest - player->GetPosition();
	const float distance = PrototypeMath::Distance(nearest,player->GetPosition());
	const bool facing = distance < 30.0f || (delta/(std::max)(distance,1.0f)).Dot(PrototypeMath::Direction(orbit->GetYaw())) > 0.15f;
	if (std::abs(player->GetPosition().y) < 80.0f && distance <= layout.interactionDistance && facing
		&& (distance < 1.0f || space.CameraDistance(player->GetPosition()+Vector3{0.0f,65.0f,0.0f},delta/distance,distance,0.0f,doorObstacle) >= distance-5.0f))
	{
		if (exitDoor->Use(inventory->Get(hand)))
		{
			if (progress.HasKey()) { progress.stage = exitDoor->state.targetOpen ? EscapeStage::ReachExit : EscapeStage::ReturnToExit; }
			objectSounds->Trigger("exit","door",exitDoor->state.targetOpen ? "Open" : "Close",layout.doorPosition);
		}
		else { toast = "Use the hand holding the matching key."; toastTime = 2.0f; }
		return;
	}
	if (hand == 1) { Interact(); }
}
