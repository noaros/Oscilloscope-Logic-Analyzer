# Vendored CMSIS headers

Only the headers the firmware needs are copied here, unmodified.

| Folder | Source | Version | License |
|---|---|---|---|
| `device/` | [STMicroelectronics/cmsis_device_f4](https://github.com/STMicroelectronics/cmsis_device_f4) `Include/` | v2.6.8 | Apache-2.0 (`device/LICENSE.md`) |
| `core/` | [ARM-software/CMSIS_5](https://github.com/ARM-software/CMSIS_5) `CMSIS/Core/Include/` | 5.9.0 | Apache-2.0 (`core/LICENSE.txt`) |

`system_stm32f4xx.h` is included by `stm32f4xx.h`, but its `.c` file is not used: `common/clock.c` defines `SystemCoreClock` and sets up the clocks itself.
