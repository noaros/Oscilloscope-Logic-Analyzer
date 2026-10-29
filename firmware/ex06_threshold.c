/* Exercise 6: measure the STM32's input thresholds and hysteresis.
 *
 * PG0 is an input (no pull). The loop copies what the chip reads on PG0 to DBG0 (PG2),
 * so the scope can show the input voltage at the moment the chip changes its mind.
 * Generator: triangle, 100 Hz, 0-3.3 V -> 1 kΩ -> PG0.
 */

#include "stm32f4xx.h"
#include "clock.h"
#include "dbg.h"

int main(void)
{
    clock_init_180mhz();
    dbg_init(GPIO_SPEED_VERY_HIGH);

    /* GPIOG's clock is already on from dbg_init(). */
    gpio_pull(GPIOG, 0, GPIO_PULL_NONE);
    gpio_mode(GPIOG, 0, GPIO_MODE_IN);

    for (;;) {
        if (GPIOG->IDR & GPIO_IDR_ID0) {
            DBG0_HI();
        } else {
            DBG0_LO();
        }
    }
}
