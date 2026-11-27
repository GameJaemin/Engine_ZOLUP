#pragma once
#include "Component/Component.h"
#include <array>

/** 손에 든 아이템 ID만 관리한다. 숨기 중 아이템 처리 등 보류된 규칙은 추가하지 않는다. */
class HandInventoryComponent final : public Component
{
public:
	bool Take(int item, int hand)
	{
		if (item <= 0 || hand < 0 || hand > 1 || items[hand] != 0) { return false; }
		if (items[0] == item || items[1] == item) { return false; }
		items[hand] = item;
		return true;
	}
	bool CanUse(int item, int hand) const { return hand >= 0 && hand < 2 && items[hand] == item && item > 0; }
	int Get(int hand) const { return hand >= 0 && hand < 2 ? items[hand] : 0; }
	bool Contains(int item) const { return item>0&&(items[0]==item||items[1]==item); }
	std::array<int, 2> Capture() const { return items; }
	void Restore(const std::array<int, 2>& state) { items = state; }
private:
	std::array<int, 2> items{};
};
