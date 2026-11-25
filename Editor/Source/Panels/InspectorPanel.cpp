#include "InspectorPanel.h"

#include "Component/Component.h"
#include "ComponentTypeRegistry.h"
#include "Core/Math/EulerAngles.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "EditorSelection.h"
#include "GameObject.h"
#include "World.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cctype>
#include <cfloat>
#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <type_traits>
#include <typeinfo>
#include <variant>
#include <vector>

namespace
{
	// if constexpr 마지막 분기에서 처리하지 않은 타입을 빌드 오류로 만들기 위한 항상 거짓인 값이다.
	template <typename>
	inline constexpr bool AlwaysFalse = false;

	/** camelCase 속성 이름을 화면 표시용으로 바꾼다. 첫 글자를 대문자로 하고 대문자 앞에 공백을 넣는다. 예: verticalFovDegrees -> Vertical Fov Degrees */
	std::string MakeDisplayName(std::string_view fieldName)
	{
		std::string displayName;
		displayName.reserve(fieldName.size() + 4);

		for (std::size_t index = 0; index < fieldName.size(); ++index)
		{
			const char character = fieldName[index];
			if (index == 0)
			{
				displayName.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(character))));
				continue;
			}
			if (std::isupper(static_cast<unsigned char>(character))) { displayName.push_back(' '); }
			displayName.push_back(character);
		}
		return displayName;
	}

	/** typeInfo나 그 부모에 Inspector에 보이는(Hidden이 아닌) 속성이 하나라도 있으면 true를 반환한다. */
	bool HasVisibleFields(const ComponentTypeInfo& typeInfo)
	{
		const ComponentTypeInfo* current = &typeInfo;
		while (current != nullptr)
		{
			for (const ComponentFieldInfo& field : current->fields)
			{
				if (field.display.visibility != ComponentFieldVisibility::Hidden) { return true; }
			}
			current = current->parent;
		}
		return false;
	}

	/** field의 현재 값을 값 종류에 맞는 입력 칸으로 그리고, 바뀌면 set으로 쓴다. setter가 거부하면 다음 프레임에 원래 값이 보인다. */
	void DrawFieldEditor(const ComponentFieldInfo& field, Component& component)
	{
		const ComponentFieldValue value = field.get(component);

		std::visit([&field, &component](const auto& current)
		{
			using Value = std::decay_t<decltype(current)>;
			Value edited = current;
			bool changed = false;

			if constexpr (std::is_same_v<Value, bool>)
			{
				changed = ImGui::Checkbox("##Value", &edited);
			}
			else
			{
				// 입력 칸을 값 열의 남은 폭만큼 넓힌다.
				ImGui::SetNextItemWidth(-FLT_MIN);

				// 직접 입력한 값도 범위로 맞춘다. AlwaysClamp는 범위가 0~0인 칸도 0에 묶으므로(ClampZeroRange 포함) 쓰지 않는다.
				const ComponentFieldDisplay& display = field.display;
				constexpr ImGuiSliderFlags dragFlags = ImGuiSliderFlags_ClampOnInput;

				if constexpr (std::is_same_v<Value, int>)
				{
					if (display.enumNames.empty())
					{
						changed = ImGui::InputInt("##Value", &edited);
					}
					else
					{
						// 값이 이름 목록 밖이면 숫자 그대로 보여 준다.
						const int nameCount = static_cast<int>(display.enumNames.size());
						const std::string preview = (edited >= 0 && edited < nameCount) ? display.enumNames[edited] : std::to_string(edited);
						if (ImGui::BeginCombo("##Value", preview.c_str()))
						{
							for (int index = 0; index < nameCount; ++index)
							{
								const bool isSelected = index == edited;
								if (ImGui::Selectable(display.enumNames[index].c_str(), isSelected))
								{
									edited = index;
									changed = true;
								}
								if (isSelected) { ImGui::SetItemDefaultFocus(); }
							}
							ImGui::EndCombo();
						}
					}
				}
				else if constexpr (std::is_same_v<Value, float>)
				{
					changed = ImGui::DragFloat("##Value", &edited, display.dragSpeed, display.minValue, display.maxValue, "%.3f", dragFlags);
				}
				else if constexpr (std::is_same_v<Value, Vector2>)
				{
					float values[2] = { edited.x, edited.y };
					changed = ImGui::DragFloat2("##Value", values, display.dragSpeed, display.minValue, display.maxValue, "%.3f", dragFlags);
					edited = Vector2{ values[0], values[1] };
				}
				else if constexpr (std::is_same_v<Value, Vector3>)
				{
					float values[3] = { edited.x, edited.y, edited.z };
					if (display.isColor)
					{
						changed = ImGui::ColorEdit3("##Value", values, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR);
					}
					else
					{
						changed = ImGui::DragFloat3("##Value", values, display.dragSpeed, display.minValue, display.maxValue, "%.3f", dragFlags);
					}
					edited = Vector3{ values[0], values[1], values[2] };
				}
				else if constexpr (std::is_same_v<Value, EulerAngles>)
				{
					float values[3] = { edited.pitch, edited.yaw, edited.roll };
					changed = ImGui::DragFloat3("##Value", values, display.dragSpeed, display.minValue, display.maxValue, "%.3f", dragFlags);
					edited = EulerAngles{ values[0], values[1], values[2] };
				}
				else if constexpr (std::is_same_v<Value, Transform>)
				{
					// Transform 하나짜리 속성은 아직 없다. 생기면 위치, 회전, 크기 편집 칸으로 바꾼다.
					ImGui::TextDisabled("position %.1f, %.1f, %.1f", edited.position.x, edited.position.y, edited.position.z);
				}
				else if constexpr (std::is_same_v<Value, AssetReference>)
				{
					// 에셋을 고르는 창이 아직 없어 지정 여부만 읽기 전용으로 보여 준다.
					ImGui::TextDisabled("%s", edited.asset != nullptr ? "Assigned" : "None");
				}
				else if constexpr (std::is_same_v<Value, ComponentReference>)
				{
					// 가리키는 객체를 이 컴포넌트의 World에서 찾아 이름을 읽기 전용으로 보여 준다.
					const GameObject* owner = component.GetGameObject();
					const GameObject* target = (owner != nullptr && edited.objectID.IsValid()) ? owner->GetWorld().FindGameObject(edited.objectID) : nullptr;
					ImGui::TextDisabled("%s", target != nullptr ? target->GetName().c_str() : "None");
				}
				else if constexpr (std::is_same_v<Value, ComponentFieldList>)
				{
					// 원소를 펼쳐 보는 기능은 아직 없어 개수만 보여 준다.
					ImGui::TextDisabled("%zu elements", edited.elements.size());
				}
				else { static_assert(AlwaysFalse<Value>, "Unhandled ComponentFieldValue type."); }
			}

			if (changed) { field.set(component, ComponentFieldValue(edited)); }
		}, value);
	}

	/** typeInfo의 속성(부모 속성 포함)을 이름과 값의 2열 표로 그린다. */
	void DrawFieldTable(const ComponentTypeInfo& typeInfo, Component& component)
	{
		// 표가 보이지 않아 그릴 필요가 없으면 false이며, 이때는 EndTable을 부르지 않는다.
		if (!ImGui::BeginTable("Fields", 2, ImGuiTableFlags_SizingStretchProp)) { return; }

		ImGui::TableSetupColumn("Name", ImGuiTableColumnFlags_WidthStretch, 0.4f);
		ImGui::TableSetupColumn("Value", ImGuiTableColumnFlags_WidthStretch, 0.6f);

		ComponentTypeRegistry::ForEachField(typeInfo, [&component](const ComponentFieldInfo& field)
		{
			if (field.display.visibility == ComponentFieldVisibility::Hidden) { return; }

			// 행마다 입력 칸의 ID가 섞이지 않게 속성 이름으로 구분한다.
			ImGui::PushID(field.name.c_str());
			ImGui::TableNextRow();
			ImGui::TableSetColumnIndex(0);
			const std::string displayName = field.display.displayName.empty() ? MakeDisplayName(field.name) : field.display.displayName;
			ImGui::TextUnformatted(displayName.c_str());
			ImGui::TableSetColumnIndex(1);

			// 읽기 전용이면 편집 칸을 흐리게 그리고 입력을 막는다.
			const bool readOnly = field.display.visibility == ComponentFieldVisibility::ReadOnly;
			if (readOnly) { ImGui::BeginDisabled(); }
			DrawFieldEditor(field, component);
			if (readOnly) { ImGui::EndDisabled(); }

			ImGui::PopID();
		});

		ImGui::EndTable();
	}
}

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
		// 등록되지 않은 타입(게임이 등록하지 않은 컴포넌트)은 컴파일러가 정한 타입 이름으로 표시한다.
		const ComponentTypeInfo* typeInfo = ComponentTypeRegistry::Get().Find(*component);
		const char* typeName = typeInfo != nullptr ? typeInfo->name.c_str() : typeid(*component).name();
		// 같은 타입 컴포넌트가 둘이어도 머리글의 열림 상태가 섞이지 않게 컴포넌트 주소로 구분한다.
		ImGui::PushID(component.get());
		if (ImGui::CollapsingHeader(typeName, ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (typeInfo == nullptr) { ImGui::TextDisabled("Unregistered component type"); }
			else if (!HasVisibleFields(*typeInfo)) { ImGui::TextDisabled("No properties"); }
			else { DrawFieldTable(*typeInfo, *component); }
		}
		ImGui::PopID();
	}

	ImGui::PopID();
	ImGui::End();
}
