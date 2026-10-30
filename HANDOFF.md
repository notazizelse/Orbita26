# Handoff: SnailPulse sensors (29–30 Oct 2026)

Where the work stands, what is verified, and what to do next. Project rules and build commands are in `CLAUDE.md`.

## State in one paragraph

Rev C SnailPulse (heartbeat only, no heater) gained three sensors on 29 Oct 2026: a **DHT-11** (DATA → PC6), an **MQ-7** (AO → PB1), both on +3.3 V, and a **second BMP280** on the same I²C4 bus. BMP280 #1 keeps SDO → GND (0x76) and BMP280 #2 has SDO → VCC (0x77). The flight firmware (fw 1.2), a new sensor-test sketch, the ground station and both build guides support all of them. **30 Oct (fw 1.3): two snails, one BMP280.** Snail 1 = TCRT5000 → amplifier → PB0, snail 2 = MAX30100 (I²C4 0x57), each with its own heart rate in the SNAIL packet (now 49 bytes). The second BMP280 was dropped: its ENV fields stay but are always empty, so a healthy board is **ok=0x77**. Merged into `main` from pull request **#1** (https://github.com/notazizelse/Orbita26/pull/1). SensorTest has run on the FC; the flight firmware has not.

## What was done

| Area | Change |
|---|---|
| `firmware/tools/SensorTest` (new) | PASS/WARN/FAIL for every sensor + cross-checks, live view, plotter stream. Output on the FC USB-C, 115200 |
| `firmware/SnailPulse_Flight` → 1.2 | Rev C no-heater patch applied; DHT-11 driver; MQ-7 sampled in the ISR; both BMP280s; heater/new-sensor pin clash is a compile error |
| ENV packet | 24 → 36 bytes: + DHT-11 T/RH, MQ-7 mV, second-BMP280 P/T (empty since fw 1.3). `sensors_ok` bits 5/6 = DHT-11/MQ-7; healthy rev C board = **0x77** |
| Two snails (fw 1.3, 30 Oct) | MAX30100 FIFO read every 40 ms (`max30100_fifo.h`), `OpticalPulse` → second `HeartbeatDetector`; SNAIL packet 39 → 49 bytes; ground decoder, monitor, `radio_handoff` and SensorTest label snail 1 / snail 2 |
| `firmware/tools/HB_Tune` | No longer holds PC6 low (would block the DHT-11); plots MQ-7 instead of the old heater NTC |
| `ground/snailpulse_ground.py` | Decodes 36/30/24-byte ENV, shows and plots the new values, starts a new CSV when columns change |
| Debugging | Flight firmware pauses its watchdog while a debugger halts the core; prebuilt `.bin` files removed (build from source to debug) |
| Perfboard build guide | Step 10 (DHT-11 row 17, MQ-7 row 19, FC pads PC6/PB1; the second BMP280 was removed 30 Oct), step 11 runs SensorTest; IntroBus_L pin map; code-to-board table; pre-test voltage table |
| Walkthrough + READMEs + integration doc | Updated to match |
| Ground monitor (30 Oct) | `ground/snailpulse_monitor.py`: browser screen with every sensor's value and OK/WARN/FAIL, cross-checks, charts, FC text console. Reads the FC's USB-C or the ground LoRa module; `--demo`, `--file` |
| Flight fw (30 Oct) | Every frame is also copied to the FC's USB-C (`SP_USB_MIRROR`), STATUS reports FC sensor health (`fc_ok`), hooks for merged radio code (`sp_hooks.h`, `SP_OWN_RADIO`) |
| `radio_handoff/` (30 Oct) | What to give the radio technician: link contract, 4 integration routes, `snailpulse_frames.h`, examples, test frames |
| `scripts/mac_setup.sh` + `docs/Bench_Test.md` (30 Oct) | One-shot Mac setup that resets a stuck Arduino IDE; cables, power order, upload settings |
| `docs/Software_Setup.md` (new) | macOS/Windows: build, flash and debug in STM32CubeIDE from Russia without VPN (CubeIDE from the dl.sisoog.com mirror + signature check, CubeProgrammer CLI from inside CubeIDE, Arduino15 folder copied from outside Russia) |

## Verified

- PC unit tests pass, including new DHT-11 tests (bits, ISR jitter, checksum, negative temperatures, stuck line).
- The ground script decodes frames written by the C code, plus 30- and 24-byte ENV frames.
- All three sketches compile for Generic H750VBTx on STM32 core 2.12.0 (flight 63 %, SensorTest 43 %, HB_Tune 25 % of flash). SensorTest and HB_Tune also on core 3.0.0.
- The perfboard guide renders with no console errors; the new pads, wires and steps light up correctly.

## Not verified (do these next, in order)

0. **Done 30 Oct:** `scripts/mac_setup.sh` ran on the team Mac (M5, offline Arduino bundle) and compiled the 30 Oct firmware: SensorTest 57,484 B (43 %), SnailPulse_Flight 83,752 B (63 %). The script output didn't list per-file warnings; check `--warnings all` once in the IDE. Still open: whether opening the FC's USB-C port resets the board (CH340X DTR). Zip the repo for a Mac with `git -c core.autocrlf=false archive` and unpack with `ditto -x -k` (macOS `unzip` breaks on the Russian file names).
1. **Bench run with SensorTest.** Build and flash `firmware/tools/SensorTest` (`docs/Software_Setup.md` section 5) and fix every FAIL. The voltage checks to do first are in the perfboard guide, steps 10 and 11.
2. **Confirm the FC's USB-C is UART2 (PA3/PA2).** That comes from earlier team notes, not the IntroSat guide. If nothing prints, set `MIRROR_UART1 1` in SensorTest and read UART1 through the Bluetooth module.
3. **Flight firmware on the FC.** STATUS should say fw 1.3, ENV `ok=0x77`, both snails' heart rates on the monitor. The 30 Oct SensorTest live log showed ~60-count dips on PB0 every ~2.2 s with matching MQ-7 jumps: likely the comm board's radio. Stream with `p` with the comm board off to confirm; those dips can make snail 1 count false beats. Watch `loop_max_ms` (the DHT-11 read adds ~5 ms every 2 s) and the DHT-11 failure rate.
4. **Done 30 Oct:** PR #1 merged into `main`.
5. **Update the KiCad schematic** (`hardware/kicad_revC`): remove the no-connect flags on J6.2/J6.3, add the DHT-11 and MQ-7 connectors (no second BMP280), then re-export the walkthrough pictures.
6. **Decide the MQ-7 supply.** On 3.3 V its heater runs at ~45 % power, so it shows changes, not ppm. On 5 V it needs a divider before PB1 (10 kΩ AO → PB1, 20 kΩ PB1 → GND). Either way, keep its warm can away from the snail and the temperature sensors.
7. **Freezer run** of the whole stack (~3 h), as planned before.

## Open questions

- Is the MQ-7 meant as a real CO measurement or a "something changed" indicator? That decides 3.3 V vs 5 V.

## Files not from this work

Committed as-is on request, 29 Oct:
- `compass_artifact_wf-*.md`: two research reports on mission choices.
- `Orbita26/`: an empty KiCad stub project from 13 Oct. The real project is in `hardware/kicad_revC/`. Its `.history/` (KiCad's own local-history git folder) and `.kicad_prl` stay ignored by `.gitignore`.
