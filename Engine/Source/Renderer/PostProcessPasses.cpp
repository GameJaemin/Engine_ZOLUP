#include "PostProcessPasses.h"

#include "Core/Log.h"
#include "Renderer/HakkoResources.h"

#include "DataHub.h"

#include <algorithm>
#include <cstddef>

namespace
{
	// 내장 후처리(칸 0 톤맵, 칸 1 FXAA) 뒤의 첫 칸이다. Engine.cpp의 내장 후처리 칸 배치가 바뀌면 함께 바꾼다.
	constexpr std::size_t FirstCustomPostProcessSlot = 2;

	static_assert(MaxPostProcessParameters == RENDER_EDIT_CUSTOM_VALUE_MAX, "후처리 값 최대 개수가 렌더러 액션 데이터 칸 수와 다름");
}

void PostProcessPasses::Configure(std::uint8_t layer, const std::vector<PostProcessPass>& passes)
{
	auto& slots = Hakko::DataHub::LayerList[layer].actionPPs;
	std::size_t slot = FirstCustomPostProcessSlot;

	for (const PostProcessPass& pass : passes)
	{
		if (slot >= slots.size())
		{
			Log::Write(LogLevel::Warning, "Engine", "Too many post process passes");
			break;
		}
		if (pass.parameters.size() > MaxPostProcessParameters)
		{
			Log::Write(LogLevel::Error, "Engine", "Too many post process parameters: " + pass.shaderPath);
			continue;
		}

		// 등록에 실패한 후처리는 건너뛰고 다음 후처리가 같은 칸을 쓴다.
		Hakko::DataHub::DrawPropID drawPropID = 0;
		if (!HakkoResources::PreparePostProcessShader(pass.shaderPath, drawPropID)) { continue; }

		Hakko::DataHub::DtB_EditCustom data = Hakko::DataHub::DtB_EditCustom();
		std::copy(pass.parameters.begin(), pass.parameters.end(), data.values);

		// 칸마다 값 묶음 하나를 만들어 두고 다음 프레임부터는 내용만 바꾼다.
		Hakko::DataHub::RTB_ActionEdit& action = slots[slot];
		action.drawProp = drawPropID;
		if (action.actionDataBoxId == 0) { action.actionDataBoxId = Hakko::DataHub::AddDataBox(data); }
		else { Hakko::DataHub::UpdateDataBox(action.actionDataBoxId, data); }

		++slot;
	}

	// 지난 프레임에 이 레이어를 쓴 다른 요청의 후처리가 남지 않게 한다.
	for (; slot < slots.size(); ++slot) { slots[slot].drawProp = 0; }
}
