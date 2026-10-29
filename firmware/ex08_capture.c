/* Exercise 8: hardware frequency and duty-cycle meter (TIM2 PWM input mode).
 *
 * PA0 = TIM2_CH1 (AF1). Both capture channels look at the same input TI1:
 *   IC1 captures on the rising edge  -> CCR1 = period
 *   IC2 captures on the falling edge -> CCR2 = high time
 * and the slave controller resets the counter on every rising edge (TI1FP1).
 * TIM2 runs at 90 MHz (APB1 45 MHz x 2), so one count is 11.1 ns.
 * Generator: square, 0-3.3 V -> 1 kΩ -> PA0.
 * RM0090 section 18.3.6 (PWM input mode).
 */

#include <stdio.h>
#include "stm32f4xx.h"
#include "clock.h"
#include "dwt.h"
#include "gpio.h"
#include "uart3.h"

int main(void)
{
    clock_init_180mhz();
    dwt_init();
    uart3_init(115200);

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;
    (void)RCC->APB1ENR;
    gpio_af(GPIOA, 0, 1);   /* PA0 = TIM2_CH1 */

    TIM2->PSC = 0;
    TIM2->ARR = 0xFFFFFFFF;                     /* TIM2 is 32-bit: 47 s before it wraps */
    TIM2->CCMR1 = (1UL << TIM_CCMR1_CC1S_Pos)   /* CC1S = 01: IC1 mapped on TI1 */
                | (2UL << TIM_CCMR1_CC2S_Pos);  /* CC2S = 10: IC2 mapped on TI1 too */
    TIM2->CCER  = TIM_CCER_CC2P                 /* IC2 on the falling edge (IC1 stays rising) */
                | TIM_CCER_CC1E | TIM_CCER_CC2E;
    TIM2->SMCR  = (5UL << TIM_SMCR_TS_Pos)      /* TS = 101: trigger is TI1FP1 */
                | (4UL << TIM_SMCR_SMS_Pos);    /* SMS = 100: reset mode */
    TIM2->CR1   = TIM_CR1_CEN;

    /* Timer clock: APB1 prescaler is not 1, so the timers get twice the APB1 clock. */
    uint32_t f_tim = 2 * clock_apb1_hz();

    for (;;) {
        delay_cycles(SystemCoreClock / 5);   /* print 5 times a second */

        if (!(TIM2->SR & TIM_SR_CC1IF)) {
            printf("no signal on PA0\r\n");
            continue;
        }
        /* Reading CCR1 clears CC1IF. CCR1 and CCR2 may come from neighbouring periods,
         * which only matters if the signal is changing. The +1 is the counter's reset
         * convention: try it with and without at high frequencies. */
        uint32_t period = TIM2->CCR1 + 1;
        uint32_t high   = TIM2->CCR2 + 1;
        TIM2->SR = ~(TIM_SR_CC1OF | TIM_SR_CC2OF);   /* clear the overcapture flags */

        printf("f=%.3f Hz duty=%.2f%%  (period %lu, high %lu counts)\r\n",
               (double)f_tim / period, 100.0 * high / period, period, high);
    }
}
