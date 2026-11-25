#pragma once

#include "Component.h"

#include <array>
#include <memory>

class RenderScene;
class Texture;

// faces 배열의 면 순서다. 값이 배열 번호와 같다.
enum class SkyboxFace : unsigned char { PositiveX, NegativeX, PositiveY, NegativeY, PositiveZ, NegativeZ };

/** 한 World의 배경 후보로 사용할 큐브맵 면을 공유 참조로 보관한다. */
class SkyboxComponent : public Component
{
public:
	/** 등록된 RenderScene에서 자신을 제거한다. */
	~SkyboxComponent() override;

	/** 소속 World의 RenderScene에 등록한다. GameObject::AddComponent가 호출한다. */
	void OnRegister() override;

	/** +X, -X, +Y, -Y, +Z, -Z 순서로 지정한다. 빈 면이 있으면 배경 후보에서 건너뛴다. */
	void SetFaces(std::array<std::shared_ptr<const Texture>, 6> newFaces);
	const std::array<std::shared_ptr<const Texture>, 6>& GetFaces() const { return faces; }

	/** face 한 면의 텍스처를 반환한다. 비어 있거나 face가 범위 밖이면 nullptr다. */
	std::shared_ptr<const Texture> GetFace(SkyboxFace face) const;

	/** face 한 면만 바꾼다. face가 범위 밖이면 바꾸지 않고 false를 반환한다. */
	bool SetFace(SkyboxFace face, std::shared_ptr<const Texture> newFace);

	/** 배경 후보로 사용할지 설정한다. 기본값은 true다. */
	void SetEnabled(bool enabled);
	bool IsEnabled() const { return isEnabled; }

private:
	// 비소유이며 World가 이 컴포넌트보다 오래 유지한다.
	RenderScene* registeredRenderScene = nullptr;
	std::array<std::shared_ptr<const Texture>, 6> faces{};
	bool isEnabled = true;
};
