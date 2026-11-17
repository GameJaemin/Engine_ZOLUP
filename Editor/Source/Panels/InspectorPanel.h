#pragma once

class EditorSelection;

class InspectorPanel
{
public:
	/**
	 * 호출자는 유효한 ImGui Context에서 NewFrame 이후, Render 전에 호출한다.
	 * 선택 객체가 없거나 삭제 예정이면 창 내부를 비워둔다.
	 * 호출 중 선택 객체를 실제 파괴하거나 컴포넌트 목록의 구조를 변경하지 않는다.
	 */
	void Draw(EditorSelection& selection);
};
