#include "AnimationClip.h"

#include <algorithm>
#include <utility>

Transform AnimationNodeTrack::Sample(float time) const
{
	// NaN은 모든 비교가 거짓이라 이 조건에 걸려 첫 키 값을 쓴다.
	if (!(time > times.front()))
	{
		return values.front();
	}
	if (time >= times.back())
	{
		return values.back();
	}

	// time보다 큰 첫 키가 뒤 키다. 위 검사 덕분에 뒤 키는 항상 두 번째 키부터 마지막 키 사이에 있다.
	const auto nextTime = std::upper_bound(times.begin(), times.end(), time);
	const std::size_t next = static_cast<std::size_t>(nextTime - times.begin());
	const std::size_t prev = next - 1;

	// Step은 다음 키 시각 전까지 앞 키 값을 유지한다.
	if (interpolation == AnimationInterpolation::Step)
	{
		return values[prev];
	}

	// Linear는 구간 안 비율로 섞는다. 키 시각이 엄격히 커지므로 분모는 0이 아니다.
	const float alpha = (time - times[prev]) / (times[next] - times[prev]);

	Transform result;
	result.position = Vector3::Lerp(values[prev].position, values[next].position, alpha);
	result.rotation = Quaternion::Slerp(values[prev].rotation, values[next].rotation, alpha);
	result.scale = Vector3::Lerp(values[prev].scale, values[next].scale, alpha);
	return result;
}

AnimationClip::AnimationClip(std::string newName, std::string newPath)
	: Asset(std::move(newName), std::move(newPath))
{
}
