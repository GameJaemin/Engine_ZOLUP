#pragma once

#include "DataHub_Resource.h"

#include <cstdint>
#include <string>

class TextureRenderTarget2D
{
public:
	TextureRenderTarget2D() = default;
	TextureRenderTarget2D(const TextureRenderTarget2D&) = delete;
	TextureRenderTarget2D& operator=(const TextureRenderTarget2D&) = delete;
	TextureRenderTarget2D(TextureRenderTarget2D&&) = delete;
	TextureRenderTarget2D& operator=(TextureRenderTarget2D&&) = delete;

public:
	/**
	 * DataHub 초기화 후 호출하며, 같은 프레임의 Sketch에서 업로드 요청을 소비해야 한다.
	 * 이름이 비었거나 크기가 0이거나 이미 초기화했거나 등록에 실패하면 상태를 유지하고 false를 반환한다.
	 * true는 생성 요청 접수이며 GPU 자원 생성 완료를 보장하지 않는다.
	 */
	bool Initialize(std::wstring newName, std::uint32_t newWidth, std::uint32_t newHeight);

	/**
	 * DataHub가 살아 있을 때 호출한다. 미초기화 상태와 반복 호출은 성공한다.
	 * 해제 요청이 거부되면 상태를 유지하고 false를 반환한다.
	 * true는 GPU 해제 완료를 보장하지 않으며, 같은 이름의 즉시 재등록도 보장하지 않는다.
	 */
	bool Shutdown();

public:
	/**
	 * 초기화된 객체에 양수인 픽셀 크기로 호출한다.
	 * 같은 크기면 요청 없이 성공하며, 요청이 거부되면 기존 크기를 유지한다.
	 * 업데이트 요청은 같은 프레임의 Sketch에서 소비해야 한다.
	 * true는 GPU 자원 교체 완료를 보장하지 않는다.
	 */
	bool Resize(std::uint32_t newWidth, std::uint32_t newHeight);

	/**
	 * DataHub가 살아 있을 때 현재 텍스처 ID를 이름으로 조회한다.
	 * 초기화 전, Shutdown 후 또는 등록된 이름이 없으면 0을 반환한다.
	 * 다른 텍스처 해제로 ID가 바뀔 수 있으므로 반환값을 장기 보관하지 않는다.
	 * 0이 아닌 값도 GPU 자원 생성 완료를 보장하지 않는다.
	 */
	Hakko::DataHub::TexID FindTextureID() const;

public:
	/** DataHub에 등록할 이름이다. 등록 전에는 빈 문자열이다. */
	const std::wstring& GetName() const { return name; }

	/** 마지막으로 접수된 생성 또는 크기 변경 요청의 너비다. 단위는 픽셀이며 등록 전에는 0이다. */
	std::uint32_t GetWidth() const { return width; }

	/** 마지막으로 접수된 생성 또는 크기 변경 요청의 높이다. 단위는 픽셀이며 등록 전에는 0이다. */
	std::uint32_t GetHeight() const { return height; }

private:
	// DataHub가 이름을 참조하는 동안 수명을 유지하도록 문자열을 직접 소유한다.
	std::wstring name;

	// GPU 생성 완료를 조회한 값이 아니라 접수된 요청 크기다.
	std::uint32_t width = 0;
	std::uint32_t height = 0;
};
