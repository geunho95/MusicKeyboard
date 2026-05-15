#include <stdio.h>

#include "pico/stdlib.h"

#include "music_keyboard/app.h"
#include "music_keyboard/config.h"
#include "music_keyboard/platform/audio_hal.h"
#include "music_keyboard/platform/lcd_hal.h"

/* ── 스텝 타이밍 ──────────────────────────────────────────── */
static int64_t mk_step_interval_us(const mk_app_t *app) {
    return 60000000ll / ((int64_t)app->transport.bpm * 4ll);
}

/* ── LCD 테스트 패턴 ──────────────────────────────────────── */
static void lcd_test_pattern(void) {
    /* 1. 4분할 색상 블록 */
    mk_lcd_fill_rect(  0,  0, 71, 38, MK_LCD_RED);
    mk_lcd_fill_rect( 71,  0, 71, 38, MK_LCD_GREEN);
    mk_lcd_fill_rect(142,  0, 71, 38, MK_LCD_BLUE);
    mk_lcd_fill_rect(213,  0, 71, 38, MK_LCD_YELLOW);
    mk_lcd_fill_rect(  0, 38, 71, 38, MK_LCD_CYAN);
    mk_lcd_fill_rect( 71, 38, 71, 38, MK_LCD_MAGENTA);
    mk_lcd_fill_rect(142, 38, 71, 38, MK_LCD_WHITE);
    mk_lcd_fill_rect(213, 38, 71, 38, MK_LCD_GRAY);
    sleep_ms(1500);

    /* 2. 화면 가득 흰색 → 검정 */
    mk_lcd_clear(MK_LCD_WHITE); sleep_ms(500);
    mk_lcd_clear(MK_LCD_BLACK); sleep_ms(500);

    /* 3. UI 레이아웃 미리보기
     *
     *  ┌──────────────────────────────────────────────┐  y=0
     *  │  STATUS BAR (모드, BPM, 옥타브)   284×14    │
     *  ├──────────────────────────────────────────────┤  y=14
     *  │  MAIN AREA (뷰별 콘텐츠)          284×48    │
     *  ├──────────────────────────────────────────────┤  y=62
     *  │  TIMELINE  (16스텝)               284×14    │
     *  └──────────────────────────────────────────────┘  y=76
     */
    mk_lcd_clear(MK_LCD_BLACK);

    /* 상단 상태바 (어두운 회색) */
    mk_lcd_fill_rect(0, 0, MK_LCD_W, 14, MK_LCD_DKGRAY);
    mk_lcd_draw_str2(4, 3, "PERF", MK_LCD_WHITE, MK_LCD_DKGRAY);
    mk_lcd_draw_str(100, 4, "BPM:120", MK_LCD_CYAN, MK_LCD_DKGRAY);
    mk_lcd_draw_str(210, 4, "OCT:0", MK_LCD_YELLOW, MK_LCD_DKGRAY);

    /* 메인 영역 구분선 */
    mk_lcd_fill_rect(0, 14, MK_LCD_W, 1, MK_LCD_GRAY);
    mk_lcd_fill_rect(0, 61, MK_LCD_W, 1, MK_LCD_GRAY);

    /* 메인 영역: 사운드 이름 샘플 */
    mk_lcd_draw_str2(8, 25, "NO SAMPLE", MK_LCD_GRAY, MK_LCD_BLACK);

    /* 하단 타임라인: 16스텝 */
    int step_w  = (MK_LCD_W - 2) / 16;   /* 각 스텝 너비 */
    int tl_y    = 63;
    int tl_h    = MK_LCD_H - tl_y - 1;
    for (int i = 0; i < 16; ++i) {
        int sx = 1 + i * step_w;
        /* 4스텝마다 밝게 (박자 구분) */
        uint16_t clr = ((i % 4) == 0) ? MK_LCD_BLUE : MK_LCD_DKGRAY;
        mk_lcd_fill_rect(sx, tl_y, step_w - 1, tl_h, clr);
    }

    /* 0번 스텝 현재 위치 표시 */
    mk_lcd_fill_rect(1, tl_y, step_w - 1, tl_h, MK_LCD_GREEN);

    sleep_ms(2000);
}

/* ── 샘플 선택 뷰 ────────────────────────────────────────── */
static void lcd_draw_sample_select(mk_app_t *app) {
    mk_lcd_clear(MK_LCD_BLACK);
    mk_lcd_fill_rect(0, 0, MK_LCD_W, 14, MK_LCD_DKGRAY);
    mk_lcd_draw_str2(4, 3, "SMPL", MK_LCD_CYAN, MK_LCD_DKGRAY);

    if (app->sample_count == 0) {
        mk_lcd_draw_str(4, 30, "NO FILES IN /samples/", MK_LCD_GRAY, MK_LCD_BLACK);
        return;
    }

    /* 3줄 표시: 이전/현재/다음 */
    char buf[40];
    int8_t cur = (int8_t)app->sample_cursor;
    for (int8_t row = -1; row <= 1; row++) {
        int8_t idx = cur + row;
        if (idx < 0 || idx >= app->sample_count) continue;
        int y = 22 + row * 18;
        uint16_t fg = (row == 0) ? MK_LCD_WHITE : MK_LCD_GRAY;
        uint16_t bg = (row == 0) ? MK_LCD_BLUE  : MK_LCD_BLACK;
        if (row == 0) mk_lcd_fill_rect(0, y - 2, MK_LCD_W, 16, MK_LCD_BLUE);
        snprintf(buf, sizeof(buf), "%02u %s", (unsigned)idx + 1, app->sample_names[idx]);
        mk_lcd_draw_str(4, y, buf, fg, bg);
    }

    /* 인덱스 표시 */
    snprintf(buf, sizeof(buf), "%02u/%02u", (unsigned)app->sample_cursor + 1,
             (unsigned)app->sample_count);
    mk_lcd_draw_str(230, 4, buf, MK_LCD_WHITE, MK_LCD_DKGRAY);
}

/* ── 런타임 LCD 업데이트 ──────────────────────────────────── */
static void lcd_update(mk_app_t *app) {
    static uint16_t last_step    = 0xFFFFu;
    static bool     last_running = false;
    static uint8_t  last_view    = 0xFFu;
    static uint8_t  last_cursor  = 0xFFu;

    bool changed = (app->transport.current_step != last_step ||
                    app->transport.running       != last_running ||
                    (uint8_t)app->view          != last_view ||
                    app->sample_cursor          != last_cursor);
    if (!changed) return;

    last_step    = app->transport.current_step;
    last_running = app->transport.running;
    last_view    = (uint8_t)app->view;
    last_cursor  = app->sample_cursor;

    if (app->view == MK_VIEW_SAMPLE_SELECT) {
        lcd_draw_sample_select(app);
        return;
    }

    /* ── 상태바: 모드명 ── */
    const char *view_names[] = {"PERF", "EDIT", "REC ", "SND ", "PAT ", "BPM ", "FX  ", "SMPL"};
    uint8_t vi = (uint8_t)app->view;
    if (vi >= 8) vi = 0;
    uint16_t bar_clr = (app->record_state == MK_RECORD_ACTIVE) ? MK_LCD_RED : MK_LCD_DKGRAY;
    mk_lcd_fill_rect(0, 0, MK_LCD_W, 14, bar_clr);
    mk_lcd_draw_str2(4, 3, view_names[vi], MK_LCD_WHITE, bar_clr);

    /* BPM */
    char buf[16];
    snprintf(buf, sizeof(buf), "BPM:%u", app->transport.bpm);
    mk_lcd_draw_str(100, 4, buf, MK_LCD_CYAN, bar_clr);

    /* 재생/정지 아이콘 */
    mk_lcd_draw_str(246, 4, app->transport.running ? "PLAY" : "STOP",
                    app->transport.running ? MK_LCD_GREEN : MK_LCD_GRAY, bar_clr);

    /* ── 타임라인 16스텝 ── */
    int step_w = (MK_LCD_W - 2) / 16;
    int tl_y   = 63;
    int tl_h   = MK_LCD_H - tl_y - 1;
    uint16_t cur_step = app->transport.current_step;

    for (int i = 0; i < 16; ++i) {
        int sx = 1 + i * step_w;
        uint16_t clr;
        if ((uint16_t)i == cur_step) {
            /* 현재 스텝 */
            clr = app->transport.running ? MK_LCD_GREEN : MK_LCD_YELLOW;
        } else {
            /* 일반: 박자마다 색 구분 */
            clr = ((i % 4) == 0) ? MK_LCD_BLUE : MK_LCD_DKGRAY;
        }
        mk_lcd_fill_rect(sx, tl_y, step_w - 1, tl_h, clr);
    }
}

/* ══════════════════════════════════════════════════════════
 * main
 * ══════════════════════════════════════════════════════════ */
int main(void) {
    mk_app_t app;
    int16_t audio_block[MK_AUDIO_BLOCK_FRAMES * MK_AUDIO_CHANNELS];
    absolute_time_t next_step;
    int64_t step_interval_us;

    stdio_init_all();
    sleep_ms(1200);
    puts("[rp2350/main] starting");

    /* LCD 먼저 초기화 (SD보다 앞에 — SPI 핀 공유, SD CS는 LCD init 내부에서 HIGH 유지) */
    mk_lcd_init();
    lcd_test_pattern();

    /* 앱 초기화 (SD 포함) */
    mk_app_init(&app);

    step_interval_us = mk_step_interval_us(&app);
    next_step = delayed_by_us(get_absolute_time(), step_interval_us);

    /* 첫 LCD 상태 */
    lcd_update(&app);

    /* GP17/GP18 직접 읽기 테스트 */
    static uint32_t dbg_tick = 0;
    gpio_init(17); gpio_set_dir(17, GPIO_IN); gpio_pull_up(17);
    gpio_init(18); gpio_set_dir(18, GPIO_IN); gpio_pull_up(18);

    while (true) {
        absolute_time_t now = get_absolute_time();

        if (++dbg_tick >= 50000) {
            dbg_tick = 0;
            printf("[dbg] GP17=%u GP18=%u\n", gpio_get(17), gpio_get(18));
        }

        mk_app_tick(&app);
        step_interval_us = mk_step_interval_us(&app);

        if (absolute_time_diff_us(next_step, now) >= 0) {
            mk_app_step_transport(&app);
            next_step = delayed_by_us(next_step, step_interval_us);
        }

        while (mk_audio_hal_writable_frames() >= MK_AUDIO_BLOCK_FRAMES) {
            mk_app_render_audio(&app, audio_block, MK_AUDIO_BLOCK_FRAMES);
            if (!mk_audio_hal_submit_frames(audio_block, MK_AUDIO_BLOCK_FRAMES)) {
                break;
            }
        }

        /* LCD: 상태가 바뀔 때만 업데이트 */
        lcd_update(&app);

        tight_loop_contents();
    }

    return 0;
}
