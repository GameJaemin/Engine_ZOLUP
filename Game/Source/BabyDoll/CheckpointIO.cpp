#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "CheckpointIO.h"
#include "BabyDollApplication.h"
#include <rapidjson/document.h>
#include <rapidjson/writer.h>
#include <rapidjson/stringbuffer.h>
#include <Windows.h>
#include <fstream>
#include <iterator>

bool WritePrototypeCheckpoint(const std::filesystem::path& path, const PrototypeCheckpoint& state)
{
	rapidjson::StringBuffer buffer;
	rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
	const auto array = [&](const char* key, std::initializer_list<double> values)
	{
		writer.Key(key); writer.StartArray(); for (double value : values) { writer.Double(value); } writer.EndArray();
	};
	writer.StartObject(); writer.Key("version"); writer.Int(2);
	writer.Key("layout"); writer.String("babydoll-graybox-1");
	array("player",{state.playerPosition.x,state.playerPosition.y,state.playerPosition.z,static_cast<double>(state.playerLastFoot),static_cast<double>(state.progress.stage)});
	array("hands",{static_cast<double>(state.hands[0]),static_cast<double>(state.hands[1])});
	array("door",{state.door.locked?1.0:0.0,state.door.targetOpen?1.0:0.0,state.door.opening});
	array("heart",{state.heart.bpm,state.heart.sprintTime,state.heart.calmingTime,state.heart.calmingStart,state.heart.wasCalming?1.0:0.0});
	const auto motion=state.monster.movement.CaptureMotion();
	array("motion",{motion.position.x,motion.position.y,motion.position.z,motion.yaw,motion.turnVelocity,motion.linearSpeed,motion.actualSpeed});
	const auto& ai=state.monster;
	array("ai",{static_cast<double>(ai.state),static_cast<double>(ai.action),static_cast<double>(ai.patrolIndex),ai.stateTime,ai.actionTime,ai.hiddenTime,ai.unreachableTime,ai.resumeAfterRoar?1.0:0.0,ai.destination.x,ai.destination.y,ai.destination.z});
	writer.Key("sequence"); writer.Uint64(state.soundSequence);
	if (ai.deferredSound)
	{
		const auto& s=*ai.deferredSound;
		array("deferred",{s.position.x,s.position.y,s.position.z,s.radius,static_cast<double>(s.kind),static_cast<double>(s.floor),s.player?1.0:0.0,static_cast<double>(s.sequence),s.crossFloor?1.0:0.0,s.ignoreRadius?1.0:0.0,static_cast<double>(s.priority),s.reaction?static_cast<double>(*s.reaction):-1.0});
	}
	writer.Key("tables"); writer.StartArray();
	for (const auto& table:state.tables) { writer.StartArray(); writer.Int(table.remainingUses); writer.Bool(table.broken); writer.EndArray(); }
	writer.EndArray(); writer.Key("props"); writer.StartArray();
	for (const auto& prop:state.props)
	{
		writer.StartArray();
		for (float v:{prop.position.x,prop.position.y,prop.position.z,prop.rotation.x,prop.rotation.y,prop.rotation.z,prop.rotation.w,prop.scale.x,prop.scale.y,prop.scale.z}) { writer.Double(v); }
		writer.EndArray();
	}
	writer.EndArray(); writer.EndObject();
	std::error_code error;
	std::filesystem::create_directories(path.parent_path(),error);
	if (error) { return false; }
	const std::filesystem::path temporary=path.wstring()+L".tmp";
	std::ofstream output(temporary,std::ios::binary|std::ios::trunc);
	if (!output) { return false; }
	output.write(buffer.GetString(),static_cast<std::streamsize>(buffer.GetSize())); output.close();
	if (!output) { return false; }
	return MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
}

bool ReadPrototypeCheckpoint(const std::filesystem::path& path, PrototypeCheckpoint& output)
{
	std::error_code error;
	const auto size=std::filesystem::file_size(path,error);
	if (error || size>1024*1024) { return false; }
	std::ifstream input(path,std::ios::binary);
	const std::string json((std::istreambuf_iterator<char>(input)),{});
	rapidjson::Document d; d.Parse(json.c_str());
	if (d.HasParseError() || !d.IsObject() || !d.HasMember("version") || !d["version"].IsInt() || d["version"].GetInt()!=2
		|| !d.HasMember("layout") || !d["layout"].IsString() || std::string_view(d["layout"].GetString())!="babydoll-graybox-1") { return false; }
	const auto valid = [&](const char* name, unsigned count)
	{
		if (!d.HasMember(name) || !d[name].IsArray() || d[name].Size()!=count) { return false; }
		for (const auto& n:d[name].GetArray()) { if (!n.IsNumber() || !std::isfinite(n.GetDouble()) || std::abs(n.GetDouble())>10000000.0) { return false; } }
		return true;
	};
	if (!valid("player",5)||!valid("hands",2)||!valid("door",3)||!valid("heart",5)||!valid("motion",7)||!valid("ai",11)) { return false; }
	const auto integer=[](const rapidjson::Value& v,int low,int high) { const double n=v.GetDouble(); return n>=low&&n<=high&&std::floor(n)==n; };
	if (!integer(d["player"][3],-1,1)||!integer(d["player"][4],0,2)||!integer(d["hands"][0],0,2)||!integer(d["hands"][1],0,2)
		|| !integer(d["door"][0],0,1)||!integer(d["door"][1],0,1)||d["door"][2].GetDouble()<0||d["door"][2].GetDouble()>1
		||!integer(d["ai"][0],0,2)||!integer(d["ai"][1],0,5)||!integer(d["ai"][2],0,3)||!integer(d["ai"][7],0,1)
		||d["heart"][0].GetDouble()<1||d["heart"][0].GetDouble()>300||!integer(d["heart"][4],0,1)
		||!d.HasMember("sequence")||!d["sequence"].IsUint64()) { return false; }
	PrototypeCheckpoint state;
	const auto vector=[](const rapidjson::Value& a,unsigned offset=0) { return Vector3{a[offset].GetFloat(),a[offset+1].GetFloat(),a[offset+2].GetFloat()}; };
	state.playerPosition=vector(d["player"]); state.playerLastFoot=static_cast<int>(d["player"][3].GetDouble()); state.progress.stage=static_cast<EscapeStage>(static_cast<int>(d["player"][4].GetDouble()));
	state.hands={static_cast<int>(d["hands"][0].GetDouble()),static_cast<int>(d["hands"][1].GetDouble())};
	if (state.hands[0]!=0 && state.hands[0]==state.hands[1]) { return false; }
	state.door={d["door"][0].GetDouble()!=0,d["door"][1].GetDouble()!=0,d["door"][2].GetFloat()};
	state.heart={d["heart"][0].GetFloat(),d["heart"][1].GetFloat(),d["heart"][2].GetFloat(),d["heart"][3].GetFloat(),d["heart"][4].GetDouble()!=0};
	state.monster.movement.RestoreMotion({vector(d["motion"]),d["motion"][3].GetFloat(),d["motion"][4].GetFloat(),d["motion"][5].GetFloat(),d["motion"][6].GetFloat()});
	state.monsterPosition=state.monster.movement.GetPosition();
	state.monster.state=static_cast<StitchState>(static_cast<int>(d["ai"][0].GetDouble())); state.monster.action=static_cast<StitchAction>(static_cast<int>(d["ai"][1].GetDouble()));
	state.monster.patrolIndex=static_cast<int>(d["ai"][2].GetDouble()); state.monster.stateTime=d["ai"][3].GetFloat(); state.monster.actionTime=d["ai"][4].GetFloat();
	state.monster.hiddenTime=d["ai"][5].GetFloat(); state.monster.unreachableTime=d["ai"][6].GetFloat(); state.monster.resumeAfterRoar=d["ai"][7].GetDouble()!=0; state.monster.destination=vector(d["ai"],8);
	state.soundSequence=d["sequence"].GetUint64();
	if (d.HasMember("deferred"))
	{
		if (!valid("deferred",12)||!integer(d["deferred"][4],0,3)||!integer(d["deferred"][5],-10,10)||!integer(d["deferred"][6],0,1)||!integer(d["deferred"][8],0,1)||!integer(d["deferred"][9],0,1)||!integer(d["deferred"][11],-1,2)||d["deferred"][7].GetDouble()<0) { return false; }
		const auto& a=d["deferred"];
		SoundStimulus s{vector(a),a[3].GetFloat(),static_cast<SoundKind>(static_cast<int>(a[4].GetDouble())),static_cast<int>(a[5].GetDouble()),a[6].GetDouble()!=0,static_cast<std::uint64_t>(a[7].GetDouble()),a[8].GetDouble()!=0,a[9].GetDouble()!=0,static_cast<int>(a[10].GetDouble())};
		if (a[11].GetDouble()>=0) { s.reaction=static_cast<StitchState>(static_cast<int>(a[11].GetDouble())); }
		state.monster.deferredSound=s;
	}
	if (!d.HasMember("tables")||!d["tables"].IsArray()||d["tables"].Size()!=2||!d.HasMember("props")||!d["props"].IsArray()||d["props"].Size()>64) { return false; }
	for (unsigned index=0;index<2;++index)
	{
		const auto& row=d["tables"][index];
		if (!row.IsArray()||row.Size()!=2||!row[0].IsInt()||row[0].GetInt()<0||row[0].GetInt()>1||!row[1].IsBool()) { return false; }
		state.tables[index]={row[0].GetInt(),row[1].GetBool()};
	}
	for (const auto& row:d["props"].GetArray())
	{
		if (!row.IsArray()||row.Size()!=10) { return false; }
		for (const auto& n:row.GetArray()) { if (!n.IsNumber()||!std::isfinite(n.GetDouble())||std::abs(n.GetDouble())>100000.0) { return false; } }
		Transform t; t.position=vector(row); t.rotation={row[3].GetFloat(),row[4].GetFloat(),row[5].GetFloat(),row[6].GetFloat()}; t.scale=vector(row,7);
		const float norm=t.rotation.x*t.rotation.x+t.rotation.y*t.rotation.y+t.rotation.z*t.rotation.z+t.rotation.w*t.rotation.w;
		if (std::abs(norm-1.0f)>0.001f||t.scale.x<=0||t.scale.y<=0||t.scale.z<=0) { return false; }
		state.props.push_back(t);
	}
	state.valid=true; output=std::move(state); return true;
}
