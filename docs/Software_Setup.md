# Software setup (macOS and Windows, works from Russia)

Everything SnailPulse needs, with download commands that avoid the sites that are blocked from Russian IP addresses. Checked 29 Oct 2026.

| Site | From Russia | What we use instead |
|---|---|---|
| `arduino.cc`, `downloads.arduino.cc` | **Blocked** (Cloudflare 403 for Russian IPs, reported 11 Oct 2026). The Arduino IDE also hangs on start because it fetches its board and library lists from there. | Arduino IDE and arduino-cli installers from **GitHub releases**. For flashing only, no Arduino software at all. |
| `st.com` (STM32CubeProgrammer, ST-Link driver) | Downloads refused under export-control rules for some countries | Open-source **stlink** tools (`st-flash`) from GitHub / Homebrew. On Windows, the **Zadig** USB driver instead of ST's driver. |
| `github.com`, Homebrew (`ghcr.io`), PyPI | Normally reachable | Used for everything below |

There are two ways to use the board:

- **A. Flash the ready-made firmware** (recommended, nothing from arduino.cc or st.com). The compiled `.bin` files are in `firmware/build_backup/`.
- **B. Build from source** (only needed to change the code). This needs `downloads.arduino.cc` once; see section B.

Upload target is always the **flight controller** (STM32H750). ST-Link wires to its **PROG** header: SWCLK → CLK, SWDIO → SWD, GND → GND. Leave the ST-Link's 3.3 V unconnected when the power module is on. BOOT0 jumper on **0**.

---

## A1. macOS: flash only

```bash
# 1. Homebrew (skip if `brew --version` already works). Installer comes from GitHub.
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# 2. git, the open-source ST-Link tools, Python
brew install git stlink python

# 3. The project (flight code, test sketch, prebuilt .bin files, ground station)
git clone https://github.com/notazizelse/Orbita26.git
cd Orbita26
git checkout sensors-dht11-mq7        # until PR #1 is merged into main

# 4. Ground station + serial monitor
python3 -m pip install pyserial matplotlib
```

Flash and test:

```bash
st-info --probe                                   # must show an STM32H7 ("H74x/H75x"), flash 128 KiB
st-flash --reset write firmware/build_backup/SensorTest_core2.12.bin 0x08000000
ls /dev/cu.*                                      # find the FC's USB-C port, e.g. /dev/cu.usbserial-1410
python3 -m serial.tools.miniterm /dev/cu.usbserial-1410 115200    # Ctrl+] to quit; type t to rerun the test
```

When SensorTest shows 0 FAIL, flash the flight firmware:

```bash
st-flash --reset write firmware/build_backup/SnailPulse_Flight_v1.2_core2.12.bin 0x08000000
python3 ground/snailpulse_ground.py --port /dev/cu.usbserial-XXXX --baud <ground module rate>
```

The ground station should show `STATUS fw 1.2` and `ENV ... ok=0xF7`.

If `st-info --probe` finds nothing: check the 3 SWD wires, power the FC, and try `st-flash --connect-under-reset ...`. If the port doesn't show in `/dev/cu.*`, install the driver for the USB-serial chip printed next to the FC's USB-C (WCH CH34x or Silicon Labs CP210x; both have GitHub/vendor downloads).

## A2. Windows: flash only

PowerShell (winget comes with Windows 10/11):

```powershell
winget install --id Git.Git -e
winget install --id Python.Python.3.12 -e
# open a new PowerShell window so git and python are on PATH, then:
git clone https://github.com/notazizelse/Orbita26.git
cd Orbita26
git checkout sensors-dht11-mq7
python -m pip install pyserial matplotlib

# open-source ST-Link tools (GitHub release)
Invoke-WebRequest https://github.com/stlink-org/stlink/releases/download/v1.9.0/stlink-1.9.0-x86_64-w64-mingw32.zip -OutFile stlink.zip
Expand-Archive stlink.zip -DestinationPath C:\stlink
# st-flash.exe is in C:\stlink\stlink-1.9.0-x86_64-w64-mingw32\bin

# USB driver for the ST-Link without st.com: Zadig (GitHub release)
Invoke-WebRequest https://github.com/pbatard/libwdi/releases/download/v1.5.1/zadig-2.9.exe -OutFile zadig.exe
.\zadig.exe
```

In Zadig: Options → List All Devices → pick "STM32 STLink" → driver **WinUSB** → Install Driver. (Skip this if the ST-Link already works on that PC.)

```powershell
$st = "C:\stlink\stlink-1.9.0-x86_64-w64-mingw32\bin"
& "$st\st-info.exe" --probe
& "$st\st-flash.exe" --reset write firmware\build_backup\SensorTest_core2.12.bin 0x08000000
python -m serial.tools.miniterm COM5 115200          # COM number from Device Manager → Ports
```

---

## B. Build from source (to change the code)

All of these come from GitHub:

| What | macOS | Windows |
|---|---|---|
| Arduino IDE 2.3.10 | `arduino-ide_2.3.10_macOS_arm64.dmg` (Apple silicon) or `_macOS_64bit.dmg` (Intel) | `arduino-ide_2.3.10_Windows_64bit.exe` |
| arduino-cli 1.5.1 | `brew install arduino-cli` | `arduino-cli_1.5.1_Windows_64bit.zip` |

Download links: `https://github.com/arduino/arduino-ide/releases/tag/2.3.10` and `https://github.com/arduino/arduino-cli/releases/tag/v1.5.1`.

**The catch:** the first time they run, both still download Arduino's own index and helper tools (ctags, serial discovery) from `downloads.arduino.cc`. From Russia that needs one of:

1. a VPN switched on **only for the setup commands below** (after that everything is cached and works offline), or
2. copying the ready folder from a computer that already did this setup: `~/Library/Arduino15` + `~/Documents/Arduino/libraries` on macOS, `%LOCALAPPDATA%\Arduino15` + `Documents\Arduino\libraries` on Windows. Same OS and CPU type on both machines.

Setup (macOS shown; on Windows use `arduino-cli.exe` from the zip, same arguments). The STM32 package, its compiler and the three libraries all come from GitHub:

```bash
arduino-cli config init
arduino-cli config add board_manager.additional_urls https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
arduino-cli config set library.enable_unsafe_install true
arduino-cli core update-index
arduino-cli core install STMicroelectronics:stm32@2.12.0
arduino-cli lib install --git-url \
  https://github.com/stm32duino/STM32SD.git#1.5.0 \
  https://github.com/stm32duino/FatFs.git#4.0.4 \
  https://github.com/Obu-IntroSat/IntroStratLib-Public.git#0.1.2
```

Build and flash without STM32CubeProgrammer:

```bash
FQBN=STMicroelectronics:stm32:GenH7:pnum=GENERIC_H750VBTX
arduino-cli compile -b $FQBN --output-dir build/SensorTest firmware/tools/SensorTest
st-flash --reset write build/SensorTest/SensorTest.ino.bin 0x08000000
arduino-cli compile -b $FQBN --output-dir build/flight firmware/SnailPulse_Flight
st-flash --reset write build/flight/SnailPulse_Flight.ino.bin 0x08000000
```

In the Arduino IDE the same works: Tools → Board → Generic STM32H7 Series → Board part number **Generic H750VBTx**, then **Sketch → Export Compiled Binary**, then `st-flash` as above. The IDE's own Upload button needs STM32CubeProgrammer, which may not download from Russia.

Versions that are known to build (29 Oct 2026): STM32 core **2.12.0**, STM32SD 1.5.0, FatFs 4.0.4, IntroStratLib 0.1.2. SensorTest and HB_Tune also build on core 3.0.0; the flight code was only built on 2.12.0.

---

## Ground LoRa module

The ground E32 module's settings (channel, air rate) must match the satellite's comm board. On a Mac, `firmware/build_backup/e32_config.py` reads and sets them (it replaces the Windows-only "RF Setting" tool). It needs only `pyserial`.

## Checking the downloads

`firmware/build_backup/SHA256SUMS.txt` lists the checksums of the prebuilt `.bin` files:

```bash
cd firmware/build_backup && shasum -a 256 -c SHA256SUMS.txt          # macOS
```
```powershell
Get-FileHash firmware\build_backup\*.bin -Algorithm SHA256              # Windows: compare by eye
```
