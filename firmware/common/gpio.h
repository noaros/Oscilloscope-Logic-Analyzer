/* Small helpers for the per-pin GPIO fields. RM0090 section 8.4.
 * Each field is a read-modify-write of one register, so calling these from
 * main and an interrupt handler on the same port at the same time is unsafe. */
#ifndef GPIO_H
#define GPIO_H

#include "stm32f4xx.h"

/* MODER: 2 bits per pin */
enum { GPIO_MODE_IN = 0, GPIO_MODE_OUT = 1, GPIO_MODE_AF = 2, GPIO_MODE_ANALOG = 3 };
/* OSPEEDR: 2 bits per pin */
enum { GPIO_SPEED_LOW = 0, GPIO_SPEED_MEDIUM = 1, GPIO_SPEED_HIGH = 2, GPIO_SPEED_VERY_HIGH = 3 };
/* PUPDR: 2 bits per pin */
enum { GPIO_PULL_NONE = 0, GPIO_PULL_UP = 1, GPIO_PULL_DOWN = 2 };

static inline void gpio_mode(GPIO_TypeDef *port, unsigned pin, uint32_t mode)
{
    port->MODER = (port->MODER & ~(3UL << (2 * pin))) | (mode << (2 * pin));
}

static inline void gpio_speed(GPIO_TypeDef *port, unsigned pin, uint32_t speed)
{
    port->OSPEEDR = (port->OSPEEDR & ~(3UL << (2 * pin))) | (speed << (2 * pin));
}

static inline void gpio_pull(GPIO_TypeDef *port, unsigned pin, uint32_t pull)
{
    port->PUPDR = (port->PUPDR & ~(3UL << (2 * pin))) | (pull << (2 * pin));
}

/* Alternate function number (0-15), from the "alternate function mapping"
 * table in the datasheet (DS9405).
 * AFR[0] = AFRL for pins 0-7, AFR[1] = AFRH for pins 8-15, 4 bits per pin. */
static inline void gpio_af(GPIO_TypeDef *port, unsigned pin, uint32_t af)
{
    unsigned shift = 4 * (pin % 8);
    port->AFR[pin / 8] = (port->AFR[pin / 8] & ~(0xFUL << shift)) | (af << shift);
    gpio_mode(port, pin, GPIO_MODE_AF);
}

#endif
