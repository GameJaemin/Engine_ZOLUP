#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>
#include <array>
#include <unordered_map>
#include <string>
#include <string_view>
#include <variant>
#include <concepts>

#include <DirectXMath.h>


// 레이어 간 데이터 인터페이스 / 프로토콜 을 정의합니다
// ------
// v0.2.0-alpha: 디폴트 셰이더 추가 / 그림자, 포스트 프로세싱, 컴퓨트 빌트인 동작 구현 / ImGui 모듈 추가

// 큰 변경점, 유의사항: 
// 기존 섀도우용, 조명행렬 타입 삭제 -> 섀도우 아틀라스용 프리셋 DtB_ShadowPreset / 섀도우 패스용 DtB_ShadowView 로 변경
// 셰이더 / 리소스 / 데이터 타입 추가에 따라 기본 순서(아이디) 재배치, RESERVED_ 매크로 이용 권장
// Default_BG_VS -> Default_PP_VS 로 변경


// 기본 헤더

#include "DataHub_Types.h"
#include "DataHub_RTB.h"
#include "DataHub_Resource.h"


namespace Hakko::DataHub
{
    
    #pragma region Lifecycle

    bool Init();
    void Finalize();
    
    #pragma endregion


    #pragma region Helper

    constexpr uint32_t FNV_OFFSET_BASIS = 2166136261u;
    constexpr uint32_t FNV_PRIME = 16777619u;

    constexpr uint32_t HashString(std::string_view str) {
        uint32_t hash = FNV_OFFSET_BASIS;
        for (char c : str) {
            hash ^= static_cast<uint32_t>(c);
            hash *= FNV_PRIME;
        }
        return hash;
    }

    constexpr uint32_t HashString(std::wstring_view str) {
        uint32_t hash = FNV_OFFSET_BASIS;
        for (wchar_t c : str) {
            hash ^= static_cast<uint32_t>(c);
            hash *= FNV_PRIME;
        }
        return hash;
    }

    #pragma endregion

}


// 필수 포함 헤더

#include "DataHub_RenderIdProvider.h"
#include "DataHub_DataBoxCustomizer.h"