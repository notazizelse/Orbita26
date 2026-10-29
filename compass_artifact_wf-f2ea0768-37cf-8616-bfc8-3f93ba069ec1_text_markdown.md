# BioCube Engineering Mission Plan: What the Competition Wants, and Which Technical Experiment to Fly

**Short answer:** You must build and fly a real, working CubeSat, and you must also have a clear mission (experiment) that the device carries out and reports results on. For the engineering mission, drop the RAM bit-flip experiment as your main goal: on a few-hour flight it will almost certainly show zero upsets. Fly two missions instead. For Azizbek: a radiation-altitude profile (the Regener–Pfotzer maximum) plus a fault-tolerant flight computer demo. For Nurmuhammad: a measured-versus-predicted LoRa link-budget experiment. Both reuse hardware you already plan to fly.

## TL;DR
- **Real device + mission, both.** The most likely match for your event is the **International Space Tournament "Orbita"** (Международный космический турнир «Орбита»). Its satellite-building competition gives teams IntroSat CubeSat 3U kits. Teams finish their satellites at the final, launch them into the stratosphere, and defend their mission results in front of experts. The 2026 finals are **October 27 – November 1, 2026**, about 3 weeks from today.
- **The RAM bit-flip idea is not realistic as the main mission.** My estimate: you would need about **2–3 gigabits of SRAM** to expect even 3 upsets in 3 hours at 20+ km. That is hundreds of chips. A Montana State University balloon (RadPC@Scale, 2022 paper) flew 4 Mbit of SRAM for **50 hours** at 70,000–85,000 ft, injected and recovered 2,836 test faults, and found **no** faults caused by cosmic radiation. Keep it only as a cheap bonus, with deliberate software fault injection.
- **Best two missions:** (1) **Azizbek:** a Geiger-counter radiation-vs-altitude curve that shows the Regener–Pfotzer peak, plus a watchdog/CRC/majority-vote "computer that survives errors" demo. (2) **Nurmuhammad:** an LoRa RSSI/SNR/packet-loss-vs-distance experiment that compares his link-budget prediction with real flight data. Both give a clear graph almost guaranteed, both cost very little, and both also help the biology mission.

## Key Findings

### 1. Which competition is this? (Most likely: "Orbita")
- **What Orbita is:** the International Space Tournament "Orbita" has these parts:
  - "team competitions in Satellite Engineering"
  - "test flights into the stratosphere"
  - an exhibition of projects
  - an International Space Education Forum
  - a separate "Cosmonautics Project Contest"\[1\]
- **Organisers:** the engineering-education company "Образование Будущего" (Education of the Future) and АНО ЦИКЕНО, supported by Roscosmos.\[2\]
- **Uzbekistan link:** Uzbekistan has its own national committee. The coordinator is listed as Nora Rakhimova (UniSat Uzbekistan / StartupFactory.uz).\[1\]
- **Wording matches yours:** "Соревнование по спутникостроению" is exactly the wording Orbita uses.
- **Uzbekistan has done well here:**
  - 2025: "Победителем соревнования по спутникостроению стала команда из Узбекистана" (a team from Uzbekistan won the satellite-building competition).\[2\]
  - 2024: Gorodok.bz (10 June 2024) reported that at the Yekaterinburg final "Победителями турнира стали команды из России и Узбекистана, набравшие одинаковое количество баллов" (Russia and Uzbekistan won with the same number of points).
- **What I could not confirm:**
  - A link to the "Robotics and Engineering Association of Uzbekistan".
  - A section called "№2 Human Spaceflight". The official page lists only the satellite competition, the forum and the Cosmonautics Project Contest. "Human Spaceflight" may be a topic inside the project contest, but treat this as unverified.

**2026 calendar (official page):**

| Stage | Dates |
|---|---|
| National stage of satellite building competition | May – August 2026 |
| Mission design by finalists | September 2026 |
| Tournament Finals | October 27 – November 1, 2026 |
| International Space Education Forum | October 31, 2026 |

- **Venue:** the 2026 Orbita runs alongside the first **International Space Games** at Skolkovo, Moscow, 26 October – 1 November.\[3\]
- **Who can take part:** it is "for school students aged 14–18 and university students". Teams come from Belarus, India, Kazakhstan, South Africa, UAE, Uzbekistan and Russia. They "will conduct experiments and prepare their designs for launch into the stratosphere."\[4\]

**What teams deliver, based on how past finals worked:**
1. **A real satellite.** It is built on the **IntroSat CubeSat 3U kit**:\[5\] "Arduino-compatible" **STM32** microcontrollers, 3.3 V main supply, one 18650 battery. Teams can freely add third-party electronics.\[6\]
2. **A real flight.** A stratospheric balloon (launched with the company Stratonavtika) takes it "up to 24 km" (2024) or "up to 20 km" (2025).\[2\]\[7\]
3. **A defence.** Teams present their results to an expert panel.\[5\]
   - The 2025 summary says it plainly: "all vehicles worked during the flight, but not all experiments achieved the planned results, which determined the outcome of the competition."\[2\]
   - **This is the most important lesson for you: judges reward an experiment that actually produces its result.**
4. **The 2025 final schedule** shows the rhythm: lab work on the satellites → launch day at Kirzhach airfield → processing data the same evening → defence the next day.\[1\] **You will have only a few hours to turn flight data into graphs**, so write your plotting scripts in advance.

**Not verified (the official PDF could not be opened):**
- payload mass limit
- exact radio frequency and power rules
- scoring criteria
- whether a fully custom PCB may replace IntroSat boards

**Guidance from a comparable competition.** The Russian "Air-Engineering School" (ВИШ / CanSat in Russia) **Stratospheric League** sets these rules:
- a research "pico-satellite" "weighing up to 1.5 kg" flown to 20–30 km on the organisers' balloon\[8\]
- data transmitted "по радиоканалу 433 МГц" (over a 433 MHz radio link)\[9\]
- an on/off switch\[9\]
- power for "не менее 1 часа" (at least 1 hour)\[9\]

In Russia, the licence-free LPD433 band (433.075–434.775 MHz) allows only **10 mW**.\[10\] Higher power needs an amateur licence or the organisers' permission. **Assume 433 MHz and low power until the organisers tell you otherwise.**

### 2. "Mission" vs "vehicle": simple explanation for Nurmuhammad
- **The vehicle (bus / платформа):** the real box that flies. It is the structure, power, onboard computer, radio, GPS and sensors, and it must work at −55 °C and 3% pressure. This is what you **build**.
- **The mission (payload / эксперимент):** the question the box answers. For example: "Do snails survive?", or "How does radiation change with altitude?" This is what you **prove**.
- **You need both, plus the result.** A perfect box with no clear question gets few points. A great question with a box that fails gets no data. Judges want: question → hypothesis → device that measures it → flight → graph → conclusion.
- **Your passport should say:** "Mission: X. Success criterion: we get graph Y showing Z."

### 3. The RAM bit-flip idea: honest numbers

**Starting data:**
- **Sea-level error rate:** published ground-level soft-error studies give typical SRAM rates "of the order of 1000 Failures in Time (FIT) per Mbit for SRAM at the 65nm technology node". 1 FIT = 1 error per 10⁹ hours.\[11\]
- **A real-world test:** Autran et al. (IEEE IRPS 2012) tested more than 7 Gbit of 40 nm SRAM for over 7,500 hours on the ASTEP platform (Plateau de Bure, French Alps, 2552 m) and measured **682 FIT/Mbit** from neutrons.
- **Neutron flux at altitude:** it rises about **420 times** between ground level and 20 km, at 45° latitude (NASA-Langley model).\[12\] Another study gives roughly ×300 between sea level and aircraft altitudes.\[13\]

**My estimate (rounded):**
- Error rate at about 20 km ≈ 1000 FIT/Mbit × ~400 ≈ 4×10⁵ FIT/Mbit ≈ **0.0004 upsets per Mbit per hour**.
- Time above 15 km on your flight ≈ 3 hours → **≈0.0012 upsets per Mbit per flight**.
- To expect **3 upsets** you need about 2,500 Mbit ≈ **2.5 Gbit ≈ 300 MB of SRAM**.
- The common 23LC1024 SPI SRAM is 1 Mbit. **Sixteen of them would give about 0.02 expected upsets**, so there is a 98% chance of seeing nothing.
- Modern DRAM/SDRAM has a lower per-bit soft-error rate than SRAM. One 256–512 Mbit SDRAM on the STM32 memory bus does not rescue the experiment, and STM32 internal SRAM is tiny (≤1 MB).
- Flash memory is far harder to flip.
- Lowering the supply voltage and using older chips helps by maybe 2–5×. That is not the ~100× you would need.

**A real flight agrees with the estimate.** Montana State University's RadPC@Scale flew **four Microchip 23LC1024 1-Mbit SRAMs** for "approximately 50 hours at altitudes between 70,000 feet to 85,000 feet".\[14\] The paper reports that over the 50-hour flight "the memory experiment injected 2836 faults and successfully recovered", and that there were no faults caused by cosmic radiation. All the faults it logged came from its own fault injector.\[14\] My estimate predicts about 0.08 upsets for that flight, which matches the zero they saw.

**Verdict:** you should not fly it as the main mission. Keep it as a **bonus** (see Recommendations). The honest "we expected about 0.02 upsets and saw 0" calculation is itself a good slide that shows judges you understand the physics.

### 4. Mission ideas compared

Ratings are my judgement for a 3-week to 1-month build on your existing STM32 + LoRa + sensor platform.

| # | Mission | Owner | Extra cost | Feasibility | Chance of a clear result | Visibility to judges | Main risk |
|---|---|---|---|---|---|---|---|
| 1 | Radiation vs altitude (Regener–Pfotzer peak), Geiger | Azizbek | Low (tube + HV board) | High | **Very high** | Very high: one clean curve | HV board fails in cold/low pressure |
| 2 | LoRa link budget: RSSI/SNR/packet loss vs distance & altitude | Nurmuhammad | Very low | High | **Very high** | High: predicted vs measured graph | Ground antenna pointing; frequency rules |
| 6 | Fault-tolerant OBC (watchdog, CRC, TMR, fault injection) | Azizbek | ~Zero | High | **Very high** (injected faults) | Medium–high | Judges may see it as "only software" |
| 4 | Thermal control: heater energy to keep snail at +15…+25 °C | Azizbek + Begoyim | Low | High | High | High: supports biology | Heater drains battery |
| 10 | GNSS vs altitude (satellite count, accuracy, airborne mode) | Nurmuhammad | Zero | High | High | Medium | Wrong dynamic model = lost fix |
| 7 | Atmosphere profile: T, P, humidity, tropopause, UV | Diyora/all | Low | High | High | Medium (very common) | Sensors out of spec when cold |
| 9 | IMU spin/swing, descent rate, landing shock | Azizbek | Zero | High | High | Medium | Little new science |
| 11 | Dose and UV history linked to biology outcomes | Diyora + Azizbek | Low | High | Medium | High: ties mission together | Only one sample, weak statistics |
| 3 | Solar cell power vs altitude/temperature | Nurmuhammad | Low | Medium | Medium | Medium | Payload spin makes the sun angle random |
| 5 | Battery chemistry in cold | Begoyim | Medium | Medium | Medium | Medium | Heated box hides the effect |
| 8 | Camera: horizon detection / NDVI on MCU | Azizbek | Medium | Low–Medium | Low–Medium | Very high if it works | Too hard for STM32 in 3 weeks |
| — | RAM bit flips (main mission) | Azizbek | Low–Medium | Medium | **Very low** | Low | Almost certainly zero events |

## Details: the strongest candidates

### Mission A: Radiation altitude profile (Azizbek)
- **Question:** At what altitude is ionising radiation strongest, and what dose did the biology samples receive?
- **Why it works:** Radiation rises with altitude to a peak, then falls.\[15\] Every proper flight that crosses the peak sees it:
  - near the equator, a mean RP height of **15.0 ± 0.32 km**\[16\]
  - at mid-latitudes (Czech/Swedish flights), **17–20 km**\[17\]
  - at Reading, UK, **20 ± 2 km**\[18\]
  - an Olin College / MA Space Grant team in 2025 "clearly showed the Regener-Pfotzer Maximum… at around 70,000 ft" with a custom PCB Geiger counter\[19\]
  - Middle-East flights in 2026 measured a peak of **~2.8–3.0 μSv/h** at 17–20 km\[20\]

  Your 22–24 km flight will pass through the peak, so the curve should appear. One caveat: near solar minimum, the peak can look flatter, as the Reading 2017 flights found.\[21\]
- **Hardware:** a common Geiger tube (for example SBM-20 or J305 on a ready "RadiationD"-type HV board). Count pulses with an STM32 timer input, and log counts per 10 s with pressure and GPS altitude. Optional upgrade: two tubes in **coincidence**, which counts only particles passing through both tubes. The Regener–Pfotzer discovery itself used three counters in coincidence.\[22\] Studies of the 2017 eclipse found the coincidence peak sits lower than the omnidirectional peak, which is a nice extra finding.\[23\]
- **Programmer's work:** an interrupt counter, timestamps, a dead-time correction, and a binned plot of counts per minute vs altitude for both ascent and descent.
- **Radio technician's work:** add the count rate to every LoRa packet so the curve builds live on the ground.
- **Final result:** one graph, counts/min vs altitude, with a peak marked "Regener–Pfotzer max ≈ X km". The bonus is a "radiation dose received by the snail" number for the biology section.
- **Risks:** the HV supply can arc at low pressure. Coat it in conformal coating or put it inside the 1 atm sealed volume. Test in a freezer.

### Mission B: LoRa link-budget experiment (Nurmuhammad)
- **Question:** Does our radio link behave as our link budget predicts, and how far does it reach from the stratosphere?
- **Prediction (my calculation, check it):**
  - Free-space loss at 433 MHz over 100 km ≈ 20·log₁₀(100) + 20·log₁₀(433) + 32.44 ≈ **125 dB**.
  - With 10 mW (+10 dBm) transmit power, a 0 dBi antenna on board, a ~6 dBi Yagi on the ground, and SF12/125 kHz sensitivity of about −137 dBm, the margin is about **28 dB**.
  - So even at 10 mW the link should work far beyond 100 km, line of sight.
  - According to The Things Network, the University of Zaragoza's "Diana I" balloon probe set a **766 km** LoRa record in July 2019 with a 25 mW node at 24,859 m (868 MHz), and Thomas Telkamp (Lacuna Space) later reached **832 km** from a balloon at 38 km in April 2020.
- **Experiment design:**
  - Every packet carries GPS position, a packet counter, and the onboard-measured RSSI of ground uplinks (if two-way).
  - The ground station logs RSSI, SNR and missing packet numbers.
  - Optional: alternate SF7 and SF10/12 every other packet to compare spreading factors.
  - Watch SNR, not only RSSI. One guide explains that if the SF limit is −10 dB and you receive at −4 dB, you have 6 dB of margin, which means about twice the distance.\[24\]
- **Hardware:** a second LoRa module (for example SX1278/RFM96 at 433 MHz) on the ground, plus a laptop and a simple Yagi or "moxon" antenna. An RTL-SDR dongle is optional for watching the spectrum.
- **Final result:** a graph of **measured RSSI vs distance with the predicted free-space line drawn on top**, plus packet-loss % vs distance. Add one number for the poster: "maximum range achieved: ___ km".
- **Risks:** antenna polarisation and the payload spinning cause deep fades. Log them and explain them; they are part of the science. Confirm the allowed frequency and power first.

### Mission C: Fault-tolerant flight computer (Azizbek, shares the board)
- **Question:** Can a cheap STM32 computer detect and repair memory and software errors by itself?
- **What to build:**
  - an independent watchdog with a reset-cause log (the STM32 RCC flags)
  - CRC-32 on every log record and every telemetry packet
  - critical variables (mission time, heater state, counters) stored **three times with majority voting**
  - a background "scrubber" that rewrites corrupted copies
  - a **software fault injector** that deliberately flips a random bit every N minutes

  Fault injection is exactly how RadPC proved its scrubber worked when space gave it no real upsets.\[14\]
- **Final result:** a table and graph of "injected faults: 180; detected: 180; corrected: 180; resets: 0". The real-radiation counter sits next to it, which is the RAM bonus.

### GNSS: a must-do, not just an experiment
- Set your u-blox module to **"Airborne <1g" dynamic model**. In default mode, receivers stop giving fixes at **about 12 km**: a NEO-6M flight lost fix at about 12 km for exactly this reason.\[25\]\[26\]
- The legal COCOM limit is **18 km and 515 m/s**. Some makers apply it as "or" instead of "and", which kills GPS above 18 km.\[27\]\[28\]
- Newer u-blox M8 parts are used by balloon groups without trouble once the model is set.\[29\]
- Logging satellite count and accuracy vs altitude is then a free mini-experiment for Nurmuhammad.

### Thermal energy experiment (strong link to biology)
Measure heater current (an INA219-type current sensor) together with inside and outside temperature. This lets you report "watt-hours needed to keep a snail alive at −55 °C". Comparing on/off vs PID control within one flight is hard, because you have only one flight. A simpler version is to log the duty cycle and energy, and compare them with a ground freezer test.

### How other teams did it
- **Orbita 2024 and 2025:** teams built IntroSat 3U CubeSats with their own experiments, flew them to 20–24 km, and defended the results the next day. Uzbekistan won in 2025.\[2\]\[5\]\[7\]
- **NTI "Stratospheric satellite" final:** 15 teams assembled CubeSats, designed their own payload and experiment, and launched them on sounding balloons "to about 30 km".\[30\]
- **University of Reading:** Geiger counters and a PiN-diode detector on radiosondes produced the radiation profile.\[21\]
- **India:** low-cost weather-balloon flights near the Tropic of Cancer measured the RP height.\[16\]
- **Montana State University RadPC:** SEU-mitigation tests on balloons and HASP.\[14\]\[31\]
- **Thailand:** a yeast biology payload launched on May 4, 2023 from Wangchan Valley, Rayong, reached 29,364 m (Heliyon, 2024). The authors say UV "almost completely eliminated" the yeast, which is a strong reason to fly a UV and dose sensor beside your biology samples.
- **Ham-radio balloon trackers:** a published LoRa balloon-tracking toolkit uses u-blox M8 in Airborne mode, and also warns that the common MPL3115A2 pressure sensor becomes inaccurate above about 15 km.\[29\] **So use GPS altitude for your graphs.**

## Recommendations

**1. Primary mission for Azizbek: "Radiation and a Fault-Tolerant Computer"**
- *Passport sentence:* "We will measure how cosmic radiation changes from 0 to 24 km and find the altitude of maximum radiation (the Regener–Pfotzer maximum), using a self-checking flight computer that detects and repairs its own memory errors."
- *Parts:* 1–2 Geiger tubes + HV board, STM32 timer input, microSD, ~2–4 × 23LC1024 SRAM (bonus).

**2. Primary mission for Nurmuhammad: "Stratospheric Radio Link Budget"**
- *Passport sentence:* "We will predict our 433 MHz LoRa link budget and then test it in flight by measuring signal strength, signal-to-noise ratio and packet loss versus distance and altitude up to 24 km."
- *Parts:* onboard LoRa (already planned), a second LoRa module + Yagi/moxon on the ground, laptop logging script, u-blox GNSS in Airborne mode.

**3. Shared support mission (optional, cheap): "Life-support energy and dose"**
- Heater energy, temperature, UV and radiation dose logged for each biomodule. These give Diyora's biology results a physical explanation.

**4. RAM bonus, done cheaply and honestly**
- Add 2–4 23LC1024 SPI SRAMs filled with a known pattern (alternate 0x55/0xAA). Scan them every 10 s, and count and log any flipped bits with altitude.
- State in advance: "expected ≈0.01 upsets; a zero result is the expected outcome and confirms the calculation."
- Pair it with the software fault injector so the detection code is **proven** to work.

**5. Questions to ask your mentor and organisers now**
1. Is our event the Orbita International Space Tournament 2026 (finals 27 Oct–1 Nov, Skolkovo)? What exactly is the "№2 Human Spaceflight" section, and which section is BioCube entered in?
2. Must we use the IntroSat kit's boards, or may our custom STM32 PCB replace or add to them? Which bus and connector must it use?
3. What are the maximum mass, size, and power or battery rules? Is 340 mm for 3U fine?
4. Which radio frequency, power and modulation are allowed? Is LoRa at 433 MHz OK? Who provides the ground station?
5. How long is the flight, and what is the planned maximum altitude (20 or 24 km)? Do we get the hardware back the same day?
6. Are live animals allowed on the balloon? Is there an ethics or safety review?
7. How are the scores split between the vehicle working, the experiment result, and the presentation?
8. When must the project passport be submitted, and is there a pre-flight technical check?

## Caveats
- **Unverified rules.** I could not open the official 2026 Regulations PDF ("Regulations-of-the-International-Tournament-Orbita-3.pdf", updated 25.06.2026).\[1\] Mass limits, frequencies, scoring and the custom-PCB rules are therefore unverified. Get the PDF from your mentor or the Telegram channel t.me/orbitacontest.
- **Estimates, not measurements.** The SEU numbers are order-of-magnitude estimates built from published sea-level FIT rates and the neutron flux ratio. Real chips can differ by 10×. Even 10× better does not change the conclusion.
- **Launch latitude matters.** The launch site is probably near Moscow, around 55°N, where cosmic-ray flux is somewhat higher than at Tashkent. That shifts the RP peak and the count rates a little, but not the shape of the curve.
- **My own calculation.** The link-budget figures are my calculation using typical SX127x sensitivity. Nurmuhammad should redo them with your exact module datasheet and antennas. That redo is part of his mission.
- **A possible related repo.** A public GitHub repo, "notazizelse/Orbita26" ("A CubeSat PCB for the international Orbita competition 2026 season"), appeared in research.\[32\] If it belongs to your team, check that its stated "2026 technical requirements" match the official PDF.

## Sources

1. [ORBITA INTERNATIONAL COSMIC Tournament - spaceeducation.info](https://spaceeducation.info/en/orbita/)
2. [Турнир Орбита, сезон 2025 года — cikeno.ru](https://cikeno.ru/tournament-orbita-2025/)
3. [В России пройдут первые Международные космические игры](https://tvspb.ru/news/2026/09/24/v-rossii-projdut-pervye-mezhdunarodnye-kosmicheskie-igry)
4. [Teams from Eight Countries to Take Part in First International Space Games in Moscow — HSE University](https://www.hse.ru/en/news/campus/1205060883.html)
5. [В Екатеринбурге прошел финал Международного космического турнира «Орбита» - Новости. Ключ на старт. Космос для детей](https://space4kids.ru/3023/)
6. [Конструкторы и наборы Интросат](https://introsat.ru/ru/space-education-kits-all/space-education-kits)
7. [Space-π на Международном космическом турнире «Орбита» - Space-π](https://spacepi.space/news/space-p-na-mezhdunarodnom-kosmicheskom-turnire-orbita/)
8. [Стратосферная лига — Воздушно-инженерная школа](https://roscansat.com/championship-leagues/napravlenie-apparatyi/stratospheric-league/)
9. [Приложение 2. ТЕХНИЧЕСКИЕ ЗАДАЧИ И ТРЕБОВАНИЯ](https://roscansat.com/wp-content/uploads/2021/11/Prilozhenie-2.-TZ.pdf)
10. [Частоты рации LPD и PMR: таблица каналов и как настроить рацию правильно](https://artelv.ru/kak-nastroit-raciyu-tablica-chastot-lpd433-i-pmr446-instrukciya-i-kanaly)
11. [SRAM Soft Error Rate Evaluation Under Atmospheric Neutron Radiation and PVT variations](https://www.researchgate.net/publication/261382181_SRAM_Soft_Error_Rate_Evaluation_Under_Atmospheric_Neutron_Radiation_and_PVT_variations)
12. [Neutrons flux in atmosphere as function of altitude-at latitude of...](https://www.researchgate.net/figure/Neutrons-flux-in-atmosphere-as-function-of-altitude-at-latitude-of-45-NASA-Langley_fig2_224569278)
13. [Soft-errors induced by terrestrial neutrons and natural alpha-particle emitters in advanced memory circuits at ground level](https://www.researchgate.net/publication/223089493_Soft-errors_induced_by_terrestrial_neutrons_and_natural_alpha-particle_emitters_in_advanced_memory_circuits_at_ground_level)
14. [A Novel Approach to the RadPC Single Event Upset ...](https://www.montana.edu/blameres/vitae/publications/d_conference_full/conf_full_054_radpc_at_scale_mar22.pdf)
15. [Shielding from Cosmic Radiation: Part 3 - Post-Launch Analysis - Activity - Teach Engineering](https://www.teachengineering.org/activities/view/uow-2546-post-launch-analysis-cosmic-radiation-activity3)
16. [Measurement of secondary cosmic ray intensity at Regener-Pfotzer height using low-cost weather balloons and its correlation with solar activity](https://arxiv.org/pdf/1707.00275)
17. [Latitudinal effect on the position of Regener–Pfotzer maximum investigated by balloon flight HEMERA 2019 in Sweden and balloon flights FIK in Czechia](https://www.researchgate.net/publication/374672784_Latitudinal_effect_on_the_position_of_Regener-Pfotzer_maximum_investigated_by_balloon_flight_HEMERA_2019_in_Sweden_and_balloon_flights_FIK_in_Czechia)
18. [Comparison between count rates obtained at the Regener–Pfotzer maximum...](https://www.researchgate.net/figure/Comparison-between-count-rates-obtained-at-the-Regener-Pfotzer-maximum-as-identified_fig5_265015945)
19. [STORY: Measuring Cosmic Rays with High-Altitude Balloons](https://www.olin.edu/articles/story-measuring-cosmic-rays-high-altitude-balloons)
20. [Cosmic Radiation Dose‐Rates for Civil Aviation in the Middle East: Vertical Structure, Solar Modulation, and Comparison With Models - Yaniv - 2026 - Journal of Geophysical Research: Atmospheres - Wiley Online Library](https://agupubs.onlinelibrary.wiley.com/doi/10.1029/2026JD047316)
21. [Miniaturized atmospheric ionization detector](https://arxiv.org/pdf/1804.07532)
22. [The highest energy cosmic rays: the past, the present and the future](https://arxiv.org/pdf/1901.06676)
23. [(PDF) The Regener-Pfotzer Maximum During a Total Solar Eclipse](https://www.researchgate.net/publication/353786015_The_Regener-Pfotzer_Maximum_During_a_Total_Solar_Eclipse)
24. [SX12XX-LoRa/What is LoRa.md at master · StuartsProjects/SX12XX-LoRa](https://github.com/StuartsProjects/SX12XX-LoRa/blob/master/What%20is%20LoRa.md)
25. [Receiver-Imposed Altitude Limits on GPS Functionality ...](https://www.techrxiv.org/doi/pdf/10.36227/techrxiv.176072168.80486938/v1?onload=true)
26. [Getting started with Ublox Part 1](https://ava.upuaut.net/?cpage=1&p=738)
27. [NEO M8N Altitude configuration.](https://portal.u-blox.com/s/question/0D52p00008HKCZICA5/neo-m8n-altitude-configuration)
28. [High Altitude GPS operation](https://shop.bigredbee.com/blogs/news/high-altitude-gps-operation)
29. [An open source toolkit for the tracking, termination and recovery of high altitude balloon flights and payloads](https://arxiv.org/pdf/1904.04321)
30. [Финалисты всероссийской программы в Подмосковье запустят в стратосферу 15 спутников - ТАСС](https://tass.ru/kosmos/11721993)
31. [LaMeres 1 31th Annual AIAA/USU Conference on Small Satellites SSC17-III-13](https://www.montana.edu/blameres/vitae/publications/d_conference_full/conf_full_037_radsat_next_on_pad.pdf)
32. [GitHub - notazizelse/Orbita26: A CubeSat PCB for the international Orbita competition 2026 season · GitHub](https://github.com/notazizelse/Orbita26)
