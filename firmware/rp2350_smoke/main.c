#include <stdio.h>
#include <string.h>

#include "pico/stdlib.h"
#include "hardware/gpio.h"
#include "music_keyboard/platform/lcd_hal.h"

/* ── 핀맵 ────────────────────────────────────────────────── */
static const uint ROW_PINS[4] = { 20, 22, 15, 16 };
static const uint COL_PINS[4] = { 26, 27, 28, 19 };

static const char *KEY_NAME[4][4] = {
    { "C",  "C#", "D",  "D#" },
    { "E",  "F",  "F#", "G"  },
    { "G#", "A",  "A#", "B"  },
    { "F1", "F2", "F3", "--" },
};

/* ── 매트릭스 ────────────────────────────────────────────── */
static void matrix_init(void) {
    for (int r = 0; r < 4; r++) {
        gpio_init(ROW_PINS[r]);
        gpio_set_dir(ROW_PINS[r], GPIO_IN);
        gpio_pull_up(ROW_PINS[r]);
    }
    for (int c = 0; c < 4; c++) {
        gpio_init(COL_PINS[c]);
        gpio_set_dir(COL_PINS[c], GPIO_OUT);
        gpio_put(COL_PINS[c], 1);
    }
}

static void matrix_scan(bool pressed[4][4]) {
    for (int c = 0; c < 4; c++) {
        gpio_put(COL_PINS[c], 0);
        sleep_us(10);
        for (int r = 0; r < 4; r++)
            pressed[r][c] = !gpio_get(ROW_PINS[r]);
        gpio_put(COL_PINS[c], 1);
    }
}

/* ════════════════════════════════════════════════════════════
 * UI 레이아웃 (284×76 landscape)
 *
 *  y= 0  ┌────────────────────── 상태바 (h=14) ──────────────┐
 *  y=14  ├────────────────────── 구분선 (h=1)  ──────────────┤
 *  y=15  │                  스텝 그리드 (h=37)               │
 *  y=52  ├────────────────────── 구분선 (h=1)  ──────────────┤
 *  y=53  │                  키 레이블 (h=23)                  │
 *  y=76  └────────────────────────────────────────────────────┘
 * ════════════════════════════════════════════════════════════ */
#define BAR_H   14
#define STEP_Y  15
#define STEP_H  37
#define KEY_Y   53
#define KEY_H   23

/* 16-step 레이블 (4×4 매트릭스 순서: row0~row3, col0~col3) */
static const char *k_label[16] = {
    "C","C#","D","D#", "E","F","F#","G", "G#","A","A#","B", "F1","F2","F3","--"
};

/* 데모 패턴 (킥/스네어 느낌) */
static const bool k_pat[16] = {1,0,0,1, 0,0,1,0, 1,0,0,1, 0,0,0,0};

/* 셀 X 좌표: 284px ÷ 16 균등 분배 */
static inline int cell_x(int i)  { return (i * MK_LCD_W) / 16; }
static inline int cell_w(int i)  { return cell_x(i + 1) - cell_x(i) - 1; }

/* ── 셀 한 칸 그리기 (그리드 + 키 레이블) ─────────────────── */
static void ui_draw_cell(int i, bool active, bool highlight) {
    int x = cell_x(i);
    int w = cell_w(i);
    uint16_t grid_col = highlight  ? MK_LCD_YELLOW
                      : active     ? MK_LCD_GREEN
                      :               MK_LCD_DKGRAY;
    uint16_t lbl_col  = highlight  ? MK_LCD_YELLOW
                      : active     ? MK_LCD_GREEN
                      :               MK_LCD_GRAY;
    mk_lcd_fill_rect(x, STEP_Y, w, STEP_H, grid_col);
    mk_lcd_draw_str(x + 1, KEY_Y + (KEY_H - 7) / 2, k_label[i], lbl_col, MK_LCD_BLACK);
}

/* ── 스텝 이동 시 부분 업데이트 (깜빡임 없음) ──────────────── */
static void ui_update_step(uint8_t prev, uint8_t next, bool rec, bool run) {
    /* 이전 스텝: 원래 색으로 복원 */
    ui_draw_cell(prev, k_pat[prev], false);
    /* 새 스텝: 노란색 */
    ui_draw_cell(next, k_pat[next], true);
    /* 상태바 스텝 카운터만 갱신 */
    char buf[16];
    snprintf(buf, sizeof(buf), "%02u/16", (unsigned)(next + 1));
    mk_lcd_draw_str(27 + 6 * 6, 4, buf, MK_LCD_WHITE, MK_LCD_DKGRAY);
    (void)rec; (void)run;
}

/* ── 전체 화면 초기 그리기 ───────────────────────────────────── */
static void ui_draw(uint8_t step, bool rec, bool run) {
    /* 상태바 */
    mk_lcd_fill_rect(0, 0, MK_LCD_W, BAR_H, MK_LCD_DKGRAY);

    const char  *mode = rec ? "REC " : (run ? "PLAY" : "STOP");
    uint16_t    mcol  = rec ? MK_LCD_RED : (run ? MK_LCD_GREEN : MK_LCD_GRAY);
    mk_lcd_draw_str(3, 4, mode, mcol, MK_LCD_DKGRAY);

    char buf[48];
    snprintf(buf, sizeof(buf), "  STEP:%02u/16  SND:1  PAT:A", (unsigned)(step + 1));
    mk_lcd_draw_str(27, 4, buf, MK_LCD_WHITE, MK_LCD_DKGRAY);

    /* 구분선 */
    mk_lcd_fill_rect(0, BAR_H, MK_LCD_W, 1, MK_LCD_BLACK);

    /* 스텝 그리드 */
    for (int i = 0; i < 16; i++) {
        int x = cell_x(i);
        int w = cell_w(i);
        uint16_t col = (i == step)   ? MK_LCD_YELLOW
                     : k_pat[i]      ? MK_LCD_GREEN
                     :                  MK_LCD_DKGRAY;
        mk_lcd_fill_rect(x, STEP_Y, w, STEP_H, col);
        mk_lcd_fill_rect(x + w, STEP_Y, 1, STEP_H, MK_LCD_BLACK); /* 세로 구분선 */
    }

    /* 구분선 */
    mk_lcd_fill_rect(0, STEP_Y + STEP_H, MK_LCD_W, 1, MK_LCD_BLACK);

    /* 키 레이블 */
    mk_lcd_fill_rect(0, KEY_Y, MK_LCD_W, KEY_H, MK_LCD_BLACK);
    for (int i = 0; i < 16; i++) {
        int      x   = cell_x(i);
        uint16_t col = (i == step)   ? MK_LCD_YELLOW
                     : k_pat[i]      ? MK_LCD_GREEN
                     :                  MK_LCD_GRAY;
        mk_lcd_draw_str(x + 1, KEY_Y + (KEY_H - 7) / 2, k_label[i], col, MK_LCD_BLACK);
    }
}

/* ════════════════════════════════════════════════════════════ */
int main(void) {
    stdio_init_all();
    sleep_ms(200);

    printf("\n=== MusicKeyboard Smoke Test ===\n");

    /* LCD 초기화 + 컬러 테스트 */
    mk_lcd_init();

    extern void mk_lcd_raw_fill(uint16_t color);
    mk_lcd_raw_fill(MK_LCD_RED);   sleep_ms(400);
    mk_lcd_raw_fill(MK_LCD_GREEN); sleep_ms(400);
    mk_lcd_raw_fill(MK_LCD_BLUE);  sleep_ms(400);
    mk_lcd_raw_fill(MK_LCD_WHITE); sleep_ms(400);
    printf("[lcd] color test done\n");

    /* 매트릭스 초기화 */
    matrix_init();
    printf("[matrix] init ok\n");

    /* 초기 UI */
    uint8_t cur_step = 0;
    ui_draw(cur_step, false, true);

    bool     prev[4][4] = {0};
    bool     curr[4][4];
    uint32_t step_tick = 0;
    uint8_t  prev_step = 0;

    while (true) {
        /* 버튼 스캔 */
        matrix_scan(curr);
        for (int r = 0; r < 4; r++) {
            for (int c = 0; c < 4; c++) {
                if (curr[r][c] && !prev[r][c])
                    printf("PRESS   [%s]\n", KEY_NAME[r][c]);
                if (!curr[r][c] && prev[r][c])
                    printf("RELEASE [%s]\n", KEY_NAME[r][c]);
                prev[r][c] = curr[r][c];
            }
        }

        /* 스텝 애니메이션: 25 × 5ms = 125ms/step → 120 BPM
         * 전체 재그리기 대신 이전/현재 셀 2개만 업데이트 → 깜빡임 없음 */
        if (++step_tick >= 25) {
            step_tick = 0;
            prev_step = cur_step;
            cur_step  = (uint8_t)((cur_step + 1) % 16);
            ui_update_step(prev_step, cur_step, false, true);
        }

        sleep_ms(5);
    }
}
