# MusicKeyboard Project Direction

최종 업데이트: 2026-05-15

## 현재 방향: Standalone Lo-Fi Sampler

RP2350 + 4×4 매트릭스 버튼(1옥타브 건반 배열) + ST7789 LCD로 구성하는 독립형 로파이 샘플러 악기.

D200/UART 경로는 우회 불가 판정으로 폐기. 모든 UI와 입력은 RP2350에 직결.

## 구성

```
4×4 매트릭스 버튼 (C~B 12키 + FN1/2/3)
로터리 엔코더 2개 (옥타브, 피치벤드)
ST7789 LCD 284×76 (SPI0)
ICS43434 I2S 마이크
MAX98357A I2S 앰프 + 8Ω 스피커
SD 카드 (SPI0 공유, WAV 저장/로드)
RP2350 온보드 LED (GP25)
```

## 레퍼런스 스펙

`/Users/tvd/dev/Lofi-RP2350/rico2-lofi-sampler-guide.md`

핀맵, 동작 모드(PLAY/REC/TRIM), LCD 레이아웃, WAV 규격의 기준 문서.

## 핵심 동작

- 피치: 속도 변화 = 피치 변화 (로파이 방식, rate 기반)
- 모드: PLAY / REC / TRIM
- 뱅크: 0~7 (SD 카드 `/bank0/` ~ `/bank7/`)
- WAV 규격: 22050Hz / 16-bit signed PCM / Mono / 최대 5초

## 기술적 난점 및 해결된 이슈

- DMA 더블버퍼 HAL + audio_engine 렌더루프 조합으로만 소리 남 (직접 PIO 쓰기 ❌)
- LCD RST(GP21)와 I2S MIC DATA 겸용 → 녹음 시 mic.deinit() 후 GP21 HIGH 복구 필요
- SD 카드 미확인 상태 — 다음 우선 과제

## 남은 과제

- [ ] SD 카드 WAV 로드 확인
- [ ] ICS43434 마이크 녹음 경로 통합
- [ ] 녹음 → 트림 → 재생 end-to-end 테스트
