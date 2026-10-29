# Orbita 2026: Better Missions for the BioCube Team (Round 2)

**Our main advice: make the satellite *do* a job around the snail. Azizbek builds a custom PCB that reads the snail's heartbeat live through its shell. Nurmuhammad builds a radio experiment that tests a space technology. The Insta360 becomes a measuring instrument (a vision-based attitude sensor), not only a source of nice video.** The rotor, spark (Paschen), sound and heat-plate ideas are interesting physics. But each one can be copied in a cheap vacuum jar, or is hard to measure on a swinging balloon. They are weaker on the 27 rubric points that need clean flight data.

## TL;DR
- **Azizbek's mission, "SnailPulse":** a custom infrared heartbeat sensor (CNY70-type reflective optocoupler plus an analog filter on your PCB) reads the snail's heartbeat through the shell. The STM32 counts beats and sends the live heart rate down over LoRa. On the slide: a graph of heart rate against flight phase (launch, ascent, burst, parachute), with a predicted baseline of 46–69 beats per minute, the resting range reported by Ballard et al. in PLOS ONE (2021). We found no example of this being flown on a balloon or CubeSat.
- **Nurmuhammad's mission:** a "space-based ADS-B" demo. An RTL-SDR plus a Raspberry Pi Zero 2 W hears airliners from the stratosphere, and you compare the maximum reception range with a radio-horizon and link-budget model. Only do this if the organisers approve receiving aviation signals. The zero-mass fallback uses the kit's own LoRa link: a "Lloyd's mirror" experiment that measures ground reflectivity with two ground antennas at different heights, which links to the sister project SALOPI.
- **Insta360 (X4 recommended):** power it from the kit's 5 V rail, because the battery is rated only down to −20 °C and the flight is longer than the 8K run time. Then use the footage to rebuild the CubeSat's spin and swing from the horizon and the Sun, and check the result against the kit's gyro and magnetometer. That is a real measurement; balloon-size and Earth-curvature videos are only nice extras.

## Key Findings

### Part A — What wins at Orbita and similar competitions

**Orbita results we could confirm:**
- **2025 final (19–25 Aug, Moscow/Korolev, ~20 km):** a team from Uzbekistan won. The organiser (cikeno.ru) wrote that all satellites worked in flight, "but not all experiments achieved the planned results, which determined the outcome of the competition".\[1\]\[2\] So **the deciding factor was whether the experiment actually worked**, not how fancy it was. The winner was very probably the UniSat girls' team: UniSat's own site mentions "their victory at the 'Orbita' tournament in Moscow".\[3\]\[4\] **But we could not find what they flew.** Our research helper also searched and found no published payload description. Ask your Uzbek national committee contact (UniSat / Nora Rakhimova); they will know.\[5\]
- **2024 final (Yekaterinburg, up to 24 km, launched with Stratonavtika):** teams from Russia (Fractal Space) and Uzbekistan tied on points.\[6\]\[7\] Neither winning experiment is described in public sources we found. One 2024 participant (Yekaterinburg schoolboy Egor Vasiliev, team not confirmed) flew a **solar-powered 3D-printing device**, pitched as "cosmonauts could print broken parts in flight".\[8\] This is the same style as the organisers' examples (friction welding, solar alloy melting, solar concentrator).

**Similar Russian programmes (Stratonavtika "Стратосферный спутник", same launch operator):**
- 2022 teams flew: "best magnetometer" comparison; **static vs sun-tracking solar panel** ("Stratolaunch 2"); **radio-wave propagation through the stratosphere** ("Импульс", a winner); Liesegang rings chemistry.\[9\] "Импульс" (Chelyabinsk) won again in 2025 in the top "Researchers" league.\[10\] **So a radio experiment has already won here.** A plain RSSI-vs-distance log is "repeated many times", and Nurmuhammad needs a new angle.
- "СтратоСТАРТ" (Nalchik) flew an **optomechanical IR scanner on a gyro-stabilised platform** (ESP32-CAM, servos) in 2024.\[11\] They placed 2nd in their league in 2025.\[10\]
- ITMO students flew bone-tissue (hydroxyapatite) formation samples.\[12\]
- The rules for repeat teams say that if you repeat an experiment you must justify what changed.\[10\] That tells you the jury rewards **iteration and novelty**.

**International:**
- Global Space Balloon Challenge: AGH's LEO3 team ("Let's Explore Ozone": biology samples plus UV/ozone sensing) won **1st place for Best Scientific Experiment at GSBC 2016, ahead of 400+ teams**. Their 2019 honeybee-endurance payload (with CO2 and UV-C sensors) won 2nd in Best Educational Initiative. Purdue's AMET aerospace team won Best Design at GSBC 2014 for building its own zero-pressure balloon that flies higher than off-the-shelf balloons, which burst in low pressure.
- NASA TechRise judges score "originality, impact on education or society, feasibility within the allotted timeframe and budget, and the quality of the build plan".\[13\]

**Pattern of what juries reward:**
1. It **worked in flight and produced a full data set**. This was decisive at Orbita 2025.
2. A **device that does a task** and comes with a spaceflight story (3D printing, sun tracking, stabilised scanner, welding).
3. **A comparison or test with a clear yes/no answer** from one flight (tracking vs fixed panel, theory vs measurement).
4. **Something the audience can see** (live data on the ground screen, video proof).
5. **Biology plus engineering together** wins abroad (LEO3, bees) when the engineering measures the organism live.

### Part B — Candidate active missions, judged honestly

**Air density reference (standard atmosphere):** 20 km ≈ 55 mbar, 0.089 kg/m³; 25 km ≈ 25 mbar, 0.040 kg/m³; 30 km ≈ 12 mbar, 0.018 kg/m³. Mars surface ≈ 0.015–0.020 kg/m³; NASA tested Ingenuity at 0.0175 kg/m³.\[14\]\[15\] **So 20 km is about 5× denser than Mars and 25 km about 2×. Mars-equivalent density needs ~30 km**, but past Orbita flights reached 20–24 km.

| # | Candidate | Wow for judges | Novelty (rubric /5) | Needs stratosphere? | Active? | P(clear result, 1 flight) | Build in 3 weeks | Cost | Mass / vol / power | Main risk | Owner |
|---|---|---|---|---|---|---|---|---|---|---|---|
| 9 | **SnailPulse: live snail heartbeat telemetry** | High (live heartbeat on screen) | **5** (no balloon/CubeSat precedent found) | Partly (real flight stress: launch, cold, burst, landing shock) | Yes (sensing + onboard DSP + live link) | ~70% | Yes | $10–20 | 20 g, inside snail box, <50 mW | Weak optical signal through a thick or dark shell | Azizbek |
| 11a | **ADS-B "space-based air-traffic receiver"** | High (live map of airliners) | 4 (done by hobbyists, rare on CubeSat-in-balloon; ISTSat-1 is an orbital ADS-B CubeSat) | Yes (range set by height) | Yes (receiver + decoding) | ~75% if allowed | Yes | $60–90 | ~60 g, ~0.3U, ~2–2.5 W at 5 V | Permission; 1090 MHz antenna sticks out | Nurmuhammad |
| 11b | **Lloyd's-mirror ground reflectivity with the kit LoRa** | Medium | 4 (link to SALOPI) | Yes (geometry needs a high transmitter) | Yes (two-antenna interferometry) | ~55% | Yes | ~$15 (2nd ground receiver) | 0 g in flight | Needs balloon track (no GPS in kit) | Nurmuhammad |
| C | **Vision attitude sensor from Insta360** | High | 4 | Yes (real spin/swing dynamics) | Analysis-based | ~75% | Yes | camera only | ~200 g outside | Camera stops in cold / power | Azizbek |
| 5 | Reaction-wheel yaw control | High | 3 (McHAB, HAVOC, BIT did it) | Yes | Yes | ~40% | Tight | $30–50 | 150–250 g, 0.3U, 2–5 W peaks | String torsion saturates wheel; needs a swivel | Azizbek |
| 1 | "Mars air" rotor thrust | High | 3–4 | **Partly** (a vacuum jar does the same) | Yes | ~35% | Tight | $40–60 | 100–150 g, 0.5U, 5–20 W | Thrust only ~1.5–3 g, buried in swing noise | Begoyim + Azizbek |
| 2 | Paschen breakdown | High (glow video) | 3 | **Partly** (vacuum jar does it) | Yes | ~50% | Tight | $20–40 | 50 g, 3–5 W | HV + sparks near LoRa and biology; operator may refuse | Nurmuhammad |
| 10 | SSDV live thumbnails | High | 3 (UKHAS standard) | No | Yes | ~60% | Tight | $15–40 | 30 g, 1 W | 2.4 kbit/s is very slow; camera driver time | Azizbek |
| 3 | Sound vs pressure | Medium | 3 | Partly | Yes | ~50% | Yes | $15 | 30 g | Mic/speaker behave oddly in cold; vacuum jar can do it | Azizbek |
| 4 | Heated-plate convection coefficient | Low–Med | 3 | Partly (vacuum jar) | Yes | ~70% | Yes | $10 | 30 g, 1–2 W | Looks like logging | Begoyim |
| 6 | Burn-wire/nitinol deployable | High (video) | 3 | Partly (cold + low pressure) | Yes | ~60% | Yes | $10–20 | 50–100 g | Envelope/safety rules; nothing may fall off | Begoyim |
| 7 | UV-cure resin / 3D print at altitude | Medium–High | 3–4 | Partly | Yes | ~50% | Tight | $30 | 100–200 g, 2–5 W | Low sun in late Oct; resin is a chemical for the airline | Begoyim |
| 8 | Air/particle sampler | Medium | 3 | Yes | Yes | ~30% convincing | Tight | $30 | 100 g | Contamination; one flight is not convincing | Diyora |
| 12 | Sun tracker / TEG | Medium | 3 (done in 2022) | Partly | Yes | ~50% | Tight | $20–40 | 100 g | Low sun (~20° at noon), clouds | Begoyim |
| 13 | UV-B/UV-A ozone | Medium | 3 (common) | Yes | No | ~60% | Yes | $20 | 20 g | Passive logging, low sun | — |
| 14 | Muon telescope / shielding | Medium | 3 | Yes | No | ~60% | Tight | $80+ | 200 g+ | Passive counting again | — |
| — | RAM bit flips | Low | — | — | — | ~1% | — | — | — | ≈0.01 expected events | rejected |

**Short notes on the ideas we do *not* recommend as main missions:**

- **Mars rotor (1):**
  - Thrust is T = C_T·ρ·n²·D⁴. For a 76 mm propeller at 20 000 rpm with C_T ≈ 0.1, T ≈ 0.44 N (45 g) at sea level, about 3.4 g at 20 km and 1.5 g at 25 km.
  - At those levels the pendulum swing (±0.1 g on a 20 g motor, i.e. ±2 g of fake force) is as big as the signal.
  - Reaction torque will spin the CubeSat unless you use two counter-rotating rotors.
  - Researchers do exactly this in vacuum chambers: NASA Ames' Planetary Aeolian Laboratory spun ROAMX rotor blades up to 4000 rpm at Mars density (about 0.015 kg/m³); JPL's 25-Foot Space Simulator pushed next-generation rotor tips to Mach 1.08 over 137 test runs in November 2025; and the China Academy of Aerospace pumped its vacuum chamber to about 1950 Pa instead of Mars's 756 Pa (Zhao et al., Drones, 2023). The jury could say "use a vacuum chamber". The density at 20–24 km is not Mars density.
  - A better version: hold the motor at constant power and log RPM rising as the air thins (the "run-away" curve). That is easier to measure than thrust. Still not our first choice.
- **Paschen (2):**
  - Paschen curve for air: V = B·pd / [ln(A·pd) − ln(ln(1+1/γ))], with A ≈ 15 cm⁻¹Torr⁻¹, B ≈ 365 V cm⁻¹Torr⁻¹, γ ≈ 0.01.
  - For a 1 mm gap: ≈ 5 kV at sea level, ≈ 580 V at 20 km, ≈ 310 V at 30 km (near the minimum of ~330 V).
  - Spacecraft engineers care about this (Cal Poly measured Paschen curves for CO2/Mars).\[16\] But a ~$50 vacuum jar reproduces it exactly, and HV sparks next to the LoRa module and the live snail invite a "no" from Stratonavtika.
- **Reaction wheel (5):**
  - Sizing: the 3U body has I_z ≈ m(a²+b²)/12 = 3·(0.1²+0.1²)/12 ≈ 0.005 kg·m². Stopping a 30°/s spin needs H ≈ 0.0026 N·m·s. A 100 g, 4 cm-radius steel disc (I = 8·10⁻⁵ kg·m²) needs only ~310 rpm. Easy.
  - The real problem is the suspension line, which keeps twisting the payload, so the wheel saturates within minutes unless there is a ball-bearing swivel. Whether there is one depends on Stratonavtika's flight train (ask).
  - Known results: McHAB designed a single-wheel yaw platform; the HAVOC cold-gas system held rotation within ±40°/s for 1 h 49 min and azimuth within ±30° for 1 h.\[17\]\[18\]

### Part C — The Insta360 outside the CubeSat

**What flew before:**
- An Insta360 ONE X2 flew with NewMakeIt (~100 000 ft, with an extra battery inside a dome).\[19\]
- The Karman Space Programme flew an X3 to 26 km. Their claim is that it was mounted outside with no insulation, "operating perfectly" at −40 °C.\[20\] Note: this is a story on Insta360's own blog, so treat it as marketing.
- A Columbia student team flying an X3 "only got the first half hour of the 2.5 hour flight".\[21\] **Power and cold are the real failure points.**

**Insta360 X4 numbers:**

| Item | Value | Comment |
|---|---|---|
| Mass | 203 g | |
| Battery | 2290 mAh\[22\] | |
| Operating temperature | −20 °C to 40 °C\[23\] | The −20 °C limit applies to the battery |
| Run time at 5.7K30 | 135 min | Lab environment\[22\] |
| Run time at 8K30 | 75 min | Insta360's manual says "around 75 mins" (lab, 25 °C, screen off); B&H's 71 min is a retailer figure |
| microSD | Up to 1 TB\[24\] | |

Outside air at 20 km is about −55 °C, the flight lasts ~2–2.5 h, and you also wait on the ground before launch. So:

- **Run 5.7K30, not 8K.** It is more than enough for analysis.
- **Feed the camera from the kit's 5 V rail via a USB-C cable** (Insta360 says the cameras can record while powered by an external source with the battery inserted). Wrap the body in foam, leaving the lenses free, and put a chemical hand warmer against the battery side. Check whether the hand warmer works in thin air: it needs oxygen, so test it in your vacuum jar.
- **Start recording at the last minute**, and turn off auto-shutdown and voice/gesture control.
- **Use a 256 GB V30 card.** We estimate 2.5 h of 5.7K is roughly 60–100 GB (our estimate, not a published figure).
- **Fogging:** dry the camera with silica gel in a bag before launch. Frost on the lens is a known risk when the camera descends into wet cloud.
- **Mounting:** a 3D-printed PETG/ABS bracket bolted to the rails at the top or bottom end, so both lenses see past the body (a "stick" of 3–5 cm). It sticks out of the CubeSat envelope, so ask permission. Mass budget: 203 g + ~40 g bracket + cable.

**Analyses ranked (novelty × effort):**

1. **Vision-based attitude sensor (the best one; it can be a mission by itself).**
   - In each frame (1–5 fps is enough), find the horizon (a circle or ellipse in the equirectangular image) and the Sun. Those two give roll/pitch and yaw.
   - Compare with the kit's LSM6DS3 gyro (integrated) and LIS3MDL magnetometer heading. The Insta360 also has its own internal gyro, which Gyroflow can read (check for your model), so you get three independent attitude sources.
   - Result: graphs of spin rate and pendulum angle against time, the error of each sensor, and gyro drift in °/min. Spacecraft relevance: horizon and Sun sensors are standard CubeSat attitude sensors.
2. **Pendulum and spin dynamics.** Swing period T = 2π√(L/g). For L = 5 m, T ≈ 4.5 s. Compare with the frame-measured period, and watch the spin change at burst.
3. **Horizon dip / Earth radius.** Dip = arccos(R/(R+h)): 4.5° at 20 km, 5.1° at 25 km. Horizon distance √(2Rh) ≈ 505 km at 20 km. A nice check, but common.
4. **Balloon diameter vs ideal-gas prediction.** Classroom-standard (there is a published "Weather Balloon Activity Helps Students Understand Ideal Gas Laws"), so it scores 3/5.\[25\] The burst frame is great for the slide.
5. **Visual navigation without GPS** (match ground frames to satellite maps → ground track and wind speed). Genuinely novel for a GPS-less kit, but late-October Moscow is often overcast, so the result is uncertain.
6. **Filming the payload working** (proof video), sky colour, cloud parallax: nice visuals only.

**Workflow in the few hours between recovery and defence:**
- Copy the .insv files.
- Insta360 Studio → export equirectangular MP4 (5.7K, no stabilisation).
- `ffmpeg -i in.mp4 -vf fps=2 frames/%05d.jpg`.
- A Python/OpenCV script, **written and tested beforehand on a test video recorded in Tashkent**: threshold the sky/ground, fit the horizon, find the Sun blob, compute angles, plot against the SD-card IMU log.
- Sync clocks with a "clap": shake the CubeSat sharply in front of the camera at power-on.

### Part D — Recommended missions

#### Mission 1 (Azizbek): "SnailPulse — the first live heartbeat from the stratosphere"

**Passport sentence:** Develop and flight-test a non-invasive infrared heartbeat monitor with onboard signal processing and live LoRa telemetry, to measure the cardiac response of a garden snail (*Cornu aspersum*) to each phase of a stratospheric flight.

**Why it is strong:**
- It is active: your PCB lights the animal, filters the signal, detects beats and sends them.
- It is novel. We found no balloon or CubeSat heartbeat telemetry of a gastropod. That is a search result, not proof; say "to our knowledge".
- It turns the existing snail into an engineering mission and connects to space biology (monitoring animals and astronauts on board).
- The method is proven in the lab. Non-invasive IR heartbeat sensing of molluscs and crustaceans uses the Vishay CNY70: Electricblue's PULSE logger uses CNY70 or the smaller QRE1113 glued over the heart, and Burnett et al. describe a holder for the reflective optocoupler "glued externally over the heart region" for the crab and the garden snail.\[26\]\[27\]

**Theory and hypotheses:**
- **Baseline:** resting heart rate of *C. aspersum* at room temperature was 46.9 and 68.7 beats/min in two groups in a PLOS ONE 2021 study (Ballard et al.).
- **H1 (temperature):** heart rate follows the container temperature. With an assumed Q10 ≈ 2, which is typical for ectotherms and should be confirmed in your own ground calibration, HR(T) = HR₂₀ · 2^((T−20)/10). A drop from 20 °C to 15 °C predicts −29% (e.g. 60 → 42 bpm).
- **H2 (mechanical stress):** shocks at launch, burst and parachute opening cause a short heart-rate change. In *Helix aspersa*, withdrawal reflexes raised beat amplitude by 11.6% and shortened beat intervals by 2.1%, lasting on average 28.1 s (Weatherill & Chase 2005).\[28\]\[29\] Prediction: a detectable change lasting ~30 s after the burst shock seen on the kit accelerometer.
- **H3 (life support works):** if the heater keeps 20 ± 2 °C at 1 atm, the heart rate at float stays within ±15% of the ground baseline.
- **Signal processing:**
  - Sample at 50 Hz.
  - Band-pass 0.3–3 Hz (18–180 bpm).
  - Heart rate by autocorrelation over a 20 s window, with a quality index (peak height).
  - Send HR + quality + temperature every packet (fits in the 58-byte E32 packet).
  - Store the raw waveform on the microSD card.

**Hardware block diagram:**
```
[CNY70 #1 on shell]--+                       Azizbek's payload PCB
[CNY70 #2 spare]-----+--> LED driver (GPIO/PWM, 10-20 mA pulsed)
                          phototransistor -> TIA/gain (MCP6002 or OPA2333)
                          -> HP 0.3 Hz -> LP 3 Hz -> gain x100-1000
                          -> STM32 ADC (kit ADC input)
[SHT31/ BME280 in snail box] -> I2C4
[optional piezo film LDT0-028K in worm soil] -> 2nd ADC channel
Power: 3.3 V rail (<50 mW).  Data: SD log + LoRa (E32) downlink.
Heater control: existing kit/power board loop, setpoint 20 °C.
```

**Parts:**

| Part | Use | Approx. cost |
|---|---|---|
| Vishay CNY70 ×3 (or QRE1113) | Heartbeat sensors (one spare) | ~$1 each |
| MCP6002 or OPA2333 | Op-amp front end | $1–3 |
| Resistors, capacitors (0603) | Filter and gain | — |
| Sensirion SHT31 or Bosch BME280 | Temperature/humidity in the snail box | $3–8 |
| 3D-printed sensor cradle + soft silicone pad | Holds the sensor over the shell | — |
| JST-GH connectors | Locking connectors for flight | — |
| LDT0-028K piezo film (optional) | Worm activity | ~$5 |

**What to show the judges:**
- (a) A screenshot of the live ground-station heart rate during flight.
- (b) A graph of HR, container temperature, pressure altitude and accelerometer magnitude against time, with launch, burst and landing marked.
- (c) A table of predicted vs measured HR for each flight phase.

**If it partly fails:** if the signal quality is low in flight, show:
- the ground calibration curve (HR against temperature for 3–5 snails);
- the onboard algorithm working on raw SD data afterwards;
- the life-support temperature record.

That way you still earn points for flight operation and analysis. The engineering result becomes "what limits optical heartbeat sensing in flight".

**Ground tests (Tashkent):**
1. Find the heart position. The heart sits under the last body whorl near the shell opening; look through the shell with a torch. Measure HR by eye vs sensor for 5 snails.
2. Calibrate HR against temperature in a fridge/water bath (10, 15, 20, 25 °C).
3. Vibration test: tap the container and look for the reflex.
4. Full 3-hour run on the battery with the freezer at −20 °C and dry ice (−60 to −78 °C) around the box.
5. Vacuum jar at ~20–50 mbar, to prove the sealed box holds 1 atm and the electronics do not overheat.

#### Mission 2 (Nurmuhammad): "Stratospheric ADS-B receiver: testing space-based air-traffic surveillance"

**Passport sentence:** Measure how the reception range and number of aircraft detected by a low-cost 1090 MHz ADS-B receiver grow with altitude, and compare this with a radio-horizon and link-budget model, as a step toward CubeSat-based air-traffic surveillance (like the ISTSat-1 CubeSat).\[30\]

**Theory:**
- **Radio horizon** (4/3-Earth refraction): d ≈ 4.12(√h₁ + √h₂) km, with h in metres. Balloon at 20 000 m and aircraft at 11 000 m give ≈ 1015 km (geometric ≈ 880 km).
- **Link budget:** transponder ~250 W (54 dBm). Free-space loss at 500 km and 1090 MHz is 20log(500) + 20log(1090) + 32.44 ≈ 147 dB, giving ≈ −93 dBm. That is at the edge of RTL-SDR sensitivity (~−90 to −95 dBm, our estimate).
- **Hypothesis:** maximum range rises from ~150–250 km near the ground to 400–550 km at 20 km, so it is **link-budget limited, not horizon limited**, above ~10 km. The Moscow region (three big airports) should give 100+ aircraft.

**Hardware:**
- RTL-SDR Blog V4 (~30 g).
- Raspberry Pi Zero 2 W (~10 g) running readsb/dump1090, logging to its own SD card.
- A UART link to the STM32 (one of the 3 UARTs), sending "aircraft count, max range" every 10 s for the LoRa downlink.
- A 1090 MHz quarter-wave whip (69 mm), or a small 3D-printed collinear antenna, pointing down.
- Power: kit 5 V/3 A rail, ~2–2.5 W.

**Result on the slide:** a map of all aircraft positions received, coloured by balloon altitude, plus a graph of maximum range against altitude with the model curves.

**Risks:**
- **Legality and permission.** We could not confirm the Russian rules for receiving ADS-B from a balloon. Ask the organisers first.
- The antenna sticks out of the CubeSat.
- The Pi needs a clean shutdown (use a read-only filesystem).

**Fallback (zero flight mass): "Lloyd's mirror at 433 MHz".**
- Put two kit-type receivers at 1 m and 4 m height at the launch site, and log RSSI of the existing telemetry.
- Two-ray model: P ∝ |1 + Γ·e^(−jk·2h·sinθ)|². With λ = 0.69 m and h = 4 m, nulls fall at sinθ = nλ/(2h), i.e. θ ≈ 5°, 10°, 15°… as the balloon drifts away and the elevation angle drops.
- The depth of the nulls gives the ground reflection coefficient |Γ|. That is the same physics as SALOPI/SNoOPI (reflected signals → soil moisture/salinity).\[31\]\[32\] It is new as a demo, costs ~$15, and adds no flight mass.
- It needs the balloon's horizontal range. Ask Stratonavtika for its GPS track, since the kit has no GPS.

**Fallback for Azizbek:** if the heartbeat signal fails in tests, switch to the **Insta360 vision attitude sensor** (Part C, analysis #1). It needs no new flight hardware.

#### Day-by-day plan (8–26 October)

| Dates | Azizbek | Nurmuhammad | Begoyim / Diyora |
|---|---|---|---|
| 8–9 Oct | Send questions to organisers; breadboard CNY70 + op-amp on a snail | Send ADS-B legality question; order RTL-SDR V4 + Pi Zero 2 W (or borrow locally) | Diyora: find heart location on 5 snails; Begoyim: sensor cradle CAD |
| 10–12 Oct | Get the first clean heartbeat trace; pick gain/filter values | Set up the ground LoRa logger (RSSI/SNR + time) and the second receiver at 4 m | Print cradle v1; design camera bracket |
| 13–15 Oct | Design PCB (KiCad), order fast or mill/etch locally; write beat-detection code | Pi + readsb on the ground: log aircraft, test range from a rooftop | HR-vs-temperature calibration (Diyora) |
| 16–18 Oct | Solder PCB; integrate with the STM32 ADC/I2C4; LoRa packet format | Integrate the UART link Pi→STM32; antenna build and test | Integrate snail box + sensor; camera mount |
| 19–21 Oct | Full system test: freezer, dry ice, vacuum jar | Cold test of Pi/SDR (dry ice box, 2 h) | Drop/shock test of payload and camera mount |
| 22–23 Oct | 3-hour "flight rehearsal" on battery, with live ground station; Insta360 analysis script tested on a test video | Same rehearsal; write the link-budget calculation | Mass check (≤3 kg), CubeSat dimension check |
| 24–25 Oct | Write the theory part and predictions; prepare slides with empty graphs | Prepare the radio slides | Pack: spare snail, sensors, tools |
| 26 Oct | Travel (hand luggage: PCBs, Pi, SDR, camera; Li-ion batteries in carry-on only) | | |

## Recommendations
- **Main missions:** SnailPulse (Azizbek) plus ADS-B *or* Lloyd's mirror (Nurmuhammad), with the Insta360 as the attitude/proof instrument. One story: **"BioCube keeps a living passenger alive, watches its heart, and tests space radio and vision sensors on the way."** The biology does not need to shrink: the heartbeat sensor goes inside the snail box, and the radio payload needs ~0.3U.
- Get the **predictions on the slide before the flight** (numbers above). Theory points come from showing them against the measurements.
- **Drop** the rotor, Paschen, sound and heat-plate ideas as main missions. They can be done in a vacuum jar, and the organisers penalise "trivial" tests.

**Questions to send the organisers now:**
1. May we mount an Insta360 (~250 g) outside the 3U envelope, protruding ~4 cm at one end? May it be powered from the payload 5 V rail?
2. Is a 1090 MHz receive-only ADS-B receiver (RTL-SDR + Raspberry Pi) allowed on board in Russia, and may a 7 cm antenna protrude?
3. Can Stratonavtika give us the balloon's GPS track (time, lat, lon, alt) after the flight? Is there a swivel in the flight train, and how long is the line to our CubeSat?
4. What is the capacity (Wh) of the tournament 4S battery, and what is the maximum continuous payload power?
5. Are live animals (garden snail, earthworm) confirmed as allowed, and are there import/veterinary papers needed for bringing them from Uzbekistan? Or can we buy snails in Moscow?
6. May our ground station use a second receiver antenna on a 4 m mast at the launch site?
7. (Only if still considering it) Is any high-voltage (≤1 kV, µJ-level) discharge experiment allowed?

## Caveats
- **We could not find the actual experiments of the 2024 and 2025 Orbita winners.** Ask your national committee.
- The probabilities in the table are our engineering judgement, not measured statistics.
- The snail Q10 ≈ 2 and the RTL-SDR sensitivity are assumptions to verify in your ground tests.
- We did not confirm whether receiving ADS-B or carrying live snails across the border is legal; ask before buying.
- Insta360 cold-performance stories come partly from Insta360's own marketing.

## Sources

1. [Финал космического турнира «Орбита» 2025 - Space-π](https://spacepi.space/news/final-orbita-2025/)
2. [Турнир Орбита, сезон 2025 года — cikeno.ru](https://cikeno.ru/tournament-orbita-2025/)
3. [UniSat Nanosatellite Launch](https://unisat.uz/en/)
4. [Mass Media about us](https://unisat.uz/en/mass-media-about-us)
5. [ORBITA INTERNATIONAL COSMIC Tournament - spaceeducation.info](https://spaceeducation.info/en/orbita/)
6. [Школьник из Киселевска победил в международном космическом турнире «Орбита»](https://gorodok.bz/news/22313)
7. [Space-π на Международном космическом турнире «Орбита» - Space-π](https://spacepi.space/news/space-p-na-mezhdunarodnom-kosmicheskom-turnire-orbita/)
8. [В Екатеринбурге впервые проходит финал Международного космического турнира «Орбита» - Вести Урал](https://vesti-ural.ru/2024/05/30/225583/)
9. [Программа «Стратосферный спутник» - Space-π](https://spacepi.space/news/programma-stratosfernyj-sputnik/)
10. [Стратосферный спутник](https://stratosputnik.ru/)
11. [СтратоСТАРТ-2024 — Государственное бюджетное общеобразовательное учреждение "Детская академия творчества "Солнечный город" Министерства просвещения и науки Кабардино-Балкарской Республики](https://solnechnyjgorodkbr.ru/%D1%81%D1%82%D1%80%D0%B0%D1%82%D0%BE%D1%81%D1%82%D0%B0%D1%80%D1%82-2024/)
12. [Студенты ИТМО запустили спутник в стратосферу. Он исследовал, как пониженная гравитация влияет на формирование костной ткани](https://news.itmo.ru/ru/science/life_science/news/12929/)
13. [NASA Selects Winners of Third TechRise Student Challenge - NASA](https://www.nasa.gov/centers-and-facilities/armstrong/nasa-selects-winners-of-third-techrise-student-challenge/)
14. [Improved Mars Helicopter Aerodynamic Rotor Model for Comprehensive Analyses](https://rotorcraft.arc.nasa.gov/Publications/files/koning_AIAA_2019.pdf)
15. [(PDF) Mars Helicopter Technology Demonstrator](https://www.researchgate.net/publication/322311208_Mars_Helicopter_Technology_Demonstrator)
16. [Paschen Breakdown in a CO2 Atmosphere](https://digitalcommons.calpoly.edu/cgi/viewcontent.cgi?article=1120&context=aerosp)
17. [Design of an Attitude Control System for a High-Altitude Balloon Payload](https://www.researchgate.net/publication/346646057_Design_of_an_Attitude_Control_System_for_a_High-Altitude_Balloon_Payload)
18. [A Platform for Active Stabilization of High-Altitude Balloon Payloads in: Bulletin of the American Meteorological Society Volume 104 Issue 6 (2023)](https://journals.ametsoc.org/view/journals/bams/104/6/BAMS-D-22-0042.1.xml)
19. [The View from a 360-Degree Camera 100,000 Feet Above Earth](https://petapixel.com/2021/12/20/the-view-from-a-360-degree-camera-100000-feet-above-earth/)
20. [Karman Space Programme: Insta360 Captures Journey Toward Sustainable Space Travel](https://www.insta360.com/blog/creator-stories/karman-space-programme-insta360-space-travel.html)
21. [Jacob Boxerman - High-Altitude Balloons](https://jakebox.github.io/pages/balloons.html)
22. [Insta360 X4 - 8K 360 Camera](https://www.insta360.com/product/insta360-x4)
23. [Maximize Insta360 X4 Usage with Enhanced 2290mAh Battery](https://aceperipherals.com/products/insta360-x4-battery)
24. [Insta360 X4 FAQs: Everything You Need to Know](https://www.insta360.com/blog/tips/insta360-x4-faq.html)
25. [Weather Balloon Activity Helps Students Understand Ideal Gas Laws](https://www.researchgate.net/publication/290606975_Weather_Balloon_Activity_Helps_Students_Understand_Ideal_Gas_Laws)
26. [(PDF) An improved noninvasive method for measuring heartbeat of intertidal animals](https://www.researchgate.net/publication/236625131_An_improved_noninvasive_method_for_measuring_heartbeat_of_intertidal_animals)
27. [ELECTRICBLUE](https://electricblue.eu/pulse)
28. [Modulation of heart activity during withdrawal reflexes in the snail Helix aspersa - PubMed](https://pubmed.ncbi.nlm.nih.gov/15645297/)
29. [Modulation of heart activity during withdrawal reflexes in the snail Helix aspersa](https://link.springer.com/article/10.1007/s00359-004-0590-8)
30. [ISTSat-1](https://en.wikipedia.org/wiki/ISTSat-1)
31. [reflected gps signals: Topics by Science.gov](https://www.science.gov/topicpages/r/reflected+gps+signals)
32. [Soil Moisture Monitorization Using GNSS Reflected Signals](https://arxiv.org/pdf/0805.1881)
