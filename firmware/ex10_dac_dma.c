/* Exercise 10: DAC waveform generator.
 *
 * TIM6 overflows at 64 x SINE_HZ and its TRGO triggers DAC channel 1 (PA4).
 * Each trigger makes the DAC request the next sample, and DMA1 Stream 5 Channel 7
 * copies it from a 64-point sine table in circular mode. Once started, the CPU does nothing.
 *   Frequency:  make flash EX=10 EXTRA_CFLAGS=-DSINE_HZ=2000     (default 1000)
 * RM0090 section 14.3 (DAC triggers and DMA) and 10.3 (DMA streams, and the DMA1 request mapping table).
 */

#include <math.h>
#include <stdio.h>
#include "stm32f4xx.h"
#include "clock.h"
#include "gpio.h"
#include "uart3.h"

#ifndef SINE_HZ
#define SINE_HZ 1000
#endif

#define N_POINTS 64
#define PI_F     3.14159265f   /* M_PI is not in strict C11 */

/* 32-bit entries so the DMA can do word transfers to the 32-bit DHR12R1 register.
 * The array is in SRAM (.bss): the DMA can't read from CCM. */
static uint32_t lut[N_POINTS];

int main(void)
{
    clock_init_180mhz();
    uart3_init(115200);

    for (int i = 0; i < N_POINTS; i++) {
        lut[i] = (uint32_t)lrintf(2047.5f + 2047.5f * sinf(2.0f * PI_F * i / N_POINTS));
    }

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN | RCC_AHB1ENR_DMA1EN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM6EN | RCC_APB1ENR_DACEN;
    (void)RCC->APB1ENR;
    gpio_mode(GPIOA, 4, GPIO_MODE_ANALOG);   /* DAC_OUT1 */

    /* DMA1 Stream 5, channel 7 = DAC1 request. Memory -> peripheral, circular. */
    DMA1_Stream5->CR = 0;
    while (DMA1_Stream5->CR & DMA_SxCR_EN) {}    /* a stream can only be set up while disabled */
    DMA1_Stream5->PAR  = (uint32_t)&DAC1->DHR12R1;
    DMA1_Stream5->M0AR = (uint32_t)lut;
    DMA1_Stream5->NDTR = N_POINTS;
    DMA1_Stream5->CR = (7UL << DMA_SxCR_CHSEL_Pos)
                     | DMA_SxCR_MSIZE_1 | DMA_SxCR_PSIZE_1   /* 10 = word, both sides */
                     | DMA_SxCR_MINC                         /* step through the table */
                     | DMA_SxCR_CIRC                         /* and start again at the end */
                     | DMA_SxCR_DIR_0                        /* 01 = memory to peripheral */
                     | DMA_SxCR_EN;

    /* DAC channel 1: trigger TSEL1 = 000 (TIM6 TRGO), DMA request on, enabled. */
    DAC1->CR = DAC_CR_TEN1 | DAC_CR_DMAEN1 | DAC_CR_EN1;

    /* TIM6 at 90 MHz. 90 MHz / (64 x 1 kHz) = 1406.25 isn't a whole number, so the
     * frequency is rounded: the printout shows what you actually get. */
    uint32_t f_tim = 2 * clock_apb1_hz();
    uint32_t rate  = N_POINTS * SINE_HZ;
    TIM6->PSC = 0;
    TIM6->ARR = (f_tim + rate / 2) / rate - 1;
    TIM6->CR2 = 2UL << TIM_CR2_MMS_Pos;          /* MMS = 010: TRGO on update */
    TIM6->CR1 = TIM_CR1_CEN;

    printf("Sine on PA4: %.3f Hz (TIM6 ARR = %lu)\r\n",
           (double)f_tim / (TIM6->ARR + 1) / N_POINTS, TIM6->ARR);

    for (;;) {
        __WFI();
    }
}
