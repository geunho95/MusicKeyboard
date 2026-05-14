#include <string.h>
#include <stdio.h>

#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"

#include "music_keyboard/platform/lcd_hal.h"
#include "music_keyboard/platform/rp2350_pinmap.h"

/* ═══════════════════════════════════════════════════════════
 * 하드웨어 설정
 * ══════════════════════════════════════════════════════════ */
#define LCD_SPI       spi0
#define LCD_BAUD_HZ   20000000u   /* 20 MHz */

/*
 * 76×284 패널 ST7789 컨트롤러 오프셋
 * 컨트롤러 내부 프레임버퍼(240×320) 대비 패널 위치
 * landscape(MV=1) 이후 기준:  x_offset, y_offset
 * ※ 화면이 치우쳐 보이면 아래 두 값을 조정
 */
#define LCD_COL_OFFSET  0u
#define LCD_ROW_OFFSET  0u

/*
 * MADCTL: landscape 0x60 (MX|MV)
 *   MV(0x20) = Row/Column Exchange → landscape 전환
 *   MX(0x40) = Column Mirror
 *
 * 오프셋 스캔 결과 (2026-05-14 실측):
 *   portrait CASET 0-75  / RASET 0-283  → offset 없음
 *   landscape CASET 0-283 / RASET 82-157 → RASET offset = 82
 *
 *   RASET offset = (240 - 76) / 2 = 82
 *   (ST7789 landscape 프레임버퍼 240행 중앙에 76px 패널이 배치됨)
 */
#define LCD_MADCTL      0x60u
#define LCD_CASET_OFS   18u   /* landscape X offset: (320-MK_LCD_W)/2 */
#define LCD_RASET_OFS   82u   /* landscape Y offset: (240-MK_LCD_H)/2 */

/* ═══════════════════════════════════════════════════════════
 * ST7789 커맨드
 * ══════════════════════════════════════════════════════════ */
#define ST7789_SWRESET  0x01u
#define ST7789_SLPOUT   0x11u
#define ST7789_NORON    0x13u
#define ST7789_INVON    0x21u
#define ST7789_DISPON   0x29u
#define ST7789_CASET    0x2Au
#define ST7789_RASET    0x2Bu
#define ST7789_RAMWR    0x2Cu
#define ST7789_MADCTL   0x36u
#define ST7789_COLMOD   0x3Au

/* ═══════════════════════════════════════════════════════════
 * 저수준 SPI 헬퍼
 * ══════════════════════════════════════════════════════════ */
static inline void lcd_cs_lo(void) { gpio_put(MK_RP2350_LCD_CS_PIN,  0); }
static inline void lcd_cs_hi(void) { gpio_put(MK_RP2350_LCD_CS_PIN,  1); }
static inline void lcd_dc_cmd(void) { gpio_put(MK_RP2350_LCD_DC_PIN, 0); }
static inline void lcd_dc_dat(void) { gpio_put(MK_RP2350_LCD_DC_PIN, 1); }

/* SD CS를 HIGH 로 유지한 채 LCD 버스 사용 */
static inline void lcd_bus_claim(void) {
    gpio_put(MK_RP2350_SD_CS_PIN, 1);
    spi_set_baudrate(LCD_SPI, LCD_BAUD_HZ);
}

static void lcd_write_cmd(uint8_t cmd) {
    lcd_dc_cmd();
    lcd_cs_lo();
    spi_write_blocking(LCD_SPI, &cmd, 1);
    lcd_cs_hi();
}

static void lcd_write_dat8(uint8_t dat) {
    lcd_dc_dat();
    lcd_cs_lo();
    spi_write_blocking(LCD_SPI, &dat, 1);
    lcd_cs_hi();
}

static void lcd_write_dat_buf(const uint8_t *buf, size_t len) {
    lcd_dc_dat();
    lcd_cs_lo();
    spi_write_blocking(LCD_SPI, buf, len);
    lcd_cs_hi();
}

/* ═══════════════════════════════════════════════════════════
 * 윈도우 설정 + RAMWR 개시
 * ══════════════════════════════════════════════════════════ */
/*
 * landscape 윈도우 설정
 *   CASET = X + 18  (실측: 18~301 가시 영역, (320-284)/2=18)
 *   RASET = Y + 82  (실측: 82~157 가시 영역, (240-76)/2=82)
 */
static void lcd_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
    uint8_t buf[4];
    uint16_t rx0 = x0 + LCD_CASET_OFS;
    uint16_t rx1 = x1 + LCD_CASET_OFS;
    uint16_t ry0 = y0 + LCD_RASET_OFS;
    uint16_t ry1 = y1 + LCD_RASET_OFS;

    lcd_write_cmd(ST7789_CASET);
    buf[0] = (uint8_t)(rx0 >> 8); buf[1] = (uint8_t)rx0;
    buf[2] = (uint8_t)(rx1 >> 8); buf[3] = (uint8_t)rx1;
    lcd_dc_dat(); lcd_cs_lo();
    spi_write_blocking(LCD_SPI, buf, 4);
    lcd_cs_hi();

    lcd_write_cmd(ST7789_RASET);
    buf[0] = (uint8_t)(ry0 >> 8); buf[1] = (uint8_t)ry0;
    buf[2] = (uint8_t)(ry1 >> 8); buf[3] = (uint8_t)ry1;
    lcd_dc_dat(); lcd_cs_lo();
    spi_write_blocking(LCD_SPI, buf, 4);
    lcd_cs_hi();

    lcd_write_cmd(ST7789_RAMWR);
}

/* forward declaration */
void mk_lcd_gram_window(uint16_t cs0, uint16_t cs1,
                         uint16_t rs0, uint16_t rs1,
                         uint16_t color, uint32_t npix);

/* ═══════════════════════════════════════════════════════════
 * 초기화
 * ══════════════════════════════════════════════════════════ */
bool mk_lcd_init(void) {
    /* SPI0 초기화 */
    spi_init(LCD_SPI, LCD_BAUD_HZ);

    /* SCK/MOSI SPI 펑션 */
    gpio_set_function(MK_RP2350_LCD_SCK_PIN,  GPIO_FUNC_SPI);
    gpio_set_function(MK_RP2350_LCD_MOSI_PIN, GPIO_FUNC_SPI);

    /* DC, CS GPIO 설정 */
    gpio_init(MK_RP2350_LCD_DC_PIN);
    gpio_set_dir(MK_RP2350_LCD_DC_PIN, GPIO_OUT);
    gpio_put(MK_RP2350_LCD_DC_PIN, 1);

    gpio_init(MK_RP2350_LCD_CS_PIN);
    gpio_set_dir(MK_RP2350_LCD_CS_PIN, GPIO_OUT);
    gpio_put(MK_RP2350_LCD_CS_PIN, 1);

    /* SD_CS HIGH 유지 (SPI 버스 충돌 방지) */
    gpio_init(MK_RP2350_SD_CS_PIN);
    gpio_set_dir(MK_RP2350_SD_CS_PIN, GPIO_OUT);
    gpio_put(MK_RP2350_SD_CS_PIN, 1);

    /* RST 하드웨어 리셋 펄스 */
    gpio_init(MK_RP2350_LCD_RST_PIN);
    gpio_set_dir(MK_RP2350_LCD_RST_PIN, GPIO_OUT);
    gpio_put(MK_RP2350_LCD_RST_PIN, 1);
    sleep_ms(10);
    gpio_put(MK_RP2350_LCD_RST_PIN, 0);  /* LOW: 리셋 진입 */
    sleep_ms(10);
    gpio_put(MK_RP2350_LCD_RST_PIN, 1);  /* HIGH: 리셋 해제 */
    sleep_ms(120);

    printf("[lcd] SWRESET\n");
    lcd_write_cmd(ST7789_SWRESET);
    sleep_ms(150);

    printf("[lcd] SLPOUT\n");
    lcd_write_cmd(ST7789_SLPOUT);
    sleep_ms(120);

    printf("[lcd] COLMOD\n");
    lcd_write_cmd(ST7789_COLMOD);
    lcd_write_dat8(0x55u);
    sleep_ms(10);

    printf("[lcd] MADCTL\n");
    lcd_write_cmd(ST7789_MADCTL);
    lcd_write_dat8(LCD_MADCTL);

    /* INVON 제거 — 패널에 따라 불필요하거나 역효과 */

    printf("[lcd] NORON\n");
    lcd_write_cmd(ST7789_NORON);
    sleep_ms(10);

    /* DISPON 전에 전체 GRAM 클리어 → 화면 켜질 때 노이즈 없음 */
    printf("[lcd] full GRAM clear\n");
    mk_lcd_gram_window(0, 319, 0, 239, MK_LCD_BLACK, 320u * 240u);

    printf("[lcd] DISPON\n");
    lcd_write_cmd(ST7789_DISPON);
    sleep_ms(10);

    printf("[lcd] init ok  %u×%u  madctl=0x%02x\n",
           MK_LCD_W, MK_LCD_H, LCD_MADCTL);
    return true;
}

/* ═══════════════════════════════════════════════════════════
 * 그리기 기본
 * ══════════════════════════════════════════════════════════ */

/* 픽셀 버퍼 (한 행 단위 전송용) */
static uint8_t s_row_buf[MK_LCD_W * 2u];

/*
 * 진단용: 임의 CASET/RASET 창에 단색 채우기
 * npix = (cs1-cs0+1)*(rs1-rs0+1)
 */
void mk_lcd_gram_window(uint16_t cs0, uint16_t cs1,
                         uint16_t rs0, uint16_t rs1,
                         uint16_t color, uint32_t npix) {
    lcd_bus_claim();

    uint8_t buf[4];
    lcd_write_cmd(ST7789_CASET);
    buf[0]=(uint8_t)(cs0>>8); buf[1]=(uint8_t)cs0;
    buf[2]=(uint8_t)(cs1>>8); buf[3]=(uint8_t)cs1;
    lcd_dc_dat(); lcd_cs_lo(); spi_write_blocking(LCD_SPI,buf,4); lcd_cs_hi();

    lcd_write_cmd(ST7789_RASET);
    buf[0]=(uint8_t)(rs0>>8); buf[1]=(uint8_t)rs0;
    buf[2]=(uint8_t)(rs1>>8); buf[3]=(uint8_t)rs1;
    lcd_dc_dat(); lcd_cs_lo(); spi_write_blocking(LCD_SPI,buf,4); lcd_cs_hi();

    lcd_write_cmd(ST7789_RAMWR);
    uint8_t hi=(uint8_t)(color>>8), lo=(uint8_t)(color&0xFF);
    /* 한 번에 MK_LCD_W(284) 픽셀씩 전송 */
    for (int i=0; i<MK_LCD_W; i++) { s_row_buf[i*2]=hi; s_row_buf[i*2+1]=lo; }
    lcd_dc_dat(); lcd_cs_lo();
    uint32_t sent=0;
    while (sent < npix) {
        uint32_t chunk = npix - sent;
        if (chunk > (uint32_t)MK_LCD_W) chunk = (uint32_t)MK_LCD_W;
        spi_write_blocking(LCD_SPI, s_row_buf, chunk*2);
        sent += chunk;
    }
    lcd_cs_hi();
}

/* landscape: CASET 0-283 (X), RASET 82-157 (Y+82), 284px × 76rows */
void mk_lcd_raw_fill(uint16_t color) {
    lcd_bus_claim();

    /* CASET: 18~301 (X + LCD_CASET_OFS) */
    uint16_t c0 = LCD_CASET_OFS;
    uint16_t c1 = LCD_CASET_OFS + MK_LCD_W - 1u;
    uint8_t ca[4] = {(uint8_t)(c0>>8),(uint8_t)c0,(uint8_t)(c1>>8),(uint8_t)c1};
    lcd_write_cmd(ST7789_CASET);
    lcd_dc_dat(); lcd_cs_lo(); spi_write_blocking(LCD_SPI, ca, 4); lcd_cs_hi();

    /* RASET: 82~157 (Y + LCD_RASET_OFS) */
    uint16_t r0 = LCD_RASET_OFS;
    uint16_t r1 = LCD_RASET_OFS + MK_LCD_H - 1u;
    uint8_t ra[4] = {(uint8_t)(r0>>8),(uint8_t)r0,(uint8_t)(r1>>8),(uint8_t)r1};
    lcd_write_cmd(ST7789_RASET);
    lcd_dc_dat(); lcd_cs_lo(); spi_write_blocking(LCD_SPI, ra, 4); lcd_cs_hi();

    /* RAMWR: 284px × 76rows */
    lcd_write_cmd(ST7789_RAMWR);
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    for (int i = 0; i < MK_LCD_W; i++) {
        s_row_buf[i * 2]     = hi;
        s_row_buf[i * 2 + 1] = lo;
    }
    lcd_dc_dat(); lcd_cs_lo();
    for (int row = 0; row < MK_LCD_H; row++) {
        spi_write_blocking(LCD_SPI, s_row_buf, MK_LCD_W * 2);
    }
    lcd_cs_hi();
}

void mk_lcd_clear(uint16_t color) {
    mk_lcd_fill_rect(0, 0, MK_LCD_W, MK_LCD_H, color);
}

void mk_lcd_fill_rect(int x, int y, int w, int h, uint16_t color) {
    if (x >= MK_LCD_W || y >= MK_LCD_H || w <= 0 || h <= 0) return;
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > MK_LCD_W) w = MK_LCD_W - x;
    if (y + h > MK_LCD_H) h = MK_LCD_H - y;
    if (w <= 0 || h <= 0) return;

    lcd_bus_claim();

    /* landscape (MADCTL=0x70): X→CASET, Y→RASET, 변환 불필요 */
    lcd_set_window((uint16_t)x, (uint16_t)y,
                   (uint16_t)(x + w - 1), (uint16_t)(y + h - 1));

    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFFu);
    for (int i = 0; i < w; ++i) {
        s_row_buf[i * 2]     = hi;
        s_row_buf[i * 2 + 1] = lo;
    }

    lcd_dc_dat();
    lcd_cs_lo();
    for (int row = 0; row < h; ++row) {
        spi_write_blocking(LCD_SPI, s_row_buf, (size_t)(w * 2));
    }
    lcd_cs_hi();
}

void mk_lcd_draw_pixel(int x, int y, uint16_t color) {
    if (x < 0 || x >= MK_LCD_W || y < 0 || y >= MK_LCD_H) return;
    lcd_bus_claim();
    lcd_set_window((uint16_t)x, (uint16_t)y, (uint16_t)x, (uint16_t)y);  /* X→CASET, Y→RASET */
    uint8_t buf[2] = {(uint8_t)(color >> 8), (uint8_t)(color & 0xFFu)};
    lcd_write_dat_buf(buf, 2);
}

/* ═══════════════════════════════════════════════════════════
 * 5×7 비트맵 폰트 (ASCII 32~126)
 * 각 글자 = 5바이트, 1바이트 = 열(column), bit0=상단
 * ══════════════════════════════════════════════════════════ */
static const uint8_t k_font5x7[][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /* 32 ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* 33 ! */
    {0x00,0x07,0x00,0x07,0x00}, /* 34 " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* 35 # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* 36 $ */
    {0x23,0x13,0x08,0x64,0x62}, /* 37 % */
    {0x36,0x49,0x55,0x22,0x50}, /* 38 & */
    {0x00,0x05,0x03,0x00,0x00}, /* 39 ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* 40 ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* 41 ) */
    {0x08,0x2A,0x1C,0x2A,0x08}, /* 42 * */
    {0x08,0x08,0x3E,0x08,0x08}, /* 43 + */
    {0x00,0x50,0x30,0x00,0x00}, /* 44 , */
    {0x08,0x08,0x08,0x08,0x08}, /* 45 - */
    {0x00,0x60,0x60,0x00,0x00}, /* 46 . */
    {0x20,0x10,0x08,0x04,0x02}, /* 47 / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 48 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 49 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 50 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 51 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 52 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 53 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 54 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 55 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 56 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 57 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* 58 : */
    {0x00,0x56,0x36,0x00,0x00}, /* 59 ; */
    {0x00,0x08,0x14,0x22,0x41}, /* 60 < */
    {0x14,0x14,0x14,0x14,0x14}, /* 61 = */
    {0x41,0x22,0x14,0x08,0x00}, /* 62 > */
    {0x02,0x01,0x51,0x09,0x06}, /* 63 ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* 64 @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 65 A */
    {0x7F,0x49,0x49,0x49,0x36}, /* 66 B */
    {0x3E,0x41,0x41,0x41,0x22}, /* 67 C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 68 D */
    {0x7F,0x49,0x49,0x49,0x41}, /* 69 E */
    {0x7F,0x09,0x09,0x09,0x01}, /* 70 F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 71 G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 72 H */
    {0x00,0x41,0x7F,0x41,0x00}, /* 73 I */
    {0x20,0x40,0x41,0x3F,0x01}, /* 74 J */
    {0x7F,0x08,0x14,0x22,0x41}, /* 75 K */
    {0x7F,0x40,0x40,0x40,0x40}, /* 76 L */
    {0x7F,0x02,0x04,0x02,0x7F}, /* 77 M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 78 N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 79 O */
    {0x7F,0x09,0x09,0x09,0x06}, /* 80 P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 81 Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* 82 R */
    {0x46,0x49,0x49,0x49,0x31}, /* 83 S */
    {0x01,0x01,0x7F,0x01,0x01}, /* 84 T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 85 U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 86 V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* 87 W */
    {0x63,0x14,0x08,0x14,0x63}, /* 88 X */
    {0x07,0x08,0x70,0x08,0x07}, /* 89 Y */
    {0x61,0x51,0x49,0x45,0x43}, /* 90 Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* 91 [ */
    {0x02,0x04,0x08,0x10,0x20}, /* 92 \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* 93 ] */
    {0x04,0x02,0x01,0x02,0x04}, /* 94 ^ */
    {0x40,0x40,0x40,0x40,0x40}, /* 95 _ */
    {0x00,0x01,0x02,0x04,0x00}, /* 96 ` */
    {0x20,0x54,0x54,0x54,0x78}, /* 97 a */
    {0x7F,0x48,0x44,0x44,0x38}, /* 98 b */
    {0x38,0x44,0x44,0x44,0x20}, /* 99 c */
    {0x38,0x44,0x44,0x48,0x7F}, /* 100 d */
    {0x38,0x54,0x54,0x54,0x18}, /* 101 e */
    {0x08,0x7E,0x09,0x01,0x02}, /* 102 f */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 103 g */
    {0x7F,0x08,0x04,0x04,0x78}, /* 104 h */
    {0x00,0x44,0x7D,0x40,0x00}, /* 105 i */
    {0x20,0x40,0x44,0x3D,0x00}, /* 106 j */
    {0x7F,0x10,0x28,0x44,0x00}, /* 107 k */
    {0x00,0x41,0x7F,0x40,0x00}, /* 108 l */
    {0x7C,0x04,0x18,0x04,0x78}, /* 109 m */
    {0x7C,0x08,0x04,0x04,0x78}, /* 110 n */
    {0x38,0x44,0x44,0x44,0x38}, /* 111 o */
    {0x7C,0x14,0x14,0x14,0x08}, /* 112 p */
    {0x08,0x14,0x14,0x18,0x7C}, /* 113 q */
    {0x7C,0x08,0x04,0x04,0x08}, /* 114 r */
    {0x48,0x54,0x54,0x54,0x20}, /* 115 s */
    {0x04,0x3F,0x44,0x40,0x20}, /* 116 t */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 117 u */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 118 v */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 119 w */
    {0x44,0x28,0x10,0x28,0x44}, /* 120 x */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 121 y */
    {0x44,0x64,0x54,0x4C,0x44}, /* 122 z */
    {0x00,0x08,0x36,0x41,0x00}, /* 123 { */
    {0x00,0x00,0x7F,0x00,0x00}, /* 124 | */
    {0x00,0x41,0x36,0x08,0x00}, /* 125 } */
    {0x08,0x04,0x08,0x10,0x08}, /* 126 ~ */
};

#define FONT_W  5
#define FONT_H  7
#define FONT_GAP 1   /* 글자 간격 */

/* 1배 글자 */
void mk_lcd_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *col = k_font5x7[(uint8_t)(c - 32)];
    for (int cx = 0; cx < FONT_W; ++cx) {
        uint8_t bits = col[cx];
        for (int cy = 0; cy < FONT_H; ++cy) {
            mk_lcd_draw_pixel(x + cx, y + cy,
                              (bits & (1u << cy)) ? fg : bg);
        }
    }
    /* 오른쪽 여백 */
    mk_lcd_fill_rect(x + FONT_W, y, FONT_GAP, FONT_H, bg);
}

void mk_lcd_draw_str(int x, int y, const char *s, uint16_t fg, uint16_t bg) {
    while (*s) {
        mk_lcd_draw_char(x, y, *s++, fg, bg);
        x += FONT_W + FONT_GAP;
    }
}

/* 2배 글자 (10×14) */
void mk_lcd_draw_char2(int x, int y, char c, uint16_t fg, uint16_t bg) {
    if (c < 32 || c > 126) c = '?';
    const uint8_t *col = k_font5x7[(uint8_t)(c - 32)];
    for (int cx = 0; cx < FONT_W; ++cx) {
        uint8_t bits = col[cx];
        for (int cy = 0; cy < FONT_H; ++cy) {
            uint16_t clr = (bits & (1u << cy)) ? fg : bg;
            mk_lcd_fill_rect(x + cx * 2, y + cy * 2, 2, 2, clr);
        }
    }
    mk_lcd_fill_rect(x + FONT_W * 2, y, FONT_GAP * 2, FONT_H * 2, bg);
}

void mk_lcd_draw_str2(int x, int y, const char *s, uint16_t fg, uint16_t bg) {
    while (*s) {
        mk_lcd_draw_char2(x, y, *s++, fg, bg);
        x += (FONT_W + FONT_GAP) * 2;
    }
}
