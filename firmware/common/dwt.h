/* DWT cycle counter: counts CPU clock cycles (5.56 ns each at 180 MHz,
 * 62.5 ns at the 16 MHz HSI reset clock). ARMv7-M ARM section C1.8. */
#ifndef DWT_H
#define DWT_H

#include "stm32f4xx.h"

static inline void dwt_init(void)
{
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CYCCNT = 0;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}

/* Busy-wait for at least n cycles. The unsigned subtraction handles the
 * counter wrapping around every 2^32 cycles (about 24 s at 180 MHz). */
static inline void delay_cycles(uint32_t n)
{
    uint32_t t0 = DWT->CYCCNT;
    while ((DWT->CYCCNT - t0) < n) {}
}

#endif
