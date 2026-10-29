#ifndef UART3_H
#define UART3_H

#include <stdint.h>

/* USART3 on PD8 (TX) / PD9 (RX): the ST-LINK virtual COM port.
 * After this, printf() goes to the serial terminal on the PC.
 * Call it again after changing the clock, because the baud-rate divider
 * depends on the APB1 clock. */
void uart3_init(uint32_t baud);

#endif
