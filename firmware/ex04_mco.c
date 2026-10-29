/* Exercise 4: look at the clock tree with MCO.
 *
 *   MCO2 (PC9) = SYSCLK / 5   -> 36 MHz after clock_init_180mhz()
 *   MCO1 (PA8) = HSE          -> 8 MHz from the ST-LINK
 *              = HSI          -> 16 MHz internal RC, with  EXTRA_CFLAGS=-DMCO1_HSI
 *
 * This is also where you check clock_init_180mhz() in common/clock.c:
 * if MCO2 reads 36 MHz, the PLL, over-drive and flash settings all worked.
 * RM0090 section 6.2.10 (clock-out capability) and 6.3.3 (RCC_CFGR).
 */

#include <stdio.h>
#include "stm32f4xx.h"
#include "clock.h"
#include "gpio.h"
#include "uart3.h"

int main(void)
{
    /* Choose the MCO sources and prescalers first. RM0090 recommends setting
     * them right after reset, before the PLL runs, because changing them later
     * can glitch the output. */
    uint32_t mco1 = 0;                          /* MCO1 = 00: HSI */
#ifndef MCO1_HSI
    mco1 = RCC_CFGR_MCO1_1;                     /* MCO1 = 10: HSE */
#endif
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCO1 | RCC_CFGR_MCO1PRE | RCC_CFGR_MCO2 | RCC_CFGR_MCO2PRE))
              | mco1                            /* MCO1PRE = 0xx: no division */
              | (0UL << RCC_CFGR_MCO2_Pos)      /* MCO2 = 00: SYSCLK */
              | (7UL << RCC_CFGR_MCO2PRE_Pos);  /* MCO2PRE = 111: divide by 5 */

    clock_init_180mhz();

    /* PA8 and PC9 in alternate function 0 (MCO), very high speed for the 36 MHz edge. */
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_GPIOCEN;
    (void)RCC->AHB1ENR;
    gpio_speed(GPIOA, 8, GPIO_SPEED_VERY_HIGH);
    gpio_speed(GPIOC, 9, GPIO_SPEED_VERY_HIGH);
    gpio_af(GPIOA, 8, 0);
    gpio_af(GPIOC, 9, 0);

    uart3_init(115200);
    printf("SYSCLK %lu Hz, APB1 %lu Hz, APB2 %lu Hz\r\n",
           SystemCoreClock, clock_apb1_hz(), clock_apb2_hz());
    printf("MCO1 (PA8) = %s, MCO2 (PC9) = SYSCLK/5 = %lu Hz\r\n",
#ifdef MCO1_HSI
           "HSI 16 MHz",
#else
           "HSE 8 MHz",
#endif
           SystemCoreClock / 5);

    for (;;) {}
}
