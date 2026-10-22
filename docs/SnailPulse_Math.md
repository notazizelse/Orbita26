# SnailPulse — all the math

Every number on the schematic and in the firmware, with a derivation you can redo on paper.
Component values are from `hardware/kicad/Orbita26.kicad_sch` rev A. Reference designators are the schematic's, not the workbook's.

---

## 1. Heartbeat sensor (block A)

### 1.1 IR LED current (R1)
The TCRT5000 LED needs a series resistor:

  I_F = (V_supply − V_F) / R1 = (5 V − 1.25 V) / 220 Ω = **17 mA**

- Datasheet maximum I_F = 60 mA, so we run at about 30 % of the limit.
- Power in R1: I²R = 0.017² · 220 = **64 mW** (a 0.25 W resistor is fine).
- Power in the LED: V_F · I = 1.25 · 0.017 ≈ 21 mW. That is little self-heating next to the snail.

Why 5 V and not 3.3 V? (3.3 − 1.25)/220 = 9 mA. That would mean less light through the shell and a weaker signal.

### 1.2 Phototransistor + RV3 (light → voltage)
The phototransistor is wired as an **emitter follower**: collector to 3.3 V, emitter to RV3 (a 50 k rheostat) to GND.

  V_E = I_E · R_RV3     (I_E ≈ I_C ∝ reflected light)

- V_E can never exceed 3.3 V − V_CE(sat) ≈ 2.9 V, so the next stage is protected.
- RV3 sets the **sensitivity**. Example: 50 µA of photocurrent through 20 kΩ = 1.0 V. We tune it so the resting DC sits at about 0.8–2.0 V, mid-range, so the heartbeat ripple has room both ways.
- The heartbeat is only a **tiny change** (≈ 0.1–1 %) on top of this DC level. The heart's movement changes how much IR is reflected or absorbed.

### 1.3 High-pass filter (C11, R15) — removes DC
C11 in series, R15 to VREF:

  f_c = 1 / (2π · R15 · C11) = 1 / (2π · 1 MΩ · 1 µF) = **0.159 Hz**   (= 9.5 beats/min)
  τ = R15 · C11 = **1 s**  → settles after ≈ 5τ = 5 s

Gain of a first-order high-pass at frequency f: |H| = (f/f_c) / √(1 + (f/f_c)²)

| Heart rate | f (Hz) | passes | dB |
|---|---|---|---|
| 4 bpm | 0.067 | 39 % | −8.2 |
| 6 bpm | 0.10 | 53 % | −5.5 |
| 9.5 bpm | 0.159 | 71 % | −3.0 |
| 20 bpm | 0.33 | 90 % | −0.9 |
| 30 bpm | 0.5 | 95 % | −0.4 |
| 60 bpm | 1.0 | 99 % | −0.1 |

Why it is there: the DC from the sensor (≈ 1 V) is ~100–1000× bigger than the pulse. If we amplified it by 101 the op-amp would just saturate at the rail.
Its other end goes to **VREF (1.65 V)**, not GND, so the signal is re-centred in the middle of the 0–3.3 V range and can swing both ways.

### 1.4 Amplifier (U2A, R16, R17, C2) — gain and low-pass
This is a non-inverting amplifier whose "ground" is VREF:

  G = 1 + R17 / R16 = 1 + 100 k / 1 k = **101**  →  20·log₁₀(101) = **40.1 dB**

C2 is in parallel with R17, so the feedback impedance falls at higher frequency:

  f_LP = 1 / (2π · R17 · C2) = 1 / (2π · 100 kΩ · 470 nF) = **3.39 Hz**   (= 203 bpm)

  G(f) = 1 + |Z_f| / R16, with |Z_f| = R17 / √(1 + (2π f R17 C2)²)

| f | gain |
|---|---|
| 0.5 Hz | 99.9 |
| 1 Hz | 96.9 |
| 2 Hz | 87.1 |
| 3.39 Hz | 71.7 (= 101/√2, −3 dB) |
| 10 Hz | 33 |
| 50 Hz (mains) | 7.8 |

The gain never drops below **1**, because it is a non-inverting stage. That is why R3/C3 follows it.
**Pass band:** 0.16–3.4 Hz = **9.5–203 bpm** at −3 dB. Snail hearts beat at tens of bpm, and slower when cold or dormant.

### 1.5 Output offset (worth knowing)
At DC, C11 and C2 are open circuits. The output is then

  V_out,DC = VREF + G · V_os = 1.65 V + 101 · (±4.5 mV max) = **1.65 ± 0.45 V**

The MCP6002 input offset voltage V_os is at most ±4.5 mV (datasheet). So HB_ADC "rests" anywhere from about 1.2 to 2.1 V. That's fine: the firmware tracks the baseline and never assumes exactly 2048.
The bias current (1 pA typ.) through R15 (1 MΩ) is only 1 µV, which is negligible. That's why a CMOS op-amp fits a 1 MΩ resistor.

### 1.6 ADC filter (R3, C3)
  f_c = 1 / (2π · 1 kΩ · 100 nF) = **1.59 kHz**,  τ = 100 µs

- It removes noise above the band and limits current into the MCU pin if something goes wrong.
- C3 acts as a **charge reservoir**: when the ADC's sample-and-hold capacitor connects, C3 (100 nF, hundreds of times bigger) keeps the voltage stable.

### 1.7 ADC resolution
12-bit over 3.3 V: 1 LSB = 3.3 / 4096 = **0.806 mV**.
Referred back to the sensor (÷101): **8 µV per step**. That is the smallest change in the phototransistor voltage we can see.

---

## 2. Reference voltage (block B)

  VREF = 3.3 V · R7 / (R6 + R7) = 3.3 · 10 k / 20 k = **1.65 V** (the middle of the ADC range)

- Divider current: 3.3 V / 20 kΩ = 165 µA.
- Noise filter: the divider looks like R6‖R7 = 5 kΩ driving C4, so f_c = 1/(2π · 5 kΩ · 10 µF) = **3.2 Hz**.
- U2B (MCP6002 half B) is a **unity-gain buffer**, output tied to the − input, so V_out = V_in. R15 and R16 draw current from VREF; without the buffer that current would shift the divider.
- **Ratiometric:** VREF and the ADC both come from the same 3.3 V. If 3.3 V drifts by 1 %, VREF and the ADC scale both drift by 1 %, so the reading in counts doesn't change.

---

## 3. Heater (block C)

### 3.1 Heater power
Three 150 Ω resistors in parallel: R = 150 / 3 = **50 Ω**

  I = V / R,  P = V² / R

| Battery state (4S Li-ion) | V | I | P total | P per resistor (5 W rated) |
|---|---|---|---|---|
| full (4.2 V/cell) | 16.8 | 0.34 A | 5.6 W | 1.9 W |
| nominal (3.7 V) | 14.8 | 0.30 A | **4.4 W** | 1.46 W |
| low (3.2 V) | 12.8 | 0.26 A | 3.3 W | 1.1 W |
| bench PSU | 12.0 | 0.24 A | 2.9 W | 0.96 W |

Why three in parallel instead of one 50 Ω? The power spreads over three bodies (cooler, larger heating area), and if one opens the heater still works at ⅔ power.
Energy for a 3-hour flight at full power: 4.4 W · 3 h = **13 Wh**. The battery channel allows 6 A; we use 0.3 A.

### 3.2 Can the heater keep the box warm? (heat balance)
In steady state, heat in = heat out:

  P = (T_box − T_out) / R_th     →   R_th,needed = (T_box − T_out) / P

R_th is the thermal resistance of the box insulation in K/W. With P = 4.4 W and T_box = 20 °C:

| T outside the box | R_th needed |
|---|---|
| −20 °C | 9 K/W |
| −40 °C | 13.6 K/W |
| −60 °C | 18 K/W |

So **insulation matters as much as the heater**. Thick foam and a closed volume help, and in thin air there is almost no convection, which also helps. The firmware's 3-hour simulation (−40 °C outside, 15 K/W) holds 20 °C at ~90 % duty.

### 3.3 MOSFET (Q1 IRLZ44N)
- Gate voltage: R11 and R12 form a divider from the 3.3 V PWM pin: V_G = 3.3 · 10 k / (1 k + 10 k) = **3.0 V**. IRLZ44N V_GS(th) = 1–2 V, so it is fully on for 0.3 A.
- Conduction loss: P = I² · R_DS(on) ≈ 0.296² · 0.035 Ω = **3 mW** (R_DS(on) at V_GS = 4 V). No heatsink is needed.
- Gate RC: 1 kΩ · ~1.7 nF (C_iss) ≈ 1.7 µs ≪ 1 ms PWM period (1 kHz). Switching is clean.
- PWM average power: P_avg = D · P_max. Duty 128/255 = 50 % → 2.2 W.
- No flyback diode: the load is a resistor (no inductance), so there's no voltage spike at turn-off.

### 3.4 Why the LM393 can "win" against the MCU
The LM393 output is **open-collector**: it can only pull down. When it is on, it sinks I = 3.3 V / R11 = 3.3 V / 1 kΩ = **3.3 mA** from the MCU pin (LM393 can sink ≥ 6 mA). The gate is held at V_OL ≈ 0.2 V < V_th, so the heater is OFF regardless of the PWM. Without R11 the comparator would be fighting a push-pull pin head-on.

---

## 4. Thermistors and the hardware cutoff (block C, lower)

### 4.1 NTC resistance (Beta model)
  R(T) = R₂₅ · exp( B · (1/T − 1/T₂₅) ),   T in kelvin, R₂₅ = 10 kΩ, T₂₅ = 298.15 K, B ≈ 3950 K

Inverse, used in firmware:  1/T = 1/T₂₅ + ln(R/R₂₅) / B

### 4.2 Divider voltage (10 k pull-up to 3.3 V, NTC to GND)
  V = 3.3 · R_NTC / (10 k + R_NTC),   ADC counts = 4095 · R_NTC / (10 k + R_NTC)

From the ADC reading back to resistance: R_NTC = 10 k · ratio / (1 − ratio), ratio = counts / 4095

| T (°C) | R_NTC | V | counts |
|---|---|---|---|
| −40 | 402 k | 3.22 | 3996 |
| −20 | 105 k | 3.01 | 3740 |
| 0 | 33.6 k | 2.54 | 3156 |
| 20 | 12.5 k | 1.84 | 2278 |
| 25 | 10.0 k | 1.65 | 2048 |
| 35 | 6.5 k | 1.30 | 1614 |
| **40** | **5.3 k** | **1.143** | **1419** |
| 50 | 3.6 k | 0.87 | 1081 |

Sensitivity near 40 °C: dV/dT = (dV/dR)(dR/dT) ≈ **−30 mV/°C**, about 37 ADC counts per °C.

### 4.3 Cutoff threshold (RV2) and the comparator
- RV2 is set so its wiper reads **≈ 1.14 V**, the NTC voltage at 40 °C.
- The LM393 output is low (heater forced OFF) when V(+) < V(−), i.e. V_NTC2 < V_thresh. A hotter NTC means lower resistance and lower voltage, so it trips.
- Why the LM393 runs on 5 V: its input common-mode range is 0 … V_CC − 1.5 V. On 3.3 V that's 0–1.8 V, so an NTC reading of 3.0 V (−20 °C) would be outside it. On 5 V it's 0–3.5 V, which covers everything.
- C7 filter: Thevenin of R18 ‖ NTC at 40 °C = 10 k ‖ 5.3 k = 3.46 kΩ; τ = 3.46 kΩ · 100 nF = 0.35 ms. It kills noise spikes but is far faster than any temperature change.

---

## 5. Power budget (from the flight controller rails)

| Rail | Load | Current |
|---|---|---|
| +5 V | IR LED | 17 mA |
| +5 V | LM393 | ~0.5 mA |
| +3.3 V | MCP6002 (2 halves) | ~0.2 mA |
| +3.3 V | dividers (VREF, 2 NTC, RV2) | ~0.8 mA |
| +3.3 V | phototransistor | < 0.1 mA |
| +3.3 V | SHT21 + BMP280 | < 1 mA |
| +3.3 V | MAX30100 (IR LED pulsed) | ~5–20 mA avg |
| 14.8 V | heater | 0–300 mA |

The 3.3 V and 5 V payload channels allow 3 A each, so we use < 2 %.

---

## 6. Firmware maths

### 6.1 Sampling
- f_s = 50 Hz. The fastest heart we accept (150 bpm = 2.5 Hz) gets 20 samples per beat. The Nyquist limit is 25 Hz, far above the analog low-pass at 3.4 Hz, so there is no aliasing.
- Telemetry waveform: average every 5 samples → **10 Hz**, 8-bit (counts ÷ 16). Over 2 s that's 20 samples in each SNAIL packet.

### 6.2 Digital filters in the beat detector
Exponential moving average (a first-order IIR low-pass): y[n] = y[n−1] + α · (x[n] − y[n−1]),  α = 1 − e^(−1/(τ·f_s))

| Filter | τ | α at 50 Hz | purpose |
|---|---|---|---|
| smoothing | 80 ms | 0.221 | remove sample noise |
| baseline | 6 s | 0.00333 | follow slow drift |
| envelope release | 6 s | decay 0.99667 per sample | remember pulse size |

- Signal d = smoothed − baseline. Envelope E = max(|d|, E · decay).
- **Beat** = d rises above 0.4·E (then re-arms when d < 0.1·E). The gap between the two thresholds is **hysteresis**, so noise can't double-trigger.
- **Refractory period** = max(400 ms, 0.4 · median interval). Bumps sooner than that are ignored.
- **Rate** = 60 000 / median(last 5 intervals in ms) bpm. A median ignores one wrong interval; a mean would not.
- **Quality** = 100 · (1 − 2 · CV), CV = σ/μ of the intervals. CV is the coefficient of variation: a regular rhythm has a small CV and high quality.
- No-signal: envelope < 6 counts (≈ 5 mV at the ADC, ≈ 48 µV at the sensor).

### 6.3 Heater controller (PI)
  e = T_target − T_box,  I ← clamp(I + K_i · e · Δt, 0, 255),  duty = clamp(K_p · e + I, 0, 255)

K_p = 40 counts/°C, K_i = 0.5 counts/(°C·s). Clamping the integral ("**anti-windup**") stops it growing huge while the heater is already at 100 %.
Plate limit: duty × (35 − T_plate)/3 between 32 and 35 °C, and 0 above 35 °C.

### 6.4 Telemetry budget
Frame = payload + 7 bytes (2 sync + type + seq + len + 2 CRC).

| Packet | frame | every | B/s |
|---|---|---|---|
| SNAIL | 46 B | 2 s | 23.0 |
| BASE | 41 B | 2 s | 20.5 |
| ENV | 31 B | 4 s | 7.8 |
| STATUS | 31 B | 10 s | 3.1 |
| **total** | | | **54.4 B/s = 435 bit/s** |

Air rate 2400 bit/s, so the link is **18 %** used. One full 58-byte LoRa packet takes ≥ 58 · 8 / 2400 = **0.19 s** on air (plus LoRa preamble). Keeping every frame ≤ 58 B means one frame is exactly one radio packet: no frame is ever split, so a lost packet loses only one frame.

### 6.5 CRC-16/CCITT
Polynomial x¹⁶ + x¹² + x⁵ + 1 (0x1021), initial value 0xFFFF. It detects **all** 1- and 2-bit errors, all odd numbers of bit errors, and all bursts ≤ 16 bits. A random error slips through with probability ≈ 1/65 536. Check value: "123456789" → 0x29B1 (tested).

### 6.6 Altitude from pressure (International Standard Atmosphere)
- 0–11 km (temperature falls 6.5 K/km): h = 44 330.8 · (1 − (p / 101 325)^0.190263)
- 11–20 km (isothermal, 216.65 K): h = 11 000 − 6341.6 · ln(p / 22 632)
- 20–32 km (+1 K/km): h = 20 000 + 216 650 · ((p / 5474.9)^(−1/34.163) − 1)

| p (mbar) | ISA altitude | troposphere-only formula |
|---|---|---|
| 226 | 11.0 km | 11.0 km |
| 100 | 16.2 km | 15.8 km |
| 54.7 | 20.0 km | 18.9 km |
| **30** | **23.8 km** | **21.6 km** |
| 10 | 31.1 km | 25.9 km |

The simple formula (used by `MS5611::GetHeight()`) is only valid below 11 km. At our float altitude it under-reads by over 2 km.

### 6.7 MS5611 compensation (datasheet)
  dT = D2 − C5·2⁸ ;  TEMP = 2000 + dT·C6/2²³  (0.01 °C)
  OFF = C2·2¹⁶ + C4·dT/2⁷ ;  SENS = C1·2¹⁵ + C3·dT/2⁸
  P = (D1·SENS/2²¹ − OFF)/2¹⁵  (0.01 mbar)

If TEMP < 20 °C, the second order applies: T2 = dT²/2³¹, OFF2 = 5(TEMP−2000)²/2, SENS2 = 5(TEMP−2000)²/4. Below −15 °C, add OFF2 += 7(TEMP+1500)² and SENS2 += 11(TEMP+1500)²/2.
Datasheet check: D1 = 9 085 466, D2 = 8 569 150 → 20.07 °C, 1000.09 mbar ✔ (unit test).

### 6.8 BMP280
Bosch integer formulas (datasheet §8.2). Datasheet check: adc_T = 519 888, adc_P = 415 148 → 25.08 °C, 100 653 Pa ✔ (unit test).
**Range 300–1100 hPa:** it only works inside a sealed box. Outside at 24 km (~30 hPa) it would be out of range.

---

## 7. Radio link (rough budget)

Free-space path loss: FSPL(dB) = 20·log₁₀(d [m]) + 20·log₁₀(f [Hz]) − 147.55

| distance | FSPL @ 433 MHz | received (30 dBm TX, ~2 dBi antennas each side) |
|---|---|---|
| 10 km | 105 dB | −71 dBm |
| 30 km | 115 dB | −81 dBm |
| 50 km | 119 dB | −85 dBm |
| 100 km | 125 dB | −91 dBm |

LoRa at 2.4 kbit/s can receive signals far weaker than this; the exact sensitivity is in the E32 datasheet. So the margin is tens of dB in theory. In practice, antenna orientation, cable loss and the satellite spinning eat into it. **A lower air rate means more range**: each bit lasts longer, so the receiver integrates more energy per bit.

---

## 8. Snail-specific numbers to quote carefully

- *Cornu aspersum* (= *Helix aspersa*) heart rate: tens of bpm. About **100 bpm in young snails at high temperature** (Toader-Williams & Buicu 2011). It falls with cold, dormancy and dehydration (to ~63 % of the initial rate during dehydration, Biannic et al. 1994).
- The IR-through-shell method (CAPMON / "noninvasive heartbeat") uses the same kind of reflective IR sensor (Vishay CNY70) glued over the heart (Burnett et al. 2013). Their filters: low-pass ≈ 2.2 Hz, gain ≈ 7900 for 0.2–1 Hz. Ours: 3.4 Hz, gain 101 analog, and digital processing on top.
