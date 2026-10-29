/* Exercise 9: ADC -> DAC passthrough and aliasing you can see.
 *
 * TIM3 overflows at 10 kHz and its TRGO starts an ADC1 conversion of IN3 (PA3).
 * The end-of-conversion interrupt copies the sample to DAC channel 1 (PA4),
 * with DBG0 (PG2) high while it runs.
 *   Sampling time:  make flash EX=09 EXTRA_CFLAGS=-DSMP=0     (SMP3 code 0-7, default 3 = 56 cycles)
 * Generator: sine, 0.3-3.0 V -> 1 kΩ -> PA3.
 * RM0090 section 13 (ADC) and 14 (DAC).
 */

#include "stm32f4xx.h"
#include "clock.h"
#include "dbg.h"

#ifndef SMP
#define SMP 3    /* 0:3, 1:15, 2:28, 3:56, 4:84, 5:112, 6:144, 7:480 ADC clock cycles */
#endif

#define SAMPLE_RATE_HZ 10000

void ADC_IRQHandler(void)
{
    if (ADC1->SR & ADC_SR_EOC) {
        DBG0_HI();
        DAC1->DHR12R1 = ADC1->DR;   /* reading DR clears EOC */
        DBG0_LO();
    }
}

int main(void)
{
    clock_init_180mhz();
    dbg_init(GPIO_SPEED_VERY_HIGH);

    RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
    RCC->APB1ENR |= RCC_APB1ENR_TIM3EN | RCC_APB1ENR_DACEN;
    RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;
    (void)RCC->APB2ENR;
    gpio_mode(GPIOA, 3, GPIO_MODE_ANALOG);   /* ADC1_IN3 */
    gpio_mode(GPIOA, 4, GPIO_MODE_ANALOG);   /* DAC_OUT1: analog mode avoids a parasitic load */

    /* DAC channel 1, output buffer on (BOFF1 = 0), no trigger: a write to DHR12R1
     * reaches the output one APB1 cycle later. */
    DAC1->CR = DAC_CR_EN1;

    /* ADC clock = APB2 90 MHz / 4 = 22.5 MHz (max 36 MHz). */
    ADC->CCR = (ADC->CCR & ~ADC_CCR_ADCPRE) | ADC_CCR_ADCPRE_0;
    ADC1->SMPR2 = (uint32_t)SMP << ADC_SMPR2_SMP3_Pos;
    ADC1->SQR1  = 0;                              /* L = 0: one conversion in the sequence */
    ADC1->SQR3  = 3UL << ADC_SQR3_SQ1_Pos;        /* first (only) conversion: channel 3 */
    ADC1->CR1   = ADC_CR1_EOCIE;
    ADC1->CR2   = ADC_CR2_EXTEN_0                 /* EXTEN = 01: trigger on the rising edge */
                | (8UL << ADC_CR2_EXTSEL_Pos)     /* EXTSEL = 1000: TIM3 TRGO */
                | ADC_CR2_ADON;
    NVIC_EnableIRQ(ADC_IRQn);

    /* TIM3 at 90 MHz: an update every 9000 counts = 10 kHz, sent out as TRGO. */
    TIM3->PSC = 0;
    TIM3->ARR = 2 * clock_apb1_hz() / SAMPLE_RATE_HZ - 1;
    TIM3->CR2 = 2UL << TIM_CR2_MMS_Pos;           /* MMS = 010: TRGO on update */
    TIM3->CR1 = TIM_CR1_CEN;

    for (;;) {
        __WFI();
    }
}
