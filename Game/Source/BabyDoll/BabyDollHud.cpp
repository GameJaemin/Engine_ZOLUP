#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "BabyDollApplication.h"

#include "HeartComponent.h"
#include "HearingComponent.h"
#include "OrbitCameraComponent.h"
#include "PlayerMoveComponent.h"
#include "PoseAnimatorComponent.h"
#include "StitchBrainComponent.h"
#include "RearCameraComponent.h"
#include "ProximityLightComponent.h"
#include "CaughtCameraComponent.h"
#include "TraversalComponent.h"
#include "HandInventoryComponent.h"

#include "AssetManager.h"
#include "Component/SceneCapture2DComponent.h"
#include "Core/Math/Vector3.h"

#include <imgui.h>

#include <cstdio>
#include <functional>

namespace
{
	constexpr ImU32 Pink = IM_COL32(247, 139, 174, 255);
	constexpr ImU32 Cyan = IM_COL32(105, 219, 224, 255);
	constexpr ImU32 Muted = IM_COL32(157, 167, 185, 255);
	constexpr ImGuiWindowFlags OverlayFlags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
}

void BabyDollApplication::DrawHud()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImVec2 origin = viewport->WorkPos;
	const ImVec2 size = viewport->WorkSize;
	ImDrawList* draw = ImGui::GetForegroundDrawList();
	if (player->GetMode() == PlayerMode::Caught && !inspect)
	{
		const ImVec2 end(origin.x + size.x, origin.y + size.y);
		const int edgeAlpha = static_cast<int>(95.0f * (1.0f - CaughtCameraComponent::GetFade(caughtTime, config)));
		draw->AddRect(origin, end, IM_COL32(70, 0, 4, edgeAlpha), 0.0f, 0, 30.0f);
		draw->AddRectFilled(origin, end, IM_COL32(0, 0, 0, static_cast<int>(255.0f * CaughtCameraComponent::GetFade(caughtTime, config))));
		if (paused)
		{
			const char* message = "PAUSED - Esc to resume";
			const ImVec2 textSize = ImGui::CalcTextSize(message);
			draw->AddText(ImVec2(origin.x + (size.x - textSize.x) * 0.5f, origin.y + size.y - 50.0f), IM_COL32(235, 235, 235, 255), message);
		}
		return;
	}
	draw->AddText(ImVec2(origin.x + 28.0f, origin.y + 22.0f), Pink, "B A B Y D O L L");
	draw->AddText(ImVec2(origin.x + 28.0f, origin.y + 43.0f), Muted, grayboxReview?"LAYOUT REVIEW / 200cm GRID / SIMULATION PAUSED":GrayboxLayout::Zone(player->GetPosition()));
	if(grayboxReview) {return;}
	if (!inspect && showDebug)
	{
		draw->AddText(ImVec2(origin.x + 28.0f, origin.y + 68.0f), Cyan, progress.Objective());
		const Vector3 goal = !progress.HasKey() ? layout.keyPosition : progress.IsDoorOpen() ? layout.escapePosition : layout.doorPosition;
		const Vector3 delta = goal - player->GetPosition();
		const float angle = std::remainder(std::atan2(delta.x, delta.z) * 57.2957795f - orbit->GetYaw(), 360.0f);
		char guide[160];
		std::snprintf(guide, sizeof(guide), "%s  |  %.1f m  |  %s", !progress.HasKey() ? "BRASS KEY" : progress.IsDoorOpen() ? "ESCAPE" : "EXIT GATE", PrototypeMath::Length(delta) / 100.0f,
			std::abs(angle) < 20.0f ? "AHEAD" : std::abs(angle) > 150.0f ? "BEHIND" : angle > 0.0f ? "RIGHT" : "LEFT");
		draw->AddText(ImVec2(origin.x + 28.0f, origin.y + 88.0f), Muted, guide);
		if (soundPulseTime > 0.0f) { draw->AddText(ImVec2(origin.x + 28.0f, origin.y + 113.0f), Pink, "LOUD NOISE - the creature may have heard you"); }
	}
	if(showDebug||inspect)
	{
	ImGui::SetNextWindowPos(ImVec2(origin.x + size.x - 28.0f, origin.y + 22.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.82f);
	ImGui::Begin("AI status", nullptr, OverlayFlags);
	const ImVec4 stateColor = brain->GetState() == StitchState::Chase ? ImVec4(1.0f, 0.35f, 0.38f, 1.0f) : brain->GetState() == StitchState::Suspicious ? ImVec4(1.0f, 0.75f, 0.35f, 1.0f) : ImVec4(0.45f, 0.85f, 0.75f, 1.0f);
	ImGui::TextColored(stateColor, "%s", inspect ? "MODEL PREVIEW" : PrototypeMath::StateName(brain->GetState()));
	ImGui::TextDisabled("%s", animator->GetClipName().data());
	if (!inspect) { ImGui::Text("Distance  %.1f m", PrototypeMath::Distance(player->GetPosition(), brain->GetPosition()) / 100.0f); }
	ImGui::End();
	}
	if (inspect) { return; }
	ImGui::SetNextWindowPos(ImVec2(origin.x + 28.0f, origin.y + size.y - 28.0f), ImGuiCond_Always, ImVec2(0.0f, 1.0f));
	ImGui::SetNextWindowBgAlpha(0.88f);
	ImGui::Begin("Vitals", nullptr, OverlayFlags);
	ImGui::TextColored(ImVec4(0.97f, 0.45f, 0.55f, 1.0f), "%.0f BPM", heart->GetBPM());
	const ImVec2 graph = ImGui::GetCursorScreenPos();
	ImDrawList* windowDraw = ImGui::GetWindowDrawList();
	for (int index = 1; index < 150; ++index)
	{
		const std::function<float(int)> value = [&](int point)
		{
			const float phase = std::fmod(gameTime * heart->GetBPM() / 60.0f - static_cast<float>(point) / 150.0f + 10.0f, 1.0f);
			return -20.0f * std::exp(-std::pow((phase - 0.42f) / 0.025f, 2.0f)) + 9.0f * std::exp(-std::pow((phase - 0.48f) / 0.035f, 2.0f));
		};
		windowDraw->AddLine(ImVec2(graph.x + index - 1.0f, graph.y + 24.0f + value(index - 1)), ImVec2(graph.x + index, graph.y + 24.0f + value(index)), Pink, 1.5f);
	}
	ImGui::Dummy(ImVec2(150.0f, 46.0f));
	ImGui::TextDisabled("%s", player->GetMode() == PlayerMode::Hidden || traversal->IsHidden() ? "HIDDEN" : player->GetMode() == PlayerMode::Ladder ? "CLIMBING" : player->GetMode() == PlayerMode::Fallen ? "STUMBLED" : player->GetMode() == PlayerMode::Caught ? "CAUGHT" : "EXPOSED");
	ImGui::End();
	ImGui::SetNextWindowPos(ImVec2(origin.x + size.x * 0.5f, origin.y + size.y - 28.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
	ImGui::SetNextWindowBgAlpha(0.8f);
	ImGui::Begin("Controls", nullptr, OverlayFlags);
	if (freeMove) { ImGui::Text("W A S D   Move    Shift   Run"); }
	else
	{
		ImGui::TextColored(leftFootDown ? ImVec4(1.0f, 0.85f, 0.9f, 1.0f) : ImVec4(0.97f, 0.55f, 0.68f, 1.0f), soloControls ? "[ S ]  Left foot" : "[ S ]  P1 Left foot");
		ImGui::SameLine();
		ImGui::TextColored(rightFootDown ? ImVec4(0.85f, 1.0f, 1.0f, 1.0f) : ImVec4(0.4f, 0.85f, 0.9f, 1.0f), soloControls ? "[ D ]  Right foot" : "[ . ]  P2 Right foot");
		if (player->GetMode() == PlayerMode::Fallen) { ImGui::TextColored(ImVec4(1.0f, 0.65f, 0.55f, 1.0f), "Same foot twice - recovering..."); }
		else
		{
			const int lastFoot = traversal->IsActive()?traversal->GetLastFoot():player->GetLastFoot();
			ImGui::TextDisabled("Next step: %s", lastFoot < 0 ? "either foot" : lastFoot == 0 ? (soloControls ? "D - right foot" : ". - right foot") : "S - left foot");
		}
	}
	ImGui::TextDisabled(soloControls ? "Mouse  Look    Right click  Use / Hide / Leave" : "Mouse  Look    T  Use / Hide / Leave");
	ImGui::TextDisabled("Esc  Pause    Tab  Diagnostics");
	ImGui::End();
	if(player->GetMode()==PlayerMode::Hanging)
	{
		ImGui::SetNextWindowPos(ImVec2(origin.x+size.x*0.5f,origin.y+size.y-150.0f),ImGuiCond_Always,ImVec2(0.5f,1.0f));
		ImGui::Begin("Grip",nullptr,OverlayFlags);
		ImGui::ProgressBar(traversal->GetStamina()/config.hangDuration,ImVec2(180.0f,14.0f),"Grip"); ImGui::End();
	}
	if (captured)
	{
		const ImVec2 center(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f);
		draw->AddCircleFilled(center, 2.0f, IM_COL32(235, 240, 245, 180));
		const int target = FindInteraction();
		const int objective = FindObjectiveInteraction();
		const int link=traversal->Find(player->GetPosition(),orbit->GetYaw(),&space);
		if(traversal->IsActive()||link>=0)
		{
			const auto* active=traversal->GetLink();
			const auto kind=active?active->kind:traversal->links[link].kind;
			const char* action=kind==TraversalKind::Bar?(active?"Release bar":"Hang from bar"):kind==TraversalKind::Ladder?"Ladder - alternate feet":active?"Crawl to the exit":"Enter crawl passage";
			const char* keys=kind==TraversalKind::Bar?(soloControls?"Left click":"R"):kind==TraversalKind::Crawl&&active?(soloControls?"S / D":"S / ."):(soloControls?"Left / Right click":"R / T");
			char prompt[160]; std::snprintf(prompt,sizeof(prompt),"[ %s ]  %s",keys,action);
			draw->AddText(ImVec2(center.x-120.0f,center.y+55.0f),Cyan,prompt);
		}
		if (target >= 0 || objective || player->GetMode() == PlayerMode::Hidden)
		{
			const char* action = player->GetMode() == PlayerMode::Hidden ? "Leave hiding place" : objective == 1 ? "Take brass key" : objective == 2 ? "Open gate" : objective == 3 ? "Locked - use the hand holding the key" : objective==4?"Close gate":"Hide under table";
			char prompt[128];
			std::snprintf(prompt, sizeof(prompt), "[ %s ]  %s",objective?(soloControls?"Left / Right click":"R / T"):(soloControls ? "Right click" : "T"), action);
			const char* text = prompt;
			const ImVec2 textSize = ImGui::CalcTextSize(text);
			draw->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y + 34.0f), Cyan, text);
		}
	}
	if (toastTime > 0.0f)
	{
		const ImVec2 textSize = ImGui::CalcTextSize(toast.c_str());
		draw->AddText(ImVec2(origin.x + (size.x - textSize.x) * 0.5f, origin.y + 145.0f), Cyan, toast.c_str());
	}
}

void BabyDollApplication::DrawPanel()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	if (grayboxReview)
	{
		ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 28.0f, viewport->WorkPos.y + 22.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
		ImGui::Begin("Layout controls", nullptr, OverlayFlags);
		if (ImGui::Button("Return to gameplay view")) { grayboxReview = false; }
		ImGui::End();
		return;
	}
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 28.0f, viewport->WorkPos.y + 185.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(355.0f, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSizeConstraints(ImVec2(355.0f, 100.0f), ImVec2(355.0f, (std::max)(200.0f, viewport->WorkSize.y - 120.0f)));
	ImGui::SetNextWindowBgAlpha(0.94f);
	ImGui::Begin("Exhibition escape", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
	if (progress.stage == EscapeStage::Cleared)
	{
		ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.7f, 1.0f), "YOU ESCAPED");
		ImGui::Text("Time  %02d:%02d", static_cast<int>(clearTime) / 60, static_cast<int>(clearTime) % 60);
		ImGui::Text("Caught  %d time(s)", deaths);
		ImGui::TextWrapped("You reached the hidden elevator and escaped the exhibition.");
		if (ImGui::Button("Play again", ImVec2(-1.0f, 36.0f))) { ResetRun(); inspect = paused = false; SetCaptured(true); }
		ImGui::End();
		return;
	}
	ImGui::TextWrapped("One body. Two players. Explore the exhibition and reach the hidden elevator upstairs.");
	ImGui::TextWrapped("GRAYBOX WALKTHROUGH: arrival > exhibition loop > key gallery > locked gate > upper gallery. Slow steps stay quiet; tables and the low passage offer escape routes.");
	ImGui::TextDisabled("Audio is not available in this build.");
	ImGui::TextWrapped("Use the noise ring, threat cue, lights and rear CCTV to read danger. Space makes a loud distraction.");
	ImGui::Separator();
	if (inspect)
	{
		ImGui::TextWrapped("Inspect the imported monster, then enter the gameplay test. Escape releases the mouse.");
		if (ImGui::Button("Orbit model", ImVec2(-1.0f, 32.0f))) { SetCaptured(true); }
		if (ImGui::BeginCombo("Animation", monsterModel->animations[inspectClip]->GetName().c_str()))
		{
			for (std::size_t index = 0; index < monsterModel->animations.size(); ++index)
			{
				const std::string& name = monsterModel->animations[index]->GetName();
				if (ImGui::Selectable(name.c_str(), inspectClip == static_cast<int>(index))) { inspectClip = static_cast<int>(index); animator->Play(name); }
			}
			ImGui::EndCombo();
		}
	}
	ImGui::SeparatorText("Play mode");
	if(ImGui::Button("Layout overview (for review)",ImVec2(-1.0f,28.0f)))
	{
		grayboxReview=true; paused=true; inspect=false; SetCaptured(false);
	}
	const bool wasSolo = soloControls;
	if (ImGui::RadioButton("Single player (S / D)", soloControls)) { soloControls = true; }
	if (ImGui::RadioButton("Co-op (S / period)", !soloControls)) { soloControls = false; }
	if (wasSolo != soloControls)
	{
		freeMove = false;
		pending = {};
	}
	ImGui::TextWrapped(soloControls ? "Alternate S and D to walk. Mouse looks; T or right-click hides / leaves." : "P1: S / R / T. P2: period / mouse. T hides; R is reserved for hand actions.");
	if (ImGui::Button(inspect || gameTime == 0.0f ? "Begin escape" : "Resume", ImVec2(-1.0f, 36.0f)))
	{
		if (inspect) { ResetRun(); }
		grayboxReview=false;
		inspect = paused = false;
		SetCaptured(true);
	}
	if (gameTime > 0.0f && ImGui::Button("Restart from entrance", ImVec2(-1.0f, 28.0f))) { ResetRun(); inspect = paused = false; SetCaptured(true); }
	if (!ImGui::CollapsingHeader("Developer tools")) { ImGui::End(); return; }
	if (progress.stage != EscapeStage::Cleared && ImGui::Button("Preview jumpscare", ImVec2(-1.0f, 28.0f))) { previewCaughtRequested = true; }
	if (!inspect && ImGui::Button("Model preview", ImVec2(-1.0f, 28.0f)))
	{
		inspect = paused = true;
		SetCaptured(false);
		orbit->Reset(brain->GetPosition() + Vector3{ 0.0f, 155.0f, 0.0f }, brain->GetYaw() + 180.0f, config);
		animator->Play(monsterModel->animations[inspectClip]->GetName());
	}
	ImGui::SeparatorText("Input and camera");
	if (ImGui::Checkbox("Free movement (WASD / Shift)", &freeMove)) { player->SetMode(PlayerMode::Moving); occupiedTable = -1; }
	ImGui::SliderFloat("Sensitivity", &config.mouseSensitivity, 0.03f, 0.30f, "%.2f");
	ImGui::SliderFloat("Camera follow", &config.cameraFollow, 5.0f, 30.0f, "%.0f");
	ImGui::SliderFloat("Look response", &config.cameraLookResponse, 12.0f, 70.0f, "%.0f");
	ImGui::SliderFloat("Model facing", &visualYawOffset, -180.0f, 180.0f, "%.0f");
	ImGui::SeparatorText("Test options");
	ImGui::Checkbox("AI enabled", &aiEnabled);
	ImGui::Checkbox("Ignore capture (debug)", &godMode);
	ImGui::Checkbox("Diagnostics", &showDebug);
	ImGui::Checkbox("Preview CCTV (debug)", &previewRear);
	ImGui::SliderFloat("CCTV saturation", &config.rearSaturation, 0.0f, 1.0f, "%.2f");
	ImGui::Checkbox("Proximity lamps", &lightsEnabled);
	ImGui::Checkbox("Voxel Stitches / imported Scissorman",&useVoxelStitch);
	if(ImGui::TreeNode("Feature test positions"))
	{
		if(ImGui::Button("Ladder bottom")) {TeleportForTest({1700.0f,0.0f,400.0f});}
		if(ImGui::Button("Ladder top")) {TeleportForTest({1700.0f,640.0f,550.0f});}
		if(ImGui::Button("Crawl entrance")) {TeleportForTest({-650.0f,0.0f,1100.0f});}
		if(ImGui::Button("Crawl far entrance")) {TeleportForTest({250.0f,0.0f,1100.0f});}
		if(ImGui::Button("Hanging bar")) {TeleportForTest({250.0f,0.0f,-1250.0f});}
		if(ImGui::Button("Stair approach")) {TeleportForTest({1500.0f,0.0f,-1100.0f});}
		if(ImGui::Button("Upper room")) {TeleportForTest({1500.0f,640.0f,700.0f});}
		ImGui::TreePop();
	}
	if (ImGui::Button("Drop physics props again")) { dropPropsRequested = true; }
	if (ImGui::Button("Reset test room")) { resetRequested = true; }
	ImGui::Separator();
	ImGui::TextWrapped("Movement and stairs use the game ground adapter. Audio playback is deferred. Layout and checkpoint events are prototype test placements.");
	ImGui::End();
}

void BabyDollApplication::OnGui()
{
	if (!world || !player || !brain) { return; }
	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowRounding = 12.0f;
	style.FrameRounding = 6.0f;
	style.WindowPadding = ImVec2(16.0f, 14.0f);
	style.ItemSpacing = ImVec2(8.0f, 10.0f);
	style.Colors[ImGuiCol_WindowBg] = ImVec4(0.035f, 0.045f, 0.065f, 0.95f);
	style.Colors[ImGuiCol_Button] = ImVec4(0.18f, 0.34f, 0.38f, 1.0f);
	style.Colors[ImGuiCol_ButtonHovered] = ImVec4(0.25f, 0.47f, 0.49f, 1.0f);
	DrawHud();
	if (player->GetMode() == PlayerMode::Caught && !inspect) { return; }
	DrawRearView();
	if (!captured) { DrawPanel(); }
	if (showDebug)
	{
		ImGui::SetNextWindowPos(ImVec2(28.0f, 90.0f), ImGuiCond_FirstUseEver);
		ImGui::Begin("Component diagnostics", nullptr, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings);
		ImGui::Text("PlayerMove  %.1f cm/s | momentum %.2f", player->GetSpeed(), player->GetMomentum());
		ImGui::Text("Heart  %.2f BPM | radius %.0f cm", heart->GetBPM(), heart->GetSoundRadius());
		ImGui::Text("Hearing  foot %.0f | final %.0f cm", footRadius, soundRadius);
		ImGui::Text("StitchBrain  %s | %.2f s", PrototypeMath::StateName(brain->GetState()), brain->GetStateTime());
		ImGui::Text("Action %d | path nodes %zu", static_cast<int>(brain->GetAction()), brain->GetPath().size());
		ImGui::Text("OrbitCamera  FOV %.1f", orbit->GetFov());
		ImGui::Text("RearView  %s | hold %.2f", rearView->IsVisible() ? "active" : "off", rearView->GetRemaining());
		int darkCount = 0;
		for (ProximityLightComponent* lamp : lamps) { if (lamp->IsDark()) { ++darkCount; } }
		ImGui::Text("Lamps  %d/%zu dark | physics props %zu", darkCount, lamps.size(), physicsProps.GetCount());
		ImGui::Text("Jolt  %s", physicsHealthy ? "ready" : "FAILED - paused");
		ImGui::Text("Table A %d / %s", tables[0]->GetRemainingUses(), tables[0]->IsBroken() ? "broken" : "intact");
		ImGui::Text("Table B %d / %s", tables[1]->GetRemainingUses(), tables[1]->IsBroken() ? "broken" : "intact");
		ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
		ImGui::End();
	}
}

void BabyDollApplication::DrawRearView()
{
	if (inspect || !rearView->IsVisible()) { return; }
	SceneCapture2DComponent* capture = rearView->GetCapture();
	if (!capture) { return; }
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const float width = (std::min)(384.0f, viewport->WorkSize.x * 0.32f);
	const float height = width * config.rearHeight / config.rearWidth;
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + viewport->WorkSize.x - 28.0f, viewport->WorkPos.y + 118.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.96f);
	ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.75f, 0.15f, 0.2f, 1.0f));
	ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 2.0f);
	ImGui::Begin("Rear CCTV", nullptr, OverlayFlags | ImGuiWindowFlags_NoInputs);
	ImGui::TextColored(ImVec4(1.0f, 0.3f, 0.35f, 1.0f), "REC   REAR CAM");
	const ImVec2 position = ImGui::GetCursorScreenPos();
	const Hakko::DataHub::TexID texture = capture->GetRenderTarget().FindTextureID();
	if (texture != 0) { ImGui::Image(static_cast<ImTextureID>(texture), ImVec2(width, height)); }
	else { ImGui::Dummy(ImVec2(width, height)); }
	ImDrawList* draw = ImGui::GetWindowDrawList();
	for (float y = 0.0f; y < height; y += 4.0f) { draw->AddLine(ImVec2(position.x, position.y + y), ImVec2(position.x + width, position.y + y), IM_COL32(0, 0, 0, 48), 1.0f); }
	for(unsigned index=0;index<320;++index)
	{
		const unsigned seed=index*1664525u+static_cast<unsigned>(gameTime*24.0f)*1013904223u;
		const float x=static_cast<float>(seed%4096u)/4096.0f*width;
		const float y=static_cast<float>((seed*22695477u+1u)%4096u)/4096.0f*height;
		const ImU32 color=IM_COL32(230,230,230,static_cast<int>(22.0f*config.rearGrain));
		draw->AddRectFilled(ImVec2(position.x+x,position.y+y),ImVec2(position.x+x+1.5f,position.y+y+1.5f),color);
	}
	ImGui::Text("Distance  %.1f m", PrototypeMath::Distance(player->GetPosition(), brain->GetPosition()) / 100.0f);
	ImGui::End();
	ImGui::PopStyleVar();
	ImGui::PopStyleColor();
}
