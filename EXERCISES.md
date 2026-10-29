# Exercises: Hantek scope + signal generator + Innomaker LA1010

These exercises use three tools together:

- **Oscilloscope** (Hantek DSO2D15-class, 2 channels): shows the *analog* truth, meaning voltage over time.
- **Signal generator** (the scope's built-in AWG/"Gen Out" BNC): the stimulus.
- **Logic analyzer** (Innomaker LA1010, 16 channels, KingstVIS software): shows the *digital interpretation*, meaning 1 or 0 relative to a threshold. It also has **two PWM outputs**, which give you a second signal source.

The theme running through all of them: **the scope tells you what the signal is, and the logic analyzer tells you what a digital circuit would think it is.** The interesting lessons are in the places where those two disagree.

---

## Before you start

### Safety and good habits

1. **Common ground.** Connect the scope probe ground clip, the LA GND wire and the generator BNC shield together. If the scope and the LA plug into the same PC or the same power strip they already share ground through USB/mains earth. Never clip a scope ground to anything that isn't ground.
2. **Keep logic signals in logic range.** For "digital" exercises set the generator to **3.3 Vpp amplitude with a 1.65 V offset** (so it swings 0 V to 3.3 V). The LA1010 tolerates far more than that, but any chips you add later won't.
3. **Generator load setting.** If the generator menu offers a *High-Z* or *50 Ω* load setting, pick **High-Z** (you are driving probes, not a 50 Ω terminator). Otherwise the amplitude you see will be about 2× what you set.
4. **Probe x1 vs x10.** Use **x10** on the probe *and* set the channel to x10 in the scope menu. A mismatch gives readings that are off by 10×.

### Suggested hookup (used by most exercises)

```
Gen Out BNC ──► BNC-to-clip cable (or BNC tee) ──┬──► Scope CH1 probe tip
                                                  ├──► LA1010 CH0
                                                  └──► (circuit under test, when used)
GND ──────────────────────────────────────────────┴──► Scope probe ground, LA GND
```

A breadboard makes this easy: put the generator signal on one row and ground on the rail, then plug the probe and LA leads in.

### Useful parts (optional; many exercises need none)

- Resistors: 1 kΩ, 10 kΩ
- Capacitors: 1 nF, 10 nF, 100 nF
- One 74HC14 (Schmitt-trigger inverter) and one 74HC74 (dual D flip-flop), or a 74HC393 counter
- Any Arduino/ESP32/Raspberry Pi Pico for the protocol exercises

---

## Level 1: Get to know each instrument

### Exercise 1: Probe compensation (scope only)
**Goal:** Make sure your probes don't distort what you see.

1. Connect the probe to the scope's **probe-comp** terminal (the 1 kHz square-wave tab).
2. Auto-set. You should see a square wave.
3. Adjust the small trimmer on the probe body until the corners are square: no overshoot "ears" and no rounded corners.
4. Repeat for the second probe.

**Check:** Does the flat top look flat? Write down the frequency and amplitude the scope measures. They should be about 1 kHz and about 2–3 V; check your manual.

### Exercise 2: Your first generated signal
**Goal:** Learn the generator controls and confirm them with scope measurements.

1. Generator: **sine, 1 kHz, 2 Vpp, 0 V offset.** Feed it to CH1.
2. Turn on the scope's automatic measurements: Frequency, Period, Vpp, Vmax, Vmin, Vrms.
3. Predict before you look: Vrms of a 2 Vpp sine = 1 / √2 ≈ **0.707 V**.
4. Change the waveform to **square** and then **triangle**, and predict Vrms each time (square: 1.0 V; triangle: 1/√3 ≈ 0.577 V).
5. Add a **+1 V offset**. Watch the trace move up. Switch the channel between **DC and AC coupling** and notice that AC coupling hides the offset.

**Check:** Do the measured values match your predictions within a few percent?

### Exercise 3: First logic analyzer capture
**Goal:** Learn KingstVIS and compare it with the scope.

1. Generator: **square, 10 kHz, 0–3.3 V** (3.3 Vpp, 1.65 V offset).
2. Feed it to scope CH1 **and** LA CH0 at the same time.
3. In KingstVIS set the sample rate to **10 MHz**, the capture length to about 10 ms, and the threshold voltage to **1.65 V** (or the 3.3 V CMOS preset).
4. Capture. Use the built-in measurements (hover or the measurement panel) to read the frequency, period and duty cycle.
5. Compare with the scope's Freq and +Duty measurements.

**Check:** Both should say 10 kHz and 50%. Write down any difference. The LA's timing resolution is 1 / sample rate = 100 ns at 10 MHz. Is the difference smaller than that?

---

## Level 2: Where analog and digital disagree

### Exercise 4: Aliasing (Nyquist in real life)
**Goal:** See what happens when the logic analyzer samples too slowly.

1. Generator: **square, 1 MHz, 0–3.3 V** to the scope and LA CH0.
2. Capture on the LA at 50 MHz, then 10 MHz, 4 MHz, 2 MHz, 1 MHz and 500 kHz.
3. At each rate write down the frequency and duty cycle the LA *claims* to see.

**What you should see:** At high rates the capture is clean. As you approach 2× the signal frequency, pulse widths start to jitter. Below that the LA shows a completely wrong (aliased) frequency, or a flat line. The scope, sampling at hundreds of MS/s, still shows the truth.

**Rule of thumb to take away:** sample a digital signal at **at least 4×, ideally 10×** its fastest frequency. Bonus: try the same thing on the scope by forcing a very slow timebase. The scope can alias too.

### Exercise 5: Threshold changes the duty cycle
**Goal:** Understand that "digital" means "above or below a threshold."

1. Generator: **sine, 1 kHz, 0–3.3 V** (3.3 Vpp, 1.65 V offset). Feed it to the scope and LA.
2. LA threshold at **1.65 V**: measure the duty cycle (about 50%).
3. Change the LA threshold to about **0.8 V**, then about **2.5 V** (or the closest values KingstVIS lets you set). Capture again each time.
4. On the scope turn on cursors, put a horizontal cursor at each threshold voltage, and measure with vertical cursors how long the sine stays above that line.

**Check:** Does the LA's duty cycle match the scope cursor measurement for each threshold? Try a **triangle** wave as well. Its duty cycle changes *linearly* with the threshold, and the sine's doesn't. Can you work out why?

### Exercise 6: Slow edges + noise = false edges
**Goal:** See why real digital inputs use Schmitt triggers (hysteresis).

1. Generator: **triangle, 10 Hz, 0–3.3 V**. That gives very slow edges.
2. Make the signal noisy. Either:
   - use the AWG's noise or arbitrary mode if it has one, or
   - put a long unshielded wire in the signal path, touch it, or run it next to a phone charger or a fluorescent light.
3. Capture on the LA at about 1 MHz with a fine zoom around each threshold crossing.

**What you should see:** Near each crossing the LA shows a burst of very short pulses (chatter), even though the scope shows a single slow ramp with a little fuzz on it.

**Extension (needs a 74HC14):** Feed the triangle into the 74HC14, connect the output to LA CH1, and keep the raw signal on CH0. The chatter disappears on CH1. On the scope, put CH1 on the input and CH2 on the output, and measure the two input voltages where the output switches. Their difference is the **hysteresis**.

### Exercise 7: Glitch hunting
**Goal:** Use triggers to catch a rare event.

1. Use the generator's **pulse** waveform (or square with an adjustable duty cycle): **1 kHz, duty cycle 1%** (about 10 µs pulses).
2. On the scope, set up a **pulse-width trigger**: trigger on positive pulses **< 20 µs**. Make sure it triggers reliably.
3. Change the duty cycle to 50%. The scope stops triggering, because there are no narrow pulses any more.
4. Do the same on the LA: set a **pulse-width trigger** on CH0 in KingstVIS.

**Takeaway:** In real debugging you look for the one bad pulse in a million good ones. Both instruments can wait for it, but only if you describe it to them.

---

## Level 3: Add a simple circuit

### Exercise 8: RC low-pass filter: time constant and propagation delay
**Circuit:** Generator → **1 kΩ** → node *Vout* → **100 nF** → GND. That gives τ = RC = 100 µs.

1. Generator: **square, 500 Hz, 0–3.3 V** (a long enough period for the capacitor to charge fully).
2. Scope CH1 = generator, CH2 = Vout. You'll see the classic exponential charge and discharge curve.
3. With cursors, measure the time for Vout to reach **63%** of the step (≈ τ = 100 µs).
4. Connect **LA CH0 = generator, LA CH1 = Vout**, threshold 1.65 V. Measure the delay between the edges on CH0 and CH1.

**Predict:** The output crosses 50% at t = τ · ln 2 ≈ **69 µs**. The LA measures exactly this "propagation delay." That's how a slow node in a real circuit turns into timing errors.

5. Raise the frequency to 5 kHz and then 20 kHz. On the scope the output turns into a small triangle around 1.65 V. Look at what the LA does. At some point it shows erratic switching or none at all. Explain why.

### Exercise 9: Phase shift and Lissajous figures
**Circuit:** The same RC filter as Exercise 8, but change it to **1 kΩ + 10 nF** (cutoff f_c = 1 / (2πRC) ≈ **15.9 kHz**).

1. Generator: **sine, 2 Vpp**. Scope CH1 = input, CH2 = output.
2. Sweep the frequency: 1 kHz, 5 kHz, 15.9 kHz, 50 kHz, 160 kHz. At each step write down:
   - Vout / Vin (gain)
   - Phase shift, measured with cursors: Δt / T × 360°
3. At f_c you should measure gain ≈ **0.707** (−3 dB) and phase ≈ **−45°**.
4. Switch the scope to **XY mode**. The ellipse shows the phase: a diagonal line means 0°, a circle means 90°.
5. Plot gain (dB) against log(frequency) on paper or in a spreadsheet. You've just measured a **Bode plot** by hand.

### Exercise 10: Divide-by-2 and counters (digital chips)
**Needs:** a 74HC74 wired as a toggle flip-flop (Q̅ → D), or a 74HC393 counter, powered at 3.3 V or 5 V. Set the generator's high level to match the supply.

1. Generator: **square, 100 kHz** → clock input.
2. LA: CH0 = clock, CH1..CH4 = Q outputs (Q0..Q3 for the '393).
3. Capture. You should see 100 kHz, 50 kHz, 25 kHz, 12.5 kHz, 6.25 kHz: binary counting.
4. In KingstVIS, group CH1–CH4 into a **bus** to see the count as a hex value.
5. On the scope, put CH1 = clock and CH2 = Q0, zoom in on a rising clock edge, and measure the **clock-to-Q propagation delay** (a few ns to tens of ns). The LA at 100 MS/s has 10 ns resolution, so it can barely see this delay and the scope can. Compare them.

---

## Level 4: Use the LA1010's PWM outputs as a second generator

The LA1010 has two PWM outputs (configured in KingstVIS). That gives you a second, independent signal source.

### Exercise 11: Verify one instrument with another
1. In KingstVIS set PWM1 to **1 kHz, 25% duty**.
2. Measure it with the scope (Freq, +Duty, rise time) **and** loop it back into LA CH0.
3. Try 1 MHz and 10 MHz. Watch on the scope how the "square" wave gets rounder at high frequency: the edges stay the same length, but the period shrinks.

**Takeaway:** Every source and every instrument has limits. Measuring one with another is how you find them.

### Exercise 12: Two unsynchronized clocks (beat frequency and metastability setup)
1. Scope generator: **square, 1.000 kHz**. LA PWM1: **square, 1.010 kHz**.
2. Scope CH1 = generator, CH2 = PWM1. Trigger on CH1. CH2 slowly drifts across the screen, at the beat frequency of 10 Hz.
3. On the LA, capture both. Use the LA's measurement tools to find the phase relationship at the start and at the end of a 1-second capture.
4. **Extension:** Feed the generator into a 74HC74 D input and PWM1 into its clock. The Q output now samples one clock with another. That's the basic clock-domain-crossing problem. On the LA, look for the moments where Q's behavior becomes unpredictable.

### Exercise 13: Mixed triggering (one instrument triggers the other)
1. Generator: **burst** mode (if available) or a low-duty pulse, about 10 pulses every 10 ms.
2. LA: trigger on the first rising edge after a long low period (pulse-width trigger, low > 5 ms).
3. Scope: trigger on the same thing with a pulse-width or **holdoff** setting, so it locks onto the start of each burst instead of a random pulse.

**Takeaway:** Holdoff and pulse-width triggers are how you get a stable picture of complex, repeating patterns.

---

## Level 5: Protocols (needs a microcontroller)

The generator alone can't easily make serial data. These exercises use any Arduino/ESP32/Pico running a tiny sketch, and they're the payoff for everything above.

### Exercise 14: UART decode + analog check
1. The microcontroller sends `"Hello\n"` every 100 ms at **9600 baud** on its TX pin.
2. LA CH0 = TX. In KingstVIS add a **UART/Async Serial** decoder at 9600 8N1. You should see `H e l l o \n` decoded above the waveform.
3. Scope CH1 = TX. Measure one bit width (should be 1 / 9600 ≈ **104 µs**). Check the levels and the edge quality.
4. Deliberately set the LA decoder to the **wrong baud rate** (e.g. 4800, 19200) and look at the garbage, including the framing errors. That's what a baud-rate mismatch looks like in the field.

### Exercise 15: I²C or SPI with an injected fault
1. Connect any I²C sensor (or just try to talk to a nonexistent address). LA CH0 = SCL, CH1 = SDA, I²C decoder on.
2. Find the address byte and the ACK/NACK bit in the decode.
3. On the scope, look at SDA's **rising edges**. They are slow, RC-shaped ramps, because I²C uses pull-up resistors (open-drain).
4. **Fault injection:** Swap in a weaker pull-up (e.g. 47 kΩ) or add 1 nF from SDA to GND. On the scope the rising edges get slower. On the LA, keep raising the bus speed (100 kHz → 400 kHz) until the decoder starts reporting errors. This ties directly back to Exercises 5, 6 and 8.

---

## Logbook template

For each exercise write down:

| Item | Your entry |
|---|---|
| Setup (generator settings, wiring) | |
| Prediction (what you expect to see, with numbers) | |
| Scope measurement | |
| LA measurement | |
| Did they agree? If not, why? | |
| Screenshot file name | |

Making a prediction **before** you measure is the most valuable habit you can build. Most of the learning happens when the instrument disagrees with you.
