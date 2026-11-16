#pragma once

#include "Component/Component.h"

#include <concepts>
#include <memory>
#include <string>
#include <vector>

class World;

class GameObject
{
	// 생성과 소멸
public:
	/**
	 * TransformComponent를 기본 부착한다.
	 * 월드는 이 객체보다 오래 살아 있어야 한다. 생성과 등록은 별개다.
	 */
	explicit GameObject(World& targetWorld);
	virtual ~GameObject();

	GameObject(const GameObject&) = delete;
	GameObject& operator=(const GameObject&) = delete;
	GameObject(GameObject&&) = delete;
	GameObject& operator=(GameObject&&) = delete;

	// 이름
public:
	/** 객체의 수명 동안 표시용 이름을 빌려준다. */
	const std::string& GetName() const;

	/** 표시용 이름을 바꾼다. 다른 객체와 같은 이름을 쓸 수 있다. */
	void SetName(std::string newName);

private:
	std::string name = "GameObject";

	// 소속 월드
public:
	World& GetWorld();
	const World& GetWorld() const;

private:
	World& world;

	// 삭제 상태
public:
	/** 이 객체의 삭제 요청이 접수되었으면 true를 반환한다. */
	bool IsPendingDestroy() const { return isPendingDestroy; }

	/** 소속 World가 삭제 요청을 등록할 때 호출한다. */
	void SetPendingDestroy(bool pendingDestroy);

private:
	bool isPendingDestroy = false;

	// 부모와 자식
public:
	/**
	 * 같은 World에 등록된 객체끼리 연결한다. nullptr는 연결 해제다.
	 * 성공하면 true를 반환한다. 등록 조건 위반이나 순환은 기존 관계를 유지하고 false를 반환한다.
	 * 이 객체나 새 부모가 삭제 예정이면 새 연결은 거절한다. 연결을 해제해도 삭제 요청은 유지된다.
	 * 할당 실패 시 기존 관계를 유지하고 예외를 전파한다.
	 */
	bool SetParent(GameObject* newParent);

	/** 부모를 빌려준다. 부모가 없으면 nullptr를 반환한다. */
	GameObject* GetParent() { return parent; }
	const GameObject* GetParent() const { return parent; }

	/**
	 * 직계 자식 목록을 빌려준다. 목록이 바뀌면 원소 참조와 반복자가 무효화될 수 있다.
	 * 기존 호출과의 호환을 위해 const 객체에서도 자식 포인터는 수정 가능한 객체를 가리킨다.
	 */
	const std::vector<GameObject*>& GetChildren() const { return children; }

private:
	// World가 소유한 객체를 가리키는 비소유 참조다.
	GameObject* parent = nullptr;
	std::vector<GameObject*> children;

	// 컴포넌트
public:
	/**
	 * 소유권을 받아 등록하고 이 객체를 소속으로 설정한 뒤 컴포넌트의 OnRegister를 호출한다.
	 * 반환 포인터는 이 객체의 수명 동안만 빌려 쓴다.
	 * 빈 입력은 nullptr를 반환한다.
	 */
	Component* AddComponent(std::unique_ptr<Component> newComponent);

	/**
	 * 붙은 컴포넌트를 추가 순서대로 확인해 T 타입인 첫 번째를 빌려준다. 없으면 nullptr이다.
	 * 반환 포인터는 이 객체의 수명 동안만 유효하다.
	 */
	template <typename T>
		requires std::derived_from<T, Component>
	T* GetComponent();

	template <typename T>
		requires std::derived_from<T, Component>
	const T* GetComponent() const;

	/**
	 * 기본 TransformComponent를 포함하여 부착 순서대로 각 컴포넌트의 비소유 참조를 visitor에 한 번씩 전달한다.
	 * 순회 중 이 객체의 컴포넌트 목록에 추가하거나 목록의 구조를 변경하지 않는다.
	 * visitor가 던진 예외는 전파한다.
	 */
	template <typename Visitor>
	void ForEachComponent(Visitor&& visitor);

	/** 같은 순회 계약을 적용하며 visitor에는 const Component&를 전달한다. */
	template <typename Visitor>
	void ForEachComponent(Visitor&& visitor) const;

private:
	std::vector<std::unique_ptr<Component>> components;

	// 갱신
public:
	/** 월드가 프레임마다 호출한다. deltaTime는 초 단위다. */
	void Tick(float deltaTime);

protected:
	/** 파생 타입의 갱신 지점. 기반 클래스의 OnTick을 호출할 필요는 없다. */
	virtual void OnTick(float deltaTime);
};

template <typename T>
	requires std::derived_from<T, Component>
T* GameObject::GetComponent()
{
	for (const std::unique_ptr<Component>& component : components)
	{
		if (T* found = dynamic_cast<T*>(component.get()))
		{
			return found;
		}
	}
	return nullptr;
}

template <typename T>
	requires std::derived_from<T, Component>
const T* GameObject::GetComponent() const
{
	for (const std::unique_ptr<Component>& component : components)
	{
		if (const T* found = dynamic_cast<const T*>(component.get()))
		{
			return found;
		}
	}
	return nullptr;
}

template <typename Visitor>
void GameObject::ForEachComponent(Visitor&& visitor)
{
	for (const std::unique_ptr<Component>& component : components)
	{
		visitor(*component);
	}
}

template <typename Visitor>
void GameObject::ForEachComponent(Visitor&& visitor) const
{
	for (const std::unique_ptr<Component>& component : components)
	{
		const Component& readOnlyComponent = *component;
		visitor(readOnlyComponent);
	}
}
