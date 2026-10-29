# Exercises: Hantek scope + signal generator + Innomaker LA1010 + Nucleo-F429ZI

These exercises use four tools together:

| Tool | Role |
|---|---|
| **Oscilloscope** (Hantek DSO2D15-class, 2 channels) | Shows the *analog* truth: voltage over time, edge shapes, noise. |
| **Signal generator** (the scope's built-in AWG / "Gen Out" BNC) | A known, adjustable *stimulus* to feed into the board. |
| **Logic analyzer** (Innomaker LA1010, 16 channels, KingstVIS) | Shows the *digital interpretation* across many pins at once and decodes protocols. Its two PWM outputs act as an extra signal source. |
| **Nucleo-F429ZI** (STM32F429ZI, Cortex-M4F @ 180 MHz) | The device under test. Sometimes it *receives* the generator's signal, sometimes it *produces* signals, and often it does both. |

The idea running through all of them: **the generator is a reference you trust, the scope tells you what a signal is, the logic analyzer tells you what a digital circuit thinks it is, and the microcontroller is the thing you are trying to understand.** Most of what you learn comes from the places where these disagree.

All the firmware is **bare metal**: plain C that writes the STM32's registers directly, using only the CMSIS register definitions. There is no HAL, no CubeMX and no generated code, so every line that touches the hardware is one you can look up in the reference manual. Each exercise has a complete program in [`firmware/`](firmware/); the snippets below are the important parts of it.

---

## Part 0: Setup (do this once)

### 0.1 Safety rules for the microcontroller (read this!)

The STM32 pins are **3.3 V logic**. The signal generator can easily produce voltages that damage them: negative voltages, or more than 3.3 V.

1. **Before connecting the generator to the board, check its output on the scope with nothing else connected.** Confirm the minimum is ≥ 0 V and the maximum is ≤ 3.3 V. Many generators power up with a waveform centred on 0 V, which means a negative swing.
2. For a 0–3.3 V signal, set **amplitude 3.3 Vpp, offset +1.65 V**. If your generator has a *High-Z / 50 Ω* load setting, choose **High-Z**.
3. **Always put a 1 kΩ resistor in series** between the generator and any STM32 pin. If you make a mistake, it limits the current to a level the pin's protection diodes can survive.
4. Connect **one common ground**: board GND, scope probe ground, LA GND and the generator BNC shield.
5. Don't connect the generator to a pin the firmware has configured as an **output**. Two outputs fighting each other can damage a pin. The 1 kΩ resistor helps here too.

### 0.2 Software and documents

- **`arm-none-eabi-gcc`** (Arm GNU Toolchain) and **`make`**.
- **STM32CubeProgrammer**, for its command-line flasher `STM32_Programmer_CLI`. `openocd` or `st-flash` work too.
- **KingstVIS** for the LA1010.
- A serial terminal (PuTTY, screen, minicom) at **115200 baud** for `printf` output. The board's ST-LINK USB connection shows up as a serial port.

Keep these PDFs from st.com open; the code comments point into them:

| Document | What's in it |
|---|---|
| **RM0090** (reference manual) | Every peripheral register and bit. The main reference. |
| **DS9405** (STM32F429 datasheet) | Pin alternate-function table, electrical limits (V_IH / V_IL, rise times). |
| **UM1974** (Nucleo-144 user manual) | Which board pins connect to what. |
| **PM0214** (Cortex-M4 programming manual) | NVIC, SysTick, FPU, exceptions. |

### 0.3 Building and flashing

From the `firmware/` folder:

```sh
make flash EX=07                         # build and flash exercise 7
make flash EX=07 EXTRA_CFLAGS=-DSTEP=2   # pick a step inside an exercise
make all                                 # build every exercise
```

Each exercise is one file, `firmware/exNN_*.c`, built together with:

- `startup.c`: the vector table and the reset code (enable the FPU, copy initialised data to RAM, zero the rest, call `main`).
- `linker.ld`: where flash and RAM are.
- `common/`: the clock setup and the helpers in 0.5.

See [`firmware/README.md`](firmware/README.md) for all the options.

### 0.4 Pin plan

The Nucleo-144 board shares some pins with Ethernet, the ST-LINK, the LEDs and the button. This plan avoids those conflicts. **Double-check each pin against the board user manual (UM1974)** before wiring.

| Function | Pin | Notes |
|---|---|---|
| Debug pin DBG0 / DBG1 | **PG2 / PG3** | General-purpose outputs used as "logic-analyzer printf" |
| Generator input (EXTI / GPIO) | **PG0** | Through 1 kΩ |
| Input capture (TIM2_CH1) | **PA0** | Through 1 kΩ. TIM2 is a 32-bit timer. |
| ADC input (ADC1_IN3) | **PA3** (A0) | Through 1 kΩ, 0–3.3 V only |
| DAC output (DAC_OUT1) | **PA4** | |
| Clock output MCO1 / MCO2 | **PA8 / PC9** | |
| PWM TIM1_CH1 / TIM1_CH1N | **PE9 / PE8** | Complementary pair |
| UART (USART6 TX / RX) | **PG14 / PG9** (D1 / D0) | A UART you *can* probe |
| SPI4 SCK / MISO / MOSI / NSS | **PE2 / PE5 / PE6 / PE4** | Avoids SPI1's conflict with Ethernet |
| I²C1 SCL / SDA | **PB8 / PB9** (D15 / D14) | |

**Avoid:** PA1, PA2, PA7, PB13, PC1, PC4, PC5, PG11, PG13 (Ethernet); PD8/PD9 (ST-LINK virtual COM); PA13/PA14 (SWD debug); PB0, PB7, PB14 (LEDs); PC13 (button); PH0/PH1 (the clock input from the ST-LINK).

### 0.5 The bare-metal pattern, and the helpers you'll use everywhere

Almost every peripheral is set up the same way:

1. **Turn on its clock** in an `RCC->AHB1ENR`, `APB1ENR` or `APB2ENR` bit. Until you do, its registers ignore writes.
2. **Give it its pins**: set each pin to *alternate function* mode and choose the AF number from the datasheet's alternate-function table.
3. **Write its configuration registers**, then set its enable bit last.

`common/gpio.h` has one-line helpers for step 2, because each pin's settings are 2- or 4-bit fields packed into shared registers:

```c
gpio_mode(GPIOG, 0, GPIO_MODE_IN);      /* MODER:   input / output / alternate function / analog */
gpio_speed(GPIOG, 2, GPIO_SPEED_HIGH);  /* OSPEEDR: output slew rate */
gpio_pull(GPIOB, 8, GPIO_PULL_UP);      /* PUPDR:   internal pull-up / pull-down */
gpio_af(GPIOD, 8, 7);                   /* AFR:     alternate function 7, and MODER = AF */
```

**Debug pins** (`common/dbg.h`). Toggling a GPIO is the fastest, least intrusive way to make firmware events visible on the LA or the scope. A write to the port's BSRR register sets or clears a pin in a single store instruction, without reading the port first:

```c
#define DBG0_HI()  (GPIOG->BSRR = GPIO_BSRR_BS2)
#define DBG0_LO()  (GPIOG->BSRR = GPIO_BSRR_BR2)
#define DBG1_HI()  (GPIOG->BSRR = GPIO_BSRR_BS3)
#define DBG1_LO()  (GPIOG->BSRR = GPIO_BSRR_BR3)

dbg_init(GPIO_SPEED_VERY_HIGH);   /* GPIOG clock on, PG2/PG3 as outputs */
```

**Cycle counter** (`common/dwt.h`). The DWT unit counts CPU cycles; at 180 MHz one cycle is 5.56 ns.

```c
static inline void dwt_init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
static inline void delay_cycles(uint32_t n) {
  uint32_t t0 = DWT->CYCCNT;
  while ((DWT->CYCCNT - t0) < n) {}
}
```

**printf over USB** (`common/uart3.c`). `uart3_init(115200)` sets up USART3 on PD8/PD9, which the ST-LINK forwards to the PC. The C library calls `_write` for every `printf`; this one sends the bytes one at a time:

```c
int _write(int fd, char *p, int len) {
  for (int i = 0; i < len; i++) {
    while (!(USART3->SR & USART_SR_TXE)) {}   /* wait until the transmit register is empty */
    USART3->DR = (uint8_t)p[i];
  }
  return len;
}
```

**Clock** (`common/clock.c`). After reset the chip runs from its internal 16 MHz RC oscillator (HSI). `clock_init_180mhz()` switches to the full 180 MHz; you'll go through it in Exercise 4. Exercises 2 and 3 run on the 16 MHz HSI, and everything from Exercise 4 on runs at 180 MHz.

---

## Level 1: Get the instruments and the board talking

### Exercise 1: Sanity check all three instruments
**Goal:** Trust your tools before you trust your measurements.

1. **Probe compensation:** connect each probe to the scope's 1 kHz probe-comp tab and adjust the trimmer until the square wave has square corners. Use **x10** on the probe *and* in the channel menu.
2. **Generator check:** set **square, 10 kHz, 0–3.3 V** (3.3 Vpp, 1.65 V offset). Turn on the scope measurements: Freq, Vmax, Vmin, +Duty. Confirm the signal really is 0–3.3 V. This is the setting you'll use with the board.
3. **First LA capture:** connect the same signal to LA CH0. In KingstVIS set **10 MHz** sample rate and the threshold to **1.65 V** (or the 3.3 V CMOS preset). Capture, then read the frequency and duty cycle.

**Check:** The scope and the LA both report 10 kHz / 50%. The LA's resolution is 1 / sample rate (100 ns here). Is the difference between them smaller than that?

### Exercise 2: From Blinky to "how fast can a pin toggle?"
**Goal:** See how much each instruction costs, and see aliasing in the LA.
**Firmware:** `ex02_toggle.c`, running on the 16 MHz HSI.

1. **Step A** (`STEP=1`): blink LD1 (PB0) from the **SysTick** timer. SysTick is a 24-bit down-counter built into every Cortex-M core. Loaded with 16 000 at 16 MHz, it interrupts every 1 ms:
   ```c
   SysTick->LOAD = SystemCoreClock / 1000 - 1;
   SysTick->VAL  = 0;
   SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | SysTick_CTRL_TICKINT_Msk | SysTick_CTRL_ENABLE_Msk;

   void SysTick_Handler(void) {
     if (++ms_ticks % 100 == 0) GPIOB->ODR ^= GPIO_ODR_OD0;   /* toggle every 100 ms */
   }
   ```
   Probe PB0 on the scope and LA. It should be 5 Hz. How close is it? The HSI is only accurate to about 1%; Exercise 4 measures it.
2. **Step B** (`STEP=2`): toggle DBG0 as fast as possible by flipping the bit in the output register:
   ```c
   while (1) { GPIOG->ODR ^= GPIO_ODR_OD2; }
   ```
   That's a read, an XOR and a write per toggle. Measure the frequency on the scope.
3. **Step C** (`STEP=3`): replace that with BSRR writes:
   ```c
   while (1) { DBG0_HI(); DBG0_LO(); }
   ```
   Measure again. It should be noticeably faster. The pulse may also be *asymmetric*, because the loop branch adds time to only one half. Measure +width and −width on the scope. To see the actual instructions, run `arm-none-eabi-objdump -d build/ex02.elf` and find `main`.
4. **Aliasing:** capture the Step C signal on the LA at 100, 50, 20, 10 and 5 MHz. Write down what frequency and duty cycle the LA *claims* at each rate.

**Takeaway:** Sample a digital signal at **at least 4×, ideally 10×** its fastest frequency. When the LA is too slow it doesn't warn you; it just shows the wrong answer. The scope, at hundreds of MS/s, shows the truth. (Come back after Exercise 4 and add `clock_init_180mhz()` to Step C: the toggle gets much faster, though not the full 180 / 16 ≈ 11×, because of the flash wait states, and the LA's aliasing problem gets much worse.)

### Exercise 3: GPIO speed settings and edge rates
**Goal:** Learn what the OSPEEDR register actually does.
**Firmware:** `ex03_edges.c`.

1. The firmware makes a square wave on DBG0 with `delay_cycles(16)` in each half. Pick the output speed with `SPEED=0` (Low), `1` (Medium), `2` (High) or `3` (Very High):
   ```c
   gpio_speed(GPIOG, 2, SPEED);   /* OSPEEDR bits 5:4 for pin 2 */
   ```
2. On the scope, measure **rise time** and **fall time** of DBG0 for each speed setting.
3. For the fastest settings, replace the probe's long ground clip with the short **ground spring**, and see how much the ringing changes.

**Check:** The frequency stays the same for every setting: OSPEEDR changes only how hard the output driver switches, not the code's timing. Fast edges look great but cause ringing and EMI. Slow edges are cleaner but limit the maximum frequency. That's why the setting exists. Also note: your scope's bandwidth (150 MHz → about 2.3 ns rise time) sets a lower limit on what it can show.

### Exercise 4: Bring up 180 MHz and check it with MCO
**Goal:** Configure the clock tree yourself, then verify it with an instrument instead of trusting the code.
**Firmware:** `ex04_mco.c` and `common/clock.c`.

1. **Read `clock_init_180mhz()`** with RM0090 chapter 6 (RCC) open. It does seven things, in this order:
   1. Start **HSE in bypass mode**: the ST-LINK feeds an 8 MHz clock into PH0, so there's no crystal to start (`HSEBYP`, then `HSEON`, wait for `HSERDY`).
   2. Select **voltage scale 1** in the power controller (`PWR->CR VOS`). It's needed above 168 MHz.
   3. Configure the **PLL**: 8 MHz / M=4 = 2 MHz, × N=180 = 360 MHz, / P=2 = **180 MHz**. Then set `PLLON`.
   4. Enable **over-drive** (`ODEN`, wait `ODRDY`, then `ODSWEN`, wait `ODSWRDY`), which is also needed above 168 MHz.
   5. Set **5 flash wait states**. Flash can't be read at 180 MHz in one cycle, and this must be set *before* the clock goes up.
   6. Set the **bus prescalers**: AHB /1 = 180 MHz, APB1 /4 = 45 MHz, APB2 /2 = 90 MHz. Timers on a divided bus get twice the bus clock: 90 MHz on APB1, 180 MHz on APB2.
   7. Wait for the PLL to lock, then **switch** the system clock to it (`SW`, then wait for `SWS` to confirm).

   For each step, predict what goes wrong if you skip it. (Step 5 is the classic one: the CPU reads garbage from flash and crashes.)
2. The firmware routes clocks to the MCO pins by writing `RCC->CFGR` **before** the PLL starts, because RM0090 warns that changing them later can glitch the output:
   ```c
   RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_MCO1 | RCC_CFGR_MCO1PRE | RCC_CFGR_MCO2 | RCC_CFGR_MCO2PRE))
             | RCC_CFGR_MCO1_1                   /* MCO1 = 10: HSE */
             | (0UL << RCC_CFGR_MCO2_Pos)        /* MCO2 = 00: SYSCLK */
             | (7UL << RCC_CFGR_MCO2PRE_Pos);    /* MCO2PRE = 111: divide by 5 */
   clock_init_180mhz();
   gpio_af(GPIOA, 8, 0);                         /* PA8 = MCO1 */
   gpio_af(GPIOC, 9, 0);                         /* PC9 = MCO2 */
   ```
3. Measure **MCO2 (PC9)** on the scope's frequency counter. **36 MHz means your 180 MHz setup works.** Then capture it on the LA at 100 MHz, and work out why the LA's reading isn't trustworthy (hint: Exercise 2).
4. Measure **MCO1 (PA8)**, which is HSE: 8 MHz from the ST-LINK's crystal. Then rebuild with `EXTRA_CFLAGS=-DMCO1_HSI` to output the internal 16 MHz RC oscillator instead.
5. Write down how far each one is from its nominal value, **in ppm** (parts per million): (measured − nominal) / nominal × 10⁶.

**Takeaway:** A crystal is typically within ±50 ppm. HSI is specified to about ±1% (10 000 ppm). You'll see in Exercise 13 why that matters for UART.

### Exercise 5: Profiling code with debug pins
**Goal:** Measure exactly how long a piece of code takes.
**Firmware:** `ex05_profile.c`.

1. The firmware brackets some code with a debug pin, and times it with the cycle counter too:
   ```c
   volatile float x = 2.0f, y;
   uint32_t t0 = DWT->CYCCNT;
   DBG0_HI();
   for (int i = 0; i < 100; i++) y = sqrtf(x + i);
   DBG0_LO();
   uint32_t cycles = DWT->CYCCNT - t0;
   ```
2. Measure the pulse width with the LA. Repeat at least 10 times and look at the variation.
3. Compare: `sqrtf` (`STEP=1`, single precision, uses the FPU) vs `sqrt` (`STEP=2`, double precision; the M4's FPU is single precision only, so this runs in software). Also compare optimization levels with `OPT=-O0` and the default `-O2`.
4. DBG1 marks the `printf` that reports the cycle count. Measure both regions on the same capture.

**Check:** Does the printed cycle count agree with the LA pulse width (cycles × 5.56 ns)? The `printf` takes far longer than the maths: count the characters and work out why, at 115200 baud.

---

## Level 2: The microcontroller *listens* to the generator

### Exercise 6: Measure the STM32's input thresholds and hysteresis
**Goal:** Find out at what voltage the chip decides "0" or "1".
**Firmware:** `ex06_threshold.c`.

1. **PG0 is an input** (no pull). The firmware copies it to DBG0 as fast as possible by reading the input data register:
   ```c
   gpio_pull(GPIOG, 0, GPIO_PULL_NONE);
   gpio_mode(GPIOG, 0, GPIO_MODE_IN);
   while (1) {
     if (GPIOG->IDR & GPIO_IDR_ID0) DBG0_HI(); else DBG0_LO();
   }
   ```
2. Generator: **triangle, 100 Hz, 0–3.3 V** → 1 kΩ → PG0.
3. Scope CH1 = PG0 (the input), CH2 = DBG0 (the output). Trigger on CH2's rising edge.
4. With a horizontal cursor, read the input voltage at the instant the output goes **high**, then the voltage when it goes **low**. Their difference is the **hysteresis**.
5. Compare with the datasheet's V_IH / V_IL. Then set the LA threshold to the same values and compare the LA's view of PG0 with the chip's view (DBG0).

**Extension:** Add noise to the triangle (use the generator's noise or arbitrary mode if it has one, or a long unshielded wire you touch). Without hysteresis there would be many false edges. Does DBG0 stay clean? Does the LA's view of PG0 stay clean?

### Exercise 7: Interrupt latency and jitter
**Goal:** Measure how quickly the CPU can respond to an external event.
**Firmware:** `ex07_latency.c`.

1. Setting up a pin interrupt takes three blocks of hardware:
   - **SYSCFG** chooses which port drives EXTI line 0 (all the pin-0s share one line).
   - **EXTI** chooses the edge and unmasks the line.
   - **NVIC** lets the interrupt reach the CPU.
   ```c
   RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
   SYSCFG->EXTICR[0] = (SYSCFG->EXTICR[0] & ~SYSCFG_EXTICR1_EXTI0) | SYSCFG_EXTICR1_EXTI0_PG;
   EXTI->RTSR |= EXTI_RTSR_TR0;     /* rising edge */
   EXTI->IMR  |= EXTI_IMR_MR0;      /* unmask line 0 */
   NVIC_EnableIRQ(EXTI0_IRQn);

   void EXTI0_IRQHandler(void) {    /* the name puts it in the vector table (startup.c) */
     EXTI->PR = EXTI_PR_PR0;        /* clear the pending flag by writing 1 */
     DBG0_HI();
     DBG0_LO();
   }
   ```
2. Generator: **square, 1 kHz, 0–3.3 V** → 1 kΩ → PG0.
3. LA: CH0 = PG0, CH1 = DBG0, 100 MHz. Measure the delay from the input edge to the DBG0 pulse. Collect at least 20 samples and note the minimum, maximum and spread (**jitter**).
4. **Sleeping vs spinning:** Step 1 (the default) leaves the main loop spinning. Rebuild with `STEP=2`, where the main loop sleeps in `__WFI()` (wait for interrupt) between edges. Does waking up from sleep add latency? Does the jitter change?
5. Increase the generator frequency (10 kHz, 100 kHz, 500 kHz, 1 MHz…). Find where the LA shows **missed pulses** on DBG0.
6. Rebuild with `STEP=3`: the main loop turns interrupts off for 10 µs at a time (`__disable_irq()`, `delay_cycles(1800)`, `__enable_irq()`). Watch the worst-case latency grow.

**Takeaway:** The hardware minimum is about 12 cycles (≈ 67 ns), but real latency depends on the software around it. The LA is the only way to *see* the worst case.

### Exercise 8: Build a hardware frequency and duty-cycle meter (input capture)
**Goal:** Let a timer measure the signal in hardware, and compare three instruments.
**Firmware:** `ex08_capture.c`.

1. **TIM2** in *PWM input mode* (RM0090 "PWM input mode" in the TIM2–TIM5 chapter) uses two capture channels on the same pin, PA0 (AF1):
   - **IC1** captures on the rising edge, giving the **period**.
   - **IC2** captures on the falling edge, giving the **high time**.
   - The **slave controller** resets the counter on every rising edge.

   The timer clock is 90 MHz, so the resolution is 11.1 ns.
   ```c
   TIM2->ARR   = 0xFFFFFFFF;
   TIM2->CCMR1 = (1UL << TIM_CCMR1_CC1S_Pos)    /* CC1S = 01: IC1 from TI1 (PA0) */
               | (2UL << TIM_CCMR1_CC2S_Pos);   /* CC2S = 10: IC2 also from TI1 */
   TIM2->CCER  = TIM_CCER_CC2P                  /* IC2 on the falling edge */
               | TIM_CCER_CC1E | TIM_CCER_CC2E;
   TIM2->SMCR  = (5UL << TIM_SMCR_TS_Pos)       /* trigger = TI1FP1 */
               | (4UL << TIM_SMCR_SMS_Pos);     /* slave mode = reset */
   TIM2->CR1   = TIM_CR1_CEN;
   ```
2. The main loop reads the captures and prints them:
   ```c
   uint32_t period = TIM2->CCR1 + 1;
   uint32_t high   = TIM2->CCR2 + 1;
   printf("f=%.3f Hz duty=%.2f%%\r\n", (double)f_tim / period, 100.0 * high / period);
   ```
   (The `+1` follows the reset-on-edge convention. Check its effect at high frequencies.)
3. Generator: square 0–3.3 V at 100 Hz, 10 kHz, 1 MHz, and as high as it will go. Change the duty cycle too.
4. For each setting, write down the result from **the generator's dial, the scope, the LA and the STM32** side by side.

**Questions:**
- At 1 kHz the MCU and the generator disagree by a few hundred ppm. Who's right? (Exercise 4 told you how accurate the MCU's clock is.)
- Why does the resolution of the MCU's duty-cycle reading get worse at high frequency?

**Extension:** Use the LA1010's PWM output as a second, independent source and measure it with the scope and the MCU. Now you have four opinions.

### Exercise 9: ADC → DAC passthrough and aliasing you can see
**Goal:** Watch sampling theory happen on the scope screen.
**Firmware:** `ex09_adc_dac.c`.

1. Here three peripherals are chained in hardware:
   - **TIM3** overflows at **10 kHz** and sends a trigger pulse (TRGO) on each overflow.
   - That pulse starts an **ADC1** conversion of IN3 (PA3).
   - The end-of-conversion interrupt writes the sample to **DAC channel 1** (PA4).
   ```c
   ADC->CCR    = ADC_CCR_ADCPRE_0;                 /* ADC clock = 90 MHz / 4 = 22.5 MHz */
   ADC1->SMPR2 = 3UL << ADC_SMPR2_SMP3_Pos;        /* 56-cycle sampling time on channel 3 */
   ADC1->SQR3  = 3UL << ADC_SQR3_SQ1_Pos;          /* convert channel 3 */
   ADC1->CR1   = ADC_CR1_EOCIE;                    /* interrupt at end of conversion */
   ADC1->CR2   = ADC_CR2_EXTEN_0                   /* start on a rising trigger edge... */
               | (8UL << ADC_CR2_EXTSEL_Pos)       /* ...from TIM3 TRGO (EXTSEL = 1000) */
               | ADC_CR2_ADON;
   TIM3->ARR = 8999;                               /* 90 MHz / 9000 = 10 kHz */
   TIM3->CR2 = 2UL << TIM_CR2_MMS_Pos;             /* TRGO on each update */

   void ADC_IRQHandler(void) {
     DBG0_HI();
     DAC1->DHR12R1 = ADC1->DR;                     /* 12-bit sample straight to the DAC */
     DBG0_LO();
   }
   ```
2. Generator: **sine, 0.3–3.0 V** (2.7 Vpp, 1.65 V offset) → 1 kΩ → PA3. Scope CH1 = generator, CH2 = DAC output (PA4).
3. Sweep the generator: 100 Hz, 1 kHz, 4 kHz, 5 kHz, **9 kHz, 9.9 kHz, 11 kHz**.

**What you should see:** At 100 Hz the output is a staircase copy of the input. At 9 kHz the DAC output looks like a clean **1 kHz** sine. That's aliasing: a frequency that doesn't exist in the input. At 9.9 kHz you get 100 Hz. At exactly 5 kHz (Nyquist) the result depends on the phase.

4. LA: CH0 = DBG0. Confirm the sample rate really is 10 kHz, and measure how much of each 100 µs sample period the interrupt handler uses.
5. Change the ADC sampling time (`SMP=0` is 3 cycles, `SMP=7` is 480) and the input resistor (1 kΩ → 47 kΩ). What happens to accuracy? (The sampling capacitor needs time to charge through the source resistance.)

---

## Level 3: The microcontroller *talks*

### Exercise 10: DAC waveform generator + RC reconstruction filter
**Goal:** Build a simple signal generator and compare it with the real one.
**Firmware:** `ex10_dac_dma.c`.

1. The setup has four parts:
   - The firmware fills a 64-point sine table.
   - **TIM6** sends a trigger 64 000 times a second.
   - Each trigger makes the **DAC** ask for its next sample.
   - **DMA1 Stream 5 Channel 7** copies the next table entry into the DAC, wrapping around at the end (circular mode).

   Once it's started, the CPU does nothing at all.
   ```c
   DMA1_Stream5->PAR  = (uint32_t)&DAC1->DHR12R1;    /* to: the DAC's data register */
   DMA1_Stream5->M0AR = (uint32_t)lut;               /* from: the table */
   DMA1_Stream5->NDTR = 64;
   DMA1_Stream5->CR   = (7UL << DMA_SxCR_CHSEL_Pos)  /* channel 7 = DAC1 request */
                      | DMA_SxCR_MSIZE_1 | DMA_SxCR_PSIZE_1   /* 32-bit transfers */
                      | DMA_SxCR_MINC | DMA_SxCR_CIRC
                      | DMA_SxCR_DIR_0                /* memory to peripheral */
                      | DMA_SxCR_EN;
   DAC1->CR  = DAC_CR_TEN1 | DAC_CR_DMAEN1 | DAC_CR_EN1;   /* trigger TSEL1 = 000: TIM6 */
   TIM6->CR2 = 2UL << TIM_CR2_MMS_Pos;                     /* TRGO on update */
   TIM6->CR1 = TIM_CR1_CEN;
   ```
   90 MHz / 64 kHz = 1406.25 isn't a whole number, so the firmware rounds and prints the frequency you actually get (about 1000.2 Hz). Pick another with `SINE_HZ=2000`.
2. Scope CH1 = PA4 (the MCU sine), CH2 = the Hantek generator's 1 kHz sine. Zoom in on the MCU's **staircase**.
3. Add an **RC low-pass filter** on PA4: 1 kΩ in series, then 100 nF to GND (cutoff ≈ 1.6 kHz). Do the steps disappear? What happens to the amplitude and phase? Try 10 nF (cutoff ≈ 16 kHz) instead.
4. Use the scope's **FFT** (if available) on both signals and compare the harmonics with the Hantek AWG's.

**Extension (Bode plot):** Run the *Hantek* generator's sine (0–3.3 V) through the same RC filter. Sweep 100 Hz → 100 kHz and measure gain (Vout/Vin) and phase (Δt/T × 360°) on the scope. At the cutoff you should see 0.707× and −45°. Plot gain in dB against log(frequency).

### Exercise 11: Complementary PWM with dead time
**Goal:** Measure a timing detail that matters in motor drivers and power supplies.
**Firmware:** `ex11_deadtime.c`.

1. **TIM1 CH1 + CH1N** (PE9 / PE8, AF1) make PWM at **100 kHz, 30% duty**. TIM1 runs at 180 MHz:
   ```c
   TIM1->ARR   = 1799;                            /* 180 MHz / 1800 = 100 kHz */
   TIM1->CCR1  = 540;                             /* 540 / 1800 = 30% */
   TIM1->CCMR1 = (6UL << TIM_CCMR1_OC1M_Pos)      /* PWM mode 1 */
               | TIM_CCMR1_OC1PE;
   TIM1->CCER  = TIM_CCER_CC1E | TIM_CCER_CC1NE;  /* CH1 and its complement */
   TIM1->BDTR  = (DTG << TIM_BDTR_DTG_Pos)        /* dead time */
               | TIM_BDTR_MOE;                    /* main output enable */
   TIM1->EGR   = TIM_EGR_UG;
   TIM1->CR1   = TIM_CR1_ARPE | TIM_CR1_CEN;
   ```
   TIM1 is an *advanced* timer: its outputs stay off until you set **MOE**, a safety feature for power stages.
2. Set **dead time** with `DTG=0` (default), then `DTG=18`, then `DTG=90`. In this range one DTG step is 1 / 180 MHz = 5.56 ns.
3. LA CH0 = CH1, CH1 = CH1N at 100 MHz: measure the gap where **both are low**. Then measure the same gap on the scope (CH1 and CH2) with cursors.

**Check:** With DTG = 18 you expect 100 ns. The LA can only resolve 10 ns steps; the scope can do better. Which one would you trust for a 20 ns dead time?

### Exercise 12: Glitch hunting with triggers
**Goal:** Catch a rare, wrong event, which is what most real debugging looks like.
**Firmware:** `ex12_glitch.c`.

1. The firmware makes a stream of 10 µs pulses, but every 1000th pulse is only 1 µs long:
   ```c
   for (uint32_t n = 0;; n++) {
     if (n % 1000 == 0) { DBG1_HI(); DBG1_LO(); }   /* marker */
     DBG0_HI();
     delay_cycles((n % 1000 == 999) ? 180 : 1800);
     DBG0_LO();
     delay_cycles(1800);
   }
   ```
2. On the scope, just looking at the signal, the glitch is nearly invisible. Set up a **pulse-width trigger**: positive pulse **< 5 µs**. Now the scope shows only the glitch.
3. Do the same in KingstVIS with a **pulse-width trigger** on CH0. DBG1 marks the start of each group of 1000: check how many pulses lie between the marker and the glitch.
4. Make it harder: `EXTRA_CFLAGS="-DGLITCH_EVERY=100000"` for a rarer glitch, or `-DGLITCH_CYCLES=18` for a 100 ns one. Can both instruments still catch it?

---

## Level 4: Protocols (the payoff)

### Exercise 13: UART: decoding, baud-rate error and clock accuracy
**Firmware:** `ex13_uart.c`.

1. **USART6** (TX = PG14, RX = PG9, AF8) runs at **115200 8N1** and sends `"Hello\r\n"` every 100 ms. The baud rate comes from one division of the bus clock:
   ```c
   USART6->BRR = (clock_apb2_hz() + baud / 2) / baud;   /* 90 MHz / 115200 = 781.25 -> 781 */
   USART6->CR1 = USART_CR1_UE | USART_CR1_TE | USART_CR1_RE;

   while (!(USART6->SR & USART_SR_TXE)) {}   /* per character */
   USART6->DR = c;
   ```
   The rounding from 781.25 to 781 is itself a baud-rate error. Work it out in %.
2. LA CH0 = PG14 with KingstVIS's **Async Serial** decoder at 115200 8N1. You should see the text decoded above the waveform.
3. Scope: measure one bit width (expected 1 / 115200 = **8.68 µs**). Check the levels and the idle state (high).
4. Set the decoder to the **wrong baud rate** (57600, 230400) and look at the garbage, including framing errors.
5. **Clock experiment:** rebuild with `EXTRA_CFLAGS="-DSTEP=2 -DHSI_TRIM=16"`. The firmware now skips `clock_init_180mhz()` and runs everything from the HSI, with its trim bits set:
   ```c
   RCC->CR = (RCC->CR & ~RCC_CR_HSITRIM) | (HSI_TRIM << RCC_CR_HSITRIM_Pos);   /* 0-31, reset value 16 */
   ```
   Change `HSI_TRIM` step by step (try 20, 24, 28, then 12, 8, 4) and measure the actual bit width with the LA each time. At what error does the decoder (or a real receiver) start failing? UART typically tolerates about ±2–3% in total. The `printf` output on the ST-LINK port is detuned too, so it may fail at the same point.

### Exercise 14: SPI: clock modes and loopback
**Firmware:** `ex14_spi.c`.

1. **SPI4** is the master: SCK = PE2, MISO = PE5, MOSI = PE6 (all AF5), with NSS on PE4 driven as an ordinary output. Connect **MOSI to MISO** with a jumper (loopback). The firmware sends `0xA5, 0x3C, 0x00, 0xFF` every 100 µs and counts frames where what came back doesn't match:
   ```c
   SPI4->CR1 = SPI_CR1_MSTR | SPI_CR1_SSM | SPI_CR1_SSI   /* master, NSS handled in software */
             | (BR << SPI_CR1_BR_Pos)                     /* SCK = 90 MHz / 2^(BR+1) */
             | (CPOL ? SPI_CR1_CPOL : 0) | (CPHA ? SPI_CR1_CPHA : 0);
   SPI4->CR1 |= SPI_CR1_SPE;

   static uint8_t spi4_transfer(uint8_t out) {
     while (!(SPI4->SR & SPI_SR_TXE)) {}
     SPI4->DR = out;
     while (!(SPI4->SR & SPI_SR_RXNE)) {}   /* SPI always receives while it sends */
     return SPI4->DR;
   }
   ```
   The default `BR=5` gives 1.41 MHz.
2. LA: CH0 = SCK, CH1 = MOSI, CH2 = MISO, CH3 = NSS. Add an **SPI decoder**. Check that the MISO bytes match the MOSI bytes.
3. Change the firmware's clock mode with `SPI_MODE=0` to `3` (CPOL is bit 1, CPHA bit 0). Keep the decoder on mode 0 and see which modes decode wrongly. Then fix the decoder settings.
4. Increase the SPI clock with `BR=4`, `3`, … down to `0` (45 MHz). Find where the **LA** stops decoding correctly (hint: sample rate), and where the **scope** shows that the SCK edges are no longer square. Does the loopback error count stay at zero?

### Exercise 15: I²C: ACK/NACK and pull-up resistors
**Firmware:** `ex15_i2c.c`.

1. **I2C1** (SCL = PB8, SDA = PB9, AF4) runs at 100 kHz. The pins must be **open-drain**: I²C devices only ever pull the lines low, and resistors pull them high. The firmware scans addresses 0x08–0x77 once a second and prints the ones that answer. Run it with **no device connected** first.

   The STM32F4's I²C peripheral is driven step by step through status flags. To probe one address:
   ```c
   I2C1->CR1 |= I2C_CR1_START;               /* 1. send START...          */
   wait for SR1.SB;                          /*    ...and wait until sent */
   I2C1->DR = addr << 1;                     /* 2. send address + write   */
   wait for SR1.ADDR (ACK) or SR1.AF (NACK); /* 3. did anyone answer?     */
   if ACK: read SR1 then SR2;                /*    this clears ADDR       */
   I2C1->CR1 |= I2C_CR1_STOP;                /* 4. send STOP              */
   ```
   The timing comes from the APB1 clock: `CR2 FREQ = 45` (MHz), `CCR = 45 MHz / (2 × 100 kHz) = 225`, `TRISE = 46`.
2. LA: CH0 = SCL, CH1 = SDA, with the **I²C decoder**. DBG0 is high during each scan, so you can trigger on it. Find the address byte and the **NACK**.
3. **Pull-ups:** by default the firmware turns on the STM32's internal pull-ups (about 40 kΩ). Look at SDA's rising edges on the scope. They are slow, RC-shaped ramps, because the lines are open-drain. Now add external **4.7 kΩ** pull-ups to 3.3 V, rebuild with `INTERNAL_PULLUPS=0`, and compare.
4. Fault injection: rebuild with `-DI2C_FAST` for **400 kHz**, use a weak pull-up (47 kΩ) and/or add **1 nF** from SDA to GND. On the scope measure the rise time. Raise it until the LA decoder (or the MCU itself) starts reporting errors. The I²C spec allows a maximum 1000 ns rise time at 100 kHz and 300 ns at 400 kHz. Try it with *no* pull-ups at all: the lines float, and the firmware may report the bus as busy or every probe as a timeout. Why?
5. If you have any I²C sensor, connect it and read one of its registers, for example a BME280's ID register:
   ```sh
   make flash EX=15 EXTRA_CFLAGS="-DSENSOR_ADDR=0x76 -DSENSOR_REG=0xD0"
   ```
   Decode it on the LA: address + W, register, repeated START, address + R, data, NACK, STOP. Then find in `i2c1_read_reg()` the code that makes each part happen.

---

## Capstone ideas

- **Mini oscilloscope:** sample PA3 with the ADC at 100 kS/s via DMA, stream the data over UART, and plot it on the PC (e.g. Python + matplotlib). Validate it against the Hantek with the same generator signal. Where does yours fall short (bandwidth, resolution, triggering)?
- **Frequency-response analyser:** the MCU's DAC sweeps a sine through an RC filter, the ADC reads the output, and the MCU prints the gain in dB against frequency. Check it against your hand-measured Bode plot from Exercise 10.
- **Mini logic analyzer:** sample a GPIO port with DMA triggered by a timer, and compare your captures with the LA1010's.

---

## Logbook template

For each exercise write down:

| Item | Your entry |
|---|---|
| Setup (generator settings, wiring, firmware change) | |
| Prediction (what you expect, with numbers) | |
| Scope measurement | |
| LA measurement | |
| MCU's own measurement / printout (if any) | |
| Did they agree? If not, why? | |
| Screenshot file name | |

Making a prediction **before** you measure is the most valuable habit you can build. Most of the learning happens when an instrument disagrees with you.
