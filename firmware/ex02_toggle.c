/* Exercise 2: from Blinky to "how fast can a pin toggle?"
 *
 * Runs on the 16 MHz HSI reset clock: there is no clock setup yet (that's exercise 4).
 * Pick a step with EXTRA_CFLAGS, e.g.  make flash EX=02 EXTRA_CFLAGS=-DSTEP=1
 *   STEP 1 (A): blink LD1 (PB0) at 5 Hz from the SysTick interrupt
 *   STEP 2 (B): toggle DBG0 (PG2) as fast as possible with a read-modify-write of ODR
 *   STEP 3 (C): toggle DBG0 as fast as possible with BSRR writes
 */

#include "stm32f4xx.h"
#include "clock.h"
#include "dbg.h"

#ifndef STEP
#define STEP 3
#endif

#if STEP == 1
static volatile uint32_t ms_ticks;

/* SysTick fires every 1 ms; toggle LD1 every 100 ms -> 5 Hz. */
void SysTick_Handler(void)
{
    if (++ms_ticks % 100 == 0) {
        GPIOB->ODR ^= GPIO_ODR_OD0;
    }
}
#endif

int main(void)
{
#if STEP == 1
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    (void)RCC->AHB1ENR;
    gpio_mode(GPIOB, 0, GPIO_MODE_OUT);

    /* SysTick counts down from LOAD at the core clock and reloads: 16 MHz / 16000 = 1 kHz. */
    SysTick->LOAD = SystemCoreClock / 1000 - 1;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;
    for (;;) {
        __WFI();   /* sleep until the next interrupt */
    }

#elif STEP == 2
    dbg_init(GPIO_SPEED_VERY_HIGH);
    for (;;) {
        GPIOG->ODR ^= GPIO_ODR_OD2;   /* load, XOR, store: three instructions plus the branch */
    }

#elif STEP == 3
    dbg_init(GPIO_SPEED_VERY_HIGH);
    for (;;) {
        DBG0_HI();
        DBG0_LO();
    }

#else
#error "STEP must be 1, 2 or 3"
#endif
}
