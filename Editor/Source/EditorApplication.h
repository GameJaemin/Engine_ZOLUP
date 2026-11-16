#pragma once

#include "EditorSelection.h"
#include "EditorWorldSession.h"
#include "IApplication.h"

#include <optional>

class EditorApplication final : public IApplication
{
public:
	bool OnInitialize() override;
	void OnUpdate(const InputState& input, float deltaTime) override;
	std::optional<SceneView> GetSceneView() override;
	void OnShutdown() noexcept override;

private:
	EditorWorldSession worldSession;
	EditorSelection selection;
};
