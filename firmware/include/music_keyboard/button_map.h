#pragma once

#include <stdint.h>

typedef enum {
    MK_BUTTON_SOUND = 0,
    MK_BUTTON_PATTERN,
    MK_BUTTON_BPM,
    MK_BUTTON_PARAM_A,
    MK_BUTTON_PARAM_B,
    MK_BUTTON_PAD_1,
    MK_BUTTON_PAD_2,
    MK_BUTTON_PAD_3,
    MK_BUTTON_PAD_4,
    MK_BUTTON_RECORD,
    MK_BUTTON_PAD_5,
    MK_BUTTON_PAD_6,
    MK_BUTTON_PAD_7,
    MK_BUTTON_PAD_8,
    MK_BUTTON_FX,
    MK_BUTTON_PAD_9,
    MK_BUTTON_PAD_10,
    MK_BUTTON_PAD_11,
    MK_BUTTON_PAD_12,
    MK_BUTTON_PLAY,
    MK_BUTTON_PAD_13,
    MK_BUTTON_PAD_14,
    MK_BUTTON_PAD_15,
    MK_BUTTON_PAD_16,
    MK_BUTTON_WRITE,
    MK_BUTTON_COUNT
} mk_button_id_t;

static inline int8_t mk_button_id_to_pad_index(uint8_t button_id) {
    switch (button_id) {
        case MK_BUTTON_PAD_1:
            return 0;
        case MK_BUTTON_PAD_2:
            return 1;
        case MK_BUTTON_PAD_3:
            return 2;
        case MK_BUTTON_PAD_4:
            return 3;
        case MK_BUTTON_PAD_5:
            return 4;
        case MK_BUTTON_PAD_6:
            return 5;
        case MK_BUTTON_PAD_7:
            return 6;
        case MK_BUTTON_PAD_8:
            return 7;
        case MK_BUTTON_PAD_9:
            return 8;
        case MK_BUTTON_PAD_10:
            return 9;
        case MK_BUTTON_PAD_11:
            return 10;
        case MK_BUTTON_PAD_12:
            return 11;
        case MK_BUTTON_PAD_13:
            return 12;
        case MK_BUTTON_PAD_14:
            return 13;
        case MK_BUTTON_PAD_15:
            return 14;
        case MK_BUTTON_PAD_16:
            return 15;
        default:
            return -1;
    }
}
