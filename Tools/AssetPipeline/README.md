# AssetPipeline

FBX를 현재 SeonEngine에서 읽을 수 있는 외부 리소스 glTF 2.0으로 변환합니다. 엔진과 렌더러 소스, 게임 시작 맵은 변경하지 않습니다. Blender를 백그라운드로 실행하는 독립 콘텐츠 도구입니다.

## 실행

Blender 5.1에서 검증했습니다. Blender는 PATH 또는 `Program Files/Blender Foundation`에서 찾습니다. 다른 위치라면 `-Blender`로 실행 파일을 지정합니다. Python이나 pip 패키지는 별도로 설치하지 않습니다.

저장소 루트의 PowerShell에서 실행합니다.

```powershell
.\Tools\AssetPipeline\ConvertAsset.ps1 -Recipe .\Tools\AssetPipeline\Recipes\XBot.json
```

이미 도구가 생성한 자산을 재변환할 때만 `-Force`를 추가합니다. 이전 결과물의 해시가 바뀌었다면 `-Force`로도 덮어쓰지 않습니다. 수정본을 다른 위치에 보관한 뒤 다시 변환해야 합니다. 원본 FBX는 읽기만 합니다.

## 파일 배치

| 위치 | 역할 |
| --- | --- |
| `Scripts/convert_asset.py` | FBX import, 스킨 정리, 클립 연결, 제한적 리타깃, glTF export, 게시 |
| `Scripts/gltf_asset.py` | 외부 glTF 읽기, 클립 통합, 재질별 메시 분리, 호환성 검사 |
| `Profiles/SeonEngine.json` | 현재 엔진의 뼈/가중치/보간 제한 |
| `Recipes/*.json` | 원본 경로, 출력 위치, 클립 설정 |
| `Intermediate/AssetPipeline/<자산>/` | 실행별 임시 결과, 최신 `Report.json` |
| `Game/Content/<설정한 경로>/` | 검증에 통과한 최종 자산 |

출력에는 `<이름>.gltf`, `<이름>.bin`, 필요한 PNG 텍스처, `AssetDefinition.json`, `AssetManifest.json`이 포함됩니다. Definition은 클립 이름과 길이의 도구 메타데이터이며 기존 엔진이 자동으로 읽는 캐릭터 설정 파일은 아닙니다. Manifest에는 원본 SHA-256, 설정, Blender 버전, 검증 결과와 출력 해시를 기록합니다. 저장소 상대 경로는 `/`를 사용합니다.

출력 폴더는 `Game/Content` 하위여야 합니다. 검증 실패 시 기존 게시 결과는 유지됩니다. 검증 후 파일 복사 중 프로세스 종료나 디스크 오류에 대한 디렉터리 단위 트랜잭션은 제공하지 않습니다. 재변환 시 사용하지 않게 된 이전 파일은 자동 삭제하지 않습니다.

## 일반 Static/Skeletal FBX

```json
{
  "schemaVersion": 1,
  "name": "Prop",
  "source": "D:/TestAsset/Prop.fbx",
  "output": "Game/Content/Meshes/Prop",
  "sampleRate": 30,
  "includeSourceAnimations": true,
  "clips": []
}
```

원본 상대 경로는 recipe 파일 위치를 기준으로 해석합니다. 출력 경로는 저장소 루트 기준입니다. 도구를 다른 프로젝트로 옮길 때 원본 경로와 출력 설정을 변경합니다.

실제 armature 연결과 유효한 뼈 가중치가 있는 메시를 Skeletal로, 나머지는 Static으로 분류합니다. 한 FBX에 두 종류가 있으면 `mixed`로 보고합니다. 가중치가 전혀 없는 armature modifier는 제거하고 Static으로 내보냅니다. 일부 정점만 가중치가 없는 스킨 메시, 복수 armature modifier는 실패합니다.

다중 재질 메시의 primitive를 각각 독립 메시로 분리하고 원래 Transform/애니메이션 대상 노드 아래에 연결합니다. 현재 엔진이 메시마다 첫 primitive만 읽는 제한으로 면이 누락되는 것을 방지합니다. 스킨은 정점당 최대 4개 영향만 남기고 정규화합니다. 잘라낸 영향 수는 경고에 기록합니다.

## 외부 애니메이션 연결

기준 FBX의 메시와 스켈레톤을 한 번 내보낸 뒤, 외부 FBX의 활성 액션을 같은 타깃 리그에서 export하고 애니메이션 채널만 최종 파일에 합칩니다. 메시를 클립 수만큼 중복 저장하지 않습니다. 뼈 연결은 이름과 부모 계층으로 검사하며 원본 파일의 노드 번호를 그대로 교환하지 않습니다.

```json
{
  "schemaVersion": 1,
  "name": "Character",
  "source": "D:/Assets/Character.fbx",
  "output": "Game/Content/Characters/Character",
  "sampleRate": 30,
  "includeSourceAnimations": false,
  "clips": [
    {
      "name": "Walk",
      "source": "D:/Assets/Walk.fbx",
      "retarget": "none"
    }
  ]
}
```

외부 클립은 타깃 armature 하나와 외부 FBX의 활성 액션 하나를 지원합니다. 여러 take나 여러 활성 액션이 있으면 조용히 일부만 선택하지 않고 실패합니다. `action`으로 활성 액션 이름을 확인할 수 있지만 다른 take를 자동 선택하지는 않습니다. 기준 FBX의 활성 객체 애니메이션은 `includeSourceAnimations=true`일 때 포함하며, 비활성 take가 있으면 파일을 나누도록 안내합니다.

클립은 0초부터 시작하고 `sampleRate`에 맞춰 재샘플링하며 마지막 키도 유지합니다. FBX 자체 프레임률에 따라 import/export한 뒤 시간 단위로 재샘플링하므로 재생 시간을 변경하지 않습니다. 같은 노드의 TRS 채널은 동일 시각 배열을 사용하며 쿼터니언은 최단 경로 slerp로 보간합니다.

XBot recipe에는 다음 옵션을 명시했습니다.

- `allowRootRestTranslation=true`: 단일 루트 스켈레톤의 공통 기준 위치 차이를 허용합니다. 오프셋을 보고서에 남깁니다.
- `retarget="restRotation"`: 같은 이름/계층, 뼈 길이 차이 1% 이내에서 모델 공간 회전의 기준 자세 차이를 보정하고 타깃 뼈 길이를 유지해 베이크합니다. 기본값 `none`은 기준 행렬까지 일치해야 합니다.
- `inPlaceBone="mixamorig:Hips"`: 부모 변환을 반영한 glTF 월드 XZ 이동만 제거하고 골반 높이 변화는 보존합니다. 지정한 뼈의 조상이 애니메이션되면 실패합니다.

`restRotation`은 다른 체형/계층을 위한 범용 리타깃이나 IK/발 고정 기능이 아닙니다. 부모가 있는 armature, armature 객체 자체의 이동 애니메이션, 다른 뼈 비율은 별도 DCC 작업이 필요합니다. 변환 후 관절과 발 미끄러짐은 콘텐츠 측에서 시각 확인해야 합니다.

## 엔진 호환성과 제한

- 외부 `.bin`과 PNG를 참조하는 `.gltf`만 출력합니다. `.glb`, data URI, 압축/필수 확장, sparse accessor를 사용하지 않습니다.
- glTF의 표준 미터/Y-up 좌표계를 유지합니다. 엔진의 cm/왼손 변환을 중복 적용하지 않습니다.
- 인덱스가 있는 삼각형, POSITION/NORMAL과 float UV, 최대 512개 뼈, JOINTS_0/WEIGHTS_0만 사용합니다.
- LINEAR/float 키로 베이크하고 엔진이 무시하는 채널 시각 불일치를 검사합니다.
- 뼈 하위의 메시 객체는 현재 프리팹 생성에서 누락되므로 실패시킵니다.
- Shape key와 armature 외 modifier는 명시적 베이크가 필요하므로 실패시킵니다.
- 텍스처는 PNG와 ASCII 파일명으로 정리합니다. 원본 텍스처가 없으면 실패합니다.
- alpha MASK, doubleSided, 추가 재질 확장은 엔진에서 동일하게 표현되지 않을 수 있어 경고합니다. 임의의 FBX 셰이더나 절차적 재질을 PBR로 베이크하지 않습니다.

이 도구는 자산을 준비합니다. 이동/충돌/카메라/Idle-Walk-Run 블렌딩 구현은 포함하지 않습니다. 프로필 수치 변경만으로 엔진이 새 포맷을 지원하게 되지는 않습니다. 엔진이 바뀌면 export/validator 규칙도 함께 갱신해야 합니다.

## 검증

일반 Python 3.11 이상 또는 Blender 번들 Python으로 호환성 검사를 단독 실행할 수 있습니다.

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\5.1\python\bin\python.exe' .\Tools\AssetPipeline\Scripts\gltf_asset.py .\Game\Content\Characters\XBot\XBot.gltf
```

Blender 통합 테스트는 실제 FBX fixture를 만들어 Static/혼합/다중 재질/실패 보호/인플레이스/기준 자세 보정을 검증합니다. 결과는 `Intermediate/AssetPipeline/Tests/Results.json`에 남깁니다. 테스트 데이터는 `Intermediate` 안의 독립 워크스페이스에만 생성합니다.

```powershell
& 'C:\Program Files\Blender Foundation\Blender 5.1\blender.exe' --background --factory-startup --python-exit-code 1 --python .\Tools\AssetPipeline\Tests\run_integration.py
```

실제 엔진 로더 검증은 Visual Studio C++ 도구로 별도 probe를 빌드합니다. 엔진 프로젝트 파일과 소스는 변경하지 않으며 렌더러도 링크하지 않습니다.

```powershell
.\Tools\AssetPipeline\Tests\TestEngineImport.ps1 -Assets @('.\Game\Content\Characters\XBot\XBot.gltf')
```

probe 성공은 실제 엔진의 메시/스킨/클립 로딩과 중간 시각 샘플링 성공을 뜻합니다. 게임 화면의 조명, 재질 일치, GPU 스키닝, 캐릭터 조작감까지 검증한 것은 아닙니다.

Blender exporter 설정은 [Blender 5.1 API](https://docs.blender.org/api/5.1/bpy.ops.export_scene.html)를 기준으로 합니다.
