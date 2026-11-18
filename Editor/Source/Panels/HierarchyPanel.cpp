#include "HierarchyPanel.h"

#include "EditorSelection.h"
#include "GameObject.h"
#include "World.h"

#include <imgui.h>

#include <memory>

void HierarchyPanel::Draw(World& world, EditorSelection& selection) const
{
	if (ImGui::Begin(WindowName))
	{
		for (const std::unique_ptr<GameObject>& object : world.GetGameObjects())
		{
			if (object->GetParent() != nullptr) { continue; }
			if (object->IsPendingDestroy()) { continue; }

			DrawGameObjectNode(*object, selection);
		}
	}

	ImGui::End();
}

void HierarchyPanel::DrawGameObjectNode(GameObject& gameObject, EditorSelection& selection) const
{
	if (gameObject.IsPendingDestroy()) { return; }

	bool hasVisibleChild = false;
	for (GameObject* child : gameObject.GetChildren())
	{
		if (child->IsPendingDestroy()) { continue; }

		hasVisibleChild = true;
		break;
	}

	ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;

	if (!hasVisibleChild)
	{
		flags |= ImGuiTreeNodeFlags_Leaf;
	}

	if (selection.GetSelectedGameObject() == &gameObject)
	{
		flags |= ImGuiTreeNodeFlags_Selected;
	}

	// 이름과 별개로 객체 주소를 ID에 사용해 같은 이름의 객체를 구분한다.
	const bool isOpen = ImGui::TreeNodeEx(&gameObject, flags, "%s", gameObject.GetName().c_str());

	if (ImGui::IsItemClicked(ImGuiMouseButton_Left))
	{
		selection.Select(&gameObject);
	}

	if (isOpen)
	{
		for (GameObject* child : gameObject.GetChildren())
		{
			DrawGameObjectNode(*child, selection);
		}

		ImGui::TreePop();
	}
}
