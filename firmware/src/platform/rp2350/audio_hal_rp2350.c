#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "hardware/gpio.h"
#include "hardware/pwm.h"
#include "hardware/sync.h"
#include "pico/time.h"

#include "music_keyboard/config.h"
#include "music_keyboard/platform/audio_hal.h"
#include "music_keyboard/platform/rp2350_pinmap.h"

#define MK_RP2350_AUDIO_FIFO_CAPACITY (MK_AUDIO_BLOCK_FRAMES * 8)

static int16_t g_audio_fifo[MK_RP2350_AUDIO_FIFO_CAPACITY];
static volatile uint16_t g_audio_read_index;
static volatile uint16_t g_audio_write_index;
static volatile uint16_t g_audio_count;
static struct repeating_timer g_audio_timer;
static uint g_audio_pwm_slice;
static bool g_audio_initialized;

static bool mk_audio_hal_timer_callback(__unused struct repeating_timer *timer) {
    uint16_t level = 128u;

    if (g_audio_count > 0u) {
        int16_t sample = g_audio_fifo[g_audio_read_index];
        uint16_t scaled = (uint16_t)(((int32_t)sample + 32768) >> 8);

        g_audio_read_index = (uint16_t)((g_audio_read_index + 1u) % MK_RP2350_AUDIO_FIFO_CAPACITY);

        uint32_t irq_state = save_and_disable_interrupts();
        if (g_audio_count > 0u) {
            --g_audio_count;
        }
        restore_interrupts(irq_state);

        level = scaled;
    }

    pwm_set_gpio_level(MK_RP2350_TEMP_AUDIO_PWM_PIN, level);
    return true;
}

bool mk_audio_hal_init(const mk_audio_hal_config_t *config) {
    int64_t interval_us;

    gpio_set_function(MK_RP2350_TEMP_AUDIO_PWM_PIN, GPIO_FUNC_PWM);
    g_audio_pwm_slice = pwm_gpio_to_slice_num(MK_RP2350_TEMP_AUDIO_PWM_PIN);

    pwm_set_wrap(g_audio_pwm_slice, 255);
    pwm_set_clkdiv(g_audio_pwm_slice, 4.0f);
    pwm_set_enabled(g_audio_pwm_slice, true);
    pwm_set_gpio_level(MK_RP2350_TEMP_AUDIO_PWM_PIN, 128u);

    memset((void *)g_audio_fifo, 0, sizeof(g_audio_fifo));
    g_audio_read_index = 0u;
    g_audio_write_index = 0u;
    g_audio_count = 0u;

    interval_us = -(1000000ll / (int64_t)config->sample_rate_hz);
    if (interval_us == 0) {
        interval_us = -41;
    }

    g_audio_initialized =
        add_repeating_timer_us(interval_us, mk_audio_hal_timer_callback, NULL, &g_audio_timer);
    return g_audio_initialized;
}

void mk_audio_hal_shutdown(void) {
    if (g_audio_initialized) {
        cancel_repeating_timer(&g_audio_timer);
        g_audio_initialized = false;
    }
}

size_t mk_audio_hal_writable_frames(void) {
    uint32_t irq_state = save_and_disable_interrupts();
    size_t writable = (size_t)(MK_RP2350_AUDIO_FIFO_CAPACITY - g_audio_count);
    restore_interrupts(irq_state);
    return writable;
}

bool mk_audio_hal_submit_frames(const int16_t *interleaved_frames, size_t frame_count) {
    uint32_t irq_state;

    if (frame_count > mk_audio_hal_writable_frames()) {
        return false;
    }

    irq_state = save_and_disable_interrupts();
    for (size_t i = 0; i < frame_count; ++i) {
        int32_t left = interleaved_frames[i * MK_AUDIO_CHANNELS];
        int32_t right = interleaved_frames[(i * MK_AUDIO_CHANNELS) + 1u];
        int16_t mono = (int16_t)((left + right) / 2);

        g_audio_fifo[g_audio_write_index] = mono;
        g_audio_write_index = (uint16_t)((g_audio_write_index + 1u) % MK_RP2350_AUDIO_FIFO_CAPACITY);
        ++g_audio_count;
    }
    restore_interrupts(irq_state);

    return true;
}
