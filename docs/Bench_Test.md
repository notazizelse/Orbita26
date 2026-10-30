# Bench test: what plugs where, power order, upload, ground monitor

The short answer:
- **Code goes in through the ST-Link** (PROG header), started by **Upload** in the Arduino IDE.
- **The FC's USB-C is only for watching**: the Serial Monitor or the ground monitor.
- **Power always comes from the power module**, from one source at a time: the 12 V bench supply on the desk, or the battery pack for untethered tests and flight.

Setting up the Mac first: `bash scripts/mac_setup.sh` (see `docs/Software_Setup.md`).

## 1. What plugs where

| Cable | From → to | What it does |
|---|---|---|
| **ST-Link** (3 wires) | ST-Link SWCLK → FC PROG **CLK**, SWDIO → **SWD**, GND → **GND**; ST-Link USB → Mac | Uploads the code; also used for debugging. |
| ST-Link 3.3 V pin | **Leave it unconnected** on the small USB-stick ST-Links (their 3.3 V pin is an output). | Two power sources on the stack at once is forbidden by the IntroSat guide. An official ST-LINK/V2/V3 or Nucleo ST-Link has a T_VCC/VAPP *sense* pin instead: connect that one to the FC's 3.3 V so it can measure the target; it powers nothing. |
| **USB-C cable** | FC USB-C → Mac | A CH340X USB-serial chip on UART2. SensorTest/HB_Tune print here; the flight firmware copies its radio frames here (115200). Treat it as data only. |
| **Power** | 12 V bench supply **or** 4S battery pack → power module **X1** (XT30) | Powers the whole stack. **X6 jumpers** switch on the payload 3.3 V and 5 V (their LEDs light). |
| **Antenna** | on the comm board's E32 | **Always on before power.** The flight firmware starts transmitting at boot; transmitting without an antenna destroys the E32. |
| Ground E32 (optional) | E32 on a USB-UART adapter → Mac | Only for over-the-air tests. |

BOOT0 jumper on the FC: **0** (normal run).

## 2. Power order

1. Everything off. Antenna on. BOOT0 on 0.
2. Plug the ST-Link into PROG and into the Mac. Plug the FC's USB-C into the Mac.
3. Switch on **one** source on X1:
   - **Desk work: 12 V bench supply**, current limit about 1.5 A. If the supply hits its limit, switch off and look for a short.
   - **No bench supply:** use the battery pack the same way.
4. Check that the X6 channel LEDs are lit, then upload.
5. Done, or changing wires: **power off first**, then unplug or rewire.

**When to connect the battery pack:** for anything away from the desk (the 3-hour freezer run, range tests, flight), or whenever you have no bench supply. Never together with the bench supply: switch off and unplug one before plugging in the other. Upload works fine on battery power, because the ST-Link doesn't power anything.

## 3. Upload from the Arduino IDE

| Tools menu | Setting |
|---|---|
| Board | STM32 MCU based boards → **Generic STM32H7 Series** |
| Board part number | **Generic H750VBTx** |
| Upload method | **OpenOCD STLink (SWD)**: needs no ST software. `STM32CubeProgrammer (SWD)` also works if STM32CubeProgrammer is installed |
| Optimize | Smallest (-Os default). For stepping in a debugger: Debug (-Og) + Debug symbols |
| Port | The FC's USB-C (`usbserial…` / `wchusbserial…`). Only used by the Serial Monitor; Upload goes through the ST-Link anyway |

Click **Upload**. The output ends with `** Programming Finished **`, `** Verified OK **` and `** Resetting Target **`; the FC then runs the new code.

If it fails with "Error: open failed" or "init mode failed": ST-Link not plugged in, the 3 wires swapped, or no power on the stack. With power on, `st-info --probe` in Terminal must list an STM32H7.

## 4. Which code, in order

| Step | Upload | Watch it with | Good result |
|---|---|---|---|
| 1 | `firmware/tools/SensorTest/SensorTest.ino` | Serial Monitor 115200, or the ground monitor's Console panel | Every line PASS (perfboard guide step 11 has the voltage checks to do first). Key `m` + Serial Plotter: snail 2's pulse from the MAX30100 |
| 2 | `firmware/tools/HB_Tune/HB_Tune.ino` | Serial Plotter 115200 | A slow regular wave from snail 1 (TCRT5000); set RV3 |
| 2b | `firmware/tools/MAX30100_Pulse/MAX30100_Pulse.ino` | Serial Monitor 115200 (key `p` + Serial Plotter for the wave) | Snail 2: a `BEAT` line per beat and the bpm. A fingertip shows 50–100 bpm within ~5 s; IR must stay under 65,000 (keys `+`/`-` set the LED) |
| 3 | `firmware/SnailPulse_Flight/SnailPulse_Flight.ino` | Ground monitor on the FC's USB-C | Everything OK except "Comm board ACKs" if no comm board is on |
| 4 | (same) + comm board + antennas + ground E32 | Ground monitor on the ground E32's port | Same values over the air; "Comm board ACKs" OK |

Only one program can open a serial port: close the Serial Monitor before starting the ground monitor, and the other way round.

## 5. Ground monitor

```bash
.venv/bin/python ground/snailpulse_monitor.py                         # finds the FC's USB-C by itself
.venv/bin/python ground/snailpulse_monitor.py --port /dev/cu.usbserial-0001 --baud 9600   # ground E32
.venv/bin/python ground/snailpulse_monitor.py --demo                  # try it without hardware
```

It opens `http://localhost:8765`. Every sensor gets a row with **OK / WARN / FAIL / WAITING / STALE**, its value and what to check. Charts show the heartbeat wave, heart rate, temperatures, humidity, pressures and the MQ-7. Everything received is saved in `logs/`.

| What you see | Likely cause |
|---|---|
| Data in: WAITING | Wrong port or baud, FC not powered, or the code is not the flight firmware (SensorTest prints text: see the Console panel) |
| FAIL on one I²C4 sensor (BMP280, MAX30100) | Its address wiring (BMP280 SDO → GND) or the I²C4 wires PD13/PD12; SensorTest's scan shows who answers |
| DHT-11 FAIL now and then | One missed reading is normal; FAIL all the time = DATA not on PC6 or no 5 V on the module |
| MQ-7 FAIL | 0 V: AO wire, divider or power off. ≈4.95 V: off the scale, or the divider is missing (measure PB1: it must stay under 3.3 V). It also needs ~3 min to warm up |
| Snail 1 "no pulse" | TCRT5000 off the shell, or RV3 not set (step 2) |
| Snail 2 "no contact" | IR below 1000: nothing in front of the MAX30100. Normal with no snail |
| Snail 2 "contact but no pulse yet" | Give it ~5 s after putting it on and keep it still |
| Comm board ACKs WARN on the USB bench test | Normal: no comm board is answering |
| Worst loop time WARN | Something blocks the loop; a watchdog reset follows at 4 s |
| STALE everywhere | Data stopped: cable, power, or the FC reset (boot count goes up) |
