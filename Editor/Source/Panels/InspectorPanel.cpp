#include "InspectorPanel.h"

#include "Component/Component.h"
#include "Component/ComponentTypeRegistry.h"
#include "Component/SceneCapture2DComponent.h"
#include "Core/Math/EulerAngles.h"
#include "Core/Math/Quaternion.h"
#include "Core/Math/Vector2.h"
#include "Core/Math/Vector3.h"
#include "DataHub_Resource.h"
#include "EditorSelection.h"
#include "GameObject.h"
#include "Renderer/TextureRenderTarget2D.h"
#include "World.h"

#include <imgui.h>
#include <imgui_stdlib.h>

#include <cctype>
#include <cfloat>
#include <cstddef>
#include <memory>
#include <optional>
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

	/** 값 하나를 종류에 맞는 입력 칸으로 그리고 바뀌면 true를 반환한다. 칸 폭은 부르는 쪽이 정한다. 목록은 다루지 않는다. */
	template <typename Value>
	bool DrawValueEditor(const ComponentFieldDisplay& display, const Component& component, Value& edited)
	{
		// 직접 입력한 값도 범위로 맞춘다. AlwaysClamp는 범위가 0~0인 칸도 0에 묶으므로(ClampZeroRange 포함) 쓰지 않는다.
		constexpr ImGuiSliderFlags dragFlags = ImGuiSliderFlags_ClampOnInput;

		if constexpr (std::is_same_v<Value, bool>)
		{
			return ImGui::Checkbox("##Value", &edited);
		}
		else if constexpr (std::is_same_v<Value, int>)
		{
			if (display.enumNames.empty())
			{
				return ImGui::InputInt("##Value", &edited);
			}

			// 값이 이름 목록 밖이면 숫자 그대로 보여 준다.
			bool changed = false;
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
			return changed;
		}
		else if constexpr (std::is_same_v<Value, float>)
		{
			return ImGui::DragFloat("##Value", &edited, display.dragSpeed, display.minValue, display.maxValue, "%.3f", dragFlags);
		}
		else if constexpr (std::is_same_v<Value, Vector2>)
		{
			float values[2] = { edited.x, edited.y };
			const bool changed = ImGui::DragFloat2("##Value", values, display.dragSpeed, display.minValue, display.maxValue, "%.3f", dragFlags);
			edited = Vector2{ values[0], values[1] };
			return changed;
		}
		else if constexpr (std::is_same_v<Value, Vector3>)
		{
			float values[3] = { edited.x, edited.y, edited.z };
			const bool changed = display.isColor
				? ImGui::ColorEdit3("##Value", values, ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)
				: ImGui::DragFloat3("##Value", values, display.dragSpeed, display.minValue, display.maxValue, "%.3f", dragFlags);
			edited = Vector3{ values[0], values[1], values[2] };
			return changed;
		}
		else if constexpr (std::is_same_v<Value, EulerAngles>)
		{
			float values[3] = { edited.pitch, edited.yaw, edited.roll };
			const bool changed = ImGui::DragFloat3("##Value", values, display.dragSpeed, display.minValue, display.maxValue, "%.3f", dragFlags);
			edited = EulerAngles{ values[0], values[1], values[2] };
			return changed;
		}
		else if constexpr (std::is_same_v<Value, Quaternion>)
		{
			// 쿼터니언은 저장용 숨김 속성으로 쓰고 편집은 오일러 각 속성으로 한다. 숨기지 않았을 때를 위해 값만 읽기 전용으로 보여 준다.
			ImGui::TextDisabled("%.4f, %.4f, %.4f, %.4f", edited.x, edited.y, edited.z, edited.w);
			return false;
		}
		else if constexpr (std::is_same_v<Value, Transform>)
		{
			// Transform 하나짜리 속성은 아직 없다. 생기면 위치, 회전, 크기 편집 칸으로 바꾼다.
			ImGui::TextDisabled("position %.1f, %.1f, %.1f", edited.position.x, edited.position.y, edited.position.z);
			return false;
		}
		else if constexpr (std::is_same_v<Value, AssetReference>)
		{
			// 에셋을 고르는 창이 아직 없어 지정 여부만 읽기 전용으로 보여 준다.
			ImGui::TextDisabled("%s", edited.asset != nullptr ? "Assigned" : "None");
			return false;
		}
		else if constexpr (std::is_same_v<Value, ComponentReference>)
		{
			// 가리키는 객체를 이 컴포넌트의 World에서 찾아 이름을 읽기 전용으로 보여 준다.
			const GameObject* owner = component.GetGameObject();
			const GameObject* target = (owner != nullptr && edited.objectID.IsValid()) ? owner->GetWorld().FindGameObject(edited.objectID) : nullptr;
			ImGui::TextDisabled("%s", target != nullptr ? target->GetName().c_str() : "None");
			return false;
		}
		else if constexpr (std::is_same_v<Value, std::string>)
		{
			// 경로를 반쯤 입력한 값이 setter로 가지 않게 Enter를 눌렀을 때만 적용한다.
			return ImGui::InputText("##Value", &edited, ImGuiInputTextFlags_EnterReturnsTrue);
		}
		else { static_assert(AlwaysFalse<Value>, "Unhandled ComponentFieldScalar type."); }
	}

	/**
	 * 목록을 접어서 개수를 보여 주고, 펼치면 원소마다 입력 칸을 그린다.
	 * canResize이고 원소 기본값이 있으면 원소 추가와 삭제 버튼도 그린다. 바뀌면 true를 반환한다.
	 */
	bool DrawListEditor(const ComponentFieldInfo& field, const Component& component, ComponentFieldList& list)
	{
		if (!ImGui::TreeNode("##List", "%zu elements", list.elements.size())) { return false; }

		const bool canResize = field.display.canResize && field.elementPrototype.has_value();
		bool changed = false;
		std::optional<std::size_t> removeIndex;

		for (std::size_t index = 0; index < list.elements.size(); ++index)
		{
			ImGui::PushID(static_cast<int>(index));
			ImGui::Text("[%zu]", index);
			ImGui::SameLine();

			// 삭제 버튼이 있으면 그 자리를 남긴다.
			ImGui::SetNextItemWidth(canResize ? -ImGui::GetFrameHeightWithSpacing() : -FLT_MIN);
			changed |= std::visit([&field, &component](auto& element) { return DrawValueEditor(field.display, component, element); }, list.elements[index]);

			if (canResize)
			{
				ImGui::SameLine();
				if (ImGui::SmallButton("-")) { removeIndex = index; }
			}
			ImGui::PopID();
		}

		// 반복 중에 지우면 다음 원소를 건너뛰므로 반복이 끝난 뒤 지운다.
		if (removeIndex.has_value())
		{
			list.elements.erase(list.elements.begin() + static_cast<std::ptrdiff_t>(*removeIndex));
			changed = true;
		}
		if (canResize && ImGui::SmallButton("+"))
		{
			list.elements.push_back(*field.elementPrototype);
			changed = true;
		}

		ImGui::TreePop();
		return changed;
	}

	/** field의 현재 값을 값 종류에 맞는 입력 칸으로 그리고, 바뀌면 set으로 쓴다. setter가 거부하면 다음 프레임에 원래 값이 보인다. */
	void DrawFieldEditor(const ComponentFieldInfo& field, Component& component)
	{
		const std::optional<ComponentFieldValue> value = field.get(component);
		if (!value.has_value())
		{
			// 다른 World나 등록되지 않은 객체의 컴포넌트를 가리키는 참조다. 빈 참조(None)와 구분해 보여 주고 편집하지 않는다.
			ImGui::TextDisabled("Invalid");
			return;
		}

		std::visit([&field, &component](const auto& current)
		{
			using Value = std::decay_t<decltype(current)>;
			Value edited = current;
			bool changed = false;

			if constexpr (std::is_same_v<Value, ComponentFieldList>)
			{
				changed = DrawListEditor(field, component, edited);
			}
			else
			{
				// 입력 칸을 값 열의 남은 폭만큼 넓힌다. 체크박스는 폭의 영향을 받지 않는다.
				ImGui::SetNextItemWidth(-FLT_MIN);
				changed = DrawValueEditor(field.display, component, edited);
			}

			if (changed) { field.set(component, ComponentFieldValue(edited)); }
		}, *value);
	}

	/** component가 SceneCapture2DComponent면 캡처 결과를 확인할 수 있게 렌더 타깃을 Inspector 폭에 맞춰 그린다. 타깃 생성 전이면 그리지 않는다. */
	void DrawCapturePreview(const Component& component)
	{
		const auto* capture = dynamic_cast<const SceneCapture2DComponent*>(&component);
		if (capture == nullptr) { return; }

		const TextureRenderTarget2D& target = capture->GetRenderTarget();
		const Hakko::DataHub::TexID textureID = target.FindTextureID();
		if (textureID == 0 || target.GetWidth() == 0) { return; }

		// 타깃의 가로세로 비율을 유지한다.
		const float width = ImGui::GetContentRegionAvail().x;
		const float height = width * static_cast<float>(target.GetHeight()) / static_cast<float>(target.GetWidth());
		ImGui::Image(static_cast<ImTextureID>(textureID), ImVec2(width, height));
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
			DrawCapturePreview(*component);
		}
		ImGui::PopID();
	}

	ImGui::PopID();
	ImGui::End();
}
