# ORBITA 2026 — Technician's Mission Research Dossier
### Deep research, mission concepts, and a buildable engineering plan for the team electronics/embedded lead

**Prepared:** 18 September 2026
**For:** Azizbek Nomonjonov — Programmer / Electronics (team technician)
**Team context:** Uzbekistan national team, mentor Ezoza Yuldosheva
**Hard deadline:** 26 October 2026 (Tournament Final, per official Regulations) — **38 days from today**
**Evidence base:** official ORBITA 2026 Regulations, ORBITA evaluation rubric (2025 edition), IntroSat.Platform guides (RU + EN), ORBITA25 organiser briefing, your team's three mission drafts, plus ~25 external technical sources cited in §21.

---

## 0. EXECUTIVE SUMMARY — read this first

**The single most important fact in this entire document:** the published ORBITA scoring rubric puts **27 of 60 points (45%)** on *the payload working in flight and producing a complete, correctly-analysed dataset*, and only **5 of 60 points (8%)** on scientific novelty. A clever experiment that returns partial data scores **worse** than a modest experiment that returns a complete dataset. Design accordingly.

**Recommended mission: "SKIN-24" — a CubeSat that measures what the stratosphere does to its own body.**

A student-built 3U CubeSat is assembled at +20 °C and flown at −55 °C. Nobody measures what that does to the structure. A 300 mm PLA side panel bolted to an aluminium frame wants to shrink **0.97 mm more than the frame does** over that temperature drop. That differential goes somewhere — into panel bow, into screw preload, into board stress. Meanwhile natural convection, the only thing cooling your electronics on the ground, **collapses to ~19% of its sea-level strength** by 24 km, so every self-heated component runs far hotter than its ground test suggested.

SKIN-24 instruments one student-made side panel with a back-to-back strain-gauge array (the "space skin"), a piezo structural microphone, a distributed temperature network, and three self-heated calorimeter elements. It reconstructs the panel's bending shape live on the ground station in 3D, tracks the panel's first bending mode as its stiffness changes with temperature, and measures the altitude-dependence of convective cooling.

**Why this wins on the actual rubric:**

| Rubric line | Max | Why SKIN-24 scores |
|---|---|---|
| Scientific significance | 5 | Real, under-studied engineering question; not a repeat demo |
| Theoretical elaboration | 8 | Closed-form thermo-elastic model + NASA Ko displacement theory + plate modal theory, all with numbers |
| Technical elaboration | 8 | Uses the platform's own OPAMP/ADC/DFSDM peripherals; every component justified against −55 °C and 29 hPa |
| Assembly | 4 | Payload is a flat PCB + one instrumented panel — no loose mass, no tape |
| Ground testing | 8 | Every single channel is testable on a desk with a freezer, a caliper and a vibration motor |
| **Flight performance** | **15** | **Passive, solid-state, no moving parts, no consumables, no living cargo. Three independent data paths (SD + LoRa + redundant channel). Almost nothing can stop it returning data.** |
| **Results analysis** | **12** | **Produces continuous curves against altitude with a falsifiable hypothesis, not a single before/after comparison** |

**Cost:** ~$45–70 in parts, most of it available in Tashkent. **Build time:** working prototype in 5–7 days, flight unit in 3 weeks. **Every part of it is testable in a kitchen freezer.**

**Critical warnings surfaced by this research (details in §3):**

1. **Your biology missions (snail / earthworm / DNA) cannot score the 15 flight points on their own** — they generate no in-flight data. They are "recover and analyse later" experiments. They need a technician-built environment-logging module to become scoreable, and two of them need a heated pressure vessel (~12 Wh of a ~37 Wh battery budget) plus organiser approval for live animals.
2. **Your entire service stack is rated only to −40 °C** (LSM6DS3, LIS3MDL, MS5611, STM32H750, E32 radio) while stratospheric ambient is **−52 to −57 °C**. Thermal design is not optional.
3. **The "Engineer / CAD designer" seat in your team roster is empty.** The Regulations require a 4-member team with the roles engineer, programmer, researcher, radio technician. Fix this before anything else.
4. **Parts shipping is your critical path, not your firmware.** AliExpress to Tashkent is typically 2–4 weeks. Order in the next 48 hours or buy locally only.

---

## 1. ORBITA COMPETITION REQUIREMENTS
*(Documented from the official Regulations PDF, the official evaluation-criteria document, the organiser briefing deck, and the IntroSat platform guides. Uncertainties are flagged explicitly.)*

### 1.1 What the competition physically is

ORBITA is **not an orbital mission**. It is a **high-altitude balloon flight of a 3U CubeSat-format payload**, operated by the stratospheric flight operator **Stratonavtica**, during the finals week. Everything below follows from that.

| Parameter | Value | Source & confidence |
|---|---|---|
| Form factor | CubeSat 3U (100 × 100 × 340.5 mm envelope class) | Regulations §6.6.4; IntroSat guide. **Certain** |
| **Mass limit** | **≤ 3.0 kg including payload** | Participant Guide: *"масса аппарата с полезной нагрузкой не должна превышать 3 килограмма"*. **Certain** (note: this is *below* the CDS 3U limit of 4–6 kg) |
| Platform | Free **IntroSat.Platform 3U kit, without payload** | Regulations §6.6.4. **Certain** |
| Peak altitude | **"up to 24 km"** (Regulations); IntroSat guide says "up to 25 km"; organiser deck says "20–35 km"; 2025 flights actually reached **~20 km** | **Contradiction — see §3.4.** Design for 20 km, stretch to 24 km |
| Flight duration | Ascent 60–90 min @ ~5 m/s → 10–15 min at peak → burst → 30–40 min under parachute. **~2–2.5 h total** | ORBITA25 organiser deck. **High confidence** |
| Ambient temperature | **−70 °C to +20 °C** (organiser figure); ISA at 20–24 km gives **−52 to −57 °C** | Organiser deck + ISA. Design margin: assume −65 °C skin |
| Ambient pressure | **< 1% of sea level** (organiser); ISA: 54.7 hPa @ 20 km, **29.3 hPa @ 24 km** | Organiser deck + ISA. **High confidence** |
| Radiation | **100–200× sea-level** (organiser figure) — this is the Regener–Pfotzer maximum region | Organiser deck; corroborated by balloon literature. **High confidence** |
| Landing | Parachute; **possible water/swamp landing**; recovery by organisers | ORBITA25 deck. **Certain** |
| Recovery guaranteed? | **Not stated.** Water landing is explicitly listed as possible | **UNCERTAIN — treat as a design driver** |

### 1.2 Team, eligibility, roles

- **4 members exactly**: four students aged 12–18, or three aged 12–18 plus one university student ≤ 21 (Regulations §6.1–6.2).
- **Mandatory role split, one member each: engineer, programmer, researcher, radio technician** (§6.2).
- One adult mentor per team; **the mentor is explicitly forbidden from working on the project** (§6.3.2).
- Attachment 1 defines the expected technician competencies: *C/C++ basics, STM32 microcontroller programming, Arduino IDE, UART/I²C/SPI interfaces, basic electrical engineering* for the Electrical Engineer; *C/C++, Python, STM32, radio communication* for the Radio Engineer.

### 1.3 Timeline (2026 cycle)

| Date | Event | Source |
|---|---|---|
| 1 Jun – 10 Sep 2026 | National phase (satellite competition) | Regulations §9 |
| 10 – 20 Sep 2026 | **CubeSat kit distribution** | Regulations §9 |
| **10 Sep – 26 Oct 2026** | **Mission development window** | Regulations §9 |
| **26 Oct – 1 Nov 2026** | **Tournament Final** (launches + defence) | Regulations §9. *Website says 27 Oct – 1 Nov — use 26 Oct* |
| 31 Oct 2026 | International Space Education Forum | Website |

**You are on day 8 of a 46-day development window, with 38 days left.**

Mandatory during the window: **≥3 general webinars** and **≥3 individual consultations** per finalist team, plus **mandatory online assignments**. Regulations §6.6.11: *failure to complete assignments without a valid reason **results in disqualification***. Treat these as higher priority than any engineering task.

### 1.4 Scoring — the actual rubric

The 2025 evaluation-criteria document (linked by QR code from the organiser briefing deck) defines **60 points total**:

| # | Criterion | Max | Top-band wording |
|---|---|---|---|
| 1 | Scientific significance | **5** | *"aimed at solving a significant and relevant problem, while the idea itself is completely or partially new"* |
| 2 | Theoretical elaboration | **8** | *"full theoretical description… justification of the selected methods and approaches"* (7–8 pts requires **mathematical models and calculations**) |
| 3 | Technical elaboration | **8** | *"suitable electronic components and materials… all technical limitations and special conditions of the experiment have been taken into account"* |
| 4 | Spacecraft assembly | **4** | *"securely fastened without using the simplest methods (duct tape, tape, glue, etc.) without obvious necessity"* |
| 5 | Ground testing | **8** | *"a full cycle of tests has been carried out, all systems work properly"* |
| 6 | **Stratospheric flight performance** | **15** | *"all basic systems and payloads operated normally, and a **full amount of data** was collected"* |
| 7 | **Analysis of results** | **12** | *"the full array of data was correctly processed, reliable results obtained, **reasoned conclusions confirming or refuting the hypothesis**"* |

> ⚠️ **UNCERTAINTY — READ CAREFULLY.** The 2026 Regulations §7.10 state that the finals criteria *"will be published by the Organizers on the first working day of the finals."* The 60-point rubric above is the **2025 edition**, obtained from the organiser's own briefing material. It is the best available evidence and the categories are likely stable, but **do not treat the exact point values as guaranteed for 2026.** Ask your organiser contact directly — this is a legitimate, easy question and worth doing this week.

### 1.5 What the rubric actually rewards (strategic reading)

Four non-obvious conclusions fall out of the numbers:

1. **Novelty is nearly worthless (5/60 = 8%).** Chasing a "never been done" idea at the cost of reliability is a losing trade. The rubric's own wording only asks that the idea be *"completely or partially new"* — partial novelty is enough for full marks.
2. **Flight performance (15) is the single biggest line and it is almost entirely an engineering-discipline score.** *"A full amount of data was collected"* is what separates 13–15 from 10–12. Redundant storage paths and graceful degradation are worth more than any sensor.
3. **Analysis (12) requires a hypothesis that can be confirmed *or refuted*.** An experiment that only says "we measured X" caps out mid-band. You need a prediction with a number attached, and a dataset dense enough to test it.
4. **Ground testing (8) is free points that most teams lose.** *"A full cycle of tests… all systems work properly"* — you get these points for process, on a desk, before you ever fly. §14 of this document gives you that cycle.

Corroborating evidence from ORBITA 2025: the organisers reported that *"all apparatus functioned during flight, though not every experiment achieved its planned results."* The platform is reliable; **payloads are what fail.** That is precisely where your 15 points live.

### 1.6 What is NOT specified (genuine unknowns — ask the organisers)

These are not in any document I could obtain. Each has a real design consequence, so raise them in your consultation sessions:

| Unknown | Why it matters to you | Suggested question |
|---|---|---|
| Payload power budget / battery capacity | Determines whether heaters are viable | *"What is the capacity and nominal discharge profile of the heated 4S1P pack issued at the finals?"* |
| Whether recovery is guaranteed | Determines whether SD-only data is acceptable | *"If the payload lands in water, is the SD card expected to be recoverable?"* |
| Permitted LoRa channel | Frequency coordination with the launch operator | *"Which channel(s) will the launch team use, and which are reserved for teams?"* |
| Rules on live organisms | Blocks/unblocks your Missions 1 and 2 | *"Are live animals permitted as payload, and what containment is required?"* |
| Rules on chemicals, pyrotechnics, pressurised vessels, high voltage, lasers, magnets | Blocks several concepts in §6 | *"What payload categories are prohibited?"* |
| Whether the payload may be powered before/after the flight window | Determines pre-launch soak and post-landing logging | *"When is the remove-before-flight pin pulled, and for how long is the payload powered?"* |
| Exact side-panel mounting geometry / removable-pin location | Blocks final CAD of the instrumented panel | *"Where should the remove-before-flight pin and antenna cut-outs be located?"* (the IntroSat guide itself says to ask this) |

Regulations §7.13 says conduct and safety rules are also **published only on the first working day of the finals.** Do not build anything whose legality depends on a rule you have not read.

---

## 2. TECHNICIAN CONSTRAINTS — what the platform actually lets you build
*(Source: IntroSat.Platform краткое руководство + short guide EN. These two documents are the authoritative hardware reference and agree with each other throughout.)*

### 2.1 The platform, precisely

**Stack (top → bottom):** Power Module → Communication board (CDAM) → Flight Controller Module (FCM) → Interface Module. All boards **76 × 76 mm**, 65 mm mounting-hole pitch, joined by the **IntroBus** connector pair.

**Flight Controller Module (FCM)**
- MCU: **STM32H750VBT6** (Cortex-M7, 480 MHz capable, 128 kB flash / 1 MB RAM — note the small flash)
- Service sensors on **I²C1 (PB7/PB6), payload access prohibited**:
  - LSM6DS3 accel+gyro @ 0x6A (defaults: 104 Hz, ±4 g, ±500 dps; INT1 → PA15)
  - LIS3MDL magnetometer @ 0x1C (±16 gauss; INT → PB3, DRDY → PB4)
  - MS5611 barometer @ 0x77 (**10–1200 mbar**, OSR 1024 → ±0.027 mbar, ±0.005 °C)
  - LM75A temperature @ 0x4A (−55…+125 °C, 0.125 °C resolution; OS → PE2)
- Storage: **microSD over SDMMC2** — D0–D3 = PC8/9/10/11, CMD = PD2, CK = PC12, **CD = PB8**
- Interfaces on board: RS485 driver ADM3485EARZ, CAN driver MAX3051, CH340X USB-UART on USB-C (UART2 = PA2/PA3)
- Programming: **SWD via the PLS-5R "PROG" header** with an ST-Link; BOOT0 jumper must be in position 0 for normal run
- User LED on **PE10**; RESET button

**Communication board (CDAM)**
- Its own STM32H750VBT6 + **Ebyte E32-433T30D** LoRa module + LM75AD @ 0x4C
- Band **410–441 MHz**, 1 MHz steps, **channels 0–31**, f = 410 + C MHz
- TX power settable **30 / 27 / 24 / 21 dBm** (`AT+LTXPWR`)
- Air rate settable **300 / 1200 / 2400 / 4800 / 9600 / 19200 baud** (`AT+LAIRRATE`), **default 2400**
- Connected to FCM **UART1 (PA9/PA10)** through IntroBus_S — anything you print to `Serial1` goes over the air
- **Hard constraints:** the radio fragments into **58-byte packets** with an inter-packet delay; the board buffers **up to 4096 bytes**; it returns **ASCII ACK 0x06** when a burst has been sent
- **Never transmit without the antenna connected — it destroys the module**

**Power module (always top of stack)**
- Input **XT30-Male**: 4S1P Li-ion pack @ 14.8 V nominal (issued at the tournament, **with heaters**) or a 12 V bench supply during preparation
- Payload rails, each enabled by a **jumper on X6** and indicated by an LED:

| Rail | Max current | Notes |
|---|---|---|
| **14.8 V unregulated** | **6 A** | Via angled XT30-Female X8. Ranges **12.8–16.8 V** as the pack discharges — add your own regulator if your payload needs a fixed rail |
| **+5V_IS** | **3 A** | On IntroBus_L |
| **+3V3_IS** | **3 A** | On IntroBus_L |

Exceeding these currents **auto-disconnects the channel**. Never power the stack from two sources at once; disconnect the ST-Link 3.3 V line when the power module is attached.

### 2.2 The payload interface — your entire world

**IntroBus_L (J5), 2×20 pins.** This is the only electrical interface you get, and it is unusually generous for an educational kit:

| Left column | MCU pin | Right column | MCU pin |
|---|---|---|---|
| GND | — | GND | — |
| +3V3_IS | — | +3V3_IS | — |
| **OPAMP_OUT** | PE7 | **ADC** | PB0 |
| **OPAMP_INM** | PE8 | **ADC** | PB1 |
| **OPAMP_INP** | PE9 | MOSI2 | PB15 |
| ADC / DFSDM_CKIN | PC0 | MISO2 | PB14 |
| ADC / DFSDM_DATIN | PC1 | SCK2 | PB10 |
| ADC / DFSDM_CKOUT | PC2_C | RX8 / RS485-A | PE0 |
| ADC | PC3_C | TX8 / RS485-B | PE1 |
| COMP_INM | PC4 | RX3 | PD9 |
| COMP_OUT | PC5 | TX3 | PD8 |
| HRTIM_CHA1 | PC6 | RX5 | PB12 |
| HRTIM_CHA2 | PC7 | TX5 | PB13 |
| COMP_INP | PB2 | **SDA4** | PD13 |
| DAC_EXT | PB9 | **SCL4** | PD12 |
| DAC_OUT1 | PA4 | CAN_H | — |
| DAC_OUT2 | PA5 | CAN_L | — |
| +5V_IS | — | +5V_IS | — |
| +3V3_IS | — | +3V3_IS | — |
| GND | — | GND | — |

The guide states explicitly that the payload connector exposes the MCU's **ADC, DAC, COMP (comparator), OPAMP (operational amplifier), DFSDM (sigma-delta digital filter) and HRTIM (high-resolution timer)**, plus **I²C4, SPI2, UART3/5/8, RS485 and CAN**. Any pin can also be plain GPIO.

> **This is the most important technical fact in the document for mission selection.** A payload connector that brings out a *built-in instrumentation-grade operational amplifier, a comparator, a DAC and a sigma-delta modulator interface* is a connector designed for **analogue sensor work**. Most teams will use it as six GPIOs and an I²C bus. Using OPAMP + ADC + DFSDM as a real analogue front end is exactly the kind of thing the rubric's *"technical elaboration — suitable electronic components… all technical limitations taken into account"* band is describing.

**Mechanical envelope:**
- Service stack occupies the **top 1U**. **Payload gets the bottom 2U** (~100 × 100 × 200 mm usable).
- **Side panels and intermediate frames are built by the students themselves** (*"Боковые панели и промежуточные шпангоуты изготавливаются учащимися самостоятельно"*). One panel must carry cut-outs for the remove-before-flight pin, antenna mount and reset-button access.
- Frame: 3U rails + two bar types, M2.5 screws; boards on **M2.5×15 brass standoffs**.
- **Pre-flight wiring rule:** all payload wiring must be **soldered or use locking connectors (JST, micromatch, terminal blocks)**. **PLS-2 pin headers are explicitly NOT allowed for flight.** Breadboard jumpers are prototyping only.

> **Second most important fact:** *you build the side panels*. The structure you would be instrumenting is a structure you are **required to fabricate anyway**. A structural-health payload therefore costs you almost no extra mechanical work — it rides on a deliverable you already owe.

### 2.3 Software environment

| Item | Value |
|---|---|
| IDE | **Arduino IDE** + `stm32duino` board package + **STM32CubeProgrammer** |
| Platform library | **IntroStratLib** (classes `LSM6DS3`, `LIS3MDL`, `MS5611`, `LM75A`) |
| Storage libs | `STM32SD` + `FatFs`; requires `SD.setDx(PC8,PC9,PC10,PC11); SD.setCMD(PD2); SD.setCK(PC12);` and `#define SD_DETECT_PIN PB8` |
| Serial objects | `HardwareSerial Serial1(PA10,PA9); Serial3(PD9,PD8); Serial5(PB12,PB13); Serial8(PE0,PE1);` |
| I²C objects | `TwoWire Wire1(PB7,PB6);` (service) — `TwoWire Wire4(PD13,PD12);` (**payload**) |
| SPI object | `SPIClass SPI_2(PB15,PB14,PB10);` any free pin as CS |
| Language | C++ (Arduino flavour). The Regulations' competency list names C++ explicitly |

**Firmware constraints you must respect:**
- `while(!SD.begin());` in the platform's own example **blocks forever if no card is present.** Never ship that. (See §15.)
- STM32H750 has only **128 kB of internal flash** — the Arduino core plus SD plus your payload code will fit, but do not plan on large lookup tables in flash.
- Arduino `String` concatenation on an H7 with a 1 Hz telemetry loop is survivable, but binary packing (§12) is both faster and what the platform guide itself recommends.

### 2.4 Hard limits that constrain every idea in §6

| Limit | Value | Consequence |
|---|---|---|
| Mass | 3.0 kg total | Rules out heavy mechanisms, thick metal, large batteries |
| Payload volume | 2U (100×100×200 mm) | Rules out large optics, long booms |
| Payload power | 3 A @ 3.3 V, 3 A @ 5 V, 6 A @ 14.8 V | Generous — power is **not** your constraint; energy is |
| Energy | 4S1P pack, **capacity unknown**, ≈ 37 Wh if 2.5 Ah | Heaters are expensive; 12 Wh for a bio-module is ~⅓ of the budget |
| Downlink | LoRa 58-byte packets, 2400 bps default | ≈ **1 packet/second sustainable**; see §12 |
| Temperature | **All service silicon is −40 °C rated**; ambient −52…−57 °C | Thermal design mandatory. microSD is the weakest link |
| Wiring | Soldered or locking connectors only | No breadboards, no PLS-2 headers in flight |
| Time | **38 days**, minus mandatory webinars/assignments | Anything longer than a 3-week build is fiction |

---

## 3. YOUR LOCAL RESOURCES — what they say, and where they contradict the official rules

### 3.1 Inventory

| File | What it is | Authority |
|---|---|---|
| `ORBITA25.pdf` | Organiser briefing deck, 17 slides, 2025 edition. Contains flight-profile figures, role recommendations, experiment-selection guidance, and **two QR codes linking to the official evaluation criteria and participant guide** | **High** — organiser-produced |
| `IntroSat.Platform_ краткое руководство.pdf` (78 pp) | Full Russian platform manual | **Definitive** for hardware |
| `IntroSat.Platform_ short guide 2.pdf` | Abridged English version. Consistent with the Russian, but omits the kit spec, mechanical assembly, LoRa AT-commands, power-scheme detail, and the 2U payload-volume statement | High, but **use the Russian for anything load-bearing** |
| `International Space Tournament Orbita.docx` | Your team roster + two empty brainstorm tables (biological / technical), each seeded with an "ozone profile" example | Team working doc |
| `Mission 1.docx` | Live garden snail (*Cornu aspersum*) to ~24 km; post-flight mucus analysis | Team draft |
| `Mission 2.docx` | Live earthworm + soil to ~24 km; post-flight soil-function analysis | Team draft |
| `Mission 3.docx` | **Titled "Mission 4"** — dried bacterial DNA to ~24 km; post-flight integrity + functional assay | Team draft |

### 3.2 What the organiser deck tells you that the Regulations don't

From `ORBITA25.pdf`, the flight profile is broken into three phases with distinct engineering signatures:

1. **Ascent, 60–90 min at ~5 m/s** — *"gradual atmospheric thinning → tests for pressure integrity and thermal regulation"*
2. **Peak altitude, 10–15 min** — *"balloon bursts; equipment operates in near-space conditions"*
3. **Descent, 30–40 min** — *"parachute deployment G-forces; vibration and turbulence; possible water/swamp landing"*

And the organisers' own experiment-selection advice, which is effectively free rubric guidance:

> *"Research existing work — avoid duplication… improve prior designs: add new variables or measurement methods."*
> *"Better a completed simple experiment than an unfinished complex one."*
> *"Leverage unique stratospheric conditions. Focus on phenomena that require near-space environment. **Avoid trivial tests (e.g. water freezing) — use a freezer instead!**"*
> *"Tests with spaceflight potential get extra attention."*

That last bullet is the organisers telling you directly: **an experiment whose result transfers to real spacecraft engineering is worth more than one that doesn't.** Structural-thermal behaviour and convective collapse both transfer directly (in vacuum, convection is zero — the stratosphere is the physical mid-point of that curve).

### 3.3 Your team roster — a problem to fix this week

| Official required role (§6.2) | Your roster | Status |
|---|---|---|
| Programmer | Azizbek Nomonjonov ("Programer, electrition") | ✅ you |
| Engineer (CAD/mechanical) | *(blank)* | ❌ **EMPTY** |
| Researcher | Diyora Mirpulatova (biology payload) | ✅ |
| Radio technician | Nurmuhammad Buriev **"?"** | ⚠️ marked uncertain |
| Mentor | Ezoza Yuldosheva | ✅ (may not work on the project — §6.3.2) |

**Consequence for you specifically:** if no CAD engineer joins, *you* will end up designing the side panels and payload mounting. Budget 3–4 days for that in §18, or recruit. Note also that the ORBITA25 deck recommends a **six**-role split (adding project manager and analysis/presentation specialist) while the Regulations mandate **four** members. The Regulations win; the extra roles must be absorbed by the four of you.

### 3.4 Contradictions found — stated, sourced, and resolved

**C1 — Peak altitude.** Four different numbers:

| Source | Claim |
|---|---|
| **Regulations §1.5, §6.6.4, §7.7** | *"into the stratosphere at an altitude of **up to 24 km**"* |
| IntroSat platform guide, p.1 | *"стратосферного запуска на высоту **до 25 километров**"* |
| ORBITA25 organiser deck | *"Operating altitude: **20–35 km**"* |
| ORBITA 2025 press coverage (cikeno.ru) | balloons reached *"approximately **20 kilometres**"* |

*Which is authoritative:* the **Regulations** (the binding legal document) say "up to 24 km"; the 2025 reported outcome (~20 km) is the best evidence of what actually happens. The deck's "20–35 km" is generic marketing about the stratosphere, not a flight commitment.
*Design consequence:* **Every mission concept in this document is designed to produce its full result by 18–20 km, with anything above that as bonus.** Do not choose an experiment whose science only begins above 24 km — for example, a Paschen-breakdown experiment tuned to the pressure minimum, or an ozone-layer-peak measurement, both of which want 26–30 km. Your teammates' drafts all say "approximately 24 km", which is the optimistic end of the official range; soften that to "up to 24 km" in every written submission.

**C2 — Finals dates.** Website: *27 Oct – 1 Nov 2026.* Regulations §9: *26 Oct – 1 Nov 2026.*
*Resolution:* the Regulations are authoritative and earlier. **Plan to be finished on 25 October.**

**C3 — Evaluation criteria status.** The 60-point rubric is the **2025** document; Regulations §7.10 says 2026 criteria are published *on the first working day of the finals.*
*Consequence:* treat the rubric as a strong prior, not a contract. The *categories* (significance / theory / technical / assembly / ground test / flight / analysis) are almost certainly stable because they mirror a normal engineering-project review. Build to satisfy all seven categories rather than optimising for exact point values.

**C4 — Remote-stage deadlines.** The participant guide describes Blocks 1–3 with deadlines **1–15 August**; the 2026 Regulations put the national phase at **1 Jun – 10 Sep** and development at **10 Sep – 26 Oct**.
*Resolution:* the August block structure is the **2025** schedule. The 2026 equivalent is the "mandatory online assignments" of §6.6.10. **Confirm the 2026 assignment deadlines with your national committee immediately** — missing them is an explicit disqualification trigger.

**C5 — Your mission drafts vs. the scoring rubric.** This is the most consequential contradiction in your local material, so it gets its own section.

### 3.5 The biology missions vs. the 15 flight points — an honest assessment

Your three drafts (snail, earthworm, DNA) share one structural property: **the result is obtained in a laboratory after recovery.** Nothing is measured in flight. Against the rubric:

| Rubric line | Max | Bio-mission-only outcome |
|---|---|---|
| Stratospheric flight performance | 15 | *"all basic systems and payloads operated normally, and a full amount of data was collected during the experiment"* — **if the payload is a sealed box with a snail in it, what data was collected in flight?** The honest reading puts a data-less payload in the 5–8 band |
| Analysis of results | 12 | *"the full array of data was correctly processed"* — a single post-flight assay is not an array |
| Technical elaboration | 8 | Currently no electronics described in any of the three drafts |

**This is not an argument against the biology.** It is an argument that **the biology missions are incomplete without you.** Three specific things the drafts require and do not yet have:

1. **Mission 1 (snail) and Mission 2 (earthworm) need a thermally controlled, sealed enclosure.** The draft says the snail will be *"maintained inside a protected biological module… where essential environmental parameters will be monitored and controlled as far as technically possible."* That sentence is an electronics work package. Numbers:

   | Insulation | Enclosure area | Heat loss @ ΔT=60 K | Energy over 3 h |
   |---|---|---|---|
   | 10 mm XPS foam | 0.04 m² | 8.2 W | **24.5 Wh** |
   | **20 mm XPS foam** | **0.04 m²** | **4.1 W** | **12.2 Wh** |
   | 30 mm XPS foam | 0.04 m² | 2.7 W | 8.2 Wh |

   Against a ~37 Wh pack (if 2.5 Ah @ 14.8 V — **unconfirmed, ask the organisers**), 20 mm of foam and a ~4 W heater is feasible but consumes about a third of your energy. Thinner insulation is not. This is a real, calculable, defensible piece of engineering — and writing it up properly is worth points under *theoretical elaboration*.

2. **All three need in-flight environmental telemetry to be scientifically interpretable.** "The snail was exposed to stratospheric conditions" is not a result; "the snail's enclosure followed this temperature/pressure/dose profile" is. That means a logged T/P/RH/dose record **inside** the biological module — your hardware.

3. **Missions 1 and 2 involve live animals and require organiser permission.** Regulations §7.13 defers safety rules to the first day of finals. **Ask now.** Also note the drafts assume the animal survives; at −55 °C ambient with 3 h exposure and no thermal control, it does not.

**Recommended team-level resolution — and I think this is genuinely the strongest play available to you:**

> **Fly ONE integrated payload with two experiments that share one electronics core.**
> - **1U: bio-module** (Mission 3/"DNA" is by far the safest choice — passive, no life support, no welfare question, low mass, low power) inside an instrumented, insulated enclosure.
> - **1U + one instrumented side panel: SKIN-24 structural-thermal experiment** (§8).
> - **One payload PCB** serves both: the same temperature network, the same data logger, the same telemetry link, the same ground station.
>
> The researcher gets a real biology result *with a documented environmental record*. You get a continuous, dense, in-flight dataset that maxes the flight-performance and analysis lines. The team submits one coherent mission instead of two half-missions. And the mass/energy cost of adding SKIN-24 to a bio payload is roughly **80 g and 0.3 W**.

**C6 — File numbering.** `Mission 3.docx` is internally titled *"Mission 4 — DNA Preservation for Precision Biotechnology."* Either a Mission 3 is missing from the folder or the numbering slipped. Worth resolving before any submission so the jury doesn't see inconsistent references.

**C7 — Brainstorm tables are empty.** Both tables in `International Space Tournament Orbita.docx` contain only the seeded "Ozone profile" example row. §6 of this document fills the technical table for you; you can paste the rows directly.

---

## 4. THE ENVIRONMENT AS AN EXPERIMENTAL INPUT
*Not "things to measure" — inputs that drive a response you can measure.*

### 4.1 The quantitative profile (ISA 1976, computed)

| Alt (km) | T (°C) | P (hPa) | P (Torr) | ρ (kg/m³) | ρ/ρ₀ | Mean free path (µm) |
|---|---|---|---|---|---|---|
| 0 | +15.0 | 1013.3 | 760.0 | 1.2250 | 1.000 | 0.065 |
| 5 | −17.5 | 540.2 | 405.2 | 0.7361 | 0.601 | 0.107 |
| 8 | −37.0 | 356.0 | 267.0 | 0.5252 | 0.429 | 0.151 |
| **11 (tropopause)** | **−56.5** | 226.3 | 169.8 | 0.3639 | 0.297 | 0.217 |
| 14 | −56.5 | 141.0 | 105.8 | 0.2268 | 0.185 | 0.349 |
| 16 | −56.5 | 102.9 | 77.2 | 0.1654 | 0.135 | 0.478 |
| 18 | −56.5 | 75.0 | 56.3 | 0.1207 | 0.099 | 0.655 |
| **20** | **−56.5** | **54.7** | **41.1** | **0.0880** | **0.072** | **0.898** |
| 22 | −54.5 | 40.0 | 30.0 | 0.0637 | 0.052 | 1.241 |
| **24** | **−52.5** | **29.3** | **22.0** | **0.0463** | **0.038** | **1.709** |

Key derived facts:
- **ΔT from launch to float ≈ −70 K** (more if launched on a warm morning).
- **Air density falls to 3.8% of sea level.** This is the single most under-appreciated number in student payload design.
- **Mean free path grows 26×** — still far below the millimetre scale, so the flow stays continuum, but heat transfer and acoustic propagation both degrade badly.
- **Speed of sound falls from 340 to 298 m/s**, and acoustic attenuation rises sharply — ultrasonic sensors stop working somewhere in the middle of the flight (which is itself a publishable result, see concept #17).

### 4.2 The other environmental axes

| Variable | Behaviour 0 → 24 km | Engineering consequence |
|---|---|---|
| **Convective heat transfer** | Natural-convection coefficient scales roughly as ρ^0.5 (laminar, Nu ∝ Ra^¼, Ra ∝ ρ²) → **h drops to ≈19% of sea level** | **Every self-heated part runs hot.** Radiation becomes co-dominant (h_rad ≈ 2.6 W/m²K at 233 K) |
| **Radiation (cosmic secondaries)** | Rises to the **Regener–Pfotzer maximum at ~15–26 km** — organisers quote **100–200× sea level**. Your flight passes *through* the peak | Single-event upsets in SD/RAM become non-negligible; dose rate is a measurable, altitude-correlated signal |
| **UV** | UV-B rises steeply with altitude as the ozone column above you shrinks; UV-A far less so → **the UV-A/UV-B ratio is an ozone-column proxy** | Cheap UV photodiodes can do ozone-profile science without an electrochemical cell |
| **Ozone** | Near-zero in the troposphere, rises sharply above the tropopause; at 22–24 km you reach the **bottom of the ozone layer** | The ozone-profile idea in your brainstorm table is real, but the interesting part starts right at the top of your altitude range — risky (see C1) |
| **Humidity** | Collapses from ~50% RH to a few ppmv; frost point ≈ −80 °C | Capacitive RH sensors flatline; the *altitude at which they flatline* is a calibration-limit result |
| **Wind / dynamics** | Jet stream shear at 9–12 km; above that the gondola drifts with the air so **relative wind ≈ 0**. Motion is **pendulum swing + spin**, not aerodynamic buffet | Measured in a real campaign: pendulum **period 5.82 s (0.178 Hz)**, horizontal accelerations ~**1.8 mg**, vertical peaks to **0.08 g** near burst, initial yaw rates to **40 °/s** decaying to ~0 above 22.5 km, **landing shock up to 2.5 g** |
| **Vibration** | Extremely quiet compared to a rocket. The structure is excited mainly by **burst, parachute snatch, and landing** | Your modal-analysis excitation is impulsive, not continuous. Design for transient capture, not steady-state averaging |
| **Electric field / conductivity** | Ion-pair production from cosmic rays raises air conductivity; the electrical relaxation time falls with altitude | Charge-decay probes are possible but EMI-sensitive and hard to defend |
| **Air permittivity** | ε_r falls from 1.000536 to ≈1.00002 | A capacitive gap sensor measures **gap AND air density** — a genuine cross-coupling (see concept #31) |
| **Paschen breakdown** | pd at 24 km with a 1 mm gap ≈ 2.2 Torr·cm — near the region where breakdown voltage is lowest | **Real spacecraft concern** (NASA-HDBK-4007 exists for exactly this), but requires generating hundreds of volts → **safety risk, likely prohibited.** See §20 |

### 4.3 Cross-variable interactions — where the interesting experiments live

This is the section that matters. Single-variable measurements are what everyone else will do. These couplings are what a technician can exploit:

| Coupling | Mechanism | Measurable signature | Exploited by |
|---|---|---|---|
| **Temperature → structural deformation** | CTE mismatch between a PLA/PETG panel (α ≈ 60–70 ppm/K) and an aluminium frame (α ≈ 23.6 ppm/K). Over ΔT = −70 K a **300 mm panel wants to shrink 0.97 mm more than its frame** | Panel bow, curvature, bolt preload change, strain-gauge output | **Concepts 1, 2, 3, 10 — the flagship** |
| **Temperature → material stiffness → modal frequency** | E of PLA rises ~40% from +20 °C to −50 °C; f_n ∝ √(E/ρ) → **+18% frequency shift** | First bending mode moves from e.g. 60 → 71 Hz | **Concept 4** |
| **Air density → convective cooling → component temperature** | h ∝ ρ^0.5. A 0.1 W self-heated element on a 6 cm² tab rises **≈16 K at sea level, ≈40 K at 24 km** | Monotonic, clean, altitude-correlated temperature rise | **Concept 11 — the "banker"** |
| **Temperature → strain-gauge thermal output** | The gauge itself outputs (α_substrate − α_gauge)·ΔT even under zero stress. For PLA this is **−4130 µε** — 10× larger than the bending signal | Normally a nuisance; **used deliberately it is an in-flight CTE measurement** | **Concepts 2 + the sum/difference trick in §9** |
| **Radiation → electronics** | Pfotzer-max secondaries → SEUs, elevated photodiode dark current, ADC noise-floor shifts | Bit-flip counts, CRC failures, noise-floor vs altitude | **Concepts 12, 13** |
| **Temperature → oscillator frequency** | Crystal and internal-RC frequency both drift; the RC drifts ~100× more | Beat between HSI and HSE measured on-chip | **Concept 26** |
| **Temperature → battery internal resistance** | Li-ion R_int rises steeply below 0 °C. **Your LoRa transmitter already applies a ~650 mA current step every second — a free excitation signal** | Voltage sag per TX burst vs pack temperature | **Concept 28 — zero extra hardware** |
| **Thermal gradient → "thermal snap"** | A documented real spacecraft phenomenon: solar arrays vibrate when a rapid thermal gradient is applied (eclipse entry/exit). Your analogue is sun/shade cycling as the payload spins, and the tropopause crossing | Transient vibration burst correlated with a temperature-rate spike | **Concept 5 / the SKIN-24 stretch goal** |
| **Density → acoustic attenuation** | Ultrasonic transducers lose signal as ρ falls | The altitude at which echo is lost | **Concept 17** |

---

## 5. RESEARCH FINDINGS — what the literature says, and what it leaves open

### 5.1 Balloon-flight structural dynamics are characterised — at the gondola level

The Italian ISM-HAB campaign (Aerospace, 2021) flew an LSM9DS1 IMU at **15 Hz** on five balloon flights to 32 km and published the definitive amateur-accessible dynamics dataset: pendulum period **5.82 s**, spectral line at **0.178 Hz**, horizontal accelerations ~**1.8 mg**, vertical peaks **0.08 g** near burst, yaw rates starting at **40 °/s** and decaying to near zero above 22.5 km, landing shock **~2.5 g**, analysed by de-trending + FFT in MATLAB.

**What that tells you:** the *rigid-body* motion of a balloon gondola is a solved, published problem. Re-measuring it is the "already replicated many times" 3–4 point band of the rubric.

**What it leaves open:** nobody in that literature instruments the *structure itself*. The IMU measures where the box went, not what the box did to itself.

### 5.2 Structural health monitoring exists — but not at this scale or price

- **NASA Ko displacement theory** (Armstrong Flight Research Center) converts a line of discrete surface-strain measurements into a **deflection curve** by computing curvature at each station and integrating twice. It has flown on the Ikhana UAV and Global Observer wings using fibre-optic strain sensors, and is documented in NASA TM-2020-220465 and NASA/TP-2009 reports. It is **published, citable, and implementable in ~50 lines of C**.
- **CubeSat micro-vibration measurement in orbit** (Sensors, 2025) — ADXL367 at 100 SPS on WREN-1, characterising ADCS flywheel vibration at 21.5 Hz, processed on a dsPIC33 with ground-side FFT. The authors explicitly note they are filling a gap in *publicly available CubeSat vibration datasets*.
- **Soft electronic skin for tape-spring hinges** (Nature Communications Engineering, 2024) — a flexible sensing layer on a deployable space structure. This is the closest published relative to a "space skin", and it is **a deployment-monitoring device on a hinge, not a thermo-elastic panel monitor**.
- **Optical-fibre impact strain monitoring on composite skin panels** (Aerospace, 2025) and the EU **STRAINMON** project — real aerospace SHM, but requiring fibre-Bragg interrogators costing thousands of dollars.
- **Thermally induced spacecraft vibration ("thermal snap")** — extensively documented for solar arrays entering/leaving eclipse (IntechOpen chapters, AIAA JSR papers, the Hubble solar-array jitter literature). Established physics, established as a *design problem*, but always studied on orbit with expensive instrumentation.

### 5.3 The gap

Putting those together, here is the honest statement of what is and is not new:

> Strain-based shape reconstruction: **established** (NASA, fibre optics, expensive).
> Thermally induced structural deformation of spacecraft: **established** (orbit, expensive).
> Balloon gondola rigid-body dynamics: **established** (cheap IMUs, well published).
> CubeSat micro-vibration in orbit: **recently established** (2025, one satellite).
> **Cheap resistive strain-array shape reconstruction of a student CubeSat panel, in flight, correlated with the measured thermal field, at a total parts cost under $70: I found no published implementation.**

That is a defensible **Category C/D** claim (see §7) — *novel combination, possibly novel implementation* — and it is exactly the kind of claim the rubric's *"completely or partially new"* wording rewards, without requiring you to overclaim.

### 5.4 Prior art on the accessible technology side

- **HX711** (24-bit ratiometric bridge ADC, gain 128, ~$1): thousands of documented load-cell projects, Arduino libraries everywhere, −40…+85 °C. Ratiometric excitation cancels supply drift. Default 10 SPS; **80 SPS requires tying the RATE pin high** (a solder-jumper mod on most modules).
- **ADS1220** (24-bit, PGA 128, SPI, −40…+125 °C, ~$5): better temperature range, internal 2:1 mux, built-in IDACs for RTDs. Harder to source locally.
- **BF350-3AA / BF120-3AA foil strain gauges** (~$0.3–1 each in packs of 10): the standard cheap constantan gauge, gauge factor ≈ 2.0.
- **35 mm piezo discs** (~$0.2): usable as structural microphones / acoustic-emission sensors down to cryogenic temperatures; piezoceramic sensitivity actually *rises* slightly as it cools.
- **DS18B20** (1-Wire, −55…+125 °C, ~$1): 8+ sensors on one GPIO with unique 64-bit IDs — perfect for a distributed temperature network with a single pin.
- **Prior balloon/CubeSat student payloads** (e.g. *"High altitude balloon testing of Arduino and environmental sensors for CubeSat prototype"*, HardwareX 2022; *"Testing a Prototype 1U CubeSat on a Stratospheric Balloon Flight"*, arXiv 2102.04847) consistently do **environmental logging** — T, P, humidity, GPS, camera. Structural self-measurement is essentially absent from this literature.

---

## 6. THIRTY-THREE MISSION CONCEPTS

Each entry gives the measurement principle, the technician's build, the honest development time, and the killer objection. **Dev time assumes you, working alone, with parts in hand.** Add shipping.

### A. Structural sensing

**C1 — Thermo-elastic panel deformation mapping ("space skin")** ⭐ *flagship*
*Question:* Does a student CubeSat's own side panel measurably bow as it cools by 70 K, and can we reconstruct its shape in flight from cheap strain gauges?
*Principle:* Back-to-back foil gauge pairs on a student-made panel. **Difference of the pair = pure bending (thermal output cancels); sum of the pair = thermal output (an in-flight CTE measurement).** One sensor pair, two physical quantities.
*Build:* 8–10 BF350 gauges, 4–5 × HX711 with a shared clock line, one payload PCB, panel fabricated in PLA/PETG/FR4.
*Numbers:* 0.97 mm PLA-vs-aluminium differential shrinkage over a 300 mm panel; a 10 K through-thickness gradient on a 1.5 mm PLA panel = **350 µε** surface strain; 300 µε peak strain on a 150 mm span reconstructs to **1.0 mm** of deflection. HX711 half-bridge full scale is **±3900 µε** with a noise floor around **0.5–2 µε** in practice — a signal-to-noise ratio of order 150–600.
*Dev time:* **1–2 weeks** to a flight-credible version.
*Objection:* bonding gauges well takes practice; budget spare gauges and one practice panel.

**C2 — In-flight CTE / stiffness characterisation of 3D-printing materials**
*Question:* What is the actual CTE of FDM-printed PLA and PETG between +20 °C and −55 °C, in the print orientation students actually use?
*Principle:* Identical quarter-bridge gauges on unstressed coupons of PLA, PETG, FR4, aluminium. Thermal output vs measured coupon temperature gives α(T) directly. Watch for a slope change as the polymer stiffens.
*Numbers:* thermal output over ΔT = −70 K: PLA **−4130 µε**, PETG −3430, ABS −5530, acrylic −4480, FR4 −350, Al −882 (all relative to a steel-compensated gauge).
*Why it matters:* student CubeSats are increasingly 3D-printed, and low-temperature CTE data for FDM parts is genuinely scarce. Anyone building a printed CubeSat needs this number.
*Dev time:* **3–5 days** (it shares all hardware with C1).
*Objection:* absolute accuracy requires a known reference; solve by including an aluminium coupon whose α is known to ±2%.

**C3 — Bolt-preload / joint-relaxation monitor**
*Question:* Do the M2.5 screws holding a plastic panel to an aluminium frame lose preload when cold?
*Principle:* Strain-gauged washer or an instrumented standoff under one screw.
*Dev time:* **1–2 weeks**, mostly machining. *Objection:* making a repeatable instrumented washer at this scale is hard without a lathe. **Downgrade to a stretch goal.**

**C4 — Modal frequency thermometry** ⭐
*Question:* Can we watch the panel's first bending mode shift as the material stiffens in the cold?
*Principle:* f_n ∝ √(E/ρ). PLA's modulus rises ~40% from +20 °C to −50 °C → **+18% frequency shift**; a 60 Hz mode moves to ~71 Hz. Excite with flight transients (burst, parachute) plus a tiny onboard vibration motor pulsed once a minute; detect with the piezo disc + FFT.
*Numbers:* 2 kHz sampling, 512-point FFT → 3.9 Hz bins. An 11 Hz shift is ~3 bins — comfortably resolvable.
*Dev time:* **1 week** on top of C1. *Objection:* needs a reliable excitation source; the vibration motor is the answer (a $0.5 pager motor).

**C5 — Thermally induced snap / buckling detection**
*Principle:* A deliberately slightly-pre-buckled thin panel snaps between stable states as thermal load crosses a threshold; catch it with strain + piezo. The terrestrial analogue of spacecraft "thermal snap."
*Dev time:* **1–2 weeks.** *Objection:* the snap threshold is hard to tune reliably; risk of it never firing (0 data) or firing on the pad. **Good as a bonus channel on the same hardware, bad as a primary mission.**

**C6 — Structural acoustic emission ("listening to the satellite creak")** ⭐
*Principle:* A 35 mm piezo disc bonded to the panel, AC-coupled into a comparator, counts stress-release events (polymer micro-cracking, fastener stick-slip) per second.
*Build:* piezo → 1 MΩ + 100 nF → op-amp (use the **platform's own OPAMP on PE7/PE8/PE9**) → **COMP_INP (PB2) / COMP_OUT (PC5)** for a hardware event counter. The IntroBus literally has a comparator wired out for this.
*Data:* 1 byte/second of event count — costs nothing in telemetry.
*Dev time:* **3–5 days.** *Objection:* discriminating real AE from motion artefacts requires a co-recorded IMU channel — which you have.

**C7 — 3U frame torsion via dual IMUs**
*Principle:* Two IMUs at opposite ends of the 3U; differential yaw angle = frame twist.
*Verdict:* **Infeasible.** Frame twist is microradians; MEMS gyro noise is orders of magnitude larger. Listed so you don't waste a week rediscovering this.

**C8 — Instrumented deployable hinge**
*Principle:* Strain-gauged tape-spring hinge; measure deployment torque vs temperature.
*Prior art:* directly overlapped by the 2024 Nature Comms Eng tape-spring e-skin paper. *Dev time:* **3–4 weeks** (mechanism design dominates). **Not recommended in 38 days.**

**C9 — Crack detection via a printed conductive trace**
*Principle:* A serpentine copper trace on an FR4 panel, or conductive paint on a printed panel; a crack shows as a resistance step.
*Dev time:* **3–5 days.** *Objection:* nothing is likely to crack in a 2-hour flight → high probability of a null result. Good as a **passive bonus channel** (it costs one ADC input), bad as a primary.

**C10 — Deliberate load-path asymmetry**
*Principle:* Under-torque one corner screw by a known amount and watch how strain redistributes between the four rails.
*Verdict:* Scientifically elegant, but deliberately flying a loose fastener will cost you **assembly points (§rubric line 4)** and may alarm the organisers. **Do it on the ground, present it as a ground-test result.**

### B. Environmental sensing, done non-trivially

**C11 — Convective-collapse calorimetry** ⭐⭐ *the "banker"*
*Question:* How much of your ground-measured cooling capacity survives to 24 km?
*Principle:* Three identical self-heated elements (a 100 Ω resistor on a 6 cm² copper tab) with different geometries/orientations, each with its own temperature sensor, plus an unheated reference. The rise ΔT above ambient is P/((h_conv + h_rad)·A). Since h_conv ∝ ρ^0.5 and h_rad is constant, the rise is a direct readout of air density's effect on cooling.
*Numbers (P = 0.1 W, A = 6 cm²):*

| Altitude | h_conv (W/m²K) | h_rad (W/m²K) | Predicted rise |
|---|---|---|---|
| 0 km | 8.0 | 2.6 | **+15.8 K** |
| 11 km | 4.4 | 2.6 | +24.0 K |
| 20 km | 2.1 | 2.6 | +35.3 K |
| 24 km | 1.6 | 2.6 | **+40.3 K** |

*Why this is the banker:* it is a **monotonic curve against altitude**, it cannot fail to produce data as long as the board is powered, the theory is closed-form and defensible, and it maps directly onto real spacecraft thermal design (in vacuum h_conv = 0 — the stratosphere is the measurable mid-point of that limit).
*Build:* 3 resistors, 3 DS18B20, one MOSFET to duty-cycle the heaters. **~$6 and 2 days.**
*Objection:* solar illumination contaminates the measurement → shade the elements, add an unheated reference in the same shade, and duty-cycle the heaters (on 30 s / off 30 s) so you measure *rise above local reference*, not absolute temperature.

**C12 — Regener–Pfotzer profile + electronics upset correlation**
*Principle:* A cheap Geiger tube (J305/SBM-20) counting secondaries vs altitude; simultaneously log SD-write CRC failures and a RAM scrubbing pattern.
*Dev time:* **1 week.** *Objection:* the tube needs ~400 V — a small HV boost module. Check whether the organisers permit it. Also, a 2-hour flight may see zero upsets, making the correlation half null.

**C13 — Radiation visible in the noise floor**
*Principle:* Track the ADC noise floor / photodiode dark current vs altitude, looking for a rise near the Pfotzer maximum.
*Verdict:* Elegant idea, but the expected effect is far below the thermal drift of a cheap front end. **High risk of a null result you cannot distinguish from drift.**

**C14 — Ozone column from a UV-A/UV-B photodiode ratio**
*Principle:* Two filtered photodiodes (e.g. GUVA-S12SD for UV-A+B, plus a UV-B-selective device); the ratio is an ozone-column proxy, self-normalising against changing sun angle.
*Dev time:* **1 week.** *Objection:* sun-angle and payload-spin modulation dominate; needs a sun-vector channel (C19) to correct. Also, the science gets good only at the **top** of your altitude range — see contradiction C1.

**C15 — Atmospheric conductivity / charge relaxation probe**
*Principle:* Charge an isolated plate through a high-value resistor, measure the decay time through a JFET follower; relaxation time falls as ion density rises.
*Dev time:* **1–2 weeks.** *Objection:* femtoampere-level measurement next to a 1 W transmitter. **Very high EMI risk.**

**C16 — Humidity-sensor calibration-limit mapping**
*Principle:* Fly three different RH technologies and find the altitude at which each stops responding.
*Verdict:* Honest and cheap, but reads as a negative-result study. **Good supporting channel, weak primary.**

**C17 — Ultrasonic density/speed-of-sound probe**
*Principle:* A 40 kHz TX/RX pair over a fixed 100 mm baseline; time-of-flight gives the speed of sound (→ temperature directly), and echo amplitude gives attenuation (→ density). The altitude where the echo dies is the headline result.
*Numbers:* c falls 340 → 298 m/s; over 100 mm that's 294 → 336 µs — easily timed with the **HRTIM on PC6/PC7**.
*Dev time:* **1 week.** *Objection:* the transducers will almost certainly stop working well before 20 km, so half your flight yields no signal. **Excellent second-tier payload, entertaining result, but not a full mission.**

### C. Sensor fusion

**C18 — Full pendulum-dynamics reconstruction**
*Principle:* Fuse accel + gyro + magnetometer + barometer to recover swing amplitude, spin rate, and effective pendulum length; detect the documented "quieting" above 22.5 km.
*Verdict:* **Already published** (ISM-HAB, 2021). Use it as a *supporting* analysis, not your mission.

**C19 — Sun-vector → illumination model → predicted panel temperature** ⭐
*Principle:* 4–6 cheap photodiodes on different faces give a coarse sun vector and the spin phase. Feed that into a two-node thermal model to **predict** each panel's temperature; compare with the measured temperature. A prediction that matches is a strong *"confirming or refuting the hypothesis"* result.
*Dev time:* **1 week** on top of the temperature network. *Objection:* clouds and albedo complicate it — but that's part of the analysis, not a flaw.

**C20 — Density from barometry + ascent-rate dynamics**
*Principle:* ρ = P/(R·T) from the MS5611 alone, cross-checked against the balloon's ascent-rate change.
*Verdict:* Nearly free; include it as derived telemetry.

**C21 — "Virtual thermocouple" from differing thermal masses**
*Principle:* Several temperature sensors with deliberately different thermal time constants; deconvolve to estimate the true air temperature and the local heat-transfer coefficient.
*Verdict:* This is essentially C11 in a different coat. **Merge them.**

### D. Embedded intelligence

**C22 — Onboard FFT with spectral-peak-only downlink** ⭐
*Principle:* Sample the piezo at 2 kHz into a DMA ring buffer, run a 512-point real FFT on the Cortex-M7 (CMSIS-DSP), downlink only the top-3 peak frequencies and amplitudes.
*Compression:* 2000 samples/s × 2 B = 4 kB/s raw → **12 B/s** transmitted. **A 330× reduction.**
*Dev time:* **3–5 days.** This is one of the highest score-per-hour items in the entire document.

**C23 — Onboard anomaly detection**
*Principle:* Maintain running mean/σ over a short feature vector (strain channels, vibration RMS, temperature rates); flag samples exceeding a Mahalanobis threshold; set a bit in the telemetry and trigger an event packet.
*Dev time:* **3–5 days.**

**C24 — Adaptive telemetry with a pre-trigger ring buffer** ⭐
*Principle:* Nominal 1 Hz summary packets. On an anomaly/event trigger, dump the **preceding 2 seconds** of high-rate data (already in the ring buffer) as a burst of event packets. This directly answers "extract more science from the same bandwidth": you get high-rate data for the moments that matter (burst, parachute snatch, landing) without spending bandwidth on the quiet 90%.
*Dev time:* **3–5 days.**

**C25 — Flight-phase state machine**
*Principle:* Classify pad / ascent / float / burst / descent / landed from barometer rate + accelerometer variance; tag every record. Lets you change sampling policy per phase and makes the ground analysis trivially sliceable.
*Dev time:* **1–2 days.** Do this regardless of which mission you choose.

### E. Electronics self-characterisation

**C26 — Oscillator stability vs temperature**
*Principle:* Measure the STM32's internal HSI RC oscillator against the external crystal using a timer capture; the RC drifts ~100× more with temperature. Produces a clean f(T) curve for free.
*Dev time:* **2–3 days.** *Objection:* the result is device-specific and mildly "so what" on its own. **Excellent free bonus channel.**

**C27 — ADC / V_ref drift mapping**
*Principle:* Log the STM32's internal reference voltage and internal temperature sensor against an external precision reference (e.g. REF3030) to quantify how much of your measurement error is thermal.
*Value:* This is **measurement-integrity evidence** — it lets you put honest error bars on every other channel, which is exactly what the *"reliable results"* wording in the analysis band wants.
*Dev time:* **2–3 days.**

**C28 — Battery internal resistance vs temperature, using the radio as the load** ⭐
*Principle:* The LoRa module draws ~650 mA for ~200 ms every transmission. That is a textbook current step. Sample the pack voltage immediately before and during each TX burst; ΔV/ΔI = R_int. Plot R_int against pack temperature over the whole flight.
*Hardware cost:* **one resistor divider into an ADC pin. That is all.**
*Dev time:* **1–2 days.** Genuinely elegant, genuinely useful (cold battery performance is a real CubeSat failure mode), genuinely free.

**C29 — LoRa link characterisation vs altitude and slant range**
*Principle:* Log RSSI, packet-error rate and sequence gaps against altitude/range; correlate with antenna orientation from the IMU.
*Value:* This is *literally the radio technician's job* and produces a publishable link-budget curve. **Assign it to Nurmuhammad.**
*Dev time:* **2–3 days** (mostly ground-station software).

**C30 — EMI self-interference characterisation**
*Principle:* Deliberately test whether the 1 W TX corrupts your strain readings, by comparing ADC samples timed inside vs outside TX bursts.
*Value:* Doubles as a mitigation design (blank the ADC during TX). Showing you *found and fixed* an EMI problem is exactly the kind of engineering evidence that scores under *technical elaboration*.
*Dev time:* **1–2 days.**

### F. Mechanical → electrical transduction

**C31 — Capacitive panel-gap sensor (with a built-in confound)**
*Principle:* A copper pad on the panel facing a pad on the frame; C ∝ ε₀ε_r A/d. Read with an FDC1004 or an RC-timing trick on the HRTIM.
*The interesting part:* air's relative permittivity falls from 1.000536 to ~1.00002 over the flight — so the sensor measures **gap AND air density simultaneously**. Separating them requires the barometer. That's a real coupled-variable problem, and solving it is a nice piece of analysis.
*Dev time:* **1–2 weeks.** *Objection:* the permittivity change is ~5 × 10⁻⁴ — at the edge of a cheap front end's ability. **Second-tier.**

**C32 — Hall-effect panel displacement**
*Principle:* A small magnet on the panel, a DRV5055/A1324 linear Hall sensor on the frame; output ∝ displacement.
*Verdict:* Simple and robust, **but Hall sensors drift badly with temperature — which is the exact variable you're studying.** Use as a *redundant backup* to the strain channels, with a temperature correction, not as the primary.

**C33 — Optical reflectance displacement**
*Principle:* TCRT5000 IR reflectance vs gap.
*Verdict:* Cheap, but LED output and photodiode responsivity both drift with temperature. **Backup only.**

### Concept summary table (ready to paste into your team brainstorm doc)

| # | Mission idea | Question it answers | Sensors / hardware | Est. cost | Dev time |
|---|---|---|---|---|---|
| 1 | Panel deformation "space skin" | Does a CubeSat panel bow as it cools 70 K? | 8–10 BF350 gauges, 4–5× HX711, PCB | $20 | 1–2 wk |
| 2 | In-flight CTE of printed materials | What is α(T) of PLA/PETG at −55 °C? | 4 gauges on coupons, 1× HX711 | $6 | 3–5 d |
| 4 | Modal frequency thermometry | Does the panel get stiffer as it cools? | Piezo disc, vibration motor, FFT | $3 | 1 wk |
| 6 | Structural acoustic emission | Does the structure crack/creak in the cold? | Piezo + platform OPAMP + COMP | $1 | 3–5 d |
| 11 | Convective-collapse calorimetry | How much cooling survives to 24 km? | 3 resistors, 3 DS18B20, MOSFET | $6 | 2 d |
| 12 | Pfotzer profile + upset counting | Where does radiation peak, does it flip bits? | Geiger tube + HV module | $25 | 1 wk |
| 14 | Ozone via UV-A/UV-B ratio | How does the ozone column change? | 2 UV photodiodes | $12 | 1 wk |
| 17 | Ultrasonic density probe | Where does sound stop propagating? | 40 kHz TX/RX pair, HRTIM | $3 | 1 wk |
| 19 | Sun vector → thermal prediction | Can we predict panel temperature? | 4–6 photodiodes + mux | $3 | 1 wk |
| 22 | Onboard FFT, peaks-only downlink | Can we get 330× compression? | firmware only | $0 | 3–5 d |
| 24 | Adaptive telemetry + pre-trigger | Can we capture transients without bandwidth? | firmware only | $0 | 3–5 d |
| 25 | Flight-phase state machine | Which phase is every sample from? | firmware only | $0 | 1–2 d |
| 27 | ADC/V_ref drift mapping | How wrong are our own measurements? | 1 precision reference | $2 | 2–3 d |
| 28 | Battery R_int via radio load steps | How bad does the cold battery get? | 2 resistors | $0.2 | 1–2 d |
| 29 | LoRa link vs altitude | What's the real link budget? | firmware + ground SW | $0 | 2–3 d |
| 30 | EMI self-interference | Does our own radio corrupt our data? | firmware only | $0 | 1–2 d |

---

## 7. NOVELTY ANALYSIS — honest classification

**Classification scheme (as requested):**
**A = Common** (widely demonstrated) · **B = Existing but uncommon** (exists; our implementation could differ meaningfully) · **C = Novel combination** (parts exist; this combination appears new) · **D = Potentially novel** (no obvious prior implementation found — stated cautiously)

| Concept | Class | What exists already | What would actually be new |
|---|---|---|---|
| Environmental T/P/RH logging on a balloon | **A** | Hundreds of student payloads; HardwareX 2022; arXiv 2102.04847 | Nothing. Do not present this as your mission |
| Balloon gondola IMU dynamics | **A** | ISM-HAB (Aerospace 2021) published periods, amplitudes, yaw rates | Nothing at the rigid-body level |
| Ozone / UV profile with balloon sensors | **A** | Standard ozonesonde practice since the 1960s | Only the *method* (photodiode ratio instead of electrochemical cell) — class B at best |
| Geiger-tube Pfotzer profile | **A** | Routinely flown by school groups worldwide | Correlating it with *measured electronics upsets* would be class B |
| Strain-based shape reconstruction | **B** | **NASA Ko displacement theory**, flown on Ikhana/Global Observer with fibre-optic sensors; NASA TM-2020-220465 | Doing it with **$0.5 resistive foil gauges and a $1 ADC** instead of a $10k interrogator |
| Thermally induced spacecraft structural deformation | **B** | Extensive "thermal snap" literature on solar arrays; Hubble jitter | Measuring it on a **student-fabricated CubeSat panel**, in flight, with the thermal field measured simultaneously |
| CubeSat micro-vibration measurement | **B** | Sensors 2025 (WREN-1, ADXL367, 100 SPS); authors note a public-data gap | Tracking **modal frequency vs temperature** rather than characterising a flywheel |
| Flexible sensing skin on a space structure | **B** | Nature Comms Eng 2024 — soft e-skin on tape-spring hinges | That work monitors **deployment**; a thermo-elastic **panel** monitor is a different problem |
| Low-temperature CTE of FDM-printed polymers | **B/C** | Room-temperature and elevated-temperature data exist; cryogenic FDM data is sparse and rarely in the print orientations students use | An **in-flight** measurement, in the as-printed orientation, against a co-located aluminium reference |
| Convective-collapse calorimetry on a balloon | **C** | Altitude derating of convection is textbook engineering (and NASA-HDBK-4007 covers the HV analogue); balloon payloads rarely instrument it deliberately | A **deliberate, calibrated three-geometry measurement of h(ρ)** across 0→24 km with an unheated reference. I found no student or published balloon payload doing this as a primary experiment |
| **Cheap strain-array shape reconstruction of a CubeSat panel, correlated with a measured thermal field, at <$70** | **C/D** | Every component and every method exists separately | **The integration. I found no published implementation of resistive strain-array shape reconstruction on a CubeSat structure in flight.** Stated cautiously: absence of evidence is not evidence of absence, and school-project literature is poorly indexed |
| Sum/difference use of a back-to-back gauge pair to extract bending **and** CTE from one pair | **B/C** | Half-bridge thermal compensation is textbook (every strain-gauge handbook); using the **sum** channel as a deliberate CTE measurement is much less common | Presenting one sensor pair as a **dual-output transducer** and validating both outputs in flight |
| Battery R_int measured using the radio's own TX current step | **C** | Pulse-based R_int measurement is standard battery science | Using an **existing subsystem's duty cycle as the excitation signal**, at zero hardware cost, on a flight vehicle |
| Onboard FFT + peaks-only adaptive telemetry on a balloon CubeSat | **B** | Standard practice in professional telemetry; rare in student balloon payloads | The **ring-buffer pre-trigger** applied to burst/parachute transients |

**What you should claim, in exactly these words:**

> *"The individual techniques — resistive strain gauges, half-bridge thermal compensation, NASA's Ko displacement theory, and onboard FFT — are all established. What we believe is new is their combination: using a $0.5-per-channel resistive strain array to reconstruct, in flight, the thermo-elastic deformation of a student-fabricated CubeSat panel, simultaneously with the thermal field that causes it. We searched CubeSat mission literature, NASA and ESA structural-monitoring publications, student competition archives and open-source repositories and did not find a prior implementation at this cost point; we cannot prove none exists."*

That sentence is defensible under cross-examination, which is worth far more than an unprovable "world first."

**What you must NOT claim:** that nobody has measured thermal deformation of spacecraft structures (they have, extensively), that shape-from-strain is new (NASA published it decades ago), or that balloon structural monitoring has never been attempted.

---

## 8. TOP CONCEPTS — DEEP TECHNICAL ANALYSIS

Three concepts survive every constraint simultaneously. They are designed to **share one payload PCB, one firmware, one telemetry stream and one ground station**, so flying all three costs barely more than flying one.

---

### 8.1 PRIMARY — **SKIN-24: thermo-elastic structural self-diagnosis**

**Mission question**
> *When a student-built 3U CubeSat is carried from +20 °C / 1013 hPa to −55 °C / 30 hPa, does its structure measurably deform, and can the spacecraft detect and quantify that deformation using only sensors costing less than one dollar per channel?*

**Hypothesis (falsifiable, with numbers)**
> H1 (bending): The panel will develop a measurable bow driven principally by the **through-thickness thermal gradient** — one face illuminated by the sun, the other radiating to a −60 °C sky — with peak surface bending strain of **200–600 µε** and reconstructed deflection of **0.7–2.0 mm** over a 150 mm instrumented span. For a free 1.5 mm PLA plate, a through-thickness gradient of ΔT_t gives κ = α·ΔT_t/t, i.e. **350 µε per 10 K**; edge constraint will reduce this, and measuring by how much is part of the result. The bow will be **modulated by the payload's spin** (yaw ~40 °/s early, decaying toward zero above 22.5 km), giving a periodic signature that is itself strong evidence the measurement is real and not drift.
> H2 (membrane): The **in-plane** CTE mismatch works in the opposite sense. On cooling, the PLA panel wants to shrink 0.97 mm more than its 300 mm aluminium frame, so it is held **in tension**, not compression — a thin plate in tension flattens rather than buckles. That membrane strain appears in the **sum** channel of each pair. Because the sum channel also contains the gauge's own thermal output, the two are separated by subtracting the free CTE coupon's reading, which is pure thermal output at the same temperature. **This is precisely why the free coupons are in the payload and not an afterthought.**
> H3 (CTE): The free-coupon sum channels will yield thermal output of **−3400 to −4200 µε** over ΔT = −70 K for PLA/PETG and **−882 µε** for the aluminium reference, from which α(T) can be extracted to within ±10% — the aluminium channel being the accuracy check, since its α is independently known.
> H4 (modal): The panel's first bending mode will **increase in frequency by 10–20%** as it cools, consistent with the temperature dependence of the polymer's elastic modulus (f ∝ √(E/ρ); E of PLA rises ~40% over this range → +18%).
> **Null result is still a result:** if H1 is refuted — if the panel does *not* bow, because the mounting-hole clearance absorbs the differential or the edge constraint fully reacts the thermal moment — that is an equally publishable engineering finding, and the rubric explicitly rewards *"conclusions confirming **or refuting** the hypothesis."* This mission cannot produce a scientifically empty outcome.

**Why it matters**
Real spacecraft thermal-structural distortion budgets are a serious discipline (optical benches, antenna pointing, solar-array jitter). Student CubeSats are assembled warm and flown cold with **no distortion budget at all**, using 3D-printed panels whose low-temperature properties nobody has measured. If the answer is "student CubeSat panels bow by a millimetre and put their fasteners in tension," that is directly actionable for every team in this competition, including next year's. It is also an honest miniature of the thermal-snap problem that costs real missions real pointing performance.

**What changes with altitude, and why that drives the experiment**
The panel sees a **−70 K ramp over 60–90 minutes** (~1 K/min), then a slow radiative equilibrium at float, then a thermal transient at burst, then re-warming during descent. Meanwhile convective coupling to the air collapses to 19%, so the panel's temperature becomes dominated by **radiation** — meaning the sun-facing and shade-facing sides diverge, producing a **through-thickness and in-plane gradient** that is itself a bending moment. The spin of the gondola (yaw 40 °/s early, decaying to ~0 above 22.5 km) modulates that gradient. So the panel experiences: a slow bulk contraction, a rotating thermal gradient, and impulsive mechanical events. All three appear in the same dataset.

**Measurement principle — the core trick**

A foil strain gauge bonded to a substrate outputs
```
ε_measured = ε_mechanical + (α_substrate − α_gauge)·ΔT + (TCR_gauge/GF)·ΔT
                              └──────────── thermal output ────────────┘
```
The thermal output for PLA is **~10× larger than the bending signal** — which is why naive strain measurement on a cold flight fails.

**The solution, and the reason this design is elegant:** bond gauges in **back-to-back pairs**, one on each face of the panel, at the same (x, y).

- Under **bending**, the two faces see equal and opposite strain: ε_top = +ε_b, ε_bot = −ε_b.
- Under **thermal load**, both see the same: ε_top = ε_bot = ε_T.

Therefore:
```
BENDING   ε_b = (ε_top − ε_bot)/2    ← thermal output cancels exactly
THERMAL   ε_T = (ε_top + ε_bot)/2    ← bending cancels exactly
```
**One pair of $0.5 sensors yields two independent physical measurements.** This is the single most important design decision in the whole payload, and it is the answer to the objection "you can't do strain measurement at −55 °C with cheap gauges."

Then, from the bending strains, curvature at each station:
```
κ_i = 2·ε_b,i / t          (t = panel thickness)
```
and from the curvatures, deflection via **NASA's Ko displacement theory** — double trapezoidal integration along the sensor line:
```
θ_i = θ_{i−1} + (κ_{i−1} + κ_i)·Δx/2
w_i = w_{i−1} + (θ_{i−1} + θ_i)·Δx/2
```
with boundary conditions set by the panel's mounting (w = 0, θ = 0 at the clamped edge).

**Hardware (all realistically obtainable)**

| Function | Part | Interface | V | Qty | ≈ Price ea. | Notes |
|---|---|---|---|---|---|---|
| Strain gauge | **BF350-3AA** (350 Ω, GF 2.0) | analogue | — | 12 (+6 spare) | $0.5 | BF120-3AA (120 Ω) is an acceptable substitute; higher current |
| Bridge ADC | **HX711 module** | 2-wire bit-bang | 3.3 | 5 | $1.2 | Cut/solder RATE pad for 80 SPS. −40…+85 °C |
| Backup bridge ADC | **ADS1220** breakout | SPI2 | 3.3 | 1 | $6 | −40…+125 °C; use as the redundant chain |
| Precision reference | **REF3030** (3.0 V) | analogue | 3.3 | 1 | $2 | For ADC drift mapping (C27) |
| Structural microphone | **35 mm piezo disc** | analogue → OPAMP | — | 2 | $0.3 | AE + modal excitation pickup |
| Excitation | **pager vibration motor** | GPIO + MOSFET | 3.3 | 1 | $0.6 | Pulsed 100 ms once per minute |
| Temperature network | **DS18B20 (TO-92)** | 1-Wire, 1 GPIO | 3.3 | 8 | $1.0 | −55…+125 °C. Unique 64-bit IDs |
| Calorimeter heater | 100 Ω 1 W metal-film + 20×30 mm copper tab | GPIO + MOSFET | 3.3 | 3 | $0.3 | 0.1 W each |
| Load switch | **AO3400 / IRLML2502** N-MOSFET | GPIO | 3.3 | 4 | $0.1 | Logic-level gate |
| Analogue mux | **CD74HC4051** | 3 GPIO + 1 ADC | 3.3 | 1 | $0.5 | For photodiodes/NTCs |
| Sun sensors | **BPW34** photodiode or LDR | ADC via mux | — | 4 | $0.5 | Spin phase + illumination |
| Battery sense | 100 k / 10 k divider + 100 nF | ADC | — | 1 | $0.1 | For C28 |
| Payload PCB | 2-layer, 90 × 90 mm | — | — | 1 | $10–15 | Or protoboard for L1/L2 |
| Connectors | JST B2B-EH-A + terminal blocks | — | — | — | in kit | **Flight-legal; PLS-2 is not** |
| Panel material | 1.5 mm PETG or PLA sheet / printed panel | — | — | 2 | $3 | Plus one aluminium and one FR4 coupon |
| Adhesive | **Cyanoacrylate (M-Bond 200 equivalent)** + polyurethane coat | — | — | — | $3 | Standard gauge bonding |
| **TOTAL** | | | | | **≈ $45–70** | |

**Electronics architecture**

```
 Thermo-elastic panel deformation (physical)
     │
     ├─► BF350 gauge (top face)  ─┐
     │                            ├─► quarter/half bridge, 350 Ω, 3.3 V ratiometric excitation
     ├─► BF350 gauge (bottom)   ─┘         │
     │                                      ▼
     │                            HX711  (24-bit, PGA ×128, ratiometric)
     │                              │   shared SCK, separate DOUT per device
     │                              ▼      → all 5 devices convert synchronously
     │                        STM32H750VBT6  (GPIO bit-bang, 80 SPS)
     │
 Panel vibration / AE
     └─► piezo disc ─► 1 MΩ‖100 nF ─► PLATFORM OPAMP (PE9 in, PE8 fb, PE7 out)
                                          ├─► ADC (PB0) @ 2 kHz, DMA ring buffer ─► CMSIS-DSP FFT 512
                                          └─► COMP_INP (PB2) ─► COMP_OUT (PC5) ─► TIM counter = AE hits/s

 Structure temperature (8 pts) ─► DS18B20 1-Wire ─► single GPIO (PE7 alt or PC3)
 Calorimeter heaters ×3 ────────► MOSFET ◄─ GPIO (PC6), duty-cycled 30 s on / 30 s off
 Sun diodes ×4 ─► CD74HC4051 ──► ADC (PB1),  select = PD8/PD9/PB13
 Battery sense ─────────────────► ADC (PC3_C)
 Service sensors (LSM6DS3 / LIS3MDL / MS5611 / LM75A) ─► I²C1 (locked to platform)
     │
     ▼
 STM32H750:  calibrate → filter → fuse → state-machine tag → compress
     ├──────► microSD (SDMMC2)  ── full-rate raw log (primary record)
     └──────► UART1 115200 8E1 ─► CDAM ─► E32-433T30D LoRa 433 MHz ─► ground
                                                                        │
                                                    LoRa-UART dongle ─► PC
                                                                        ▼
                                            Python ground station: decode → CSV → live 3D panel
```

**Pin budget on IntroBus_L (verified against the platform pinout)**

| Signal | Pin | Note |
|---|---|---|
| HX711 shared SCK | PC6 | HRTIM pin used as GPIO |
| HX711 DOUT ×5 | PC7, PB2*, PB9, PA4, PA5 | *PB2 shared with COMP_INP — use PC4 instead if the comparator is used |
| Piezo → OPAMP | PE9 (INP), PE8 (INM), PE7 (OUT) | platform's internal op-amp |
| Piezo → ADC | PB0 | 2 kHz DMA |
| AE comparator | PB2 (INP) → PC5 (OUT) | hardware event counting |
| Mux analogue out | PB1 | ADC |
| Mux select A/B/C | PD8, PD9, PB13 | UART3/5 pins as GPIO |
| 1-Wire (DS18B20) | PC0 | 4.7 kΩ pull-up to 3.3 V |
| Heater MOSFET gate | PC1 | |
| Vibration motor gate | PC2_C | |
| Battery sense | PC3_C | ADC |
| Spare / ADS1220 SPI | PB15/PB14/PB10 + CS PD12 | redundant chain |

**Everything fits.** Nothing conflicts with the service bus. I²C4 (PD13/PD12) stays free as an expansion port — which is where the biology module's environment sensors would go.

**Firmware architecture** — see §11. **Telemetry** — see §12. **Ground testing** — see §14. **Failure modes** — see §15.

**Expected final result**
A set of plots that tell one story:
1. Panel centre deflection (mm) vs altitude, with the 3D reconstruction at four representative moments.
2. Measured α(T) for PLA/PETG/FR4/Al coupons, with the aluminium curve as the accuracy check.
3. First-mode frequency vs panel temperature, with the √(E/ρ) prediction overlaid.
4. Panel curvature vs measured thermal gradient — the correlation that tests H1 directly.
5. An animated reconstruction of the panel bending through the whole flight.

---

### 8.2 SECONDARY — **THERMOS: convective-collapse calorimetry** *(the insurance policy)*

**Mission question**
> *How much of a CubeSat's convective cooling capacity survives the climb to the stratosphere, and does the measured loss match the ρ^0.5 scaling used in thermal-design practice?*

**Hypothesis**
> The temperature rise of a self-heated element above a co-located unheated reference will grow from **≈16 K at ground to ≈40 K at 24 km** (for 0.1 W on a 6 cm² tab), following h_total = h_conv,0·(ρ/ρ₀)^0.5 + h_rad. Elements of different geometry will diverge in a way predicted by their characteristic length.

**Why it is the insurance policy**
It has no moving parts, no bonding step, no calibration fixture, and no failure mode short of total power loss. It produces a **monotonic, unmistakable curve against altitude**. If SKIN-24's gauges were to debond in flight (the realistic worst case), THERMOS alone still delivers a complete dataset, a confirmed hypothesis, and a defensible analysis — protecting most of the 27 points that live in flight performance + analysis.

**Build:** 3 × (100 Ω resistor soldered to a 20 × 30 mm copper tab) + 3 × DS18B20 + 1 unheated reference DS18B20 + one MOSFET. **Two days, six dollars.** Geometry variants: (a) bare tab, (b) tab with a 10 mm standoff from the panel, (c) tab inside a small perforated shield.

**Critical design detail:** duty-cycle the heaters **30 s on / 30 s off** and always compute the *rise above the co-located unheated reference within the same cycle*. This removes solar illumination, ambient drift, and sensor offset in one step. It also halves the energy cost (~0.15 W average for all three).

---

### 8.3 TERTIARY — **BLACKBOX: zero-hardware embedded intelligence**

Four firmware-only experiments that cost nothing to fly and add measurable technical depth:

| Sub-experiment | Hardware cost | Result produced |
|---|---|---|
| Onboard 512-pt FFT, peaks-only downlink (C22) | $0 | 330× telemetry compression; modal tracking data |
| Adaptive telemetry with 2 s pre-trigger ring buffer (C24) | $0 | High-rate capture of burst / parachute / landing transients |
| Flight-phase state machine (C25) | $0 | Every record tagged; phase-dependent sampling policy |
| Battery R_int from LoRa TX current steps (C28) | ~$0.20 | R_int(T) curve — a real cold-battery dataset |
| ADC / V_ref drift map (C27) | ~$2 | Honest error bars on every other channel |
| LoRa PER/RSSI vs altitude (C29) | $0 | Link-budget curve — the radio technician's deliverable |
| EMI self-test (C30) | $0 | Evidence of a found-and-fixed engineering problem |

Together these are worth more rubric points per hour of work than any additional sensor you could buy.

---

## 9. "SPACE SKIN" — FULL ENGINEERING INVESTIGATION

You asked me not to simply accept this concept. So here is each proposed sensing method, evaluated against the real constraints (−55 °C, 30 hPa, 38 days, <$100, must survive a 2.5 g landing).

### 9.1 Candidate transduction methods, compared

| Method | Physics | Resolution achievable | Cost/channel | −55 °C behaviour | Verdict |
|---|---|---|---|---|---|
| **Foil strain gauge (Wheatstone)** | ΔR/R = GF·ε, GF ≈ 2.0 | **±3900 µε** full scale, ~**0.5–2 µε** usable | **$0.50** | Excellent; thermal output is large but **cancels in a back-to-back pair** | ✅ **PRIMARY** |
| Serpentine trace on the panel itself (printed/etched copper) | Same ΔR/R, but GF of pure copper ≈ 2.0 *plus* a large TCR term (Cu TCR ≈ +3900 ppm/K) | Poor: **TCR dominates by ~1000×** | $0 (part of the PCB) | Catastrophic — a 1 K error looks like 2000 µε of strain | ❌ **Rejected.** This is the trap in the "conductive traces change resistance" idea: copper is a far better thermometer than strain gauge. Constantan (the alloy in a real gauge) is used precisely because its TCR is ~±20 ppm/K |
| Conductive-paint / carbon grid | Piezoresistive, very high gauge factor (10–100) | Good sensitivity, terrible stability | $2 | Drift and hysteresis get worse when cold | ⚠️ Interesting but unqualifiable in 38 days |
| **Flex sensor (Spectra Symbol)** | Carbon ink resistance vs bend radius | Coarse: usable only for **large** bends (>10 mm) | $10–15 | Severe drift; ink binder stiffens | ❌ **Rejected** — wrong range (we need 0.1–1 mm), too expensive, poor cold behaviour |
| **Piezo disc (PZT)** | Charge ∝ dynamic strain | Superb for **dynamic** signals; **cannot measure static bend** (charge leaks) | $0.30 | Sensitivity *improves* slightly when cold | ✅ **COMPLEMENTARY** — pairs perfectly with gauges (static from gauges, dynamic from piezo) |
| Capacitive gap (pad-to-pad) | C = ε₀ε_r·A/d | ~10 µm with a good front end | $3 (FDC1004) | Air's ε_r changes by 5×10⁻⁴ over the flight — **a confound that must be corrected with the barometer** | ⚠️ Second-tier; scientifically interesting *because* of the confound |
| **Hall + magnet** | B ∝ 1/d³ near the magnet | ~20 µm over a 2 mm range | $2 | **Hall sensitivity drifts ~−0.1%/K** → a 70 K swing is a 7% error on the exact variable under study | ⚠️ **Backup only**, with temperature correction |
| Optical reflectance (TCRT5000) | Reflected IR intensity vs gap | ~50 µm, non-linear | $1 | LED output falls, photodiode responsivity rises — both with temperature | ⚠️ Backup only |
| Laser triangulation / optical lever | Beam deflection over a long path | Excellent (µm) | $15+ | Alignment stability over 70 K is the problem | ❌ Too fragile for a 2.5 g landing |
| **IMU (accelerometer)** | Rigid-body motion, not deformation | Cannot see a 0.5 mm static bow | (already on board) | Fine | ✅ Use it for **motion context and modal excitation reference**, not for deformation |
| Fibre Bragg grating | Wavelength shift ∝ strain | Gold standard | **$3000+ interrogator** | Excellent | ❌ Out of budget — and this is precisely the gap your cheap version fills |

**Conclusion:** the space skin is real, but only in one specific form — **an array of back-to-back foil-gauge pairs, complemented by a piezo disc for dynamics.** Everything else is either the wrong range, the wrong stability class, or unaffordable. Note particularly that the intuitive idea ("just run a serpentine copper trace on the PCB and watch its resistance") **does not work**, for a reason that is itself worth putting in your presentation: it measures temperature, not strain. Knowing *why* a plausible idea fails is exactly the kind of evidence the *technical elaboration* band is asking for.

### 9.2 Bridge topology — why the back-to-back half-bridge wins

Four options for wiring 350 Ω gauges:

| Topology | Output | Thermal output | Notes |
|---|---|---|---|
| Quarter bridge (1 gauge + 3 fixed R) | V_ex/4 · GF · ε | **Full, uncancelled** | Needed for the CTE coupons, where thermal output *is* the signal |
| Half bridge, adjacent arms, **back-to-back on the panel** | V_ex/2 · GF · ε_b | **Cancels exactly** | ✅ **This is the one.** Also **doubles** sensitivity |
| Half bridge with a dummy gauge on an unstrained coupon | V_ex/4 · GF · ε | Cancels *if* the coupon tracks temperature | Needs thermal matching; more wiring |
| Full bridge (4 active gauges) | V_ex · GF · ε | Cancels | 4× sensitivity but 4× the gauges and bonding work — overkill here |

**Sensitivity numbers, done properly.** The HX711 supplies its own bridge excitation (AVDD, generated on-module) and digitises ratiometrically, so the *strain* full scale is independent of the excitation voltage — a genuinely useful property:

| Supply | AVDD | Half-bridge sensitivity | HX711 full scale (×128) | Full-scale strain |
|---|---|---|---|---|
| VCC = 5 V | ≈4.3 V | **4.30 µV/µε** | ±16.8 mV | **±3906 µε** |
| VCC = 3.3 V | ≈2.7 V | **2.70 µV/µε** | ±10.6 mV | **±3906 µε** |

A 400 µε bending signal therefore produces **1.7 mV (at 5 V) or 1.1 mV (at 3.3 V)** at the HX711 input — about **10% of full scale**, which is exactly where you want to sit. The theoretical 24-bit LSB is ~0.05 µε; input-referred noise (~200 nV RMS at 80 SPS) puts the real floor at **0.05–0.1 µε**, and wiring and thermal EMFs push it to a practical **0.5–2 µε**. Against a 200–600 µε signal that is a signal-to-noise ratio of **150–600**. Noise is not your problem; bonding quality is.

> ⚠️ **Supply-voltage trap.** HX711 modules are usually run at 5 V for best sensitivity — but then **DOUT drives 5 V logic into the STM32H750's input pins.** PA4 and PA5 are analogue-capable (DAC) pins and are **not 5 V tolerant**; driving them at 5 V can damage the MCU. Two safe options: (a) **run the HX711 modules from 3.3 V** — you lose 37% of the absolute sensitivity and none of the strain full scale, and the noise floor is still 100× below the signal (**recommended**); or (b) run at 5 V and put a 10 kΩ series resistor plus a 20 kΩ pull-down on each DOUT line. Do not skip this check — it is exactly the kind of "technical limitation taken into account" the rubric asks for.

**Excitation and self-heating:** each 350 Ω arm at 3.3 V draws 4.7 mA and dissipates **7.8 mW**. On aluminium that is negligible; on PLA (thermal conductivity ~0.13 W/m·K) it produces a small local hot spot. Mitigation: **use 350 Ω not 120 Ω** (three times less power), and if you want to be rigorous, gate the bridge excitation off between samples. The HX711's ratiometric design means excitation drift cancels automatically.

### 9.3 Reading N channels — multiplexing strategy

Three architectures were considered:

**Option A — one ADC + analogue multiplexer (CD74HC4051) + instrumentation amplifier (INA333)**
- Pros: one precision front end, cheapest silicon.
- Cons: the mux sits in the **microvolt** path. Its R_on (~70 Ω) and charge injection add offset that varies with temperature; settling after each channel switch costs time. Doable, but it is the hardest of the three to make trustworthy in 3 weeks.

**Option B — one HX711 per half-bridge, shared clock** ✅ **RECOMMENDED**
- Each HX711 has its own bridge; no analogue multiplexing at all. The mux is *digital*.
- **The trick:** the HX711 has no chip-select. Tie **all SCK lines together** and give each device its **own DOUT** line. When you pulse the shared clock, every device shifts out its bit simultaneously — so you read **5 channels in one 25-clock loop, perfectly time-synchronised**, using **6 GPIO pins total**.
- At 80 SPS this costs ~1 ms of CPU per sample set. Simultaneity matters: for shape reconstruction you need all stations sampled at the same instant, and this gives it to you for free.
- Cons: 5 separate ADCs means 5 separate offsets and gains to calibrate. That is a one-evening calibration job (§14).

**Option C — ADS1220 (2 differential channels each, internal mux + PGA)**
- Pros: −40…+125 °C, better noise, SPI, built-in mux.
- Cons: harder to source in Tashkent, more firmware work.
- **Use one ADS1220 as a redundant parallel chain** on two of the five stations. If the HX711 chain dies in the cold, you still have two curvature stations — enough for a reduced reconstruction. Redundancy of this kind is exactly what protects the 15 flight points.

### 9.4 Sensor placement and how N measurements become a shape

**Panel geometry — and a design decision you control.** A 3U side panel is roughly **96 × 300 mm**, attached to the frames at the top, middle and bottom bars. The exact boundary conditions depend on how *you* design the panel — and that is an advantage, not a nuisance: **choose boundary conditions your model can handle.**

Recommendation: instrument only the **upper bay, between the top and middle frames — a clean 150 mm span**, clamped at both ends, with the long edges left free of fasteners. That makes it a well-defined clamped beam, and it keeps the deflections in a range that is both physically realistic and visually obvious (300 µε → 1.0 mm).

**Strategy 1 (recommended) — 1D beam reconstruction along the 150 mm span.**
Place **4 gauge pairs** at x = 22, 52, 98, 128 mm (asymmetric, so the array is not blind to antisymmetric modes).

```
        x=0       22      52            98     128         L=150mm
 frame ══╪═════════╪═══════╪═════════════╪══════╪═══════════╪══ frame
            (S1↕)  (S2↕)        (S3↕)   (S4↕)
         ↕ = a back-to-back pair (one gauge each face)
```
Reconstruction (NASA Ko displacement theory):
```
κ_i = 2·ε_b,i / t
θ_i = θ_{i-1} + (κ_{i-1}+κ_i)·Δx/2      (θ_0 = 0 at the clamped edge)
w_i = w_{i-1} + (θ_{i-1}+θ_i)·Δx/2      (w_0 = 0)
```
4 stations → a 6-point deflection curve (2 boundary + 4 measured), cubic-spline interpolated to a smooth line. **This runs in <50 µs on the H7 and can be done onboard, so the downlink carries curvature, and the ground station carries the reconstruction.**

**Strategy 2 — 2D plate reconstruction (stretch goal).**
Add 1 pair oriented along y and 1 pair at 45°, giving κ_x, κ_y and the twist κ_xy. Then fit an assumed plate surface:
```
w(x,y) = Σ_k a_k · φ_k(x,y)
```
with 4–5 analytic basis functions for the panel's boundary conditions (e.g. products of clamped-beam modes). Each sensor's strain is a known linear functional of the a_k, so:
```
ε = B·a    →    a = (BᵀB)⁻¹ Bᵀ ε      (least squares, 6×5 → trivial)
```
With 6 pairs and 5 basis functions the system is overdetermined and well-conditioned, and the residual gives you an honest reconstruction-error estimate.

**Practical recommendation given 38 days:** build Strategy 1 (4 pairs = 8 gauges), and if week 2 goes well, add the two extra pairs for Strategy 2. Design the PCB with **6 bridge headers from the start** so adding them costs no rework.

### 9.5 Required sampling rate — derived, not guessed

| Phenomenon | Timescale | Required rate | Chosen rate |
|---|---|---|---|
| Bulk thermo-elastic bow | minutes (1 K/min ramp) | 0.1 Hz | **10 Hz** (100× margin) |
| Sun/shade gradient from spin | ~8–9 s period early, slowing | 1 Hz | **10 Hz** |
| Burst transient | ~0.1–1 s | 20 Hz | **80 Hz** (HX711 fast mode) |
| Parachute snatch | ~50–200 ms | 50 Hz | **80 Hz** |
| First bending mode of the panel | 40–100 Hz expected | ≥ 200 Hz (Nyquist ×2 margin) | **2 kHz** on the piezo channel |
| Acoustic emission events | µs-scale pulses | hardware counter | **comparator + timer** |

So: **strain at 80 SPS, piezo at 2 kHz, temperatures at 1 Hz, IMU at 104–416 Hz.** The strain chain does not need to resolve the vibration — that is the piezo's job. This division of labour is what lets the whole thing fit in a cheap bit-banged interface.

### 9.6 Wiring topology and flight survivability

- Each gauge: 2 wires. 8–12 gauges → **16–24 thin wires** from the panel to the payload PCB. Use **30 AWG stranded silicone-insulated wire** (stays flexible at −55 °C; PVC insulation goes brittle).
- **Strain relief is the #1 mechanical failure risk.** Bond a solder-terminal pad next to each gauge, take the fine gauge leads to that pad, and run the ruggedised wire from there. Never let flight loads reach the gauge's own foil tabs.
- Bundle the wires and anchor them to the panel with two adhesive tie-down points, so the 2.5 g landing shock is taken by the anchors, not the joints.
- Terminate at the payload PCB with **JST B2B-EH-A** connectors (explicitly flight-legal per the platform guide). **PLS-2 headers are forbidden for flight.**
- Coat bonded gauges with polyurethane conformal coating after calibration — this protects against condensation during descent (a real risk: the payload comes back through humid air while still at −30 °C, and frost forms).

### 9.7 So — is this a serious CubeSat experiment or just a cool prototype?

Honest answer: **it becomes a serious experiment only if three things are true**, and all three are achievable:

1. **The thermal output is cancelled, not hoped away.** Achieved by the back-to-back pair, and provable on the ground by putting the instrumented panel in a freezer *unloaded* and showing the difference channel stays flat while the sum channel swings by thousands of µε. **This single test is your credibility.**
2. **The reconstruction is validated against an independent measurement.** Achieved by a three-point bend fixture: push the panel centre by a known displacement measured with a dial indicator or feeler gauges, and show the reconstructed deflection matches to within a stated percentage. **Do this before flight and put the correlation plot in the presentation.**
3. **The predicted effect is large enough to be unambiguous.** It is: 200–600 µε predicted against a ~5 µε noise floor — a signal-to-noise ratio of 40–120. Compare that with idea C13 (radiation in the noise floor), where the predicted effect is *below* the drift. That comparison is why this concept is the flagship and that one is not.

If all three hold, you are not demonstrating a gadget — you are reporting a measurement with error bars, a validated reconstruction, and a hypothesis that the data either confirms or refutes. That is what the top band of the rubric describes.

---

## 10. ELECTRONICS ARCHITECTURES

### 10.1 Signal chain, stage by stage

```
PHYSICAL          →  TRANSDUCER    →  CONDITIONING        →  DIGITISE        →  PROCESS
─────────────────────────────────────────────────────────────────────────────────────────
panel curvature   →  2× BF350       →  half-bridge,        →  HX711 24-bit    →  offset/gain cal,
                     back-to-back      3.3 V ratiometric      PGA ×128,          median-of-5,
                                       excitation             80 SPS             κ = 2ε/t
─────────────────────────────────────────────────────────────────────────────────────────
coupon CTE        →  1× BF350       →  quarter bridge      →  HX711 ch. B     →  ε_T vs T_coupon
                     on free coupon    + 350 Ω ref            (PGA ×32)          → α(T)
─────────────────────────────────────────────────────────────────────────────────────────
panel vibration   →  35 mm piezo    →  1 MΩ ‖ 100 nF       →  ADC1 @ 2 kHz    →  Hann window,
                                       → STM32 OPAMP          via DMA            512-pt rFFT,
                                       (PE9/PE8/PE7), ×10     circular buffer    peak pick ×3
─────────────────────────────────────────────────────────────────────────────────────────
AE events         →  same piezo     →  STM32 COMP          →  TIM ext. count  →  hits/second
                                       (PB2 in, PC5 out)
─────────────────────────────────────────────────────────────────────────────────────────
structure temp    →  8× DS18B20     →  1-Wire, 4.7 kΩ      →  digital, 12-bit →  gradient dT/dx,
                     distributed       pull-up, 1 GPIO        1 Hz               rate dT/dt
─────────────────────────────────────────────────────────────────────────────────────────
convective h      →  3× heated tab  →  MOSFET duty cycle   →  DS18B20 pairs   →  ΔT_rise vs ρ
                     + reference       30 s on / 30 s off
─────────────────────────────────────────────────────────────────────────────────────────
sun / spin phase  →  4× BPW34       →  1 MΩ load resistor  →  CD74HC4051 →ADC →  sun vector,
                                                              5 Hz               spin rate
─────────────────────────────────────────────────────────────────────────────────────────
battery R_int     →  divider 100k/  →  100 nF filter       →  ADC, sampled    →  ΔV/ΔI at each
                     10k on 14.8 V                             pre/during TX      TX burst
─────────────────────────────────────────────────────────────────────────────────────────
altitude/att.     →  MS5611/LSM6DS3 →  (on-board)          →  I²C1, 10–104 Hz →  ρ = P/(R·T),
                     /LIS3MDL                                                     flight phase
```

### 10.2 Three front-end options, with the trade made explicit

| | **Option 1: HX711 ×5** ✅ | Option 2: ADS1220 | Option 3: STM32 OPAMP + internal ADC |
|---|---|---|---|
| Channels | 5 (+5 on ch. B) | 2 differential each | 1 (plus external mux) |
| Resolution | 24-bit, ×128 | 24-bit, ×128 | 16-bit + OPAMP gain |
| Temp range | −40…+85 °C | **−40…+125 °C** | −40…+85 °C (MCU) |
| Interface | bit-bang 2-wire | SPI2 | native |
| Cost | $6 total | $6 each | $0 (already there) |
| Availability in Tashkent | **good** | poor | n/a |
| Risk | proven, thousands of projects | fewer examples | needs HAL-level register work outside the Arduino core |
| **Verdict** | **Primary chain** | **Redundant chain, 2 stations** | **Use for the piezo, where its gain-bandwidth shines and 16 bits is plenty** |

Flying **two independent measurement technologies** for the same physical quantity is the single most effective thing you can do to protect the flight-performance score. If one chain dies at −45 °C, you still return data — and you also get a free result ("the HX711 stopped at −43 °C, the ADS1220 kept going"), which is itself a useful engineering finding.

### 10.3 Power budget

| Item | Rail | Current | Power | Duty | Average |
|---|---|---|---|---|---|
| 5 × HX711 + bridges (each full bridge = 2 parallel 700 Ω strings ≈ 9.4 mA) | 3.3 V | 47 mA | 155 mW | 100% | 155 mW |
| ADS1220 + bridges | 3.3 V | 20 mA | 66 mW | 100% | 66 mW |
| 3 × calorimeter heaters | 3.3 V | 3 × 33 mA | 300 mW | 50% | 150 mW |
| DS18B20 ×8 | 3.3 V | 8 mA (conv.) | 26 mW | 10% | 3 mW |
| Photodiodes + mux | 3.3 V | 1 mA | 3 mW | 100% | 3 mW |
| Vibration motor | 3.3 V | 80 mA | 264 mW | 0.2% | 0.5 mW |
| **Payload total** | | | | | **≈ 380 mW** |
| STM32H750 @ 200 MHz | 3.3 V | ~120 mA | 400 mW | 100% | 400 mW |
| microSD writes | 3.3 V | 100 mA burst | 330 mW | 5% | 17 mW |
| LoRa TX 30 dBm | 5 V | 650 mA | 3250 mW | 20% | 650 mW |
| LoRa idle | 5 V | 16 mA | 80 mW | 80% | 64 mW |
| **Grand total** | | | | | **≈ 1.5 W** |

Against a ~37 Wh pack (if 2.5 Ah — **confirm this**), a 3-hour flight at 1.5 W uses **4.5 Wh, about 12%.** You have enormous margin. **This is why you can afford redundancy, and why a bio-module heater at 4 W (12 Wh) is also affordable if the researcher needs it.** Note the LoRa TX peak of 650 mA on the 5 V rail against a 3 A limit — fine, but keep a 470 µF bulk capacitor near the payload's 5 V input to keep TX current spikes out of your analogue ground.

### 10.4 Grounding and EMI — do this or regret it

You are running a 1 W transmitter 100 mm from a microvolt-level bridge amplifier. Three mandatory rules:

1. **Star ground.** All bridge grounds return to one point at the HX711 cluster; that point connects to IntroBus GND by one trace. Do not let LoRa return current share a path with bridge return current.
2. **Blank the ADC during TX.** The firmware knows when it writes to `Serial1`. Discard or flag any strain sample whose conversion overlapped a TX burst. Better: schedule TX in a window between conversion cycles. This costs 5 lines of code and is measurable (that is experiment C30).
3. **Twist and shield the gauge wires.** Twisted pairs for each bridge; a grounded foil wrap on the bundle if you have it.

---

## 11. FIRMWARE ARCHITECTURE

### 11.1 Structure

```
setup()
 ├─ clock/GPIO init, all MOSFET gates LOW (heaters off, motor off)
 ├─ Wire1.begin();  init LSM6DS3(0x6A), LIS3MDL(0x1C), MS5611(0x77), LM75A(0x4A)   [100 ms delays]
 ├─ Wire4.begin();  scan payload bus (expansion / bio module)
 ├─ SD.setDx/ setCMD/ setCK;  sd_ok = SD.begin()        ← NEVER while(!SD.begin())
 ├─ open /LOG####.BIN with an incrementing index (never overwrite a previous flight)
 ├─ HX711: shared SCK low; power-up sequence; discard first 10 readings per channel
 ├─ read calibration table from /CAL.TXT (offsets, gains, gauge factors, thickness)
 ├─ ADS1220 init over SPI2 (redundant chain)
 ├─ ADC1 + DMA circular buffer for piezo @ 2 kHz;  COMP + TIM for AE counting
 ├─ 60 s ground-reference capture: zero all strain channels, record P0, T0
 └─ Serial1.begin(115200, SERIAL_8E1)   → LoRa

loop()   — cooperative scheduler, no delay() anywhere, millis()-based
 ├─ @2 kHz (DMA IRQ)  : piezo samples → ring buffer (4096 samples = 2 s pre-trigger)
 ├─ @80 Hz            : read 5× HX711 in one shared-clock loop (synchronous)
 │                      → median-of-5 → calibrated ε_top/ε_bot → ε_b, ε_T → κ
 ├─ @104 Hz           : LSM6DS3 accel+gyro;  RMS accumulator
 ├─ @10 Hz            : MS5611 P,T → altitude, ρ = P/(R·T);  LIS3MDL
 ├─ @5 Hz             : mux sweep photodiodes → sun vector, spin phase
 ├─ @1 Hz             : DS18B20 network; heater duty toggle; battery divider
 │                      512-pt FFT on the newest piezo window → top-3 peaks
 │                      flight-phase state machine update
 │                      anomaly detector update
 │                      build 58-byte TM packet → SD write + Serial1 TX
 └─ on EVENT          : freeze ring buffer → emit N event packets (type 0x02)
```

### 11.2 Key subsystems

**Synchronous 5-channel HX711 read (the core routine)**
```c
// Shared SCK on PC6; DOUT on PC7, PC4, PB9, PA4, PA5
bool hx_ready(void){                       // all five DOUT low = all converted
  return !(rd(PC7)|rd(PC4)|rd(PB9)|rd(PA4)|rd(PA5));
}
void hx_read_all(int32_t v[5]){
  for(int i=0;i<24;i++){
    SCK_HIGH(); delay_us(1);
    v[0]=(v[0]<<1)|rd(PC7);  v[1]=(v[1]<<1)|rd(PC4);
    v[2]=(v[2]<<1)|rd(PB9);  v[3]=(v[3]<<1)|rd(PA4);
    v[4]=(v[4]<<1)|rd(PA5);
    SCK_LOW();  delay_us(1);
  }
  for(int p=0;p<1;p++){ SCK_HIGH(); delay_us(1); SCK_LOW(); delay_us(1); } // gain 128, ch A
  for(int i=0;i<5;i++) if(v[i]&0x800000) v[i]|=~0xFFFFFF;                  // sign-extend
}
```
One loop, five channels, sampled at the same instant. ~1 ms per sample set at 80 SPS.

**Calibration model** (applied in firmware, coefficients loaded from the SD card so you can re-calibrate without reflashing):
```
ε_i  = (raw_i − offset_i) · scale_i                    [µε]
ε_b  = (ε_top − ε_bot) / 2                             bending
ε_T  = (ε_top + ε_bot) / 2                             thermal output
κ    = 2·ε_b / t                                       curvature  [1/m]
α(T) = dε_T/dT + α_gauge                               in-flight CTE
```
`offset_i` is re-zeroed during the 60 s ground-reference capture; `scale_i` comes from the three-point-bend calibration (§14).

**Filtering.** Median-of-5 on each strain channel (kills single-sample spikes from EMI without smearing real transients), then a first-order IIR with τ = 1 s **on the telemetry copy only** — the SD log keeps the unfiltered stream, so the ground analysis can always go back to the raw data. Never filter your only copy.

**Timestamping.** `millis()` since boot in the packet; additionally log the MS5611 pressure, which gives an independent altitude-based time reference if the clock misbehaves in the cold (experiment C26 quantifies exactly how much it does).

**Flight-phase state machine**
```
PAD      → ASCENT   : dP/dt < −2 hPa/s sustained 10 s
ASCENT   → FLOAT    : |dP/dt| < 0.2 hPa/s for 30 s and altitude > 15 km
FLOAT    → BURST    : |a| spike > 0.15 g  OR  dP/dt sign reversal
BURST    → DESCENT  : dP/dt > +5 hPa/s sustained 10 s
DESCENT  → LANDED   : |a| ≈ 1 g steady and |dP/dt| < 0.1 hPa/s for 60 s
```
Every record is tagged with the phase — which makes the ground analysis trivially sliceable and shows the jury you thought about the mission as a timeline, not a data dump.

**Anomaly detection** (running z-score, no floating-point library bloat):
```c
// per channel, exponentially weighted mean & variance
m  += (x - m) * ALPHA;
v  += ((x-m)*(x-m) - v) * ALPHA;
z   = (x - m) / sqrtf(v + 1e-6f);
if (fabsf(z) > 4.0f) trigger_event(ch, z);
```
A trigger sets a bit in the telemetry `anomaly_flags` byte and freezes the piezo ring buffer for event-packet dump.

**Fault handling — the part that protects your 15 points**

| Fault | Detection | Response |
|---|---|---|
| SD card absent/failed | `SD.begin()` returns false; write returns 0 | **Continue flying.** Set status bit; switch telemetry to full rate to use the radio as the recorder |
| HX711 channel stuck | DOUT never goes low within 200 ms | Mark channel dead in `payload_status_bits`; exclude from reconstruction; reduce to a 3-station fit |
| I²C bus hang | Transaction timeout | Re-init the bus (toggle SCL 9×, re-`begin()`); count occurrences |
| Sensor out of range | Value outside physical bounds | Substitute last-good, set a flag — **never let a bad value silently enter a plot** |
| Watchdog | IWDG, 8 s timeout | Reset and resume; append to the existing log file, do not overwrite |
| Brown-out | BOR + battery voltage channel | Log the event; shed the heaters first |
| LoRa buffer overrun | >4096 B queued, or no 0x06 ACK | Drop the *oldest* packet; never block the main loop waiting for the radio |

**The single biggest firmware trap in the platform's own documentation:** the official example uses `while(!SD.begin());`. If the card is not detected — which is a realistic cold-temperature failure — **your satellite hangs on the pad and you score zero.** Replace it everywhere.

### 11.3 Ground-station software

Python 3, ~300 lines:
```
pyserial → frame sync (0xAA 0x55) → length/type → CRC16 check
         → struct.unpack into a record
         → append to CSV + in-memory ring
         → live plots (pyqtgraph): altitude, temperatures, ε_b1..ε_b4, f_peak, RSSI
         → 3D panel reconstruction (matplotlib 3D or a small WebGL page)
         → packet-loss / sequence-gap counter  ← this is experiment C29
```
Write the CSV **immediately on receipt**, before any plotting. A ground station that crashes during the flight must not take the data with it.

---

## 12. TELEMETRY AND DATA ARCHITECTURE

### 12.1 The radio's real constraints

From the platform manual, the E32-433T30D:
- fragments everything into **58-byte packets** with an inter-packet delay (so 58 bytes costs barely more than 57, but 59 costs noticeably more);
- default air rate **2400 baud**, selectable up to 19200;
- the comms board buffers **≤ 4096 bytes** and returns **ACK 0x06** when a burst has gone out;
- the manual explicitly advises: *"Do not transmit character strings, better to transmit only data"*, and recommends a preamble, a postamble, and a packet-type byte.

**Airtime arithmetic:**

| Air rate | 58-byte packet on air | Max rate (ideal) | At 50% duty |
|---|---|---|---|
| 2400 | 193 ms | 5.2 pkt/s | 2.6 pkt/s |
| 4800 | 97 ms | 10.3 pkt/s | 5.2 pkt/s |
| 9600 | 48 ms | 20.7 pkt/s | 10.3 pkt/s |
| 19200 | 24 ms | 41.4 pkt/s | 20.7 pkt/s |

**Recommendation: 2400 baud, 1 packet/second.** That is a 5× duty-cycle margin, maximum link range, and it still delivers **612 kB over a 3-hour flight**. Do not chase throughput you don't need — range and reliability are worth more than resolution you can get from the SD card anyway.

**Channel selection:** f = 410 + C MHz, channels 0–31. The manual warns against 433 MHz (amateur-heavy) and 434 MHz (LPD band), and says to **space at least 2 MHz from the launch operator's frequency**. Good candidates: **C = 15 (425 MHz), C = 18 (428 MHz), C = 21 (431 MHz)**. **Confirm with the organisers at the finals.** Set FEC = Enabled, Fixed mode = Disabled, Address = 0, power = 21 dBm for lab work and **30 dBm before flight**.

### 12.2 Main telemetry packet — exactly 58 bytes

Designed to land precisely on the radio's native fragment size, so every packet is one fragment with no partial-packet penalty.

| Offset | Field | Type | Encoding |
|---|---|---|---|
| 0–1 | sync | u8 ×2 | `0xAA 0x55` |
| 2 | packet type | u8 | `0x01` = TM_MAIN |
| 3–6 | t_ms | u32 | ms since boot |
| 7–8 | pressure | u16 | Pa / 4 (covers 0–262 kPa, 4 Pa resolution) |
| 9–10 | altitude | i16 | metres |
| 11–12 | T_ambient | i16 | °C × 8 (**native LM75A format — `GetTemperatureTimes8()`**) |
| 13–14 | T_panel_outer | i16 | °C × 8 (outer face of the instrumented panel) |
| 15–16 | T_panel_inner | i16 | °C × 8 (inner face — the pair gives ΔT_through) |
| 17–18 | T_calor_A | i16 | °C × 8 |
| 19–20 | T_calor_ref | i16 | °C × 8 |
| 21–22 | T_board | i16 | °C × 8 |
| 23–24 | ε_b1 | i16 | µε (bending strain, station 1). Curvature κ = 2ε_b/t is computed on the ground, so a later correction to the thickness or gauge factor does not invalidate the downlinked data |
| 25–26 | ε_b2 | i16 | µε |
| 27–28 | ε_b3 | i16 | µε |
| 29–30 | ε_b4 | i16 | µε |
| 31–32 | ε_T PLA coupon | i16 | µε (thermal output) |
| 33–34 | ε_T Al coupon | i16 | µε |
| 35–36 | a_x | i16 | mg |
| 37–38 | a_y | i16 | mg |
| 39–40 | a_z | i16 | mg |
| 41–42 | ω_z | i16 | 0.01 °/s |
| 43–44 | f_peak | u16 | 0.1 Hz (dominant vibration frequency) |
| 45–46 | a_rms | u16 | µg (vibration RMS this second) |
| 47 | AE hits | u8 | acoustic-emission events this second |
| 48 | flight phase | u8 | 0 pad · 1 ascent · 2 float · 3 burst · 4 descent · 5 landed |
| 49–50 | V_batt | u16 | mV |
| 51 | sensor health | u8 | bit per subsystem (SD, HX1–5, DS18B20 bus, ADS1220) |
| 52 | anomaly flags | u8 | bit per detector |
| 53–54 | sequence | u16 | wraps; used for packet-loss statistics |
| 55–56 | CRC16-CCITT | u16 | over bytes 2–54 |
| 57 | end | u8 | `0x0D` |

**58 bytes exactly.** Compared with the naive string form the manual warns about (`"t:12345;p:29305;T:-52.5;k1:412;..."` ≈ 180+ bytes and lossy), this is a **3× improvement in packet efficiency with no precision loss**. Explaining that trade-off in your defence is a direct hit on the *technical elaboration* criterion.

### 12.3 Event packet (type 0x02) — the high-value bandwidth

When the anomaly detector or the flight-phase machine fires, dump the **2 seconds preceding the trigger** from the piezo ring buffer, decimated and delta-encoded:
```
[0xAA 0x55][0x02][event_id u8][t_ms u32][trigger_ch u8][z_score i16]   ← 11 B header
[44 × i8 delta-encoded piezo samples]                                  ← 44 B payload
[CRC16][0x0D]                                                          ←  3 B tail
```
**58 bytes exactly**, same as the main packet, so both land on one radio fragment. A burst of **8 such packets covers the whole 2 s window at ~176 Hz effective.** Sent once per event, during descent when nothing else is competing for the link.

**This is the answer to "extract more scientific value from the same telemetry bandwidth":** spend 58 B/s on continuous low-rate summary, and reserve short bursts of high-rate data for the ~5 moments in the flight that actually contain transients. A fixed high-rate stream would need 30× the bandwidth to capture the same events.

### 12.4 Storage — three independent records

| Record | Medium | Rate | Purpose |
|---|---|---|---|
| Full raw log | microSD, binary | 80 Hz strain, 2 kHz piezo bursts, 1 Hz everything else | Primary science record. ~50 MB for a 3 h flight |
| Telemetry mirror | microSD, same 58-byte packets appended | 1 Hz | Lets you reconcile what was sent vs what was received → packet-loss analysis (C29) |
| Downlink | LoRa → ground CSV | 1 Hz | Survives even if the payload is lost or lands in water |

**The third one is the insurance that matters.** The organisers explicitly warn of a *"possible water/swamp landing."* If the payload is unrecoverable, the LoRa stream is your entire dataset — which is why the 1 Hz packet is designed to be *scientifically complete on its own*, not just a status ping. Every hypothesis in §8 can be tested using only the downlinked packets. That is a deliberate design decision and worth stating explicitly in your defence.

### 12.5 Example telemetry, decoded

```
timestamp_ms  alt_m  P_hPa  T_amb  T_out  T_in    eb1  eb2  eb3  eb4  eT_PLA  eT_Al  a_rms  f_peak  AE  phase
    0         112   1000.1  +21.4  +22.1   +21.9    -2   +1    0   -1      3      1      41    0.0    0   PAD
  600000     3021    701.2   -4.2   +6.8    -2.1   -38  -61  -55  -31   -1420   -310    58   62.4    0   ASCENT
 1800000    10980    226.4  -55.9  -28.4   -44.2  -143 -239 -218 -126   -3620   -790   112   66.1    2   ASCENT
 3600000    19870     55.1  -56.2  -18.9   -49.8  -207 -344 -318 -180   -4040   -868    74   70.8    1   FLOAT
 4210000    20050     54.4  -55.8  -17.2   -48.9  -201 -338 -310 -176   -4010   -861   980   71.2    9   BURST
 4900000     8430    342.9  -31.4   -9.8   -26.7  -101 -172 -158  -92   -2180   -470   410   64.9    3   DESCENT
```

Read that table as an engineer: bending strain grows monotonically with the panel's through-thickness gradient (T_out − T_in reaches 31 K at float; 344 µε is about a third of the 1050 µε a *free* PLA plate would show, so the edge constraint is reacting roughly two-thirds of the thermal moment — itself a quantitative result); ε_T tracks the coupon temperature; f_peak rises from 62.4 → 71.2 Hz as the panel stiffens (**+14%, matching H4**); a_rms spikes at burst and AE hits cluster there. **That is a complete scientific story in 58 bytes per second.**

---

## 13. DYNAMIC VISUALISATION

You specifically asked for an experiment whose result can be shown *moving*. Here is exactly how.

### 13.1 The chain, end to end

```
panel bends  →  8 gauges change resistance  →  5 bridges  →  HX711 ×5
   →  STM32 computes ε_b,i = (ε_top − ε_bot)/2   [the thermal output cancels here]
   →  58-byte packet, 1 Hz, over LoRa
   →  LoRa-UART dongle on the ground-station laptop
   →  Python: CRC check → unpack → ε_b1..ε_b4 → κ_i = 2ε_b,i/t
   →  Ko double-integration → deflection w(x) at 6 stations
   →  cubic spline → 60-point curve → extrude across the panel width
   →  3D surface, colour-mapped by temperature, redrawn at 1 Hz
   →  a projector showing the panel visibly bowing as the balloon climbs
```

### 13.2 The mathematics, concretely

With 4 measured curvatures plus two boundary conditions, for the instrumented span L = 150 mm:

```python
import numpy as np
from scipy.interpolate import CubicSpline

t   = 1.5e-3                       # panel thickness, m
x   = np.array([0, 22, 52, 98, 128, 150])*1e-3     # boundary + 4 stations + boundary
def reconstruct(eps_b_ue):
    kappa = np.concatenate(([0], 2*np.array(eps_b_ue)*1e-6/t, [0]))   # 1/m
    theta = np.concatenate(([0], np.cumsum((kappa[:-1]+kappa[1:])/2*np.diff(x))))
    w     = np.concatenate(([0], np.cumsum((theta[:-1]+theta[1:])/2*np.diff(x))))
    # enforce the second boundary condition (both edges clamped): remove the linear trend
    w    -= np.linspace(0, w[-1], len(w))
    return CubicSpline(x, w)       # call at 60 points for a smooth curve
```

For the **2D** version with κ_x, κ_y and κ_xy, use the Ritz least-squares fit from §9.4 and evaluate `w(x,y)` on a 20 × 20 grid.

**Sanity check (computed, not asserted):** four stations reading −207, −344, −318, −180 µε on a 1.5 mm panel give curvatures of −0.276, −0.459, −0.424, −0.240 m⁻¹, which integrate to a peak deflection of **1.15 mm** at x = 52 mm. The full scaling is: 100 µε → 0.33 mm, 200 µε → 0.66 mm, 300 µε → 1.00 mm, 400 µε → 1.33 mm, 600 µε → 1.99 mm. A millimetre is visible to the naked eye on a physical panel, which matters enormously for the demonstration — you can hold up the panel, push it, and the screen moves.

### 13.3 The demonstration that wins the room

Bring the **spare instrumented panel** to the defence, connected to a laptop.

1. Show the live 3D reconstruction with the panel at rest. Flat.
2. Press the panel centre with a finger. **The surface on screen bows in real time.** Everyone in the room now understands what your sensor does, in two seconds, without a single equation.
3. Point a hot-air gun or an ice pack at one side. **The surface bows the other way, and the colour map shifts** — showing thermal deformation live.
4. Then play the flight recording back through the same viewer, at 60× speed: two hours of flight in two minutes, the panel visibly bowing as the altitude counter climbs, snapping at burst, relaxing on the way down.

That last point is the key: **the same software does the live demo and the flight playback.** Build one tool, use it twice.

### 13.4 Implementation options, ranked by effort

| Approach | Effort | Notes |
|---|---|---|
| `matplotlib` `plot_surface`, redrawn at 1 Hz | **4 hours** | Ugly but works; fine for the MVP |
| `pyqtgraph` `GLSurfacePlotItem` | **1 day** | Smooth, 60 fps, best effort/result ratio ✅ |
| Browser page + `three.js`, fed by a small websocket bridge | 2–3 days | Prettiest; also the easiest thing to screen-record for the presentation |
| Blender / Unity | ✗ | Not worth it in 38 days |

**Recommendation:** `pyqtgraph` for the live tool, plus a recorded screen capture of the flight playback embedded in the presentation (so a laptop failure on defence day cannot cost you the demo).

---

## 14. GROUND TESTING — proving it works before it flies

Rubric line 5 is worth **8 points** for *"a full cycle of tests… all systems work properly."* This is the cheapest scoring opportunity in the whole competition, and it is entirely under your control.

### 14.1 Minimum viable test set (must do — 1 day total)

| # | Test | Method | Pass criterion |
|---|---|---|---|
| T1 | **Thermal-output cancellation** | Put the *unloaded* instrumented panel in a kitchen freezer (−18 °C). Log for 60 min | **Difference channel drifts < 30 µε; sum channel swings > 1500 µε.** This one test proves the entire measurement principle |
| T2 | **Bending calibration** | Three-point bend fixture (two supports + a micrometer screw or stacked feeler gauges). Apply 0, ±0.25, ±0.5, ±1.0, ±1.5 mm | Linear response, R² > 0.99; reconstructed deflection within **±10%** of the dial-indicator reading |
| T3 | **Channel cross-talk** | Load station 1 only; read all stations | Stations 2–4 respond in the pattern predicted by beam theory, not randomly |
| T4 | **Calorimeter baseline** | Run the heaters at ground pressure, record ΔT_rise | ≈ 16 K ± 5 K, stable, repeatable across three cycles |
| T5 | **Data-path integrity** | Run the full firmware for 3 h on the bench | Zero CRC failures; SD log and received CSV match packet-for-packet |
| T6 | **Radio blanking / EMI** | Key the LoRa at 30 dBm during strain acquisition | Strain noise increase < 5 µε with blanking enabled (**and show the un-blanked case for contrast — that's experiment C30**) |
| T7 | **Power-on / power-cycle** | 20 cold power cycles | Boots every time, appends to the log, never overwrites |

### 14.2 Advanced tests (should do — 2–3 days)

| # | Test | Method | What it proves |
|---|---|---|---|
| T8 | **Full thermal range** | Dry ice in an insulated cooler reaches **−78 °C** — colder than the flight. Ramp down over 60 min to mimic the ascent rate | Everything survives and keeps measuring below −55 °C. **Identifies the exact temperature at which each component quits** — which is itself a result |
| T9 | **Thermal cycling** | 5 cycles, +25 °C ↔ −40 °C | Gauge bonds survive; calibration repeats within 5%; no solder-joint cracking |
| T10 | **Reduced pressure** | Hand vacuum pump + a jar gets to ~100 hPa; a $60 two-stage rotary-vane pump gets far below 30 hPa | Calorimeter ΔT_rise increases as predicted; no corona; no outgassing from adhesives |
| T11 | **Combined cold + low pressure** | Dry ice *inside* the vacuum jar | The real flight condition. If the payload passes T11, it will fly |
| T12 | **Modal identification** | Vibration motor swept 10–200 Hz, or tap-test with a small hammer; FFT the piezo | First-mode frequency identified at room temperature; repeat inside the freezer to confirm the predicted upward shift |
| T13 | **Vibration & shock** | Bolt the assembled CubeSat to a small speaker cone or an orbital sander for 10 min; then a 200 mm drop onto foam (≈2.5 g) | Nothing loosens; all channels still read after |
| T14 | **Radio range** | Two people, 2–5 km line of sight, log RSSI and PER | Link margin confirmed at 30 dBm. Extrapolate to the 25–80 km slant range |
| T15 | **End-to-end rehearsal** | Full 3 h flight simulation: batteries only, real telemetry, ground station running, someone acting as flight director | The most valuable test you will run. Everything that will go wrong on launch day goes wrong here instead |

### 14.3 The calibration procedure (do this once, carefully, and record it)

1. **Zero:** with the panel unloaded at a known temperature, record `offset_i` for all channels (60 s average).
2. **Span:** three-point bend to a known centre deflection δ; compute the expected surface strain ε = 4·δ·t/L²; set `scale_i` so the measured value matches.
3. **Verify:** apply a *different* deflection and check the reconstruction, not just the raw channel.
4. **Thermal:** repeat the zero at −18 °C and confirm the offsets have not shifted more than your stated tolerance.
5. **Write** `offset_i`, `scale_i`, panel thickness, gauge factor and sensor positions into `/CAL.TXT` on the SD card.
6. **Photograph everything.** The calibration fixture, the dial indicator, the plots. Rubric lines 3 and 5 are both about evidence, and photographs of a real fixture are worth more than a paragraph claiming you calibrated.

---

## 15. FAILURE MODES AND MITIGATIONS

| # | Failure | Likelihood | Effect | Detection | Mitigation / tolerance |
|---|---|---|---|---|---|
| F1 | **Strain gauge debonds** | Medium | Channel reads garbage or rails | Value outside ±5000 µε; sudden step | 5 stations, reconstruction degrades gracefully to 3; cyanoacrylate + polyurethane coat; practise on 3 sacrificial panels first |
| F2 | **Gauge lead wire breaks at the landing shock** | Medium | Channel open-circuit | HX711 output rails to full scale | Strain-relief pads (§9.6); wire anchors; redundant ADS1220 chain on 2 stations |
| F3 | **HX711 stops below −40 °C** | **Medium-High** | Entire primary strain chain lost | DOUT never asserts ready | **ADS1220 (−40…+125 °C) redundant chain**; mount the payload PCB on the warm side, near the battery pack; insulate the PCB but *not* the panel |
| F4 | **microSD fails in the cold** | **High** — consumer cards are often only 0…+70 °C | Full raw log lost | `SD.begin()` false; write returns 0 | **Buy an industrial-grade card (−40…+85 °C)**; never `while(!SD.begin())`; on failure, raise telemetry rate and rely on the downlink |
| F5 | **ADC saturation** | Low | Clipped strain data | Value at ±full-scale | Half-bridge full scale at ×128 is **±3906 µε** — **6–10× the predicted maximum** of 200–600 µε. Auto-fallback to gain ×64 (→ ±7800 µε) if a channel clips twice. Note the CTE coupons run as *quarter* bridges, full scale ±7812 µε, which comfortably covers the −4130 µε thermal output |
| F6 | **EMI from 1 W LoRa TX corrupts strain** | **High if unmitigated** | Periodic 1 Hz artefacts in the strain data | Artefact correlates with the TX schedule | Star ground; blank ADC during TX; twisted pairs; median-of-5 filter; **and quantify it as experiment C30** |
| F7 | **Thermal drift of offsets** | Certain | Slow baseline wander | Sum vs difference channels diverge | Back-to-back pairs cancel it by construction; ratiometric HX711; unheated reference; post-flight drift correction using the pad/landed segments |
| F8 | **Calibration drift over the flight** | Medium | Absolute error grows | Compare pre-launch and post-landing zero | Bracket the flight with two zero captures; report strain *changes*, not absolutes, wherever possible |
| F9 | **MCU hang / watchdog reset** | Low | Data gap | Sequence number jumps; boot counter | IWDG 8 s; append-not-overwrite log files; boot counter in telemetry |
| F10 | **Total loss of radio link** | Medium (it is a 2 W-ERP link over up to 80 km) | No live data | No packets at the ground station | SD card is the primary record; telemetry is designed to be redundant, not essential — **and vice versa** |
| F11 | **Payload not recovered (water landing)** | Medium — explicitly warned by organisers | SD card lost | Obvious | **The 1 Hz downlink is scientifically complete on its own** (§12.4). This is the design decision that protects you |
| F12 | **Sampling rate too low to see a transient** | Low | Burst/parachute event under-resolved | Aliased or single-sample spikes | 80 SPS strain + 2 kHz piezo + 2 s pre-trigger ring buffer |
| F13 | **Resolution insufficient** | Very low | Signal buried | SNR check during T2 | Predicted SNR 40–120 — verified before flight, not assumed |
| F14 | **False anomaly detections flood the link** | Medium | Event packets crowd out telemetry | Event rate counter | Hard cap: max 1 event burst per 60 s; events queued, not sent immediately; telemetry always has priority |
| F15 | **Data corruption in transit** | Certain at some rate | Bad records | CRC16 fails | CRC on every packet; ground station logs *both* good and bad frames for later forensic use |
| F16 | **Power channel auto-disconnects on overcurrent** | Low | Payload dies silently | Payload status bits go stale | Total payload draw 380 mA peak vs a 3 A limit — **8× margin**; 470 µF bulk cap; fuse the heater branch |
| F17 | **Battery voltage sags in the cold** | Medium | Brown-out | V_batt channel | Shed heaters first at a defined threshold; the pack is heated by the organisers; **and measure it as experiment C28** |
| F18 | **Condensation/frost on descent** | Medium | Bridge leakage, phantom strain | Impossible-rate strain changes coincident with descent | Conformal coating on all bonded gauges and the payload PCB |
| F19 | **You break the panel during integration** | **Medium — be honest** | No mission | Obvious | **Build two complete instrumented panels.** The spare is also your demo unit (§13.3) |
| F20 | **A mandatory online assignment is missed** | Medium | **Disqualification** (Regulations §6.6.11) | Calendar | Put every webinar and assignment deadline in a shared calendar **today** |

**Note the shape of this table.** Most of the high-likelihood failures are *thermal* or *procedural*, not scientific. That is the real lesson of the ORBITA 2025 outcome — the platform worked, the payloads didn't.

---

## 16. COMPONENT AND COST ANALYSIS

### 16.1 Bill of materials — SKIN-24 + THERMOS + BLACKBOX

| Part | Function | Interface | Voltage | Qty | Unit $ | Total $ | Availability | Alternatives | Key limitation |
|---|---|---|---|---|---|---|---|---|---|
| **BF350-3AA** foil strain gauge, 350 Ω, GF 2.0 | Strain transduction | analogue | — | 18 (12 used + 6 spare) | 0.50 | 9.00 | AliExpress packs of 10; some Tashkent shops | BF120-3AA (120 Ω, 3× the power); HT350 | Bonding skill is the limiting factor, not the part |
| **HX711** breakout module | 24-bit ratiometric bridge ADC, PGA ×128 | 2-wire bit-bang | 3.3 V | 6 (5 + spare) | 1.20 | 7.20 | **Widely available in Tashkent** (`electron.uz`, `uzum.uz`) | ADS1220, ADS1232, NAU7802 | −40…+85 °C; 10 SPS default — **must mod the RATE pad for 80 SPS** |
| **ADS1220** breakout | Redundant bridge ADC | SPI2 | 3.3 V | 1 | 6.00 | 6.00 | AliExpress; scarce locally | NAU7802 (I²C, −40…+85) | Harder firmware; worth it for the −40…+125 °C range |
| **DS18B20** TO-92 | Distributed temperature network | 1-Wire, 1 GPIO | 3.3 V | 12 | 1.00 | 12.00 | Very common | LM75A on I²C4 (4 addresses); MCP9808 | Parasitic power is unreliable — use 3-wire |
| **35 mm piezo disc** | Vibration + acoustic emission | analogue | — | 3 | 0.30 | 0.90 | Common | Any piezo buzzer element | Cannot measure static strain |
| **CD74HC4051** analogue mux | Photodiode/NTC scanning | 3 GPIO + ADC | 3.3 V | 2 | 0.50 | 1.00 | Common | CD74HC4067 (16 ch) | 70 Ω R_on — keep out of microvolt paths |
| **BPW34** photodiode | Sun vector / spin phase | ADC via mux | — | 6 | 0.50 | 3.00 | Common | LDR (slower, cheaper); TEMT6000 | Needs a 1 MΩ load; wide dynamic range |
| **REF3030** 3.0 V reference | ADC drift mapping | analogue | 3.3 V | 1 | 2.00 | 2.00 | AliExpress | LM4040-3.0 | ±0.2% initial; the tempco is what matters |
| **AO3400 / IRLML2502** N-MOSFET | Heater + motor switching | GPIO | 3.3 V gate | 6 | 0.10 | 0.60 | Common | 2N7002 (lower current) | Must be logic-level |
| 100 Ω 1 W metal-film resistor | Calorimeter heater | — | 3.3 V | 4 | 0.10 | 0.40 | Common | Any 1 W resistor | Solder to a copper tab for known area |
| Pager vibration motor | Modal excitation | GPIO + MOSFET | 3.3 V | 2 | 0.60 | 1.20 | Common | Small speaker + amplifier | 80 mA — must be switched, not GPIO-driven |
| **Industrial microSD 8–16 GB (−40…+85 °C)** | Primary data record | SDMMC2 | 3.3 V | 2 | 12.00 | 24.00 | Order online — **long lead time** | SanDisk Industrial; Kingston SDCIT | **Consumer cards are often 0…+70 °C. This is failure mode F4** |
| Payload PCB, 2-layer, 90 × 90 mm | Integration | — | — | 5 (min order) | 2.50 | 12.50 | JLCPCB — **~10–14 days to Tashkent** | Protoboard for L1/L2 | Order by day 10 or go with protoboard |
| **JST B2B-EH-A** + 2-pin terminal blocks | Flight-legal connections | — | — | — | — | in kit | — | — | **PLS-2 headers are forbidden for flight** |
| 30 AWG silicone-insulated wire | Gauge wiring | — | — | 10 m | 0.40/m | 4.00 | Common | PTFE-insulated wire | **PVC insulation goes brittle at −55 °C** |
| Cyanoacrylate + polyurethane coating | Gauge bonding + protection | — | — | — | — | 5.00 | Common | M-Bond 200 (proper but scarce) | Cure fully before calibration |
| PETG / PLA sheet 1.5 mm + FR4 + Al coupons | Instrumented panel + CTE coupons | — | — | — | — | 8.00 | Local | 3D-printed panels | Print orientation affects α — document it |
| 470 µF low-ESR capacitor | TX current-spike decoupling | — | 16 V | 2 | 0.30 | 0.60 | Common | 220 µF ×2 | Keep close to the payload 5 V input |
| **TOTAL** | | | | | | **≈ $97** | | | |

Trim to the **~$45 minimum viable set** by dropping the ADS1220, the REF3030, one spare panel, the custom PCB (use protoboard) and the second industrial SD card. **Do not trim the industrial SD card or the spare strain gauges.**

### 16.2 Sourcing strategy — this is your real critical path

| Source | Lead time | Use for |
|---|---|---|
| **Local Tashkent electronics shops** (`electron.uz`, Prom.uz vendors, radio-parts markets) | **same day** | HX711, DS18B20, MOSFETs, resistors, piezo discs, wire, muxes, photodiodes — **buy everything you can here** |
| **Uzum Market** (`uzum.uz`) | 1–3 days | Arduino/STM32 sensor kits, jumpers, breadboards |
| **AliExpress** | **2–4 weeks to Uzbekistan** | Strain gauges, ADS1220, industrial SD. **Order in the next 48 hours or not at all** |
| **JLCPCB / PCBWay** | 10–14 days to Tashkent | Payload PCB. Order by day 10 (28 Sep) at the latest |

> ⚠️ **Your project will not fail on firmware. It will fail because a $4 packet of strain gauges took five weeks to arrive.** Place the AliExpress order today, with 2× the quantity you need, and design the fallback (protoboard + locally-bought parts) so that the mission still flies if nothing arrives.

---

## 17. ENGINEERING COMPARISON AND DEVELOPMENT TIME

### 17.1 Scoring methodology (stated before the scores, as requested)

Each criterion is scored **1–5** against fixed anchors. These are **my engineering estimates given your stated constraints, not objective measurements.** A different technician with different skills would score differently, and that is legitimate.

| Score | Feasibility | Dev time | Cost | Elec. complexity | Prog. complexity | Mech. complexity | Eng./sci. value | Testability | Data quality | Novelty potential | CubeSat compat. | Failure risk | Demo quality |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| **5** | certain | ≤3 days | <$10 | trivial | trivial | none | high transfer to real spacecraft | fully testable on a desk | dense continuous curves | class C/D | drops straight in | negligible | live, moving, obvious |
| **3** | likely | 1–2 wk | $30–60 | moderate | moderate | some fabrication | useful | partially testable | discrete points | class B | needs adaptation | moderate | static plots |
| **1** | doubtful | >4 wk | >$150 | hard analogue | hard DSP/ML | precision mechanism | little transfer | untestable pre-flight | single before/after | class A | conflicts | high | nothing to show |

Note: for *Development time*, *Cost*, *Complexity* and *Failure risk*, **5 = better** (faster/cheaper/simpler/safer).

### 17.2 Comparison of the strongest concepts

| Criterion | **SKIN-24** (strain skin) | **THERMOS** (convective) | **BLACKBOX** (firmware) | Modal thermometry (C4) | Geiger/Pfotzer (C12) | Ozone UV ratio (C14) | Ultrasonic density (C17) | Bio-only (Missions 1–3) |
|---|---|---|---|---|---|---|---|---|
| Feasibility | 4 | **5** | **5** | 4 | 4 | 3 | 3 | 2 |
| Development time | 3 | **5** | 4 | 4 | 3 | 3 | 3 | 2 |
| Cost | 4 | **5** | **5** | **5** | 3 | 4 | **5** | 3 |
| Electronics complexity | 2 | **5** | **5** | 4 | 3 | 4 | 4 | 3 |
| Programming complexity | 3 | **5** | 2 | 3 | 4 | 4 | 3 | **5** |
| Mechanical complexity | 3 | **5** | **5** | 4 | 4 | 4 | 4 | 1 |
| Engineering/scientific value | **5** | 4 | 3 | 4 | 3 | 3 | 2 | 4 |
| Testability on the ground | **5** | **5** | **5** | 4 | 3 | 2 | 4 | 2 |
| Data quality (density, continuity) | **5** | **5** | 4 | 4 | 4 | 3 | 2 | 1 |
| Novelty potential | **5** (C/D) | 4 (C) | 3 (B) | 4 (B/C) | 1 (A) | 2 (A/B) | 3 (B) | 3 (B) |
| CubeSat compatibility | **5** | **5** | **5** | **5** | 3 | 4 | 4 | 2 |
| Failure risk (5 = low) | 3 | **5** | **5** | 4 | 3 | 3 | 2 | 2 |
| Demonstration quality | **5** | 3 | 3 | 4 | 3 | 2 | 3 | 4 |
| **TOTAL (of 65)** | **52** | **57** | **54** | **53** | **41** | **41** | **42** | **34** |

**How to read this table.** THERMOS scores highest — because it is nearly risk-free, not because it is the most impressive. SKIN-24 scores lower on risk and complexity but far higher on value, novelty and demonstration. **That is exactly why you fly both.** THERMOS is the floor under your score; SKIN-24 is the ceiling. BLACKBOX costs nothing and lifts both.

The bio-only column scores lowest not because the biology is bad but because, as a *flight payload assessed by this rubric*, it produces no in-flight data, cannot be ground-tested meaningfully, and carries the highest failure risk (live organism, life support, recovery-dependent). Adding your instrumentation to it moves several of those 1s and 2s to 4s.

### 17.3 Development time — three levels

#### LEVEL 1 — Proof of concept *(target: 3–5 days)*
**Goal:** prove the measurement principle exists, on a desk, with parts you can buy today.
- **Components:** 2 strain gauges, 1 HX711, 1 Arduino or the FCM itself, 1 piece of plastic sheet, 1 DS18B20, 1 resistor + MOSFET.
- **Circuit:** one back-to-back pair → half bridge → HX711 → MCU.
- **Firmware:** read HX711, print to serial. ~60 lines.
- **Mechanical:** none — tape the coupon to a table edge.
- **Test:** press the coupon, see the number move. Put it in the freezer, see the difference channel stay flat.
- **Expected result:** a plot showing bending response and thermal-output cancellation. **This one plot de-risks the entire mission.**

#### LEVEL 2 — Engineering prototype *(target: 10–14 days cumulative)*
**Goal:** the full measurement system, on protoboard, with real firmware and a live visualisation.
- **Components:** 8 gauges, 5 HX711 (shared clock), 8 DS18B20, piezo + OPAMP, 3 calorimeter elements, mux + photodiodes, protoboard.
- **Circuit:** the full §10.1 chain, hand-wired, with a star ground.
- **Firmware:** scheduler, synchronous 5-channel read, calibration from SD, 58-byte packets, FFT, state machine, fault handling.
- **Mechanical:** one instrumented panel, properly bonded, with strain relief; three-point bend fixture.
- **Test:** T1–T7 complete; live 3D reconstruction working.
- **Expected result:** validated calibration (±10%), a working live demo, 3 h of bench data with zero CRC errors.

#### LEVEL 3 — Flight / competition version *(target: 21–25 days cumulative)*
**Goal:** integrated into the 3U, environmentally qualified, documented.
- **Components:** everything on a custom PCB (or a very neat protoboard), JST/terminal-block connections only, industrial SD card, ADS1220 redundant chain, conformal coating.
- **Circuit:** as L2 plus EMI mitigation, bulk decoupling, fusing.
- **Firmware:** as L2 plus watchdog, event packets, blanking, heater shedding, boot counter.
- **Mechanical:** two complete instrumented side panels (flight + spare/demo), payload PCB mounted on standoffs from the intermediate frame, full 3U assembly ≤ 3 kg.
- **Test:** T8–T15 complete, including the combined cold + vacuum test and a full 3 h rehearsal.
- **Expected result:** a flight-ready payload with a signed-off test report, calibration record, mass budget, power budget, and a rehearsed presentation.

---

## 18. DAY-BY-DAY DEVELOPMENT PLAN
**From today (Fri 18 Sep 2026) to the hard deadline (Mon 26 Oct 2026) — 38 days.**

Priorities: **[M] Must-have** · **[S] Should-have** · **[N] Nice-to-have**

### Phase 0 — Unblock the critical path (Days 0–2, Fri 18 – Sun 20 Sep)

| Day | Task | Pri |
|---|---|---|
| **0 — Fri 18** | **Place the AliExpress order** (strain gauges ×20, ADS1220, industrial microSD ×2, REF3030). Nothing else matters today | **[M]** |
| 0 | Email the organisers the §1.6 question list (altitude, recovery, prohibited payloads, LoRa channel, battery capacity, live-animal rules, panel cut-out locations) | **[M]** |
| 0 | Put every webinar + mandatory-assignment deadline into a shared team calendar (§3.4 C4, failure mode F20) | **[M]** |
| 1 — Sat 19 | Buy locally: HX711 ×6, DS18B20 ×12, piezo ×3, MOSFETs, resistors, mux, photodiodes, 30 AWG silicone wire, protoboard | **[M]** |
| 1 | Team meeting: **fill the empty Engineer/CAD seat**; confirm the radio technician; agree the integrated mission (SKIN-24 + bio-module) | **[M]** |
| 2 — Sun 20 | Confirm kit receipt (Regulations §6.6.6 requires documentary evidence — take the team photo with the kit) | **[M]** |
| 2 | Set up the toolchain: Arduino IDE + stm32duino + IntroStratLib + STM32SD + STM32CubeProgrammer; flash the LED blink example over SWD | **[M]** |

### Phase 1 — Proof of concept (Days 3–9, Mon 21 – Sun 27 Sep)

| Day | Task | Pri |
|---|---|---|
| 3 — Mon 21 | Bond 2 practice gauges to a scrap coupon. **Expect the first two to be bad.** Bond 2 more | **[M]** |
| 4 — Tue 22 | HX711 + one half-bridge reading on the FCM; verify the 24-bit read and sign extension | **[M]** |
| 5 — Wed 23 | **Freezer test T1** — thermal-output cancellation. *This is the go/no-go gate for the whole mission* | **[M]** |
| 6 — Thu 24 | Three-point bend fixture built; first calibration curve (T2) | **[M]** |
| 7 — Fri 25 | Calorimeter breadboard: 1 heater + 2 DS18B20 + MOSFET; measure ΔT_rise at ground (T4) | **[M]** |
| 8 — Sat 26 | Shared-clock 5× HX711 read routine working (even with only 2 boards populated) | **[M]** |
| 9 — Sun 27 | **Milestone review.** If T1 failed, switch primary to THERMOS and demote SKIN-24 to a secondary channel | **[M]** |

### Phase 2 — Engineering prototype (Days 10–16, Mon 28 Sep – Sun 4 Oct)

| Day | Task | Pri |
|---|---|---|
| 10 — Mon 28 | **Order the payload PCB** (or commit to protoboard). Design the panel in CAD with the engineer | **[M]** |
| 10 | Fabricate instrumented panel #1: 8 gauges, strain relief pads, JST terminations | **[M]** |
| 11 — Tue 29 | DS18B20 network on one GPIO; enumerate and map all 8 ROM IDs to physical positions | **[M]** |
| 12 — Wed 30 | Piezo → platform OPAMP → ADC + DMA at 2 kHz; verify with a tap test | **[S]** |
| 13 — Thu 1 Oct | CMSIS-DSP 512-point FFT; peak picking; verify against a known tone from a phone speaker | **[S]** |
| 14 — Fri 2 | 58-byte binary packet format + CRC16; SD logging with the append-not-overwrite scheme | **[M]** |
| 15 — Sat 3 | LoRa configured (AT commands, channel, FEC on, 21 dBm for lab); end-to-end link to the ground station | **[M]** |
| 16 — Sun 4 | **Ground station v1**: decode → CSV → live plots. *Data must be landing on disk by tonight* | **[M]** |

### Phase 3 — Flight build and calibration (Days 17–23, Mon 5 – Sun 11 Oct)

| Day | Task | Pri |
|---|---|---|
| 17 — Mon 5 | Full calibration procedure (§14.3) on panel #1; write `/CAL.TXT`; photograph everything | **[M]** |
| 18 — Tue 6 | Fabricate instrumented panel #2 (spare + demo unit) | **[M]** |
| 19 — Wed 7 | Flight-phase state machine + anomaly detector + fault handling; **remove every `while(!SD.begin())`** | **[M]** |
| 20 — Thu 8 | Ko reconstruction in the ground station; **live 3D panel visualisation working** | **[S]** |
| 21 — Fri 9 | Payload PCB assembly (or final protoboard build) with JST/terminal-block connections only | **[M]** |
| 22 — Sat 10 | ADS1220 redundant chain on 2 stations; EMI blanking implemented and measured (T6 / C30) | **[S]** |
| 23 — Sun 11 | Integrate the payload into the 3U frame; **weigh the whole assembly** — must be ≤ 3 kg | **[M]** |

### Phase 4 — Environmental qualification (Days 24–30, Mon 12 – Sun 18 Oct)

| Day | Task | Pri |
|---|---|---|
| 24 — Mon 12 | **Dry-ice cold test T8** to −78 °C. Record the temperature at which each subsystem quits | **[M]** |
| 25 — Tue 13 | Fix whatever T8 broke. (Something will break. This day exists for that) | **[M]** |
| 26 — Wed 14 | Thermal cycling T9 ×5; re-verify calibration afterwards | **[M]** |
| 27 — Thu 15 | Reduced-pressure test T10; combined cold+vacuum T11 if a pump is available | **[S]** |
| 28 — Fri 16 | Modal identification T12 at room temperature and in the freezer — **this produces H3's ground truth** | **[S]** |
| 29 — Sat 17 | Vibration + drop test T13; radio range test T14 (2–5 km line of sight) | **[M]** |
| 30 — Sun 18 | **Full 3-hour flight rehearsal T15** on batteries, with the ground station and a flight director | **[M]** |

### Phase 5 — Freeze, document, rehearse (Days 31–37, Mon 19 – Sun 25 Oct)

| Day | Task | Pri |
|---|---|---|
| **31 — Mon 19** | **HARDWARE AND FIRMWARE FREEZE.** No changes after today except bug fixes with a written justification | **[M]** |
| 32 — Tue 20 | Write the test report: every test, method, result, photo. This is rubric line 5 (8 points) | **[M]** |
| 33 — Wed 21 | Write the theory section: thermo-elastic model, Ko theory, modal prediction, convective scaling — **with the numbers** (rubric line 2, 8 points) | **[M]** |
| 34 — Thu 22 | Build the presentation. Lead with the live demo, not the slides | **[M]** |
| 35 — Fri 23 | Record a screen capture of the live demo and the flight playback as a backup | **[S]** |
| 36 — Sat 24 | Full dress rehearsal of the defence; the researcher and radio technician each present their section | **[M]** |
| 37 — Sun 25 | Pack: flight unit, spare panel, spare gauges, soldering iron, laptop, ground station, LoRa dongle, antenna, **printed calibration record**, spare SD cards | **[M]** |
| **38 — Mon 26** | **Tournament Final begins** | |

### Scope control — what gets cut first if you fall behind

| If you are behind by | Cut this | Keep this |
|---|---|---|
| 2 days | The 2D Ritz reconstruction (keep 1D Ko) | Everything else |
| 4 days | The ADS1220 redundant chain; the sun-vector channel | 4-station strain, calorimeter, telemetry |
| 1 week | The custom PCB (use protoboard); reduce to 3 strain stations | Calorimeter, temperature network, telemetry, SD logging |
| 2 weeks | **All of SKIN-24. Fly THERMOS + BLACKBOX alone.** | You will still return a complete dataset and a confirmed hypothesis |

That bottom row is the point of the whole architecture: **there is a floor below which you cannot fall, and it still scores.**

---

## 19. RECOMMENDED MINIMUM VIABLE SYSTEMS

Three nested systems. Build them in this order; each is complete and scoreable on its own.

### MVP-0 — "It cannot fail" *(2 days, $6)*
- 3 self-heated calorimeter elements + 4 DS18B20 + MOSFET
- Flight-phase state machine
- 1 Hz binary telemetry + SD logging with proper fault handling
- Battery R_int from LoRa TX steps (2 resistors)

**Delivers:** a monotonic convective-cooling curve vs altitude, a cold-battery R_int curve, a phase-tagged dataset. **Tests a numerical hypothesis. Has essentially no failure mode.** If everything else collapses, this alone earns a defensible score on all seven rubric lines.

### MVP-1 — "The real mission" *(+10 days, +$25)*
Add:
- 4 back-to-back gauge pairs on one instrumented panel + 5× HX711 (shared clock)
- 2 CTE coupons (PLA + aluminium reference)
- Piezo disc + platform OPAMP + 2 kHz FFT
- Ground station with **live 3D panel reconstruction**

**Delivers:** SKIN-24 in full — in-flight panel deformation, in-flight CTE, modal frequency vs temperature, plus the live demo. **This is the recommended target.**

### MVP-2 — "Everything" *(+5 days, +$20)*
Add:
- ADS1220 redundant chain on 2 stations
- Sun-vector photodiode array + thermal prediction model
- 2D Ritz surface reconstruction (6 gauge pairs)
- Acoustic-emission hardware event counter via the platform comparator
- ADC/V_ref drift mapping with a precision reference

**Delivers:** redundancy, a predictive thermal model to test against measurement, a true 2D surface, and honest error bars on everything.

### The one-sentence mission statement to put on your title slide

> **"SKIN-24: we instrumented a student-built CubeSat panel with $0.50 sensors to find out whether a 3U spacecraft deforms, stiffens and overheats its own electronics on the way to the stratosphere — and to see whether a satellite can diagnose its own structure in flight."**

---

## 20. WHAT WE SHOULD **NOT** ATTEMPT

Listed with the reason, so you can defend the decision if a judge asks why you didn't.

| Do not attempt | Why not |
|---|---|
| **A serpentine copper trace as a strain sensor** | Copper's TCR (+3900 ppm/K) swamps its gauge factor by ~1000×. It is an excellent thermometer and a useless strain gauge. (Explaining *this* in your presentation is worth more than attempting it) |
| **Flex sensors (Spectra Symbol etc.)** | Wrong range (they need >10 mm bends; you have 0.1–1 mm), 20–30× the cost of a foil gauge, and severe cold drift |
| **Fibre-Bragg-grating shape sensing** | The interrogator costs more than the entire competition budget. This is precisely the gap your cheap version fills |
| **A frame-torsion measurement with two IMUs** | The signal is microradians; MEMS gyro noise is orders of magnitude larger. Guaranteed null result |
| **High-voltage Paschen / corona experiments** | Needs hundreds of volts on a student payload. Real spacecraft concern (NASA-HDBK-4007 exists for it), but the safety rules are published only on day 1 of the finals and it is very likely prohibited. Not worth the disqualification risk |
| **A Geiger tube as the primary experiment** | Class A novelty (every school flies one), needs a ~400 V module, and a 2-hour flight may record zero electronics upsets — half your hypothesis returns null |
| **Ozone-layer-peak science** | The interesting part starts at 22–26 km. The Regulations say "up to 24 km" and 2025 actually reached ~20 km. You would be betting your mission on the optimistic end of a contradicted number |
| **Deployable mechanisms, hinges, or anything that moves** | Mechanism design is 3–4 weeks minimum, it is the highest-risk item in any CubeSat, and moving parts are exactly what the *assembly* criterion penalises when they are improvised |
| **A pressurised or sealed vessel** | Pressure differential at 30 hPa is ~1 bar across the wall. Failure is energetic. Likely prohibited; certainly needs organiser approval |
| **Live animals without a written organiser ruling** | Welfare, containment, and a life-support system you have not budgeted energy for. Get the ruling in writing before spending a day on it |
| **Measuring radiation in the ADC noise floor** | Predicted effect is below the thermal drift of a cheap front end. You could not distinguish a positive result from a drift artefact |
| **Machine learning onboard** | The H750 has 128 kB of flash. A running z-score does the same job in 6 lines and you can explain every parameter of it to a judge |
| **Chasing 19200 baud telemetry** | Costs link margin at 25–80 km slant range for data you already have on the SD card. 2400 baud at 1 Hz is the right engineering answer |
| **Redesigning the platform's service systems** | The kit works; ORBITA 2025 confirmed every apparatus functioned. Your points are in the payload, not in improving something that already flies |
| **Anything requiring a part that has not yet arrived by 12 October** | Day 24 is the environmental-test gate. A component that misses it cannot be qualified, and an unqualified component is a failure waiting to happen in front of the jury |

---

## 21. SOURCES AND EVIDENCE

### 21.1 Primary competition sources

- [ORBITA International Space Tournament — official page](https://spaceeducation.info/en/orbita/) — competition structure, categories, dates, participating countries, contacts
- [Regulations of the International Tournament Orbita (PDF, 2026 edition)](https://spaceeducation.info/wp-content/uploads/2026/08/Regulations-of-the-International-Tournament-Orbita-3.pdf) — §5–9 and Attachment 1: eligibility, team composition and roles, timeline, 3U kit provision, "up to 24 km", 3 kg context, deliverables, evaluation process, disqualification triggers, required competencies
- [ORBITA Tournament — Evaluation criteria for final projects (Google Doc, 2025 edition)](https://docs.google.com/document/d/1wmB0PILsCorPZbKljBHPtitmNSPyQgAdm-DzfdO0pzY/edit?usp=sharing) — **the 60-point rubric quoted throughout §1.4** (reached via QR code in the organiser briefing deck)
- [Participant Guide for the Orbita Tournament (Google Doc, EN)](https://docs.google.com/document/d/17579zglLN3YmaNhj5Uv_i94HNEA4ESlLzLCHaR4y4fM/edit?usp=sharing) — **the 3 kg mass limit**, block structure, report contents
- [Руководство для участников турнира «Орбита» (Google Doc, RU)](https://docs.google.com/document/d/14IvCzVfpaJwvQN9Zcq0taIYLfdi_TcrjpacdP3cId70/edit?usp=sharing) — Russian original of the above; *"масса аппарата с полезной нагрузкой не должна превышать 3 килограмма"*
- [IntroSat — educational CubeSat kit vendor](https://introsat.ru/) — STM32 family, 3U format, annual stratospheric launches to 20–30 km, kit price from 99 900 ₽
- [Stratonautica — educational stratospheric launches](https://stratonautica.ru/obrazovatelnie-zapuski) — launch operator listed as a tournament partner; pre-flight EMC, thermal-vacuum and vibro-dynamic testing expectations
- [Турнир Орбита, сезон 2025 года — cikeno.ru](https://cikeno.ru/tournament-orbita-2025/) — 2025 outcome: ~20 km reached, *"all apparatus functioned during flight, though not every experiment achieved its planned results"*, Uzbekistan team won the satellite competition
- [Международный турнир «Орбита» — Дворец Молодёжи](https://dm-centre.ru/novosti/kvantorium/mezhdunarodnyj-turnir-orbita/) — corroborating event description

### 21.2 Local files (primary evidence, on your machine)

- `orbita26/resources/ORBITA25.pdf` — organiser briefing deck; flight profile (ascent 60–90 min @ 5 m/s, 10–15 min at peak, 30–40 min descent), *"Temperature: −70 °C to +20 °C"*, *"Radiation: 100–200× stronger than at sea level"*, role recommendations, experiment-selection guidance, and the two QR codes that led to the rubric and participant guide
- `orbita26/resources/IntroSat.Platform_ краткое руководство.pdf` — **the definitive hardware reference**: STM32H750VBT6, IntroBus_L pinout, OPAMP/COMP/DAC/DFSDM/HRTIM availability, power-channel limits, LoRa AT commands and 58-byte packet behaviour, 2U payload volume, student-fabricated side panels, flight-legal connector rules
- `orbita26/resources/IntroSat.Platform_ short guide 2.pdf` — English abridgement; sensor addresses, IntroStratLib usage, SD-card setup
- `orbita26/resources/International Space Tournament Orbita.docx` — team roster and brainstorm tables
- `orbita26/resources/Mission 1.docx`, `Mission 2.docx`, `Mission 3.docx` — the three biology mission drafts analysed in §3.5

### 21.3 Technical and scientific literature

**Structural health monitoring and shape sensing**
- [NASA/TM-2020-220465 — Fiber-Optic Strain-Based Deflection and Twist Measurement](https://ntrs.nasa.gov/api/citations/20200001121/downloads/20200001121.pdf)
- [Applications of Ko Displacement Theory to the Deformed Shape Predictions of the Doubly-Tapered Ikhana Wing (NASA)](https://ntrs.nasa.gov/api/citations/20090040594/downloads/20090040594.pdf)
- [Ko Displacement Theory for Structural Shape Predictions — NASA Tech Briefs](https://www.techbriefs.com/component/content/article/8905-drc-006-024)
- [Shape Sensing for a UAV Composite Half-Wing: Modal Method vs Ko's Displacement Theory (Aerospace, 2022)](https://doi.org/10.3390/aerospace9090509)
- [Soft electronic skin for self-deployable tape-spring hinges (Nature Communications Engineering, 2024)](https://www.nature.com/articles/s44172-024-00163-x)
- [An Impact Strain Monitoring and Simulating Method for Large-Size Composite Skin Panel with Optical Fiber Sensors (Aerospace, 2025)](https://doi.org/10.3390/aerospace12121070)
- [STRAINMON — Strain Monitoring in Composite Stiffened Panels Using Sensors (EU CORDIS)](https://cordis.europa.eu/project/id/296514/reporting/es)

**Thermally induced spacecraft dynamics ("thermal snap")**
- [Measurement of Satellite Solar Array Panel Vibrations Caused by Thermal Snap and Gas Jet Thruster Firing (IntechOpen)](https://www.intechopen.com/chapters/18984)
- [Vibration of Satellite Solar Array Paddle Caused by Thermal Shock When a Satellite Goes Through the Eclipse (IntechOpen)](https://www.intechopen.com/chapters/40863)
- [Solar-array-induced disturbance of the Hubble Space Telescope pointing system (AIAA JSR)](https://arc.aiaa.org/doi/10.2514/3.26664)
- [Thermally Induced Dynamics of Satellite Solar Panels (AIAA JSR)](https://arc.aiaa.org/doi/pdf/10.2514/2.3633)

**Balloon and CubeSat flight dynamics / environment**
- [Dynamic Characterization of a High-Altitude Balloon during a Flight Campaign (Aerospace, 2021)](https://www.mdpi.com/2226-4310/8/1/21) — **source of the pendulum period 5.82 s / 0.178 Hz, 1.8 mg horizontal, 0.08 g vertical near burst, 40 °/s initial yaw, 2.5 g landing shock, 15 Hz sampling, FFT analysis**
- [Micro-Vibrations Analysis in LEO CubeSats Using MEMS Accelerometers (Sensors, 2025)](https://www.mdpi.com/1424-8220/25/18/5917) — ADXL367 at 100 SPS on WREN-1; authors note the public-dataset gap
- [A Platform for Active Stabilization of High-Altitude Balloon Payloads (BAMS, 2023)](https://journals.ametsoc.org/view/journals/bams/104/6/BAMS-D-22-0042.1.xml)
- [Testing a Prototype 1U CubeSat on a Stratospheric Balloon Flight (arXiv:2102.04847)](https://arxiv.org/pdf/2102.04847)
- [High altitude balloon testing of Arduino and environmental sensors for CubeSat prototype (HardwareX, 2022)](https://www.sciencedirect.com/science/article/pii/S2468067222000748)
- [Uncertainties in atmospheric velocity measurement due to balloon-gondola pendulum-like motions (Adv. Space Res.)](https://sciencedirect.com/science/article/abs/pii/S027311771000637X)

**Radiation environment**
- [The disappearance of the Pfotzer–Regener maximum in dose equivalent measurements in the stratosphere (Space Weather, 2016)](https://agupubs.onlinelibrary.wiley.com/doi/full/10.1002/2016SW001402)
- [Data Analysis and Curve Fitting to Determine the Regener–Pfotzer Maximum (Academic High Altitude Conference, 2017)](https://via.library.depaul.edu/ahac/2017/poster3/2/)

**High voltage / Paschen (for the rejected concept)**
- [NASA-HDBK-4007 — Spacecraft High-Voltage Paschen and Corona Design Handbook](https://standards.nasa.gov/sites/default/files/standards/NASA/Baseline-w/CHANGE-3/3/2020_12_29_nasa-hdbk-4007_w-change_3_revalidated.pdf)

**Components and instrumentation**
- [Adafruit HX711 24-bit ADC for Load Cells / Strain Gauges — documentation](https://learn.adafruit.com/adafruit-hx711-24-bit-adc/overview)
- [Strain gauges — Wolles Elektronikkiste (bridge topologies, temperature compensation)](https://wolles-elektronikkiste.de/en/strain-gauges)
- [MS5611 high-resolution barometric sensor, 10–1200 mbar (AMSYS)](https://www.amsys-sensor.com/products/pressure-sensor/ms5611-high-resolution-barometric-sensor-10-1200-mbar/)
- [MS5611 — precise altitude measurement application note (AMSYS)](https://www.amsys-sensor.com/downloads/notes/ms5611-precise-altitude-measurement-with-a-pressure-sensor-module-amsys-509e.pdf)
- [Kingston Industrial microSD datasheet (−40…+85 °C)](https://www.kingston.com/datasheets/SDCIT_us.pdf)
- [Industrial SD cards — key factors to consider (ATP)](https://www.atpinc.com/blog/industrial-sd-cards-factors-requirements-to-consider)
- [Ebyte E32-433T30D LoRa module datasheet](https://www.cdebyte.com/pdf-down.aspx?id=1462)
- [STM32 AN2606 — system memory boot mode](https://www.st.com/resource/en/application_note/an2606-introduction-to-system-memory-boot-mode-on-stm32-mcus-stmicroelectronics.pdf)
- [LSM6DS3 application note AN5130](https://www.st.com/resource/en/application_note/an5130-lsm6ds3trc-alwayson-3d-accelerometer-and-3d-gyroscope-stmicroelectronics.pdf)
- [LIS3MDL datasheet](https://www.st.com/resource/en/datasheet/lis3mdl.pdf)
- [LM75A datasheet (NXP)](https://www.nxp.com/docs/en/data-sheet/LM75A.pdf)

**Local sourcing**
- [Microelectronica / electron.uz — Tashkent electronics](https://electron.uz/?product_cat=arduino)
- [Uzum Market — sensor kits, Tashkent](https://uzum.uz/ru/product/nabor-iz-37-1055758)
- [Магазины радиодеталей в Ташкенте](https://www.goldenpages.uz/en/rubrics/?Id=1563)

### 21.4 Calculations performed for this document

All numeric claims marked "computed" were generated for this report and are reproducible:
- **ISA 1976 atmosphere** 0–32 km (T, P, ρ, speed of sound, mean free path) — §4.1
- **Strain-gauge front-end budget** (µV/µε at gains 1–128; STM32 16-bit LSB vs strain resolution) — §9.2
- **Thermal output** for Al/FR4/PLA/PETG/ABS/PMMA/CFRP/steel over ΔT = −70 K — §6 C2
- **Panel bending strain** from curvature for several thickness/span/deflection combinations — §8.1
- **LoRa airtime** for 58-byte packets at 300–19200 baud — §12.1
- **Power budget** for the full payload plus platform — §10.3
- **Mass budget** for a 3U build against the 3 kg limit (≈2.0 kg with 1 kg margin) — §2.4
- **Convective-collapse model**: h ∝ ρ^0.5 with h_rad = 4εσT³, giving the 16 K → 40 K rise prediction — §8.2
- **Modal frequency shift** from E(T): +18% for PLA, +14% for PETG — §6 C4
- **Differential shrinkage** PLA vs aluminium: 0.325 mm over 100 mm, **0.974 mm over 300 mm** — §4.3
- **Bio-module heater budget**: 4.1 W / 12.2 Wh for 20 mm XPS foam at ΔT = 60 K — §3.5

---

## APPENDIX — THE SIX THINGS TO DO TODAY

1. **Order the strain gauges, ADS1220 and industrial microSD cards from AliExpress.** Everything else in this document depends on parts arriving. (§16.2)
2. **Email the organisers the seven questions in §1.6** — especially the live-animal ruling, the battery capacity, and the LoRa channel allocation.
3. **Put every mandatory webinar and assignment deadline in a shared calendar.** Missing one is an explicit disqualification trigger (Regulations §6.6.11).
4. **Fill the empty Engineer/CAD seat** on the team roster, and confirm the radio technician.
5. **Decide the integrated mission with your researcher:** one payload, bio-module + SKIN-24, sharing your electronics. (§3.5)
6. **Buy two strain gauges locally and bond them to a piece of plastic tonight.** The freezer test on day 5 is the go/no-go gate for the entire mission — get to it as fast as possible.

---

*Prepared 18 September 2026. Every factual claim in this document is either sourced in §21, computed in §21.4, or explicitly flagged as an uncertainty. Where the official rules are silent — notably the 2026 evaluation criteria and the safety rules, both published only on the first working day of the finals — this document says so rather than guessing.*
