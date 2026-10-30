# Handoff: SnailPulse sensors (29–30 Oct 2026)

Where the work stands, what is verified, and what to do next. Project rules and build commands are in `CLAUDE.md`.

## State in one paragraph

Rev C SnailPulse (heartbeat only, no heater) gained three sensors on 29 Oct 2026: a **DHT-11** (DATA → PC6), an **MQ-7** (AO → PB1), both on +3.3 V at first and on **+5 V since 30 Oct (fw 1.4)**, and a **second BMP280** on the same I²C4 bus. BMP280 #1 keeps SDO → GND (0x76) and BMP280 #2 has SDO → VCC (0x77). The flight firmware (fw 1.2), a new sensor-test sketch, the ground station and both build guides support all of them. **30 Oct (fw 1.3): two snails, one BMP280.** Snail 1 = TCRT5000 → amplifier → PB0, snail 2 = MAX30100 (I²C4 0x57), each with its own heart rate in the SNAIL packet (now 49 bytes). The second BMP280 was dropped: its ENV fields stay but are always empty, so a healthy board was **ok=0x77**. **30 Oct (fw 1.5): GY-21 removed.** It held I²C4's SCL low (likely damaged); the bus works without it. The firmware no longer touches 0x40: box temperature comes from the BMP280, humidity from the DHT-11, ENV's SHT21 fields are sent as invalid and a healthy board is now **ok=0x76**. All of it is on `main` (pull requests #1 and #2, then the 30 Oct work pushed directly). SensorTest has run on the FC; the flight firmware has not. **Radio:** `radio_handoff/` is the complete package for the radio technician, now with test sketches for the FC (`LinkTest_FC`, `CommBridge_FC`) and a PC tool that plays either end of the FC ↔ comm board link (`tools/sp_link_tool.py`); none of them has run on hardware yet.

## What was done

| Area | Change |
|---|---|
| `firmware/tools/SensorTest` (new) | PASS/WARN/FAIL for every sensor + cross-checks, live view, plotter stream. Output on the FC USB-C, 115200 |
| `firmware/SnailPulse_Flight` → 1.2 | Rev C no-heater patch applied; DHT-11 driver; MQ-7 sampled in the ISR; both BMP280s; heater/new-sensor pin clash is a compile error |
| ENV packet | 24 → 36 bytes: + DHT-11 T/RH, MQ-7 mV, second-BMP280 P/T (empty since fw 1.3). `sensors_ok` bits 5/6 = DHT-11/MQ-7; healthy rev C board = **0x77** |
| GY-21 (SHT21) removed (fw 1.5, 30 Oct) | `SP_HAS_SHT21 0` in `config.h`, `HAVE_SHT21 0` in SensorTest (new DHT-11 vs BMP280 check). ENV `box_t`/`box_rh` = -32768 / 0xFFFF, bit 0 clear, healthy = 0x76. Ground script and monitor show box air from BMP280 + DHT-11 and mark the SHT21 "not fitted". Set both switches to 1 to use a new GY-21 |
| DHT-11 + MQ-7 on 5 V (fw 1.4, 30 Oct) | PC6 idles without the internal pull-up (the module pulls DATA to 5 V); MQ-7 AO through 10 kΩ / 20 kΩ to PB1, scaled back so ENV `mq7_mv` is the voltage at the module (0–5000 mV, `mq7_scale.h`, unit-tested); monitor warns on firmware < 1.4 |
| Two snails (fw 1.3, 30 Oct) | MAX30100 FIFO read every 40 ms (`max30100_fifo.h`), `OpticalPulse` → second `HeartbeatDetector`; SNAIL packet 39 → 49 bytes; ground decoder, monitor, `radio_handoff` and SensorTest label snail 1 / snail 2 |
| `firmware/tools/HB_Tune` | No longer holds PC6 low (would block the DHT-11); plots MQ-7 instead of the old heater NTC |
| `ground/snailpulse_ground.py` | Decodes 36/30/24-byte ENV, shows and plots the new values, starts a new CSV when columns change |
| Debugging | Flight firmware pauses its watchdog while a debugger halts the core; prebuilt `.bin` files removed (build from source to debug) |
| Perfboard build guide | Step 10 (DHT-11 row 17, MQ-7 row 19, FC pads PC6/PB1; the second BMP280 was removed 30 Oct), step 11 runs SensorTest; IntroBus_L pin map; code-to-board table; pre-test voltage table |
| Walkthrough + READMEs + integration doc | Updated to match |
| Ground monitor (30 Oct) | `ground/snailpulse_monitor.py`: browser screen with every sensor's value and OK/WARN/FAIL, cross-checks, charts, FC text console. Reads the FC's USB-C or the ground LoRa module; `--demo`, `--file` |
| Flight fw (30 Oct) | Every frame is also copied to the FC's USB-C (`SP_USB_MIRROR`), STATUS reports FC sensor health (`fc_ok`), hooks for merged radio code (`sp_hooks.h`, `SP_OWN_RADIO`) |
| `radio_handoff/` (30 Oct) | What to give the radio technician: link contract, 4 integration routes, `snailpulse_frames.h`, examples, test frames |
| Radio test tools (30 Oct, late) | `radio_handoff/fc_sketches/LinkTest_FC` (test frames to the comm board with the flight ACK rule, report of ACKs / ACK time / throughput on USB-C), `CommBridge_FC` (type to the comm board through the FC, ACK shows as `<06>`), `radio_handoff/tools/sp_link_tool.py` (`fake-comm` / `fake-fc` / `selftest` with a 3.3 V USB-UART adapter). README section 5 has the test table, one hop at a time. Test frames in the README and `test_host.cpp` now carry fw 1.5 values (ok 0x76, fw 0x0105) |
| `snailpulse_frames.h` copies | Arduino only compiles files inside the sketch folder, so `ground_receiver/` and both `fc_sketches/` folders hold copies. `test_host.cpp` checks they are identical to `radio_handoff/snailpulse_frames.h`, and that the header's structs, CRC, builder and parser match `protocol.h` |
| `firmware/tools/MAX30100_Pulse` (30 Oct) | Snail 2 bench check: bpm and beat count from the MAX30100 on the FC USB-C, same sensor settings as the flight firmware, its own simple beat finder (keys `t p r + -`) |
| `docs/Software_Setup.md` | Note: the IDE's "Failed to install library: Arduino_BuiltIn … EOF" message at start-up is harmless (its example libraries come from arduino.cc) |
| `scripts/mac_setup.sh` + `docs/Bench_Test.md` (30 Oct) | One-shot Mac setup that resets a stuck Arduino IDE; cables, power order, upload settings |
| `docs/Software_Setup.md` (new) | macOS/Windows: build, flash and debug in STM32CubeIDE from Russia without VPN (CubeIDE from the dl.sisoog.com mirror + signature check, CubeProgrammer CLI from inside CubeIDE, Arduino15 folder copied from outside Russia) |

## Verified

- PC unit tests pass, including new DHT-11 tests (bits, ISR jitter, checksum, negative temperatures, stuck line).
- The ground script decodes frames written by the C code, plus 30- and 24-byte ENV frames.
- All three sketches compile for Generic H750VBTx on STM32 core 2.12.0 (flight 63 %, SensorTest 43 %, HB_Tune 25 % of flash). SensorTest and HB_Tune also on core 3.0.0.
- The perfboard guide renders with no console errors; the new pads, wires and steps light up correctly.
- 30 Oct, late: PC tests pass with the new `snailpulse_frames.h` checks; the ground script decodes the fw 1.5 test frames; `sp_link_tool.py selftest` passes (fake FC against fake comm board in memory, with and without missing ACKs).
- 30 Oct, late, arduino-cli on Windows, core 2.12.0, `--warnings all`: all six FC sketches compile with no warnings from our files (IntroStratLib's `I2C_Err.h` / `LM75A.cpp` warn on their own). Flash: SnailPulse_Flight 84,328 B (64 %), SensorTest 44 %, MAX30100_Pulse 32 %, LinkTest_FC 27 %, HB_Tune 25 %, CommBridge_FC 22 %.

## Not verified (do these next, in order)

0. **Done 30 Oct:** `scripts/mac_setup.sh` ran on the team Mac (M5, offline Arduino bundle) and compiled the 30 Oct firmware: SensorTest 57,484 B (43 %), SnailPulse_Flight 83,752 B (63 %). The script output didn't list per-file warnings; check `--warnings all` once in the IDE. Still open: whether opening the FC's USB-C port resets the board (CH340X DTR). Zip the repo for a Mac with `git -c core.autocrlf=false archive` and unpack with `ditto -x -k` (macOS `unzip` breaks on the Russian file names).
1. **Bench run with SensorTest.** Build and flash `firmware/tools/SensorTest` (`docs/Software_Setup.md` section 5) and fix every FAIL. The voltage checks to do first are in the perfboard guide, steps 10 and 11.
2. **Confirm the FC's USB-C is UART2 (PA3/PA2).** That comes from earlier team notes, not the IntroSat guide. If nothing prints, set `MIRROR_UART1 1` in SensorTest and read UART1 through the Bluetooth module.
3. **Flight firmware on the FC.** STATUS should say fw 1.5, ENV `ok=0x76`, both snails' heart rates on the monitor. Watch `loop_max_ms` (the DHT-11 read adds ~5 ms every 2 s) and the DHT-11 failure rate.
   - **Fixed 30 Oct:** MCP6002 pin 8 (supply) was wired to +5 V; it belongs on +3V3 (the amp output must stay inside PB0's 0–3.3 V). If PB0 ever sits at 4095 (clipped) or 0, check the amp with the voltage table in perfboard guide step 11 before trusting snail 1.
   - **Snail 1 dips:** SensorTest showed regular ~60-count dips on PB0 (every ~2.2 s). Unplugging the MQ-7, then the DHT-11, then the MAX30100 points to the **MAX30100**: its LED pulses (~100 Hz) and supply current steps beat with the 50 Hz PB0 sampling. Confirm by plugging it back in and watching the dips return (HB_Tune or SensorTest `p`). Fixes to try, one at a time: its own GND wire back to the FC, 10–100 µF across its VCC/GND at the module, its wires routed away from the amplifier. Moving its VCC from 3.3 V to 5 V is fine **only if** its SDA/SCL pull-ups go to 3.3 V, not to VIN (measure SDA to GND with the module on 5 V: must read ≤ 3.4 V). These dips can make snail 1 count false beats.
   - Snail 2 alone: `firmware/tools/MAX30100_Pulse` prints its bpm and beat count.
4. **Done 30 Oct:** everything merged into `main`.
5. **Update the KiCad schematic** (`hardware/kicad_revC`): remove the no-connect flags on J6.2/J6.3, add the DHT-11 and MQ-7 connectors (no second BMP280), then re-export the walkthrough pictures.
6. **Done 30 Oct: MQ-7 and DHT-11 on 5 V.** Check the MQ-7 divider before powering: with the module at full output, PB1 must stay below 3.3 V (10 kΩ AO → PB1, 20 kΩ PB1 → GND). The heater now draws ~150 mA from +5V_IS. As before, keep its warm can away from the snail and the temperature sensors.
7. **Freezer run** of the whole stack (~3 h), as planned before.
8. **Radio link, one hop at a time** (with the radio technician, `radio_handoff/README.md` section 5): `sp_link_tool.py selftest`; FC + `LinkTest_FC` against `fake-comm`; comm board against `fake-fc`; then `LinkTest_FC` through the real comm board and ground E32 to `snailpulse_monitor.py`. Write down the ACK time and lost-frame rate. Then the same with SnailPulse_Flight. The radio sketches compile but have **never run** on an FC.

## Open questions

- Does opening the FC's USB-C port reset the board (CH340X DTR)? Matters for long logging sessions.
- Does the stock IntroSat comm board accept setting commands (`AT+LCHANNEL`, `AT+LAIRRATE`, `AT+LTXPWR` in the guide) on the FC's UART1, and with what line ending? `CommBridge_FC` is the way to find out.
- Which channel and air rate the organisers assign at the finals (ground E32 must match: `firmware/build_backup/e32_config.py`).
- Does the MAX30100 need to move to 5 V? Only if it misbehaves on 3.3 V; see step 3 for the pull-up check first.

## Files not from this work

Committed as-is on request, 29 Oct:
- `compass_artifact_wf-*.md`: two research reports on mission choices.
- `Orbita26/`: an empty KiCad stub project from 13 Oct. The real project is in `hardware/kicad_revC/`. Its `.history/` (KiCad's own local-history git folder) and `.kicad_prl` stay ignored by `.gitignore`.
