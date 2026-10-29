/* System clock setup. RM0090 chapter 6 (RCC), the PWR chapter ("Entering Over-drive mode")
 * and section 3.5.1 (flash wait states). */

#include "clock.h"
#include "stm32f4xx.h"

/* CMSIS convention: the current HCLK frequency. Starts at the HSI reset value. */
uint32_t SystemCoreClock = 16000000;

void clock_init_180mhz(void)
{
    /* 1. HSE in bypass mode: the ST-LINK drives an 8 MHz clock into PH0, there
     *    is no crystal to oscillate. HSEBYP may only be written while HSE is off. */
    RCC->CR |= RCC_CR_HSEBYP;
    RCC->CR |= RCC_CR_HSEON;
    while (!(RCC->CR & RCC_CR_HSERDY)) {}

    /* 2. Voltage scale 1 is needed above 168 MHz. It takes effect once the PLL is on. */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;
    PWR->CR |= PWR_CR_VOS;

    /* 3. PLL: 8 MHz / M=4 = 2 MHz VCO input, x N=180 = 360 MHz VCO, / P=2 = 180 MHz.
     *    Q=8 gives 45 MHz; it only matters for USB/SDIO, which need 48 MHz and aren't used.
     *    Only the PLL fields are changed; the reserved bits keep their reset value. */
    RCC->PLLCFGR = (RCC->PLLCFGR & ~(RCC_PLLCFGR_PLLM | RCC_PLLCFGR_PLLN | RCC_PLLCFGR_PLLP |
                                     RCC_PLLCFGR_PLLSRC | RCC_PLLCFGR_PLLQ))
                 | (4UL   << RCC_PLLCFGR_PLLM_Pos)
                 | (180UL << RCC_PLLCFGR_PLLN_Pos)
                 | (0UL   << RCC_PLLCFGR_PLLP_Pos)     /* 00 = divide by 2 */
                 | RCC_PLLCFGR_PLLSRC_HSE
                 | (8UL   << RCC_PLLCFGR_PLLQ_Pos);
    RCC->CR |= RCC_CR_PLLON;

    /* 4. Over-drive mode is needed above 168 MHz. Enable it, then switch the
     *    regulator over, waiting for each step. */
    PWR->CR |= PWR_CR_ODEN;
    while (!(PWR->CSR & PWR_CSR_ODRDY)) {}
    PWR->CR |= PWR_CR_ODSWEN;
    while (!(PWR->CSR & PWR_CSR_ODSWRDY)) {}

    /* 5. Flash needs 5 wait states at 180 MHz and 2.7-3.6 V (RM0090 section 3.5.1).
     *    Set them *before* raising the clock, and read back to confirm. */
    FLASH->ACR = FLASH_ACR_LATENCY_5WS | FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != FLASH_ACR_LATENCY_5WS) {}

    /* 6. Bus prescalers: AHB /1, APB1 /4 (max 45 MHz), APB2 /2 (max 90 MHz).
     *    The MCO bits are left alone so an exercise can set them before this call. */
    RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_HPRE | RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2))
              | RCC_CFGR_HPRE_DIV1 | RCC_CFGR_PPRE1_DIV4 | RCC_CFGR_PPRE2_DIV2;

    /* 7. Wait for the PLL to lock, then make it the system clock. */
    while (!(RCC->CR & RCC_CR_PLLRDY)) {}
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

    SystemCoreClock = 180000000;
}

/* PPREx field values 0xx = /1, 100 = /2, 101 = /4, 110 = /8, 111 = /16. */
static uint32_t apb_divider(uint32_t ppre)
{
    return (ppre < 4) ? 1 : 1UL << (ppre - 3);
}

uint32_t clock_apb1_hz(void)
{
    return SystemCoreClock / apb_divider((RCC->CFGR & RCC_CFGR_PPRE1) >> RCC_CFGR_PPRE1_Pos);
}

uint32_t clock_apb2_hz(void)
{
    return SystemCoreClock / apb_divider((RCC->CFGR & RCC_CFGR_PPRE2) >> RCC_CFGR_PPRE2_Pos);
}
