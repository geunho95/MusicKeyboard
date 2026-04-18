#pragma once

#include <stdint.h>

#include "music_keyboard/types.h"

void mk_sample_bank_init(mk_sample_bank_t *bank);
const mk_sample_slot_t *mk_sample_bank_get(const mk_sample_bank_t *bank, uint8_t slot);
