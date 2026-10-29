# SnailPulse flight firmware

Runs on the **IntroSat.Platform flight controller module** (STM32H750VBT6) used at Orbita.
It keeps the platform's normal job and adds the SnailPulse packets on top:

| Source | What it does |
|---|---|
| **Organizers' code** — [IntroStratLib](https://github.com/Obu-IntroSat/IntroStratLib-Public) v0.1.2 (copy in `libraries/IntroStratLib`, not committed) | LSM6DS3 accel/gyro, LIS3MDL magnetometer, LM75A board temperature on the I2C1 service bus |
| **Platform rules** from the IntroSat guide | UART1 (PA10/PA9) → communication board → LoRa E32-433T30D, 115200 8E1, wait for ACK `0x06` after each block; microSD on SDMMC2 |
| **SnailPulse (ours)** | Heartbeat sampling and beat detection, box sensors (SHT21, BMP280, MAX30100) on I2C4, heater control, binary packets, SD log, watchdog |

## Folder

```
firmware/
├── SnailPulse_Flight/        ← open SnailPulse_Flight.ino in the Arduino IDE
│   ├── config.h              pins, periods, thresholds — edit here
│   ├── protocol.h/.cpp       packet format (shared with the ground station)
│   ├── heartbeat.h/.cpp      beat detector
│   ├── thermal.h/.cpp        thermistor maths + heater controller
│   ├── payload_sensors.*     SHT21, BMP280 (0x77), MAX30100 drivers (I2C4) + DHT-11 (PC6)
│   ├── dht11_decode.h        DHT-11 bit/checksum decoding (unit-tested)
│   ├── fc_baro.h, ms5611_comp.h   non-blocking MS5611 with correct cold compensation
│   ├── bmp280_comp.h         Bosch compensation formulas
│   └── radio.h               ACK-gated frame queue
├── tools/SensorTest/         bench sketch: PASS/FAIL check of every sensor (payload, DHT-11 on PC6,
│                             MQ-7 on PB1, BMP280 at 0x77, FC's own sensors), output on the FC USB-C
├── tools/HB_Tune/            bench sketch: stream HB_ADC (+ MQ-7) to the Serial Plotter to set RV3
├── tests/test_host.cpp       PC unit tests (no board needed)
└── libraries/IntroStratLib/  organizers' library (git-ignored copy)
```

## Build and flash

1. Arduino IDE → Board Manager → **STM32 MCU based boards**.
2. Library Manager → **IntroStratLib**, **STM32SD** and **FatFs**. Or copy `libraries/IntroStratLib` into your Arduino `libraries` folder.
3. Board: **Generic STM32H7 Series**. Board part number: **STM32H750VBTx**. Upload method: **STM32CubeProgrammer (SWD)**.
4. Connect the ST-Link to the **PROG** connector (CLK, SWD, GND). Set the BOOT0 jumper to **0**, then Upload.
5. On the ground LoRa-UART module, set the same channel, air rate, FEC and power as the comm board. Then run:
   `python ground/snailpulse_ground.py --port <port> --baud <UartRate of the ground module>`

## What it sends

Every frame is `AA 55 | type | seq | len | payload | CRC16`. Each frame is at most 58 bytes, so it fits in **one** LoRa packet.

| Type | Every | Bytes (frame) | Contents |
|---|---|---|---|
| `0x10` SNAIL | 2 s | 46 | bpm×10, last interval, beat count, quality 0–100, flags, amplitude, DC level, **20 waveform samples (10 Hz)** |
| `0x01` BASE | 2 s | 41 | MS5611 pressure/temp/altitude (ISA), accel, gyro, mag, LM75A |
| `0x11` ENV | 4 s | 37 | SHT21 box T/RH, BMP280 box pressure, MAX30100 IR, sensor-OK bits, **DHT-11 T/RH, MQ-7 mV** (fw 1.2; heater fields stay empty on rev C) |
| `0x7F` STATUS | 10 s | 31 | firmware version, boot count, frames sent / no-ACK / dropped, I2C and SD errors, worst loop time, watchdog reset |

This averages about **56 bytes/s ≈ 0.45 kbit/s** against the 2.4 kbit/s air rate. That leaves room for retries and for other teams on nearby channels.
The SD card stores everything at full rate. `HBnnn.CSV` holds the raw 50 Hz heartbeat signal and `TLMnnn.CSV` holds every packet; `nnn` is the boot count.

## Design decisions (useful for the defence)

- **Sampling in a timer interrupt (TIM6, 50 Hz).** SD writes and I2C reads can block `loop()` for tens of milliseconds. The ISR reads the ADC into a ring buffer, so heartbeat timing stays exact. Only the ISR touches the ADC.
- **Beat detector.** Smoothing (τ 80 ms) and a moving baseline (τ 6 s) feed an envelope follower. A beat is an upward crossing at 40 % of the envelope. The refractory period is 0.4 s minimum, or 40 % of the usual interval. The reported rate is the **median of the last 5 intervals**, so one missed or extra beat does not spoil it. Accepted range is 3–150 bpm. Tested on synthetic snail pulses from 4 to 120 bpm, a 10→25 bpm step, and noise only (gives 0 bpm, quality 0).
- **Waveform is sent too.** On the ground we can recount beats ourselves, so we don't have to trust the onboard algorithm.
- **Heater control.** A PI loop holds the **box air at 20 °C** using the SHT21. The heater plate (NTC1) is limited to **35 °C** in software, with a taper over the last 3 °C. If a sensor fails, the controller falls back: SHT21 dead → plate held at 25 °C; both dead → fixed 20 % duty (~0.9 W). The LM393 hardware cutoff (~40 °C) works **even if the MCU hangs**.
- **Watchdog (4 s).** A stuck I2C bus or SD card resets the MCU instead of killing the mission. The STATUS packet reports whether the last reset came from the watchdog.
- **Own MS5611 driver.** IntroStratLib 0.1.2 has two problems with this sensor. `GetPressure()` blocks for ~80 ms. Its second-order compensation mixes units and gets the sign of T2 wrong, so **below 20 °C** it reports e.g. −163 °C at a real −1.6 °C and pressure about 1 % low (see `tests/`). We use the datasheet formulas, and ISA altitude that is valid above 11 km (at 30 mbar the simple formula says 21.7 km; ISA says 23.9 km).

## Tests on a PC

```
cd firmware/tests
g++ -std=c++17 -O1 -Wall -I../SnailPulse_Flight test_host.cpp ../SnailPulse_Flight/protocol.cpp \
    ../SnailPulse_Flight/heartbeat.cpp ../SnailPulse_Flight/thermal.cpp -o test_host && ./test_host
python ../../ground/snailpulse_ground.py --file frames.bin --out /tmp/gs   # decode frames made by the C code
```

These tests check: the CRC, the frame parser (with garbage and corrupted frames), the beat detector, NTC maths (40 °C → 1.143 V), heater fallbacks, a 3-hour thermal simulation at −40 °C, BMP280 and MS5611 against the datasheet examples, ISA altitude, and the ACK/timeout/overflow logic of the radio queue.

> Not yet verified on real hardware: firmware 1.2 (29 Oct) **compiles** for the Generic H750VBTx with STM32 board package 2.12.0, IntroStratLib, STM32SD 1.5.0 and FatFs 4.0.4 (83 KB of 128 KB flash, no warnings in our code), and the PC tests pass, but it has **not run on the board yet**. First bench run: flash `tools/SensorTest` and get 0 FAIL, then flash this sketch, check that STATUS says fw 1.2 on the ground station and that ENV shows ok=0x77.
