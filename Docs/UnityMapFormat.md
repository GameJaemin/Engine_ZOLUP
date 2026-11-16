# Unity → ZOLUP 맵 포맷 초안

맵 JSON은 씬의 오브젝트, 계층, 배치, 컴포넌트를 기록한다. 메시의 정점과 인덱스, 모델 기본 재질은 `.gltf`와 외부 `.bin` 및 PNG로 함께 배포한다. Unity에서 새로 만든 메시도 엔진에서 읽을 수 있는 모델 파일로 변환한 뒤 맵에서 참조한다.

## JSON 예시

```json
{
  "version": 1,
  "assets": {
    "models": {
      "wall": "Game/Content/Meshes/Wall/Wall.gltf"
    },
    "materials": {
      "paintBlue": {
        "baseColor": [0.15, 0.32, 0.65],
        "roughness": 0.8,
        "metallic": 0.0
      }
    }
  },
  "objects": [
    {
      "id": "geometry",
      "name": "Geometry",
      "transform": {
        "position": [0, 0, 0],
        "rotation": [0, 0, 0, 1],
        "scale": [1, 1, 1]
      }
    },
    {
      "id": "wall-a",
      "name": "Wall",
      "parentId": "geometry",
      "transform": {
        "position": [0, 0, 300],
        "rotation": [0, 0, 0, 1],
        "scale": [1, 1, 1]
      },
      "components": [
        {
          "type": "staticModel",
          "model": "wall",
          "materialOverrides": [
            { "node": 0, "material": "paintBlue" }
          ]
        }
      ]
    },
    {
      "id": "sun",
      "name": "Sun",
      "transform": {
        "position": [0, 0, 0],
        "rotation": [0.3826834, 0, 0, 0.9238795],
        "scale": [1, 1, 1]
      },
      "components": [
        {
          "type": "light",
          "kind": "directional",
          "color": [1, 0.95, 0.85],
          "intensity": 1.0,
          "intensityUnit": "relative"
        }
      ]
    }
  ]
}
```

## 오브젝트와 좌표

- 최상위 필드는 `version`, `objects`, 선택 필드 `assets`로 구성한다. 버전이 없는 기존 맵은 이름과 월드 Transform을 가진 루트 오브젝트 목록으로 해석하는 호환 경로를 둔다.
- `id`는 맵에서 고유하고 이름을 변경해도 유지한다. `name`은 표시 이름이며 중복을 허용한다. `parentId`가 있으면 같은 맵의 부모를 가리키고, 생략하면 루트다. 누락된 부모와 순환 참조는 로드 오류다.
- 부모가 있으면 Unity 로컬 TRS를 기록한다. 루트는 월드 TRS를 기록한다. 내보낼 자식의 부모도 빈 그룹으로 포함하면 원래 계층을 보존할 수 있다.
- 맵 좌표는 왼손 좌표계(+X 오른쪽, +Y 위, +Z 앞)를 쓴다. Unity 씬에서 1 unit을 1m로 작업하면 위치와 거리만 100배 한다. `rotation`은 단위 쿼터니언 `[x, y, z, w]`, `scale`은 배율이다. glTF 자산은 기존 glTF 로더가 자산의 축과 단위를 변환한다.
- 오브젝트의 `active`는 Unity `activeSelf`에 대응한다. 컴포넌트의 `enabled`는 해당 Renderer나 Light의 활성 상태에 대응한다. 둘 다 생략 시 `true`이며, 부모의 비활성 상태는 자식에도 적용한다. `components`를 생략하면 그룹이나 마커로 쓸 수 있다.

## 정적 모델과 재질

- `assets.models`의 키를 `staticModel.model`에서 참조한다. 모델과 맵에서 직접 참조하는 텍스처 경로는 저장소 루트 기준이다. glTF 내부의 `.bin`과 텍스처 URI는 해당 glTF 파일 기준이다.
- 모델은 맵 오브젝트 아래에 glTF 노드 계층을 생성한다. 기본 재질은 glTF에서 읽는다. 씬에서 특정 인스턴스의 재질을 변경했다면 `materialOverrides`에 glTF 노드 인덱스와 `assets.materials`의 키를 기록한다. 재질을 바꾸는 노드에는 메시가 있어야 하며 한 노드에 한 번만 지정한다.
- 초기 재질 오버라이드는 엔진의 `Material`에 맞춰 선형 RGB `baseColor`, `metallic`, `roughness`, 선택적 PNG `baseColorTexture`를 쓴다. 색상 텍스처는 sRGB로 로드한다. 계수는 0~1이며, 생략 시 각각 흰색, 1, 1을 사용한다. Unity에서 Smoothness가 0~1 스칼라로 정의된 재질은 `roughness = 1 - smoothness`로 변환한다.
- 현재 glTF 로더는 메시의 첫 primitive만 읽는다. 여러 submesh와 재질 슬롯을 가진 Unity 메시를 보존하려면, 내보내기에서 슬롯별로 glTF 메시와 노드를 나누거나 엔진의 다중 primitive 지원을 먼저 구현해야 한다. 노드별 재질 오버라이드는 전자의 구조에 대응한다.
- 노멀, 발광, 알파 모드, 금속성·거칠기 텍스처, UV 타일링, 커스텀 셰이더는 렌더러와 Unity 재질 변환 규칙을 정할 때 확장한다. Unity 렌더 파이프라인과 셰이더에 따라 속성명과 텍스처 채널 배치가 달라진다.

## 라이트

공통 필드는 `type: "light"`, `kind`, 선형 RGB `color`, `intensity`, `intensityUnit`, 선택적 `enabled`다. 위치는 오브젝트의 월드 위치를 쓰고, 빛이 진행하는 방향은 오브젝트의 로컬 +Z를 월드 회전으로 변환해 얻는다.

| `kind` | 종류별 필드 | 적용 범위 |
| --- | --- | --- |
| `directional` | 추가 필드 없음. 현재 엔진 세기 단위는 `relative` | 1차 로드 대상 |
| `point` | `rangeCm` | 거리 감쇠와 함께 구현 |
| `spot` | `rangeCm`, `innerConeDegrees`, `outerConeDegrees` | Unity의 전체 원뿔 각도로 저장 |
| `area` | `shape: "rectangle"`이면 `widthCm`, `heightCm`, `shape: "disc"`이면 `radiusCm` | 면광원 또는 베이크 지원 이후 구현 |

현재 엔진은 첫 번째 활성 방향광 하나를 렌더러에 전달한다. 초기 내보내기는 적용할 방향광 하나를 선택하도록 하고, 추가 방향광과 지원하지 않는 종류는 오브젝트 이름과 함께 진단한다. Unity의 물리 단위 조명은 렌더 파이프라인별 변환 규칙을 정한 뒤 단위와 값을 함께 기록한다. 그림자, 쿠키, 환경광, 스카이박스, 라이트맵도 실제 소비 기능에 맞춰 확장한다.

## 1차 가져오기 범위

1. Unity에서 계층과 로컬 TRS, 정적 모델 참조, 기본 재질 및 재질 오버라이드, 방향광을 내보낸다. 모델 변환을 함께 실행하거나 미리 변환된 엔진 자산으로 매핑한다.
2. 엔진에서 오브젝트를 생성한 뒤 ID로 부모를 연결하고 컴포넌트를 붙인다. 맵이 방향광을 제공하면 시작 코드의 하드코딩된 방향광보다 맵 설정을 우선 적용한다. 누락된 자산이나 지원하지 않는 종류는 오브젝트 ID와 참조 경로를 포함해 진단한다.
3. 부모가 있는 벽 두 개가 동일한 모델을 공유하면서 서로 다른 재질로 보이고, Unity에서 설정한 방향광으로 조명되는지 확인한다.

씬 카메라는 다음 단계에서 `camera` 컴포넌트로 추가할 수 있다. 엔진 투영 정보에는 모드, 세로 FOV 또는 직교 전체 높이, near/far 거리가 필요하다. 플레이어 시작점과 충돌체도 실제 게임 로직과 연결할 때 별도 컴포넌트로 정의한다.
