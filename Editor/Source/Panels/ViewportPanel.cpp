#include "ViewportPanel.h"

#include "DataHub_Resource.h"
#include "EditorViewport.h"
#include "Renderer/TextureRenderTarget2D.h"

#include <imgui.h>

#include <cstdint>

void ViewportPanel::Draw(EditorViewport& viewport)
{
	wasImageRightClicked = false;

	if (!ImGui::Begin(WindowName))
	{
		ImGui::End();
		return;
	}

	const ImVec2 availableSize = ImGui::GetContentRegionAvail();

	// 정수 픽셀로 바꿨을 때 너비와 높이가 모두 1 이상인 경우만 준비한다.
	if (availableSize.x >= 1.0f && availableSize.y >= 1.0f)
	{
		const std::uint32_t width = static_cast<std::uint32_t>(availableSize.x);
		const std::uint32_t height = static_cast<std::uint32_t>(availableSize.y);

		if (!viewport.PrepareRenderTarget(width, height))
		{
			ImGui::TextUnformatted("Could not prepare the viewport render target.");
		}
		else
		{
			const Hakko::DataHub::TexID textureID = viewport.GetRenderTarget().FindTextureID();

			if (textureID != 0)
			{
				const ImVec2 imageSize{ static_cast<float>(width), static_cast<float>(height) };
				ImGui::Image(static_cast<ImTextureID>(textureID), imageSize);

				if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
				{
					// 클릭 뒤 같은 프레임에 이미지 안으로 이동한 경우를 거른다.
					const ImVec2 clickPosition = ImGui::GetIO().MouseClickedPos[ImGuiMouseButton_Right];
					const ImVec2 imageMin = ImGui::GetItemRectMin();
					const ImVec2 imageMax = ImGui::GetItemRectMax();
					const bool isInsideX = clickPosition.x >= imageMin.x && clickPosition.x < imageMax.x;
					const bool isInsideY = clickPosition.y >= imageMin.y && clickPosition.y < imageMax.y;
					wasImageRightClicked = isInsideX && isInsideY;
				}

			}
		}
	}

	ImGui::End();
}
