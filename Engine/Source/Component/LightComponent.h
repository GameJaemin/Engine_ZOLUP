#pragma once

#include "Component.h"
#include "Core/Math/Vector3.h"

/** 조명 종류가 공유하는 색상, 상대 세기와 활성 여부를 보관한다. */
class LightComponent : public Component
{
protected:
	LightComponent() = default;

	// 색상
public:
	/** 선형 RGB 값이며 알파는 없다. 기본값은 흰색이다. */
	Vector3 GetColor() const { return color; }

	/** 모든 성분이 유한한 0 이상의 값일 때만 받는다. 아니면 기존 값을 유지하고 false를 반환한다. */
	bool SetColor(const Vector3& newColor);

private:
	Vector3 color = { 1.0f, 1.0f, 1.0f };

	// 세기
public:
	/** 색상에 곱할 상대 배율이며 물리 단위는 없다. 기본값은 1이다. */
	float GetIntensity() const { return intensity; }

	/** 유한한 0 이상의 값만 받는다. 아니면 기존 값을 유지하고 false를 반환한다. */
	bool SetIntensity(float newIntensity);

private:
	float intensity = 1.0f;

	// 활성 여부
public:
	bool IsEnabled() const { return isEnabled; }

	/** 조명 활성 설정만 바꾸며 컴포넌트 Tick에는 영향을 주지 않는다. 기본값은 true다. */
	void SetEnabled(bool enabled);

private:
	bool isEnabled = true;
};
