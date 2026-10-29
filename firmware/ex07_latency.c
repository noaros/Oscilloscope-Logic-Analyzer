/* Exercise 7: interrupt latency and jitter.
 *
 * A rising edge on PG0 raises EXTI line 0; the handler pulses DBG0 (PG2).
 * Measure input edge -> DBG0 pulse on the LA.
 *   STEP 1: the main loop spins doing nothing                 make flash EX=07
 *   STEP 2: the main loop sleeps in __WFI() between interrupts
 *           (waking up adds a little latency)                 make flash EX=07 EXTRA_CFLAGS=-DSTEP=2
 *   STEP 3: the main loop disables interrupts for 10 µs at a time,
 *           so the worst-case latency grows                   make flash EX=07 EXTRA_CFLAGS=-DSTEP=3
 * Generator: square, 1 kHz, 0-3.3 V -> 1 kΩ -> PG0.
 * RM0090 section 12.2 (EXTI) and 9.2.3 (SYSCFG_EXTICR1).
 */

#include "stm32f4xx.h"
#include "clock.h"
#include "dbg.h"
#include "dwt.h"

#ifndef STEP
#define STEP 1
#endif

void EXTI0_IRQHandler(void)
{
    EXTI->PR = EXTI_PR_PR0;   /* clear the pending bit by writing 1, first, so it can't fire twice */
    DBG0_HI();
    DBG0_LO();
}

int main(void)
{
    clock_init_180mhz();
    dbg_init(GPIO_SPEED_VERY_HIGH);
    dwt_init();

    gpio_pull(GPIOG, 0, GPIO_PULL_NONE);
    gpio_mode(GPIOG, 0, GPIO_MODE_IN);

    /* Route port G to EXTI line 0: EXTICR1 bits 3:0 = 0110 (PG). SYSCFG needs its clock for this. */
    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
    SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~SYSCFG_EXTICR1_EXTI0) | SYSCFG_EXTICR1_EXTI0_PG;

    EXTI->RTSR |= EXTI_RTSR_TR0;   /* rising edge */
    EXTI->FTSR &= ~EXTI_FTSR_TR0;
    EXTI->PR    = EXTI_PR_PR0;     /* drop anything pending from before */
    EXTI->IMR  |= EXTI_IMR_MR0;    /* unmask the line */
    NVIC_EnableIRQ(EXTI0_IRQn);

    for (;;) {
#if STEP == 1
        /* spin */
#elif STEP == 2
        __WFI();
#elif STEP == 3
        __disable_irq();
        delay_cycles(1800);          /* 10 µs at 180 MHz with interrupts off */
        __enable_irq();
#else
#error "STEP must be 1, 2 or 3"
#endif
    }
}
