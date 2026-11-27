#include "TraversalComponent.h"

namespace
{
	float Distance3(const Vector3& a, const Vector3& b) { return PrototypeMath::Length(a - b); }
}

std::optional<TraversalCameraShot> TraversalComponent::GetCameraShot(const Vector3& playerPosition) const
{
	if (!UsesFixedCamera()) { return std::nullopt; }
	const TraversalLink& link=links[active];
	TraversalCameraShot shot{link.cameraPosition,link.cameraTarget,link.cameraFov};
	if (link.kind==TraversalKind::Ladder)
	{
		// 오르기/내리기 모두 사다리 정면을 향하고 카메라는 그 반대편에 둔다.
		shot.target = playerPosition+Vector3{0.0f,link.cameraFollowHeight,0.0f};
		shot.position = shot.target-PrototypeMath::Direction(link.ladderFacingYaw)*link.ladderCameraDistance;
		shot.framingHeight=140.0f;
	}
	return shot;
}

int TraversalComponent::Find(const Vector3& position, float yaw, const GameplaySpace* space) const
{
	int selected = -1;
	float best = -2.0f;
	for (std::size_t index = 0; index < links.size(); ++index)
	{
		const TraversalLink& link = links[index];
		const Vector3 landing=link.end+link.exitOffset;
		const Vector3 target = link.kind != TraversalKind::Bar && Distance3(position, landing) < Distance3(position, link.start) ? landing : link.start;
		const Vector3 delta = target - position;
		const float distance = Distance3(position, target);
		if (distance > 180.0f) { continue; }
		if(space && distance>1.0f && space->CameraDistance(position+Vector3{0.0f,60.0f,0.0f},delta/distance,distance,0.0f)<distance-5.0f) {continue;}
		const float horizontal = PrototypeMath::Distance(position, target);
		Vector3 facingDirection=delta/(std::max)(horizontal,0.001f);
		if(link.kind==TraversalKind::Crawl)
		{
			const Vector3 direction=Distance3(position,landing)<Distance3(position,link.start)?link.start-link.end:link.end-link.start;
			facingDirection=direction/(std::max)(PrototypeMath::Distance(link.start,link.end),0.001f);
		}
		const float facing = horizontal < 35.0f ? 1.0f : facingDirection.Dot(PrototypeMath::Direction(yaw));
		const float score = facing - distance / 1000.0f;
		if (facing > 0.1f && score > best) { best = score; selected = static_cast<int>(index); }
	}
	return selected;
}

bool TraversalComponent::Begin(int index, PlayerMoveComponent& player, const PrototypeConfig& config, const GameplaySpace* space)
{
	if (IsActive() || player.GetMode() != PlayerMode::Moving || index < 0 || index >= static_cast<int>(links.size())) { return false; }
	const TraversalLink& link = links[index];
	if(link.kind==TraversalKind::Bar && stamina<=0.0f) {return false;}
	const Vector3 upperLanding=link.end+link.exitOffset;
	const bool fromEnd = link.kind != TraversalKind::Bar && Distance3(player.GetPosition(), upperLanding) < Distance3(player.GetPosition(), link.start);
	const Vector3 entry = fromEnd ? upperLanding : link.start;
	if (Distance3(player.GetPosition(), entry) > 180.0f) { return false; }
	float initialProgress=fromEnd?1.0f:0.0f;
	Vector3 initialPosition=link.kind==TraversalKind::Bar||fromEnd?link.end:entry;
	if(link.kind==TraversalKind::Crawl)
	{
		const Vector3 axis=link.end-link.start;
		initialProgress=std::clamp((player.GetPosition()-link.start).Dot(axis)/(std::max)(axis.Dot(axis),0.001f),0.0f,1.0f);
		initialPosition=Vector3::Lerp(link.start,link.end,initialProgress);
		if(space&&(!space->IsFree(initialPosition,config.playerRadius,75.0f)||!space->CanTraverse(player.GetPosition(),initialPosition,config.playerRadius,75.0f))) {return false;}
	}
	active = index;
	reverse = fromEnd;
	progress = initialProgress;
	targetProgress=progress;
	lastFoot = -1;
	stumbleTime=0.0f;
	slips = 0;
	sinceFoot = 10.0f;
	stamina = (std::min)(stamina,config.hangDuration);
	floor = static_cast<int>(std::floor((entry.y + 1.0f) / 640.0f));
	player.SetMode(link.kind == TraversalKind::Ladder ? PlayerMode::Ladder : link.kind == TraversalKind::Crawl ? PlayerMode::Crawling : PlayerMode::Hanging);
	if(link.kind==TraversalKind::Crawl)
	{
		const Vector3 direction=(fromEnd?link.start-link.end:link.end-link.start);
		player.SetYaw(std::atan2(direction.x,direction.z)*57.2957795f);
	}
	else if(link.kind==TraversalKind::Ladder) { player.SetYaw(link.ladderFacingYaw); }
	player.Teleport(initialPosition);
	return true;
}

bool TraversalComponent::Leave(PlayerMoveComponent& player, const GameplaySpace& space)
{
	if (!IsActive()) { return false; }
	if(stumbleTime>0.0f) {return false;}
	const TraversalLink& link = links[active];
	Vector3 landing = link.kind == TraversalKind::Bar ? link.start : player.GetPosition();
	// 통로 안에서는 해제해도 서지 않는다. 양 끝의 안전한 외부 지점에서만 이탈한다.
	if(link.kind==TraversalKind::Crawl)
	{
		if(progress>0.0001f&&progress<0.9999f) {return false;}
		landing=progress<0.5f?link.start:link.end;
	}
	// 사다리 중간에서는 낙하시키지 않는다. 안전한 양 끝에서만 놓을 수 있다.
	if (link.kind == TraversalKind::Ladder)
	{
		if (progress > 0.08f && progress < 0.92f) { return false; }
		landing = progress < 0.5f ? link.start : link.end+link.exitOffset;
	}
	if (!space.IsFree(landing, 30.0f, 120.0f)) { return false; }
	player.SetMode(PlayerMode::Moving);
	player.Teleport(landing);
	active = -1;
	speed = 0.0f;
	return true;
}

void TraversalComponent::Step(const PrototypeCommand& command, float deltaTime, const PrototypeConfig& config, const GameplaySpace& space, PlayerMoveComponent& player)
{
	mistake = false;
	speed = 0.0f;
	// 지상 회복 속도는 기획 미정이므로 추가하지 않는다. 새 판/복귀 때 용량을 초기화한다.
	if (!IsActive()) { return; }
	const TraversalLink& link = links[active];
	if(link.kind==TraversalKind::Ladder) { player.SetYaw(link.ladderFacingYaw); }
	if(stumbleTime>0.0f) {stumbleTime=(std::max)(0.0f,stumbleTime-deltaTime); if(stumbleTime<=0.0f) {player.SetMode(PlayerMode::Crawling);} return;}
	if (link.kind == TraversalKind::Bar)
	{
		stamina = (std::max)(0.0f, stamina - deltaTime);
		if (stamina <= 0.0f)
		{
			if (Leave(player, space)) { player.SetMode(PlayerMode::Fallen); mistake = true; }
		}
		return;
	}
	sinceFoot += deltaTime;
	const float length = Distance3(link.start, link.end);
	if (length < 1.0f) { Leave(player, space); return; }
	const Vector3 before = player.GetPosition();
	const float previousProgress=progress;
	for (int foot : command.feet)
	{
		if (foot == lastFoot)
		{
			if (link.kind == TraversalKind::Ladder)
			{
				++slips;
				targetProgress = (std::max)(0.0f, targetProgress - config.ladderSlip / length);
				mistake = slips >= static_cast<int>(config.ladderNoiseCount);
			}
			else { player.SetMode(PlayerMode::Fallen); stumbleTime=config.fallenTime; mistake = true; return; }
		}
		else if (link.kind == TraversalKind::Ladder) { targetProgress += (reverse ? -config.ladderStep : config.ladderStep) / length; }
		lastFoot = foot;
		sinceFoot = 0.0f;
	}
	if (link.kind == TraversalKind::Crawl && lastFoot >= 0 && sinceFoot <= config.rhythmWindow)
	{
		progress += (reverse ? -1.0f : 1.0f) * config.crawlSpeed * deltaTime / length;
	}
	targetProgress=std::clamp(targetProgress,0.0f,1.0f);
	if(link.kind==TraversalKind::Ladder) {progress+=std::clamp(targetProgress-progress,-300.0f*deltaTime/length,300.0f*deltaTime/length);}
	progress = std::clamp(progress, 0.0f, 1.0f);
	const Vector3 next = Vector3::Lerp(link.start, link.end, progress);
	if (link.kind == TraversalKind::Ladder || space.IsFree(next, config.playerRadius, 75.0f)) { player.Teleport(next); }
	else {progress=previousProgress;}
	speed = Distance3(player.GetPosition(), before) / deltaTime;
	const bool reached = reverse ? progress <= 0.0f : progress >= 1.0f;
	if (reached) { Leave(player, space); }
}
