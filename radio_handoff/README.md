# SnailPulse → radio: handoff for the radio technician

SnailPulse is our payload on the IntroSat flight controller (FC). It produces small binary **frames**: heart rate, box sensors, FC sensors and health. Your job is to get those frames to the ground **byte for byte**. This folder has everything needed for that.

| File | What it is |
|---|---|
| `README.md` | This page: the link, the frame format, the four ways to integrate, test frames |
| `snailpulse_frames.h` | One C/C++ header: frame layout, CRC, a byte-by-byte parser, `struct`s for every packet. No Arduino, no malloc. Tested in C99 and C++ against frames built by the flight code |
| `example_fc_hooks.cpp` | Template for route C (your code runs inside our FC firmware) |
| `ground_receiver/ground_receiver.ino` | Example for route D (E32 on a microcontroller at the ground, forwards to USB) |
| `fc_sketches/LinkTest_FC/` | **Test sketch for the FC**: sends test frames to the comm board with the flight rule, reports ACKs, ACK time and throughput on the FC USB-C. No payload board needed (section 5) |
| `fc_sketches/CommBridge_FC/` | **Test sketch for the FC**: laptop ↔ FC ↔ comm board serial bridge, to type commands to the comm board and see its replies (ACK shows as `<06>`) |
| `tools/sp_link_tool.py` | **PC tool** with a USB-UART adapter: `fake-comm` plays the comm board (test the FC alone), `fake-fc` plays the FC (test the comm board alone), `selftest` needs nothing |
| `../firmware/build_backup/e32_config.py` | Reads / sets the ground E32's channel, air rate and power over a USB-UART adapter |
| `../ground/snailpulse_monitor.py` | Our ground screen: shows every sensor with OK / WARN / FAIL. Reads any serial port that carries the frames |
| `../ground/snailpulse_ground.py` | Python decoder + CSV logger (the reference for field scaling) |
| `../firmware/SnailPulse_Flight/` | The flight firmware. `protocol.h` = frame format, `config.h` = pins and settings, `sp_hooks.h` = the hooks for route C |

The Arduino sketches carry a copy of `snailpulse_frames.h` in their own folder (the IDE only compiles files inside the sketch folder). If you change the header, copy it into `ground_receiver/` and both `fc_sketches/` folders: the PC tests (`firmware/tests`) fail while the copies differ.

To hand it over as one zip (run in the `Orbita26` folder):

```bash
git archive -o snailpulse_radio_handoff.zip HEAD radio_handoff ground firmware/SnailPulse_Flight \n    firmware/build_backup/e32_config.py docs/Software_Setup.md docs/Bench_Test.md
```

---

## 1. The link, hop by hop

```
SnailPulse code on the FC ──UART1 (PA9 TX / PA10 RX), 115200 8E1──► IntroSat comm board (STM32 + E32-433T30D)
          ◄────────────── 0x06 (ACK) after each frame has gone out ──┘        │
                                                                     LoRa 433 MHz band, air rate 2400 (default)
                                                                              ▼
laptop ◄──USB── USB-UART adapter (or microcontroller) ◄──UART, its own rate (often 9600 8N1)── ground E32
```

- The FC sends **one frame**, then waits for **0x06** from the comm board, then sends the next. No 0x06 within **1.5 s** → it sends the next anyway and counts a missed ACK. Up to 8 frames wait in a queue; when it's full, the oldest is dropped.
- **One frame = one LoRa packet.** Frames are 31–56 bytes; the E32 sends up to 58 bytes per packet. Never split or merge frames.
- **Don't change the bytes.** No extra headers inside a frame, no text conversion. The ground finds frames by the `AA 55` start bytes and the CRC, so noise between frames is harmless.
- Bench test without any radio: the FC also copies every frame to its **USB-C at 115200** (`SP_USB_MIRROR 1` in `config.h`). `snailpulse_monitor.py` on that port shows exactly what the radio would carry.

## 2. Frame format

```
AA 55 | type | seq | len | payload (len bytes) | CRC-16 low | CRC-16 high
```

- `seq` counts 0–255 per frame. Gaps on the ground = lost frames.
- CRC-16/CCITT-FALSE: polynomial 0x1021, start 0xFFFF, no reflection, no final XOR, over `type, seq, len, payload`. Check value: CRC of the ASCII string `123456789` is `0x29B1`.
- Little-endian numbers, packed structs (no padding).

| Type | Name | Every | Frame bytes | Contents |
|---|---|---|---|---|
| `0x10` | SNAIL | 2 s | 56 | snail 1 (TCRT5000): heart rate ×10, beat interval, beat count, quality, flags, pulse amplitude, DC level, 20 waveform samples (10 Hz); snail 2 (MAX30100): heart rate ×10, beat count, quality, flags, amplitude, IR level. Firmware before 1.3 sent 46 |
| `0x01` | BASE | 2 s | 41 | FC barometer (MS5611) pressure/temp/altitude, accelerometer, gyro, magnetometer, board temperature |
| `0x11` | ENV | 4 s | 43 | SHT21 T/RH (invalid since fw 1.5), BMP280 P/T (second-BMP280 fields always empty), DHT-11 T/RH, MQ-7 mV, MAX30100, sensor-OK bits |
| `0x7F` | STATUS | 10 s + at boot | 31 | firmware version, boot count, frames sent / no ACK / dropped, I²C and SD errors, worst loop time, reset cause, FC sensor-OK bits |

Field by field: the `struct`s in `snailpulse_frames.h` (the comments give units and "invalid" markers).

**Air time:** about 55 bytes/s ≈ 0.45 kbit/s, under 20 % of the 2400 bit/s air rate. If you add your own data on the same radio, keep the total under about 150 bytes/s and still one packet per frame.

## 3. Choose your route

### A. Stock IntroSat comm board firmware (most likely: nothing to code)

Our firmware already talks to the comm board exactly as the IntroSat guide says. You only:
1. Set the satellite comm board and the ground E32 to the **same channel, air rate, FEC and address** (the organisers assign the channel at the finals; ground E32 tool: `firmware/build_backup/e32_config.py`).
2. **Antenna on both before any power**: transmitting without one destroys the E32.
3. Ground: E32 → USB-UART adapter → laptop, then
   `python3 ground/snailpulse_monitor.py --port <adapter port> --baud <ground E32 UART rate>`.

### B. You write the comm board's firmware

Your firmware must look to the FC like the stock one:
1. UART to the FC: **115200 baud, 8 data bits, even parity, 1 stop bit**.
2. Collect bytes into frames (`sp_parser_feed()` from `snailpulse_frames.h` tells you when one is complete; `parser.raw` / `raw_len` are the exact bytes).
3. Send each frame as **one** LoRa packet, unchanged.
4. When it has gone out, send **one byte 0x06** back to the FC.
5. Bytes that are not frames can be dropped.

### C. Your code runs on the FC and owns the radio (one combined firmware)

The SnailPulse firmware stays the main program; your code becomes a module inside it:
1. Copy your `.cpp`/`.h` files and `example_fc_hooks.cpp` (rename it, e.g. `radio_tech.cpp`) into `firmware/SnailPulse_Flight/`.
2. In `config.h` set `#define SP_OWN_RADIO 0`. SnailPulse then **never touches UART1**, and hands every frame to `sp_frame_out(frame, len)`, which you implement. The template queues the frames and sends them with the ACK rule; replace the sending part with your radio code.
3. Your old `setup()` / `loop()` become functions called from `sp_ext_setup()` / `sp_ext_loop(now_ms)`.

Rules inside the FC firmware:

| Rule | Why |
|---|---|
| `sp_ext_loop()` must return in a few ms; no `delay()` loops, no waiting for the radio | The loop also runs the sensors; a 4 s hang triggers the watchdog reset |
| Don't call `analogRead()` | The heartbeat ADC is read only inside a 50 Hz timer interrupt (TIM6); extra ADC use breaks its timing |
| Don't use pins PB0, PB1, PC6 (our analog/DHT-11), PD12/PD13 (our I²C4), PA2/PA3 (USB-C copy), timer TIM6 | Used by SnailPulse |
| I²C1 (PB6/PB7) already has the FC sensors open (`FcWire`) | Don't create a second `TwoWire` on the same pins |
| Flash: SnailPulse uses about 63 % of the 128 KB | Leave room; check the "Sketch uses" line after compiling |

Build and flash exactly as for SnailPulse (`docs/Software_Setup.md`, `docs/Bench_Test.md`).

### D. Your own ground program

- **C / C++ / Arduino:** `snailpulse_frames.h`. Feed every received byte to `sp_parser_feed()`; on a 1, `memcpy` the payload into the matching `struct` (check `parser.len >= sizeof(struct)`; ENV frames from old firmware are 24 bytes, SNAIL frames 39). `ground_receiver/` is a complete Arduino example that forwards frames to USB, so our monitor still works through your board.
- **Python:** `from snailpulse_ground import Parser, decode` (folder `ground/`).
- Anything else: the table and structs above; test with the frames below.

## 4. Test frames

Built by the flight code itself (`firmware/tests/test_host.cpp`); your parser must accept all four:

```
SNAIL  AA 55 10 01 31 88 13 00 00 F5 00 00 00 00 00 4D 00 00 00 00 00 80 96 A5 A6 9B 85 6E 5D 58 61 74 8C 9F A7 A2 90 79 64 58 5B 14 00 00 FB 00 28 00 40 01 B4 00 8C 23 61 55
ENV    AA 55 11 02 24 70 17 00 00 00 80 FF FF EC 8A 01 00 DA 07 00 80 00 00 00 00 76 00 00 00 34 08 7C 15 9C 01 00 00 00 00 00 80 7C C1
BASE   AA 55 01 03 22 58 1B 00 00 B8 0B 00 00 2E FB A4 A3 03 00 00 00 00 00 E8 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 64 84
STATUS AA 55 7F 04 18 40 1F 00 00 05 01 03 00 00 00 00 00 00 00 00 00 00 00 00 00 00 0F 00 00 44 FE
```

Expected (fw 1.5 values): SNAIL snail 1 24.5 bpm, quality 77; snail 2 25.1 bpm, quality 64, IR 9100 · ENV SHT21 fields invalid (box T = -32768, RH = 0xFFFF: the GY-21 was removed), BMP280 101100 Pa / 20.10 °C, second BMP280 0 Pa (not fitted), DHT-11 21.00 °C / 55 %, MQ-7 412 mV, sensors_ok 0x76 · BASE 3000 Pa, altitude 23850.0 m · STATUS firmware 0x0105, boot 3, fc_ok 0x0F. SNAIL frames from firmware before 1.3 are 46 bytes (payload 39): accept both. Flip any one byte and the CRC must reject the frame.

Our monitor can replay a capture: `python3 ground/snailpulse_monitor.py --file capture.bin`, or try it with no hardware: `--demo`.

## 5. Test tools: check one hop at a time

Find a fault by testing each side alone before joining them. All FC sketches upload exactly like the flight firmware (`docs/Software_Setup.md`); their report comes out of the FC USB-C at 115200 (Serial Monitor, any line ending). They need only the FC, not the payload board.

| What you test | Setup | Tool | Good result |
|---|---|---|---|
| This laptop's tools | nothing | `python3 radio_handoff/tools/sp_link_tool.py selftest` | `ALL PASSED` |
| FC → UART1, no comm board | USB-UART adapter (3.3 V) on FC UART1: PA9 → adapter RX, PA10 ← adapter TX, GND | `LinkTest_FC` on the FC, `sp_link_tool.py fake-comm --port <adapter>` on the laptop | Laptop prints each frame (snail 1 30.0 bpm, snail 2 45.0 bpm); FC report: `no ACK 0`, ACK time ≈ 150 ms (the fake delay) |
| The same with the real flight firmware | as above, SnailPulse_Flight on the FC | `fake-comm` | Real sensor values; STATUS `fw 1.5`. `--no-ack`: FC keeps sending every 1.5 s and counts missed ACKs |
| Comm board alone (stock or your firmware) | adapter on the comm board's FC UART lines (IntroBus_S UART1), antenna on, ground E32 listening | `sp_link_tool.py fake-fc --port <adapter>` (add `--flood` for throughput) | `no ACK 0`, steady ACK time (a few hundred ms at air rate 2400); the ground monitor shows the test frames |
| Comm board settings | FC + comm board joined | `CommBridge_FC` on the FC, type in the Serial Monitor; `~f` sends one test frame | Answers to your commands; `<06>` after `~f` |
| Whole link | FC + comm board + ground E32 | `LinkTest_FC` (`n` flight timing, `f` flood) + `snailpulse_monitor.py --port <ground port>` | Monitor shows the test frames: altitude climbs 1 m per BASE frame, STATUS boot count 0xC0DE; few % lost frames at most |
| Ground E32 settings | ground E32 in config mode (both switches down) | `python3 firmware/build_backup/e32_config.py --port <adapter>` | Channel / air rate match the satellite's comm board |

The test frames from `LinkTest_FC` and `fake-fc` have the real format with made-up values, so every ground tool decodes them. `fake-comm --save capture.bin` records what the FC sent; replay it with `snailpulse_monitor.py --file capture.bin`.

Never power the comm board or an E32 without its antenna: transmitting without one destroys the E32. USB-UART adapters must be set to 3.3 V logic.

## 6. Joint test checklist

0. **Each side alone** with the tools in section 5.
1. **Bench, USB only:** FC flashed with SnailPulse, USB-C to a laptop, `snailpulse_monitor.py` all green except "Comm board ACKs" (no comm board yet).
2. **Add the comm board + antennas + ground E32:** the monitor on the ground port shows the same values; "Comm board ACKs" turns OK; lost frames stay at a few % at most.
3. **Range:** carry the satellite away (or attenuate). Watch "Lost frames" and CRC errors.
4. **Long run:** 3 h powered; STATUS keeps arriving every 10 s, `frames_dropped` stays 0.
