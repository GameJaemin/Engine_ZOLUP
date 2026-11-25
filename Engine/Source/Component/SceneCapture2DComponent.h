#pragma once

#include "Component.h"
#include "Renderer/SceneRenderRequest.h"
#include "Renderer/TextureRenderTarget2D.h"

#include <cstdint>
#include <optional>
#include <string>

class RenderScene;

class SceneCapture2DComponent : public Component
{
public:
	/** 등록된 RenderScene에서 자신을 제거하고 렌더 타깃의 해제를 요청한다. DataHub가 살아 있는 동안 소멸해야 한다. */
	~SceneCapture2DComponent() override;

	/**
	 * 소속 World의 RenderScene에 캡처 후보로 등록하고 현재 해상도로 렌더 타깃 생성을 요청한다.
	 * GameObject::AddComponent가 호출하며 DataHub 초기화 후여야 한다. 생성 요청은 같은 프레임의 Sketch에서 처리해야 한다.
	 */
	void OnRegister() override;

	/** 출력 해상도다. 단위는 픽셀이며 기본값은 256 x 256이다. */
	std::uint32_t GetWidth() const { return width; }
	std::uint32_t GetHeight() const { return height; }

	/**
	 * 1 이상 MaxResolution 이하만 받는다. 타깃이 있으면 크기 변경을 요청하고, 거부되면 기존 값을 유지하고 false를 반환한다.
	 * 등록 전에는 값만 저장하며 OnRegister가 그 값으로 타깃을 만든다.
	 */
	bool SetWidth(std::uint32_t newWidth);
	bool SetHeight(std::uint32_t newHeight);

	static constexpr std::uint32_t MaxResolution = 4096;

	/**
	 * 같은 객체의 CameraComponent에서 시점 값을 얻는다.
	 * 소속 객체나 카메라가 없거나 시점 조회에 실패하면 출력값을 유지하고 false를 반환한다.
	 */
	bool GetCaptureViewInfo(CameraViewInfo& outCameraViewInfo) const;

	/** 이 컴포넌트가 소유한 타깃을 빌려준다. 등록 전이나 생성 요청이 거부되면 빈 타깃이다. */
	const TextureRenderTarget2D& GetRenderTarget() const { return renderTarget; }

	/**
	 * 소속 World의 장면과 카메라 시점 및 출력 타깃을 요청으로 묶는다.
	 * 소속 객체가 없거나 삭제 예정이거나, 타깃 또는 시점이 준비되지 않으면 nullopt를 반환한다.
	 * 장면과 타깃은 같은 프레임의 렌더 제출 처리가 끝날 때까지 빌린다.
	 */
	std::optional<SceneRenderRequest> BuildRenderRequest();

private:
	// 비소유이며 World가 이 컴포넌트보다 오래 유지한다.
	RenderScene* registeredRenderScene = nullptr;
	TextureRenderTarget2D renderTarget;

	// 등록 전에도 유지하는 요청 해상도다. GPU 생성 완료 여부와 별개로 타깃 크기는 이 값을 따른다.
	std::uint32_t width = 256;
	std::uint32_t height = 256;
};
