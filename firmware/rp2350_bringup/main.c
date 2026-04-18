#include <stdio.h>

#include "pico/stdlib.h"

#include "music_keyboard/app.h"
#include "music_keyboard/config.h"
#include "music_keyboard/platform/audio_hal.h"

static int64_t mk_step_interval_us(const mk_app_t *app) {
    return 60000000ll / ((int64_t)app->transport.bpm * 4ll);
}

int main(void) {
    mk_app_t app;
    int16_t audio_block[MK_AUDIO_BLOCK_FRAMES * MK_AUDIO_CHANNELS];
    absolute_time_t next_step;
    int64_t step_interval_us;

    stdio_init_all();
    sleep_ms(1200);
    puts("[rp2350/main] bring-up starting");

    mk_app_init(&app);
    step_interval_us = mk_step_interval_us(&app);
    next_step = delayed_by_us(get_absolute_time(), step_interval_us);

    while (true) {
        absolute_time_t now = get_absolute_time();

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

        tight_loop_contents();
    }

    return 0;
}
