# Hami/Emi 봇 도움말
토너먼트 운영, 플레이어 프로필, 서버 관리 도구, 서버 설정이 적은 수의 슬래시 명령어로 묶여 있습니다.

## 주요 명령어
- `/bot`: 도움말, 상태 확인, 봇 정보, 개인정보 안내.
- `/profile`: 플레이어 프로필, 연결된 계정, 개인 언어, TETR.IO 조회.
- `/settings`: 서버 역할, 언어 설정, 서버 관리 로그 경로.
- `/mod`: 경고, 메모, 기록, 타임아웃, 추방, 차단, 차단 해제.
- `/tournament`: 토너먼트 생성, 참가 신청, 체크인, 대진 운영, 패널, 이벤트 설정.

## 토너먼트 하위 그룹
- `/tournament bracket`: 경기 생성, 결과 보고, 정정, 노쇼 처리, 방송 배정, 순위표, SVG 내보내기.
- `/tournament config`: 이벤트 역할, 채널, 감사 로그, 형식, 규칙 세트.

## 플레이어 흐름
플레이어는 가능하면 버튼을 사용합니다: 참가 신청 패널, 체크인 패널, 경기 화면 버튼. 슬래시 명령어는 스태프 복구와 감사 가능한 운영을 위해 남아 있습니다.

## 언어
도움말 페이지는 `resources/help/<language>/<module>/<command>.md`에서 불러옵니다. 페이지가 없거나 비어 있으면 `EN-gb` 텍스트로 대체됩니다.

## 예시
- `/bot help category:tournament`
- `/bot help category:tournament_bracket command:report`
- `/bot help category:settings command:secondary_language`
