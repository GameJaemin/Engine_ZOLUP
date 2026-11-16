#pragma once

#include "DataHub_Types.h"

class StaticMesh;
struct Material;

namespace HakkoResources
{
	/**
	 * DataHub 초기화 후 호출한다. 같은 에셋 경로로 등록한 메시를 재사용한다.
	 * 빈 경로나 등록 실패는 false를 반환한다. 새 메시는 같은 프레임에 렌더러가 업로드 요청을 처리해야 한다.
	 * 경로 변환과 할당 실패 예외는 전파한다.
	 */
	bool PrepareStaticMesh(const StaticMesh& mesh);

	/** DataHub에서 메시 경로로 ID를 조회한다. 미등록이면 0을 반환한다. 경로 변환과 할당 실패 예외는 전파한다. */
	Hakko::DataHub::MeshID FindStaticMeshId(const StaticMesh& mesh);

	/**
	 * DataHub 초기화 후 메인 스레드의 렌더링 준비 중 재질의 텍스처 조합을 준비한다.
	 * 재질이 nullptr이면 오류용 조합 0을 출력하고 true를 반환한다.
	 * 지정된 텍스처의 경로 키와 형식 및 슬롯별 기본값을 기준으로 조합을 재사용한다.
	 * 색상 또는 ORM 텍스처가 없으면 흰색을, 노멀 텍스처가 없으면 기본 노멀을 쓰며, 발광 슬롯은 검정으로 고정한다.
	 * 새 조합은 같은 프레임에 렌더러가 업로드 요청을 처리해야 한다. 성공 시 ID를 출력하며 GPU 준비 완료를 뜻하지는 않는다.
	 * 텍스처 준비나 조합 등록에 실패하면 false를 반환하고 outTexturePropID는 0으로 유지한다.
	 * 경로 변환과 할당 실패 예외는 전파한다. 실패 전에 등록한 텍스처는 유지한다.
	 */
	bool PrepareTextureProp(const Material* material, Hakko::DataHub::TexPropID& outTexturePropID);
}
