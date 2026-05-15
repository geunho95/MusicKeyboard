# MusicKeyboard — 할 일 목록

## 우선순위 높음

- [ ] **SD 카드 WAV 샘플 로드** — `f_mount()` + `storage_hal_load_project()` 정상 동작 확인, 실제 WAV 파일 재생 테스트
- [ ] **엔코더 BPM 조절 테스트** — ENC1(GP6/7) 돌릴 때 BPM 증감, LCD 상태바 반영 확인
- [ ] **버튼 물리 배치 검증** — 현재 논리 ID(0=C … 11=B, 12=PLAY, 13=REC, 14=MODE)와 실제 PCB 버튼 위치 일치 여부 확인

## 기능 개발

- [ ] **LCD UI 다중 화면** — 현재 PERF 뷰 하나뿐; SOUND SELECT / PATTERN / BPM / FX 뷰 추가
- [ ] **실제 WAV 샘플 재생** — fallback 스퀘어웨이브 → SD에서 로드한 샘플로 교체
- [ ] **녹음 → 재생 루프 end-to-end** — REC 버튼 → 루프 캡처 → 자동 재생 흐름 완성
- [ ] **I2S 마이크(ICS43434) 통합** — 라이브 녹음 입력 경로 구현
- [ ] **패턴 체인** — `pattern_chain_length` 활용, 복수 패턴 이어붙이기
- [ ] **FX 파라미터** — 리버브/딜레이 등 파라미터 UI + 오디오 엔진 연동

## 미디(MIDI)

- [ ] **MIDI 출력** — USB MIDI 또는 TRS MIDI (건반 누름 → Note On/Off)
- [ ] **MIDI 클럭 동기** — 외부 MIDI 클럭에 BPM 맞추기

## PCB / 하드웨어

- [ ] **최종 PCB 설계** — 브레드보드 프로토타입(Set B) 검증 완료 후 정식 PCB 제작
- [ ] **MAX98357A 실장 검토** — 납땜 모듈 vs SMD 직접 실장 결정
- [ ] **엔코더 노브 + 케이스 설계** — 3D 프린트 또는 아크릴 케이스

## 문서

- [ ] **핀맵 다이어그램 최신화** — Set B 확정 배선 기준으로 회로도 업데이트
- [ ] **README 최신화** — 오래된 host-build 위주 설명을 RP2350 기준으로 전면 개정
