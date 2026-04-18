#pragma once

#include <stddef.h>
#include <stdint.h>

#include "music_keyboard/types.h"

void mk_app_init(mk_app_t *app);
void mk_app_tick(mk_app_t *app);
void mk_app_step_transport(mk_app_t *app);
void mk_app_handle_button_event(mk_app_t *app, mk_button_event_t event);
void mk_app_render_audio(mk_app_t *app, int16_t *interleaved_frames, size_t frame_count);
