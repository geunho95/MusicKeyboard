#pragma once

#include <stdint.h>

#include "music_keyboard/types.h"

void mk_transport_init(mk_transport_t *transport, uint16_t bpm, uint8_t bars);
void mk_transport_toggle(mk_transport_t *transport);
void mk_transport_stop(mk_transport_t *transport);
void mk_transport_advance(mk_transport_t *transport);
uint16_t mk_transport_total_steps(const mk_transport_t *transport);
