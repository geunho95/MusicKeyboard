#include <stdio.h>
#include <string.h>

#include "music_keyboard/button_map.h"
#include "music_keyboard/core/audio_engine.h"
#include "music_keyboard/platform/d200_link.h"

bool mk_d200_link_init(void) {
    puts("[host/d200] init");
    return true;
}

bool mk_d200_link_poll_button_event(mk_button_event_t *event) {
    static const mk_button_event_t scripted_events[] = {
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_SOUND},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PAD_1},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_SOUND},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PAD_5},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_WRITE},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PAD_1},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PAD_5},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PAD_9},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PAD_13},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_WRITE},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PATTERN},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PAD_1},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PATTERN},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PLAY},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_RECORD},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PAD_8},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_PAD_12},
        {MK_BUTTON_EVENT_PRESS, MK_BUTTON_RECORD},
    };
    static size_t next_event = 0;
    static uint32_t poll_count = 0;

    ++poll_count;

    if ((poll_count % 16u) != 0u || next_event >= (sizeof(scripted_events) / sizeof(scripted_events[0]))) {
        memset(event, 0, sizeof(*event));
        return false;
    }

    *event = scripted_events[next_event++];
    printf("[host/d200] button %u press\n", event->id);
    return true;
}

void mk_d200_link_update_leds(const mk_app_t *app) {
    static uint8_t last_led_state = 0xff;
    uint8_t led_state = 0u;

    if (app->record_state == MK_RECORD_ACTIVE) {
        led_state = 2u;
    } else if (app->transport.running) {
        led_state = 1u;
    }

    if (led_state == last_led_state) {
        return;
    }
    last_led_state = led_state;
    printf(
        "[host/d200] led=%s\n",
        led_state == 2u ? "record" : led_state == 1u ? "play" : "off"
    );
}

void mk_d200_link_publish_status(const mk_app_t *app) {
    printf(
        "[host/d200] bpm=%u running=%s step=%u view=%u sound=%u pattern=%u chain=%u rec=%u voices=%zu\n",
        app->transport.bpm,
        app->transport.running ? "yes" : "no",
        app->transport.current_step,
        (unsigned)app->view,
        app->selected_sound,
        app->current_pattern,
        app->pattern_chain_length,
        (unsigned)app->record_state,
        mk_audio_engine_active_voice_count(&app->audio)
    );
}
