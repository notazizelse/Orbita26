#!/bin/bash
# SnailPulse: one-shot Mac setup.
#  - fixes an Arduino IDE that hangs on its loading screen (clean reinstall + reset of its state)
#  - installs the STM32 board package, the 3 libraries, the ST-Link tools and the ground monitor
#  - compiles SensorTest and the flight firmware to prove it all works
#
# Run from the Orbita26 folder (takes 5-15 min the first time, mostly downloads):
#     bash scripts/mac_setup.sh
# Where arduino.cc is blocked (Russia), give it the offline bundle (scripts/build_mac_arduino_bundle.py,
# or an Arduino15 folder packed on a Mac outside; docs/Software_Setup.md section 4):
#     bash scripts/mac_setup.sh --arduino15 ~/Downloads/arduino-offline-mac-arm64.tgz
# An Arduino IDE .dmg next to it (or in ~/Downloads) is installed from there instead of from GitHub.
#
# Nothing is deleted: the old IDE settings go to ~/arduino-ide-backup-<date>/.

set -u
CORE_VER=2.12.0
STM_URL=https://github.com/stm32duino/BoardManagerFiles/raw/main/package_stmicroelectronics_index.json
FQBN=STMicroelectronics:stm32:GenH7:pnum=GENERIC_H750VBTX
REPO="$(cd "$(dirname "$0")/.." && pwd)"
A15="$HOME/Library/Arduino15"
APP="/Applications/Arduino IDE.app"
TGZ=""
[ "${1:-}" = "--arduino15" ] && TGZ="${2:-}"

say()  { printf "\n\033[1m== %s\033[0m\n" "$*"; }
ok()   { printf "  \033[32mOK\033[0m   %s\n" "$*"; }
warn() { printf "  \033[33mWARN\033[0m %s\n" "$*"; }
die()  { printf "  \033[31mSTOP\033[0m %s\n" "$*"; exit 1; }

[ "$(uname)" = Darwin ] || die "this script is for macOS"
[ -f "$REPO/firmware/SnailPulse_Flight/SnailPulse_Flight.ino" ] || die "run it from the Orbita26 folder: bash scripts/mac_setup.sh"
[ -z "$TGZ" ] || [ -f "$TGZ" ] || die "file not found: $TGZ"
ARCH=$(uname -m)
echo "Mac: $(sw_vers -productVersion), $ARCH"

# ---------------------------------------------------------------------------------------------
say "1/7 Homebrew"
for b in /opt/homebrew/bin/brew /usr/local/bin/brew; do
  command -v brew >/dev/null 2>&1 || { [ -x "$b" ] && eval "$("$b" shellenv)"; }
done
if ! command -v brew >/dev/null 2>&1; then
  echo "  Installing Homebrew (it asks for your Mac password)..."
  /bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)" || die "Homebrew install failed"
  for b in /opt/homebrew/bin/brew /usr/local/bin/brew; do [ -x "$b" ] && eval "$("$b" shellenv)"; done
fi
command -v brew >/dev/null 2>&1 || die "brew not found after install; open a new Terminal and run this script again"
ok "$(brew --version | head -1)"

# ---------------------------------------------------------------------------------------------
say "2/7 Command-line tools (git, Python, arduino-cli, ST-Link / OpenOCD / GDB)"
brew install git python gnu-getopt arduino-cli stlink open-ocd arm-none-eabi-gdb || die "brew install failed (network?)"
ok "installed"

# ---------------------------------------------------------------------------------------------
say "3/7 Reset the Arduino IDE (fixes the endless loading screen)"
osascript -e 'quit app "Arduino IDE"' >/dev/null 2>&1
sleep 2
pkill -f "Arduino IDE" 2>/dev/null; pkill -f arduino-cli 2>/dev/null; pkill -f arduino-language-server 2>/dev/null
BK="$HOME/arduino-ide-backup-$(date +%Y%m%d-%H%M%S)"
moved=0
for d in "$HOME/.arduinoIDE" "$HOME/Library/Application Support/arduino-ide" "$A15/staging" "$A15/tmp"; do
  if [ -e "$d" ]; then mkdir -p "$BK"; mv "$d" "$BK/" && moved=1; fi
done
# A half-downloaded index file also freezes the IDE: move broken ones away.
for f in "$A15"/package_index.json "$A15"/library_index.json "$A15"/package_stmicroelectronics_index.json; do
  if [ -f "$f" ] && ! python3 -c 'import json,sys; json.load(open(sys.argv[1]))' "$f" 2>/dev/null; then
    mkdir -p "$BK"; mv "$f" "$BK/"; rm -f "$f.sig"; moved=1; warn "broken $(basename "$f") moved away"
  fi
done
[ $moved = 1 ] && ok "old IDE state moved to $BK" || ok "nothing to reset"

mkdir -p "$HOME/.arduinoIDE"
cat > "$HOME/.arduinoIDE/arduino-cli.yaml" <<EOF
board_manager:
  additional_urls:
    - $STM_URL
EOF
cat > "$HOME/.arduinoIDE/settings.json" <<EOF
{
  "arduino.checkForUpdates": false
}
EOF
ok "IDE config: STM32 board URL added, start-up update check off"

# ---------------------------------------------------------------------------------------------
say "4/7 Arduino IDE app"
DMG=""
for d in "$(dirname "${TGZ:-/nonexistent}")" "$HOME/Downloads" "$HOME/Desktop"; do
  [ -z "$DMG" ] && DMG=$(ls "$d"/arduino-ide_*_macOS_*.dmg 2>/dev/null | head -1)
done
if [ -n "$DMG" ]; then
  echo "  installing from $DMG"
  MNT=$(hdiutil attach -nobrowse -readonly "$DMG" | awk -F'	' '/\/Volumes\//{print $NF}' | tail -1)
  [ -d "$MNT/Arduino IDE.app" ] || die "no Arduino IDE.app inside $DMG"
  [ -d "$APP" ] && { mkdir -p "$BK"; mv "$APP" "$BK/"; }
  ditto "$MNT/Arduino IDE.app" "$APP" && hdiutil detach -quiet "$MNT"
  xattr -dr com.apple.quarantine "$APP" 2>/dev/null
  ok "Arduino IDE installed from $(basename "$DMG")"
elif brew list --cask arduino-ide >/dev/null 2>&1; then
  brew upgrade --cask arduino-ide >/dev/null 2>&1
  ok "Arduino IDE (Homebrew) is installed"
else
  brew install --cask --force arduino-ide || die "Arduino IDE install failed"
  ok "Arduino IDE installed to /Applications (from GitHub)"
fi
if [ "$ARCH" = arm64 ] && [ "$(lipo -archs "$APP/Contents/MacOS/Arduino IDE" 2>/dev/null)" = x86_64 ]; then
  warn "the IDE in /Applications is an Intel build on an Apple-silicon Mac; run: brew reinstall --cask --force arduino-ide"
fi

# ---------------------------------------------------------------------------------------------
say "5/7 STM32 board package $CORE_VER + libraries"
if [ -n "$TGZ" ]; then
  if [ -d "$A15/packages" ]; then mkdir -p "$BK"; mv "$A15" "$BK/Arduino15-before"; fi
  # Offline bundle = paths from your home folder (Library/Arduino15, Documents/Arduino);
  # an Arduino15 folder packed on another Mac starts with Arduino15/.
  if tar -tzf "$TGZ" | head -3 | grep -q '^Library/'; then DEST="$HOME"; else DEST="$HOME/Library"; fi
  tar -xzf "$TGZ" -C "$DEST" || die "could not unpack $TGZ"
  # Files copied from another computer can carry the "downloaded" flag, which makes macOS block
  # the compiler and tools on first use. Clear it.
  xattr -dr com.apple.quarantine "$A15" "$HOME/Documents/Arduino" 2>/dev/null
  ok "unpacked $(basename "$TGZ")"
fi
[ -f "$A15/arduino-cli.yaml" ] || arduino-cli config init >/dev/null
arduino-cli config get board_manager.additional_urls 2>/dev/null | grep -q stm32duino \
  || arduino-cli config add board_manager.additional_urls "$STM_URL"
arduino-cli config set library.enable_unsafe_install true

ARD=$(curl -s -o /dev/null -w '%{http_code}' -m 20 https://downloads.arduino.cc/packages/package_index.json)
GH=$(curl -s -o /dev/null -w '%{http_code}' -m 20 -L "$STM_URL")
echo "  downloads.arduino.cc: HTTP $ARD    github.com: HTTP $GH"
if [ "$GH" != 200 ]; then
  [ -n "$TGZ" ] && warn "github.com not reachable: using only the offline bundle" \
    || die "github.com is not reachable from this network; get the offline bundle (docs/Software_Setup.md)"
fi

HAVE_BUILTIN=0; [ -d "$A15/packages/builtin/tools/ctags" ] && HAVE_BUILTIN=1
if [ "$ARD" != 200 ] && [ $HAVE_BUILTIN = 0 ]; then
  warn "downloads.arduino.cc is blocked here and Arduino's helper tools are missing."
  warn "Get the Arduino15 folder from a Mac outside (docs/Software_Setup.md, section 4), then run:"
  warn "    bash scripts/mac_setup.sh --arduino15 <that .tgz file>"
  CORE_OK=0
elif arduino-cli core list 2>/dev/null | grep -q "STMicroelectronics:stm32 *$CORE_VER"; then
  CORE_OK=1; ok "STM32 board package $CORE_VER already installed"
else
  arduino-cli core update-index || warn "index update had errors (normal where arduino.cc is blocked)"
  if arduino-cli core install "STMicroelectronics:stm32@$CORE_VER"; then CORE_OK=1; else CORE_OK=0; warn "board package install failed"; fi
fi
LIBDIR="$HOME/Documents/Arduino/libraries"
if [ -d "$LIBDIR/STM32SD" ] && [ -d "$LIBDIR/FatFs" ] && [ -d "$LIBDIR/IntroStratLib-Public" ]; then
  ok "libraries already installed"
else
  arduino-cli lib install --git-url \
    https://github.com/stm32duino/STM32SD.git#1.5.0 \
    https://github.com/stm32duino/FatFs.git#4.0.4 \
    https://github.com/Obu-IntroSat/IntroStratLib-Public.git#0.1.2 >/dev/null 2>&1
fi
arduino-cli lib list 2>/dev/null | grep -Ei "STM32SD|FatFs|IntroStrat" | sed 's/^/  /'

# ---------------------------------------------------------------------------------------------
say "6/7 Test compile (SensorTest + flight firmware; first run takes a few minutes)"
BUILD_OK=0
if [ "${CORE_OK:-0}" = 1 ]; then
  BUILD_OK=1
  for s in firmware/tools/SensorTest firmware/SnailPulse_Flight; do
    if out=$(arduino-cli compile -b "$FQBN" "$REPO/$s" 2>&1); then
      ok "$(basename "$s"): $(echo "$out" | grep -m1 'Sketch uses' | cut -c1-70)"
    else
      BUILD_OK=0; echo "$out" | tail -15; warn "$(basename "$s") did not compile"
    fi
  done
else
  warn "skipped (no board package yet)"
fi

# ---------------------------------------------------------------------------------------------
say "7/7 Ground monitor (Python + pyserial in $REPO/.venv)"
PY="$(brew --prefix)/bin/python3"
[ -x "$PY" ] || PY=python3
"$PY" -m venv "$REPO/.venv" && "$REPO/.venv/bin/pip" install -q --upgrade pip pyserial matplotlib \
  && ok "run:  .venv/bin/python ground/snailpulse_monitor.py" || warn "Python setup failed"

st-info --probe 2>/dev/null | grep -qi "chipid" && ok "ST-Link found and it sees a chip" \
  || echo "  (ST-Link not connected or FC not powered: fine for now)"
ls /dev/cu.* 2>/dev/null | grep -Ei "usbserial|wchusb" | sed 's/^/  serial port: /'

# ---------------------------------------------------------------------------------------------
say "Done"
if [ $BUILD_OK = 1 ]; then ok "toolchain works"; else warn "toolchain not complete: read the WARN lines above"; fi
cat <<'EOF'

Next:
  1. Open "Arduino IDE" from Applications. First start can take ~1 min (it indexes the board package).
  2. Tools > Board > STM32 MCU based boards > Generic STM32H7 Series
     Tools > Board part number > Generic H750VBTx
     Tools > Upload method > OpenOCD STLink (SWD)        (no ST software needed)
     Tools > Port > the FC's USB-C (usbserial / wchusbserial), for the Serial Monitor
  3. File > Open > Orbita26/firmware/tools/SensorTest/SensorTest.ino, then Upload.
     Wiring and power order: docs/Bench_Test.md
  4. Ground monitor:  .venv/bin/python ground/snailpulse_monitor.py   (or --demo to try it without hardware)

If the IDE still sits on the loading screen, start it from Terminal and keep the log:
  "/Applications/Arduino IDE.app/Contents/MacOS/Arduino IDE" 2>&1 | tee ~/arduino-ide-start.log
If no usbserial port shows up for the FC's USB-C, install the CH34x driver:
  brew install --cask wch-ch34x-usb-serial-driver      (then allow it in System Settings > Privacy & Security)
EOF
