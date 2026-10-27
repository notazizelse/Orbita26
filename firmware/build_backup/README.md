# Backup builds (made 24 Oct 2026)

Compiled from the files in `firmware/` with STM32 board package **2.12.0** (target: Generic H750VBTx,
-Os, Newlib Nano, generic Serial, no USB), IntroStratLib from `firmware/libraries`, STM32SD 1.5.0, FatFs 4.0.4.
Compiler: arm-none-eabi-gcc 13.2 (the Arduino IDE uses 14.2; same code, slightly different binary).
They have NOT been run on the flight controller. Prefer building in the Arduino IDE; use these only
if the IDE install fails.

| File | What | Flash use |
|---|---|---|
| SnailPulse_Flight_revC_no_heater_core2.12.bin | flight code with revC_no_heater.patch applied: fw 1.1, PB1/PC6 untouched | 85 312 B / 128 KB |
| SnailPulse_Flight_heater_ON_core2.12.bin | flight code exactly as in the repo (heater control active) | 89 188 B / 128 KB |
| HB_Tune_core2.12.bin | bench tool, streams PB0 over the FCM USB-C (115200) | 41 020 B |
| revC_no_heater.patch | the source change for rev C: SP_HEATER_ENABLED 0, version 1.1, ground plot skips the missing heater sensor. Apply from the Orbita26 folder: `patch -p1 < firmware/build_backup/revC_no_heater.patch` | |
| e32_config.py | read/set the ground E32 LoRa-UART module from a Mac (replaces Windows "RF Setting") | |

Flash a .bin with STM32CubeProgrammer: connect ST-Link (SWD) → Connect → Erasing & Programming →
file = the .bin, start address **0x08000000**, tick Verify + Run after programming → Start Programming.
