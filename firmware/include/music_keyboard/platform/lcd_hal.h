#pragma once

#include <stdbool.h>
#include <stdint.h>

/*
 * ST7789 LCD HAL — 284×76 landscape
 *
 * SPI0 공유 (SD 카드와 동일 버스, 다른 CS)
 * 핀: DC=GP0, CS=GP1, SCK=GP2, MOSI=GP3
 * RST → 3.3V 직결, BLK → 3.3V 직결
 */

/* 화면 크기 (landscape) */
#define MK_LCD_W   284
#define MK_LCD_H    76

/* RGB565 색상 헬퍼 */
#define MK_RGB(r, g, b) \
    ((uint16_t)(((r) & 0xF8u) << 8) | (uint16_t)(((g) & 0xFCu) << 3) | (uint16_t)(((b) & 0xF8u) >> 3))

#define MK_LCD_BLACK   0x0000u
#define MK_LCD_WHITE   0xFFFFu
#define MK_LCD_RED     MK_RGB(255,   0,   0)
#define MK_LCD_GREEN   MK_RGB(  0, 200,   0)
#define MK_LCD_BLUE    MK_RGB(  0,  80, 255)
#define MK_LCD_YELLOW  MK_RGB(255, 220,   0)
#define MK_LCD_CYAN    MK_RGB(  0, 200, 200)
#define MK_LCD_MAGENTA MK_RGB(200,   0, 200)
#define MK_LCD_GRAY    MK_RGB( 80,  80,  80)
#define MK_LCD_DKGRAY  MK_RGB( 30,  30,  30)
#define MK_LCD_ORANGE  MK_RGB(255, 120,   0)

/* ── 초기화 ── */
bool mk_lcd_init(void);

/* ── 그리기 기본 ── */
void mk_lcd_clear(uint16_t color);
void mk_lcd_fill_rect(int x, int y, int w, int h, uint16_t color);
void mk_lcd_draw_pixel(int x, int y, uint16_t color);

/* ── 텍스트 (5×7 비트맵 폰트, 1배속) ── */
void mk_lcd_draw_char(int x, int y, char c, uint16_t fg, uint16_t bg);
void mk_lcd_draw_str(int x, int y, const char *s, uint16_t fg, uint16_t bg);
/* 2배 확대 */
void mk_lcd_draw_char2(int x, int y, char c, uint16_t fg, uint16_t bg);
void mk_lcd_draw_str2(int x, int y, const char *s, uint16_t fg, uint16_t bg);
