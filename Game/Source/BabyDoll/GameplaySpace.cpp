#include "GameplaySpace.h"

#include <limits>
#include <functional>
#include <queue>
#include <utility>

bool GameplaySpace::IsFree(const Vector3& position, float radius, float height) const
{
	if (std::abs(position.x) > halfWidth - radius || std::abs(position.z) > halfDepth - radius) { return false; }
	for (const PrototypeObstacle& obstacle : obstacles)
	{
		if (!obstacle.enabled || position.y + height <= obstacle.minimum.y || position.y >= obstacle.maximum.y) { continue; }
		const float dx = position.x - std::clamp(position.x, obstacle.minimum.x, obstacle.maximum.x);
		const float dz = position.z - std::clamp(position.z, obstacle.minimum.z, obstacle.maximum.z);
		if (dx * dx + dz * dz < radius * radius) { return false; }
	}
	return true;
}

bool GameplaySpace::CanTraverse(const Vector3& start, const Vector3& end, float radius, float height) const
{
	if (!IsFree(start, radius, height) || !IsFree(end, radius, height)) { return false; }
	for (const PrototypeObstacle& obstacle : obstacles)
	{
		if (!obstacle.enabled || start.y + height <= obstacle.minimum.y || start.y >= obstacle.maximum.y) { continue; }
		float nearTime = 0.0f;
		float farTime = 1.0f;
		const float origin[]{ start.x, start.z };
		const float delta[]{ end.x - start.x, end.z - start.z };
		const float minimum[]{ obstacle.minimum.x - radius, obstacle.minimum.z - radius };
		const float maximum[]{ obstacle.maximum.x + radius, obstacle.maximum.z + radius };
		bool hit = true;
		for (int axis = 0; axis < 2; ++axis)
		{
			if (std::abs(delta[axis]) < 0.00001f)
			{
				if (origin[axis] < minimum[axis] || origin[axis] > maximum[axis]) { hit = false; break; }
				continue;
			}
			float first = (minimum[axis] - origin[axis]) / delta[axis];
			float last = (maximum[axis] - origin[axis]) / delta[axis];
			if (first > last) { std::swap(first, last); }
			nearTime = (std::max)(nearTime, first);
			farTime = (std::min)(farTime, last);
			if (nearTime > farTime) { hit = false; break; }
		}
		if (hit) { return false; }
	}
	return true;
}

Vector3 GameplaySpace::Move(const Vector3& position, const Vector3& displacement, float radius, float height) const
{
	Vector3 result = position;
	const int count = (std::max)(1, static_cast<int>(std::ceil(PrototypeMath::Length(displacement) / (radius * 0.4f))));
	const Vector3 step = displacement / static_cast<float>(count);
	for (int index = 0; index < count; ++index)
	{
		const Vector3 desired = result + step;
		if (IsFree(desired, radius, height)) { result = desired; continue; }
		const Vector3 slideX{ desired.x, result.y, result.z };
		if (IsFree(slideX, radius, height)) { result = slideX; }
		const Vector3 slideZ{ result.x, result.y, desired.z };
		if (IsFree(slideZ, radius, height)) { result = slideZ; }
	}
	return result;
}

float GameplaySpace::CameraDistance(const Vector3& origin, const Vector3& direction, float distance, float radius) const
{
	float available = distance;
	for (const PrototypeObstacle& obstacle : obstacles)
	{
		if (!obstacle.enabled) { continue; }
		float nearTime = 0.0f;
		float farTime = available;
		const float origins[3]{ origin.x, origin.y, origin.z };
		const float directions[3]{ direction.x, direction.y, direction.z };
		const float minimum[3]{ obstacle.minimum.x - radius, obstacle.minimum.y - radius, obstacle.minimum.z - radius };
		const float maximum[3]{ obstacle.maximum.x + radius, obstacle.maximum.y + radius, obstacle.maximum.z + radius };
		bool hit = true;
		for (int axis = 0; axis < 3; ++axis)
		{
			if (std::abs(directions[axis]) < 0.00001f)
			{
				if (origins[axis] < minimum[axis] || origins[axis] > maximum[axis]) { hit = false; break; }
				continue;
			}
			float first = (minimum[axis] - origins[axis]) / directions[axis];
			float last = (maximum[axis] - origins[axis]) / directions[axis];
			if (first > last) { std::swap(first, last); }
			nearTime = (std::max)(nearTime, first);
			farTime = (std::min)(farTime, last);
			if (nearTime > farTime) { hit = false; break; }
		}
		if (hit && farTime >= 0.0f) { available = (std::min)(available, (std::max)(0.0f, nearTime - 4.0f)); }
	}
	return available;
}

std::vector<Vector3> GameplaySpace::FindPath(const Vector3& start, const Vector3& destination, float radius, float height) const
{
	constexpr float CellSize = 100.0f;
	const int width = static_cast<int>(2.0f * halfWidth / CellSize) + 1;
	const int depth = static_cast<int>(2.0f * halfDepth / CellSize) + 1;
	const int total = width * depth;
	const std::function<Vector3(int)> point = [&](int id) { return Vector3{ (id % width) * CellSize - halfWidth, 0.0f, (id / width) * CellSize - halfDepth }; };
	const std::function<int(const Vector3&)> cell = [&](const Vector3& position) { return std::clamp(static_cast<int>(std::round((position.z + halfDepth) / CellSize)), 0, depth - 1) * width + std::clamp(static_cast<int>(std::round((position.x + halfWidth) / CellSize)), 0, width - 1); };
	int first = cell(start);
	int last = cell(destination);
	// 목표가 테이블 아래라면 그 자리에 침투하지 않고 가장 가까운 접근 가능한 칸을 고른다.
	const std::function<int(int, const Vector3&)> nearestFree = [&](int candidate, const Vector3& desired)
	{
		if (IsFree(point(candidate), radius, height)) { return candidate; }
		float best = (std::numeric_limits<float>::max)();
		int selected = -1;
		for (int id = 0; id < total; ++id)
		{
			if (!IsFree(point(id), radius, height)) { continue; }
			const float distance = PrototypeMath::Distance(point(id), desired);
			if (distance < best) { best = distance; selected = id; }
		}
		return selected;
	};
	first = nearestFree(first, start);
	last = nearestFree(last, destination);
	if (first < 0 || last < 0) { return {}; }
	std::vector<float> costs(total, (std::numeric_limits<float>::max)());
	std::vector<int> parents(total, -1);
	std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, std::greater<std::pair<float, int>>> open;
	costs[first] = 0.0f;
	open.push({ 0.0f, first });
	while (!open.empty())
	{
		const int current = open.top().second;
		open.pop();
		if (current == last) { break; }
		const int cx = current % width;
		const int cz = current / width;
		for (int dz = -1; dz <= 1; ++dz)
		{
			for (int dx = -1; dx <= 1; ++dx)
			{
				if ((dx == 0 && dz == 0) || cx + dx < 0 || cx + dx >= width || cz + dz < 0 || cz + dz >= depth) { continue; }
				const int next = (cz + dz) * width + cx + dx;
				if (!IsFree(point(next), radius, height)) { continue; }
				if (dx != 0 && dz != 0 && (!IsFree(point(cz * width + cx + dx), radius, height) || !IsFree(point((cz + dz) * width + cx), radius, height))) { continue; }
				const float cost = costs[current] + (dx != 0 && dz != 0 ? 1.414214f : 1.0f) * CellSize;
				if (cost >= costs[next]) { continue; }
				costs[next] = cost;
				parents[next] = current;
				open.push({ cost + PrototypeMath::Distance(point(next), point(last)), next });
			}
		}
	}
	if (first != last && parents[last] < 0) { return {}; }
	std::vector<Vector3> path;
	for (int id = last; id != first; id = parents[id]) { path.push_back(point(id)); }
	std::reverse(path.begin(), path.end());
	if (IsFree(destination, radius, height)) { path.push_back(destination); }
	else if (path.empty()) { path.push_back(point(last)); }
	return path;
}
