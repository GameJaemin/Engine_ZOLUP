#include "Prefab/PrefabComponents.h"

#include "Camera/CameraViewInfo.h"
#include "Component/CameraComponent.h"
#include "Core/Log.h"
#include "GameObject.h"

#include <format>
#include <memory>
#include <string_view>
#include <utility>

bool ReadCameraProperties(const rapidjson::Value& properties, std::string_view context, PrefabCameraDefinition& camera)
{
	const auto projection = properties.FindMember("projection");
	if (projection == properties.MemberEnd() || !projection->value.IsString())
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: projection must be perspective or orthographic", context));
		return false;
	}

	const std::string_view projectionName(projection->value.GetString(), projection->value.GetStringLength());
	const bool isPerspective = projectionName == "perspective";
	if (!isPerspective && projectionName != "orthographic")
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: projection must be perspective or orthographic", context));
		return false;
	}

	const char* projectionField = isPerspective ? "verticalFovDegrees" : "orthographicHeightCm";
	for (auto member = properties.MemberBegin(); member != properties.MemberEnd(); ++member)
	{
		const std::string_view name(member->name.GetString(), member->name.GetStringLength());
		if (name == "projection" || name == projectionField || name == "nearClipCm" || name == "farClipCm") { continue; }

		Log::Write(LogLevel::Error, "Prefab", std::format("{}: {}: unsupported camera field for {} projection", context, name, projectionName));
		return false;
	}

	// 값의 허용 범위는 CameraComponent setter가 정한다. 검사용이라 객체에 붙이지 않는다.
	CameraComponent validator;
	const auto projectionValue = properties.FindMember(projectionField);
	if (projectionValue == properties.MemberEnd() || !IsPrefabFiniteFloat(projectionValue->value))
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: {} must be a finite float", context, projectionField));
		return false;
	}

	const float projectionNumber = projectionValue->value.GetFloat();
	if (isPerspective)
	{
		if (!validator.SetVerticalFovDegrees(projectionNumber))
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: verticalFovDegrees must be greater than 0 and less than 180", context));
			return false;
		}
	}
	else if (!validator.SetOrthographicHeight(projectionNumber))
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: orthographicHeightCm must be positive", context));
		return false;
	}

	float nearClipCm = validator.GetNearClipDistance();
	const auto nearClip = properties.FindMember("nearClipCm");
	if (nearClip != properties.MemberEnd())
	{
		if (!IsPrefabFiniteFloat(nearClip->value) || nearClip->value.GetFloat() <= 0.0f)
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: nearClipCm must be a positive finite float", context));
			return false;
		}
		nearClipCm = nearClip->value.GetFloat();
	}

	float farClipCm = validator.GetFarClipDistance();
	const auto farClip = properties.FindMember("farClipCm");
	if (farClip != properties.MemberEnd())
	{
		if (!IsPrefabFiniteFloat(farClip->value))
		{
			Log::Write(LogLevel::Error, "Prefab", std::format("{}: farClipCm must be a finite float", context));
			return false;
		}
		farClipCm = farClip->value.GetFloat();
	}

	if (!validator.SetClipDistances(nearClipCm, farClipCm))
	{
		Log::Write(LogLevel::Error, "Prefab", std::format("{}: farClipCm must be greater than nearClipCm", context));
		return false;
	}

	camera.projection = isPerspective ? CameraProjectionMode::Perspective : CameraProjectionMode::Orthographic;
	camera.projectionValue = projectionNumber;
	camera.nearClipCm = nearClipCm;
	camera.farClipCm = farClipCm;
	return true;
}

void AttachPrefabCamera(GameObject& object, const PrefabCameraDefinition& camera)
{
	std::unique_ptr<CameraComponent> component = std::make_unique<CameraComponent>();
	component->SetProjectionMode(camera.projection);

	// ReadCameraProperties에서 같은 setter로 검사한 값이라 실패하지 않는다.
	if (camera.projection == CameraProjectionMode::Perspective)
	{
		component->SetVerticalFovDegrees(camera.projectionValue);
	}
	else
	{
		component->SetOrthographicHeight(camera.projectionValue);
	}
	component->SetClipDistances(camera.nearClipCm, camera.farClipCm);
	object.AddComponent(std::move(component));
}
