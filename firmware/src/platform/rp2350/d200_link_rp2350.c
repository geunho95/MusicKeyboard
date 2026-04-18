#include <stdio.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "pico/stdlib.h"
#include "pico/time.h"

#include "music_keyboard/button_map.h"
#include "music_keyboard/platform/d200_link.h"
#include "music_keyboard/platform/rp2350_pinmap.h"

typedef struct {
    uint gpio;
    uint8_t logical_id;
    bool previous_pressed;
    absolute_time_t last_change;
} mk_rp2350_button_t;

static mk_rp2350_button_t g_buttons[] = {
    {MK_RP2350_BUTTON_BLUE_0_PIN, MK_BUTTON_PAD_1, false, 0},
    {MK_RP2350_BUTTON_BLUE_1_PIN, MK_BUTTON_PAD_2, false, 0},
    {MK_RP2350_BUTTON_BLUE_2_PIN, MK_BUTTON_PAD_3, false, 0},
    {MK_RP2350_BUTTON_BLUE_3_PIN, MK_BUTTON_PAD_4, false, 0},
    {MK_RP2350_BUTTON_RED_PIN, MK_BUTTON_RECORD, false, 0},
};

bool mk_d200_link_init(void) {
    gpio_set_function(MK_RP2350_ONBOARD_LED_PIN, GPIO_FUNC_PWM);
    {
        uint slice = pwm_gpio_to_slice_num(MK_RP2350_ONBOARD_LED_PIN);
        pwm_set_wrap(slice, 255);
        pwm_set_clkdiv(slice, 4.0f);
        pwm_set_gpio_level(MK_RP2350_ONBOARD_LED_PIN, 0);
        pwm_set_enabled(slice, true);
    }

    for (size_t i = 0; i < (sizeof(g_buttons) / sizeof(g_buttons[0])); ++i) {
        gpio_init(g_buttons[i].gpio);
        gpio_set_dir(g_buttons[i].gpio, GPIO_IN);
        gpio_pull_up(g_buttons[i].gpio);
        g_buttons[i].previous_pressed = false;
        g_buttons[i].last_change = get_absolute_time();
    }

    printf(
        "[rp2350/buttons] blue={%u,%u,%u,%u} red=%u\n",
        MK_RP2350_BUTTON_BLUE_0_PIN,
        MK_RP2350_BUTTON_BLUE_1_PIN,
        MK_RP2350_BUTTON_BLUE_2_PIN,
        MK_RP2350_BUTTON_BLUE_3_PIN,
        MK_RP2350_BUTTON_RED_PIN
    );
    return true;
}

bool mk_d200_link_poll_button_event(mk_button_event_t *event) {
    absolute_time_t now = get_absolute_time();

    memset(event, 0, sizeof(*event));

    for (uint8_t i = 0; i < (sizeof(g_buttons) / sizeof(g_buttons[0])); ++i) {
        bool pressed = !gpio_get(g_buttons[i].gpio);

        if (pressed != g_buttons[i].previous_pressed) {
            if (absolute_time_diff_us(g_buttons[i].last_change, now) < 15000) {
                continue;
            }

            g_buttons[i].previous_pressed = pressed;
            g_buttons[i].last_change = now;

            if (pressed) {
                event->type = MK_BUTTON_EVENT_PRESS;
                event->id = g_buttons[i].logical_id;
                return true;
            }

            event->type = MK_BUTTON_EVENT_RELEASE;
            event->id = g_buttons[i].logical_id;
            return true;
        }
    }

    return false;
}

void mk_d200_link_publish_status(const mk_app_t *app) {
    static uint16_t last_step = 0xffffu;
    static mk_record_state_t last_record_state = 0xff;
    static bool last_running = false;
    static uint8_t last_view = 0xffu;

    if (app->transport.current_step == last_step &&
        app->record_state == last_record_state &&
        app->transport.running == last_running &&
        app->view == last_view) {
        return;
    }

    last_step = app->transport.current_step;
    last_record_state = app->record_state;
    last_running = app->transport.running;
    last_view = (uint8_t)app->view;

    printf(
        "[rp2350/status] running=%u step=%u rec=%u view=%u sound=%u pattern=%u chain=%u\n",
        app->transport.running ? 1u : 0u,
        app->transport.current_step,
        (unsigned)app->record_state,
        (unsigned)app->view,
        app->selected_sound,
        app->current_pattern,
        app->pattern_chain_length
    );
}

void mk_d200_link_update_leds(const mk_app_t *app) {
    uint32_t level = 0;

    if (app->record_state == MK_RECORD_ACTIVE) {
        uint64_t ms = to_ms_since_boot(get_absolute_time());
        level = ((ms / 120u) & 1u) ? 255u : 0u;
    } else if (app->transport.running) {
        uint16_t step = app->transport.current_step;
        level = ((step % 4u) == 0u) ? 180u : 32u;
    }

    pwm_set_gpio_level(MK_RP2350_ONBOARD_LED_PIN, level);
}
