#pragma once

#include "Core/Math/Matrix4x4.h"
#include "Core/Math/Vector3.h"

#include <algorithm>
#include <cmath>

namespace ThirdPersonMath
{
	inline float Length(const Vector3& value)
	{
		return std::sqrt(value.Dot(value));
	}

	inline Vector3 Normalize(const Vector3& value)
	{
		const float length = Length(value);
		return length > 0.00001f ? value / length : Vector3{};
	}

	inline Vector3 TransformPoint(const Vector3& value, const Matrix4x4& matrix)
	{
		return Vector3{ value.x * matrix.m[0][0] + value.y * matrix.m[1][0] + value.z * matrix.m[2][0] + matrix.m[3][0], value.x * matrix.m[0][1] + value.y * matrix.m[1][1] + value.z * matrix.m[2][1] + matrix.m[3][1], value.x * matrix.m[0][2] + value.y * matrix.m[1][2] + value.z * matrix.m[2][2] + matrix.m[3][2] };
	}

	inline Vector3 ClosestTrianglePoint(const Vector3& point, const Vector3& a, const Vector3& b, const Vector3& c)
	{
		const Vector3 ab = b - a;
		const Vector3 ac = c - a;
		const Vector3 ap = point - a;
		const float d1 = ab.Dot(ap);
		const float d2 = ac.Dot(ap);
		if (d1 <= 0.0f && d2 <= 0.0f) { return a; }
		const Vector3 bp = point - b;
		const float d3 = ab.Dot(bp);
		const float d4 = ac.Dot(bp);
		if (d3 >= 0.0f && d4 <= d3) { return b; }
		const float vc = d1 * d4 - d3 * d2;
		if (vc <= 0.0f && d1 >= 0.0f && d3 <= 0.0f) { return a + ab * (d1 / (d1 - d3)); }
		const Vector3 cp = point - c;
		const float d5 = ab.Dot(cp);
		const float d6 = ac.Dot(cp);
		if (d6 >= 0.0f && d5 <= d6) { return c; }
		const float vb = d5 * d2 - d1 * d6;
		if (vb <= 0.0f && d2 >= 0.0f && d6 <= 0.0f) { return a + ac * (d2 / (d2 - d6)); }
		const float va = d3 * d6 - d5 * d4;
		if (va <= 0.0f && d4 - d3 >= 0.0f && d5 - d6 >= 0.0f) { return b + (c - b) * ((d4 - d3) / (d4 - d3 + d5 - d6)); }
		const float inverse = 1.0f / (va + vb + vc);
		return a + ab * (vb * inverse) + ac * (vc * inverse);
	}

	inline void ClosestSegments(const Vector3& a, const Vector3& b, const Vector3& c, const Vector3& d, Vector3& first, Vector3& second)
	{
		const Vector3 u = b - a;
		const Vector3 v = d - c;
		const Vector3 offset = a - c;
		const float uu = u.Dot(u);
		const float vv = v.Dot(v);
		const float uv = u.Dot(v);
		const float ur = u.Dot(offset);
		const float vr = v.Dot(offset);
		float s = 0.0f;
		float t = 0.0f;
		if (uu <= 0.000001f)
		{
			t = vv > 0.000001f ? std::clamp(vr / vv, 0.0f, 1.0f) : 0.0f;
		}
		else if (vv <= 0.000001f)
		{
			s = std::clamp(-ur / uu, 0.0f, 1.0f);
		}
		else
		{
			const float denominator = uu * vv - uv * uv;
			s = denominator > 0.000001f ? std::clamp((uv * vr - ur * vv) / denominator, 0.0f, 1.0f) : 0.0f;
			t = (uv * s + vr) / vv;
			if (t < 0.0f)
			{
				t = 0.0f;
				s = std::clamp(-ur / uu, 0.0f, 1.0f);
			}
			else if (t > 1.0f)
			{
				t = 1.0f;
				s = std::clamp((uv - ur) / uu, 0.0f, 1.0f);
			}
		}
		first = a + u * s;
		second = c + v * t;
	}

	/** 캡슐 축과 삼각형 사이의 최근접 점이다. 선분이 면을 관통하는 경우도 포함한다. */
	inline void ClosestSegmentTriangle(const Vector3& start, const Vector3& end, const Vector3& a, const Vector3& b, const Vector3& c, Vector3& axisPoint, Vector3& surfacePoint)
	{
		axisPoint = start;
		surfacePoint = ClosestTrianglePoint(start, a, b, c);
		float best = (axisPoint - surfacePoint).Dot(axisPoint - surfacePoint);
		const Vector3 atEnd = ClosestTrianglePoint(end, a, b, c);
		const float endDistance = (end - atEnd).Dot(end - atEnd);
		if (endDistance < best)
		{
			axisPoint = end;
			surfacePoint = atEnd;
			best = endDistance;
		}
		const Vector3 normal = Normalize((b - a).Cross(c - a));
		const float denominator = normal.Dot(end - start);
		if (std::abs(denominator) > 0.000001f)
		{
			const float time = normal.Dot(a - start) / denominator;
			if (time >= 0.0f && time <= 1.0f)
			{
				const Vector3 point = Vector3::Lerp(start, end, time);
				const Vector3 closest = ClosestTrianglePoint(point, a, b, c);
				if (Length(point - closest) < 0.001f)
				{
					axisPoint = surfacePoint = point;
					return;
				}
			}
		}
		const Vector3 vertices[3] = { a, b, c };
		for (int edge = 0; edge < 3; ++edge)
		{
			Vector3 first;
			Vector3 second;
			ClosestSegments(start, end, vertices[edge], vertices[(edge + 1) % 3], first, second);
			const float distance = (first - second).Dot(first - second);
			if (distance < best)
			{
				best = distance;
				axisPoint = first;
				surfacePoint = second;
			}
		}
	}
}
