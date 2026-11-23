#include "Prefab/PrefabComponents.h"

#include "Component/AmbientLightComponent.h"
#include "Component/DirectionalLightComponent.h"
#include "Component/LightComponent.h"
#include "Component/PointLightComponent.h"
#include "Component/SpotLightComponent.h"
#include "Core/Log.h"
#include "Core/Math/Vector3.h"
#include "GameObject.h"

#include <array>
#include <format>
#include <memory>
#include <optional>
#include <string_view>
#include <utility>

namespace
{
	struct LightTypeName
	{
		PrefabLightType type;
		std::string_view name;
	};

	// 프리팹 JSON의 component type 이름이다.
	constexpr std::array<LightTypeName, 4> LightTypeNames{ {
		{ PrefabLightType::Directional, "directionalLight" },
		{ PrefabLightType::Point, "pointLight" },
		{ PrefabLightType::Spot, "spotLight" },
		{ PrefabLightType::Ambient, "ambientLight" },
	} };

	std::string_view GetLightTypeName(PrefabLightType type)
	{
		for (const LightTypeName& entry : LightTypeNames)
		{
			if (entry.type == type) { return entry.name; }
		}
		return "light";
	}

	bool LogLightError(std::string_view context, std::string_view message)
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: {}", context, message));
		return false;
	}
}

std::optional<PrefabLightType> FindPrefabLightType(std::string_view typeName)
{
	for (const LightTypeName& entry : LightTypeNames)
	{
		if (entry.name == typeName) { return entry.type; }
	}
	return std::nullopt;
}

bool ReadLightProperties(const rapidjson::Value& properties, std::string_view context, PrefabLightDefinition& light)
{
	const bool isSpotLight = light.type == PrefabLightType::Spot;
	const bool hasRange = isSpotLight || light.type == PrefabLightType::Point;
	const std::string_view typeName = GetLightTypeName(light.type);

	for (auto member = properties.MemberBegin(); member != properties.MemberEnd(); ++member)
	{
		const std::string_view name(member->name.GetString(), member->name.GetStringLength());
		if (name == "color" || name == "intensity" || name == "enabled") { continue; }
		if (hasRange && name == "rangeCm") { continue; }
		if (isSpotLight && name == "outerConeDegrees") { continue; }
		return LogLightError(context, std::format("{}: unsupported {} field", name, typeName));
	}

	constexpr std::string_view RangeError = "rangeCm must be a positive finite float";
	float rangeCm = 0.0f;
	if (hasRange)
	{
		const auto range = properties.FindMember("rangeCm");
		if (range == properties.MemberEnd() || !IsPrefabFiniteFloat(range->value)) { return LogLightError(context, RangeError); }
		rangeCm = range->value.GetFloat();
	}

	// 값의 허용 범위는 조명 컴포넌트 setter가 정한다. 검사용이라 객체에 붙이지 않아 RenderScene에 등록되지 않는다.
	std::unique_ptr<LightComponent> validator;
	float outerConeDegrees = 0.0f;
	if (light.type == PrefabLightType::Point)
	{
		std::unique_ptr<PointLightComponent> pointLight = std::make_unique<PointLightComponent>();
		if (!pointLight->SetRange(rangeCm)) { return LogLightError(context, RangeError); }
		validator = std::move(pointLight);
	}
	else if (isSpotLight)
	{
		constexpr std::string_view ConeError = "outerConeDegrees must be greater than 0 and at most 180";
		const auto outerCone = properties.FindMember("outerConeDegrees");
		if (outerCone == properties.MemberEnd() || !IsPrefabFiniteFloat(outerCone->value)) { return LogLightError(context, ConeError); }
		const double coneDegrees = outerCone->value.GetDouble();
		if (coneDegrees <= 0.0 || coneDegrees > 180.0) { return LogLightError(context, ConeError); }

		std::unique_ptr<SpotLightComponent> spotLight = std::make_unique<SpotLightComponent>();
		if (!spotLight->SetRange(rangeCm)) { return LogLightError(context, RangeError); }
		// JSON의 전체 원뿔 각도를 컴포넌트가 받는 반각으로 바꾼다.
		if (!spotLight->SetOuterConeAngleDegrees(static_cast<float>(coneDegrees * 0.5))) { return LogLightError(context, ConeError); }
		outerConeDegrees = static_cast<float>(coneDegrees);
		validator = std::move(spotLight);
	}
	else if (light.type == PrefabLightType::Ambient)
	{
		validator = std::make_unique<AmbientLightComponent>();
	}
	else
	{
		validator = std::make_unique<DirectionalLightComponent>();
	}

	const auto color = properties.FindMember("color");
	if (color != properties.MemberEnd())
	{
		constexpr std::string_view ColorError = "color must contain three non-negative finite floats";
		if (!color->value.IsArray() || color->value.Size() != 3) { return LogLightError(context, ColorError); }

		const rapidjson::Value::ConstArray rgb = color->value.GetArray();
		for (const rapidjson::Value& channel : rgb)
		{
			if (!IsPrefabFiniteFloat(channel) || channel.GetDouble() < 0.0) { return LogLightError(context, ColorError); }
		}
		if (!validator->SetColor(Vector3{ rgb[0].GetFloat(), rgb[1].GetFloat(), rgb[2].GetFloat() })) { return LogLightError(context, ColorError); }
	}

	const auto intensity = properties.FindMember("intensity");
	if (intensity != properties.MemberEnd())
	{
		if (!IsPrefabFiniteFloat(intensity->value) || intensity->value.GetDouble() < 0.0 || !validator->SetIntensity(intensity->value.GetFloat()))
		{
			return LogLightError(context, "intensity must be a non-negative finite float");
		}
	}

	const auto enabled = properties.FindMember("enabled");
	if (enabled != properties.MemberEnd())
	{
		if (!enabled->value.IsBool()) { return LogLightError(context, "enabled must be a boolean"); }
		validator->SetEnabled(enabled->value.GetBool());
	}

	// 생략한 값은 validator의 기본값이 남아 있다.
	light.color = validator->GetColor();
	light.intensity = validator->GetIntensity();
	light.enabled = validator->IsEnabled();
	light.rangeCm = rangeCm;
	light.outerConeDegrees = outerConeDegrees;
	return true;
}

void AttachPrefabLight(GameObject& object, const PrefabLightDefinition& light)
{
	// ReadLightProperties에서 같은 setter로 검사한 값이라 실패하지 않는다.
	std::unique_ptr<LightComponent> component;
	switch (light.type)
	{
	case PrefabLightType::Point:
	{
		std::unique_ptr<PointLightComponent> pointLight = std::make_unique<PointLightComponent>();
		pointLight->SetRange(light.rangeCm);
		component = std::move(pointLight);
		break;
	}
	case PrefabLightType::Spot:
	{
		std::unique_ptr<SpotLightComponent> spotLight = std::make_unique<SpotLightComponent>();
		spotLight->SetRange(light.rangeCm);
		spotLight->SetOuterConeAngleDegrees(light.outerConeDegrees * 0.5f);
		component = std::move(spotLight);
		break;
	}
	case PrefabLightType::Ambient:
		component = std::make_unique<AmbientLightComponent>();
		break;
	case PrefabLightType::Directional:
		component = std::make_unique<DirectionalLightComponent>();
		break;
	}

	component->SetColor(light.color);
	component->SetIntensity(light.intensity);
	component->SetEnabled(light.enabled);
	object.AddComponent(std::move(component));
}
