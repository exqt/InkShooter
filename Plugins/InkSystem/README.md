# InkSystem

UE 5.8용 표면 샘플링 및 렌더 타깃 기반 잉크 페인팅 플러그인.

## 프로젝트 설정

- `InkShooter.uproject`에서 플러그인을 활성화하고 게임 모듈에 `InkSystem` 의존성을 추가했습니다.
- `DefaultEngine.ini`의 `bSupportUVFromHitResults=True`가 필요합니다. 설정 변경 후 에디터를 재시작해야 합니다.
- `Ink Paintable Volume`의 `Render Settings`에서 `Brush Material`, `Normal Brush Material`, `Overlay Material`을 직접 지정합니다. 포함된 머티리얼은 각각 `M_InkRTBrush`, `M_InkRTNormalBrush`, `M_InkPaintOverlay`입니다.

## 사용

1. 월드에 `Ink Paintable Volume`을 하나 배치합니다. 먼저 등록된 볼륨만 사용하며, 추가 볼륨은 경고를 출력하고 무시합니다. 서브시스템의 `GetPaintableVolume()`으로 접근할 수 있습니다.
2. UV0과 복잡 충돌이 있는 Static 메시 액터에 `Ink Paintable Component`를 추가합니다. 메시는 `Visibility` 채널을 Block해야 합니다. 예제는 `/InkSystem/Materials/BP_PaintableActor`입니다.
3. 각 컴포넌트에서 `Rebuild Samples`를 실행하고 레벨을 저장합니다. 에디터에서 액터를 이동·회전·스케일 변경하면 마지막 이동 이벤트로부터 0.2초 후 자동으로 다시 샘플링합니다. 메시 자체를 변경한 경우에는 버튼으로 다시 샘플링합니다.
4. `InkProjectile`은 서버의 Hit 처리에서 볼륨의 `Paint()`를 호출합니다. 반경은 투사체 Blueprint의 `Paint Radius`로 지정하며 기본값은 100cm입니다. 서버가 발사자의 `InkShooterPlayerState.TeamId`(0~2)를 읽어 투사체 스폰 완료 전에 `PaintColorId = TeamId + 1`을 지정합니다. 팀 0·1·2는 각각 빨강·초록·파랑이며, 색상 ID 0은 미도색입니다. 색상 ID는 클라이언트에도 초기 복제됩니다. 투사체 효과의 `Ink Color`는 별도 설정입니다. `OnInkImpact`는 도색 이후 추가 처리를 위한 이벤트입니다.
5. `Ink.ShowSamples 1`로 샘플을 표시하고 `Ink.LogStatistics`로 도색 개수를 확인합니다.

멀티플레이에서는 칠할 액터의 Replicates도 활성화해야 합니다. 색상은 현재 RGB 고정 팔레트이며, 시각적 브러시는 512×512 렌더 타깃에 64×64 픽셀로 찍힙니다. `Radius`는 월드 공간의 샘플 판정 반경입니다. 늦게 접속한 클라이언트를 위한 기존 도색 복원은 구현되어 있지 않습니다.

## 바닥 잉크 이동

`InkShooterCharacter`의 `Ink Sample Detector`가 실제 지지 바닥 주변의 샘플 색상 비율을 조회합니다. 기본 감지 반경은 75cm이며, 바닥 법선 유사도 0.9 이상이고 바닥 평면에서 10cm 이내인 샘플만 계산합니다. 미도색·적군 샘플도 분모에 포함하고, 샘플이 없거나 공중이면 비율은 0입니다.

`InkShooterMovementComponent`는 매 이동 계산 전에 서버와 클라이언트에서 아군 색상(`TeamId + 1`)과 다른 팀 색상의 비율을 갱신합니다. 지상에서 아군 비율이 `Minimum Team Ink Ratio` 0.4 이상이면 오징어 형태에만 `Team Ink Speed Multiplier` 1.5배를 적용하고 인간 형태는 기본 속도를 사용합니다. 다른 팀 비율이 `Minimum Other Ink Ratio` 0.4 이상이면 두 형태 모두 `Other Ink Speed Multiplier` 0.5배입니다. 두 조건을 동시에 만족하면 아군 잉크를 우선합니다. 미도색·공중에서는 기본 속도를 사용합니다. BP에서 `Is On Team Ink`, `Team Ink Ratio`, `Is On Other Ink`, `Other Ink Ratio`를 확인할 수 있습니다.
