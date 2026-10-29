#ifndef CLOCK_H
#define CLOCK_H

#include <stdint.h>

/* After reset the core runs from HSI at 16 MHz with all bus prescalers at /1.
 * clock_init_180mhz() switches to HSE (8 MHz from the ST-LINK) -> PLL -> 180 MHz:
 *   HCLK 180 MHz, APB1 45 MHz (timers 90 MHz), APB2 90 MHz (timers 180 MHz). */
void clock_init_180mhz(void);

/* Bus clocks decoded from RCC->CFGR and SystemCoreClock, so they are right
 * whether or not clock_init_180mhz() was called. */
uint32_t clock_apb1_hz(void);
uint32_t clock_apb2_hz(void);

#endif
