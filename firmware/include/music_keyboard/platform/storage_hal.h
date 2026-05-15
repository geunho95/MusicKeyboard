#pragma once

#include <stdbool.h>

#include "music_keyboard/types.h"

bool mk_storage_hal_init(void);
bool mk_storage_hal_load_project(mk_app_t *app);
bool mk_storage_hal_save_project(const mk_app_t *app);
bool mk_storage_hal_load_sample_by_name(mk_app_t *app, const char *filename);
uint8_t mk_storage_hal_list_samples(char names[][32], uint8_t max_count);
