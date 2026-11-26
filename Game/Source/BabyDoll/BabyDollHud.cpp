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

#include "AssetManager.h"
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
	draw->AddText(ImVec2(origin.x + 28.0f, origin.y + 22.0f), Pink, "B A B Y D O L L");
	draw->AddText(ImVec2(origin.x + 28.0f, origin.y + 43.0f), Muted, "CONTENT PROTOTYPE  /  SCISSORMAN");
	ImGui::SetNextWindowPos(ImVec2(origin.x + size.x - 28.0f, origin.y + 22.0f), ImGuiCond_Always, ImVec2(1.0f, 0.0f));
	ImGui::SetNextWindowBgAlpha(0.82f);
	ImGui::Begin("AI status", nullptr, OverlayFlags);
	const ImVec4 stateColor = brain->GetState() == StitchState::Chase ? ImVec4(1.0f, 0.35f, 0.38f, 1.0f) : brain->GetState() == StitchState::Suspicious ? ImVec4(1.0f, 0.75f, 0.35f, 1.0f) : ImVec4(0.45f, 0.85f, 0.75f, 1.0f);
	ImGui::TextColored(stateColor, "%s", inspect ? "MODEL PREVIEW" : PrototypeMath::StateName(brain->GetState()));
	ImGui::TextDisabled("%s", animator->GetClipName().data());
	if (!inspect) { ImGui::Text("Distance  %.1f m", PrototypeMath::Distance(player->GetPosition(), brain->GetPosition()) / 100.0f); }
	ImGui::End();
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
	ImGui::TextDisabled("%s", player->GetMode() == PlayerMode::Hidden ? "HIDDEN" : player->GetMode() == PlayerMode::Fallen ? "STUMBLED" : player->GetMode() == PlayerMode::Caught ? "CAUGHT" : "EXPOSED");
	ImGui::End();
	ImGui::SetNextWindowPos(ImVec2(origin.x + size.x * 0.5f, origin.y + size.y - 28.0f), ImGuiCond_Always, ImVec2(0.5f, 1.0f));
	ImGui::SetNextWindowBgAlpha(0.8f);
	ImGui::Begin("Controls", nullptr, OverlayFlags);
	ImGui::TextColored(ImVec4(0.97f, 0.55f, 0.68f, 1.0f), freeMove ? "W A S D   Move    Shift   Run" : "S   Left foot       D   Right foot");
	ImGui::TextDisabled("Mouse  Look    T  Hide / Leave    Space  Test sound");
	ImGui::TextDisabled("Esc  Pause    Tab  Diagnostics");
	ImGui::End();
	if (captured)
	{
		const ImVec2 center(origin.x + size.x * 0.5f, origin.y + size.y * 0.5f);
		draw->AddCircleFilled(center, 2.0f, IM_COL32(235, 240, 245, 180));
		const int target = FindInteraction();
		if (target >= 0 || player->GetMode() == PlayerMode::Hidden)
		{
			const char* text = player->GetMode() == PlayerMode::Hidden ? "[ T ]  Leave hiding place" : "[ T ]  Hide under table";
			const ImVec2 textSize = ImGui::CalcTextSize(text);
			draw->AddText(ImVec2(center.x - textSize.x * 0.5f, center.y + 34.0f), Cyan, text);
		}
	}
	if (toastTime > 0.0f)
	{
		const ImVec2 textSize = ImGui::CalcTextSize(toast.c_str());
		draw->AddText(ImVec2(origin.x + (size.x - textSize.x) * 0.5f, origin.y + 95.0f), Cyan, toast.c_str());
	}
	if (player->GetMode() == PlayerMode::Caught)
	{
		const int alpha = static_cast<int>(std::clamp(caughtTime / 1.2f, 0.0f, 1.0f) * 240.0f);
		draw->AddRectFilled(origin, ImVec2(origin.x + size.x, origin.y + size.y), IM_COL32(12, 5, 12, alpha));
		draw->AddText(ImVec2(origin.x + size.x * 0.5f - 30.0f, origin.y + size.y * 0.5f), Pink, "CAUGHT");
	}
}

void BabyDollApplication::DrawPanel()
{
	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	ImGui::SetNextWindowPos(ImVec2(viewport->WorkPos.x + 28.0f, viewport->WorkPos.y + 90.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSize(ImVec2(355.0f, 0.0f), ImGuiCond_Always);
	ImGui::SetNextWindowSizeConstraints(ImVec2(355.0f, 100.0f), ImVec2(355.0f, (std::max)(200.0f, viewport->WorkSize.y - 120.0f)));
	ImGui::SetNextWindowBgAlpha(0.94f);
	ImGui::Begin("Prototype console", nullptr, ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoCollapse);
	ImGui::TextDisabled("SCISSORMAN  /  Goth Scarecrow  /  CC0");
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
	if (ImGui::Button(inspect ? "Start gameplay test" : "Resume", ImVec2(-1.0f, 36.0f)))
	{
		if (inspect) { ResetRun(); }
		inspect = paused = false;
		SetCaptured(true);
	}
	if (!inspect && ImGui::Button("Model preview", ImVec2(-1.0f, 28.0f)))
	{
		inspect = paused = true;
		SetCaptured(false);
		orbit->Reset(brain->GetPosition() + Vector3{ 0.0f, 155.0f, 0.0f }, brain->GetYaw() + 180.0f, config);
		animator->Play(monsterModel->animations[inspectClip]->GetName());
	}
	ImGui::SeparatorText("Input and camera");
	if (ImGui::Checkbox("Free movement (WASD / Shift)", &freeMove)) { player->SetMode(PlayerMode::Moving); occupiedTable = -1; }
	ImGui::TextWrapped("Default: alternate S / D. The engine does not expose the period key yet.");
	ImGui::SliderFloat("Sensitivity", &config.mouseSensitivity, 0.03f, 0.30f, "%.2f");
	ImGui::SliderFloat("Camera follow", &config.cameraFollow, 5.0f, 30.0f, "%.0f");
	ImGui::SliderFloat("Look response", &config.cameraLookResponse, 12.0f, 70.0f, "%.0f");
	ImGui::SliderFloat("Model facing", &visualYawOffset, -180.0f, 180.0f, "%.0f");
	ImGui::SeparatorText("Test options");
	ImGui::Checkbox("AI enabled", &aiEnabled);
	ImGui::Checkbox("Ignore capture (debug)", &godMode);
	ImGui::Checkbox("Diagnostics", &showDebug);
	if (ImGui::Button("Reset test room")) { resetRequested = true; }
	ImGui::Separator();
	ImGui::TextWrapped("Prototype: one floor, two hiding tables, hearing AI. No real audio, tentacles or rear CCTV.");
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
		ImGui::Text("Table A %d / %s", tables[0]->GetRemainingUses(), tables[0]->IsBroken() ? "broken" : "intact");
		ImGui::Text("Table B %d / %s", tables[1]->GetRemainingUses(), tables[1]->IsBroken() ? "broken" : "intact");
		ImGui::Text("%.1f FPS", ImGui::GetIO().Framerate);
		ImGui::End();
	}
}
