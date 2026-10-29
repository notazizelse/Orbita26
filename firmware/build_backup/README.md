# Prebuilt firmware and backup tools

Ready-to-flash binaries for the **flight controller** (STM32H750), so nobody needs the Arduino toolchain just to flash. This also matters where arduino.cc or st.com can't be reached (see `docs/Software_Setup.md`).

Built 29 Oct 2026 from this repo with arduino-cli, STM32 board package **2.12.0** (Generic H750VBTx, default options), IntroStratLib 0.1.2, STM32SD 1.5.0, FatFs 4.0.4. **None of them has run on the flight controller yet.**

| File | What | When |
|---|---|---|
| `SensorTest_core2.12.bin` | `firmware/tools/SensorTest`: PASS/FAIL for every sensor on the FC USB-C, 115200 | First, after wiring |
| `HB_Tune_core2.12.bin` | `firmware/tools/HB_Tune`: PB0 + MQ-7 at 50 Hz for the Serial Plotter | Tuning RV3 on the snail |
| `SnailPulse_Flight_v1.2_core2.12.bin` | Flight firmware 1.2: rev C no heater, DHT-11 (PC6), MQ-7 (PB1), BMP280 0x76 + 0x77 | Flight |
| `SHA256SUMS.txt` | Checksums of the three files | Check a download |
| `revC_no_heater.patch` | The 24 Oct source change that switched the heater off. **Already applied** in fw 1.2; kept for history | — |
| `e32_config.py` | Read/set the ground E32 LoRa-UART module from a Mac (replaces Windows "RF Setting") | Ground station setup |

The older 24 Oct binaries (fw 1.0 heater-on and 1.1 no-heater) are gone on purpose. They don't know the new sensors, and the heater-on one drives PC6 as a PWM output, which now fights the DHT-11.

## Flashing

With the open-source stlink tools (no st.com download needed):

```bash
st-flash --reset write SensorTest_core2.12.bin 0x08000000
```

Or with STM32CubeProgrammer: connect ST-Link (SWD) → Connect → Erasing & Programming → file = the .bin, start address **0x08000000**, tick Verify + Run after programming → Start Programming.

ST-Link → FC **PROG** header: SWCLK → CLK, SWDIO → SWD, GND → GND. BOOT0 jumper on 0.

## Rebuilding

```bash
FQBN=STMicroelectronics:stm32:GenH7:pnum=GENERIC_H750VBTX
arduino-cli compile -b $FQBN --output-dir build/flight firmware/SnailPulse_Flight
cp build/flight/SnailPulse_Flight.ino.bin firmware/build_backup/SnailPulse_Flight_v1.2_core2.12.bin
```

Then update `SHA256SUMS.txt` (`shasum -a 256 *.bin > SHA256SUMS.txt`) and this table.
