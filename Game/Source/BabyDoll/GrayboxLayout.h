#pragma once
#include "GameplaySpace.h"
#include <array>

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
	inline const char* Zone(const Vector3& p)
	{
		if(p.y>500.0f) {return "05 / UPPER GALLERY";}
		if(p.x>825.0f) {return "04 / SERVICE ACCESS";}
		if(p.x<-825.0f) {return "03 / KEY GALLERY";}
		if(p.z<-650.0f) {return "01 / ARRIVAL";}
		return "02 / EXHIBITION LOOP";
	}
}
