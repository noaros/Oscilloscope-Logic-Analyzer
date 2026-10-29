/* printf over the ST-LINK virtual COM port. RM0090 section 30 (USART). */

#include "uart3.h"
#include "clock.h"
#include "gpio.h"

void uart3_init(uint32_t baud)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;
    RCC->APB1ENR |= RCC_APB1ENR_USART3EN;
    (void)RCC->APB1ENR;

    gpio_af(GPIOD, 8, 7);   /* PD8 = USART3_TX, AF7 */
    gpio_af(GPIOD, 9, 7);   /* PD9 = USART3_RX, AF7 */

    /* With 16x oversampling, BRR holds USARTDIV x 16 = f_APB1 / baud,
     * as a 12-bit mantissa and 4-bit fraction (RM0090 section 30.6.3).
     * Adding baud / 2 rounds to the nearest value. */
    USART3->CR1 = 0;
    USART3->BRR = (clock_apb1_hz() + baud / 2) / baud;
    USART3->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;
}

/* newlib calls this for printf / puts. Blocking: waits for each byte. */
int _write(int fd, char *p, int len)
{
    (void)fd;
    for (int i = 0; i < len; i++) {
        while (!(USART3->SR & USART_SR_TXE)) {}
        USART3->DR = (uint8_t)p[i];
    }
    return len;
}
