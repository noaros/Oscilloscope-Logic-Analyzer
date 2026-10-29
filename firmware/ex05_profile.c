/* Exercise 5: profiling code with debug pins.
 *
 * DBG0 (PG2) is high while 100 square roots run; DBG1 (PG3) is high during the printf.
 * The same region is also timed with the DWT cycle counter and printed, so you can
 * check the LA's pulse width against cycles x 5.56 ns.
 *   STEP 1: sqrtf (single precision, done by the FPU)      make flash EX=05
 *   STEP 2: sqrt  (double precision, done in software)     make flash EX=05 EXTRA_CFLAGS=-DSTEP=2
 *   Optimisation:                                          make flash EX=05 OPT=-O0
 */

#include <math.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "clock.h"
#include "dbg.h"
#include "dwt.h"
#include "uart3.h"

#ifndef STEP
#define STEP 1
#endif

int main(void)
{
    clock_init_180mhz();
    dbg_init(GPIO_SPEED_VERY_HIGH);
    dwt_init();
    uart3_init(115200);

#if STEP == 1
    volatile float x = 2.0f, y;
#elif STEP == 2
    volatile double x = 2.0, y;
#else
#error "STEP must be 1 or 2"
#endif

    for (;;) {
        uint32_t t0 = DWT->CYCCNT;
        DBG0_HI();
        for (int i = 0; i < 100; i++) {
#if STEP == 1
            y = sqrtf(x + i);
#else
            y = sqrt(x + i);
#endif
        }
        DBG0_LO();
        uint32_t cycles = DWT->CYCCNT - t0;
        (void)y;

        DBG1_HI();
        printf("%lu cycles = %.2f us\r\n", cycles, cycles * 1e6f / SystemCoreClock);
        DBG1_LO();

        delay_cycles(SystemCoreClock / 10);   /* 100 ms */
    }
}
