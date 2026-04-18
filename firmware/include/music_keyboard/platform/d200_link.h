#pragma once

#include <stdbool.h>

#include "music_keyboard/types.h"

bool mk_d200_link_init(void);
bool mk_d200_link_poll_button_event(mk_button_event_t *event);
void mk_d200_link_publish_status(const mk_app_t *app);
void mk_d200_link_update_leds(const mk_app_t *app);
