# Handoff: work in progress

Context for whoever picks this up next (human or Claude Code session).

## Equipment
- Hantek DSO2D15-class scope (2 ch, 150 MHz) with a built-in signal generator. The user wrote "DSOTD15"; the exact model is unconfirmed.
- Innomaker LA1010 logic analyzer: 16 channels, 100 MS/s, KingstVIS software, 2 PWM outputs.
- Nucleo-F429ZI: STM32F429ZI, 180 MHz Cortex-M4F. The ST-LINK provides an 8 MHz clock to HSE (bypass mode) and a virtual COM port on USART3 (PD8/PD9).

## Done
- The exercises are in `README.md` (moved from `EXERCISES.md`): 15 exercises plus capstones. The Nucleo is the device under test, and all code is bare metal (CMSIS register definitions only, no HAL).
- Pin plan, safety rules and helpers (debug pins, DWT cycle counter, printf) are in Part 0 of that file.

## Next task: convert the exercises to bare metal (register level, no HAL)
The user asked for this.

### Progress (2026-10-29)
- Step 1 skeleton is done and builds clean with `make all` (arm-none-eabi-gcc 15.3): `Makefile`, `linker.ld`, `startup.c`, `common/` (clock, dbg, dwt, uart3, gpio helpers, syscalls). CMSIS headers are **vendored** (copied, not submodules) in `firmware/cmsis/`.
- All exercise programs are written: `ex02`-`ex15` (exercise 1 needs no firmware). `make all` builds them all with no warnings, and every documented option (`STEP`, `SPEED`, `SMP`, `SINE_HZ`, `DTG`, `GLITCH_*`, `HSI_TRIM`, `SPI_MODE`, `BR`, `I2C_FAST`, `INTERNAL_PULLUPS`, `SENSOR_ADDR/REG`) was compile-checked. Key bitfield values were checked against RM0090 encodings.
- Not yet run on hardware. Flash `ex04` first: PC9 should read 36 MHz, which confirms `clock_init_180mhz()`.
- Design notes: ex02 Step A blinks from a SysTick interrupt; ex07 compares spinning / `__WFI()` sleep / interrupts-off (STEP 1-3); ex10's table is `uint32_t` so the DMA does word writes to DHR12R1, and 1 kHz is rounded (ARR 1405, printed at startup); ex03 and ex02 run on the 16 MHz HSI.
- Step 2 is done: the exercise guide (now in `README.md`) is fully bare metal, with snippets taken from the `firmware/` files. No HAL or CubeMX anywhere.
- Makefile conventions: pick a step with `EXTRA_CFLAGS=-DSTEP=n`, optimisation with `OPT=`, flasher with `FLASH_TOOL=cubeprog|openocd|stflash`.

### Plan

1. **Add a buildable `firmware/` skeleton:**
   - Toolchain: `arm-none-eabi-gcc` + Makefile.
   - CMSIS device headers from ST's `cmsis-device-f4` and the core headers from ARM `CMSIS_5`. Vendored: only the needed headers are copied.
   - Write our own `linker.ld` (2 MB flash @ 0x08000000, 192 KB SRAM @ 0x20000000, 64 KB CCM) and `startup.c` (vector table + Reset_Handler: copy `.data`, zero `.bss`, enable the FPU through `SCB->CPACR`, call `main`).
   - `common/`:
     - `clock.c`: 180 MHz setup. HSE bypass (`HSEON` | `HSEBYP`), PLL M=4 N=180 P=2 Q=8 (Q only matters for USB), `PWREN`, VOS scale 1, over-drive enable + switch (`ODEN`, then `ODSWEN`), `FLASH->ACR` LATENCY=5 + prefetch/ICEN/DCEN, AHB /1, APB1 /4 (45 MHz, timer clock 90 MHz), APB2 /2 (90 MHz, timer clock 180 MHz).
     - `dbg.h`: debug pins PG2/PG3.
     - `dwt.c`: cycle counter.
     - `uart3.c`: printf via `_write`. Note BRR uses the APB1 clock.
   - One source file per exercise (`ex02_toggle.c`, …), built with `make EX=07`, flashed with `make flash` (st-flash or openocd).
   - Early exercises can run on the 16 MHz HSI reset clock (no clock setup needed). Exercise 4 is where the user writes and verifies the 180 MHz setup.
2. **Rewrite the code snippets in `EXERCISES.md` to register level**, pointing at the files in `firmware/`. Reference **RM0090** (reference manual) sections for each peripheral: GPIO MODER/OSPEEDR/BSRR/IDR, EXTI+SYSCFG_EXTICR+NVIC, TIM2 PWM input (CCMR1 CC1S=01, CC2S=10, CC2P falling, SMCR TS=TI1FP1 SMS=reset), ADC1 external trigger TIM3 TRGO (EXTSEL=1000), DAC + TIM6 TRGO + DMA1 Stream5 Ch7, TIM1 BDTR DTG/MOE, USART6, SPI4, I2C1 (the F4 I2C peripheral needs CCR/TRISE and has the SB/ADDR/BTF event sequence).
3. **Compile-check everything with `arm-none-eabi-gcc` before committing.** Register and bit names must match the CMSIS header. A `make all` that builds every exercise is the test.
4. Keep the existing pin plan, safety section, logbook template and tone. Update the README to mention the firmware folder.

## Branch
Work happens on `main` (the earlier `claude/sweet-tesla-tzl9qa` branch was merged and deleted).
