#pragma once

class EditorSelection;
class GameObject;
class World;

class HierarchyPanel
{
public:
	/**
	 * 호출자는 유효한 ImGui Context에서 NewFrame 이후, Render 전에 호출한다.
	 * selection은 전달한 World를 관찰하고 있어야 한다.
	 * 호출 중 객체 목록과 부모 및 자식 관계를 변경하거나 실제 삭제하지 않는다.
	 */
	void Draw(World& world, EditorSelection& selection) const;

public:
	// ImGui 창 이름이다. 도킹 기본 배치도 이 이름으로 창을 찾으므로 같은 상수를 쓴다.
	static constexpr const char* WindowName = "Hierarchy";

private:
	void DrawGameObjectNode(GameObject& gameObject, EditorSelection& selection) const;
};
