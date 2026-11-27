#include "BabyDollApplication.h"
#include "CheckpointIO.h"
#include "HandInventoryComponent.h"
#include "TraversalComponent.h"
#include "PlayerMoveComponent.h"
#include "Paths.h"
#include "Core/Log.h"

void BabyDollApplication::ClearCheckpointFile()
{
	std::filesystem::path path;
	if (Paths::GetAbsolutePath("Binaries/BabyDollSave/checkpoint.json",path)) { std::error_code error; std::filesystem::remove(path,error); }
}

void BabyDollApplication::SaveCheckpoint()
{
	if (player->GetMode()!=PlayerMode::Moving) { return; }
	checkpoint.playerPosition=player->GetPosition(); checkpoint.monsterPosition=brain->GetPosition(); checkpoint.valid=true;
	checkpoint.progress=progress; checkpoint.hands=inventory->Capture(); checkpoint.door=exitDoor->state; checkpoint.playerLastFoot=player->GetLastFoot();
	checkpoint.monster=brain->Capture(); checkpoint.heart=heart->Capture(); checkpoint.soundSequence=hearing->GetSequence();
	checkpoint.tables={tables[0]->Capture(),tables[1]->Capture()}; checkpoint.props=physicsProps.Capture(*world);
	std::filesystem::path path;
	if (Paths::GetAbsolutePath("Binaries/BabyDollSave/checkpoint.json",path) && WritePrototypeCheckpoint(path,checkpoint)) { toast="Checkpoint saved"; toastTime=2.0f; }
	else { Log::Write(LogLevel::Error,"BabyDoll","Checkpoint is kept in memory; disk write failed"); }
}

bool BabyDollApplication::LoadCheckpoint()
{
	std::filesystem::path path; PrototypeCheckpoint candidate;
	if (!Paths::GetAbsolutePath("Binaries/BabyDollSave/checkpoint.json",path)||!ReadPrototypeCheckpoint(path,candidate)) { return false; }
	if (candidate.props.size()!=physicsProps.GetCount() || !space.IsFree(candidate.playerPosition,config.playerRadius,config.playerHeight)
		||!space.GroundHeight(candidate.playerPosition,candidate.playerPosition.y)||candidate.heart.bpm<config.heartRest||candidate.heart.bpm>config.heartMaximum) { return false; }
	checkpoint=std::move(candidate); RestoreCheckpoint(); upperCheckpointSaved=player->GetPosition().y>=640.0f; return true;
}
