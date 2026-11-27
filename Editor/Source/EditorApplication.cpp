#include "EditorApplication.h"

#include "AssetManager.h"
#include "Camera/CameraViewInfo.h"
#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Log.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Vector3.h"
#include "Engine.h"
#include "Font.h"
#include "GameObject.h"
#include "Mesh/StaticMesh.h"
#include "Paths.h"
#include "Prefab/ModelPrefab.h"
#include "World.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace
{
	// 맵 열기 기능이 생기기 전까지 에디터가 시작할 때 여는 맵이다. 저장소 루트 기준 경로다.
	constexpr std::string_view DefaultLevelPath = "Game/Content/Maps/DevMap.json";

	// 기본 도킹 배치와 창 생성에서 같은 이름을 사용한다.
	constexpr const char* PlayToolbarWindowName = "Play Toolbar";

	void LoadEditorFont()
	{
		const std::shared_ptr<const Font> editorFont = AssetManager::Get().LoadFont("Editor/Content/Fonts/NotoSansCJKkr-Regular.otf");
		if (editorFont == nullptr) { return; }

		const std::span<const std::byte> bytes = editorFont->GetBytes();
		if (bytes.size() > static_cast<std::size_t>((std::numeric_limits<int>::max)()))
		{
			Log::Write(LogLevel::Warning, "Editor", "Editor font exceeds the size supported by ImGui");
			return;
		}

		ImGuiIO& io = ImGui::GetIO();
		ImFontConfig config;
		// AssetManager의 캐시가 ImGui Context가 파괴될 때까지 원본 바이트를 보관한다.
		config.FontDataOwnedByAtlas = false;
		config.Flags |= ImFontFlags_NoLoadError;

		ImFont* font = io.Fonts->AddFontFromMemoryTTF(const_cast<std::byte*>(bytes.data()), static_cast<int>(bytes.size()), 0.0f, &config);
		if (font == nullptr)
		{
			Log::Write(LogLevel::Warning, "Editor", "Failed to register the editor font");
			return;
		}

		io.FontDefault = font;
	}

	// label 버튼을 color 계열로 그린다. 마우스를 올리면 밝게, 누르는 동안 어둡게 한다.
	bool DrawColoredButton(const char* label, const ImVec4& color)
	{
		ImGui::PushStyleColor(ImGuiCol_Button, color);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 1.2f, color.y * 1.2f, color.z * 1.2f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(color.x * 0.8f, color.y * 0.8f, color.z * 0.8f, 1.0f));
		const bool clicked = ImGui::Button(label);
		ImGui::PopStyleColor(3);
		return clicked;
	}

	struct PickSegment
	{
		Vector3 start;
		Vector3 end;
	};

	std::optional<PickSegment> BuildPickSegment(const ViewportPickClick& click, const CameraViewInfo& camera)
	{
		Matrix4x4 projection;
		if (!camera.CalculateProjectionMatrix(click.aspectRatio, projection)) { return std::nullopt; }

		const float screenX = click.imageUV.x * 2.0f - 1.0f;
		const float screenY = 1.0f - click.imageUV.y * 2.0f;
		const float viewX = screenX / projection.m[0][0];
		const float viewY = screenY / projection.m[1][1];
		const bool isPerspective = camera.projectionMode == CameraProjectionMode::Perspective;
		const float nearScale = isPerspective ? camera.nearClipDistance : 1.0f;
		const float farScale = isPerspective ? camera.farClipDistance : 1.0f;

		// 원근 투영의 화면 좌표는 깊이로 나눈 값이다. near와 far의 깊이를 각각 곱해 되돌린다.
		const Vector3 viewStart{ viewX * nearScale, viewY * nearScale, camera.nearClipDistance };
		const Vector3 viewEnd{ viewX * farScale, viewY * farScale, camera.farClipDistance };
		const Matrix4x4 cameraToWorld = Matrix4x4::CreateFromQuaternion(camera.rotation) * Matrix4x4::CreateTranslation(camera.position);
		const Vector3 start = cameraToWorld.TransformPoint(viewStart);
		const Vector3 end = cameraToWorld.TransformPoint(viewEnd);
		const Vector3 direction = end - start;
		const float lengthSquared = direction.Dot(direction);
		if (!std::isfinite(lengthSquared) || lengthSquared <= 0.0f) { return std::nullopt; }
		return PickSegment{ start, end };
	}

	bool IntersectPickTriangle(const Vector3& start, const Vector3& direction, const Vector3& a, const Vector3& b, const Vector3& c, float& outFraction)
	{
		const Vector3 edgeAB = b - a;
		const Vector3 edgeAC = c - a;
		const Vector3 perpendicular = direction.Cross(edgeAC);
		const float determinant = edgeAB.Dot(perpendicular);
		// 시계 방향 앞면만 그리는 일반 메시와 같은 기준이다. 평행하거나 면적이 0인 면도 여기서 제외된다.
		if (!(determinant > 0.0f) || !std::isfinite(determinant)) { return false; }

		// u와 v는 교점의 삼각형 내부 좌표다. 나누기 전에 determinant 배율로 비교한다.
		const Vector3 fromA = start - a;
		const float u = fromA.Dot(perpendicular);
		if (!(u >= 0.0f && u <= determinant)) { return false; }
		const Vector3 cross = fromA.Cross(edgeAB);
		const float v = direction.Dot(cross);
		if (!(v >= 0.0f && v <= determinant - u)) { return false; }

		// direction은 end - start다. 이 비율이 0이면 near, 1이면 far에 닿는다.
		const float fraction = edgeAC.Dot(cross) / determinant;
		if (!(fraction >= 0.0f && fraction <= 1.0f)) { return false; }
		outFraction = fraction;
		return true;
	}

	GameObject* PickGameObject(World& world, const PickSegment& segment)
	{
		const Vector3 direction = segment.end - segment.start;
		GameObject* closestObject = nullptr;
		float closestFraction = 1.0f;

		for (const std::unique_ptr<GameObject>& object : world.GetGameObjects())
		{
			if (object->IsPendingDestroy()) { continue; }
			const TransformComponent* transform = object->GetComponent<TransformComponent>();
			if (transform == nullptr) { continue; }
			const Matrix4x4 worldMatrix = transform->GetWorldMatrix();

			for (const std::unique_ptr<Component>& component : object->GetComponents())
			{
				const StaticMeshComponent* meshComponent = dynamic_cast<const StaticMeshComponent*>(component.get());
				if (meshComponent == nullptr) { continue; }
				const std::shared_ptr<const StaticMesh>& mesh = meshComponent->GetStaticMesh();
				if (mesh == nullptr || !mesh->skinWeights.empty()) { continue; }

				for (std::size_t index = 2; index < mesh->indices.size(); index += 3)
				{
					const std::uint32_t aIndex = mesh->indices[index - 2];
					const std::uint32_t bIndex = mesh->indices[index - 1];
					const std::uint32_t cIndex = mesh->indices[index];
					const std::size_t vertexCount = mesh->vertices.size();
					if (aIndex >= vertexCount || bIndex >= vertexCount || cIndex >= vertexCount) { continue; }

					const Vector3 a = worldMatrix.TransformPoint(mesh->vertices[aIndex].position);
					const Vector3 b = worldMatrix.TransformPoint(mesh->vertices[bIndex].position);
					const Vector3 c = worldMatrix.TransformPoint(mesh->vertices[cIndex].position);
					float fraction = 0.0f;
					if (!IntersectPickTriangle(segment.start, direction, a, b, c, fraction)) { continue; }
					if (closestObject != nullptr && fraction >= closestFraction) { continue; }
					closestFraction = fraction;
					closestObject = object.get();
				}
			}
		}

		return closestObject;
	}

	/**
	 * 도킹 공간을 오른쪽 Inspector, 아래 Content Browser, 왼쪽 Hierarchy와 가운데 Viewport로 나눈다.
	 * 비율은 viewport 작업 영역 기준이다.
	 */
	void BuildDefaultDockLayout(ImGuiID dockSpaceID, const ImGuiViewport& viewport)
	{
		ImGui::DockBuilderAddNode(dockSpaceID, ImGuiDockNodeFlags_DockSpace);
		// 나누기 전에 크기를 정해야 나눈 비율이 정확하다.
		ImGui::DockBuilderSetNodeSize(dockSpaceID, viewport.WorkSize);

		// 나눌 때마다 centerID는 남은 칸으로 바뀌고, 마지막에 남은 칸에 Viewport를 배치한다.
		ImGuiID centerID = dockSpaceID;
		const ImGuiID rightID = ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Right, 0.25f, nullptr, &centerID);
		const ImGuiID bottomID = ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Down, 0.3f, nullptr, &centerID);
		// 남은 폭은 전체의 75%이므로 전체의 20%가 되도록 0.2 / 0.75를 사용한다.
		const ImGuiID leftID = ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Left, 0.2f / 0.75f, nullptr, &centerID);

		// 남은 가운데 칸의 위쪽을 버튼 한 줄 높이만큼 떼어 툴바를 둔다. 가운데 칸 높이는 전체의 70%다.
		const float toolbarHeight = ImGui::GetFrameHeight() + ImGui::GetStyle().WindowPadding.y * 2.0f;
		const float toolbarRatio = toolbarHeight / (viewport.WorkSize.y * 0.7f);
		const ImGuiID toolbarID = ImGui::DockBuilderSplitNode(centerID, ImGuiDir_Up, toolbarRatio, nullptr, &centerID);
		// 탭 막대가 있으면 버튼 한 줄보다 칸이 커지므로 숨긴다.
		ImGui::DockBuilderGetNode(toolbarID)->LocalFlags |= ImGuiDockNodeFlags_NoTabBar;

		ImGui::DockBuilderDockWindow(PlayToolbarWindowName, toolbarID);
		ImGui::DockBuilderDockWindow(InspectorPanel::WindowName, rightID);
		ImGui::DockBuilderDockWindow(ContentBrowserPanel::WindowName, bottomID);
		ImGui::DockBuilderDockWindow(HierarchyPanel::WindowName, leftID);
		ImGui::DockBuilderDockWindow(ViewportPanel::WindowName, centerID);
		ImGui::DockBuilderFinish(dockSpaceID);
	}
}

bool EditorApplication::OnInitialize(Engine& inEngine)
{
	engine = &inEngine;

	// Engine이 Run 초기화에서 ImGui Context를 만든 뒤 호출하므로 UI 호출이 아닌 설정 변경은 여기서 할 수 있다.
	// 패널을 탭과 칸으로 붙일 수 있게 한다. 렌더러가 창 밖 출력을 지원하지 않아 멀티 뷰포트는 켜지 않는다.
	ImGui::GetIO().ConfigFlags |= ImGuiConfigFlags_DockingEnable;
	LoadEditorFont();

	// TODO: 맵 열기 기능이 생기면 기본 맵 대신 사용자가 고른 맵을 연다.
	std::filesystem::path levelPath;
	if (!Paths::GetAbsolutePath(DefaultLevelPath, levelPath))
	{
		Log::Write(LogLevel::Error, "Editor", "Could not resolve the default level path from the repository root");
	}
	else if (!worldSession.OpenLevel(levelPath, selection))
	{
		Log::Write(LogLevel::Error, "Editor", "Failed to open the default level");
	}

	// 기본 맵을 열지 못해도 패널과 콘텐츠 브라우저는 쓸 수 있으므로 빈 World로 시작한다.
	if (worldSession.GetEditWorld() == nullptr)
	{
		worldSession.NewLevel();
		if (!selection.SetWorld(worldSession.GetEditWorld())) { return false; }
	}

	// 파일시스템 탐색만 초기화하므로 ImGui Context나 프레임은 필요하지 않다.
	if (!contentBrowserPanel.Initialize())
	{
		return false;
	}

	return true;
}

void EditorApplication::OnUpdate(const InputState& input, float deltaTime)
{
	ProcessPlayRequest();
	ProcessLevelOpenRequest();

	// Playing에서만 Play World를 진행한다. 플레이어가 없어 입력은 전달하지 않는다.
	if (worldSession.GetPlayState() == EditorPlayState::Playing)
	{
		World* playWorld = worldSession.GetActiveWorld();
		playWorld->Tick(deltaTime);
		// OnGui는 그릴 수 있는 프레임에만 불리므로, Tick이 만든 삭제 요청은 여기서도 처리한다.
		playWorld->DestroyPendingGameObjects();
		if (!playWorld->GetPhysicsScene().Update(deltaTime))
		{
			Log::Write(LogLevel::Warning, "Physics", "Failed to update play world physics");
		}
	}

	if (editorViewport.UpdateCamera(input, deltaTime))
	{
		engine->SetCursorVisible(true);
	}
}

void EditorApplication::OnGui()
{
	ImGuizmo::BeginFrame();

	const ImGuiViewport* viewport = ImGui::GetMainViewport();
	const ImGuiID dockSpaceID = ImGui::GetID("EditorDockSpace");

	// imgui.ini를 꺼 두어 실행할 때마다 도킹 공간이 없으므로 첫 프레임에 기본 배치를 만든다.
	// 이후 프레임에는 노드가 있으므로 다시 만들지 않고, 사용자가 바꾼 배치를 유지한다.
	if (ImGui::DockBuilderGetNode(dockSpaceID) == nullptr)
	{
		BuildDefaultDockLayout(dockSpaceID, *viewport);
	}

	// 엔진 창 전체를 패널을 붙일 수 있는 도킹 공간으로 만든다. 패널보다 먼저 매 프레임 호출한다.
	// 가운데 도킹 창이 없을 때는 뒤의 화면이 보이고 입력도 통과하게 한다.
	ImGui::DockSpaceOverViewport(dockSpaceID, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

	DrawPlayToolbar();

	editorViewport.ResetRenderRequestState();
	const std::optional<std::string> droppedPrefabPath = viewportPanel.Draw(editorViewport, selection);

	if (editorViewport.TryBeginCameraControl(viewportPanel.WasImageRightClicked()))
	{
		engine->SetCursorVisible(false);
	}

	World* world = worldSession.GetActiveWorld();
	if (world == nullptr) { return; }

	// Play World에 만든 객체는 Stop 때 사라지므로 편집으로 착각하지 않게 막는다.
	if (droppedPrefabPath.has_value() && worldSession.GetPlayState() != EditorPlayState::Editing)
	{
		Log::Write(LogLevel::Warning, "Editor", "Stop play before placing a prefab");
	}
	else if (droppedPrefabPath.has_value())
	{
		const std::shared_ptr<const PrefabObjects> prefab = AssetManager::Get().LoadPrefab(*droppedPrefabPath);
		if (prefab == nullptr)
		{
			Log::Write(LogLevel::Warning, "Editor", "Failed to load the dropped prefab");
		}
		else
		{
			GameObject* root = InstantiatePrefab(*world, *prefab, PrefabPlacement{});
			if (root == nullptr)
			{
				Log::Write(LogLevel::Warning, "Editor", "Failed to instantiate the dropped prefab");
			}
			else
			{
				// 루트의 회전과 크기는 Prefab 값을 유지하고 위치만 원점에 둔다.
				root->GetComponent<TransformComponent>()->SetPosition(Vector3{ 0.0f, 0.0f, 0.0f });
				if (!selection.Select(root))
				{
					Log::Write(LogLevel::Warning, "Editor", "Could not select the dropped prefab instance");
				}
			}
		}
	}
	else if (!editorViewport.IsCameraControlActive())
	{
		const std::optional<ViewportPickClick> click = viewportPanel.GetPickClick();
		if (click.has_value())
		{
			const std::optional<PickSegment> segment = BuildPickSegment(*click, editorViewport.GetCameraViewInfo());
			if (segment.has_value())
			{
				GameObject* pickedObject = PickGameObject(*world, *segment);
				if (pickedObject != nullptr)
				{
					selection.Select(pickedObject);
				}
				else
				{
					selection.ClearSelection();
				}
			}
		}
	}

	// 프리팹 생성을 마친 뒤 World의 객체 목록을 순회한다.
	// Hierarchy에서 바뀐 선택을 같은 프레임의 Inspector가 읽도록 먼저 그린다.
	hierarchyPanel.Draw(*world, selection);
	inspectorPanel.Draw(selection);

	// 콘텐츠 파일 선택은 패널 내부에서 관리하므로 EditorSelection을 전달하지 않는다.
	contentBrowserPanel.Draw();

	// 에디터의 객체 생성과 삭제 요청은 OnGui에서만 일어난다. 렌더 요청 전에 정리해 삭제 예정 객체가 그려지지 않게 한다.
	world->DestroyPendingGameObjects();
	if (worldSession.GetPlayState() == EditorPlayState::Editing &&
		!world->GetPhysicsScene().PrepareStaticBodies())
	{
		Log::Write(LogLevel::Warning, "Physics", "Failed to prepare one or more editor static bodies");
	}
}

std::optional<SceneRenderRequest> EditorApplication::GetRenderRequest()
{
	World* world = worldSession.GetActiveWorld();

	if (world == nullptr) { return std::nullopt; }
	return editorViewport.BuildRenderRequest(world->GetRenderScene());
}

void EditorApplication::OnShutdown() noexcept
{
	// 조작 중에 종료되면 숨긴 커서가 남지 않도록 되돌린다.
	if (editorViewport.StopCameraControl())
	{
		engine->SetCursorVisible(true);
	}

	if (!editorViewport.ShutdownRenderTarget())
	{
		Log::Write(LogLevel::Error, "Editor", "Failed to request viewport render target release");
	}

	contentBrowserPanel.ShutdownThumbnails();

	selection.SetWorld(nullptr);
	worldSession.Close();

	engine = nullptr;
}

void EditorApplication::ProcessLevelOpenRequest()
{
	const std::optional<std::string> relativePath = contentBrowserPanel.TakeLevelOpenRequest();
	if (!relativePath) { return; }

	// 세션도 거절하지만, 원인을 알 수 있도록 먼저 안내한다. 요청은 꺼냈으므로 Stop 뒤에 다시 실행되지 않는다.
	if (worldSession.GetPlayState() != EditorPlayState::Editing)
	{
		Log::Write(LogLevel::Warning, "Editor", "Stop play before opening a map: " + *relativePath);
		return;
	}

	std::filesystem::path absolutePath;
	if (!Paths::GetAbsolutePath(*relativePath, absolutePath))
	{
		Log::Write(LogLevel::Error, "Editor", "Could not resolve map path: " + *relativePath);
		return;
	}

	if (!worldSession.OpenLevel(absolutePath, selection))
	{
		Log::Write(LogLevel::Error, "Editor", "Could not open map: " + *relativePath);
	}
}

void EditorApplication::DrawPlayToolbar()
{
	if (ImGui::Begin(PlayToolbarWindowName, nullptr, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
	{
		const EditorPlayState state = worldSession.GetPlayState();

		// 세 버튼 너비(글자 + 좌우 여백)와 사이 간격 두 개를 더해 가운데에서 시작한다. 창이 좁으면 왼쪽부터 그린다.
		const ImGuiStyle& style = ImGui::GetStyle();
		const float buttonsWidth = ImGui::CalcTextSize("Play").x + ImGui::CalcTextSize("Pause").x + ImGui::CalcTextSize("Stop").x
			+ style.FramePadding.x * 6.0f + style.ItemSpacing.x * 2.0f;
		const float offset = (ImGui::GetContentRegionAvail().x - buttonsWidth) * 0.5f;
		if (offset > 0.0f) { ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offset); }

		// Paused에서는 Play가 Resume으로 동작한다.
		ImGui::BeginDisabled(state == EditorPlayState::Playing);
		if (DrawColoredButton("Play", ImVec4(0.20f, 0.55f, 0.25f, 1.0f)))
		{
			playRequest = state == EditorPlayState::Paused ? EditorPlayRequest::Resume : EditorPlayRequest::Play;
		}
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::BeginDisabled(state != EditorPlayState::Playing);
		if (DrawColoredButton("Pause", ImVec4(0.70f, 0.50f, 0.15f, 1.0f))) { playRequest = EditorPlayRequest::Pause; }
		ImGui::EndDisabled();

		ImGui::SameLine();
		ImGui::BeginDisabled(state == EditorPlayState::Editing);
		if (DrawColoredButton("Stop", ImVec4(0.65f, 0.20f, 0.20f, 1.0f))) { playRequest = EditorPlayRequest::Stop; }
		ImGui::EndDisabled();
	}
	// Begin이 false를 반환해도 End는 항상 호출한다.
	ImGui::End();
}

void EditorApplication::ProcessPlayRequest()
{
	const std::optional<EditorPlayRequest> request = std::exchange(playRequest, std::nullopt);
	if (!request.has_value()) { return; }

	switch (*request)
	{
	case EditorPlayRequest::Play:
		// 실패 이유는 Capture, Restore가 로그로 남긴다.
		if (!worldSession.StartPlay(selection))
		{
			Log::Write(LogLevel::Error, "Editor", "Could not start play");
		}
		break;
	case EditorPlayRequest::Pause:
		worldSession.SetPaused(true);
		break;
	case EditorPlayRequest::Resume:
		worldSession.SetPaused(false);
		break;
	case EditorPlayRequest::Stop:
		worldSession.StopPlay(selection);
		break;
	}
}
