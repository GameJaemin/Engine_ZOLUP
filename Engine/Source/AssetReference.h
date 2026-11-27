#pragma once

#include "Asset.h"

#include <memory>
#include <type_traits>
#include <typeindex>
#include <typeinfo>
#include <utility>

/**
 * 에셋 하나를 가리키는 실행 중 참조다. type은 필드가 선언한 에셋 타입이며, asset이 nullptr면 지정하지 않은 상태다.
 * asset이 있으면 그 객체는 type 타입이거나 그 파생 타입이다. 생성자가 이 관계를 보장하므로 사용하는 쪽은 type만 보고 변환해도 된다.
 */
class AssetReference
{
public:
	/** AssetType 타입의 참조를 만든다. type은 AssetType이며, newAsset이 nullptr면 그 타입의 지정 안 한 참조다. */
	template <typename AssetType>
	explicit AssetReference(std::shared_ptr<const AssetType> newAsset)
		: type(typeid(AssetType)), asset(std::move(newAsset))
	{
		static_assert(std::is_base_of_v<Asset, AssetType>, "AssetReference can only refer to Asset types.");
	}

	/** newType 타입의 지정 안 한 참조를 만든다. 저장한 빈 참조를 되돌릴 때처럼 타입 정보만 있을 때 쓴다. */
	explicit AssetReference(std::type_index newType) : type(newType) {}

	std::type_index GetType() const { return type; }
	const std::shared_ptr<const Asset>& GetAsset() const { return asset; }

private:
	std::type_index type;
	std::shared_ptr<const Asset> asset;
};
