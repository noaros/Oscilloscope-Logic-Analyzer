/* Exercise 13: UART decoding, baud-rate error and clock accuracy.
 *
 * USART6 TX on PG14 (D1), RX on PG9 (D0), both AF8, 115200 8N1.
 * Sends "Hello\r\n" every 100 ms.
 *   STEP 1: 180 MHz from the HSE (crystal-accurate)          make flash EX=13
 *   STEP 2: stay on the 16 MHz HSI and detune it with the
 *           HSITRIM bits (0-31, reset value 16)               make flash EX=13 EXTRA_CFLAGS="-DSTEP=2 -DHSI_TRIM=24"
 * In STEP 2 the printf on the ST-LINK port is detuned too, so it may turn to garbage as well.
 * RM0090 section 30 (USART) and 6.3.1 (RCC_CR, HSITRIM).
 */

#include <stdio.h>
#include "stm32f4xx.h"
#include "clock.h"
#include "dwt.h"
#include "gpio.h"
#include "uart3.h"

#ifndef STEP
#define STEP 1
#endif
#ifndef HSI_TRIM
#define HSI_TRIM 16
#endif

#define BAUD 115200

static void usart6_init(uint32_t baud)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN;
    RCC->APB2ENR |= RCC_APB2ENR_USART6EN;
    (void)RCC->APB2ENR;
    gpio_af(GPIOG, 14, 8);   /* USART6_TX */
    gpio_af(GPIOG, 9, 8);    /* USART6_RX */

    /* USART6 is on APB2 (90 MHz at 180 MHz, 16 MHz on HSI). BRR = f_APB2 / baud, rounded. */
    USART6->CR1 = 0;
    USART6->BRR = (clock_apb2_hz() + baud / 2) / baud;
    USART6->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

static void usart6_puts(const char *s)
{
    while (*s) {
        while (!(USART6->SR & USART_SR_TXE)) {}
        USART6->DR = (uint8_t)*s++;
    }
}

int main(void)
{
#if STEP == 1
    clock_init_180mhz();
#elif STEP == 2
    RCC->CR = (RCC->CR & ~RCC_CR_HSITRIM) | ((uint32_t)HSI_TRIM << RCC_CR_HSITRIM_Pos);
#else
#error "STEP must be 1 or 2"
#endif
    dwt_init();
    uart3_init(115200);
    usart6_init(BAUD);

    printf("USART6 BRR = %lu (mantissa %lu, fraction %lu/16), HSITRIM = %lu\r\n",
           USART6->BRR, USART6->BRR >> 4, USART6->BRR & 0xF,
           (RCC->CR & RCC_CR_HSITRIM) >> RCC_CR_HSITRIM_Pos);

    for (;;) {
        usart6_puts("Hello\r\n");
        delay_cycles(SystemCoreClock / 10);   /* 100 ms */
    }
}
