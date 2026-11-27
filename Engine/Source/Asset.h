#pragma once

#include <cstdint>
#include <string>

/** 메시, 텍스처, 사운드 같은 엔진 콘텐츠의 공통 이름과 경로를 보관한다. */
class Asset
{
public:
	/**
	 * UTF-8 이름과 저장소 루트 기준 상대 경로를 보관한다. 이름은 중복될 수 있다.
	 * 파일 없이 생성한 에셋은 빈 경로를 사용한다. 경로 정규화와 파일 로딩은 호출하는 쪽에서 처리한다.
	 * 파일 안의 일부인 에셋은 "파일경로|번호" 또는 "파일경로|default" 모양이라 실제 파일 경로가 아니다(Paths::SplitSubAssetPath).
	 * 코드가 합성한 텍스처(ORM, 발광)도 파일에서 다시 만들 수 없어 빈 경로를 쓴다.
	 */
	Asset(std::string newName, std::string newPath);

	/** 기반 포인터로 삭제할 때 파생 클래스도 소멸시킨다. */
	virtual ~Asset() = default;

	// 이름과 경로는 정체성이라 복사하지 않는다. 이동한 원본 객체는 더 사용하지 않는다.
	Asset(const Asset&) = delete;
	Asset& operator=(const Asset&) = delete;
	Asset(Asset&&) = default;
	Asset& operator=(Asset&&) = default;

	/** 표시용 이름을 빌려준다. 반환한 참조는 이 객체가 살아 있는 동안 사용한다. */
	const std::string& GetName() const;

	/** 파일 없는 에셋은 빈 경로다. 반환한 참조는 이 객체가 살아 있는 동안 사용한다. */
	const std::string& GetPath() const;

	/**
	 * 이 객체를 만들 때 받은 번호다. 실행 중에는 객체마다 다르고 다시 쓰이지 않으며 0이 아니다.
	 * 실행할 때마다 달라지므로 파일에 저장하지 않는다. 렌더러 리소스 이름처럼 실행 중에 에셋 객체를 구분할 때 쓴다.
	 */
	std::uint64_t GetInstanceID() const;

private:
	std::string name;
	std::string path;
	// 생성자에서 정한다. 이동하면 새 객체로 옮겨지고, 이동한 원본은 더 사용하지 않는다.
	std::uint64_t instanceID;
};
