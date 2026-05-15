# RP2350 Bringup 현황 (2026-05-14)

## 하드웨어 구성

| 부품 | 연결 상태 |
|------|-----------|
| Raspberry Pi Pico 2 (RP2350) | ✅ |
| ST7789 LCD 284×76 | ✅ 브레드보드 외 보드에 연결 |
| MAX98357A I2S 앰프 + 스피커 | ✅ VIN+SD → VSYS(5V), GP10/11/12 |
| 4×4 버튼 매트릭스 | ✅ |
| 로터리 엔코더 2개 | ✅ |
| SD 카드 (TF) | 미확인 (fallback 샘플로 동작 중) |

---

## 1. LCD 화면 ✅ 동작 확인

**대상 코드**: `firmware/rp2350_smoke/` 및 `firmware/rp2350_bringup/`

**화면 구성** (284×76 landscape):

```
y= 0  ┌─── 상태바 (h=14) ── [STOP/PLAY/REC] [STEP:xx/16] [SND:x] [BPM:xxx] ──┐
y=14  ├─── 구분선 ─────────────────────────────────────────────────────────── ┤
y=15  │    스텝 그리드 (h=37) — 16칸, 현재 스텝 = YELLOW                      │
y=52  ├─── 구분선 ─────────────────────────────────────────────────────────── ┤
y=53  │    키 레이블 (h=23) — C C# D D# E F F# G G# A A# B FN1 FN2 FN3       │
y=76  └────────────────────────────────────────────────────────────────────────┘
```

**드라이버**: `firmware/src/platform/rp2350/st7789_rp2350.c`

**핀맵**:
```
DC   = GP0
CS   = GP1
SCK  = GP2 (SPI0, SD 카드와 공유)
MOSI = GP3 (SPI0, SD 카드와 공유)
RST  → 3.3V 직결
BLK  → 3.3V 직결
```

**HAL API** (`include/music_keyboard/platform/lcd_hal.h`):
```c
bool mk_lcd_init(void);
void mk_lcd_clear(uint16_t color);
void mk_lcd_fill_rect(int x, int y, int w, int h, uint16_t color);
void mk_lcd_draw_str(int x, int y, const char *s, uint16_t fg, uint16_t bg);   // 5×7 폰트 1배
void mk_lcd_draw_str2(int x, int y, const char *s, uint16_t fg, uint16_t bg);  // 5×7 폰트 2배
```

**초기화 순서** (중요: SD 카드보다 LCD init 먼저):
```c
mk_lcd_init();              // SPI0 초기화 + ST7789 설정, 이 시점에 SD CS는 HIGH 유지
mk_lcd_raw_fill(RED);       // 색상 플래시 (300ms × 3 → 자가진단)
mk_app_init(&app);          // SD 포함 나머지 초기화
```

---

## 2. 버튼 스캔 ✅ 동작 확인

**대상 코드**: `firmware/rp2350_smoke/main.c`

### 4×4 매트릭스 배선

```
         COL0(GP26)  COL1(GP27)  COL2(GP28)  COL3(GP19)
ROW0(GP20) [  C  ]   [  C# ]    [  D  ]    [ D# ]   id: 0~ 3
ROW1(GP22) [  E  ]   [  F  ]    [ F#  ]    [  G ]   id: 4~ 7
ROW2(GP15) [ G#  ]   [  A  ]    [ A#  ]    [  B ]   id: 8~11
ROW3(GP16) [ FN1 ]   [ FN2 ]    [ FN3 ]    [ -- ]   id:12~14 (15=미사용)
```

- **스캔 방식**: COL → OUTPUT (순서대로 LOW 구동), ROW → INPUT (내부 풀업, LOW 감지)
- **다이오드**: 1N4148, A→ROW, K(띠)→COL 방향 필수
- **디바운스**: 15ms

### 엔코더

| 역할 | A핀 | B핀 | SW핀 | button_id |
|------|-----|-----|------|-----------|
| ENC1 (BPM/선택) | GP6 | GP7 | GP17 | 회전=19, 푸시=16 |
| ENC2 (피치/볼륨) | GP8 | GP9 | GP18 | 회전=20, 푸시=17 |

- **Gray code 4-상태 머신**, 누적 ±4 에서 1클릭 확정
- **이벤트 타입**: `MK_BUTTON_EVENT_PRESS`, `MK_BUTTON_EVENT_RELEASE`, `MK_BUTTON_EVENT_ENC_CW`, `MK_BUTTON_EVENT_ENC_CCW`

### 버튼 ID → 기능 매핑 (`include/music_keyboard/button_map.h`)

```c
MK_KEY_C  = 0  …  MK_KEY_B = 11   // 피아노 건반 → semitone 직접 대응
MK_BTN_PLAY  = 12  // FN1: 재생/정지 토글
MK_BTN_REC   = 13  // FN2: 녹음 on/off (온보드 LED 빨간 깜박임)
MK_BTN_MODE  = 14  // FN3: 뷰 모드 순환
```

---

## 3. 사운드 ✅ 동작 확인

### 소리가 나는 코드

**`firmware/rp2350_bringup/`** 및 **`firmware/rp2350_smoke/`** (2026-05-14 이후)

두 빌드 모두 동일한 오디오 스택을 공유:

```
mk_app_init()
  └─ mk_audio_hal_init()          // DMA 더블버퍼 I2S 초기화
  └─ mk_storage_init()            // SD 없으면 fallback 샘플 생성
       └─ mk_storage_generate_fallback_sample()
            // 정수 스퀘어웨이브: period=64, amplitude=15000
            // envelope 페이드아웃, 12000 프레임 @ 24kHz

메인 루프:
  mk_app_tick()                   // 버튼 스캔 → mk_app_trigger_piano_key()
  while (writable >= 64 frames):
    mk_app_render_audio()         // 활성 voice → PCM 믹싱
    mk_audio_hal_submit_frames()  // DMA 버퍼에 제출
```

**건반 누르면 재생되는 흐름**:
```
버튼 PRESS (id 0~11)
  → mk_app_trigger_piano_key(app, semitone)
  → rate = 2^((semitone + octave*12) / 12)  // 피치 계산
  → 오디오 엔진 voice 활성화
  → render 루프에서 fallback 샘플을 rate 배속으로 재생
```

### 오디오 HAL

**드라이버**: `firmware/src/platform/rp2350/audio_hal_rp2350_i2s.c`

**핀맵**:
```
BCLK = GP10  (PIO side-set bit0)
LRCK = GP11  (PIO side-set bit1)
DIN  = GP12  (PIO OUT)
```

**설정**:
```
PIO0, SM0
샘플레이트: 24000 Hz
비트뎁스: 16-bit 스테레오
클럭분주: 150MHz / (24000 × 64) ≈ 97.66
DMA 더블버퍼: 각 버퍼 64 프레임 × 2ch × 2byte = 256 bytes
IRQ: DMA_IRQ_0 → mk_i2s_dma_handler (버퍼 스왑)
```

**PIO 프로그램**: `firmware/src/platform/rp2350/audio_i2s_out.pio`
- `.side_set 2` (BCLK + LRCK 동시 제어)
- 64 PIO 사이클 = 스테레오 1프레임 (좌16비트 + 우16비트)

### 소리가 나지 않았던 시도 (원인 분석)

| 시도 | 방법 | 결과 | 원인 |
|------|------|------|------|
| sinf() 440Hz + HAL submit | 수동 버퍼 채우기 | ❌ 무음 | HAL DMA 타이밍과 엇박자, writable_frames 체크 미흡으로 추정 |
| pio_sm_put_blocking 직접 | HAL 우회, PIO 직접 쓰기 | ❌ 무음 | PIO 클럭 설정 또는 side-set 타이밍 불일치로 추정 |
| 정수 스퀘어웨이브 + 직접 PIO | sinf() 제거 | ❌ 무음 | 동일하게 HAL 우회 경로 문제 |
| **mk_app_init + audio_engine + fallback 샘플** | **bringup 경로 그대로 사용** | **✅ 소리남** | DMA 더블버퍼 경로가 정상 동작 |

**결론**: 하드웨어(MAX98357A, 배선) 이상 없음. 소리 나는 필수 조건은 **DMA 더블버퍼 HAL + audio_engine 렌더 루프** 조합.

---

## 빌드 / 플래시 명령

```bash
# 빌드
cd firmware/rp2350_smoke/build
cmake .. -DPICO_BOARD=pico2
make -j4

# 플래시 (USB CDC 리셋, BOOTSEL 버튼 불필요)
picotool load -f -F build/musickeyboard_rp2350_smoke.uf2
```

---

## 남은 과제

- [ ] SD 카드 마운트 확인 (실제 WAV 샘플 로드)
- [ ] 버튼 물리 배치 검증 (현재 논리 ID는 정확, 물리 위치 확인 필요)
- [ ] 엔코더 BPM 조절 테스트
- [ ] 녹음 → 재생 루프 end-to-end 테스트
