# Bare-metal firmware for the exercises

Register-level C for the Nucleo-F429ZI: no HAL and no CubeIDE project, just the CMSIS headers, a Makefile, a linker script and a startup file you can read in a few minutes.

## Build and flash

Needs `arm-none-eabi-gcc` and `make`. Flashing uses `STM32_Programmer_CLI` (installed with STM32CubeProgrammer) by default.

```sh
make EX=02                         # build exercise 2 -> build/ex02.elf, build/ex02.bin
make flash EX=02                   # build and flash through the ST-LINK
make flash EX=02 EXTRA_CFLAGS=-DSTEP=1   # pick a step inside an exercise
make EX=05 OPT=-O0                 # different optimisation level
make all                           # build every exercise
make flash EX=02 FLASH_TOOL=openocd      # or FLASH_TOOL=stflash
```

Use two digits for `EX`. Changing `EXTRA_CFLAGS` or `OPT` triggers a full rebuild of that exercise.

## Layout

| Path | What it is |
|---|---|
| `startup.c` | Vector table and `Reset_Handler`: enables the FPU, copies `.data`, zeroes `.bss`, calls `main`. Every interrupt handler is a weak alias, so defining e.g. `EXTI0_IRQHandler` in an exercise replaces it. |
| `linker.ld` | 2 MB flash @ 0x08000000, 192 KB SRAM @ 0x20000000, 64 KB CCM @ 0x10000000 (CPU only, not DMA). |
| `common/clock.c` | `clock_init_180mhz()`: HSE bypass (8 MHz from the ST-LINK) → PLL → 180 MHz, with over-drive and 5 flash wait states. Without it the chip runs on the 16 MHz HSI. |
| `common/dbg.h` | Debug pins DBG0/DBG1 on PG2/PG3 (`DBG0_HI()` etc.). |
| `common/dwt.h` | `dwt_init()` and `delay_cycles()` with the DWT cycle counter. |
| `common/uart3.c` | `uart3_init(115200)`: `printf` to the ST-LINK virtual COM port (USART3, PD8/PD9). |
| `common/gpio.h` | One-line helpers for the MODER / OSPEEDR / PUPDR / AFR fields. |
| `common/syscalls.c` | Minimal newlib stubs (heap for `printf`). |
| `exNN_*.c` | One program per exercise. |
| `cmsis/` | Vendored CMSIS headers, see `cmsis/README.md`. |

The reference for every register is **RM0090** (STM32F42x/43x reference manual). Pin alternate functions are in the **STM32F429 datasheet (DS9405)**.
