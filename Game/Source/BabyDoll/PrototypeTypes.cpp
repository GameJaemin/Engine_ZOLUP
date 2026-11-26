#include "PrototypeTypes.h"

#include "Core/Log.h"
#include "Paths.h"

#include <rapidjson/document.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

bool LoadPrototypeConfig(PrototypeConfig& config)
{
	std::filesystem::path path;
	if (!Paths::GetAbsolutePath("Game/Content/BabyDoll/Config/Prototype.json", path)) { return false; }
	std::ifstream file(path);
	if (!file) { return false; }
	const std::string json((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
	rapidjson::Document document;
	document.Parse(json.c_str());
	if (document.HasParseError() || !document.IsObject()) { return false; }
	PrototypeConfig candidate = config;
	struct ConfigField { const char* name; float* value; float minimum; float maximum; };
	const ConfigField fields[]{
		{ "maxSpeed", &candidate.maxSpeed, 1.0f, 2000.0f },
		{ "accelerationTime", &candidate.accelerationTime, 0.1f, 20.0f },
		{ "rhythmWindow", &candidate.rhythmWindow, 0.1f, 3.0f },
		{ "fastInterval", &candidate.fastInterval, 0.05f, 1.0f },
		{ "momentumDecay", &candidate.momentumDecay, 0.01f, 10.0f },
		{ "playerRadius", &candidate.playerRadius, 5.0f, 50.0f },
		{ "playerHeight", &candidate.playerHeight, 60.0f, 150.0f },
		{ "fallenTime", &candidate.fallenTime, 0.1f, 10.0f },
		{ "patrolSpeed", &candidate.patrolSpeed, 1.0f, 1000.0f },
		{ "chaseSpeed", &candidate.chaseSpeed, 1.0f, 1500.0f },
		{ "suspiciousSpeed", &candidate.suspiciousSpeed, 1.0f, 1500.0f },
		{ "monsterAcceleration", &candidate.monsterAcceleration, 50.0f, 5000.0f },
		{ "monsterBraking", &candidate.monsterBraking, 50.0f, 5000.0f },
		{ "monsterTurnRate", &candidate.monsterTurnRate, 30.0f, 720.0f },
		{ "monsterTurnAcceleration", &candidate.monsterTurnAcceleration, 60.0f, 4000.0f },
		{ "monsterLookAhead", &candidate.monsterLookAhead, 50.0f, 400.0f },
		{ "monsterVisualYawOffset", &candidate.monsterVisualYawOffset, -180.0f, 180.0f },
		{ "catchDistance", &candidate.catchDistance, 1.0f, 300.0f },
		{ "calmTime", &candidate.calmTime, 0.1f, 20.0f },
		{ "mouseSensitivity", &candidate.mouseSensitivity, 0.01f, 1.0f },
		{ "cameraDistance", &candidate.cameraDistance, 100.0f, 1200.0f },
		{ "runningCameraDistance", &candidate.runningCameraDistance, 100.0f, 1400.0f },
		{ "cameraFov", &candidate.cameraFov, 35.0f, 110.0f },
		{ "runningCameraFov", &candidate.runningCameraFov, 35.0f, 120.0f },
		{ "cameraFollow", &candidate.cameraFollow, 1.0f, 60.0f },
		{ "cameraLookResponse", &candidate.cameraLookResponse, 1.0f, 100.0f },
		{ "rearWidth", &candidate.rearWidth, 64.0f, 2048.0f },
		{ "rearHeight", &candidate.rearHeight, 64.0f, 2048.0f },
		{ "rearSaturation", &candidate.rearSaturation, 0.0f, 2.0f },
		{ "rearHold", &candidate.rearHold, 0.0f, 30.0f },
		{ "rearFarDistance", &candidate.rearFarDistance, 100.0f, 5000.0f },
		{ "rearNearDistance", &candidate.rearNearDistance, 50.0f, 3000.0f },
		{ "rearCameraDistance", &candidate.rearCameraDistance, 50.0f, 1500.0f },
		{ "rearClosestDistance", &candidate.rearClosestDistance, 50.0f, 1500.0f },
		{ "rearCameraHeight", &candidate.rearCameraHeight, 0.0f, 400.0f },
		{ "rearAimHeight", &candidate.rearAimHeight, 0.0f, 300.0f },
		{ "rearHalfWidth", &candidate.rearHalfWidth, 10.0f, 600.0f },
		{ "rearHalfHeight", &candidate.rearHalfHeight, 10.0f, 600.0f },
		{ "rearCloseFill", &candidate.rearCloseFill, 0.1f, 1.0f },
		{ "rearFollow", &candidate.rearFollow, 1.0f, 60.0f },
		{ "lampDarkRadius", &candidate.lampDarkRadius, 50.0f, 5000.0f },
		{ "lampFlickerTime", &candidate.lampFlickerTime, 0.05f, 5.0f },
		{ "lampIntensity", &candidate.lampIntensity, 0.0f, 20.0f },
		{ "lampRange", &candidate.lampRange, 100.0f, 4000.0f },
		{ "propDropHeight", &candidate.propDropHeight, 150.0f, 500.0f }
	};
	for (const ConfigField& field : fields)
	{
		if (!document.HasMember(field.name)) { continue; }
		const rapidjson::Value& value = document[field.name];
		if (!value.IsNumber()) { return false; }
		const float number = value.GetFloat();
		if (!std::isfinite(number) || number < field.minimum || number > field.maximum) { return false; }
		*field.value = number;
	}
	if (candidate.fastInterval >= candidate.rhythmWindow || candidate.playerHeight < candidate.playerRadius * 2.0f) { return false; }
	if (candidate.rearNearDistance >= candidate.rearFarDistance || candidate.rearClosestDistance > candidate.rearCameraDistance) { return false; }
	if (std::floor(candidate.rearWidth) != candidate.rearWidth || std::floor(candidate.rearHeight) != candidate.rearHeight) { return false; }
	config = candidate;
	Log::Write(LogLevel::Info, "BabyDoll", "Loaded prototype tuning");
	return true;
}
