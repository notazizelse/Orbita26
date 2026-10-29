# Software setup for building and debugging (macOS, from Russia without VPN)

How to build the SnailPulse code from source, flash it, and step through it with a debugger on a Mac in Russia, with no VPN. Checked 29 Oct 2026. Windows notes are at the end.

**Quick start (Mac):** in the `Orbita26` folder run

```bash
bash scripts/mac_setup.sh
```

It installs everything in sections 1 and 4 and fixes an Arduino IDE stuck on its loading screen: a clean reinstall from GitHub, with its old settings moved to a backup folder. It also sets up the ground monitor and test-compiles the code. Then follow `docs/Bench_Test.md` for wiring, power order and upload. STM32CubeIDE and CubeProgrammer (sections 2, 3 and 6) are only needed for step-by-step debugging.

## What is blocked and what we use instead

| Need | Official source | From Russia | What this guide uses |
|---|---|---|---|
| STM32CubeIDE (debugger) | st.com | **Refused** (ST applies export-control rules; reports since 2022, a browser VPN also failed) | ST's own installer from the **dl.sisoog.com** mirror, checked with Apple's signature check |
| STM32CubeProgrammer | st.com | **Refused** | The **STM32_Programmer_CLI** that ships inside STM32CubeIDE. No mirror has the macOS GUI version |
| Arduino toolchain | arduino.cc | **Blocked** (Cloudflare 403, Oct 2026) | arduino-cli from Homebrew; the STM32 core and libraries from GitHub; one small folder copied from a computer outside Russia (section 4) |
| Homebrew, GitHub, PyPI | | Reachable | Everything else |

There is no Homebrew package for any ST tool (checked: only `gcc-arm-embedded` and SEGGER tools exist).

**About the mirror:** dl.sisoog.com is run by Sisoog, an Iranian electronics site that hosts ST tools because st.com is also closed to Iran. It is not ST. The files carry ST's original names and sizes, but no checksums are published, so **check the signature in step 2.3 before running anything**. ST closes these downloads to Russia on purpose; whether to use a mirror is your decision.

---

## 1. Base tools (Homebrew)

```bash
# Homebrew (skip if `brew --version` works). Installer comes from GitHub.
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# git, Python, GNU getopt (the STM32 upload script needs it on macOS), Arduino CLI,
# and the open-source debug tools used as a fallback in section 7
brew install git python gnu-getopt arduino-cli open-ocd arm-none-eabi-gdb stlink

# Apple silicon only (M1/M2/M3/M4): STM32CubeIDE for Mac is an Intel build and runs under Rosetta
softwareupdate --install-rosetta --agree-to-license

# The project
git clone https://github.com/notazizelse/Orbita26.git
cd Orbita26
git checkout sensors-dht11-mq7        # until PR #1 is merged into main
# Python packages go in a private folder (.venv): Homebrew's Python refuses a plain `pip install`
"$(brew --prefix)/bin/python3" -m venv .venv
.venv/bin/pip install pyserial matplotlib
```

## 2. STM32CubeIDE 2.0.0 for macOS (debugger, includes CubeProgrammer CLI)

### 2.1 Download (~930 MB)

```bash
mkdir -p ~/Downloads/st && cd ~/Downloads/st
curl -fL -C - -O https://dl.sisoog.com/st_software/CubeIDE/st-stm32cubeide_2.0.0_26820_20251114_1348_x86_64.dmg.zip
ls -l st-stm32cubeide_2.0.0_26820_20251114_1348_x86_64.dmg.zip     # must be 932944726 bytes
```

If the download stops, run the same `curl` line again: `-C -` resumes it.

### 2.2 Install

```bash
unzip st-stm32cubeide_2.0.0_26820_20251114_1348_x86_64.dmg.zip      # gives the .dmg (948120741 bytes)
open st-stm32cubeide_2.0.0_26820_20251114_1348_x86_64.dmg
```

Follow the installer window that opens (accept ST's licence, install into Applications).

### 2.3 Check that it is ST's original (do not skip)

```bash
APP=$(ls -d /Applications/STM32CubeIDE*.app | head -1); echo "$APP"
codesign --verify --deep --strict --verbose=2 "$APP"          # must end with "satisfies its Designated Requirement"
codesign -dv --verbose=4 "$APP" 2>&1 | grep Authority          # first line must name STMicroelectronics
```

If any check fails, or the Authority line names anyone other than STMicroelectronics, delete the app and the download, and don't run it.

Start it once from Applications. It may offer a myST login or updates: close/skip them, nothing below needs them.

## 3. STM32CubeProgrammer: use the CLI inside STM32CubeIDE

There's no VPN-free macOS copy of the STM32CubeProgrammer app. STM32CubeIDE carries its command-line version, which is all that flashing and the Arduino **Upload** need.

```bash
CLI=$(find "$APP" -name STM32_Programmer_CLI -type f | head -1); echo "$CLI"
"$CLI" --version

# Put it where the STM32 Arduino core looks for it on macOS
PDIR=/Applications/STMicroelectronics/STM32Cube/STM32CubeProgrammer/STM32CubeProgrammer.app/Contents/MacOs
sudo mkdir -p "$PDIR"
sudo ln -sfn "$(dirname "$CLI")" "$PDIR/bin"
"$PDIR/bin/STM32_Programmer_CLI" --version                    # same output as above
```

With the ST-Link wired to the flight controller (see the box below) and the FC powered:

```bash
"$CLI" -c port=SWD mode=UR          # should print Device ID 0x450 (STM32H7xx) and flash size 128 KBytes
```

> **Wiring.** Upload/debug target is always the **flight controller** (STM32H750). ST-Link → FC **PROG** header: SWCLK → CLK, SWDIO → SWD, GND → GND. Leave the ST-Link's 3.3 V unconnected when the power module is on. BOOT0 jumper on **0**. macOS needs no ST-Link driver.

## 4. Arduino toolchain without arduino.cc

The STM32 board package, its compiler, OpenOCD and our three libraries all download from **GitHub**. Only arduino-cli's own small helper tools (ctags, serial discovery) come from `downloads.arduino.cc`, which is blocked. So someone **outside Russia** does the setup once on a Mac of the **same chip type** (Apple silicon or Intel: `uname -m` must print the same on both) and sends the folder.

**4a. On the Mac outside Russia:**

```bash
brew install arduino-cli
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
arduino-cli config set library.enable_unsafe_install true
arduino-cli core update-index
arduino-cli core install STMicroelectronics:stm32@2.12.0
tar -czf arduino15-mac-$(uname -m).tgz -C ~/Library Arduino15     # ~0.5-1 GB; send it any way that reaches the client
```

**4b. On the client's Mac:**

```bash
uname -m                                                    # must match the file name you received
tar -xzf arduino15-mac-arm64.tgz -C ~/Library               # or ...-x86_64.tgz
arduino-cli core list                                       # must show STMicroelectronics:stm32 2.12.0
arduino-cli lib install --git-url \
  https://github.com/stm32duino/STM32SD.git#1.5.0 \
  https://github.com/stm32duino/FatFs.git#4.0.4 \
  https://github.com/Obu-IntroSat/IntroStratLib-Public.git#0.1.2
```

Nothing after this needs arduino.cc. (`arduino-cli core update-index` will report an error for arduino.cc from Russia; you don't need to run it.)

## 5. Build with debug information, then flash

Run from the `Orbita26` folder. The two extra board options add debug symbols (`-g`) and use `-Og`, which keeps the code close to the source so stepping makes sense:

```bash
FQBN="STMicroelectronics:stm32:GenH7:pnum=GENERIC_H750VBTX,opt=ogstd,dbg=enable_sym"

arduino-cli compile -b "$FQBN" --output-dir ~/snail-build/SensorTest firmware/tools/SensorTest
st-flash --reset write ~/snail-build/SensorTest/SensorTest.ino.bin 0x08000000
```

In the Arduino IDE the same is **Upload** with Tools → Upload method → **OpenOCD STLink (SWD)** (no ST software needed) plus Tools → Optimize → Debug (-Og) and Debug symbols for debugging.

Same for the other sketches: `firmware/SnailPulse_Flight` (flight firmware) and `firmware/tools/HB_Tune`. Each output folder gets a `.elf` (for the debugger) and a `.bin`.

Flashing straight with the CLI also works:

```bash
"$CLI" -c port=SWD mode=UR -d ~/snail-build/SensorTest/SensorTest.ino.bin 0x08000000 -v -rst
```

Serial output (SensorTest results, debug prints) is on the FC's USB-C at 115200:

```bash
ls /dev/cu.*                                                    # e.g. /dev/cu.usbserial-1410
.venv/bin/python -m serial.tools.miniterm /dev/cu.usbserial-1410 115200  # Ctrl+] to quit
```

For flight, a normal build without the debug options (`FQBN=STMicroelectronics:stm32:GenH7:pnum=GENERIC_H750VBTX`) is smaller and faster.

## 6. Debug in STM32CubeIDE

1. Open STM32CubeIDE and pick any empty workspace folder, e.g. `~/snail-ws`.
2. **File → Import → C/C++ → STM32 Cortex-M Executable → Next.**
   - Executable: `~/snail-build/SensorTest/SensorTest.ino.elf`
   - MCU: **STM32H750VBTx**
   - Finish. The Debug Configurations window opens.
3. **Debugger** tab:
   - Debug probe: **ST-LINK (ST-LINK GDB server)**. If it can't connect on macOS, switch this to **ST-LINK (OpenOCD)**.
   - Interface: **SWD**.
   - Reset behaviour: **Connect under reset**.
4. **Startup** tab: leave **Download** ticked so it flashes the ELF. Click **Debug**.
5. It stops at `main`. Open the sketch with **File → Open File…** (e.g. `firmware/tools/SensorTest/SensorTest.ino`), double-click in the margin to set breakpoints, then press Resume (F8).
   - Useful views: Variables, Expressions, **Live Expressions** (watch values without stopping), and **SFRs** (STM32 registers, e.g. I2C4 or GPIOC).
6. After changing code, re-run the `arduino-cli compile` line from section 5. Then start the same debug configuration again (Run → Debug History); it reloads the new ELF.

Notes:
- The flight firmware's 4 s watchdog is paused while the debugger holds the chip (fw 1.2, `SnailPulse_Flight.ino` setup), so a breakpoint doesn't reset the board. Builds older than 29 Oct 2026 reset 4 s after stopping.
- While stopped, the 50 Hz heartbeat sampling and the radio stop too. That's expected; don't judge sensor timing from a session that hit breakpoints.

## 7. Fallback: debugging without any ST software

If the mirror is unreachable or the signature check fails, Homebrew's open-source tools from section 1 do the same over the same ST-Link:

```bash
# terminal 1
openocd -f interface/stlink.cfg -f target/stm32h7x.cfg

# terminal 2
arm-none-eabi-gdb ~/snail-build/SensorTest/SensorTest.ino.elf \
  -ex "target extended-remote :3333" -ex "monitor reset halt" -ex load \
  -ex "break setup" -ex continue
```

Flash only: `st-flash --reset write ~/snail-build/SensorTest/SensorTest.ino.bin 0x08000000`. To make `arduino-cli upload` use OpenOCD instead of the CubeProgrammer CLI, add `,upload_method=OpenOCDSTLink` to the FQBN.

---

## Windows

The same mirror has Windows installers:

| What | Link | Size |
|---|---|---|
| STM32CubeIDE 2.1.1 | `https://dl.sisoog.com/st_software/CubeIDE/st-stm32cubeide_2.1.1_28236_20260312_0043_x86_64.exe.zip` | 866 MB |
| STM32CubeProgrammer 2.21.0 (64-bit) | `https://dl.sisoog.com/st_software/cubeprogrammer/stm32cubeprg-win64-v2-21-0.zip` | 187 MB |

```powershell
winget install --id Git.Git -e
winget install --id Python.Python.3.12 -e
winget install --id ArduinoSA.CLI -e          # arduino-cli; if winget can't fetch it, use the zip from github.com/arduino/arduino-cli/releases
curl.exe -fL -C - -O https://dl.sisoog.com/st_software/CubeIDE/st-stm32cubeide_2.1.1_28236_20260312_0043_x86_64.exe.zip
curl.exe -fL -C - -O https://dl.sisoog.com/st_software/cubeprogrammer/stm32cubeprg-win64-v2-21-0.zip
Expand-Archive st-stm32cubeide_2.1.1_28236_20260312_0043_x86_64.exe.zip -DestinationPath cubeide
Expand-Archive stm32cubeprg-win64-v2-21-0.zip -DestinationPath cubeprog
# Check that every installer is signed by ST before running it:
Get-ChildItem cubeide, cubeprog -Recurse -Include *.exe | Get-AuthenticodeSignature | Format-Table Status, @{n='Signer';e={$_.SignerCertificate.Subject}}, Path
```

Every line must show **Valid** with `CN=STMicroelectronics...`. Install CubeProgrammer to its default folder, and the Arduino core's Upload finds it. Section 4's Arduino folder on Windows is `%LOCALAPPDATA%\Arduino15`, so copy it from a Windows PC outside Russia. Build and debug as in sections 5–6.

---

## Ground monitor

`ground/snailpulse_monitor.py` shows every sensor with OK / WARN / FAIL in the browser, from the FC's USB-C (bench) or the ground LoRa module (over the air). Usage: `docs/Bench_Test.md` section 5.

## Ground LoRa module

The ground E32 module's settings (channel, air rate) must match the satellite's comm board. On a Mac, `firmware/build_backup/e32_config.py` reads and sets them (it replaces the Windows-only "RF Setting" tool). It needs only `pyserial`.
