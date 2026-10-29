/* Exercise 12: glitch hunting with triggers.
 *
 * DBG0 (PG2): a stream of 10 µs pulses with a 10 µs gap, but every GLITCH_EVERY-th
 * pulse is only GLITCH_CYCLES long. DBG1 (PG3) pulses at the start of each group,
 * so you can count from the marker to the glitch.
 *   make flash EX=12 EXTRA_CFLAGS="-DGLITCH_EVERY=100000 -DGLITCH_CYCLES=18"
 * Defaults: every 1000th pulse, 180 cycles = 1 µs. 18 cycles = 100 ns.
 */

#include "stm32f4xx.h"
#include "clock.h"
#include "dbg.h"
#include "dwt.h"

#ifndef GLITCH_EVERY
#define GLITCH_EVERY 1000
#endif
#ifndef GLITCH_CYCLES
#define GLITCH_CYCLES 180
#endif

int main(void)
{
    clock_init_180mhz();
    dbg_init(GPIO_SPEED_VERY_HIGH);
    dwt_init();

    for (uint32_t n = 0;; n++) {
        if (n % GLITCH_EVERY == 0) {
            DBG1_HI();
            DBG1_LO();
        }
        DBG0_HI();
        delay_cycles((n % GLITCH_EVERY == GLITCH_EVERY - 1) ? GLITCH_CYCLES : 1800);
        DBG0_LO();
        delay_cycles(1800);
    }
}
