#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "music_keyboard/button_map.h"
#include "music_keyboard/platform/d200_link.h"
#include "music_keyboard/platform/rp2350_pinmap.h"

/* ═══════════════════════════════════════════════════════════
 * 4×4 매트릭스
 * COL → OUTPUT (순서대로 LOW 구동)
 * ROW → INPUT  (내부 풀업, LOW 감지)
 * 다이오드: A→ROW, K(띠)→COL 방향
 * ══════════════════════════════════════════════════════════ */
static const uint8_t k_row_pins[4] = {
    MK_RP2350_MATRIX_ROW0_PIN,
    MK_RP2350_MATRIX_ROW1_PIN,
    MK_RP2350_MATRIX_ROW2_PIN,
    MK_RP2350_MATRIX_ROW3_PIN,
};
static const uint8_t k_col_pins[4] = {
    MK_RP2350_MATRIX_COL0_PIN,
    MK_RP2350_MATRIX_COL1_PIN,
    MK_RP2350_MATRIX_COL2_PIN,
    MK_RP2350_MATRIX_COL3_PIN,
};

/* 슬롯 15 (ROW3-COL3) 는 미사용 — 스캔은 하되 이벤트 무시 */
static bool            g_mat_prev[16];
static absolute_time_t g_mat_debounce[16];

/* ═══════════════════════════════════════════════════════════
 * 엔코더 푸시 버튼 (직결 GPIO)
 * ══════════════════════════════════════════════════════════ */
typedef struct {
    uint8_t gpio;
    uint8_t logical_id;
    bool    prev_pressed;
    absolute_time_t last_change;
} mk_enc_sw_t;

static mk_enc_sw_t g_enc_sw[2] = {
    {MK_RP2350_ENC1_SW_PIN, MK_ENC1_SW, false, 0},
    {MK_RP2350_ENC2_SW_PIN, MK_ENC2_SW, false, 0},
};

/* ═══════════════════════════════════════════════════════════
 * 엔코더 회전 (Gray code 4-상태 머신)
 *
 * state = (prev_A << 1) | prev_B  (0~3)
 * 전이 테이블: +1=CW, -1=CCW, 0=무효
 *   00→01 CW,  01→11 CW,  11→10 CW,  10→00 CW
 *   00→10 CCW, 10→11 CCW, 11→01 CCW, 01→00 CCW
 * ══════════════════════════════════════════════════════════ */
typedef struct {
    uint8_t gpio_a;
    uint8_t gpio_b;
    uint8_t logical_id;
    uint8_t prev_ab;    /* (A<<1)|B 이전 값 */
    int8_t  accumulator; /* 누적: ±4 에서 1클릭 확정 */
} mk_encoder_t;

/* 전이 테이블: enc_dir[prev][cur] */
static const int8_t k_enc_dir[4][4] = {
    /* cur: 00  01  10  11 */
    /* 00 */ { 0, -1,  1,  0},
    /* 01 */ { 1,  0,  0, -1},
    /* 10 */ {-1,  0,  0,  1},
    /* 11 */ { 0,  1, -1,  0},
};

static mk_encoder_t g_encoders[2] = {
    {MK_RP2350_ENC1_A_PIN, MK_RP2350_ENC1_B_PIN, MK_ENC1, 0, 0},
    {MK_RP2350_ENC2_A_PIN, MK_RP2350_ENC2_B_PIN, MK_ENC2, 0, 0},
};

/* ═══════════════════════════════════════════════════════════
 * 이벤트 FIFO  (lock-free 단일 코어용)
 * ══════════════════════════════════════════════════════════ */
#define MK_EVENT_QUEUE_SIZE 32u

static mk_button_event_t g_event_q[MK_EVENT_QUEUE_SIZE];
static volatile uint8_t  g_q_head = 0;
static volatile uint8_t  g_q_tail = 0;

static void q_push(mk_button_event_type_t type, uint8_t id) {
    uint8_t next = (uint8_t)((g_q_tail + 1u) % MK_EVENT_QUEUE_SIZE);
    if (next == g_q_head) return;  /* 가득 참 → 버림 */
    g_event_q[g_q_tail].type = type;
    g_event_q[g_q_tail].id   = id;
    g_q_tail = next;
}

static bool q_pop(mk_button_event_t *out) {
    if (g_q_head == g_q_tail) return false;
    *out    = g_event_q[g_q_head];
    g_q_head = (uint8_t)((g_q_head + 1u) % MK_EVENT_QUEUE_SIZE);
    return true;
}

/* ═══════════════════════════════════════════════════════════
 * 초기화
 * ══════════════════════════════════════════════════════════ */
bool mk_d200_link_init(void) {
    /* ── 온보드 LED (PWM) ── */
    gpio_set_function(MK_RP2350_ONBOARD_LED_PIN, GPIO_FUNC_PWM);
    {
        uint slice = pwm_gpio_to_slice_num(MK_RP2350_ONBOARD_LED_PIN);
        pwm_set_wrap(slice, 255);
        pwm_set_clkdiv(slice, 4.0f);
        pwm_set_gpio_level(MK_RP2350_ONBOARD_LED_PIN, 0);
        pwm_set_enabled(slice, true);
    }

    /* ── 매트릭스 COL (OUTPUT, 기본 HIGH) ── */
    for (int c = 0; c < 4; ++c) {
        gpio_init(k_col_pins[c]);
        gpio_set_dir(k_col_pins[c], GPIO_OUT);
        gpio_put(k_col_pins[c], 1);
    }

    /* ── 매트릭스 ROW (INPUT, 내부 풀업) ── */
    for (int r = 0; r < 4; ++r) {
        gpio_init(k_row_pins[r]);
        gpio_set_dir(k_row_pins[r], GPIO_IN);
        gpio_pull_up(k_row_pins[r]);
    }

    /* 매트릭스 디바운스 초기화 */
    absolute_time_t now = get_absolute_time();
    for (int i = 0; i < 16; ++i) {
        g_mat_prev[i]     = false;
        g_mat_debounce[i] = now;
    }

    /* ── 엔코더 푸시 버튼 (INPUT, 풀업) ── */
    for (int i = 0; i < 2; ++i) {
        gpio_init(g_enc_sw[i].gpio);
        gpio_set_dir(g_enc_sw[i].gpio, GPIO_IN);
        gpio_pull_up(g_enc_sw[i].gpio);
        g_enc_sw[i].prev_pressed = false;
        g_enc_sw[i].last_change  = now;
    }

    /* ── 엔코더 A/B (INPUT, 풀업) ── */
    for (int i = 0; i < 2; ++i) {
        gpio_init(g_encoders[i].gpio_a);
        gpio_set_dir(g_encoders[i].gpio_a, GPIO_IN);
        gpio_pull_up(g_encoders[i].gpio_a);

        gpio_init(g_encoders[i].gpio_b);
        gpio_set_dir(g_encoders[i].gpio_b, GPIO_IN);
        gpio_pull_up(g_encoders[i].gpio_b);

        /* 현재 상태로 초기화 (부팅 시 허위 이벤트 방지) */
        uint8_t a = gpio_get(g_encoders[i].gpio_a) ? 1u : 0u;
        uint8_t b = gpio_get(g_encoders[i].gpio_b) ? 1u : 0u;
        g_encoders[i].prev_ab    = (uint8_t)((a << 1) | b);
        g_encoders[i].accumulator = 0;
    }

    printf("[input] matrix rows={%u,%u,%u,%u} cols={%u,%u,%u,%u}\n",
           k_row_pins[0], k_row_pins[1], k_row_pins[2], k_row_pins[3],
           k_col_pins[0], k_col_pins[1], k_col_pins[2], k_col_pins[3]);
    printf("[input] enc_sw={%u,%u} enc_a={%u,%u} enc_b={%u,%u}\n",
           g_enc_sw[0].gpio, g_enc_sw[1].gpio,
           g_encoders[0].gpio_a, g_encoders[1].gpio_a,
           g_encoders[0].gpio_b, g_encoders[1].gpio_b);

    return true;
}

/* ═══════════════════════════════════════════════════════════
 * 스캔 헬퍼 — 매트릭스 한 프레임 스캔 후 FIFO에 적재
 * ══════════════════════════════════════════════════════════ */
static void scan_matrix(void) {
    absolute_time_t now = get_absolute_time();

    for (int c = 0; c < 4; ++c) {
        /* 해당 COL만 LOW로 구동 */
        gpio_put(k_col_pins[c], 0);
        sleep_us(5);  /* 안정화 대기 */

        for (int r = 0; r < 4; ++r) {
            uint8_t idx     = (uint8_t)(r * 4 + c);
            bool    pressed = !gpio_get(k_row_pins[r]);

            if (pressed != g_mat_prev[idx]) {
                /* 디바운스: 15ms 미만이면 무시 */
                if (absolute_time_diff_us(g_mat_debounce[idx], now) < 15000) {
                    continue;
                }
                g_mat_prev[idx]     = pressed;
                g_mat_debounce[idx] = now;

                /* 미사용 슬롯(15)은 이벤트 발생 안 함 */
                if (idx == MK_BTN_UNUSED) continue;

                q_push(pressed ? MK_BUTTON_EVENT_PRESS : MK_BUTTON_EVENT_RELEASE,
                       idx);
            }
        }

        /* COL 복귀 */
        gpio_put(k_col_pins[c], 1);
    }
}

/* ═══════════════════════════════════════════════════════════
 * 스캔 헬퍼 — 엔코더 푸시 버튼
 * ══════════════════════════════════════════════════════════ */
static void scan_enc_sw(void) {
    absolute_time_t now = get_absolute_time();

    for (int i = 0; i < 2; ++i) {
        bool pressed = !gpio_get(g_enc_sw[i].gpio);
        if (pressed == g_enc_sw[i].prev_pressed) continue;
        if (absolute_time_diff_us(g_enc_sw[i].last_change, now) < 15000) continue;

        g_enc_sw[i].prev_pressed = pressed;
        g_enc_sw[i].last_change  = now;
        q_push(pressed ? MK_BUTTON_EVENT_PRESS : MK_BUTTON_EVENT_RELEASE,
               g_enc_sw[i].logical_id);
    }
}

/* ═══════════════════════════════════════════════════════════
 * 스캔 헬퍼 — 엔코더 회전 (Gray code)
 * accumulator ±4 초과 시 1클릭 확정 → 노이즈 필터링
 * ══════════════════════════════════════════════════════════ */
static void scan_encoders(void) {
    for (int i = 0; i < 2; ++i) {
        uint8_t a   = gpio_get(g_encoders[i].gpio_a) ? 1u : 0u;
        uint8_t b   = gpio_get(g_encoders[i].gpio_b) ? 1u : 0u;
        uint8_t cur = (uint8_t)((a << 1) | b);

        if (cur == g_encoders[i].prev_ab) continue;

        int8_t delta = k_enc_dir[g_encoders[i].prev_ab][cur];
        g_encoders[i].prev_ab     = cur;
        g_encoders[i].accumulator = (int8_t)(g_encoders[i].accumulator + delta);

        /* 엔코더는 클릭당 4 펄스 (보통) → 누적 ±4 에서 확정 */
        if (g_encoders[i].accumulator >= 4) {
            g_encoders[i].accumulator = 0;
            q_push(MK_BUTTON_EVENT_ENC_CW, g_encoders[i].logical_id);
        } else if (g_encoders[i].accumulator <= -4) {
            g_encoders[i].accumulator = 0;
            q_push(MK_BUTTON_EVENT_ENC_CCW, g_encoders[i].logical_id);
        }
    }
}

/* ═══════════════════════════════════════════════════════════
 * 공개 API
 * ══════════════════════════════════════════════════════════ */
bool mk_d200_link_poll_button_event(mk_button_event_t *event) {
    /* 모든 입력 스캔 후 FIFO에서 하나씩 꺼냄 */
    scan_matrix();
    scan_enc_sw();
    scan_encoders();
    return q_pop(event);
}

void mk_d200_link_publish_status(const mk_app_t *app) {
    static uint16_t          last_step    = 0xffffu;
    static mk_record_state_t last_rec     = (mk_record_state_t)0xffu;
    static bool              last_running = false;
    static uint8_t           last_view    = 0xffu;

    if (app->transport.current_step == last_step &&
        app->record_state           == last_rec  &&
        app->transport.running      == last_running &&
        app->view                   == (mk_ui_view_t)last_view) {
        return;
    }

    last_step    = app->transport.current_step;
    last_rec     = app->record_state;
    last_running = app->transport.running;
    last_view    = (uint8_t)app->view;

    printf("[status] run=%u step=%u rec=%u view=%u snd=%u pat=%u chain=%u\n",
           app->transport.running      ? 1u : 0u,
           app->transport.current_step,
           (unsigned)app->record_state,
           (unsigned)app->view,
           app->selected_sound,
           app->current_pattern,
           app->pattern_chain_length);
}

void mk_d200_link_update_leds(const mk_app_t *app) {
    uint32_t level = 0;

    if (app->record_state == MK_RECORD_ACTIVE) {
        if (app->transport.running) {
            /* 루프 진행률에 비례한 밝기 */
            uint32_t total = (uint32_t)MK_STEPS_PER_BAR * app->transport.bars;
            if (total > 0u) {
                level = (app->transport.current_step + 1u) * 255u / total;
            }
        } else {
            level = 128u;  /* 녹음 모드 진입 표시 */
        }
    } else if (app->record_state == MK_RECORD_ARMED) {
        uint64_t ms = to_ms_since_boot(get_absolute_time());
        level = ((ms / 100u) & 1u) ? 255u : 0u;  /* 100ms 깜박임 */
    } else if (app->transport.running) {
        /* 4스텝마다 밝게 → 박자 표시 */
        level = ((app->transport.current_step % 4u) == 0u) ? 180u : 32u;
    }

    pwm_set_gpio_level(MK_RP2350_ONBOARD_LED_PIN, level);
}
