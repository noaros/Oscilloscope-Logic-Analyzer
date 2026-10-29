/* Debug pins DBG0 = PG2, DBG1 = PG3: "logic-analyzer printf".
 * A BSRR write sets or clears a pin in one store, with no read-modify-write,
 * so it is safe from interrupts and takes a single bus cycle. RM0090 section 8.4.7. */
#ifndef DBG_H
#define DBG_H

#include "gpio.h"

#define DBG0_HI()  (GPIOG->BSRR = GPIO_BSRR_BS2)
#define DBG0_LO()  (GPIOG->BSRR = GPIO_BSRR_BR2)
#define DBG1_HI()  (GPIOG->BSRR = GPIO_BSRR_BS3)
#define DBG1_LO()  (GPIOG->BSRR = GPIO_BSRR_BR3)

/* Configure PG2/PG3 as push-pull outputs at the given speed (GPIO_SPEED_*). */
static inline void dbg_init(uint32_t speed)
{
    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOGEN;
    (void)RCC->AHB1ENR;   /* dummy read: give the clock time to reach the port (erratum in ES0206) */
    DBG0_LO();
    DBG1_LO();
    gpio_speed(GPIOG, 2, speed);
    gpio_speed(GPIOG, 3, speed);
    gpio_mode(GPIOG, 2, GPIO_MODE_OUT);
    gpio_mode(GPIOG, 3, GPIO_MODE_OUT);
}

#endif
