# Handoff: SnailPulse sensors (29 Oct 2026)

Where the work stands, what is verified, and what to do next. Project rules and build commands are in `CLAUDE.md`.

## State in one paragraph

Rev C SnailPulse (heartbeat only, no heater) gained three sensors on 29 Oct 2026: a **DHT-11** (DATA → PC6), an **MQ-7** (AO → PB1), both on +3.3 V, and a **second BMP280** on the same I²C4 bus. BMP280 #1 keeps SDO → GND (0x76) and BMP280 #2 has SDO → VCC (0x77). The flight firmware (fw 1.2), a new sensor-test sketch, the ground station and both build guides support all of them. Everything is on branch `sensors-dht11-mq7`, in pull request **#1** (https://github.com/notazizelse/Orbita26/pull/1), not yet merged. **Nothing has run on the real hardware yet.**

## What was done

| Area | Change |
|---|---|
| `firmware/tools/SensorTest` (new) | PASS/WARN/FAIL for every sensor + cross-checks, live view, plotter stream. Output on the FC USB-C, 115200 |
| `firmware/SnailPulse_Flight` → 1.2 | Rev C no-heater patch applied; DHT-11 driver; MQ-7 sampled in the ISR; both BMP280s; heater/new-sensor pin clash is a compile error |
| ENV packet | 24 → 36 bytes: + DHT-11 T/RH, MQ-7 mV, BMP280 #2 P/T. `sensors_ok` bits 5/6/7 = DHT-11/MQ-7/BMP280 #2; healthy rev C board = **0xF7** |
| `firmware/tools/HB_Tune` | No longer holds PC6 low (would block the DHT-11); plots MQ-7 instead of the old heater NTC |
| `ground/snailpulse_ground.py` | Decodes 36/30/24-byte ENV, shows and plots the new values, starts a new CSV when columns change |
| `firmware/build_backup/` | Prebuilt `.bin` for SensorTest, HB_Tune and flight fw 1.2 + SHA256SUMS |
| Perfboard build guide | Step 10 (DHT-11 row 17, MQ-7 row 19, BMP280 #2 row 14, FC pads PC6/PB1), step 11 runs SensorTest; IntroBus_L pin map; code-to-board table; pre-test voltage table |
| Walkthrough + READMEs + integration doc | Updated to match |
| `docs/Software_Setup.md` (new) | macOS/Windows install and flash commands that avoid arduino.cc and st.com (blocked from Russia) |

## Verified

- PC unit tests pass, including new DHT-11 tests (bits, ISR jitter, checksum, negative temperatures, stuck line).
- The ground script decodes frames written by the C code, plus 30- and 24-byte ENV frames.
- All three sketches compile for Generic H750VBTx on STM32 core 2.12.0 (flight 63 %, SensorTest 43 %, HB_Tune 25 % of flash). SensorTest and HB_Tune also on core 3.0.0.
- The perfboard guide renders with no console errors; the new pads, wires and steps light up correctly.

## Not verified (do these next, in order)

1. **Bench run with SensorTest.** Flash `firmware/build_backup/SensorTest_core2.12.bin` (or build it) and fix every FAIL. The voltage checks to do first are in the perfboard guide, steps 10 and 11.
2. **Confirm the FC's USB-C is UART2 (PA3/PA2).** That comes from earlier team notes, not the IntroSat guide. If nothing prints, set `MIRROR_UART1 1` in SensorTest and read UART1 through the Bluetooth module.
3. **Flight firmware on the FC.** STATUS should say fw 1.2, ENV `ok=0xF7`. Watch `loop_max_ms` (the DHT-11 read adds ~5 ms every 2 s) and the DHT-11 failure rate.
4. **Merge PR #1** once the bench run passes.
5. **Update the KiCad schematic** (`hardware/kicad_revC`): remove the no-connect flags on J6.2/J6.3, add the DHT-11, MQ-7 and BMP280 #2 connectors, then re-export the walkthrough pictures.
6. **Decide the MQ-7 supply.** On 3.3 V its heater runs at ~45 % power, so it shows changes, not ppm. On 5 V it needs a divider before PB1 (10 kΩ AO → PB1, 20 kΩ PB1 → GND). Either way, keep its warm can away from the snail and the temperature sensors.
7. **Freezer run** of the whole stack (~3 h), as planned before.

## Open questions

- Which BMP280 sits inside the sealed snail box and which outside? The firmware calls them #1 (0x76) and #2 (0x77) only; the ground plots label them the same way.
- Is the MQ-7 meant as a real CO measurement or a "something changed" indicator? That decides 3.3 V vs 5 V.

## Files not from this work

Committed as-is on request, 29 Oct:
- `compass_artifact_wf-*.md`: two research reports on mission choices.
- `Orbita26/`: an empty KiCad stub project from 13 Oct. The real project is in `hardware/kicad_revC/`. Its `.history/` (KiCad's own local-history git folder) and `.kicad_prl` stay ignored by `.gitignore`.
