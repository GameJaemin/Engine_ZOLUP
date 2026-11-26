#include "GLTF/GLTFLoader.h"

#include <cmath>
#include <cstdio>
#include <filesystem>

/** 실제 엔진 로더로 도구의 결과를 검사한다. 엔진/렌더러 실행 파일에는 포함하지 않는다. */
int wmain(int argumentCount, wchar_t** arguments)
{
	if (argumentCount < 2) { return 2; }
	for (int argument = 1; argument < argumentCount; ++argument)
	{
		GLTFModel model;
		if (!GLTFLoader::LoadModel(std::filesystem::path(arguments[argument]), model)) { return 1; }
		std::size_t vertexCount = 0;
		for (const StaticMesh& mesh : model.meshes)
		{
			vertexCount += mesh.vertices.size();
		}
		for (const AnimationClip& clip : model.animations)
		{
			if (clip.tracks.empty() || clip.duration <= 0.0f) { return 1; }
			for (const AnimationNodeTrack& track : clip.tracks)
			{
				const Transform pose = track.Sample(clip.duration * 0.5f);
				if (!std::isfinite(pose.position.x) || !std::isfinite(pose.rotation.w)) { return 1; }
			}
			std::printf("CLIP %s tracks=%zu duration=%.3f\n", clip.GetName().c_str(), clip.tracks.size(), clip.duration);
		}
		std::printf("PASS meshes=%zu vertices=%zu skins=%zu clips=%zu\n", model.meshes.size(), vertexCount, model.skins.size(), model.animations.size());
	}
	return 0;
}
