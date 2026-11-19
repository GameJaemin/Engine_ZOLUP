#pragma once

#include "DataHub_Types.h"

// ServiceTransferBox: 서비스로 보내는 데이터 전송 규약 입니다


namespace Hakko::DataHub
{

    #pragma region ServiceTransferBox

    struct ServiceTransferBox
    {
    protected:
        ServiceTransferBox() = default;
    };
    typedef ServiceTransferBox STB;


    #pragma region Readback

    // RTB_ReadbackTexture 의 결과: 텍스처 한 장의 생 데이터
    // 요청한 프레임의 Render 끝 상태. 몇 프레임 뒤 Sketch 에서 올라오고, 그 다음 Sketch 가 비움
    struct ServiceTransferBox_ReadbackTexture : public STB
    {
        TexID texId;                        // 요청을 읽은 프레임 기준 id
        uint32_t width;
        uint32_t height;
        std::vector<std::byte> rawData;     // 줄 사이 패딩 없이 width * height 픽셀, 포맷 해석은 읽는 쪽이
    };
    typedef ServiceTransferBox_ReadbackTexture STB_ReadbackTexture;


    extern std::vector<STB_ReadbackTexture> ReadbackResultList;

    #pragma endregion

    #pragma endregion

}
