#pragma once
#include "TraversalComponent.h"
#include "HandInventoryComponent.h"
#include "DoorComponent.h"
#include "ObjectSoundComponent.h"
#include "CheckpointIO.h"
#include "BabyDollApplication.h"
#include "Paths.h"
#include <functional>
#include <fstream>
#include <cstdio>
#include <chrono>

inline void RunChapterTests(const std::function<void(bool,const char*)>& check)
{
	PrototypeConfig config; GameplaySpace flat; PlayerMoveComponent player; TraversalComponent travel;
	player.Reset({});
	travel.links={ {TraversalKind::Ladder,{},{0.0f,640.0f,0.0f},{300.0f,500.0f,-300.0f}},
		{TraversalKind::Crawl,{500.0f,0.0f,0.0f},{1000.0f,0.0f,0.0f},{}},
		{TraversalKind::Bar,{},{0.0f,330.0f,0.0f},{}} };
	check(travel.Begin(0,player),"ladder can only begin from moving state at an endpoint");
	check(travel.UsesFixedCamera()&&player.GetYaw()==0.0f&&travel.GetCameraShot(player.GetPosition())->position.z<player.GetPosition().z,
		"ascending ladder faces its rungs and selects a camera behind the doll");
	PrototypeCommand foot; foot.feet={0}; travel.Step(foot,1.0f/120.0f,config,flat,player);
	for(int frame=0;frame<18;++frame) {travel.Step({},1.0f/120.0f,config,flat,player);}
	check(std::abs(player.GetPosition().y-42.0f)<0.01f,"alternating ladder input climbs exactly 42 cm");
	for (int i=1;i<16;++i) { foot.feet={i%2}; travel.Step(foot,1.0f/120.0f,config,flat,player); for(int frame=0;frame<18;++frame) {travel.Step({},1.0f/120.0f,config,flat,player);} }
	check(!travel.IsActive() && player.GetMode()==PlayerMode::Moving && player.GetPosition().y==640.0f,"ladder automatically exits at the upper landing");
	check(travel.Begin(0,player) && travel.UsesFixedCamera(),"descending a ladder selects its fixed camera");
	const auto upperShot=travel.GetCameraShot(player.GetPosition());
	const auto lowerShot=travel.GetCameraShot({0.0f,120.0f,0.0f});
	check(upperShot&&lowerShot&&std::abs(upperShot->position.y-lowerShot->position.y-520.0f)<0.01f
		&&std::abs(lowerShot->target.y-180.0f)<0.01f,
		"descending ladder camera and aim move down with the doll instead of staying at the upper landing");
	for(int i=0;i<16;++i) { foot.feet={i%2}; travel.Step(foot,1.0f/120.0f,config,flat,player); for(int frame=0;frame<18;++frame) {travel.Step({},1.0f/120.0f,config,flat,player);} }
	check(!travel.IsActive() && std::abs(player.GetPosition().y)<0.01f,"descending ladder exits safely at the lower landing");
	check(!travel.GetCameraShot(player.GetPosition()),"ladder landing releases the special camera back to orbit");
	for(float facing:{0.0f,90.0f,-135.0f})
	{
		TraversalComponent oriented; oriented.links={travel.links[0]}; oriented.links[0].ladderFacingYaw=facing;
		for(bool descending:{false,true})
		{
			PlayerMoveComponent climber; climber.Reset(descending?oriented.links[0].end:oriented.links[0].start);
			climber.SetYaw(facing+120.0f); oriented.Reset(); oriented.Begin(0,climber);
			climber.SetYaw(facing+60.0f); oriented.Step(foot,1.0f/120.0f,config,flat,climber);
			const auto shot=oriented.GetCameraShot(climber.GetPosition());
			check(std::abs(std::remainder(climber.GetYaw()-facing,360.0f))<0.01f&&shot
				&&(shot->position-shot->target).Dot(PrototypeMath::Direction(facing))<-299.0f,
				"ladder keeps its facing and rear camera for either travel direction and rotated placement");
		}
	}
	player.Reset({}); travel.Reset(); travel.Begin(0,player);
	for(int i=0;i<12;++i) { foot.feet={i%2}; travel.Step(foot,1.0f/120.0f,config,flat,player); for(int frame=0;frame<18;++frame) {travel.Step({},1.0f/120.0f,config,flat,player);} }
	const float beforeSlip=player.GetPosition().y;
	foot.feet={1}; travel.Step(foot,1.0f/120.0f,config,flat,player);
	for(int frame=0;frame<40;++frame) {travel.Step({},1.0f/120.0f,config,flat,player);}
	check(std::abs(player.GetPosition().y-(beforeSlip-90.0f))<0.02f && !travel.MadeMistake(),"first ladder slip loses 90 cm without loud noise");
	for(int i=0;i<4;++i) { travel.Step(foot,1.0f/120.0f,config,flat,player); }
	check(travel.MadeMistake(),"fifth slip on one ladder emits a mistake event");
	player.Reset({}); travel.Reset(); travel.Begin(2,player);
	check(travel.IsHidden() && player.GetMode()==PlayerMode::Hanging,"hanging counts as hidden with no footstep movement");
	travel.Step({},10.01f,config,flat,player);
	check(!travel.IsActive() && player.GetMode()==PlayerMode::Fallen && travel.MadeMistake(),"ten seconds of hanging exhausts stamina and causes a noisy fall");
	player.Reset({500.0f,0.0f,0.0f}); travel.Reset(); travel.Begin(1,player);
	for(int frame=0;frame<720 && travel.IsActive();++frame)
	{
		PrototypeCommand command; if(frame%48==0) {command.feet={frame/48%2};} travel.Step(command,1.0f/120.0f,config,flat,player);
	}
	check(!travel.IsActive() && player.GetPosition().x>999.0f,"crawl link carries alternating input to its exit");
	player.Reset({500.0f,0.0f,0.0f}); travel.Reset(); travel.Begin(1,player);
	foot.feet={0}; travel.Step(foot,1.0f/120.0f,config,flat,player); travel.Step(foot,1.0f/120.0f,config,flat,player);
	check(travel.IsActive()&&travel.IsHidden()&&player.GetMode()==PlayerMode::Fallen,"stumbling inside a crawl link preserves its protected traversal state");
	travel.Step({},config.fallenTime+0.01f,config,flat,player);
	check(player.GetMode()==PlayerMode::Crawling,"crawl stumble recovers to crawling rather than standing inside the passage");
	GameplaySpace passage;
	passage.obstacles={
		{{-550.0f,140.0f,1000.0f},{150.0f,180.0f,1200.0f}},
		{{-550.0f,0.0f,970.0f},{150.0f,180.0f,1000.0f}},
		{{-550.0f,0.0f,1200.0f},{150.0f,180.0f,1230.0f}},
		{{-550.0f,0.0f,1000.0f},{150.0f,140.0f,1200.0f},true,false,75.0f}
	};
	TraversalComponent crawl;
	crawl.links={{TraversalKind::Crawl,{-650.0f,0.0f,1100.0f},{250.0f,0.0f,1100.0f},{-900.0f,35.0f,1100.0f},180.0f,{}, {-200.0f,35.0f,1100.0f},30.0f}};
	const auto crawlFrame=[&](int frame)
	{
		PrototypeCommand command; if(frame%48==0) {command.feet={frame/48%2};}
		crawl.Step(command,1.0f/120.0f,config,passage,player);
	};
	player.Reset(crawl.links[0].start); crawl.Begin(0,player);
	for(int frame=0;frame<480;++frame) {crawlFrame(frame);}
	const Vector3 inside=player.GetPosition();
	check(!crawl.Leave(player,passage)&&crawl.IsActive()&&player.GetMode()==PlayerMode::Crawling&&PrototypeMath::Distance(player.GetPosition(),inside)<0.01f,
		"clicking inside a crawl passage cannot switch the player to standing movement");
	for(int frame=480;frame<660&&crawl.IsActive();++frame) {crawlFrame(frame);}
	check(!crawl.IsActive()&&player.GetMode()==PlayerMode::Moving&&passage.IsFree(player.GetPosition(),30.0f,120.0f)&&player.GetPosition().x>240.0f,
		"crawl reaches a clear standing landing within 5.5 seconds of alternating input");
	crawl.Reset(); player.Reset(crawl.links[0].end); crawl.Begin(0,player);
	for(int frame=0;frame<1400&&crawl.IsActive();++frame) {crawlFrame(frame);}
	check(!crawl.IsActive()&&player.GetPosition().x<-640.0f&&player.GetMode()==PlayerMode::Moving,"reverse crawl also exits outside the opposite entrance");
	const Vector3 walkAttempt=passage.Move(crawl.links[0].start,{400.0f,0.0f,0.0f},30.0f,120.0f);
	check(walkAttempt.x<=-579.0f&&!passage.CanTraverse(crawl.links[0].start,crawl.links[0].end,30.0f,120.0f)
		&&passage.CanTraverse(crawl.links[0].start,crawl.links[0].end,30.0f,75.0f),"standing movement cannot bypass the crawl-only passage entry");
	crawl.Reset(); player.Reset(walkAttempt);
	check(crawl.Find(player.GetPosition(),90.0f,&passage)==0&&crawl.Begin(0,player,config,&passage)&&PrototypeMath::Distance(player.GetPosition(),walkAttempt)<0.01f,
		"approaching the crawl mouth on foot can enter without looking backwards or snapping backwards");
	bool visibleRoute=true;
	for(int index=0;index<=10;++index)
	{
		const Vector3 point=Vector3::Lerp(crawl.links[0].start,crawl.links[0].end,index/10.0f)+Vector3{0.0f,35.0f,0.0f};
		const Vector3 ray=point-crawl.links[0].cameraPosition; const float distance=PrototypeMath::Length(ray);
		visibleRoute=visibleRoute&&passage.CameraDistance(crawl.links[0].cameraPosition,ray/distance,distance,1.0f)>=distance-2.0f;
	}
	check(visibleRoute,"fixed crawl camera sees the complete route through the open entrance without removing the ceiling");
	passage.obstacles.push_back({{-210.0f,0.0f,1000.0f},{-150.0f,140.0f,1200.0f}});
	crawl.Reset(); player.Reset(crawl.links[0].start); crawl.Begin(0,player);
	for(int frame=0;frame<2000;++frame) {crawlFrame(frame);}
	check(crawl.IsActive()&&player.GetPosition().x<-239.0f,"blocked crawl progress cannot advance invisibly and teleport past an obstacle");
	HandInventoryComponent hands;
	check(hands.Take(1,0)&&hands.Take(2,1)&&!hands.Take(3,0)&&!hands.CanUse(1,1),"two hands hold distinct items and only the matching hand can use a key");
	DoorComponent door;
	door.Step(1.0f,true); check(door.state.locked&&!door.state.targetOpen&&!door.IsPassable(),"standing beside a closed locked door does not automatically open it");
	check(!door.Use(2)&&door.Use(1)&&!door.state.locked,"door rejects a wrong key and remembers unlocking");
	door.Restore({}); door.OpenForMonster(true); door.Step(1.4f,false);
	check(!door.IsPassable(),"patrolling monster waits while opening a locked door");
	door.Step(0.1f,false); check(door.IsPassable(),"monster can open locked doors in 1.5 seconds");
	door.Restore({}); door.OpenForMonster(false); door.Step(0.2f,false); check(door.IsPassable(),"chasing monster opens a door in 0.2 seconds");
	HearingComponent hearing; hearing.Emit({},100.0f,SoundKind::Object,false,1,true,true,8,StitchState::Chase);
	check(hearing.Hear({5000.0f,0.0f,0.0f},0).has_value(),"object sound supports CrossFloor and IgnoreRadius independently of audio playback");
	hearing.Emit({},100.0f,SoundKind::Heart,true,0); check(hearing.Hear({},0)->player,"player sound takes priority over a high-priority object sound");
	ObjectSoundComponent objects;
	objects.rules.push_back({"door","Open",300.0f,0.0f,0.2f,1.0f,StitchState::Suspicious,false,false,false,3,""});
	hearing.BeginFrame(); objects.Trigger("door1","door","Open",{}); objects.Step(0.1f,hearing);
	check(hearing.Hear({})->reaction==StitchState::Suspicious,"object sound data carries an explicit AI reaction");
	hearing.BeginFrame(); objects.Step(0.2f,hearing); hearing.BeginFrame(); objects.Step(0.01f,hearing);
	check(!hearing.Hear({}),"sustained object sounds stop after their configured duration");
	GameplaySpace layered;
	layered.groundPatches={{{-150.0f,0.0f,-1000.0f},{150.0f,0.0f,280.0f},0.0f,640.0f,32},{{-500.0f,0.0f,280.0f},{500.0f,0.0f,1500.0f},640.0f,640.0f,0}};
	Vector3 walker{0.0f,0.0f,-1000.0f};
	for(int i=0;i<400;++i) { walker=layered.Move(walker,{0.0f,0.0f,4.0f},30.0f,120.0f); }
	check(walker.y==640.0f && walker.z>500.0f,"ground adapter climbs 32 twenty-centimetre steps onto the upper floor");
	const auto route=layered.FindPath({0.0f,0.0f,-1100.0f},{300.0f,640.0f,1000.0f},72.0f,296.0f);
	check(!route.empty() && route.back().y==640.0f,"layered navigation connects floors through stairs rather than teleporting vertically");
	AgentMovement follower; follower.Reset({0.0f,0.0f,-1100.0f},0.0f);
	int stalled=0;
	const auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(8);
	for(int frame=0;frame<3000 && stalled<60 && std::chrono::steady_clock::now()<deadline;++frame)
	{
		const Vector3 before=follower.GetPosition();
		follower.Step({300.0f,640.0f,1000.0f},170.0f,1.0f/120.0f,config,layered);
		stalled=PrototypeMath::Length(follower.GetPosition()-before)<0.001f?stalled+1:0;
	}
	if(follower.GetPosition().y!=640.0f) {std::printf("STAIR DIAGNOSTIC %.1f %.1f %.1f\n",follower.GetPosition().x,follower.GetPosition().y,follower.GetPosition().z);}
	check(follower.GetPosition().y==640.0f&&PrototypeMath::Distance(follower.GetPosition(),{300.0f,640.0f,1000.0f})<40.0f,
		"actual AI movement climbs the stairs and reaches its destination upstairs");
	StitchBrainComponent brain; brain.Reset({}); StitchObservation observation; observation.playerPosition={0.0f,640.0f,0.0f};
	brain.Step(observation,1.0f/120.0f,config,flat); check(!brain.HasCaughtPlayer(),"monster cannot catch a player on a different floor at the same XZ");
	brain.Reset({500.0f,0.0f,0.0f}); observation={}; observation.playerPosition={0.0f,330.0f,0.0f}; observation.hidden=true; observation.heartBPM=150.0f;
	observation.sound=SoundStimulus{observation.playerPosition,1500.0f,SoundKind::Heart}; brain.Step(observation,1.0f/120.0f,config,flat);
	check(brain.GetDestination().y==0.0f,"monster approaches the ground beneath a hanging player");
	observation.inCrawl=true;
	for(int frame=0;frame<300;++frame) {brain.Step(observation,1.0f/120.0f,config,flat);}
	check(brain.GetState()!=StitchState::Chase,"heartbeat inside an inaccessible crawl link does not restart an endless chase");
	// 체크포인트에 추격/포효/예약 소리와 최대 심박이 저장된 경우의 실제 재시작 순서.
	StitchSnapshot alerted=brain.Capture();
	alerted.state=StitchState::Chase; alerted.action=StitchAction::Roaring;
	alerted.resumeAfterRoar=true; alerted.deferredSound=SoundStimulus{{},4500.0f,SoundKind::Foot};
	alerted.stateTime=12.0f; alerted.actionTime=0.8f;
	brain.Restore(alerted);
	HeartComponent restartHeart; restartHeart.OnDetected();
	const auto elevatedHeart=restartHeart.Capture();
	restartHeart.Restore(elevatedHeart); restartHeart.Reset(config);
	const Vector3 restartPlayer{0.0f,0.0f,0.0f};
	check(brain.RestartPatrol({50.0f,0.0f,0.0f},restartPlayer,config,flat),
		"checkpoint restart finds a free distant patrol position when the saved monster is too close");
	const auto restarted=brain.Capture();
	check(restarted.state==StitchState::Patrol && restarted.action==StitchAction::Moving && !restarted.deferredSound
		&& !restarted.resumeAfterRoar && restarted.stateTime==0.0f && restarted.actionTime==0.0f && brain.GetPath().empty() && brain.GetSpeed()==0.0f,
		"checkpoint restart discards chase memory, paths, velocity and deferred noises");
	check(PrototypeMath::Distance(brain.GetPosition(),restartPlayer)>=config.heartFearRadius
		&& restartHeart.GetBPM()==config.heartRest && restartHeart.GetSoundRadius()==0.0f,
		"checkpoint restarts outside fear range with a silent resting heartbeat");
	HearingComponent restartHearing; restartHearing.Emit(restartPlayer,4500.0f,SoundKind::Foot);
	restartHearing.RestoreSequence(5);
	observation={}; observation.playerPosition=restartPlayer;
	for(int frame=0;frame<120;++frame)
	{
		restartHearing.BeginFrame();
		restartHeart.Step(0.0f,false,brain.GetState()==StitchState::Chase,false,PrototypeMath::Distance(brain.GetPosition(),restartPlayer),1.0f/120.0f,config);
		restartHearing.Emit(restartPlayer,restartHeart.GetSoundRadius(),SoundKind::Heart);
		observation.sound=restartHearing.Hear(brain.GetPosition());
		brain.Step(observation,1.0f/120.0f,config,flat);
	}
	check(brain.GetState()==StitchState::Patrol && !brain.HasCaughtPlayer(),
		"idle checkpoint restart does not immediately resume chase on restored heartbeat or stale sounds");
	restartHearing.BeginFrame(); restartHearing.Emit(restartPlayer,4500.0f,SoundKind::Foot);
	observation.sound=restartHearing.Hear(brain.GetPosition()); brain.Step(observation,1.0f/120.0f,config,flat);
	check(brain.GetState()==StitchState::Chase,"new audible player noise still triggers chase after checkpoint restart");
	const Vector3 distant{0.0f,0.0f,1000.0f};
	check(brain.RestartPatrol(distant,{0.0f,0.0f,-1100.0f},config,flat) && PrototypeMath::Distance(brain.GetPosition(),distant)<0.01f,
		"safe saved monster positions are retained instead of always teleporting to the initial spawn");
	observation={}; observation.playerPosition={0.0f,640.0f,0.0f};
	brain.Step(observation,1.0f/120.0f,config,flat);
	check(PrototypeMath::Distance(brain.GetDestination(),StitchBrainComponent::PatrolRoute[0])<0.01f,
		"patrol on another floor follows its route without knowing the player's location");
	GameplaySpace blockedRestart=flat; blockedRestart.obstacles.push_back({{-2200.0f,-1.0f,-1800.0f},{2200.0f,1000.0f,1800.0f}});
	check(!brain.RestartPatrol({},restartPlayer,config,blockedRestart),"checkpoint restart rejects a map with no free safe patrol spawn");
	PrototypeCheckpoint snapshot; snapshot.valid=true; snapshot.playerPosition={100.0f,0.0f,100.0f}; snapshot.hands={1,2}; snapshot.progress.stage=EscapeStage::ReachExit;
	snapshot.tables[0]={0,true}; snapshot.door={false,true,1.0f}; snapshot.monster=brain.Capture(); snapshot.props.push_back({});
	std::filesystem::path file;
	if(Paths::GetAbsolutePath("Intermediate/BabyDoll/Tests/checkpoint-roundtrip.json",file))
	{
		PrototypeCheckpoint loaded;
		check(WritePrototypeCheckpoint(file,snapshot)&&ReadPrototypeCheckpoint(file,loaded)&&loaded.hands==snapshot.hands&&loaded.tables[0].broken&&loaded.door.opening==1.0f,
			"checkpoint disk roundtrip restores hands, door and broken table state");
		{ std::ofstream corrupt(file,std::ios::trunc); corrupt<<"{\"version\":2}"; }
		const Vector3 previous=loaded.playerPosition;
		check(!ReadPrototypeCheckpoint(file,loaded)&&PrototypeMath::Distance(loaded.playerPosition,previous)==0.0f,"invalid checkpoints are rejected without partially replacing the current state");
		std::error_code error; std::filesystem::remove(file,error);
	}
}
