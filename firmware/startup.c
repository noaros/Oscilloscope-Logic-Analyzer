/* Vector table and reset handler for the STM32F429ZI.
 * RM0090 "Interrupts and events" chapter (vector table) and PM0214 (exception model). */

#include <stdint.h>
#include "stm32f4xx.h"

extern uint32_t _estack, _sidata, _sdata, _edata, _sbss, _ebss;
extern void __libc_init_array(void);
extern int main(void);

void Reset_Handler(void);

/* Any interrupt the firmware doesn't handle ends up here. In the debugger,
 * read SCB->ICSR (VECTACTIVE) to see which one it was. */
void Default_Handler(void)
{
    for (;;) {}
}

/* Every handler is a weak alias of Default_Handler. Defining a function with
 * the same name (e.g. EXTI0_IRQHandler) in an exercise replaces it. */
#define WEAK_ALIAS __attribute__((weak, alias("Default_Handler")))

void NMI_Handler(void)        WEAK_ALIAS;
void HardFault_Handler(void)  WEAK_ALIAS;
void MemManage_Handler(void)  WEAK_ALIAS;
void BusFault_Handler(void)   WEAK_ALIAS;
void UsageFault_Handler(void) WEAK_ALIAS;
void SVC_Handler(void)        WEAK_ALIAS;
void DebugMon_Handler(void)   WEAK_ALIAS;
void PendSV_Handler(void)     WEAK_ALIAS;
void SysTick_Handler(void)    WEAK_ALIAS;

/* Device interrupts, by the name used in the IRQn_Type enum of stm32f429xx.h. */
#define DEVICE_IRQS(X)                                                        \
    X(WWDG) X(PVD) X(TAMP_STAMP) X(RTC_WKUP) X(FLASH) X(RCC)                  \
    X(EXTI0) X(EXTI1) X(EXTI2) X(EXTI3) X(EXTI4)                              \
    X(DMA1_Stream0) X(DMA1_Stream1) X(DMA1_Stream2) X(DMA1_Stream3)           \
    X(DMA1_Stream4) X(DMA1_Stream5) X(DMA1_Stream6) X(ADC)                    \
    X(CAN1_TX) X(CAN1_RX0) X(CAN1_RX1) X(CAN1_SCE) X(EXTI9_5)                 \
    X(TIM1_BRK_TIM9) X(TIM1_UP_TIM10) X(TIM1_TRG_COM_TIM11) X(TIM1_CC)        \
    X(TIM2) X(TIM3) X(TIM4) X(I2C1_EV) X(I2C1_ER) X(I2C2_EV) X(I2C2_ER)       \
    X(SPI1) X(SPI2) X(USART1) X(USART2) X(USART3) X(EXTI15_10)                \
    X(RTC_Alarm) X(OTG_FS_WKUP) X(TIM8_BRK_TIM12) X(TIM8_UP_TIM13)            \
    X(TIM8_TRG_COM_TIM14) X(TIM8_CC) X(DMA1_Stream7) X(FMC) X(SDIO) X(TIM5)   \
    X(SPI3) X(UART4) X(UART5) X(TIM6_DAC) X(TIM7)                             \
    X(DMA2_Stream0) X(DMA2_Stream1) X(DMA2_Stream2) X(DMA2_Stream3)           \
    X(DMA2_Stream4) X(ETH) X(ETH_WKUP) X(CAN2_TX) X(CAN2_RX0) X(CAN2_RX1)     \
    X(CAN2_SCE) X(OTG_FS) X(DMA2_Stream5) X(DMA2_Stream6) X(DMA2_Stream7)     \
    X(USART6) X(I2C3_EV) X(I2C3_ER) X(OTG_HS_EP1_OUT) X(OTG_HS_EP1_IN)        \
    X(OTG_HS_WKUP) X(OTG_HS) X(DCMI) X(HASH_RNG) X(FPU) X(UART7) X(UART8)     \
    X(SPI4) X(SPI5) X(SPI6) X(SAI1) X(LTDC) X(LTDC_ER) X(DMA2D)

#define DECLARE_IRQ(name) void name##_IRQHandler(void) WEAK_ALIAS;
DEVICE_IRQS(DECLARE_IRQ)

typedef void (*vector_t)(void);

/* Entry 0 is the initial stack pointer, then the 15 system exceptions, then
 * the device interrupts at 16 + IRQn. Placing each one by its IRQn enum value
 * leaves the reserved slots (e.g. IRQ 79) as zero. */
#define VECTOR_IRQ(name) [16 + name##_IRQn] = name##_IRQHandler,

__attribute__((section(".isr_vector"), used))
const vector_t vector_table[16 + DMA2D_IRQn + 1] = {
    [0]  = (vector_t)&_estack,
    [1]  = Reset_Handler,
    [2]  = NMI_Handler,
    [3]  = HardFault_Handler,
    [4]  = MemManage_Handler,
    [5]  = BusFault_Handler,
    [6]  = UsageFault_Handler,
    [11] = SVC_Handler,
    [12] = DebugMon_Handler,
    [14] = PendSV_Handler,
    [15] = SysTick_Handler,
    DEVICE_IRQS(VECTOR_IRQ)
};

void Reset_Handler(void)
{
    /* Give the CPU full access to the FPU (coprocessors CP10 and CP11) before
     * any floating-point instruction runs. See PM0214, CPACR. */
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __DSB();
    __ISB();

    /* Copy initialised variables from flash to RAM, and zero the rest. */
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata; ) {
        *dst++ = *src++;
    }
    for (uint32_t *dst = &_sbss; dst < &_ebss; ) {
        *dst++ = 0;
    }

    /* Run C library and C++ static constructors. */
    __libc_init_array();

    main();
    for (;;) {}
}
