/* Exercise 3: GPIO speed settings and edge rates.
 *
 * A square wave on DBG0 (PG2) with the output speed set by SPEED:
 *   make flash EX=03 EXTRA_CFLAGS=-DSPEED=0     0 Low, 1 Medium, 2 High, 3 Very high (default)
 * OSPEEDR only changes the output driver's slew rate, not the timing of the code,
 * so the frequency stays the same and only the rise / fall times change.
 * Runs on the 16 MHz HSI reset clock: each half is 1 µs of delay plus loop overhead.
 */

#include "stm32f4xx.h"
#include "dbg.h"
#include "dwt.h"

#ifndef SPEED
#define SPEED GPIO_SPEED_VERY_HIGH
#endif

int main(void)
{
    dbg_init(SPEED);
    dwt_init();

    for (;;) {
        DBG0_HI();
        delay_cycles(16);
        DBG0_LO();
        delay_cycles(16);
    }
}
