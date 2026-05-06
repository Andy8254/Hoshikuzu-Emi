# /settings
언어, 보조 표시, 스태프 역할, 서버 관리 로그를 위한 서버 전체 봇 설정입니다.

## 사용 대상
서버 소유자, 최고 관리자, 설정된 스태프 역할입니다. 봇은 설정을 쓰기 전에 권한을 확인합니다.

## 흐름
역할을 먼저 설정한 뒤 언어와 로그를 설정합니다. 변경 후 `/settings show`로 확인하세요.

## 명령어
- `show`: 현재 서버 설정을 표시합니다.
- `set_admin_role`: 서버 최고 관리자 역할을 설정합니다.
- `set_moderator_role`: 서버 관리자 역할을 설정합니다.
- `set_staff_role`: 서버 스태프 역할을 설정합니다.
- `language`: 서버 기본 언어를 설정합니다.
- `secondary_language`: 서버 보조 표시 언어를 설정하거나 해제합니다.
- `modlog_set`: 서버 관리 로그 채널을 설정합니다.
- `modlog_clear`: 서버 관리 로그 채널을 해제합니다.

## 도움말
명령어별 페이지는 `/bot help category:settings command:<command>`를 사용하세요.
