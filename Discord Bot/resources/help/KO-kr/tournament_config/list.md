# /tournament config
채널, 감사 로그, 역할, 기본 형식, 경기 규칙 세트 등 토너먼트 설정을 위한 도구를 제공합니다.
## 사용 대상
이 커맨드는 토너먼트 최고 관리자와 신뢰할 수 있는 스태프가 사용할 수 있습니다.
아래에 있는 명령어들은 이벤트 운영 방식에 영향을 줄 수 있습니다. 이 점 유념해 주시기 바랍니다.
## 사용 방법
패널/로그 채널과 역할을 먼저 설정한 뒤, 경기 생성 전에 형식과 규칙 세트를 설정합니다.
## 명령어
- `roles`: 토너먼트 역할 설정을 표시합니다.
- `set_staff_role`: 토너먼트 스태프 역할을 설정합니다.
- `set_admin_role`: 토너먼트 최고 관리자 역할을 설정합니다.
- `clear_staff_role`: 토너먼트 스태프 역할을 해제합니다.
- `clear_admin_role`: 토너먼트 최고 관리자 역할을 해제합니다.
- `set_channel`: 토너먼트 패널 채널을 설정합니다.
- `clear_channel`: 토너먼트 패널 채널을 해제합니다.
- `log_channel_assign`: 토너먼트 로그 채널을 설정합니다.
- `log_channel_clear`: 토너먼트 로그 채널을 해제합니다.
- `set_format`: 토너먼트 기본 형식을 설정합니다.
- `ruleset_show`: 토너먼트의 전체적인 규칙을 표시합니다.
- `ruleset_set_primary`: 지정된 토너먼트의 첫번째 규칙(풀 매치 - Pool Match)을 설정합니다.
- `ruleset_set_secondary`: 지정된 토너먼트의 두번째 규칙을 설정합니다. 이는 주로 TOP 8 또는 결승전에서 적용됩니다.
- `ruleset_clear_secondary`: 토너먼트의 두번째 규칙을 비활성화합니다.
## 도움말
주어진 명령어에 대한 자세한 정보는 `/bot help category:tournament_config command:<command>`를 통해 확인하실 수 있습니다.
