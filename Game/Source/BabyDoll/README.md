# BabyDoll 프로토타입: 코드와 플레이 흐름 보고서

2026-11-26 엔진 업데이트 기준. Engine/Renderer 소스와 기획 원본 PDF/XLSX는 수정하지 않았다. Game 코드, 게임 설정, 프로젝트의 게임 파일 등록만 변경했다. 이 문서는 구현 보고서이며 원본 기획의 변경안이 아니다.

## 이번 개선

- 기본 모드는 1인용이며 S(왼발) / D(오른발)를 번갈아 눌러 이동한다. 시작 화면의 Play mode에서 협동 S/마침표 모드도 선택할 수 있다. 입력 순서를 유지하고 OS 키 반복을 걸음에서 제외한다. 양쪽 키의 눌림 상태를 분홍/청록 안내로 표시한다. 1인용과 협동은 동일한 이동 규칙을 사용한다.
- 추격 시 엔진 SceneCapture2D로 CCTV를 켜고, 추격 종료 후 5초 유지한다. 거리에 따른 구도, 부드러운 추적, 저채도, REC/스캔라인을 적용했다. 벽에 가까우면 측면 구도를 찾고 실제 카메라 거리로 FOV를 계산한다. 종료 시 캡처 객체를 제거해 추가 렌더링을 중단한다.
- 전등 9개에 근접 조명 컴포넌트를 부착했다. 몬스터가 수평 15m 안에 들어오면 0.6초 깜빡인 뒤 꺼지고, 범위를 벗어나면 즉시 복구한다. 한 층 테스트룸 규칙이다. Ambient/Directional은 남아 있어 완전 암전은 아니다.
- 바닥·벽·전시대·테이블에 엔진 박스 콜라이더를, 상자 3개에 Jolt 강체를 연결했다. 상자의 낙하·회전·정착 결과를 게임 이동/카메라 장애물 AABB에도 반영한다. 테이블 파괴 시 상판과 다리 콜라이더를 함께 제거하고 리셋 시 복구한다.
- 엔진 물리 씬의 최대 64바디 제한에 맞춰 바닥 타일 99개를 연속된 물리 바닥 하나로 합쳤다. 외형은 타일을 유지한다.
- 새 조명 타입과 편집 필드를 등록했다. 기본 물리 객체와 이 조명을 엔진 스냅샷으로 복제하는 검사도 추가했다. 전체 게임 저장은 아직 완성하지 않았다.

## 실행과 기능별 확인

Visual Studio의 F5는 기본 인자 `--editor`로 에디터를 연다. 모델 미리보기는 `SeonEngine.exe`를 인자 없이 실행하면 열린다. [Visual Studio 실행 설정](../../../Docs/BuildSetup.md#visual-studio에서-실행)

기본 실행 파일: `Binaries/v143/x64/Debug/SeonEngine.exe`. 모델 미리보기에서 Single player (S / D)를 선택하고 Start single player로 들어간다. 협동은 Co-op (S / period)를 선택한 뒤 Start co-op으로 시작한다. 기존 Bistro 데모는 `--third-person`, 에디터는 `--editor`, 자동 검사는 `--babydoll-test`다.

```powershell
./Tools/AssetPipeline/BuildBabyDoll.ps1 -NoDebugSymbols
./Binaries/v143/x64/Debug/SeonEngine.exe --babydoll-test
./Binaries/v143/x64/Debug/SeonEngine.exe
```

v143 14.44로 검증했다. `-NoDebugSymbols`는 설치된 PDB DLL 버전 충돌(LNK1101)을 피하는 실행 검증용 옵션이다. IDE 디버깅에서는 정상적인 심벌 환경에서 이 옵션 없이 빌드한다. 공용 디버그 설정은 변경하지 않았다.

| 조작 | 동작 |
| --- | --- |
| S / D 번갈아 누름 | 1인용 왼발/오른발, 리듬 가속 |
| S / . 번갈아 누름 | 협동 왼발/오른발, 리듬 가속 |
| 같은 발 두 번 | 넘어짐과 실수 소리 이벤트 1회 |
| 마우스 | 시야와 이동 방향 |
| T | 바라보는 테이블에 숨기 / 시야 방향으로 나오기 |
| Space | AI 확인용 큰 실수 소리 이벤트 |
| Esc | 일시정지와 커서 해제 / 재개 |
| Tab | 컴포넌트 진단 |

R 손동작은 예약만 했다. 1인용은 오른발 D와 우클릭 상호작용을 사용한다. Play mode는 일시정지 메뉴에서도 변경할 수 있다. Free movement는 WASD/Shift로 카메라 감각을 확인하는 개발 모드다.

1. 모델 미리보기에서 Idle/Walk/Run을 확인한다.
2. 1인용 게임에서 S와 D를 번갈아 눌러 이동/가속/카메라 추적을 확인한다. 같은 발을 반복해 넘어짐도 확인한다.
3. Esc 메뉴의 Preview CCTV로 영상을 단독 확인한다. 이 옵션을 끄면 추격 시에만 열린다. AI enabled/Ignore capture는 상태 실험용이다.
4. Tab에서 AI 상태, CCTV 유지 시간, 꺼진 전등 수, 상자 수와 Jolt ready를 확인한다. Proximity lamps로 효과를 비교한다.
5. Drop physics props again 후 Resume으로 상자를 다시 낙하시킨다.
6. 테이블 첫 추격 점유/이탈, 두 번째 점유와 파괴를 플레이 동선에서 확인한다.

## 코드 읽는 순서와 실행 흐름

`GameMain.cpp` → `BabyDollApplication::OnInitialize` → `OnUpdate` → `Simulate` → `Present` → `OnGui` 순서로 읽으면 된다. 게임 규칙 컴포넌트는 자동 Tick 대신 조립 코드가 Step을 명시적으로 호출한다.

```text
OnInitialize
  설정 로드 → 방/콜라이더/전등 → 몬스터/포즈
  플레이어·심박·CCTV·메인 카메라 부착 → 물리 상자
  시작 상태 복원 → 물리 준비 → 모델 미리보기

OnUpdate (렌더 프레임마다)
  입력/마우스 읽기 → 명령 누적 → 시야 목표 갱신
  고정 120Hz 반복:
    Simulate
      상호작용 → 플레이어 이동 → 심박/소리 → 청각 → AI/경로 이동
      테이블 파괴/잡힘 → 체크포인트 복귀
    PhysicsScene.Update(1/120) → 엔진 내부 60Hz 물리
    물리 상자 Transform → GameplaySpace 장애물 갱신
    단발 명령 소비
  World.Tick → Present → 삭제 예정 객체 정리

Present (렌더 프레임마다)
  이전/현재 틱의 위치·회전 보간 → CCTV/전등
  플레이어 도형·몬스터 포즈·소리 링 → 메인 카메라
OnGui → HUD/CCTV 텍스처/메뉴
GetRenderRequest → 메인 뷰 (엔진이 등록된 SceneCapture 뷰도 함께 렌더)
```

마우스 변위에는 deltaTime을 곱하지 않는다. 시야/추적은 지수 응답을 적용하고 고정 틱 사이 위치와 회전을 함께 보간한다. 긴 프레임은 0.1초까지만 소비한다. 포커스를 잃으면 일시정지하고 남은 입력을 폐기한다. 게임 규칙은 120Hz, Jolt는 내부 60Hz다. 물리 상자는 엔진이 돌려주는 포즈를 사용하며 별도 렌더 보간은 아직 없다.

## 실제 파일과 책임

아래 파일은 모두 `Game/Source/BabyDoll/`에 있다. World가 객체/컴포넌트를 소유하고 앱은 포인터를 빌린다. 값과 좁은 공간 질의로 의존을 줄였다. 표현 어댑터는 같은 객체의 Camera/Light/Transform 등을 조회한다.

| 코드 (.h/.cpp, 별도 표기 제외) | 책임 |
| --- | --- |
| BabyDollApplication | 생성, 실행 순서, 리셋, 테이블 외형을 조립 |
| BabyDollHud.cpp | 상태·CCTV 텍스처 표시, 메뉴 명령 |
| PrototypeTypes | 설정/명령/상태/계산, JSON 검증 |
| PrototypeInputAdapter.h | 엔진 입력을 순서 있는 게임 명령으로 변환 |
| PlayerMoveComponent | 박자/운동량/이동/넘어짐 |
| HeartComponent | 심박과 심장 소리 반경 |
| HearingComponent | 위치/반경/층/우선순위로 들린 소리 선택 |
| StitchBrainComponent | Patrol/Suspicious/Chase와 행동 단계, 목적지 결정 |
| AgentMovement | 경로 추종/가감속/회전 제한 |
| GameplaySpace | 평면 충돌/카메라 거리/2D A*. NavMeshVolume이 아님 |
| HideTableComponent | 수명/점유 세션/출구/파괴 가능 여부 |
| PoseAnimatorComponent | 클립 교차 재생, Walk/Run 위상 보존, 엔진 포즈 적용 |
| OrbitCameraComponent | 메인 카메라 각도/추적/줌/FOV/충돌 |
| RearCameraComponent | 추격 CCTV 수명/구도/추적, SceneCapture 생성/제거 |
| ProximityLightComponent | 근접 깜빡임/소등/복구, 타입과 편집 필드 등록 |
| PhysicsProps | 상자 생성/리셋, Jolt 포즈를 이동 장애물에 반영 |
| PrototypeTests.h | GPU 없이 게임 규칙/물리/스냅샷 검사 |

AI는 들린 위치를 추적 목적지로 사용한다. 실제 플레이어 위치는 잡기·숨기 관찰·근접 감속 등의 별도 판정에 사용한다. 경로는 100cm 격자 A*이며 불필요한 꺾임을 줄이고 앞쪽 지점을 추종한다. 몸의 전방으로 이동하며 큰 선회에서는 감속하고 회전 속도/가속도를 제한한다. 원본 SCISSORMAN의 +X 정면과 엔진 좌표계는 +90도 모델 보정으로 맞췄다.

## 수정 가능한 JSON 값 45개

`Game/Content/BabyDoll/Config/Prototype.json` 수정 후 다시 실행하면 적용된다. 재컴파일은 필요 없다. 허용 범위는 `PrototypeTypes.cpp::LoadPrototypeConfig`에 있다. 잘못된 값은 부분 적용하지 않고 로드를 실패시킨다. 런타임 자동 저장/파일 재로드는 없다.

| 키 | 현재 값 | 의미/단위 |
| --- | --- | --- |
| maxSpeed | 680 | 플레이어 최고 속도 cm/s |
| accelerationTime | 3.3 | 리듬 가속 기준 시간, 초 |
| rhythmWindow | 0.75 | 교대 발 이력 유지 시간, 초 |
| fastInterval | 0.17 | 빠른 걸음 기준 간격, 초 |
| momentumDecay | 0.9 | 초당 운동량 감소 |
| playerRadius | 30 | 플레이어 충돌 반경 cm |
| playerHeight | 120 | 플레이어 충돌 높이 cm |
| fallenTime | 1.5 | 넘어짐 지속 시간, 초 |
| patrolSpeed | 170 | 정찰 속도 cm/s |
| chaseSpeed | 400 | 추격 속도 cm/s |
| suspiciousSpeed | 600 | 의심 위치 조사 속도 cm/s |
| monsterAcceleration | 650 | 가속도 cm/s² |
| monsterBraking | 1100 | 감속도 cm/s² |
| monsterTurnRate | 220 | 최대 회전 속도, 도/초 |
| monsterTurnAcceleration | 900 | 최대 회전 가속도, 도/초² |
| monsterLookAhead | 140 | 경로 선행 거리 cm |
| monsterVisualYawOffset | 90 | 모델 정면 보정, 도 |
| catchDistance | 130 | 노출 플레이어 잡기 거리 cm |
| calmTime | 2 | 의심 상태에서 숨은 심박 진정 시간, 초 |
| mouseSensitivity | 0.13 | 마우스 변위당 회전, 도 |
| cameraDistance | 430 | 정지 카메라 거리 cm |
| runningCameraDistance | 560 | 최고 속도 카메라 거리 cm |
| cameraFov | 80 | 정지 수직 FOV, 도 |
| runningCameraFov | 96 | 최고 속도 수직 FOV, 도 |
| cameraFollow | 16 | 위치 추적 응답률, 클수록 빠름 |
| cameraLookResponse | 35 | 시야 회전 응답률, 클수록 빠름 |
| rearWidth | 512 | CCTV 렌더 타깃 가로 px, 정수 |
| rearHeight | 288 | CCTV 렌더 타깃 세로 px, 정수 |
| rearSaturation | 0.35 | CCTV 채도, 0=흑백/1=원색 |
| rearHold | 5 | 추격 종료 후 유지 시간, 초 |
| rearFarDistance | 1500 | 거리별 구도 변화 시작 거리 cm |
| rearNearDistance | 250 | 거리별 구도 변화 완료 거리 cm |
| rearCameraDistance | 470 | 먼 구도 카메라 거리 cm |
| rearClosestDistance | 230 | 가까운 구도 카메라 거리 cm |
| rearCameraHeight | 60 | 몬스터 중심(148cm) 위 카메라 높이 가산 cm |
| rearAimHeight | 45 | 몬스터 중심 위 시선 높이 가산 cm |
| rearHalfWidth | 260 | 구도 계산용 가로 반크기 cm |
| rearHalfHeight | 230 | 구도 계산용 세로 반크기 cm |
| rearCloseFill | 0.45 | 근거리 구도 채움 계수, 작을수록 넓게 담음 |
| rearFollow | 10 | CCTV 위치 추적 응답률 |
| lampDarkRadius | 1500 | 근접 소등 반경 cm, 수평 |
| lampFlickerTime | 0.6 | 소등 전 깜빡임 시간, 초 |
| lampIntensity | 2.8 | 전등 기본 강도 |
| lampRange | 1250 | 점광원 범위 cm |
| propDropHeight | 380 | 상자 초기 낙하 높이 cm, 중심 기준 |

제약: fastInterval < rhythmWindow, playerHeight >= playerRadius × 2, rearNearDistance < rearFarDistance, rearClosestDistance <= rearCameraDistance. CCTV 표시 폭은 화면의 32%, 최대 384px다. 렌더 타깃 해상도를 올리면 추가 뷰 비용이 커진다.

메뉴에서 즉시 바꿀 값은 Sensitivity, Camera follow, Look response, Model facing, CCTV saturation이다. Free movement/Play mode/AI enabled/Ignore capture/Diagnostics/Preview CCTV/Proximity lamps도 런타임 옵션이다. 조명 타입의 darkRadiusCm/flickerTime/baseIntensity는 Inspector용 등록 필드다. 현재 별도 테스트룸을 에디터에서 자동으로 여는 작업까지 연결한 것은 아니다.

아직 코드에서 수정하는 값: 방/테이블/스폰 배치는 BabyDollApplication.cpp::BuildRoom 및 PlayerSpawn/MonsterSpawn, 정찰 지점/행동 타이머는 StitchBrainComponent.cpp, 격자는 GameplaySpace.cpp, 상자 개수/배치는 PhysicsProps.cpp, 심박 기준(72/100/150)과 소리 수식은 HeartComponent.cpp다. 카메라 어깨 오프셋/피치 제한/충돌 여유는 OrbitCameraComponent.cpp에 있다. 이 값은 변경 후 빌드해야 한다.

## 미구현: 엔진 지원이 필요한 부분과 콘텐츠 후속 작업

| 항목 | 현재 상태 / 남은 이유 |
| --- | --- |
| 정밀 캐릭터 물리 이동 | 공개 API에 캡슐 캐릭터 컨트롤러, sweep/raycast/overlap 질의가 없어 GameplaySpace를 유지한다. 계단/접지/임의 삼각형 지형 대응은 남아 있다. |
| 물리 상자 밀기 | 캐릭터가 Jolt 바디가 아니므로 물리적인 밀기/접촉 반응은 미연결이다. 현재 상자 AABB를 장애물로 회피한다. |
| 일반 맵 길찾기 | 엔진 NavMeshVolume/베이크·경로 API가 없다. 평면 A*를 Bistro 임의 지형/계단에 그대로 사용할 수 없다. |
| 다수 물리 객체 | 현재 씬 최대 64바디다. 테스트룸은 바닥을 합쳐 대응했다. 대규모 맵은 엔진 용량 확장이 필요하다. |
| 실제 소리 재생 | 확인한 공용 API에 음원 재생/공간 오디오 경로가 없어 소리 이벤트 판정과 UI만 동작한다. |
| 전체 게임 저장/Play 복제 | 엔진 스냅샷은 준비되어 있다. 게임 AI/이동/심박/테이블 상태 등록과 참조 재연결은 후속 작업이다. 새 전등/기본 물리 객체 복제만 검사했다. |
| CCTV 추가 효과 | 저채도/스캔라인/REC는 구현했다. 필름 그레인은 이번 범위에서 제외했다. |
| 손동작/사다리/봉/문/열쇠/퍼즐/챕터 | 한 층 테스트룸 이후의 콘텐츠 작업이다. |
| 최종 캐릭터/애니메이션 | 플레이어는 도형이며 SCISSORMAN 기존 클립을 임시 매핑했다. 스티치 전용 촉수/포효/파괴/제자리 선회 클립은 없다. |

재시작은 시작 체크포인트의 메모리 상태 복원이다. 디스크 영속 저장과 임의 진행 시점 복원은 미구현이다. 테이블은 첫 추격 점유에서 수명을 소모하고 다음 점유부터 파괴 가능하다. 잔해에는 콜라이더를 두지 않는다. 숨은 심박은 기획의 2초 뒤 100 미만을 만족하도록 99.9로 진정시키는 구현 정책이다. UI는 기본 ImGui 폰트에 맞춰 영어다.

## 제거와 재사용

CCTV는 생성/Step/Clear/HUD 호출, 조명 효과는 부착/Step/타입 등록, 물리 상자는 Initialize/Reset/SyncSpace 호출을 빼면 기능 단위로 제거할 수 있다. 공통 PrototypeTypes와 명시된 엔진 API를 함께 가져가거나 호출 측 타입으로 치환해 재사용한다.

전체 제거는 GameMain 앱 선택/테스트 분기와 프로젝트 파일 등록을 제거한 뒤 게임 전용 폴더를 제거하는 방식이다. 엔진은 BabyDoll을 참조하지 않는다. 타입 등록은 콘텐츠 초기화 함수에서만 수행한다.

## 에셋과 검증

SCISSORMAN: Goth Scarecrow, [배포 페이지](https://goth-scarecrow.itch.io/scissorman), CC0. 원본 SHA-256/변환 정보는 Game/Content/BabyDoll/Scissorman/Source.json에 있다. PrepareScissorman.py로 외부 glTF/bin/PNG와 30Hz 키를 생성하고 BuildPrototypeGeometry.py로 상자/링을 생성한다. 메시 1개/스킨 1개/클립 10개를 사용한다.

2026-11-26 Debug 빌드 및 --babydoll-test 45개 검사 통과. 입력/키 반복/솔로 매핑, 박자/실수, 청각/심박/숨기, AI 상태, 경로/회전/가감속, 30/120Hz 이동·마우스 일관성, CCTV 수명/거리 계수, Jolt 낙하/정착, 조명 깜빡임/소등/복구와 스냅샷 필드 복원을 검사했다.

실행 창에서 모델 미리보기→플레이, 메인/CCTV 동시 출력과 저채도/스캔라인, 이동하는 몬스터, 전등 4/9 소등, 상자 3개/Jolt ready, Esc 일시정지를 확인했다. 벽 근접 CCTV 구도도 이후 보완했다. GPU 영상의 모든 위치 조합, 테이블 접근/강제 파괴 전체 동선과 장시간 플레이 감각은 추가 플레이 검증 대상이다.
