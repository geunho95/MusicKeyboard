#include "music_keyboard/core/transport.h"
#include "music_keyboard/config.h"

void mk_transport_init(mk_transport_t *transport, uint16_t bpm, uint8_t bars) {
    transport->bpm = bpm;
    transport->bars = (bars == 0u || bars > MK_MAX_BARS) ? 1u : bars;
    transport->current_step = 0;
    transport->running = false;
}

void mk_transport_toggle(mk_transport_t *transport) {
    transport->running = !transport->running;
}

void mk_transport_stop(mk_transport_t *transport) {
    transport->running = false;
    transport->current_step = 0;
}

void mk_transport_advance(mk_transport_t *transport) {
    uint16_t total_steps = mk_transport_total_steps(transport);
    if (total_steps == 0u) {
        transport->current_step = 0;
        return;
    }

    transport->current_step = (uint16_t)((transport->current_step + 1u) % total_steps);
}

uint16_t mk_transport_total_steps(const mk_transport_t *transport) {
    return (uint16_t)(transport->bars * MK_STEPS_PER_BAR);
}
