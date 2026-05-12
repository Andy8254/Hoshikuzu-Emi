# Beta1 토너먼트 운영 가이드

이 문서는 beta1 빌드로 실제 토너먼트를 운영할 때 스태프가 참고하는 운영 플로우와 문제 해결 가이드입니다. beta1이 실전 검증을 충분히 마치기 전까지는 Challonge, Battlefy, 스프레드시트 같은 예비 브래킷을 반드시 준비해 두세요.

## 스태프 역할

- 토너먼트 관리자: 토너먼트 생성, 역할/채널 설정, 룰 설정, 결과 정정, 긴급 조치 승인.
- 토너먼트 스태프: 참가 신청, 체크인, 시드 검토, 매치 스레드, 결과 입력, 기권/실격, 노쇼 처리, 플레이어 지원.
- 방송 스태프: 방송 매치 배정과 현재 매치 상태 확인.
- 백업 기록 담당: 시드, 브래킷 상태, 완료된 경기 결과를 외부 문서에도 기록.

## 이벤트 전 준비

### 1. 실행 중인 봇 확인

등록을 열기 전에 의도한 봇 빌드가 온라인인지 확인합니다.

- stable과 canary는 환경 변수와 토큰으로 구분합니다.
- beta 테스트 이벤트는 특별한 이유가 없다면 canary를 우선 사용합니다.
- `/codex ping` 또는 `/codex info`에 정상 응답하는지 확인합니다.

Discord에서 명령어가 보이지 않으면 봇을 재시작하고 길드 명령어 등록이 끝날 때까지 잠시 기다립니다.

### 2. 스태프 권한 설정

서버당 한 번, 또는 이벤트 전 역할이 바뀐 경우 실행합니다.

```text
/tournament config set_staff_role role:<스태프 역할>
/tournament config set_admin_role role:<관리자 역할>
/tournament config roles
```

확인할 점:

- 스태프 역할이 토너먼트 운영 명령어를 사용할 수 있어야 합니다.
- 관리자 역할은 결과 정정 같은 고위험 명령어를 사용할 수 있어야 합니다.
- `/tournament config roles`에 의도한 역할이 표시되어야 합니다.

### 3. 채널 설정

토너먼트 진행 채널과 감사 로그 채널을 설정합니다.

```text
/tournament config set_channel channel:<등록/체크인 채널>
/tournament config log_channel_assign channel:<비공개 스태프 로그 채널>
```

로그 채널에서는 참가 신청, 체크인, 결과 입력, 기권, 노쇼 처리, 스태프 호출을 확인합니다.

### 4. 토너먼트 생성

의도한 게임과 형식으로 토너먼트를 생성합니다.

```text
/tournament create name:<이벤트 이름> game:<게임> format:<형식>
```

생성 후 확인합니다.

```text
/tournament info id:<id>
/tournament staff_info id:<id>
```

토너먼트 ID는 스태프 채널에 고정하거나 별도 메모로 남겨 둡니다. 대부분의 명령어에 필요합니다.

### 5. 게임별 제한 또는 레이팅 설정

TETR.IO 이벤트에서는 필요한 경우 제한을 설정합니다.

```text
/tournament config tetrio_restrictions id:<id> current_rank_min:<랭크> current_rank_max:<랭크> tr_min:<값> tr_max:<값> allow_unranked:<true|false>
```

TE:C 또는 PPT2처럼 수동 레이팅을 쓰는 이벤트에서는 시드 전에 점수를 입력합니다.

```text
/tournament config rating_set id:<id> user:<플레이어> bucket:<버킷> points:<점수> note:<선택 메모>
/tournament config rating_list id:<id> bucket:<버킷>
```

기본 수동 레이팅 버킷:

- TE:C: overall, connected VS, zone battle, score attack, classical score attack.
- PPT2: puzzle, puyo puyo, tetris.
- General: single rank point.

### 6. 매치 룰 설정

현재 룰을 확인합니다.

```text
/tournament config ruleset_show id:<id>
```

기본 룰을 설정합니다.

```text
/tournament config ruleset_set_primary id:<id> first_to:<점수> deuce:<방식> win_by:<차이> score_cap:<상한> allow_draw:<true|false>
```

Top 8 또는 그랜드 파이널에서 다른 룰을 적용해야 하면 보조 룰을 설정합니다.

```text
/tournament config ruleset_set_secondary id:<id> trigger:<top8|grand_finals> first_to:<점수> deuce:<방식> win_by:<차이> score_cap:<상한> allow_draw:<true|false>
```

점수 상한이 없으면 `score_cap:0`을 사용합니다.

## 참가 신청 플로우

### 1. 참가 신청 열기

```text
/tournament registration_open id:<id>
```

플레이어는 직접 신청합니다.

```text
/tournament register id:<id> username:<유저명>
```

스태프는 플레이어를 대신 등록할 수 있습니다.

```text
/tournament register id:<id> user:<플레이어> username:<유저명>
```

신청 취소는 `abort:true`를 사용합니다.

### 2. 참가자 모니터링

```text
/tournament participants id:<id>
/tournament staff_info id:<id>
```

중복 유저명, 누락된 유저명, 잘못된 게임 계정, 명백한 레이팅 입력 오류를 확인합니다.

### 3. 참가 신청 닫기

```text
/tournament registration_close id:<id>
```

닫은 후에는 가볍게 다시 열지 않습니다. 늦은 참가를 허용해야 한다면 스태프 결정 내용을 로그에 남깁니다.

## 체크인 플로우

### 1. 체크인 열기

마감 시간은 Unix timestamp로 입력합니다.

```text
/tournament checkin_open id:<id> closes_at:<unix timestamp> grace_time:<초>
```

플레이어는 직접 체크인합니다.

```text
/tournament checkin id:<id> username:<유저명>
```

스태프는 플레이어를 대신 체크인할 수 있습니다.

```text
/tournament checkin id:<id> user:<플레이어> username:<유저명>
```

체크인 취소는 `abort:true`를 사용합니다.

### 2. 체크인 닫기

```text
/tournament checkin_close id:<id>
```

그 다음 상태를 확인합니다.

```text
/tournament participants id:<id>
/tournament staff_info id:<id>
```

시드에는 체크인한 플레이어만 포함됩니다.

## 시드 플로우

### 빠른 적용

일반적인 테스트나 재량 조정이 필요 없는 경우:

```text
/tournament seed id:<id> mode:<general|tetrio|rating> bucket:<rating일 때 버킷>
```

이 명령어는 즉시 시드를 적용합니다.

### CSV 검토 플로우

실제 beta1 토너먼트에서는 이 방식을 권장합니다.

```text
/tournament seed_export id:<id> mode:<general|tetrio|rating> bucket:<rating일 때 버킷>
```

스태프 작업 순서:

1. CSV를 다운로드합니다.
2. 플레이어 행의 순서를 조정합니다.
3. 유저명은 수정하지 않습니다.
4. 수정한 CSV를 업로드합니다.

```text
/tournament seed_import id:<id> file:<csv 첨부파일>
```

Import 규칙:

- CSV 행 순서가 최종 시드 순서가 됩니다.
- `seed_export`로 받은 전체 CSV를 그대로 사용할 수 있습니다.
- 유저명만 있는 CSV도 사용할 수 있습니다.
- 유저명 목록은 체크인한 참가자와 정확히 일치해야 합니다.
- 검증에 실패하면 시드는 적용되지 않습니다.

Import 후 확인합니다.

```text
/tournament participants id:<id>
```

## 브래킷 플로우

### 1. 브래킷 생성

```text
/tournament bracket generate id:<id> type:<선택 형식>
```

최종 시드가 확정된 뒤에만 생성합니다.

### 2. 현재 매치 확인

```text
/tournament bracket current id:<id>
/tournament bracket round id:<id> round:<라운드 번호>
/tournament bracket match id:<id> match_id:<매치 ID>
```

### 3. 매치 스레드 생성

```text
/tournament bracket threads id:<id> buttons:<true|false>
```

특정 라운드만 생성할 때:

```text
/tournament bracket threads id:<id> round:<라운드 번호> buttons:<true|false>
```

플레이어가 스레드 UI에서 체크인하거나 결과를 보고해야 한다면 `buttons:true`를 사용합니다.

### 4. 결과 입력

```text
/tournament bracket report id:<id> match_id:<매치 ID> score_a:<점수> score_b:<점수>
```

완료된 결과가 잘못되었고 이후 상태가 정정을 허용하는 경우:

```text
/tournament bracket correct_report id:<id> match_id:<매치 ID> score_a:<점수> score_b:<점수> confirm:CORRECT
```

정정은 관리자 승인 후 사용합니다.

### 5. 기권과 노쇼 처리

직접 기권 또는 실격을 기록할 때:

```text
/tournament bracket forfeit id:<id> match_id:<매치 ID> player:<플레이어> reason:<사유>
```

마감된 노쇼 상태를 처리할 때:

```text
/tournament bracket resolve_no_shows id:<id>
```

노쇼 처리 후에는 반드시 로그 채널을 확인합니다.

### 6. 방송 매치 배정

```text
/tournament bracket stream_assign id:<id> match_id:<매치 ID>
/tournament bracket stream_list id:<id>
/tournament bracket stream_clear id:<id> match_id:<매치 ID>
```

### 7. 공개 출력물

```text
/tournament bracket standings id:<id>
/tournament bracket svg id:<id>
/tournament bracket match_svg id:<id> match_id:<매치 ID>
```

SVG는 스태프 검토, 공지, 방송 자료로 사용할 수 있습니다.

## 라이브 이벤트 체크리스트

브래킷 시작 전:

- 봇 빌드와 토큰 확인.
- 스태프/관리자 역할 확인.
- 토너먼트 채널과 로그 채널 설정 완료.
- 토너먼트 ID를 스태프 채널에 고정.
- 참가 신청 마감.
- 체크인 마감.
- 수동 레이팅 사용 시 입력 완료.
- 시드 CSV 백업 완료.
- 필요 시 검토된 시드 CSV import 완료.
- 최종 시드 이후 브래킷 생성.
- 현재 매치와 매치 스레드 확인.
- 외부 예비 브래킷 또는 스프레드시트 준비.

브래킷 진행 중:

- 검증된 결과만 입력합니다.
- 분쟁이 있으면 `current`, `round`, `match`로 상태를 먼저 확인합니다.
- 수동 판단은 스태프 채널에 기록합니다.
- 플레이어가 도움이 필요하면 `call_staff`를 사용합니다.
- 정정은 최소한으로 사용하고, 사용 후 즉시 브래킷 상태를 확인합니다.
- 주요 진행 시점마다 SVG를 백업합니다.

이벤트 종료 후:

- 최종 브래킷 SVG를 저장합니다.
- 최종 순위 또는 결과 메시지를 남깁니다.
- beta 피드백용 미해결 이슈를 기록합니다.
- 이벤트 데이터가 더 이상 필요하지 않을 때만 `/tournament clear`를 사용합니다.

## 문제 해결

### Slash 명령어가 보이지 않음

가능한 원인:

- 명령어 변경 후 봇을 재시작하지 않음.
- 길드 명령어 등록이 아직 끝나지 않음.
- 잘못된 봇 빌드나 토큰이 온라인 상태.

조치:

1. stable/canary 봇 신원을 확인합니다.
2. 의도한 봇을 재시작합니다.
3. `/codex ping`을 확인합니다.
4. Discord 명령어 갱신을 잠시 기다립니다.

### 스태프가 토너먼트 명령어를 사용할 수 없음

가능한 원인:

- 토너먼트 스태프 역할이 설정되지 않음.
- 해당 유저에게 스태프/관리자 역할이 없음.
- Discord 역할 구조나 권한이 변경됨.

조치:

```text
/tournament config roles
/tournament config set_staff_role role:<스태프 역할>
/tournament config set_admin_role role:<관리자 역할>
```

이후 다시 명령어를 실행하게 합니다.

### Tournament not found

가능한 원인:

- 토너먼트 ID가 틀림.
- 토너먼트가 삭제됨.
- 데이터가 clear됨.
- 다른 테스트에서 사용한 ID를 보고 있음.

조치:

1. 스태프 채널에 고정된 ID를 확인합니다.
2. `/tournament staff_info id:<id>`를 실행합니다.
3. 찾을 수 없으면 새 토너먼트를 생성하거나 백업 기록으로 전환합니다.

### 플레이어가 참가 신청을 못 함

가능한 원인:

- 참가 신청이 열려 있지 않음.
- 이미 등록되어 있음.
- 유저명이 없거나 운영 방식에 맞지 않음.
- TETR.IO 제한에 걸림.

조치:

1. `/tournament info id:<id>`를 확인합니다.
2. `/tournament participants id:<id>`를 확인합니다.
3. 스태프가 승인하면 `/tournament register id:<id> user:<플레이어> username:<유저명>`으로 수동 등록합니다.

### 플레이어가 체크인을 못 함

가능한 원인:

- 체크인이 열려 있지 않음.
- 등록되지 않은 플레이어.
- 체크인 시간이 종료됨.
- 등록 때와 다른 유저명을 사용함.

조치:

1. `/tournament participants id:<id>`를 확인합니다.
2. 스태프가 승인하면 `/tournament checkin id:<id> user:<플레이어> username:<유저명>`으로 수동 체크인합니다.
3. 참가시키지 않을 플레이어는 체크인하지 않은 상태로 둡니다.

### TETR.IO 프로필 또는 시드 데이터가 이상함

가능한 원인:

- TETR.IO API가 불완전한 데이터를 반환함.
- 플레이어가 unranked 상태.
- 유저명을 잘못 입력함.
- 제한 설정 때문에 제외됨.

조치:

1. `/profile tetrio username:<이름>`으로 확인합니다.
2. `/tournament config tetrio_restrictions` 설정을 재확인합니다.
3. 최종 판단이 필요하면 CSV 시드 플로우를 사용합니다.
4. TETR.IO가 아닌 이벤트는 수동 레이팅 버킷을 사용합니다.

### 수동 레이팅 시드에서 플레이어가 제외됨

가능한 원인:

- 선택한 버킷의 레이팅 점수가 없음.
- 게임과 맞지 않는 버킷을 선택함.
- 플레이어가 체크인하지 않음.

조치:

```text
/tournament config rating_list id:<id> bucket:<버킷>
/tournament config rating_set id:<id> user:<플레이어> bucket:<버킷> points:<점수>
/tournament participants id:<id>
```

이후 `seed_export` 또는 `seed`를 다시 실행합니다.

### Seed import 실패

가능한 원인:

- CSV에 체크인하지 않은 유저명이 있음.
- CSV에서 체크인한 참가자가 빠짐.
- CSV에 중복 유저명이 있음.
- 행 순서만 바꿔야 하는데 유저명을 수정함.
- 토너먼트 ID가 틀림.

조치:

1. `/tournament seed_export`로 새 CSV를 받습니다.
2. 행 순서만 바꿉니다.
3. 유저명은 수정하지 않습니다.
4. 다시 import합니다.

유저명만 있는 CSV도 가능하지만, 유저명 목록은 체크인 참가자와 정확히 일치해야 합니다.

### 브래킷 생성 실패 또는 브래킷이 이상함

가능한 원인:

- 시드가 적용되지 않음.
- 체크인한 플레이어가 너무 적음.
- 형식을 잘못 선택함.
- 최종 시드 import 전에 브래킷을 생성함.

조치:

1. 참가자와 시드를 확인합니다.
2. 필요하면 시드 export/import를 다시 진행합니다.
3. 최종 시드가 확정된 뒤에만 브래킷을 생성합니다.
4. 실제 이벤트 중 잘못 생성했다면, 재생성이 안전하다는 스태프 합의가 없는 한 예비 브래킷으로 전환합니다.

### Match report 실패

가능한 원인:

- 매치가 현재 보고 가능한 상태가 아님.
- 매치 ID가 틀림.
- 점수가 룰셋과 맞지 않음.
- 이미 완료된 매치임.

조치:

```text
/tournament bracket current id:<id>
/tournament bracket match id:<id> match_id:<매치 ID>
/tournament config ruleset_show id:<id>
```

점수가 맞는데도 거부되면 결과를 외부 기록에 남기고 스태프 에스컬레이션으로 처리합니다.

### 잘못된 점수가 입력됨

조치:

1. 영향을 받는 하위 매치의 결과 입력을 잠시 멈춥니다.
2. 양 플레이어 또는 스태프 증거로 올바른 점수를 확인합니다.
3. 다음 명령어를 사용합니다.

```text
/tournament bracket correct_report id:<id> match_id:<매치 ID> score_a:<점수> score_b:<점수> confirm:CORRECT
```

4. `/tournament bracket current`와 `/tournament bracket match`로 확인합니다.

이미 하위 매치가 진행되어 정정이 실패하면 봇 상태를 보존하고 백업 운영으로 전환합니다.

### 노쇼 처리가 이상함

조치:

1. 매치 스레드 메시지와 버튼 상태를 확인합니다.
2. grace time을 확인합니다.
3. `/tournament bracket match id:<id> match_id:<매치 ID>`를 실행합니다.
4. 상태가 처리 가능한 시점일 때만 `/tournament bracket resolve_no_shows id:<id>`를 사용합니다.
5. 분쟁이 있으면 해당 매치 그룹을 멈추고 스태프 판단으로 처리합니다.

### 매치 스레드가 생성되지 않음

가능한 원인:

- 브래킷이 생성되지 않음.
- 현재 매치가 없음.
- 봇에게 스레드 권한이 없음.
- 이미 다른 위치에 스레드가 생성됨.

조치:

```text
/tournament bracket current id:<id>
/tournament bracket threads id:<id> buttons:true
```

스레드가 계속 생성되지 않으면 Discord 권한을 확인합니다.

### SVG export 실패

가능한 원인:

- 브래킷이 없음.
- 매치 ID가 틀림.
- SVG 생성기가 예상하지 못한 브래킷 상태를 만남.

조치:

1. `/tournament bracket current`를 확인합니다.
2. `/tournament bracket svg id:<id>`를 시도합니다.
3. 단일 매치 SVG는 매치 ID를 먼저 확인한 뒤 `/tournament bracket match_svg`를 사용합니다.
4. 계속 실패하면 텍스트 순위나 예비 브래킷을 사용합니다.

### 봇이 느리거나 응답하지 않음

조치:

1. 같은 명령어를 빠르게 반복하지 않습니다.
2. Discord 자체 지연인지 확인합니다.
3. 봇 프로세스가 살아 있는지 확인합니다.
4. 기다리는 동안 결과는 외부 문서에 계속 기록합니다.
5. 봇이 복구되면 한 매치씩 상태를 맞춥니다.

### 긴급 백업 전환

다음 상황에서는 백업 운영으로 전환합니다.

- 브래킷 진행 중 명령어가 사라짐.
- 브래킷 상태가 불가능한 상태가 됨.
- 잘못된 결과를 정정으로 안전하게 복구할 수 없음.
- 완료된 여러 매치를 봇에 입력할 수 없음.
- 스태프가 시드 또는 참가자 상태를 검증할 수 없음.

전환 절차:

1. 스태프 채널에 백업 운영 전환을 공지합니다.
2. 가능하면 현재 봇 상태를 export하거나 스크린샷으로 남깁니다.
3. 예비 브래킷 또는 스프레드시트로 이벤트를 계속 진행합니다.
4. 상태를 변경하는 봇 명령어 사용을 중단합니다.
5. 이벤트 후 beta 디버깅을 위해 로그를 보관합니다.

## 이벤트 종료 보고 템플릿

이벤트 후 아래 형식으로 기록합니다.

```text
이벤트:
날짜:
봇 빌드:
stable 또는 canary:
토너먼트 ID:
형식:
참가 신청 수:
체크인 수:
시드 방식:
브래킷 생성 시각:
발생 이슈:
수동 개입:
백업 운영 사용 여부:
실패한 명령어:
기대 동작:
실제 동작:
스크린샷/로그 링크:
```
