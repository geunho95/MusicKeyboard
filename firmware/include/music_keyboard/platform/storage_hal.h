#pragma once

#include <stdbool.h>

#include "music_keyboard/types.h"

bool mk_storage_hal_init(void);
bool mk_storage_hal_load_project(mk_app_t *app);
bool mk_storage_hal_save_project(const mk_app_t *app);
