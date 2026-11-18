#include "InspectorPanel.h"

#include "Component/Component.h"
#include "Component/TransformComponent.h"
#include "Core/Math/EulerAngles.h"
#include "Core/Math/Vector3.h"
#include "EditorSelection.h"
#include "GameObject.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cmath>
#include <memory>
#include <string>
#include <typeinfo>

void InspectorPanel::Draw(EditorSelection& selection)
{
	if (!ImGui::Begin(WindowName))
	{
		ImGui::End();
		return;
	}

	GameObject* selected = selection.GetSelectedGameObject();
	if (selected == nullptr || selected->IsPendingDestroy())
	{
		ImGui::End();
		return;
	}

	ImGui::PushID(selected);

	// 편집 결과를 매 프레임 이름에 반영하고, 입력 항목의 편집 상태는 객체별 ImGui ID로 구분한다.
	std::string name = selected->GetName();
	if (ImGui::InputText("Name", &name))
	{
		selected->SetName(name);
	}

	const GameObject* parent = selected->GetParent();
	ImGui::TextUnformatted("Parent");
	ImGui::SameLine();
	ImGui::TextUnformatted(parent != nullptr ? parent->GetName().c_str() : "None");

	ImGui::SeparatorText("Components");

	for (const std::unique_ptr<Component>& component : selected->GetComponents())
	{
		ImGui::BulletText("%s", typeid(*component).name());
	}

	TransformComponent* transform = selected->GetComponent<TransformComponent>();
	if (transform != nullptr)
	{
		ImGui::SeparatorText("Local Transform");

		const Vector3 position = transform->GetPosition();
		float positionValues[3] = { position.x, position.y, position.z };

		if (ImGui::DragFloat3("Position", positionValues, 0.1f))
		{
			if (std::isfinite(positionValues[0]) && std::isfinite(positionValues[1]) && std::isfinite(positionValues[2]))
			{
				transform->SetPosition(Vector3{ positionValues[0], positionValues[1], positionValues[2] });
			}
		}

		const EulerAngles rotation = transform->GetRotation();
		float rotationValues[3] = { rotation.pitch, rotation.yaw, rotation.roll };

		if (ImGui::DragFloat3("Rotation (deg)", rotationValues, 0.5f))
		{
			if (std::isfinite(rotationValues[0]) && std::isfinite(rotationValues[1]) && std::isfinite(rotationValues[2]))
			{
				transform->SetRotation(EulerAngles{ rotationValues[0], rotationValues[1], rotationValues[2] });
			}
		}

		const Vector3 scale = transform->GetScale();
		float scaleValues[3] = { scale.x, scale.y, scale.z };

		if (ImGui::DragFloat3("Scale", scaleValues, 0.01f))
		{
			if (std::isfinite(scaleValues[0]) && std::isfinite(scaleValues[1]) && std::isfinite(scaleValues[2]))
			{
				transform->SetScale(Vector3{ scaleValues[0], scaleValues[1], scaleValues[2] });
			}
		}
	}

	ImGui::PopID();
	ImGui::End();
}
