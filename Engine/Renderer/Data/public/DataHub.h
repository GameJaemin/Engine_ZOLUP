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
// v0.1.0-preview


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