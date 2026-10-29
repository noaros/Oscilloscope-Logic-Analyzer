/* Exercise 11: complementary PWM with dead time.
 *
 * TIM1 CH1 (PE9) and CH1N (PE8), both AF1: 100 kHz, 30% duty.
 * TIM1 runs at 180 MHz (APB2 90 MHz x 2), so ARR = 1799 and CCR1 = 540.
 *   Dead time:  make flash EX=11 EXTRA_CFLAGS=-DDTG=18     (default 0; 18 -> 100 ns, 90 -> 500 ns)
 * For DTG values below 128 each step is one timer clock, 5.56 ns.
 * RM0090 section 17.3.11 (complementary outputs and dead-time) and 17.4.18 (TIMx_BDTR).
 */

#include "stm32f4xx.h"
#include "clock.h"
#include "gpio.h"

#ifndef DTG
#define DTG 0
#endif

#if DTG < 0 || DTG > 127
#error "DTG must be 0-127 here (larger values use a coarser encoding)"
#endif

int main(void)
{
    clock_init_180mhz();

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    (void)RCC->APB2ENR;
    gpio_speed(GPIOE, 8, GPIO_SPEED_VERY_HIGH);
    gpio_speed(GPIOE, 9, GPIO_SPEED_VERY_HIGH);
    gpio_af(GPIOE, 8, 1);   /* TIM1_CH1N */
    gpio_af(GPIOE, 9, 1);   /* TIM1_CH1 */

    TIM1->PSC   = 0;
    TIM1->ARR   = 1799;                               /* 180 MHz / 1800 = 100 kHz */
    TIM1->CCR1  = 540;                                /* 540 / 1800 = 30% */
    TIM1->CCMR1 = (6UL << TIM_CCMR1_OC1M_Pos)         /* OC1M = 110: PWM mode 1 */
                | TIM_CCMR1_OC1PE;                    /* CCR1 preload */
    TIM1->CCER  = TIM_CCER_CC1E | TIM_CCER_CC1NE;     /* both outputs, active high */
    TIM1->BDTR  = ((uint32_t)DTG << TIM_BDTR_DTG_Pos)
                | TIM_BDTR_MOE;                       /* main output enable: advanced timers need it */
    TIM1->EGR   = TIM_EGR_UG;                         /* load the preloaded registers */
    TIM1->CR1   = TIM_CR1_ARPE | TIM_CR1_CEN;

    for (;;) {
        __WFI();
    }
}
