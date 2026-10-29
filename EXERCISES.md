# Exercises: Hantek scope + signal generator + Innomaker LA1010 + Nucleo-F429ZI

These exercises use four tools together:

| Tool | Role |
|---|---|
| **Oscilloscope** (Hantek DSO2D15-class, 2 channels) | Shows the *analog* truth: voltage over time, edge shapes, noise. |
| **Signal generator** (the scope's built-in AWG / "Gen Out" BNC) | A known, adjustable *stimulus* to feed into the board. |
| **Logic analyzer** (Innomaker LA1010, 16 channels, KingstVIS) | Shows the *digital interpretation* across many pins at once and decodes protocols. Its two PWM outputs act as an extra signal source. |
| **Nucleo-F429ZI** (STM32F429ZI, Cortex-M4F @ 180 MHz) | The device under test. Sometimes it *receives* the generator's signal, sometimes it *produces* signals, and often it does both. |

The idea running through all of them: **the generator is a reference you trust, the scope tells you what a signal is, the logic analyzer tells you what a digital circuit thinks it is, and the microcontroller is the thing you are trying to understand.** Most of what you learn comes from the places where these disagree.

---

## Part 0: Setup (do this once)

### 0.1 Safety rules for the microcontroller (read this!)

The STM32 pins are **3.3 V logic**. The signal generator can easily produce voltages that damage them: negative voltages, or more than 3.3 V.

1. **Before connecting the generator to the board, check its output on the scope with nothing else connected.** Confirm the minimum is ≥ 0 V and the maximum is ≤ 3.3 V. Many generators power up with a waveform centred on 0 V, which means a negative swing.
2. For a 0–3.3 V signal, set **amplitude 3.3 Vpp, offset +1.65 V**. If your generator has a *High-Z / 50 Ω* load setting, choose **High-Z**.
3. **Always put a 1 kΩ resistor in series** between the generator and any STM32 pin. If you make a mistake, it limits the current to a level the pin's protection diodes can survive.
4. Connect **one common ground**: board GND, scope probe ground, LA GND and the generator BNC shield.
5. Don't connect the generator to a pin the firmware has configured as an **output**. Two outputs fighting each other can damage a pin. The 1 kΩ resistor helps here too.

### 0.2 Software

- **STM32CubeIDE** (free, from ST). Create projects with **Board Selector → NUCLEO-F429ZI**. Accept "initialize peripherals to default mode": that sets up the clock at 180 MHz, the LEDs, the user button and USART3, which is the virtual COM port over the ST-LINK USB.
- **KingstVIS** for the LA1010.
- A serial terminal (PuTTY, screen, minicom, or the terminal built into CubeIDE) at **115200 baud** for `printf` output.

*Alternative:* the Arduino IDE with the **STM32duino** core supports this board. The ideas carry over, but the snippets below use the ST HAL.

### 0.3 Pin plan

The Nucleo-144 board shares some pins with Ethernet, the ST-LINK, the LEDs and the button. This plan avoids those conflicts. **Double-check each pin against the board's pin labels in CubeMX or the board user manual (UM1974)** before wiring.

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

### 0.4 Two helpers you'll use everywhere

**Debug pins.** Toggling a GPIO is the fastest, least intrusive way to make firmware events visible on the LA or the scope. Writing to the BSRR register takes a single store instruction.

```c
#define DBG0_HI()  (GPIOG->BSRR = GPIO_PIN_2)
#define DBG0_LO()  (GPIOG->BSRR = (uint32_t)GPIO_PIN_2 << 16)
#define DBG1_HI()  (GPIOG->BSRR = GPIO_PIN_3)
#define DBG1_LO()  (GPIOG->BSRR = (uint32_t)GPIO_PIN_3 << 16)
```

Configure PG2/PG3 in CubeMX as `GPIO_Output`, **speed: Very High**.

**Cycle counter.** The DWT unit counts CPU cycles; at 180 MHz one cycle is 5.56 ns.

```c
static void dwt_init(void) {
  CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
  DWT->CYCCNT = 0;
  DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
static inline void delay_cycles(uint32_t n) {
  uint32_t t0 = DWT->CYCCNT;
  while ((DWT->CYCCNT - t0) < n) {}
}
```

**printf over USB.** Add this to `main.c` so `printf` goes to the ST-LINK virtual COM port:

```c
int _write(int fd, char *p, int len) {
  HAL_UART_Transmit(&huart3, (uint8_t *)p, len, HAL_MAX_DELAY);
  return len;
}
```

---

## Level 1: Get the instruments and the board talking

### Exercise 1: Sanity check all three instruments
**Goal:** Trust your tools before you trust your measurements.

1. **Probe compensation:** connect each probe to the scope's 1 kHz probe-comp tab and adjust the trimmer until the square wave has square corners. Use **x10** on the probe *and* in the channel menu.
2. **Generator check:** set **square, 10 kHz, 0–3.3 V** (3.3 Vpp, 1.65 V offset). Turn on the scope measurements: Freq, Vmax, Vmin, +Duty. Confirm the signal really is 0–3.3 V. This is the setting you'll use with the board.
3. **First LA capture:** connect the same signal to LA CH0. In KingstVIS set **10 MHz** sample rate and the threshold to **1.65 V** (or the 3.3 V CMOS preset). Capture, then read the frequency and duty cycle.

**Check:** The scope and the LA both report 10 kHz / 50%. The LA's resolution is 1 / sample rate (100 ns here). Is the difference between them smaller than that?

### Exercise 2: From Blinky to "how fast can a pin toggle?"
**Goal:** See how much software overhead costs, and see aliasing in the LA.

1. **Step A:** blink LD1 (PB0) with `HAL_GPIO_TogglePin` + `HAL_Delay(100)`. Probe PB0 on the scope and LA. Is it exactly 5 Hz? (`HAL_Delay` adds an extra tick, so look carefully.)
2. **Step B:** remove the delay and toggle DBG0 as fast as possible:
   ```c
   while (1) { HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_2); }
   ```
   Measure the frequency on the scope.
3. **Step C:** replace that with direct register writes:
   ```c
   while (1) { DBG0_HI(); DBG0_LO(); }
   ```
   Measure again. It should be many times faster. The pulse may also be *asymmetric*, because the loop branch adds time to only one half. Measure +width and −width on the scope.
4. **Aliasing:** capture the Step C signal on the LA at 100, 50, 20, 10 and 5 MHz. Write down what frequency and duty cycle the LA *claims* at each rate.

**Takeaway:** Sample a digital signal at **at least 4×, ideally 10×** its fastest frequency. When the LA is too slow it doesn't warn you; it just shows the wrong answer. The scope, at hundreds of MS/s, shows the truth.

### Exercise 3: GPIO speed settings and edge rates
**Goal:** Learn what the "GPIO speed" setting in CubeMX actually does.

1. Use the Step C toggle from Exercise 2, but slow it down with `delay_cycles(90)` in each half so you get a clean square wave of roughly 1 MHz.
2. On the scope, measure **rise time** and **fall time** of DBG0 with the GPIO speed set to **Low**, then **Medium**, **High** and **Very High** (rebuild each time).
3. For the fastest settings, replace the probe's long ground clip with the short **ground spring**, and see how much the ringing changes.

**Check:** Fast edges look great but cause ringing and EMI. Slow edges are cleaner but limit the maximum frequency. That's why the setting exists. Also note: your scope's bandwidth (150 MHz → about 2.3 ns rise time) sets a lower limit on what it can show.

### Exercise 4: Look at the clock tree with MCO
**Goal:** Verify the microcontroller's clock with an instrument instead of trusting the configuration.

1. In CubeMX enable **RCC → MCO2** (PC9) with source **SYSCLK** and prescaler **/5**, giving 36 MHz:
   ```c
   HAL_RCC_MCOConfig(RCC_MCO2, RCC_MCO2SOURCE_SYSCLK, RCC_MCODIV_5);
   ```
2. Measure it on the scope's frequency counter. Then capture it on the LA at 100 MHz, and work out why the LA's reading isn't trustworthy (hint: Exercise 2).
3. Enable **MCO1** (PA8) with source **HSE**, giving 8 MHz. On this board HSE comes from the ST-LINK's crystal. Then switch it to **HSI** (the internal RC oscillator, 16 MHz).
4. Write down how far each one is from its nominal value, **in ppm** (parts per million): (measured − nominal) / nominal × 10⁶.

**Takeaway:** A crystal is typically within ±50 ppm. HSI is specified to about ±1% (10 000 ppm). You'll see in Exercise 13 why that matters for UART.

### Exercise 5: Profiling code with debug pins
**Goal:** Measure exactly how long a piece of code takes.

1. Bracket some code with a debug pin:
   ```c
   volatile float x = 2.0f, y;
   DBG0_HI();
   for (int i = 0; i < 100; i++) y = sqrtf(x + i);
   DBG0_LO();
   ```
2. Measure the pulse width with the LA. Repeat at least 10 times and look at the variation.
3. Compare: `sqrtf` (single precision, uses the FPU) vs `sqrt` (double precision; the M4's FPU is single precision only, so this runs in software). Also compare optimization levels `-O0` and `-O2`.
4. Use DBG1 to mark a second region (e.g. a `printf`) and measure both on the same capture.

**Check:** Cross-check one measurement against `DWT->CYCCNT` printed over UART. Do the cycle count and the LA pulse width agree (cycles × 5.56 ns)?

---

## Level 2: The microcontroller *listens* to the generator

### Exercise 6: Measure the STM32's input thresholds and hysteresis
**Goal:** Find out at what voltage the chip decides "0" or "1".

1. Configure **PG0 as an input** (no pull). The firmware copies it to DBG0 as fast as possible:
   ```c
   while (1) {
     if (GPIOG->IDR & GPIO_PIN_0) DBG0_HI(); else DBG0_LO();
   }
   ```
2. Generator: **triangle, 100 Hz, 0–3.3 V** → 1 kΩ → PG0.
3. Scope CH1 = PG0 (the input), CH2 = DBG0 (the output). Trigger on CH2's rising edge.
4. With a horizontal cursor, read the input voltage at the instant the output goes **high**, then the voltage when it goes **low**. Their difference is the **hysteresis**.
5. Compare with the datasheet's V_IH / V_IL. Then set the LA threshold to the same values and compare the LA's view of PG0 with the chip's view (DBG0).

**Extension:** Add noise to the triangle (use the generator's noise or arbitrary mode if it has one, or a long unshielded wire you touch). Without hysteresis there would be many false edges. Does DBG0 stay clean? Does the LA's view of PG0 stay clean?

### Exercise 7: Interrupt latency and jitter
**Goal:** Measure how quickly the CPU can respond to an external event.

1. Configure **PG0 as EXTI, rising edge**, interrupt enabled in the NVIC. In the callback:
   ```c
   void HAL_GPIO_EXTI_Callback(uint16_t pin) {
     if (pin == GPIO_PIN_0) { DBG0_HI(); DBG0_LO(); }
   }
   ```
2. Generator: **square, 1 kHz, 0–3.3 V** → 1 kΩ → PG0.
3. LA: CH0 = PG0, CH1 = DBG0, 100 MHz. Measure the delay from the input edge to the DBG0 pulse. Collect at least 20 samples and note the minimum, maximum and spread (**jitter**).
4. Now write the handler yourself: put `EXTI->PR = EXTI_PR_PR0; DBG0_HI(); DBG0_LO();` directly in `EXTI0_IRQHandler` and skip the HAL. How much faster is it?
5. Increase the generator frequency (10 kHz, 100 kHz, 500 kHz, 1 MHz…). Find where the LA shows **missed pulses** on DBG0.
6. Add a busy loop (`delay_cycles(1800)` = 10 µs) to the main loop with interrupts disabled around it (`__disable_irq()` / `__enable_irq()`), and watch the worst-case latency grow.

**Takeaway:** The hardware minimum is about 12 cycles (≈ 67 ns), but real latency depends on the software around it. The LA is the only way to *see* the worst case.

### Exercise 8: Build a hardware frequency and duty-cycle meter (input capture)
**Goal:** Let a timer measure the signal in hardware, and compare three instruments.

1. Configure **TIM2** in **PWM Input** mode on CH1 (PA0): CH1 captures on the rising edge (period), CH2 captures on the falling edge of the same input (high time), and the slave mode resets the counter on TI1FP1. The timer clock is 90 MHz, so the resolution is 11.1 ns.
2. Read and print:
   ```c
   uint32_t period = HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_1) + 1;
   uint32_t high   = HAL_TIM_ReadCapturedValue(&htim2, TIM_CHANNEL_2) + 1;
   printf("f=%.3f Hz duty=%.2f%%\r\n", 90e6 / period, 100.0 * high / period);
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

1. Configure **TIM3** with TRGO = Update at **10 kHz**. **ADC1 IN3 (PA3)**: external trigger *Timer 3 Trigger Out*, conversion-complete interrupt on, sampling time 56 cycles or more. **DAC channel 1 (PA4)** with the output buffer on.
2. In the callback, copy each sample straight to the DAC and mark it with a debug pin:
   ```c
   void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef *h) {
     DBG0_HI();
     HAL_DAC_SetValue(&hdac, DAC_CHANNEL_1, DAC_ALIGN_12B_R, HAL_ADC_GetValue(h));
     DBG0_LO();
   }
   ```
3. Generator: **sine, 0.3–3.0 V** (2.7 Vpp, 1.65 V offset) → 1 kΩ → PA3. Scope CH1 = generator, CH2 = DAC output (PA4).
4. Sweep the generator: 100 Hz, 1 kHz, 4 kHz, 5 kHz, **9 kHz, 9.9 kHz, 11 kHz**.

**What you should see:** At 100 Hz the output is a staircase copy of the input. At 9 kHz the DAC output looks like a clean **1 kHz** sine. That's aliasing: a frequency that doesn't exist in the input. At 9.9 kHz you get 100 Hz. At exactly 5 kHz (Nyquist) the result depends on the phase.

5. LA: CH0 = DBG0. Confirm the sample rate really is 10 kHz, and measure how much of each 100 µs sample period the callback uses.
6. Change the ADC sampling time and the input resistor (1 kΩ → 47 kΩ). What happens to accuracy? (The sampling capacitor needs time to charge through the source resistance.)

---

## Level 3: The microcontroller *talks*

### Exercise 10: DAC waveform generator + RC reconstruction filter
**Goal:** Build a simple signal generator and compare it with the real one.

1. Make a 64-point sine look-up table (`uint16_t lut[64]`, values 0–4095). Configure **TIM6** TRGO = Update and **DAC1** trigger = *Timer 6 Trigger Out*, with DMA in **circular** mode, half-word:
   ```c
   HAL_TIM_Base_Start(&htim6);
   HAL_DAC_Start_DMA(&hdac, DAC_CHANNEL_1, (uint32_t *)lut, 64, DAC_ALIGN_12B_R);
   ```
   With 64 points and TIM6 at 64 kHz you get a 1 kHz sine. Zero CPU time is used once it's running.
2. Scope CH1 = PA4 (the MCU sine), CH2 = the Hantek generator's 1 kHz sine. Zoom in on the MCU's **staircase**.
3. Add an **RC low-pass filter** on PA4: 1 kΩ in series, then 100 nF to GND (cutoff ≈ 1.6 kHz). Do the steps disappear? What happens to the amplitude and phase? Try 10 nF (cutoff ≈ 16 kHz) instead.
4. Use the scope's **FFT** (if available) on both signals and compare the harmonics with the Hantek AWG's.

**Extension (Bode plot):** Run the *Hantek* generator's sine (0–3.3 V) through the same RC filter. Sweep 100 Hz → 100 kHz and measure gain (Vout/Vin) and phase (Δt/T × 360°) on the scope. At the cutoff you should see 0.707× and −45°. Plot gain in dB against log(frequency).

### Exercise 11: Complementary PWM with dead time
**Goal:** Measure a timing detail that matters in motor drivers and power supplies.

1. Configure **TIM1 CH1 + CH1N** (PE9 / PE8) for PWM at **100 kHz, 30% duty**. The TIM1 clock is 180 MHz, so use ARR = 1799 and CCR1 = 540.
2. Set **dead time** (DTG) to 0, then 18, then 90. In this range one DTG step is 1 / 180 MHz = 5.56 ns.
3. Start both outputs:
   ```c
   HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
   HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
   ```
4. LA CH0 = CH1, CH1 = CH1N at 100 MHz: measure the gap where **both are low**. Then measure the same gap on the scope (CH1 and CH2) with cursors.

**Check:** With DTG = 18 you expect 100 ns. The LA can only resolve 10 ns steps; the scope can do better. Which one would you trust for a 20 ns dead time?

### Exercise 12: Glitch hunting with triggers
**Goal:** Catch a rare, wrong event, which is what most real debugging looks like.

1. The firmware makes a stream of 10 µs pulses, but every 1000th pulse is only 1 µs long:
   ```c
   for (uint32_t n = 0;; n++) {
     DBG0_HI();
     delay_cycles((n % 1000 == 999) ? 180 : 1800);
     DBG0_LO();
     delay_cycles(1800);
   }
   ```
2. On the scope, just looking at the signal, the glitch is nearly invisible. Set up a **pulse-width trigger**: positive pulse **< 5 µs**. Now the scope shows only the glitch.
3. Do the same in KingstVIS with a **pulse-width trigger** on CH0. Use DBG1 to mark when `n % 1000 == 0`, and check how many pulses lie between the marker and the glitch.
4. Make it harder: change the glitch to once every 100 000 pulses, or shorten it to 100 ns. Can both instruments still catch it?

---

## Level 4: Protocols (the payoff)

### Exercise 13: UART: decoding, baud-rate error and clock accuracy
1. Configure **USART6** (TX = PG14) at **115200 8N1**. Send `"Hello\r\n"` every 100 ms.
2. LA CH0 = PG14 with KingstVIS's **Async Serial** decoder at 115200 8N1. You should see the text decoded above the waveform.
3. Scope: measure one bit width (expected 1 / 115200 = **8.68 µs**). Check the levels and the idle state (high).
4. Set the decoder to the **wrong baud rate** (57600, 230400) and look at the garbage, including framing errors.
5. **Clock experiment:** switch the system clock source from HSE (crystal) to **HSI**, then use the HSI trim bits (`__HAL_RCC_HSI_CALIBRATIONVALUE_ADJUST`) to deliberately make it 2–3% off. Measure the actual bit width with the LA. At what error does the decoder (or a real receiver) start failing? UART typically tolerates about ±2–3% in total.

### Exercise 14: SPI: clock modes and loopback
1. Configure **SPI4** as master: SCK = PE2, MOSI = PE6, MISO = PE5, a software NSS on PE4. Connect **MOSI to MISO** with a jumper (loopback). Send `0xA5, 0x3C, 0x00, 0xFF` repeatedly at about 1 MHz.
2. LA: CH0 = SCK, CH1 = MOSI, CH2 = MISO, CH3 = NSS. Add an **SPI decoder**. Check that the MISO bytes match the MOSI bytes.
3. Change the firmware's **CPOL/CPHA** through all four modes. Keep the decoder on mode 0 and see which modes decode wrongly. Then fix the decoder settings.
4. Increase the SPI clock with the prescaler (up to 45 MHz). Find where the **LA** stops decoding correctly (hint: sample rate), and where the **scope** shows that the SCK edges are no longer square.

### Exercise 15: I²C: ACK/NACK and pull-up resistors
1. Configure **I2C1** (SCL = PB8, SDA = PB9) at 100 kHz. Scan addresses 0x08–0x77 with `HAL_I2C_IsDeviceReady()` and print the results. Run it with **no device connected** first.
2. LA: CH0 = SCL, CH1 = SDA, with the **I²C decoder**. Find the address byte and the **NACK**.
3. **Pull-ups:** with only the STM32's internal pull-ups (about 40 kΩ), look at SDA's rising edges on the scope. They are slow, RC-shaped ramps, because I²C is open-drain. Now add external **4.7 kΩ** pull-ups to 3.3 V and compare.
4. Fault injection: change to **400 kHz**, use a weak pull-up (47 kΩ) and/or add **1 nF** from SDA to GND. On the scope measure the rise time. Raise it until the LA decoder (or the MCU itself) starts reporting errors. The I²C spec allows a maximum 1000 ns rise time at 100 kHz and 300 ns at 400 kHz.
5. If you have any I²C sensor, connect it and decode a real register read: address + W, register, repeated START, address + R, data.

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
