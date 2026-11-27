#pragma once
#include "GameplaySpace.h"
#include <array>
#include <span>

/** 기획/아트 검토용 전시관 한 구간. cm 단위 구획을 렌더와 이동 판정이 공유한다. */
namespace GrayboxLayout
{
	inline constexpr std::array<PrototypeObstacle,4> Screens{{
		{{-775.0f,0.0f,-650.0f},{80.0f,430.0f,-590.0f}},
		{{-80.0f,0.0f,150.0f},{775.0f,430.0f,210.0f}},
		{{-650.0f,0.0f,-200.0f},{-590.0f,360.0f,350.0f}},
		{{-2200.0f,0.0f,0.0f},{-1400.0f,430.0f,60.0f}}
	}};
	inline constexpr std::array<Vector3,10> Lamps{{
		{0.0f,460.0f,-1400.0f},{400.0f,460.0f,-450.0f},{-300.0f,460.0f,400.0f},
		{0.0f,460.0f,1400.0f},{-1150.0f,460.0f,1000.0f},{-1850.0f,460.0f,1150.0f},
		{-1800.0f,460.0f,-650.0f},{1500.0f,460.0f,-1300.0f},
		{1400.0f,1080.0f,800.0f},{1850.0f,1080.0f,1400.0f}
	}};

	inline constexpr std::array<PrototypeObstacle,4> UpperRoomShell{{
		{{1000.0f,640.0f,280.0f},{1050.0f,1240.0f,1700.0f}},
		{{2050.0f,640.0f,280.0f},{2100.0f,1240.0f,1700.0f}},
		{{1050.0f,640.0f,1650.0f},{2050.0f,1240.0f,1700.0f}},
		{{1050.0f,1240.0f,280.0f},{2050.0f,1280.0f,1700.0f}}
	}};

	inline float FitCeilingPointRange(const Vector3& lamp, float maximumRange, std::span<const PrototypeObstacle> walls)
	{
		const float floor = lamp.y >= 640.0f ? 640.0f : 0.0f;
		// Limit upstairs lighting to the bottom of the floor slab, including the ladder opening.
		float range = (std::min)(maximumRange, floor > 0.0f ? lamp.y - 604.0f : 620.0f);
		const std::array origin{lamp.x, lamp.y, lamp.z};
		for (const auto& wall : walls)
		{
			if (wall.maximum.y <= floor || wall.minimum.y >= (floor > 0.0f ? 1240.0f : 600.0f)) { continue; }
			const std::array lower{wall.minimum.x, wall.minimum.y, wall.minimum.z};
			const std::array upper{wall.maximum.x, wall.maximum.y, wall.maximum.z};
			const Vector3 closest{std::clamp(lamp.x, lower[0], upper[0]), std::clamp(lamp.y, lower[1], upper[1]), std::clamp(lamp.z, lower[2], upper[2])};
			const Vector3 offset = closest - lamp;
			const float nearestSquared = offset.Dot(offset);
			// The sphere may enter the wall, but it must stop before a ray could exit any face of it.
			for (std::size_t axis = 0; axis < origin.size(); ++axis)
			{
				const float entry = std::clamp(origin[axis], lower[axis], upper[axis]) - origin[axis];
				const float exit = origin[axis] <= lower[axis] ? upper[axis] - origin[axis]
					: origin[axis] >= upper[axis] ? origin[axis] - lower[axis]
					: (std::min)(origin[axis] - lower[axis], upper[axis] - origin[axis]);
				range = (std::min)(range, std::sqrt(nearestSquared - entry * entry + exit * exit) - 4.0f);
			}
		}
		return (std::max)(1.0f, range);
	}

	struct GalleryPointLight
	{
		Vector3 position;
		float range;
	};

	inline GalleryPointLight FitGalleryPointLight(const Vector3& ceiling, float maximumRange, std::span<const PrototypeObstacle> walls)
	{
		const bool upstairs = ceiling.y >= 640.0f;
		const float floor = upstairs ? 640.0f : 0.0f;
		const float floorSampleOffset = upstairs ? 80.0f : 100.0f;
		const float floorSampleRangeFraction = upstairs ? 0.93f : 0.80f;
		const float minimumHeight = floor + 45.0f;
		Vector3 position = ceiling;
		float range = FitCeilingPointRange(position, maximumRange, walls);
		// Keep the emitter as high as possible while leaving enough falloff margin at floor level.
		while (position.y > minimumHeight && Vector3{floorSampleOffset, position.y - floor, 0.0f}.Length() > range * floorSampleRangeFraction)
		{
			position.y = (std::max)(minimumHeight, position.y - 5.0f);
			range = FitCeilingPointRange(position, maximumRange, walls);
		}
		return {position, range};
	}

	inline const char* Zone(const Vector3& p)
	{
		if(p.y>500.0f) {return "05 / UPPER GALLERY";}
		if(p.x>825.0f) {return "04 / SERVICE ACCESS";}
		if(p.x<-825.0f) {return "03 / KEY GALLERY";}
		if(p.z<-650.0f) {return "01 / ARRIVAL";}
		return "02 / EXHIBITION LOOP";
	}
}
