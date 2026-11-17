#pragma once

struct Image;

namespace ORMImageBuilder
{
	/**
	 * 선형 RGBA8 이미지에서 차폐의 R과 금속성-거칠기의 G, B를 합친다.
	 * nullptr인 입력의 채널과 출력 A는 255로 채운다. 입력은 변경하지 않는다.
	 * 두 입력이 모두 없거나 이미지가 유효하지 않거나 크기가 다르면 false를 반환한다.
	 * 성공하면 outImage를 교체하며 실패하면 유지한다. 메모리 할당 예외는 전파한다.
	 */
	bool Create(const Image* occlusion, const Image* metallicRoughness, Image& outImage);
}
