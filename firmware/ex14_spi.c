/* Exercise 14: SPI clock modes and loopback.
 *
 * SPI4 master, 8-bit, MSB first. SCK = PE2, MISO = PE5, MOSI = PE6 (AF5),
 * NSS = PE4 driven as a plain GPIO. Jumper MOSI to MISO for the loopback.
 * Sends 0xA5 0x3C 0x00 0xFF every 100 µs and counts frames where MISO didn't match.
 *   Mode:      make flash EX=14 EXTRA_CFLAGS=-DSPI_MODE=1    (0-3: CPOL = bit 1, CPHA = bit 0)
 *   Prescaler: make flash EX=14 EXTRA_CFLAGS=-DBR=0          (SCK = 90 MHz / 2^(BR+1); default 5 = 1.41 MHz, 0 = 45 MHz)
 * RM0090 section 28 (SPI).
 */

#include <stdio.h>
#include "stm32f4xx.h"
#include "clock.h"
#include "dwt.h"
#include "gpio.h"
#include "uart3.h"

#ifndef SPI_MODE
#define SPI_MODE 0
#endif
#ifndef BR
#define BR 5
#endif

#define NSS_HI()  (GPIOE->BSRR = GPIO_BSRR_BS4)
#define NSS_LO()  (GPIOE->BSRR = GPIO_BSRR_BR4)

static uint8_t spi4_transfer(uint8_t out)
{
    while (!(SPI4->SR & SPI_SR_TXE)) {}
    SPI4->DR = out;
    while (!(SPI4->SR & SPI_SR_RXNE)) {}
    return (uint8_t)SPI4->DR;
}

int main(void)
{
    clock_init_180mhz();
    dwt_init();
    uart3_init(115200);

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;
    RCC->APB2ENR |= RCC_APB2ENR_SPI4EN;
    (void)RCC->APB2ENR;
    NSS_HI();
    gpio_mode(GPIOE, 4, GPIO_MODE_OUT);
    for (unsigned pin = 2; pin <= 6; pin++) {
        if (pin == 3 || pin == 4) {
            continue;
        }
        gpio_speed(GPIOE, pin, GPIO_SPEED_VERY_HIGH);
        gpio_af(GPIOE, pin, 5);   /* PE2 SCK, PE5 MISO, PE6 MOSI */
    }

    /* Master with software slave management: SSM = 1 and SSI = 1 keep the internal
     * NSS high, otherwise the peripheral thinks another master took the bus (MODF). */
    SPI4->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI
              | ((uint32_t)BR << SPI_CR1_BR_Pos)
              | ((SPI_MODE & 2) ? SPI_CR1_CPOL : 0)
              | ((SPI_MODE & 1) ? SPI_CR1_CPHA : 0);
    SPI4->CR1 |= SPI_CR1_SPE;

    printf("SPI4 mode %d, SCK = %lu Hz\r\n", SPI_MODE, clock_apb2_hz() >> (BR + 1));

    static const uint8_t tx[] = { 0xA5, 0x3C, 0x00, 0xFF };
    uint32_t frames = 0, errors = 0;
    uint32_t last_print = DWT->CYCCNT;

    for (;;) {
        uint8_t rx[sizeof tx];
        NSS_LO();
        for (unsigned i = 0; i < sizeof tx; i++) {
            rx[i] = spi4_transfer(tx[i]);
        }
        while (SPI4->SR & SPI_SR_BSY) {}   /* last bit fully out before NSS goes high */
        NSS_HI();

        frames++;
        for (unsigned i = 0; i < sizeof tx; i++) {
            if (rx[i] != tx[i]) {
                errors++;
                break;
            }
        }

        if (DWT->CYCCNT - last_print > SystemCoreClock) {   /* once a second */
            printf("%lu frames, %lu with a mismatch (last: %02X %02X %02X %02X)\r\n",
                   frames, errors, rx[0], rx[1], rx[2], rx[3]);
            last_print = DWT->CYCCNT;
        }
        delay_cycles(SystemCoreClock / 10000);   /* 100 µs between frames */
    }
}
