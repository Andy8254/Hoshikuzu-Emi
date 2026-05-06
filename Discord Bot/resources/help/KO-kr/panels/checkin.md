# 체크인 패널
체크인과 경기 동작 패널을 설명합니다.

## 사용법
`/tournament checkin_open id:<id> closes_at:<unix> [grace_time]`

## 설명
체크인 패널은 버튼 하나로 출석을 확인합니다. 경기 스레드에는 Check in, Report Score, Forfeit, Call Staff 버튼이 표시됩니다.

## 참고
패널은 입력을 줄여 줍니다. 스태프용 슬래시 명령어는 복구와 감사 가능한 운영 경로로 남겨 둡니다.

`closes_at`은 일반 체크인이 마감되는 시점을 정합니다. `grace_time`은 그 이후에 허용되는 추가 지각 체크인 시간이며, 단위는 초입니다.

예: 체크인을 개시 시점부터 1시간 동안 열려면 `closes_at`을 개시 시점 + 3600초로 설정하세요. 정확히 1시간 뒤에 닫으려면 `grace_time:0`, 지각 체크인을 10분 허용하려면 `grace_time:600`을 사용하세요.
