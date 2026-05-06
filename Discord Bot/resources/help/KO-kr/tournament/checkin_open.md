# /tournament checkin_open
체크인을 열고 체크인 패널을 게시합니다.

## 사용법
`/tournament checkin_open id:<id> closes_at:<unix> [grace_time]`

## 설명
플레이어는 Check in 버튼을 누르면 됩니다. 봇은 참가 신청 때 등록된 유저네임을 자동으로 사용합니다.

## 참고
`closes_at`은 일반 체크인이 마감되는 Unix 타임스탬프입니다.

`grace_time`은 `closes_at` 이후에 허용되는 추가 지각 체크인 시간이며, 단위는 초입니다. 기본 체크인 지속 시간이 아닙니다.

체크인을 개시 시점부터 1시간 동안만 열고 지각 체크인을 허용하지 않으려면, `closes_at`을 개시 시점으로부터 1시간 뒤로 설정하고 `grace_time`을 `0`으로 설정하세요.

체크인을 1시간 동안 열고 지각 체크인을 10분 허용하려면, `closes_at`을 개시 시점으로부터 1시간 뒤로 설정하고 `grace_time`을 `600`으로 설정하세요.
