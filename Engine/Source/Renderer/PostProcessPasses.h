#pragma once

#include "Renderer/SceneRenderRequest.h"

#include <cstdint>
#include <vector>

namespace PostProcessPasses
{
	/**
	 * DataHub 초기화 후 메인 스레드에서 매 프레임 호출한다. 내장 후처리(톤맵, FXAA) 뒤 칸부터 passes를 순서대로 채우고 남은 칸은 끈다.
	 * 등록에 실패하거나 값이 MaxPostProcessParameters개를 넘는 후처리는 건너뛴다.
	 */
	void Configure(std::uint8_t layer, const std::vector<PostProcessPass>& passes);
}
