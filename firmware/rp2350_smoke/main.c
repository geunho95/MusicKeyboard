#include <stdio.h>

#include "pico/stdlib.h"

int main(void) {
    stdio_init_all();

    sleep_ms(1200);

    while (true) {
        printf("MusicKeyboard RP2350 smoke target alive\n");
        sleep_ms(1000);
    }

    return 0;
}
