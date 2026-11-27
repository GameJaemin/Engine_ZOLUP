#pragma once
#include "GameplaySpace.h"
#include <limits>
#include <queue>
#include <functional>

/** 명시한 바닥 패치와 계단만 연결한다. 같은 XZ라도 다른 높이의 노드는 별개다. */
inline std::vector<Vector3> FindLayeredPath(const GameplaySpace& space, const Vector3& start, const Vector3& goal, float radius, float height)
{
	constexpr float Cell = 100.0f;
	const int width = static_cast<int>(space.halfWidth * 2.0f / Cell) + 1;
	const int depth = static_cast<int>(space.halfDepth * 2.0f / Cell) + 1;
	std::vector<Vector3> nodes;
	std::vector<std::vector<int>> cells(width * depth);
	for (int cell = 0; cell < width * depth; ++cell)
	{
		Vector3 point{ (cell % width) * Cell - space.halfWidth, 0.0f, (cell / width) * Cell - space.halfDepth };
		std::vector<float> heights{ 0.0f };
		for (const GroundPatch& patch : space.groundPatches) { if (patch.Contains(point)) { heights.push_back(patch.Height(point)); } }
		std::sort(heights.begin(), heights.end());
		heights.erase(std::unique(heights.begin(), heights.end()), heights.end());
		for (float y : heights)
		{
			point.y = y;
			if (!space.IsFree(point, radius, height)) { continue; }
			cells[cell].push_back(static_cast<int>(nodes.size()));
			nodes.push_back(point);
		}
	}
	const auto nearest = [&](const Vector3& desired)
	{
		int selected = -1;
		float best = (std::numeric_limits<float>::max)();
		for (std::size_t index = 0; index < nodes.size(); ++index)
		{
			const float score = PrototypeMath::Length(nodes[index] - desired);
			if (score < best && std::abs(nodes[index].y - desired.y) <= 70.0f) { best = score; selected = static_cast<int>(index); }
		}
		return selected;
	};
	const int first = nearest(start), last = nearest(goal);
	if (first < 0 || last < 0) { return {}; }
	std::vector<float> costs(nodes.size(), (std::numeric_limits<float>::max)());
	std::vector<int> parents(nodes.size(), -1);
	std::priority_queue<std::pair<float, int>, std::vector<std::pair<float, int>>, std::greater<std::pair<float, int>>> open;
	costs[first] = 0.0f;
	open.push({ 0.0f, first });
	while (!open.empty())
	{
		const int current = open.top().second;
		open.pop();
		if (current == last) { break; }
		const int cx = static_cast<int>(std::round((nodes[current].x + space.halfWidth) / Cell));
		const int cz = static_cast<int>(std::round((nodes[current].z + space.halfDepth) / Cell));
		for (int dz = -1; dz <= 1; ++dz) for (int dx = -1; dx <= 1; ++dx)
		{
			if (cx + dx < 0 || cx + dx >= width || cz + dz < 0 || cz + dz >= depth) { continue; }
			for (int next : cells[(cz + dz) * width + cx + dx])
			{
				if (next == current || std::abs(nodes[current].y - nodes[next].y) > 65.0f) { continue; }
				if (!space.CanTraverse(nodes[current], nodes[next], radius, height)) { continue; }
				const float cost = costs[current] + PrototypeMath::Length(nodes[current] - nodes[next]);
				if (cost >= costs[next]) { continue; }
				costs[next] = cost;
				parents[next] = current;
				open.push({ cost + PrototypeMath::Length(nodes[next] - nodes[last]), next });
			}
		}
	}
	if (first != last && parents[last] < 0) { return {}; }
	std::vector<Vector3> path;
	for (int node = last; node != first; node = parents[node]) { path.push_back(nodes[node]); }
	std::reverse(path.begin(), path.end());
	if (space.IsFree(goal, radius, height) && (path.empty() || space.CanTraverse(path.back(), goal, radius, height))) { path.push_back(goal); }
	return path;
}
