# BabyDoll 프로토타입: 코드와 플레이 흐름 보고서

현재 확정 기능 구현은 하단의 **확정 기능 확장: 특수 이동·양손·다층·저장** 절을 기준으로 한다. 초기 개선 내역과 검사 개수는 작업 당시의 이력이다.

2026-11-26 엔진 업데이트 기준. 이번 탈출 프로토타입 작업은 Game 코드와 게임 설정/문서만 변경했다. Engine/Renderer/Editor, 프로젝트 파일, 기획 원본 PDF/XLSX는 수정하지 않았다. 이 문서는 구현 보고서이며 원본 기획의 변경안이 아니다.

검증: Debug x64(v143 14.44, PDB 포함) 빌드 성공. BabyDoll 74개와 ThirdPerson 14개 검사 통과. 문 표면/가장자리 접근, 추격 중 열쇠 사용, 다른 벽에 의한 가림도 검사한다. 탈출 경로 검사는 실제 교대 발 입력과 PlayerMoveComponent 충돌을 사용하며, 전시대/테이블/칸막이/잠긴 문이 포함된 배치에서 키 획득 → 귀환 → 문 개방 → 클리어를 검사한다. 잠긴 문 상태의 괴물 순찰, 포효 중 단발 소리 기억, 원격 테이블 파괴 방지, 파괴 후 막힌 출구도 회귀 검사한다. 실제 창에서는 시작 화면과 목표/조작 안내의 렌더링을 확인했다. 사람의 협동 완주와 난이도/재미 평가는 별도 플레이 테스트가 필요하다.

## 이번 개선

- 시작부터 클리어까지 이어지는 한 층 탈출 구간이다. 전시홀 입구 → 북쪽 금색 기둥 사이로 창고 진입 → 황동 열쇠 획득 → 전시홀로 귀환 → 남동쪽 빨간 문 해제 → 초록 탈출 표식 도달 → 결과 화면/다시 플레이 순서다. 이 배치는 프로토타입용이며 원안의 2층 엘리베이터 챕터를 대체 확정하지 않는다.
- 기본 모드는 협동 S(왼발) / 마침표(오른발)다. 개발용 Single player에서는 S/D를 쓴다. 양쪽 모두 동일한 이동 규칙이며 키 반복을 걸음에서 제외한다. T(솔로 우클릭)는 열쇠 획득, 출구 해제, 숨기/나오기에 사용한다.
- 잡히면 1.6초 후 입구 체크포인트로 돌아온다. 열쇠/문/테이블 수명과 잔해/괴물/심박/물리 상자는 시작 상태로 함께 초기화한다. 중간 체크포인트나 디스크 저장은 제공하지 않는다. 총 플레이 시간과 잡힌 횟수는 완주 결과에 표시한다.
- 청각 판정은 기존 게임 로직을 유지한다. 실제 음원 재생은 연결하지 않는다. 실수 소리 알림, 소리 링, 심박, 위협 상태, 조명, CCTV가 시각 단서다. 목표 거리/방향을 표시하며 키와 열린 출구는 월드의 금색/초록색 표식으로 찾는다.
- 일반 포효/테이블 파괴 중 들은 소리는 기억해 종료 후 평가한다. 접근할 수 없는 숨기 지점의 수색 시간 초과는 의심으로 전환하며, 테이블 파괴는 가까이에서 정해진 관찰 시간을 채웠을 때만 시작한다. 파괴 후 출구가 모두 막혀도 Hidden 보호가 남지 않으며 안전한 착지가 전혀 없으면 체크포인트로 복귀한다.
- 추격 시 엔진 SceneCapture2D로 CCTV를 켜고, 추격 종료 후 5초 유지한다. 거리에 따른 구도, 부드러운 추적, 저채도, REC/스캔라인을 적용했다. 벽에 가까우면 측면 구도를 찾고 실제 카메라 거리로 FOV를 계산한다. 종료 시 캡처 객체를 제거해 추가 렌더링을 중단한다.
- 전등 9개에 근접 조명 컴포넌트를 부착했다. 몬스터가 수평 15m 안에 들어오면 0.6초 깜빡인 뒤 꺼지고, 범위를 벗어나면 즉시 복구한다. 한 층 테스트룸 규칙이다. Ambient/Directional은 남아 있어 완전 암전은 아니다.
- 바닥·벽·전시대·테이블에 엔진 박스 콜라이더를, 상자 3개에 Jolt 강체를 연결했다. 상자의 낙하·회전·정착 결과를 게임 이동/카메라 장애물 AABB에도 반영한다. 테이블 파괴 시 상판과 다리 콜라이더를 함께 제거하고 리셋 시 복구한다.
- 엔진 물리 씬의 최대 64바디 제한에 맞춰 바닥 타일 99개를 연속된 물리 바닥 하나로 합쳤다. 외형은 타일을 유지한다.
- 새 조명 타입과 편집 필드를 등록했다. 기본 물리 객체와 이 조명을 엔진 스냅샷으로 복제하는 검사도 추가했다. 전체 게임 저장은 아직 완성하지 않았다.

## 실행과 기능별 확인

Visual Studio에서 F5를 누르면 기본 인자 `--editor`로 에디터가 열린다. [Visual Studio 실행 설정](../../../Docs/BuildSetup.md#visual-studio에서-실행)

`Binaries/v143/x64/Debug/SeonEngine.exe`를 인자 없이 실행하면 BabyDoll 시작 화면이 열린다. `Single player (S / D)` 또는 `Co-op (S / period)`을 선택한 뒤 `Begin escape`를 누른다. `Esc`로 일시정지하거나 재개할 수 있고, 완주 화면에서 `Play again`을 누르면 새 판이 시작된다.

모델 미리보기는 `Developer tools`에서 `Model preview`를 선택하면 열린다. WASD 이동, 무적 및 AI 끄기 옵션도 `Developer tools`에 있다. Bistro 데모는 `--third-person`, BabyDoll 자동 검사는 `--babydoll-test`로 실행한다.

```powershell
./Tools/AssetPipeline/BuildBabyDoll.ps1
./Binaries/v143/x64/Debug/SeonEngine.exe --babydoll-test
./Binaries/v143/x64/Debug/SeonEngine.exe
```

v143 14.44와 디버그 심벌을 포함한 빌드로 검증한다. 제한된 실행 환경에서는 PDB 서비스 접근 제한으로 LNK1101이 발생할 수 있으며 일반 빌드 환경에서 검증한다. `-NoDebugSymbols`는 해당 제한 환경의 실행 확인용 선택 옵션이다.

| 조작 | 동작 |
| --- | --- |
| S / D 번갈아 누름 | 1인용 왼발/오른발, 리듬 가속 |
| S / . 번갈아 누름 | 협동 왼발/오른발, 리듬 가속 |
| 같은 발 두 번 | 넘어짐과 실수 소리 이벤트 1회 |
| 마우스 | 시야와 이동 방향 |
| T | 열쇠 획득 / 잠긴 출구 열기 / 테이블 숨기·나오기 |
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
  시작 상태 복원 -> 물리 준비 -> BabyDoll 시작 화면

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

## 수정 가능한 게임 설정

`Game/Content/BabyDoll/Config/Prototype.json` 수정 후 다시 실행하면 적용된다. 재컴파일은 필요 없다. 허용 범위는 `PrototypeTypes.cpp::LoadPrototypeConfig`에 있다. 잘못된 값은 부분 적용하지 않고 로드를 실패시킨다. 런타임 자동 저장/파일 재로드는 없다.

탈출 구간의 `Config/Escape.json`은 열쇠 위치(keyX/keyZ), 탈출 표식 위치(escapeX/escapeZ), 상호작용 거리(interactionDistance), 클리어 반경(escapeRadius)을 조절한다. 벽과 출입구는 고정된 한 층 배치이므로 허용 범위 안에서만 이동할 수 있다. 공간 단위는 cm다.

추가 AI 값: hideSuspicionRadius=300cm, hideSuspicionTime=2초, searchTimeout=3초, smashTime=1.4초, roarTime=2초, listenTime=1.3초, lookTime=2.2초, patrolWait=2.2초, hideExitTime=1.1초, wrongSoundRadius=4500cm, chaseStartDistance=1000cm. calmTime은 의심 중 심박 진정 시간으로 별도 조절한다.

최소 보행 보정: `minimumWalkSpeed`는 프로토타입 임시값 200cm/s다. 첫 발 입력으로 걷기를 시작하고, 0.75초 이내의 느린 교대 입력에도 최소 목표 속도를 유지한다. 출발·정지는 기존 속도 보간을 사용하므로 순간적으로 속도가 바뀌지 않는다. 입력이 0.75초 넘게 끊기면 하한을 해제하고 감속하며, 빠른 입력의 가속량·최고 속도·넘어짐 규칙은 유지한다. 벽 충돌이나 숨기/넘어짐 등의 상태에는 속도를 강제하지 않는다. 기획 원본의 수치를 수정한 것이 아니라 미정인 보행 감각을 보완한 게임 구현이다.

| 키 | 현재 값 | 의미/단위 |
| --- | --- | --- |
| maxSpeed | 680 | 플레이어 최고 속도 cm/s |
| accelerationTime | 3.3 | 리듬 가속 기준 시간, 초 |
| minimumWalkSpeed | 200 | 발 입력이 rhythmWindow 안에서 이어질 때 최소 목표 보행 속도, cm/s. maxSpeed 이하 |
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

아직 코드에서 수정하는 값: 고정 방 벽/출입구/테이블 배치는 EscapeScenario.h, 스폰은 BabyDollApplication.cpp, 정찰 지점은 StitchBrainComponent.h::PatrolRoute, 격자는 GameplaySpace.cpp, 상자 개수/배치는 PhysicsProps.cpp, 심박 기준(72/100/150)과 소리 수식은 HeartComponent.cpp다. 카메라 어깨 오프셋/피치 제한/충돌 여유는 OrbitCameraComponent.cpp에 있다. 이 값은 변경 후 빌드해야 한다.

## 미구현: 엔진 지원이 필요한 부분과 콘텐츠 후속 작업

| 항목 | 현재 상태 / 남은 이유 |
| --- | --- |
| 정밀 캐릭터 물리 이동 | 공개 API에 캡슐 캐릭터 컨트롤러, sweep/raycast/overlap 질의가 없어 GameplaySpace를 유지한다. 계단/접지/임의 삼각형 지형 대응은 남아 있다. |
| 물리 상자 밀기 | 캐릭터가 Jolt 바디가 아니므로 물리적인 밀기/접촉 반응은 미연결이다. 현재 상자 AABB를 장애물로 회피한다. |
| 일반 맵 길찾기 | 엔진 NavMeshVolume/베이크·경로 API가 없다. 명시한 바닥 패치와 계단의 다층 A*는 추가했으나 Bistro 임의 지형 자동 베이크는 아니다. |
| 다수 물리 객체 | 현재 씬 최대 64바디다. 테스트룸은 바닥을 합쳐 대응했다. 대규모 맵은 엔진 용량 확장이 필요하다. |
| 실제 소리 재생 | 확인한 공용 API에 음원 재생/공간 오디오 경로가 없어 소리 이벤트 판정과 UI만 동작한다. |
| 엔진 전체 World 저장/Play 복제 | 게임 전용 체크포인트 저장/복원은 추가했다. 엔진의 임의 World 스냅샷으로 새 게임 컴포넌트를 복제하는 범용 바인딩은 별도다. |
| 정식 맵·미정 콘텐츠 | 특수 이동·상층 테스트룸·엘리베이터 종료는 추가했다. 정식 방 배치/퍼즐 조건 등 기획 미정 콘텐츠는 확정하지 않는다. |
| 최종 캐릭터/애니메이션 | 복셀 플레이어와 몸 벌어짐/촉수 스티치를 절차 애니메이션으로 검증한다. 최종 아트용 스켈레탈 클립과 본 마스크 블렌딩은 임시 외형과 구분한다. |

재시작은 현재 검증 이벤트의 체크포인트를 복원하며 디스크에도 저장한다. 물리 속도·일시적인 효과까지 포함한 임의 World 전체 복원과는 구분한다. 테이블은 첫 추격 점유에서 수명을 소모하고 다음 점유부터 파괴 가능하다. 잔해에는 콜라이더를 두지 않는다. 숨은 심박은 기준값보다 0.1 낮게 진정시키는 구현 정책이다. UI는 기본 ImGui 폰트에 맞춰 영어다.

## 제거와 재사용

CCTV는 생성/Step/Clear/HUD 호출, 조명 효과는 부착/Step/타입 등록, 물리 상자는 Initialize/Reset/SyncSpace 호출을 빼면 기능 단위로 제거할 수 있다. 공통 PrototypeTypes와 명시된 엔진 API를 함께 가져가거나 호출 측 타입으로 치환해 재사용한다.

전체 제거는 GameMain 앱 선택/테스트 분기와 프로젝트 파일 등록을 제거한 뒤 게임 전용 폴더를 제거하는 방식이다. 엔진은 BabyDoll을 참조하지 않는다. 타입 등록은 콘텐츠 초기화 함수에서만 수행한다.

## 에셋과 검증

SCISSORMAN: Goth Scarecrow, [배포 페이지](https://goth-scarecrow.itch.io/scissorman), CC0. 원본 SHA-256/변환 정보는 Game/Content/BabyDoll/Scissorman/Source.json에 있다. PrepareScissorman.py로 외부 glTF/bin/PNG와 30Hz 키를 생성하고 BuildPrototypeGeometry.py로 상자/링을 생성한다. 메시 1개/스킨 1개/클립 10개를 사용한다.

2026-11-26 Debug 빌드 및 --babydoll-test 45개 검사 통과. 입력/키 반복/솔로 매핑, 박자/실수, 청각/심박/숨기, AI 상태, 경로/회전/가감속, 30/120Hz 이동·마우스 일관성, CCTV 수명/거리 계수, Jolt 낙하/정착, 조명 깜빡임/소등/복구와 스냅샷 필드 복원을 검사했다.

실행 창에서 모델 미리보기→플레이, 메인/CCTV 동시 출력과 저채도/스캔라인, 이동하는 몬스터, 전등 4/9 소등, 상자 3개/Jolt ready, Esc 일시정지를 확인했다. 벽 근접 CCTV 구도도 이후 보완했다. GPU 영상의 모든 위치 조합, 테이블 접근/강제 파괴 전체 동선과 장시간 플레이 감각은 추가 플레이 검증 대상이다.

## 잡힘 점프스케어 (2026-11-27)

잡힘의 시작은 BabyDollApplication::BeginCaught()에서 한 번 처리한다. 플레이어 이동을 막고 CCTV를 닫은 뒤 CaughtCameraComponent에 몬스터와 플레이어 위치를 전달한다. 몬스터는 플레이어 쪽을 향하고 기존 Scissorman_SimplesAttack 클립을 한 번 재생한다. 메인 카메라는 일반 OrbitCamera 대신 얼굴 앞 고정 구도에서 빠르게 다가가며 위치/롤 흔들림을 적용한다. 잡힘 전용 약한 조명이 얼굴을 비추며, 일반 HUD와 CAUGHT 문구는 숨긴다.

기본 연출은 1.8초다. 1.15초까지 얼굴 연출을 보여주고 0.35초 동안 암전한 뒤 검은 화면을 유지하다 기존 체크포인트로 복귀한다. 흔들림은 잡힘 경과 시간으로 계산하므로 프레임률에 따라 누적되거나 일시정지 중 진행하지 않는다. Esc 일시정지는 유지하며 잡힘 중 마우스로 카메라 방향을 바꿀 수 없다. 개발 메뉴의 Preview jumpscare로 같은 경로를 즉시 실행할 수 있다. 이 버튼도 사망 횟수를 늘리고 체크포인트로 복귀한다.

| Prototype.json 키 | 기본값 | 의미 |
| --- | --- | --- |
| caughtDuration | 1.8 | 잡힘부터 복귀까지 총 시간(1~2초) |
| caughtFaceHeight | 250 | Head 노드가 없는 모델의 대체 조준 높이(cm) |
| caughtCameraDistance | 70 | 얼굴 앞 초기 카메라 거리(cm), 이후 72%까지 접근 |
| caughtFov | 48 | 초기 수직 화각(도), 이후 8도 좁아짐 |
| caughtShakeStrength | 14 | 위치 흔들림 크기(cm), 0으로 끄기 |
| caughtShakeAngle | 8 | 회전 흔들림 크기(도), 곰돌이는 pitch/yaw/roll에 분배, 0으로 끄기 |
| caughtFadeStart | 1.15 | 암전 시작 시간(초) |
| caughtFadeTime | 0.35 | 암전 소요 시간(초) |

caughtFadeStart + caughtFadeTime + 0.1 <= caughtDuration 조건으로 완전 암전 구간을 확보한다. 설정은 재실행 시 적용한다. 전용 잡기 클립이 없어 기존 공격 클립을 임시 사용한다. 현재 AudioSystem/AudioComponent는 재생 API가 없는 뼈대여서 점프스케어 효과음은 연결하지 못했다. 엔진/렌더러 변경은 없다. 제거하려면 CaughtCameraComponent 부착과 BeginCaught/Present 연결, HUD 암전 출력을 빼고 기존 Orbit 카메라/복귀 처리만 유지하면 된다.

공격 포즈에서 머리가 움직여 가슴이 화면을 가리지 않도록 PoseAnimatorComponent::TryGetNodeWorldPosition으로 SCISSORMAN의 Head2 뼈(없으면 Head)의 월드 위치를 읽고 그 위 12cm를 조준한다. 이 위치는 CaughtCameraComponent에 값으로 전달한다. 머리 노드가 없으면 caughtFaceHeight를 사용한다. Debug 빌드와 자동 검사 82개를 통과했으며, 머리 계층/월드 변환과 포즈 추적도 검사했다.

최종 실행 검증에서 Head2 얼굴 클로즈업, HUD/CCTV 숨김, 암전, 입구 체크포인트 복귀를 확인했다. 연출 중 일시정지되면 검은 화면에서도 Esc 재개 안내를 표시한다. 메뉴의 Developer tools > Preview jumpscare로 재확인할 수 있다.

## 발광 전등과 라이트 동기화

천장 높이 600cm 밑에 전등 9개를 배치한다. 각 전등은 어두운 하우징, 발광 커버 메시, 아래쪽을 비추는 PointLight, ProximityLightComponent로 구성된다. 따뜻한 색과 차가운 색을 번갈아 사용한다. 발광은 실제 emissiveTexture를 사용하는 재질이며, 주변 조명은 PointLight가 담당한다. 발광 재질 자체가 주변을 조명하는 GI나 번짐 효과를 새로 구현한 것은 아니다.

ProximityLightComponent::Step의 단일 깜빡임 결과를 PointLight 강도와 커버 재질에 함께 적용한다. 켜짐(100%)/약광(12%)/소등(0%)을 같은 틱에 전환한다. 근접 0.6초 후 꺼지고 반경을 벗어나거나 기능을 끄면 복구하는 기존 규칙은 유지한다. Reset도 두 출력을 즉시 복구한다.

BindEmissiveVisual(visual, materials)로 발광 메시 계층을 연결하며, materials는 소등/약광/점등 순서다. 컴포넌트는 메시를 ObjectID로 조회하므로 메시를 제거해도 포인터가 남지 않는다. 재질과 텍스처는 초기 로딩 때 공유하고 상태 변경 시에만 교체한다. 발광 커버를 제거하면 기존 라이트 제어는 그대로 동작한다. 이 런타임 메시 연결은 전체 게임 저장/복제 시 재연결할 대상이며, 아직 해당 참조까지 직렬화하지 않는다.

Game/Content/BabyDoll/Materials의 LampWarmOn/Dim, LampCoolOn/Dim, LampOff.material.json과 대응 PNG가 전등 외형을 담당한다. Tools/AssetPipeline/BuildLampMaterials.py는 외부 패키지 없이 선형 색상(약광은 0.12배)을 sRGB PNG로 변환해 재생성한다. 소등 재질에는 발광 텍스처가 없다. 색을 바꾸면 BuildRoom의 PointLight 색도 함께 맞춘다. 위치·크기는 BuildRoom에서, 깜빡임 반경/시간/라이트 강도는 기존 Prototype.json의 lamp 설정에서 조정한다.

검증: 전체 재빌드 및 자동 검사 88개 통과. 실제 실행에서 점등된 발광 커버와 몬스터 접근 시 커버/주변 라이트의 동시 소등을 확인했다. Pull 이전 오브젝트의 Model/Material 형식이 남아 전체 재빌드했으며, 이 환경의 PDB 불일치는 빌드 프로세스에 _CL_=/Z7을 지정해 우회했다. 공용 프로젝트 설정은 변경하지 않았다.

## 2026-11-27 보행 규칙·복셀 곰돌이·카메라 개선

정지 후 0.75초를 넘겨도 PlayerMoveComponent는 마지막 발을 기억한다. S → 정지 → S는 넘어짐, S → 정지 → 반대 발은 기본 보행으로 재개한다. 박자 콤보가 끊길 때 가속 운동량만 초기화한다. 새 판/체크포인트 복귀/상태 전환으로 발 이력을 초기화하는 기존 처리는 유지한다. HUD는 다음 발을 안내하고, 솔로 상호작용 표시는 실제 우클릭 조작과 맞췄다.

기본 걷기에서는 카메라 거리와 화각을 유지하고, 300cm/s 이상에서 달리기 구도로 부드럽게 전환한다. 몸 회전도 위치와 같은 고정 틱 보간 비율을 적용해 렌더 프레임 사이 회전 튐을 줄였다. 카메라 어깨 오프셋도 장애물 검사를 거치며, 후방 장애물 진입 시 즉시 당기고 벗어날 때 부드럽게 복구하는 처리는 유지한다.

BuildVoxelDoll.py가 4cm 복셀 격자에서 곰돌이를 생성한다. 내부 면은 제거하고 표면을 색상별로 합쳐 glTF/bin으로 저장한다. 외부 아트 다운로드나 런타임 복셀 객체 수천 개 생성 없이, 실제 복셀 형상을 기존 정적 메시 경로로 표시한다. 몸통·머리·양팔·양다리 6부위, 총 5,468삼각형이다. 분홍/하늘색 몸, 귀·주둥이·눈·눈물·봉합선, 등 뒤 빨간 리본과 꼬리를 포함한다. 현재 GLTFLoader는 메시당 첫 primitive만 변환하므로 색상별 단일 primitive 메시를 하위 노드로 나눴다. 엔진 코드는 변경하지 않았다.

VoxelDollComponent는 위치·속도·몸/시야 방향·플레이어 상태를 값으로 받아 외형만 갱신한다. 실제 이동 거리로 보행 위상을 진행하고, 달릴 때 보폭을 늘려 과도한 빨리감기를 줄인다. 팔/다리 교차 운동, 작은 체중 이동, 호흡, 달리기 기울기, 회전 기울기, 머리 시선, 숨기/넘어짐 자세를 절차적으로 보간한다. 벽에 막히면 보행 위상이 진행하지 않고, 일시정지 중 애니메이션 시간도 흐르지 않는다. 새 모델을 적용할 때 이 컴포넌트와 외형 생성 부분을 교체하면 이동·충돌·심박·AI는 그대로 사용할 수 있다. 부위 참조는 World의 ObjectID로 조회하며 모델 제거 시 남은 포인터를 역참조하지 않는다.

조정값은 Prototype.json에 있다. minimumWalkSpeed=200cm/s, dollStrideLength=135cm(한 보행 주기의 이동 거리, 달리기에서 최대 1.7배), dollStepSwing=30도, dollBodyBob=2cm, dollTurnLean=6도다. 형상/색은 BuildVoxelDoll.py, 관절 위치와 표현 로직은 VoxelDollComponent.cpp에서 조정한다. 재생 파일에 저장된 스켈레탈 애니메이션이나 발 IK는 아니며, 실제 모델의 본 애니메이션으로 교체할 대상이다. 런타임 외형 바인딩의 저장/복제는 아직 지원하지 않는다. 사운드 연결은 요청에 따라 후순위로 유지했다.

검증: Release 최적화 빌드와 Debug 빌드, 자동 검사 103개 통과. 정지 후 같은 발/반대 발 입력, 느린 보행, 기존 탈출 경로, 카메라 화각/측면 벽, 거리 기반 보행/일시정지/넘어짐을 검사했다. 실제 창에서 전체 색상 메시와 리본을 포함한 곰돌이 로딩을 확인했다. 기존 Release 테스트 창이 실행 중이어서 최종 Release 최적화 결과는 Binaries/v143/x64/Quality에 별도로 출력했다. 배포 스크립트의 -RuntimeDirectory로 이 경로를 지정할 수 있다.

## 확정 기능 확장: 특수 이동·양손·다층·저장

이번 확장은 Game/Source/BabyDoll, Game/Content/BabyDoll, 에셋 생성/배포 도구와 게임 소스 등록용 프로젝트 항목만 변경한다. 기획 원본과 Engine/Renderer 코드는 변경하지 않는다. 발전기·퍼즐 종류/조작 조건·엘리베이터 개방 조건·스토리 단서 배치·아이템 자동 낙하/몬스터 운반·스태미나 지상 회복 속도 등 보류/미정 사항과 실제 음원 재생은 추가하지 않는다. 계단·사다리·연구실·봉·통로·엘리베이터와 체크포인트 발생 지점은 기능 검증용 임시 배치다.

조작: 협동은 S/마침표 교대 보행, R 왼손/T 오른손이다. 솔로는 S/D, 좌클릭 왼손/우클릭 오른손이다. 빈손의 키로 열쇠를 들고 해당 손으로 잠긴 문에 사용한다. 잠금 해제 후에는 같은 문을 열고 닫을 수 있다. 사다리와 통로는 어느 손으로든 진입하고, 봉은 왼손으로 잡고 놓는다. 특수 이동 사이의 직접 전환은 막고 이동 상태에서만 진입한다. 아이템 보유와 상태 전환을 강제로 연동하는 미정 정책은 추가하지 않았다.

| 파일 | 역할 |
| --- | --- |
| TraversalComponent | 사다리 42cm 교대 이동/90cm 미끄러짐/5번째 실수 소음, 통로 진입·통과·이탈, 봉 매달리기·10초 소진·소음 |
| HandInventoryComponent | 좌우 손별 아이템 ID, 빈손 검사, 일치하는 손의 열쇠 사용, 저장 상태 |
| DoorComponent | 잠금/개폐/통과 가능 상태, 몬스터의 정찰 1.5초·추격 0.2초 개방, 닫힐 때 끼임 방지 |
| GameplaySpace / LayerNavigation | 바닥 패치·32단 계단의 접지, 서로 다른 높이의 노드와 계단 연결, 낭떠러지 진입 방지 |
| ObjectSoundComponent | ObjectSounds.json의 반경·충돌 속도·지속 시간·쿨다운·AI 반응·층/거리 예외·우선순위·SfxId 읽기 |
| CheckpointIO / ChapterSave | 버전/배치 ID 검증, 임시 파일 후 교체 저장, 손·문·테이블·물리 상자 포즈 복원, 재시작 시 순찰/안정 심박 초기화 |
| ChapterContent | 검증용 상층 연구실·계단·사다리 개구부·통로·봉·엘리베이터와 상호작용 조립 |
| StitchVisualComponent | 복셀 스티치의 몸 벌어짐, 6개 촉수의 독립 운동, 이동/두리번/포효/파괴 표현 |

Play 흐름은 기존 방 탐색 → 창고 열쇠를 원하는 손으로 획득 → 해당 손으로 빨간 문 해제 → 계단 또는 사다리로 상층 이동 → 엘리베이터 영역 진입으로 챕터 종료다. 별도의 미정 퍼즐/엘리베이터 잠금 조건은 만들지 않았다. 몬스터는 다른 층의 플레이어를 계단으로 따라가며 잠긴 문도 연다. 사다리로 층을 바꾸거나 통로에 들어가면 추격 포기 포효를 거친다. 매달린 플레이어에게는 지면의 마지막 소리 위치로 접근하고 숨기 판정을 적용한다. 통로 안에서 넘어져도 통로 보호가 유지된다.

카메라는 사다리 상승·하강과 좁은 통로에서 전용 구도로 0.5초에 걸쳐 전환하고 마우스 회전을 무시한다. 사다리에서는 이동 방향과 관계없이 몸과 머리가 ladderFacingYaw(기본 0도, 현재 맵 +Z의 가로대 방향)를 바라본다. 카메라는 이 방향의 반대편 ladderCameraDistance(기본 300cm)에 두고 플레이어 위치 + cameraFollowHeight(기본 60cm)를 바라본다. 사다리에서는 cameraPosition/cameraTarget 대신 이 상대 구도를 사용한다. 바닥/벽이 시야를 가리면 거리를 줄이고 시야가 열리면 부드럽게 늘리며, 가까운 구간은 몸 높이 140cm를 담도록 화각을 최대 110도까지 확보한다. 통로는 기존 고정 위치·조준점·화각을 유지한다. 빠져나오면 기존 Orbit으로 복귀한다. HUD의 목표 문구/AI 상태는 진단에서만 보인다. 매달리기 중에만 스태미나 게이지를 표시하며 CCTV는 저채도·스캔라인·REC·그레인 오버레이를 사용한다.

복셀 플레이어는 느린 좌우 한 걸음 반응과 속도 구간별 보행 표현, 공포 대기·기상·손 사용·사다리·기어가기·매달리기 자세를 추가했다. 복셀 스티치는 추격 중 15m~6.5m에서 몸/촉수가 열리고, 의심 중에는 유지하며 정찰에서 초당 0.55로 닫힌다. 애니메이터용 최종 스켈레탈 클립을 만든 것은 아니며 독립 부위의 절차 애니메이션으로 기능을 검증한다. SCISSORMAN은 삭제하지 않고 개발 메뉴의 Voxel Stitches 체크를 끄거나 모델 미리보기에서 사용할 수 있다. 모델 생성은 BuildVoxelDoll.py / BuildVoxelStitch.py다.

Prototype.json에서 심박 기준/소리 곡선/사다리/통로/매달리기 수치도 수정할 수 있다. linkCalmToHideSuspicion=true이면 심박 진정 시간을 숨기 의심 전환 시간에 연동한다. 사물 소리의 문/테이블/상자 행은 프로토타입 임시값으로 ObjectSounds.json에 분리했다. SfxId는 읽기만 하며 음원을 재생하지 않는다. 정식 맵 배치와 순찰 지점은 콘텐츠 조립 코드에 남아 있으므로 맵 에셋이 확정되면 데이터로 옮길 대상이다.

자동 저장은 현재 검증용 열쇠 획득/상층 도착 이벤트에 연결했다. Binaries/BabyDollSave/checkpoint.json을 읽어 다음 실행에서 복원하며, Reset test room / 새 플레이는 저장을 초기화한다. 손·문·테이블 수명/파괴 상태, 상자 Transform과 진행 상황을 복원한다. 사망 후 복귀와 디스크 불러오기는 심박을 안정 수치로 초기화하고 AI를 새 순찰로 시작한다. 저장 당시 추격/포효·예약 소리·경로·속도는 이어받지 않는다. AI 위치는 비어 있고 플레이어와 충분히 떨어져 있으면 유지하고, 아니면 검증된 순찰 지점 중 먼 곳을 선택한다. 기준 거리는 max(heartFearRadius, catchDistance + chaseSpeed * 2초)이며 바닥/충돌도 검사한다. 안전한 위치가 전혀 없으면 안내 후 일시정지한다. 새 소리에는 즉시 정상 반응하며 임시 무적/청각 차단은 적용하지 않는다. 순찰 중 다른 층의 플레이어 위치를 무조건 목적지로 사용하던 동작도 제거했다. 물리 상자의 선속도/각속도는 공개 RigidBody API에서 조회·설정할 수 없어 포즈 복원 후 정지 상태로 물리를 재등록한다. 깜빡임/일시적인 사물 소리의 재생 단계까지 영속 저장하지는 않는다.

엔진 한계에 대한 대응: 캡슐 컨트롤러·일반 지형 sweep·NavMesh API가 없어 바닥 패치와 게임 A*를 사용한다. 임의 지형 자동 베이크가 아니다. 사다리 중간에서 놓기는 낙하/접지 지원 없이 공중에 남지 않도록 끝부분에서만 허용한다. 바닥 구멍/계단의 캐릭터 이동은 게임 어댑터가 처리하며 Jolt의 캐릭터 바디로 움직이지 않는다. 사물 충돌 소음은 게임 AABB 접촉 시도로 판단하고 Jolt 접촉 콜백이라고 표시하지 않는다. 대규모 물리 배치는 엔진의 바디 한도 확장이 필요하다.

확인 방법: Esc → Developer tools → Feature test positions에서 Ladder bottom/top, Crawl entrance, Hanging bar, Stair approach, Upper room으로 이동한다. 특수 이동 확인 시 Ignore capture를 사용하면 연출 중 잡히는 것을 막을 수 있다. 각 이동은 교대 입력으로 확인하고, 사다리는 같은 발 반복과 5번째 소음을, 봉은 10초 소진을, 통로는 추격 포기와 고정 카메라를 확인한다. 개발용 위치 이동은 정식 체크포인트 배치 도구가 아니다.

## Crawl 표시·상태 수정

통로 고정 카메라를 측벽/지붕 너머의 위치에서 열린 입구를 바라보는 낮은 위치로 옮겼다. 천장이나 벽을 숨기지 않고 통로 전체의 곰돌이를 볼 수 있도록 조준점과 화각을 조정했다. 양쪽 진입/이탈 지점은 실제 통로 밖(-650 / 250cm, 통로는 X -550~150cm)에 둔다. 기어가는 외형은 바닥에 묻히지 않도록 높이를 올리고, 통로 안 실수 중에도 낮은 자세를 유지한다.

중간에서 R/T(솔로 좌/우클릭)를 다시 눌러도 Moving으로 풀리지 않는다. 출구까지 교대 입력으로 이동하면 바깥의 안전한 지점에서만 자동으로 일어난다. 진입 전 평상 보행으로 통로를 통과하는 우회도 낮은 자세 전용 통과 영역(passHeight)으로 차단했다. 이 영역은 카메라를 막거나 메시를 숨기지 않는다. 입구에 바짝 붙어도 통로 진행 방향을 보고 상호작용할 수 있고, 진입 때 플레이어를 뒤로 순간 이동시키지 않는다. 통로 안에서 장애물에 막히면 진행률도 멈춰 뒤쪽으로 순간 이동하지 않는다.

확인: Developer tools → Feature test positions → Crawl entrance / Crawl far entrance. 진입 후 S/마침표(솔로 S/D)를 교대로 누른다. 통로 안에서 손 키를 다시 누르면 자세를 유지하고 출구 안내를 표시한다. 실제 창에서 곰돌이·천장이 보이는 것과 중간 해제 차단, 반대쪽 진입을 확인했다. Engine/Renderer와 기획 원본은 수정하지 않았다.

## 곰돌이 점프스케어: 촉수 정면 구도

복셀 곰돌이의 잡힘 구도는 얼굴 밀착 대신 벌어진 몸/촉수 앞에서 조준점 기준 420cm 거리를 유지한다. 조준점은 몸 높이 225cm, 전방 35cm이며 화각은 54도로 고정한다. 잡힘 시작에 외형을 완전히 열어 촉수가 즉시 보이게 하고, 미리보기에서도 잡힘 동안 열린 상태를 유지한다. 촉수가 렌즈로 접근하는 동작이나 카메라 끌림 없이 카메라만 강하게 흔든다. 좌우/상하 이동과 제한된 pitch/yaw/roll 흔들림으로 곰돌이가 화면 중앙 근처에 남게 한다. 기존 1.8초 연출·암전·체크포인트 복귀는 유지한다.

Prototype.json의 caughtTentacleDistance, caughtTentacleAimHeight, caughtTentacleFov로 구도를 조정한다. caughtTentacleMinimumDistance는 이전 구도와의 설정 호환용이며 현재 고정 구도에서는 사용하지 않는다. caughtShakeStrength(기본 14cm)와 caughtShakeAngle(기본 8도)로 흔들림을 조정한다. 시작 거리보다 최소 거리가 크면 설정을 거부한다. 기본 방향이 벽에 막히면 주변 방향 중 여유 있는 촬영 방향을 우선 선택한다. 전용 조명의 범위도 늘려 떨어진 카메라에서 몸이 어두워지지 않도록 했다. 가져온 SCISSORMAN은 기존 머리 추적/밀착 구도를 유지한다. CaughtCameraComponent는 모델 이름 대신 선택적 CaughtCameraFraming 값만 받는다.

검증: Release 빌드와 자동 검사 147개 통과. 여러 방향과 연출 시점에서 고정 거리·화각, 중앙 구도와 일시정지 중 흔들림 재현을 확인한다.

## 실행 배포본 만들기

게임을 종료한 뒤 `Tools/AssetPipeline/UpdateBabyDoll.ps1`을 실행한다. Release 빌드와 자동 검사, 임시 배포본 검증을 마친 뒤 `Binaries/Packages/BabyDoll_Latest/`와 `BabyDoll_Latest.zip`을 갱신한다. 이전 BabyDoll 배포 폴더/ZIP과 `Binaries/v143/x64`의 Release 이외 출력, 불필요한 PDB/보조 EXE 및 예전 임시 빌드 명령 파일은 제거한다. 삭제/이동은 검증된 출력 경로 안에서만 수행한다. 가장 최근 체크포인트는 로컬 최신 실행 폴더에 보존하고 배포 ZIP에는 포함하지 않는다. 소스·에셋·정규 Intermediate 빌드 캐시는 유지한다. `PackageBabyDoll.ps1`은 갱신 스크립트 내부의 배포 조립용으로 사용한다.

ZIP 전체를 풀고 `Play.cmd`를 실행한다. EXE, DXC/VC 런타임 DLL, 런타임 셰이더, BabyDoll 콘텐츠와 라이브러리 고지를 포함한다. 소스·PDB·Bistro/에디터 에셋은 포함하지 않으므로 기본 BabyDoll 실행용이다. 현재 Paths 구현은 `Binaries/v143/x64/Release/SeonEngine.exe` 위치를 기준으로 루트를 계산하므로 EXE만 꺼내거나 폴더 구조를 바꾸면 안 된다. 대상은 Windows x64와 DirectX 12 GPU이며 다른 PC에서의 호환성 검사는 별도로 필요하다.

2026-11-27 검증: Release 빌드 성공, 배포 폴더의 실행 파일에서 자동 검사 88개 통과. 별도 배포 폴더에서 실제 시작 화면과 맵·SCISSORMAN·전등·UI 로딩을 확인했다. 엔진과 렌더러 코드는 변경하지 않았다.

체크포인트 재시작 수정 검증: Release 빌드 및 자동 검사 155개 통과. 저장 당시 추격/포효/예약 소리, 높은 심박, 근접 AI 배치와 안전한 저장 위치 유지, 새 소리 반응, 다른 층 순찰, 재배치 불가능한 지형을 확인했다.

사다리 하강 카메라 수정 검증: Release 빌드 및 배포본 자동 검사 160개 통과. 하강 높이 추적, 상층 바닥/천장 주변 시야와 몸체 구도, 일시정지 중 위치 유지 및 사다리 이탈 후 Orbit 복귀를 확인한다.

사다리 방향/후방 구도 수정 검증: Release 및 최신 배포본 각각 자동 검사 167개 통과. 상승·하강, 회전 배치, 사다리 정면 유지, 후방 구도와 개구부 시야/화각을 확인한다. 이후 배포 갱신은 UpdateBabyDoll.ps1로 단일 최신 경로를 재사용한다.

## 전시관 그레이박스 시연

시작 구역 → 꺾이는 전시 통로 → 서쪽 열쇠 구역 → 잠긴 문 → 상층 엘리베이터. 칸막이는 스티치와 다음 구역을 일찍 볼 수 없게 하고, 테이블과 낮은 통로는 기존 회피 동작을 검증하는 데 쓴다. 계단과 사다리는 상층 이동 선택지다. 배치는 검토용이며 기존 저장의 좌표를 사용하지 않도록 맵 ID를 분리했다.

시작 또는 일시정지 메뉴의 Layout overview (for review)를 누르면 지붕을 치운 2m 격자 조감도가 열린다. 시뮬레이션은 일시정지 상태다. Return to gameplay view에서 원래 조명을 복원한다. GrayboxLayout.h에서 칸막이와 전등 위치를 조정하고 Prototype.json에서 주변광·방향광·전등 세기와 범위를 조정한다. 전등 근접 반응 반경은 15m다.

빠른 입력이 끊기면 관측한 입력 간격에 맞춰 감속을 일찍 시작한다. 느린 교대 보행의 최소 속도와 같은 발 반복 실수는 유지한다. 카메라 방향으로 즉시 조향하고 이동 속도는 보간한다.
