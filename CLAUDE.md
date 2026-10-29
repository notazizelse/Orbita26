# CLAUDE.md: working on SnailPulse (Orbita 2026)

Read this first, then `HANDOFF.md` for where the work stands.

## What this is

A payload for the IntroSat.Platform 3U CubeSat (Orbita 2026 stratospheric flight). An IR sensor on a live snail's shell feeds an analog band-pass amplifier; the satellite's **flight controller** (FC, STM32H750VBT6) counts heartbeats and radios them down over LoRa, plus box environment sensors. The payload board is a **perfboard with no processor**: all code runs on the FC.

Hardware is **rev C**: heartbeat only, no heater. On 29 Oct 2026 a DHT-11, an MQ-7 and a second BMP280 were added.

## Map

| Path | What |
|---|---|
| `firmware/SnailPulse_Flight/` | Flight firmware (fw 1.2). `config.h` = every pin, address, period |
| `firmware/tools/SensorTest/` | Bench sketch: PASS/FAIL for every sensor over the FC USB-C (115200) |
| `firmware/tools/HB_Tune/` | Streams PB0 (+ MQ-7) to the Serial Plotter for tuning RV3 |
| `firmware/tests/test_host.cpp` | PC unit tests for the Arduino-free code |
| `firmware/build_backup/` | E32 config script, rev C heater patch (no prebuilt firmware: always build from source) |
| `ground/snailpulse_ground.py` | Ground-station decoder (serial → console + CSV + plots); `Parser`/`decode` are reused by the monitor |
| `ground/snailpulse_monitor.py` | Live ground screen in the browser: every sensor OK/WARN/FAIL + charts (`--demo`, `--file`) |
| `radio_handoff/` | For the radio technician: link contract, `snailpulse_frames.h` (C99 parser + structs), FC hook template, ground receiver |
| `scripts/mac_setup.sh` | Mac setup: fixes a stuck Arduino IDE, installs core/libs/ST-Link tools, `.venv` for the monitor |
| `docs/Bench_Test.md` | Cables, power order, upload settings, what the monitor's states mean |
| `hardware/perfboard/SnailPulse_perfboard_build.html` | **Main build guide** (interactive hole-by-hole layout, steps, multimeter checks) |
| `docs/SnailPulse_RevC_Walkthrough.html` | Schematic walkthrough (1 MB, embedded SVG; edit text only) |
| `docs/Software_Setup.md` | Build, flash and debug (STM32CubeIDE) on macOS/Windows, including from Russia without VPN |
| `hardware/kicad_revC/` | Rev C KiCad project (**not yet updated** for the 29 Oct sensors) |
| `docs/Firmware_and_Integration.md` | How the firmware works and plugs into IntroSat |

## Pins and addresses (rev C + 29 Oct)

| Signal | FC pin | Notes |
|---|---|---|
| Heartbeat amp (HB_ADC) | PB0 (ADC) | J6.1, sampled 50 Hz in the TIM6 ISR |
| MQ-7 AO | PB1 (ADC) | J6.2. Module on 3.3 V. **PB1 is not 5 V-tolerant as an analog input** |
| DHT-11 DATA | PC6 | J6.3. Was the heater PWM in rev B: never drive it as an output now |
| I²C4 SDA / SCL | PD13 / PD12 | payload bus |
| I²C1 SDA / SCL | PB7 / PB6 | FC service bus, read only for us |
| Radio UART1 | PA10 / PA9 | 115200 8E1 to the comm board, ACK 0x06 |
| USB-C (debug) | PA3 / PA2 | UART2, used by SensorTest / HB_Tune |

I²C4: SHT21 0x40 · MAX30100 0x57 · **BMP280 #1 0x76 (SDO→GND, J8)** · **BMP280 #2 0x77 (SDO→VCC)**. I²C1: LSM6DS3 0x6A, LIS3MDL 0x1C, LM75A 0x4A, MS5611 0x77 (different bus, no clash with BMP280 #2).

## Build and test

**Arduino:** board package `STMicroelectronics:stm32@2.12.0`, FQBN `STMicroelectronics:stm32:GenH7:pnum=GENERIC_H750VBTX`, libraries IntroStratLib 0.1.2, STM32SD 1.5.0, FatFs 4.0.4. Full commands in `docs/Software_Setup.md`.

```bash
arduino-cli compile -b STMicroelectronics:stm32:GenH7:pnum=GENERIC_H750VBTX --warnings all firmware/SnailPulse_Flight
```

For debugging add `,opt=ogstd,dbg=enable_sym` to the FQBN and `--output-dir <dir>`; import the `.elf` in STM32CubeIDE (File → Import → STM32 Cortex-M Executable, MCU STM32H750VBTx). The flight firmware freezes its watchdog while the core is halted (`DBGMCU->APB4FZ1`), so breakpoints don't reset it.

**PC tests** (any C++17 compiler; on a machine without one, `pip install ziglang` then `python -m ziglang c++ ...`):

```bash
cd firmware/tests
g++ -std=c++17 -O1 -Wall -I../SnailPulse_Flight test_host.cpp ../SnailPulse_Flight/protocol.cpp \
    ../SnailPulse_Flight/heartbeat.cpp ../SnailPulse_Flight/thermal.cpp -o test_host && ./test_host
python ../../ground/snailpulse_ground.py --file frames.bin --out /tmp/gs     # decode the frames the test wrote
```

Before claiming firmware work is done: PC tests pass, the ground script decodes `frames.bin`, and all three sketches compile with no warnings from our files (IntroStratLib's `I2C_Err.h` warns on its own; ignore it).

## Rules that keep things working

- **Only the TIM6 ISR calls `analogRead()`** in the flight code (PB0 at 50 Hz, PB1 at 5 Hz). `loop()` never touches the ADC.
- **Never block `loop()`**: sensors are state machines; the 4 s watchdog resets the MCU otherwise. The DHT-11 bit read (~5 ms) starts right after an ISR tick (`last_tick_us`) so the 50 Hz interrupt can't stretch a bit.
- **Maths stays Arduino-free** (`heartbeat`, `thermal`, `protocol`, `*_comp.h`, `dht11_decode.h`) so `test_host.cpp` can test it. New maths gets a test there.
- **Packet changes touch these places together:** the struct in `protocol.h` (+ its `static_assert` size), `ground/snailpulse_ground.py` (format string + old-size fallbacks), the checks in `ground/snailpulse_monitor.py` (`evaluate`, `demo_frames`), `radio_handoff/snailpulse_frames.h` (structs + size checks) and its README test frames, the frame dump in `test_host.cpp`, and the packet tables in `firmware/README.md` and `docs/Firmware_and_Integration.md`. Only **append** fields so old logs still decode. Every frame must fit one 58-byte LoRa packet (payload ≤ 51 B).
- **Wiring changes touch the guides together:** the perfboard guide (layout JSON `L`, `NETS`, `WIRE`, `OFF`, `STEPS` + Requirements), the walkthrough text, `README.md`, and `config.h`. The perfboard guide stores ticked checks in `localStorage` keyed by step number; inserting a step needs the migration pattern already in the file.
- Every frame leaves through `sendFrame()` in the sketch: comm board queue (`SP_OWN_RADIO`), USB-C copy (`SP_USB_MIRROR`, UART2 PA3/PA2) and the `sp_frame_out()` hook (`sp_hooks.h`, weak defaults in `sp_hooks.cpp`). Merged code (radio technician) plugs in through those hooks, never by editing `loop()`.
- `SP_HEATER_ENABLED` must stay 0 while the DHT-11 / MQ-7 use PC6 / PB1 (`config.h` has an `#error` for it).

## Gotchas found the hard way

- STM32 board package **3.x renamed `HardwareSerial` to `Uart`**; the sketches use a `FcSerial` typedef for both.
- The IDE default C library (newlib-nano) **can't `printf` floats**: SensorTest formats them with `ff()`.
- The Arduino IDE auto-generates function prototypes near the top of an `.ino`: custom types used in signatures must live in a header (`sensortest_types.h`).
- On Windows, the arduino-cli toolchain breaks if its data folder path is long (260-char limit): keep it short, e.g. `%TEMP%\spa`.
- `arduino.cc` is blocked from Russia, and `st.com` refuses STM32CubeIDE/CubeProgrammer downloads there: see `docs/Software_Setup.md` (mirror + signature check, Arduino15 folder copied from outside). On macOS the CubeProgrammer CLI comes from inside STM32CubeIDE.
- `docs/SnailPulse_RevC_Walkthrough.html` is too big for the app's browser pane; check its HTML with a script.

## Style

Docs and comments are plain, short English for students and a jury: say what to do and why, give expected multimeter readings with ranges, no jargon without a one-line explanation. Commit messages describe what changed and why.
