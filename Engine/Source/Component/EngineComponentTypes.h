#pragma once

class ComponentTypeRegistry;

/** 엔진 컴포넌트 타입을 모두 등록한다. Engine::Initialize가 한 번 호출하며, 하나라도 실패하면 false를 반환한다. */
bool RegisterEngineComponentTypes(ComponentTypeRegistry& registry);
