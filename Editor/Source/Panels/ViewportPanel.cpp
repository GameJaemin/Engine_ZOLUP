#include "ViewportPanel.h"

#include "Camera/CameraViewInfo.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Transform.h"
#include "Core/Math/Vector3.h"
#include "DataHub_Resource.h"
#include "EditorAssetDragDrop.h"
#include "EditorSelection.h"
#include "EditorViewport.h"
#include "GameObject.h"
#include "Renderer/TextureRenderTarget2D.h"

#include <imgui.h>
#include <ImGuizmo.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>

namespace
{
	Matrix4x4 CalculateParentWorldRotation(const GameObject& object)
	{
		Matrix4x4 parentWorldRotation;
		const GameObject* parent = object.GetParent();
		while (parent != nullptr)
		{
			const TransformComponent* parentTransform = parent->GetComponent<TransformComponent>();
			const Matrix4x4 parentLocalRotation = Matrix4x4::CreateFromQuaternion(parentTransform->GetTransform().rotation);
			parentWorldRotation = parentWorldRotation * parentLocalRotation;
			parent = parent->GetParent();
		}

		return parentWorldRotation;
	}

	float ApplyScaleRatio(float initialScale, float ratio)
	{
		// ImGuizmo의 행렬 계산에서 생긴 1배 주변 오차로 다른 축이 바뀌지 않게 한다.
		constexpr float UnitRatioTolerance = 1.0e-6f;
		return std::abs(ratio - 1.0f) <= UnitRatioTolerance ? initialScale : initialScale * ratio;
	}

	bool DrawTransformGizmo(EditorSelection& selection, const EditorViewport& viewport, ViewportGizmoMode gizmoMode, Vector3& scaleAtDragStart, const ImVec2& imageMin, const ImVec2& imageMax)
	{
		GameObject* selected = selection.GetSelectedGameObject();
		if (selected == nullptr || selected->IsPendingDestroy()) { return false; }

		TransformComponent* transform = selected->GetComponent<TransformComponent>();
		if (transform == nullptr) { return false; }

		const float imageWidth = imageMax.x - imageMin.x;
		const float imageHeight = imageMax.y - imageMin.y;
		if (imageWidth <= 0.0f || imageHeight <= 0.0f) { return false; }

		const CameraViewInfo cameraViewInfo = viewport.GetCameraViewInfo();
		Matrix4x4 projection;
		if (!cameraViewInfo.CalculateProjectionMatrix(imageWidth / imageHeight, projection)) { return false; }

		const Matrix4x4 view = cameraViewInfo.CalculateViewMatrix();
		const Matrix4x4 worldMatrix = transform->GetWorldMatrix();
		const Vector3 worldPosition{ worldMatrix.m[3][0], worldMatrix.m[3][1], worldMatrix.m[3][2] };
		const Matrix4x4 worldTranslation = Matrix4x4::CreateTranslation(worldPosition);

		const bool isOrthographic = cameraViewInfo.projectionMode == CameraProjectionMode::Orthographic;
		// ImGuizmo 1.10은 원근 투영의 이 깊이에서 내부 클립 영역을 해제하지 않고 반환한다.
		const Matrix4x4 clipMatrix = worldTranslation * (view * projection);
		constexpr float ImGuizmoMinClipZ = 0.001f;
		if (!isOrthographic && clipMatrix.m[3][2] < ImGuizmoMinClipZ) { return false; }

		const bool isMoveMode = gizmoMode == ViewportGizmoMode::Move;
		const bool isRotateMode = gizmoMode == ViewportGizmoMode::Rotate;
		const bool isScaleMode = gizmoMode == ViewportGizmoMode::Scale;

		ImGuizmo::OPERATION operation = ImGuizmo::TRANSLATE;
		if (isRotateMode)
		{
			operation = ImGuizmo::ROTATE_X | ImGuizmo::ROTATE_Y | ImGuizmo::ROTATE_Z;
		}
		else if (isScaleMode)
		{
			operation = ImGuizmo::SCALE;
		}

		const ImGuizmo::MODE coordinateMode = isScaleMode ? ImGuizmo::LOCAL : ImGuizmo::WORLD;
		Matrix4x4 gizmoMatrix = worldTranslation;

		// 루트 객체는 부모 변환이 없으므로 변환을 되돌릴 때 단위 행렬을 쓴다.
		Matrix4x4 inverseParentMatrix;
		bool hasParentInverse = true;
		if (isMoveMode)
		{
			const GameObject* parent = selected->GetParent();
			if (parent != nullptr)
			{
				const TransformComponent* parentTransform = parent->GetComponent<TransformComponent>();
				hasParentInverse = parentTransform->GetWorldMatrix().GetAffineInverse(inverseParentMatrix);
			}
		}
		else
		{
			// 축별 크기나 반전이 섞이지 않도록 계층의 회전만 합성한다.
			const Matrix4x4 parentWorldRotation = CalculateParentWorldRotation(*selected);
			const Matrix4x4 localRotation = Matrix4x4::CreateFromQuaternion(transform->GetTransform().rotation);
			gizmoMatrix = localRotation * parentWorldRotation * worldTranslation;
			if (isRotateMode)
			{
				hasParentInverse = parentWorldRotation.GetAffineInverse(inverseParentMatrix);
			}
		}

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->PushClipRect(imageMin, imageMax, true);
		ImGuizmo::SetDrawlist(drawList);
		ImGuizmo::SetRect(imageMin.x, imageMin.y, imageWidth, imageHeight);
		ImGuizmo::SetOrthographic(isOrthographic);
		ImGuizmo::PushID(selected);
		if (ImGuizmo::IsUsingAny() && !ImGuizmo::IsUsing())
		{
			// 다른 객체에서 시작한 드래그가 새 선택으로 이어지지 않게 한다.
			ImGuizmo::Enable(false);
		}

		const bool canUseGizmo = hasParentInverse && !viewport.IsCameraControlActive() && ImGui::IsMousePosValid();
		ImGuizmo::Enable(canUseGizmo);

		if (isScaleMode && canUseGizmo && !ImGuizmo::IsUsing() && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			scaleAtDragStart = transform->GetScale();
		}

		std::optional<ImGuizmo::Style> originalStyle;
		if (isRotateMode)
		{
			ImGuizmo::Style& style = ImGuizmo::GetStyle();
			originalStyle = style;
			const ImVec4 highlightColor(1.0f, 0.83f, 0.36f, 1.0f);
			style.Colors[ImGuizmo::DIRECTION_X] = ImVec4(1.0f, 0.38f, 0.43f, 1.0f);
			style.Colors[ImGuizmo::DIRECTION_Y] = ImVec4(0.45f, 0.86f, 0.45f, 1.0f);
			style.Colors[ImGuizmo::DIRECTION_Z] = ImVec4(0.40f, 0.66f, 1.0f, 1.0f);
			style.Colors[ImGuizmo::SELECTION] = highlightColor;
			style.Colors[ImGuizmo::ROTATION_USING_BORDER] = highlightColor;
			ImVec4& rotationFill = style.Colors[ImGuizmo::ROTATION_USING_FILL];
			rotationFill = ImVec4(highlightColor.x, highlightColor.y, highlightColor.z, rotationFill.w);
		}

		Matrix4x4 scaleDelta;
		float* deltaMatrix = isScaleMode ? &scaleDelta.m[0][0] : nullptr;
		const bool changed = ImGuizmo::Manipulate(&view.m[0][0], &projection.m[0][0], operation, coordinateMode, &gizmoMatrix.m[0][0], deltaMatrix);
		if (originalStyle.has_value())
		{
			// ImGuizmo 스타일은 Move와 공유하므로 회전 링을 그린 뒤 복원한다.
			ImGuizmo::GetStyle() = *originalStyle;
		}

		if (!isMoveMode && ImGuizmo::IsUsing() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
		{
			// ImGuizmo 1.10은 창 밖의 회전 및 크기 드래그에서 버튼 해제를 건너뛸 수 있다.
			ImGuizmo::Enable(false);
		}
		ImGuizmo::PopID();
		drawList->PopClipRect();

		if (!canUseGizmo || !changed) { return true; }

		if (isScaleMode)
		{
			// 배율은 누적값이므로 직전 프레임의 크기 대신 드래그 시작값에 적용한다.
			const float x = ApplyScaleRatio(scaleAtDragStart.x, scaleDelta.m[0][0]);
			const float y = ApplyScaleRatio(scaleAtDragStart.y, scaleDelta.m[1][1]);
			const float z = ApplyScaleRatio(scaleAtDragStart.z, scaleDelta.m[2][2]);
			if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z)) { return true; }

			transform->SetScale(Vector3{ x, y, z });
			return true;
		}

		const Matrix4x4 localMatrix = gizmoMatrix * inverseParentMatrix;
		if (isMoveMode)
		{
			const Vector3 localPosition{ localMatrix.m[3][0], localMatrix.m[3][1], localMatrix.m[3][2] };
			if (!std::isfinite(localPosition.x) || !std::isfinite(localPosition.y) || !std::isfinite(localPosition.z)) { return true; }

			transform->SetPosition(localPosition);
			return true;
		}

		const Quaternion rotation = Quaternion::CreateFromRotationMatrix(localMatrix);
		const float lengthSquared = rotation.x * rotation.x + rotation.y * rotation.y + rotation.z * rotation.z + rotation.w * rotation.w;
		if (lengthSquared <= 0.0f || !std::isfinite(lengthSquared)) { return true; }

		const float inverseLength = 1.0f / std::sqrt(lengthSquared);
		Transform localTransform = transform->GetTransform();
		localTransform.rotation = Quaternion{ rotation.x * inverseLength, rotation.y * inverseLength, rotation.z * inverseLength, rotation.w * inverseLength };
		transform->SetTransform(localTransform);
		return true;
	}
}

std::optional<std::string> ViewportPanel::Draw(EditorViewport& viewport, EditorSelection& selection)
{
	wasImageRightClicked = false;
	pickClick.reset();

	if (!ImGui::Begin(WindowName))
	{
		ImGuizmo::Enable(false);
		ImGui::End();
		return std::nullopt;
	}

	const bool isGizmoModeLocked = ImGuizmo::IsUsingAny() || viewport.IsCameraControlActive();
	ImGui::BeginDisabled(isGizmoModeLocked);
	if (ImGui::RadioButton("Move", gizmoMode == ViewportGizmoMode::Move))
	{
		gizmoMode = ViewportGizmoMode::Move;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Rotate", gizmoMode == ViewportGizmoMode::Rotate))
	{
		gizmoMode = ViewportGizmoMode::Rotate;
	}
	ImGui::SameLine();
	if (ImGui::RadioButton("Scale", gizmoMode == ViewportGizmoMode::Scale))
	{
		gizmoMode = ViewportGizmoMode::Scale;
	}
	ImGui::EndDisabled();

	std::optional<std::string> droppedPrefabPath;
	bool drewGizmo = false;
	const ImVec2 availableSize = ImGui::GetContentRegionAvail();

	// 정수 픽셀로 바꿨을 때 너비와 높이가 모두 1 이상인 경우만 준비한다.
	if (availableSize.x >= 1.0f && availableSize.y >= 1.0f)
	{
		const std::uint32_t width = static_cast<std::uint32_t>(availableSize.x);
		const std::uint32_t height = static_cast<std::uint32_t>(availableSize.y);

		if (!viewport.PrepareRenderTarget(width, height))
		{
			ImGui::TextUnformatted("Could not prepare the viewport render target.");
		}
		else
		{
			const Hakko::DataHub::TexID textureID = viewport.GetRenderTarget().FindTextureID();

			if (textureID != 0)
			{
				const ImVec2 imageSize{ static_cast<float>(width), static_cast<float>(height) };
				ImGui::Image(static_cast<ImTextureID>(textureID), imageSize);
				const ImVec2 imageMin = ImGui::GetItemRectMin();
				const ImVec2 imageMax = ImGui::GetItemRectMax();
				const bool imageLeftClicked = ImGui::IsItemClicked(ImGuiMouseButton_Left);
				const bool hasDragPayload = ImGui::GetDragDropPayload() != nullptr;

				if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				{
					// 클릭 뒤 같은 프레임에 이미지 안으로 이동한 경우를 거른다.
					const ImVec2 clickPosition = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Right];
					const bool isInsideX = clickPosition.x >= imageMin.x && clickPosition.x < imageMax.x;
					const bool isInsideY = clickPosition.y >= imageMin.y && clickPosition.y < imageMax.y;
					wasImageRightClicked = isInsideX && isInsideY;
				}

				if (ImGui::BeginDragDropTarget())
				{
					const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PrefabDragPayloadType);
					if (payload != nullptr && payload->IsDelivery() && payload->DataSize > 0)
					{
						droppedPrefabPath.emplace(static_cast<const char*>(payload->Data), static_cast<std::size_t>(payload->DataSize));
					}
					ImGui::EndDragDropTarget();
				}

				drewGizmo = DrawTransformGizmo(selection, viewport, gizmoMode, scaleAtDragStart, imageMin, imageMax);
				const bool gizmoClaimsClick = ImGuizmo::IsUsingAny() || (drewGizmo && ImGuizmo::IsOver());
				const bool canPick = !isGizmoModeLocked && !gizmoClaimsClick && !hasDragPayload;
				if (imageLeftClicked && canPick)
				{
					const ImVec2 clickPosition = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Left];
					const bool isInsideX = clickPosition.x >= imageMin.x && clickPosition.x < imageMax.x;
					const bool isInsideY = clickPosition.y >= imageMin.y && clickPosition.y < imageMax.y;
					if (isInsideX && isInsideY)
					{
						const Vector2 imageUV{ (clickPosition.x - imageMin.x) / imageSize.x, (clickPosition.y - imageMin.y) / imageSize.y };
						pickClick = ViewportPickClick{ imageUV, imageSize.x / imageSize.y };
					}
				}
			}
		}
	}

	if (!drewGizmo)
	{
		ImGuizmo::Enable(false);
	}
	wasImageRightClicked = wasImageRightClicked && !ImGuizmo::IsUsingAny();

	ImGui::End();
	return droppedPrefabPath;
}
