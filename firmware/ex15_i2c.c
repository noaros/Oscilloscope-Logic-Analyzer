/* Exercise 15: I²C ACK/NACK and pull-up resistors.
 *
 * I2C1 master, SCL = PB8 (D15), SDA = PB9 (D14), AF4, open-drain.
 * Scans addresses 0x08-0x77 once a second and prints the ones that ACK.
 * DBG0 (PG2) is high during each scan, as an LA trigger.
 *   400 kHz:                make flash EX=15 EXTRA_CFLAGS=-DI2C_FAST
 *   No internal pull-ups:   make flash EX=15 EXTRA_CFLAGS=-DINTERNAL_PULLUPS=0   (use external 4.7 kΩ)
 *   Read a sensor register: make flash EX=15 EXTRA_CFLAGS="-DSENSOR_ADDR=0x76 -DSENSOR_REG=0xD0"
 *     (write the register number, repeated START, read one byte)
 * RM0090 section 27 (I2C). The F4 I2C peripheral is driven by its event flags
 * SB -> ADDR -> TXE/BTF -> RXNE, each with its own clearing rule.
 */

#include <stdio.h>
#include "stm32f4xx.h"
#include "clock.h"
#include "dbg.h"
#include "dwt.h"
#include "uart3.h"

#ifndef INTERNAL_PULLUPS
#define INTERNAL_PULLUPS 1
#endif

enum { I2C_OK, I2C_NACK, I2C_TIMEOUT };

/* Wait up to 1 ms for a flag in SR1. A missing pull-up or a stuck bus would
 * otherwise hang the program here. */
static int wait_sr1(uint32_t flag)
{
    uint32_t t0 = DWT->CYCCNT;
    while (!(I2C1->SR1 & flag)) {
        if (DWT->CYCCNT - t0 > SystemCoreClock / 1000) {
            return I2C_TIMEOUT;
        }
    }
    return I2C_OK;
}

static void i2c1_init(void)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    (void)RCC->APB1ENR;

    GPIOB->OTYPER |= GPIO_OTYPER_OT8 | GPIO_OTYPER_OT9;   /* open-drain: I²C only pulls low */
    gpio_pull(GPIOB, 8, INTERNAL_PULLUPS ? GPIO_PULL_UP : GPIO_PULL_NONE);
    gpio_pull(GPIOB, 9, INTERNAL_PULLUPS ? GPIO_PULL_UP : GPIO_PULL_NONE);
    gpio_speed(GPIOB, 8, GPIO_SPEED_MEDIUM);
    gpio_speed(GPIOB, 9, GPIO_SPEED_MEDIUM);
    gpio_af(GPIOB, 8, 4);   /* I2C1_SCL */
    gpio_af(GPIOB, 9, 4);   /* I2C1_SDA */

    I2C1->CR1 = I2C_CR1_SWRST;   /* start from a clean state */
    I2C1->CR1 = 0;

    /* The timing registers are in units of the APB1 clock (45 MHz). */
    uint32_t pclk = clock_apb1_hz();
    uint32_t mhz  = pclk / 1000000;
    I2C1->CR2 = mhz;                                  /* FREQ = 45 */
#ifdef I2C_FAST
    /* Fast mode, DUTY = 0: SCL low = 2 x CCR, high = CCR -> CCR = pclk / (3 x 400 kHz), rounded up. */
    I2C1->CCR   = I2C_CCR_FS | ((pclk + 3 * 400000 - 1) / (3 * 400000));
    I2C1->TRISE = mhz * 300 / 1000 + 1;               /* max rise time 300 ns */
#else
    /* Standard mode: SCL low = high = CCR -> CCR = pclk / (2 x 100 kHz) = 225. */
    I2C1->CCR   = pclk / (2 * 100000);
    I2C1->TRISE = mhz + 1;                            /* max rise time 1000 ns */
#endif
    I2C1->CR1 = I2C_CR1_PE;
}

static void i2c1_stop(void)
{
    I2C1->CR1 |= I2C_CR1_STOP;
    uint32_t t0 = DWT->CYCCNT;
    while ((I2C1->CR1 & I2C_CR1_STOP) && DWT->CYCCNT - t0 < SystemCoreClock / 1000) {}
}

/* START (or repeated START), then send the address byte. Returns I2C_OK when ADDR
 * is set, which the caller must clear by reading SR1 then SR2. */
static int i2c1_start(uint8_t addr_byte)
{
    I2C1->CR1 |= I2C_CR1_START;
    if (wait_sr1(I2C_SR1_SB) != I2C_OK) {
        return I2C_TIMEOUT;
    }
    I2C1->DR = addr_byte;   /* reading SR1 (above) then writing DR clears SB */

    uint32_t t0 = DWT->CYCCNT;
    for (;;) {
        uint32_t sr1 = I2C1->SR1;
        if (sr1 & I2C_SR1_ADDR) {
            return I2C_OK;
        }
        if (sr1 & I2C_SR1_AF) {             /* acknowledge failure: nobody answered */
            I2C1->SR1 = ~I2C_SR1_AF;        /* rc_w0: write 0 to clear */
            return I2C_NACK;
        }
        if (DWT->CYCCNT - t0 > SystemCoreClock / 1000) {
            return I2C_TIMEOUT;
        }
    }
}

static void clear_addr(void)
{
    (void)I2C1->SR1;
    (void)I2C1->SR2;
}

#ifdef SENSOR_ADDR
/* Write the register number, repeated START, read one byte, STOP. */
static int i2c1_read_reg(uint8_t addr, uint8_t reg, uint8_t *val)
{
    int r = i2c1_start((uint8_t)(addr << 1));
    if (r != I2C_OK) {
        i2c1_stop();
        return r;
    }
    clear_addr();
    I2C1->DR = reg;
    if (wait_sr1(I2C_SR1_BTF) != I2C_OK) {   /* byte fully sent and ACKed */
        i2c1_stop();
        return I2C_TIMEOUT;
    }

    r = i2c1_start((uint8_t)(addr << 1) | 1);   /* repeated START, address + R */
    if (r != I2C_OK) {
        i2c1_stop();
        return r;
    }
    /* Single-byte read: NACK and STOP must be set up around clearing ADDR,
     * so the peripheral NACKs the only byte and then stops. */
    I2C1->CR1 &= ~I2C_CR1_ACK;
    clear_addr();
    I2C1->CR1 |= I2C_CR1_STOP;
    if (wait_sr1(I2C_SR1_RXNE) != I2C_OK) {
        return I2C_TIMEOUT;
    }
    *val = (uint8_t)I2C1->DR;
    return I2C_OK;
}
#else
static int i2c1_probe(uint8_t addr)
{
    int r = i2c1_start((uint8_t)(addr << 1));   /* address + W */
    if (r == I2C_OK) {
        clear_addr();
    }
    i2c1_stop();
    return r;
}
#endif

int main(void)
{
    clock_init_180mhz();
    dbg_init(GPIO_SPEED_VERY_HIGH);
    dwt_init();
    uart3_init(115200);
    i2c1_init();

    printf("I2C1 at %s, internal pull-ups %s\r\n",
#ifdef I2C_FAST
           "400 kHz",
#else
           "100 kHz",
#endif
           INTERNAL_PULLUPS ? "on" : "off");

    for (;;) {
        if (I2C1->SR2 & I2C_SR2_BUSY) {
            printf("Bus busy: SCL or SDA is held low (missing pull-ups?). Resetting I2C1.\r\n");
            i2c1_init();
        } else {
#ifdef SENSOR_ADDR
            uint8_t val;
            int r = i2c1_read_reg(SENSOR_ADDR, SENSOR_REG, &val);
            if (r == I2C_OK) {
                printf("0x%02X reg 0x%02X = 0x%02X\r\n", SENSOR_ADDR, SENSOR_REG, val);
            } else {
                printf("0x%02X reg 0x%02X: %s\r\n", SENSOR_ADDR, SENSOR_REG,
                       r == I2C_NACK ? "NACK" : "timeout");
            }
#else
            int found = 0, timeouts = 0;
            DBG0_HI();
            printf("Scan:");
            for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
                int r = i2c1_probe(addr);
                if (r == I2C_OK) {
                    printf(" 0x%02X", addr);
                    found++;
                } else if (r == I2C_TIMEOUT) {
                    timeouts++;
                }
            }
            DBG0_LO();
            printf("  -> %d found%s\r\n", found, timeouts ? ", with timeouts" : "");
#endif
        }
        delay_cycles(SystemCoreClock);   /* 1 s */
    }
}
