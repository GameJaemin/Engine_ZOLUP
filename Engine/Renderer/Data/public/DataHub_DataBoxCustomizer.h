#pragma once

#include "DataHub.h"

#include <memory>
#include <cstddef>
#include <concepts>


namespace Hakko::DataHub
{

    // 미구현: Structured 는 등록 시 리포트 후 Constant 로 취급
    enum DataBoxUploadMode
    {
        DataBoxUploadMode_Constant,
        DataBoxUploadMode_Structured,
    };

    struct IDtBProvider
    {
        virtual ~IDtBProvider() = default;

        size_t sizeofType = 0;
        void* data = nullptr;
        uint32_t size = 0;
        std::vector<DataBoxID> blankedIdList;
        DataBoxUploadMode uploadMode = DataBoxUploadMode_Constant;

        void* GetData(size_t i) { return static_cast<std::byte*>(data) + i * sizeofType; }

        virtual void ClearData() = 0;
    };
    
    
    template <std::derived_from<DataBox_Custom> CustomDataBox>
    struct DataBoxProvider : public IDtBProvider
    {
        std::vector<CustomDataBox> dataBoxList;

        DataBoxProvider() { sizeofType = sizeof(CustomDataBox); }

        void ClearData() override
        {
            dataBoxList = {};
            blankedIdList = {};
            data = nullptr;
            size = 0;
        }
    };
    

    class DataBoxCustomizer
    {
    private:
        static inline DataBoxTID next_tid = DataBoxType_CustomizableBegin;
        static inline std::vector<std::unique_ptr<IDtBProvider>> providerList;

    public:    
        template <std::derived_from<DataBox_Custom> CustomDataBox>
        static DataBoxTID GetTypeId();

        template <std::derived_from<DataBox_Custom> CustomDataBox>
        static void RegisterCustomDataBox(DataBoxUploadMode uploadMode = DataBoxUploadMode_Constant);

        template <std::derived_from<DataBox_Custom> CustomDataBox>
        static DataBoxID AddDataBox(CustomDataBox&& dataBox);

        template <std::derived_from<DataBox_Custom> CustomDataBox>
        static void RemoveDataBox(DataBoxID dataId);

        static void RemoveDataBox(DataBoxTID tid, DataBoxID dataId);

        // [주의]
        // 벡터 직접 조작은 권장되진 않습니다
        // IDtBProvider 값 갱신 필요
        template <std::derived_from<DataBox_Custom> CustomDataBox>
        static std::vector<CustomDataBox>& GetDataBoxList();

        
        static void ClearAll();


        static IDtBProvider* GetProvider(DataBoxTID tid);
        static DataBoxTID GetProviderCount();
    };
    
}


namespace Hakko::DataHub
{

#pragma region BuiltInTypeId

template <>
inline DataBoxTID DataBoxCustomizer::GetTypeId<DataBox_Render3DGeneral>()
{
    return DataBoxType_DtB_3DGeneral;
}

template <>
inline DataBoxTID DataBoxCustomizer::GetTypeId<DataBox_Render3DOptional>()
{
    return DataBoxType_DtB_3DOptional;
}

template <>
inline DataBoxTID DataBoxCustomizer::GetTypeId<DataBox_Render2DScreen>()
{
    return DataBoxType_DtB_2DScreen;
}

#pragma endregion


template <std::derived_from<DataBox_Custom> CustomDataBox>
DataBoxTID DataBoxCustomizer::GetTypeId()
{
    static const DataBoxTID tid = next_tid++;
    return tid;
}

template <std::derived_from<DataBox_Custom> CustomDataBox>
void DataBoxCustomizer::RegisterCustomDataBox(DataBoxUploadMode uploadMode)
{
    // 미구현
    if (uploadMode == DataBoxUploadMode_Structured)
    {
        DataHub::Internal::PushHubReport(HubReport_Contract, L"[Hub] 데이터박스 구조버퍼 업로드 모드 미구현 (Constant 로 취급)");
        uploadMode = DataBoxUploadMode_Constant;
    }

    DataBoxTID tid = GetTypeId<CustomDataBox>();

    if (providerList.size() > tid && providerList[tid] != nullptr)
    {
        DataHub::Internal::PushHubReport(HubReport_Internal, L"[Hub] 데이터박스 이미 등록된 타입");
        return;
    }
    
    if (providerList.size() <= tid)
        providerList.resize(tid + 1);
    providerList[tid] = std::make_unique<DataBoxProvider<CustomDataBox>>();

    DataBoxProvider<CustomDataBox>* provider = static_cast<DataBoxProvider<CustomDataBox>*>(providerList[tid].get());
    provider->sizeofType = sizeof(CustomDataBox);
    provider->data = provider->dataBoxList.data();
    provider->size = 0;
    provider->uploadMode = uploadMode;
}

template <std::derived_from<DataBox_Custom> CustomDataBox>
DataBoxID DataBoxCustomizer::AddDataBox(CustomDataBox&& dataBox)
{
    DataBoxTID tid = GetTypeId<CustomDataBox>();

    if (providerList.size() <= tid || providerList[tid] == nullptr)
    {
        DataHub::Internal::PushHubReport(HubReport_Contract, L"[Hub] 데이터박스 타입 미등록 (Init 시점에 등록 필요)");
        return 0;
    }

    DataBoxProvider<CustomDataBox>* provider = static_cast<DataBoxProvider<CustomDataBox>*>(providerList[tid].get());
    std::vector<CustomDataBox>& list = provider->dataBoxList;
    std::vector<DataBoxID>& blk = provider->blankedIdList;

    DataBoxID dataId;

    if (blk.size() > 0)
    {
        dataId = blk.back();
        list[dataId] = std::forward<CustomDataBox>(dataBox);
        blk.pop_back();
    }
    else
    {
        dataId = list.size();
        list.push_back(std::forward<CustomDataBox>(dataBox));
    }

    provider->data = provider->dataBoxList.data();
    provider->size = provider->dataBoxList.size();

    return dataId;
}

template <std::derived_from<DataBox_Custom> CustomDataBox>
void DataBoxCustomizer::RemoveDataBox(DataBoxID dataId)
{
    RemoveDataBox(GetTypeId<CustomDataBox>(), dataId);
}

inline void DataBoxCustomizer::RemoveDataBox(DataBoxTID tid, DataBoxID dataId)
{
    if (providerList.size() <= tid || providerList[tid] == nullptr)
    {
        DataHub::Internal::PushHubReport(HubReport_Contract, L"[Hub] 데이터박스 타입 미등록 (Init 시점에 등록 필요)");
        return;
    }

    if (dataId == 0 || dataId >= providerList[tid]->size)
        return;

    providerList[tid]->blankedIdList.push_back(dataId);
}

template <std::derived_from<DataBox_Custom> CustomDataBox>
std::vector<CustomDataBox>& DataBoxCustomizer::GetDataBoxList()
{
    DataBoxTID tid = GetTypeId<CustomDataBox>();

    if (providerList.size() <= tid || providerList[tid] == nullptr)
    {
        DataHub::Internal::PushHubReport(HubReport_Contract, L"[Hub] 데이터박스 타입 미등록 (Init 시점에 등록 필요)");
        static std::vector<CustomDataBox> unregistered;
        unregistered.clear();
        return unregistered;
    }
    
    DataBoxProvider<CustomDataBox>* provider = static_cast<DataBoxProvider<CustomDataBox>*>(providerList[tid].get());
    return provider->dataBoxList;
}

inline void DataBoxCustomizer::ClearAll()
{
    for (size_t i = 0; i < providerList.size(); i++)
    {
        if (providerList[i] != nullptr)
            providerList[i]->ClearData();
    }
}


inline IDtBProvider* DataBoxCustomizer::GetProvider(DataBoxTID tid)
{
    if (providerList.size() <= tid)
        return nullptr;

    return providerList[tid].get();
}

inline DataBoxTID DataBoxCustomizer::GetProviderCount()
{
    return (DataBoxTID)providerList.size();
}

}
