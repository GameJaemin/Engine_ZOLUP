#include "ObjectSoundComponent.h"
#include "Paths.h"
#include <rapidjson/document.h>
#include <fstream>
#include <iterator>

bool ObjectSoundComponent::Load()
{
	std::filesystem::path path;
	if (!Paths::GetAbsolutePath("Game/Content/BabyDoll/Config/ObjectSounds.json", path)) { return false; }
	std::ifstream file(path);
	if (!file) { return false; }
	const std::string json((std::istreambuf_iterator<char>(file)), {});
	rapidjson::Document document;
	document.Parse(json.c_str());
	if (document.HasParseError() || !document.IsArray()) { return false; }
	std::vector<ObjectSoundRule> candidate;
	for (const auto& row : document.GetArray())
	{
		if (!row.IsObject()) { return false; }
		ObjectSoundRule rule;
		const auto text = [&](const char* key, std::string& output)
		{
			if (!row.HasMember(key) || !row[key].IsString()) { return false; }
			output = row[key].GetString(); return true;
		};
		std::string reaction;
		if (!text("ObjectId", rule.objectID) || !text("SoundEvent", rule.event) || !text("StitchReaction", reaction) || !text("SfxId", rule.sfxID)) { return false; }
		if (reaction != "Chase" && reaction != "Suspicious" && reaction != "Ignore") { return false; }
		rule.reaction = reaction == "Chase" ? StitchState::Chase : StitchState::Suspicious;
		rule.ignore = reaction == "Ignore";
		for (auto field : { std::pair{ "Radius", &rule.radius }, { "MinImpactSpeed", &rule.minImpactSpeed }, { "Duration", &rule.duration }, { "Cooldown", &rule.cooldown } })
		{
			if (!row.HasMember(field.first) || !row[field.first].IsNumber()) { return false; }
			*field.second = row[field.first].GetFloat();
			if (!std::isfinite(*field.second) || *field.second < 0.0f || *field.second > 100000.0f) { return false; }
		}
		if (!row.HasMember("CrossFloor") || !row["CrossFloor"].IsBool() || !row.HasMember("IgnoreRadius") || !row["IgnoreRadius"].IsBool() || !row.HasMember("Priority") || !row["Priority"].IsInt()) { return false; }
		rule.crossFloor = row["CrossFloor"].GetBool(); rule.ignoreRadius = row["IgnoreRadius"].GetBool(); rule.priority = row["Priority"].GetInt();
		candidate.push_back(std::move(rule));
	}
	rules = std::move(candidate);
	Reset();
	return true;
}

void ObjectSoundComponent::Trigger(const std::string& instance, const std::string& kind, const std::string& event, const Vector3& position, float impactSpeed)
{
	const std::string key = instance + "/" + event;
	for (const Cooldown& cooldown : cooldowns) { if (cooldown.key == key && cooldown.remaining > 0.0f) { return; } }
	for (const auto& rule : rules)
	{
		const bool impact=event=="Step"||event=="Bump"||event=="Fall";
		if (rule.objectID != kind || rule.event != event || rule.ignore || (impact&&impactSpeed < rule.minImpactSpeed)) { continue; }
		active.push_back({ rule, position, (std::max)(rule.duration, 0.00001f) });
		cooldowns.push_back({ key, rule.cooldown });
	}
}

void ObjectSoundComponent::Step(float deltaTime, HearingComponent& hearing)
{
	for (auto& event : active)
	{
		hearing.Emit(event.position, event.rule.radius, SoundKind::Object, false, static_cast<int>(std::floor((event.position.y + 1.0f) / 640.0f)),
			event.rule.crossFloor, event.rule.ignoreRadius, event.rule.priority, event.rule.reaction);
		event.remaining -= deltaTime;
	}
	std::erase_if(active, [](const Active& value) { return value.remaining <= 0.0f; });
	for (auto& cooldown : cooldowns) { cooldown.remaining -= deltaTime; }
	std::erase_if(cooldowns, [](const Cooldown& value) { return value.remaining <= 0.0f; });
}
