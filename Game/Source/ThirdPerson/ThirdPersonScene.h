#pragma once

#include "ThirdPersonMath.h"

#include "Component/StaticMeshComponent.h"
#include "Component/TransformComponent.h"
#include "Core/Math/Matrix4x4.h"
#include "GameObject.h"
#include "Mesh/StaticMesh.h"

#include <algorithm>
#include <cstddef>
#include <limits>
#include <vector>

struct CharacterTriangle
{
	Vector3 a;
	Vector3 b;
	Vector3 c;
};

struct CharacterBounds
{
	Vector3 minimum = { (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)(), (std::numeric_limits<float>::max)() };
	Vector3 maximum = { -(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)(), -(std::numeric_limits<float>::max)() };

	void Include(const Vector3& point)
	{
		minimum = { (std::min)(minimum.x, point.x), (std::min)(minimum.y, point.y), (std::min)(minimum.z, point.z) };
		maximum = { (std::max)(maximum.x, point.x), (std::max)(maximum.y, point.y), (std::max)(maximum.z, point.z) };
	}

	bool Overlaps(const CharacterBounds& other) const
	{
		return minimum.x <= other.maximum.x && maximum.x >= other.minimum.x && minimum.y <= other.maximum.y && maximum.y >= other.minimum.y && minimum.z <= other.maximum.z && maximum.z >= other.minimum.z;
	}
};

struct CharacterBVHNode
{
	CharacterBounds bounds;
	std::size_t first = 0;
	std::size_t count = 0;
	int left = -1;
	int right = -1;
};

/** 정적 맵의 CPU 충돌 질의다. 구성 후 맵 배치가 바뀌면 다시 구성한다. */
class ThirdPersonScene
{
public:
	/** 레벨 루트와 자손을 한 번 읽고 스킨이 없는 메시의 삼각형만 복사한다. */
	void Build(const std::vector<GameObject*>& roots)
	{
		triangles.clear();
		nodes.clear();
		std::vector<GameObject*> pending = roots;
		for (std::size_t index = 0; index < pending.size(); ++index)
		{
			GameObject* object = pending[index];
			pending.insert(pending.end(), object->GetChildren().begin(), object->GetChildren().end());
			const Matrix4x4 matrix = object->GetComponent<TransformComponent>()->GetWorldMatrix();
			for (const std::unique_ptr<Component>& component : object->GetComponents())
			{
				const StaticMeshComponent* meshComponent = dynamic_cast<const StaticMeshComponent*>(component.get());
				if (meshComponent == nullptr || meshComponent->GetStaticMesh() == nullptr) { continue; }
				const StaticMesh& mesh = *meshComponent->GetStaticMesh();
				if (!mesh.skinWeights.empty()) { continue; }
				for (std::size_t face = 0; face + 2 < mesh.indices.size(); face += 3)
				{
					const Vector3 a = matrix.TransformPoint(mesh.vertices[mesh.indices[face]].position);
					const Vector3 b = matrix.TransformPoint(mesh.vertices[mesh.indices[face + 1]].position);
					const Vector3 c = matrix.TransformPoint(mesh.vertices[mesh.indices[face + 2]].position);
					AddTriangle(a, b, c);
				}
			}
		}
		Finalize();
	}

	void AddTriangle(const Vector3& a, const Vector3& b, const Vector3& c)
	{
		if ((b - a).Cross(c - a).Length() < 0.001f) { return; }
		triangles.push_back({ a, b, c });
	}

	void Finalize()
	{
		nodes.clear();
		if (!triangles.empty())
		{
			nodes.reserve(triangles.size() / 4 + 1);
			BuildNode(0, triangles.size());
		}
	}

	std::size_t GetTriangleCount() const { return triangles.size(); }

	/** xz 위치의 가장 높은 보행 가능 바닥을 [bottom, top] cm 안에서 찾는다. */
	bool FindGround(const Vector3& position, float top, float bottom, float& height) const
	{
		bool found = false;
		height = bottom;
		const CharacterBounds bounds{ { position.x - 0.01f, bottom, position.z - 0.01f }, { position.x + 0.01f, top, position.z + 0.01f } };
		Visit(0, bounds, [&](const CharacterTriangle& triangle)
		{
			const Vector3 edge1 = triangle.b - triangle.a;
			const Vector3 edge2 = triangle.c - triangle.a;
			const Vector3 normal = edge1.Cross(edge2).Normalized();
			// 양면으로 바닥을 검사한다. 최대 경사는 약 48도다.
			if (std::abs(normal.y) < 0.67f) { return; }
			const float denominator = edge1.x * edge2.z - edge1.z * edge2.x;
			if (std::abs(denominator) < 0.00001f) { return; }
			const float dx = position.x - triangle.a.x;
			const float dz = position.z - triangle.a.z;
			const float u = (dx * edge2.z - dz * edge2.x) / denominator;
			const float v = (edge1.x * dz - edge1.z * dx) / denominator;
			if (u < -0.00001f || v < -0.00001f || u + v > 1.00001f) { return; }
			const float y = triangle.a.y + edge1.y * u + edge2.y * v;
			if (y <= top && y >= height)
			{
				height = y;
				found = true;
			}
		});
		return found;
	}

	/** foot은 발바닥 원점이다. 가장 깊은 캡슐 접촉 하나의 밀어낼 벡터를 반환한다. */
	Vector3 GetPenetration(const Vector3& foot, float radius, float height) const
	{
		const Vector3 start = foot + Vector3{ 0.0f, radius, 0.0f };
		const Vector3 end = foot + Vector3{ 0.0f, height - radius, 0.0f };
		const CharacterBounds bounds{ { foot.x - radius, foot.y, foot.z - radius }, { foot.x + radius, foot.y + height, foot.z + radius } };
		float deepest = 0.0f;
		Vector3 correction;
		Visit(0, bounds, [&](const CharacterTriangle& triangle)
		{
			Vector3 axis;
			Vector3 surface;
			ThirdPersonMath::ClosestSegmentTriangle(start, end, triangle.a, triangle.b, triangle.c, axis, surface);
			const Vector3 difference = axis - surface;
			const float distance = difference.Length();
			const float depth = radius - distance;
			if (depth <= deepest + 0.001f) { return; }
			Vector3 normal = difference.Normalized();
			if (distance < 0.00001f)
			{
				normal = (triangle.b - triangle.a).Cross(triangle.c - triangle.a).Normalized();
				if (normal.Dot((start + end) * 0.5f - triangle.a) < 0.0f)
				{
					normal = normal * -1.0f;
				}
			}
			deepest = depth;
			correction = normal * (depth + 0.02f);
		});
		return correction;
	}

	Vector3 Resolve(const Vector3& foot, float radius, float height) const
	{
		Vector3 result = foot;
		for (int iteration = 0; iteration < 12; ++iteration)
		{
			const Vector3 correction = GetPenetration(result, radius, height);
			if (correction.Length() < 0.001f) { break; }
			result += correction;
		}
		return result;
	}

	/** 8cm 간격의 구체 중첩 검사 후 이분 탐색한다. 현재 카메라 반경은 14cm 이상이다. */
	float CameraDistance(const Vector3& pivot, const Vector3& direction, float distance, float radius) const
	{
		const int steps = (std::max)(1, static_cast<int>(std::ceil(distance / 8.0f)));
		float previous = 0.0f;
		for (int step = 0; step <= steps; ++step)
		{
			const float current = distance * static_cast<float>(step) / static_cast<float>(steps);
			const Vector3 center = pivot + direction * current;
			if (GetPenetration(center - Vector3{ 0.0f, radius, 0.0f }, radius, radius * 2.0f).Length() > 0.001f)
			{
				float low = previous;
				float high = current;
				for (int iteration = 0; iteration < 8; ++iteration)
				{
					const float middle = (low + high) * 0.5f;
					const Vector3 point = pivot + direction * middle - Vector3{ 0.0f, radius, 0.0f };
					if (GetPenetration(point, radius, radius * 2.0f).Length() > 0.001f)
					{
						high = middle;
					}
					else
					{
						low = middle;
					}
				}
				return (std::max)(0.0f, low - 1.0f);
			}
			previous = current;
		}
		return distance;
	}

private:
	int BuildNode(std::size_t first, std::size_t count)
	{
		const int index = static_cast<int>(nodes.size());
		nodes.emplace_back();
		CharacterBounds bounds;
		for (std::size_t triangle = first; triangle < first + count; ++triangle)
		{
			bounds.Include(triangles[triangle].a);
			bounds.Include(triangles[triangle].b);
			bounds.Include(triangles[triangle].c);
		}
		nodes[index].bounds = bounds;
		nodes[index].first = first;
		nodes[index].count = count;
		if (count <= 12) { return index; }
		const Vector3 extent = bounds.maximum - bounds.minimum;
		const int axis = extent.x > extent.y && extent.x > extent.z ? 0 : (extent.y > extent.z ? 1 : 2);
		const std::size_t middle = first + count / 2;
		std::nth_element(triangles.begin() + first, triangles.begin() + middle, triangles.begin() + first + count, [axis](const CharacterTriangle& a, const CharacterTriangle& b)
		{
			const Vector3 centerA = a.a + a.b + a.c;
			const Vector3 centerB = b.a + b.b + b.c;
			return axis == 0 ? centerA.x < centerB.x : (axis == 1 ? centerA.y < centerB.y : centerA.z < centerB.z);
		});
		const int left = BuildNode(first, middle - first);
		const int right = BuildNode(middle, first + count - middle);
		nodes[index].left = left;
		nodes[index].right = right;
		return index;
	}

	template <typename Visitor>
	void Visit(int index, const CharacterBounds& bounds, const Visitor& visitor) const
	{
		if (nodes.empty() || !nodes[index].bounds.Overlaps(bounds)) { return; }
		const CharacterBVHNode& node = nodes[index];
		if (node.left >= 0)
		{
			Visit(node.left, bounds, visitor);
			Visit(node.right, bounds, visitor);
			return;
		}
		for (std::size_t triangle = node.first; triangle < node.first + node.count; ++triangle)
		{
			visitor(triangles[triangle]);
		}
	}

private:
	std::vector<CharacterTriangle> triangles;
	std::vector<CharacterBVHNode> nodes;
};
