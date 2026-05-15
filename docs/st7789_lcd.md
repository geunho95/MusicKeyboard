# ST7789 LCD 구현 레퍼런스

## 하드웨어 스펙

| 항목 | 값 |
|------|-----|
| 패널 해상도 | 284×76 (landscape) |
| 컨트롤러 내부 프레임버퍼 | 240×320 (ST7789 기본) |
| 인터페이스 | SPI0, 20MHz |
| 색상 포맷 | RGB565 (16-bit) |

## 핀 연결

| GPIO | 기능 | 비고 |
|------|------|------|
| GP0 | DC | 명령/데이터 선택 |
| GP1 | CS | LCD 칩셀렉 |
| GP2 | SCK | SPI0 (SD 공유) |
| GP3 | MOSI | SPI0 (SD 공유) |
| GP21 | RST | 임시 배선 (ICS43434 MIC SD 자리) |
| — | BLK | 3.3V 직결 (항상 켜짐) |
| GP5 | SD CS | SD를 쓰지 않을 때 HIGH 유지 필수 |

## MADCTL / 오프셋 설정 (실측값)

```c
#define LCD_MADCTL    0x60u   /* MX|MV — landscape */
#define LCD_CASET_OFS 18u     /* X offset: (320 - 284) / 2 */
#define LCD_RASET_OFS 82u     /* Y offset: (240 - 76)  / 2 */
```

- `MV(0x20)`: Row/Column Exchange → landscape 전환
- `MX(0x40)`: Column Mirror
- ST7789 컨트롤러 내부 240×320 프레임버퍼의 중앙에 284×76 패널이 배치되어 있어 오프셋 보정 필요
- `INVON` 제거 — 이 패널에서는 역효과 (불필요)

## 초기화 순서

```c
// 1. SPI + GPIO 초기화
spi_init(spi0, 20000000);
gpio_set_function(GP2, GPIO_FUNC_SPI);   // SCK
gpio_set_function(GP3, GPIO_FUNC_SPI);   // MOSI

// SD CS는 반드시 HIGH로 초기화 (SPI 버스 충돌 방지)
gpio_put(MK_RP2350_SD_CS_PIN, 1);

// 2. HW 리셋 펄스 (RST: HIGH→LOW 10ms→HIGH→120ms 대기)
gpio_put(RST, 1); sleep_ms(10);
gpio_put(RST, 0); sleep_ms(10);
gpio_put(RST, 1); sleep_ms(120);

// 3. 커맨드 시퀀스
SWRESET → sleep 150ms
SLPOUT  → sleep 120ms
COLMOD  0x55  → sleep 10ms   (RGB565)
MADCTL  0x60               (MX|MV landscape)
NORON   → sleep 10ms
// GRAM 전체 클리어 후 DISPON (켜질 때 노이즈 방지)
mk_lcd_gram_window(0, 319, 0, 239, BLACK, 320*240);
DISPON  → sleep 10ms
```

> **초기화 순서 주의**: SD 카드 init보다 LCD init을 먼저 해야 함. `mk_lcd_init()` → `mk_lcd_raw_fill()` → `mk_app_init()`

## 윈도우 설정 (lcd_set_window)

```
CASET: [x0 + 18, x1 + 18]   (landscape X)
RASET: [y0 + 82, y1 + 82]   (landscape Y)
RAMWR 후 픽셀 데이터 전송
```

## API

```c
// 헤더: include/music_keyboard/platform/lcd_hal.h
// 구현: src/platform/rp2350/st7789_rp2350.c

bool mk_lcd_init(void);

void mk_lcd_clear(uint16_t color);
void mk_lcd_fill_rect(int x, int y, int w, int h, uint16_t color);
void mk_lcd_draw_pixel(int x, int y, uint16_t color);

// 5×7 비트맵 폰트, 1배 (글자 크기: 6×7 px, FONT_GAP=1 포함)
void mk_lcd_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg);
void mk_lcd_draw_str (int x, int y, const char *s, uint16_t fg, uint16_t bg);

// 2배 확대 (글자 크기: 12×14 px)
void mk_lcd_draw_char2(int x, int y, char c, uint16_t fg, uint16_t bg);
void mk_lcd_draw_str2 (int x, int y, const char *s, uint16_t fg, uint16_t bg);

// 진단용: CASET/RASET 좌표 직접 지정
void mk_lcd_gram_window(uint16_t cs0, uint16_t cs1,
                        uint16_t rs0, uint16_t rs1,
                        uint16_t color, uint32_t npix);

// landscape 전체 화면 단색 채우기 (오프셋 보정 포함)
void mk_lcd_raw_fill(uint16_t color);
```

## 색상 상수 (RGB565)

```c
MK_LCD_BLACK, MK_LCD_WHITE, MK_LCD_RED,    MK_LCD_GREEN,
MK_LCD_BLUE,  MK_LCD_YELLOW, MK_LCD_CYAN,  MK_LCD_MAGENTA,
MK_LCD_GRAY,  MK_LCD_DKGRAY, MK_LCD_ORANGE

// 임의 색상
MK_RGB(r, g, b)   // r/g/b: 0~255
```

## 화면 레이아웃 (284×76)

```
y= 0  ┌── 상태바 (h=14) ── [STOP/PLAY/REC] [STEP:xx/16] [SND:x] [BPM:xxx] ──┐
y=14  ├── 구분선 ────────────────────────────────────────────────────────────┤
y=15  │   스텝 그리드 (h=37) — 16칸, 현재 스텝 = YELLOW                      │
y=52  ├── 구분선 ────────────────────────────────────────────────────────────┤
y=53  │   키 레이블 (h=23) — C C# D D# E F F# G G# A A# B FN1 FN2 FN3       │
y=76  └─────────────────────────────────────────────────────────────────────┘
```

## SPI 버스 공유 (SD + LCD)

```c
// LCD 사용 전 항상 호출
static inline void lcd_bus_claim(void) {
    gpio_put(MK_RP2350_SD_CS_PIN, 1);       // SD CS HIGH
    spi_set_baudrate(spi0, 20000000);        // LCD 속도로 전환
}
// SD 사용 시에는 diskio_sd_spi_rp2350.c 내부에서 CS/속도 전환
```

## 폰트

- 내장 5×7 비트맵 폰트, ASCII 32~126
- 각 글자 = 5바이트 (열 단위, bit0=상단)
- 1배: 6×7 px (gap 1), 2배: 12×14 px (gap 2)
- 한글 미지원
