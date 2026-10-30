// MAX30100_Pulse - bench tool for SNAIL 2 (MAX30100 pulse sensor on I2C4).
// Shows the heart rate (bpm) and counts the beats.
//
// UPLOAD TO: the IntroSat flight controller (same settings as SensorTest):
//   Tools > Board > STM32 MCU based boards > Generic STM32H7 Series, Board part number: Generic H750VBTx,
//   Upload method: OpenOCD STLink (SWD) or STM32CubeProgrammer (SWD).
// Output: FCM USB Type-C (UART2, PA3/PA2), 115200 baud. Serial Monitor, any line ending.
// Wiring: MAX30100 SDA -> PD13, SCL -> PD12, VIN -> +3V3, GND -> GND (address 0x57).
//
// What you see (text mode):
//   * one line per beat:  BEAT #12   interval 0.82 s = 73.1 bpm
//   * one status line per second: IR level, pulse size, average bpm, beats so far
// Keys:  t = text (default)   p = Serial Plotter view   r = reset the beat count
//        + / - = IR LED brighter / dimmer (start: 27.1 mA, same as the flight firmware)
//
// Try it on a fingertip first: rest the finger lightly (pressing hard stops the blood flow) and
// keep still. A person shows 50-100 bpm after ~3 s. A snail shows 5-60 bpm and a much smaller pulse.
//
// How beats are found: the IR level minus its 1 s average = the pulse wave. Blood (or a snail's
// hemolymph) arriving absorbs IR, so the level dips at each beat. A beat is counted when the dip
// passes half of the recent pulse size, and only 0.3-20 s after the last one (3-200 bpm).
// bpm = 60 / the median of the last 5 intervals, so one missed or extra beat doesn't move it much.
// The flight firmware uses its own detector (optical_pulse.h + heartbeat.cpp, 3-150 bpm):
// this sketch is for checking the sensor and finding a good spot and LED current.

// Stop with one clear message if the IDE is set to another chip.
#if !defined(STM32H750xx)
#error "Wrong board. Tools > Board > STM32 MCU based boards > Generic STM32H7 Series, then Tools > Board part number > Generic H750VBTx"
#endif
#include <Arduino.h>
#include <Wire.h>
#include <math.h>
#include <stdarg.h>

// STM32 board package 3.x renamed the UART class from HardwareSerial to Uart.
#if defined(STM32_CORE_VERSION_MAJOR) && (STM32_CORE_VERSION_MAJOR >= 3)
Uart UsbSerial(PA3, PA2);
#else
HardwareSerial UsbSerial(PA3, PA2);
#endif
TwoWire PayWire(PD13, PD12);               // I2C4 payload bus

#define PIN_LED     PE10                   // FCM user LED: flashes on every beat
const uint8_t  MAX_ADDR   = 0x57;
const float    FS_HZ      = 100.0f;        // MAX30100 sample rate (set in maxBegin)
const uint16_t CONTACT_IR = 1000;          // below this almost no light comes back: nothing in front
const uint16_t SAT_IR     = 65000;         // 16-bit ADC near full scale: too much light, LED dimmer
const float    MIN_PULSE  = 20.0f;         // IR counts; a smaller pulse wave counts as no pulse
const uint32_t MIN_IBI_MS = 300;           // 200 bpm
const uint32_t MAX_IBI_MS = 20000;         //   3 bpm

// IR LED current: register 0x09 low 4 bits. Value -> mA x 10.
const uint16_t LED_MA10[16] = {0, 44, 76, 110, 142, 174, 208, 240, 271, 306, 338, 370, 402, 436, 468, 500};
uint8_t ledCode = 8;                       // 27.1 mA, same as the flight firmware

bool plotMode = false;

// ------------------------------ printing ------------------------------
// printf without %f: the IDE's default C library cannot print floats, so values are integers.
static void P(const char *fmt, ...) {
  char b[160];
  va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  UsbSerial.print(b);
}
static const char *x10(uint32_t v) {      // 731 -> "73.1"
  static char b[4][12]; static uint8_t k = 0;
  k = (uint8_t)((k + 1) & 3);
  snprintf(b[k], 12, "%lu.%lu", (unsigned long)(v / 10), (unsigned long)(v % 10));
  return b[k];
}

// ------------------------------ MAX30100 ------------------------------
static bool rdReg(uint8_t reg, uint8_t *b, uint8_t n) {
  PayWire.beginTransmission(MAX_ADDR);
  PayWire.write(reg);
  if (PayWire.endTransmission(false) != 0) return false;
  if (PayWire.requestFrom(MAX_ADDR, n) != n) return false;
  for (uint8_t i = 0; i < n; i++) b[i] = PayWire.read();
  return true;
}
static bool wrReg(uint8_t reg, uint8_t v) {
  PayWire.beginTransmission(MAX_ADDR); PayWire.write(reg); PayWire.write(v);
  return PayWire.endTransmission() == 0;
}
static bool maxClearFifo() { return wrReg(0x02, 0) && wrReg(0x03, 0) && wrReg(0x04, 0); }

// Same settings as the flight firmware: heart-rate mode (IR only), 100 Hz, 1600 us pulses, 16 bit.
static bool maxBegin() {
  uint8_t id = 0;
  if (!rdReg(0xFF, &id, 1)) {
    P("MAX30100: no answer at 0x57. Check SDA -> PD13, SCL -> PD12, VIN -> 3.3 V, GND.\r\n");
    P("          SDA/SCL must idle at 3.0-3.3 V (many modules pull up to 1.8 V only).\r\n");
    return false;
  }
  if (id != 0x11) { P("MAX30100: part id 0x%02X, want 0x11. Another chip at 0x57?\r\n", id); return false; }
  bool ok = wrReg(0x06, 0x40);             // reset
  delay(10);
  ok = ok && maxClearFifo()
          && wrReg(0x07, 0x47)             // SpO2 config: high resolution, 100 Hz, 1600 us
          && wrReg(0x09, ledCode)          // red LED off, IR LED current
          && wrReg(0x06, 0x02);            // mode: heart rate (IR only)
  if (!ok) P("MAX30100: answers but setup failed (I2C errors).\r\n");
  return ok;
}

// ------------------------------ beat finder ------------------------------
struct Pulse {
  float dc = 0, wave = 0, size = 0;        // 1 s average, pulse wave, recent pulse size (counts)
  bool primed = false, armed = false;
  uint32_t t0 = 0, lastBeat = 0;
  uint32_t ibi[5] = {0}; uint8_t nIbi = 0, iIbi = 0;
  uint32_t beats = 0, lastIbi = 0;
  uint16_t ir = 0;
  bool beatNow = false;
};
Pulse pl;
const float A_DC   = 1.0f - expf(-1.0f / (1.0f * FS_HZ));    // 1 s average (removes the DC level)
const float A_WAVE = 1.0f - expf(-1.0f / (0.05f * FS_HZ));   // 50 ms smoothing (removes noise)
const float SIZE_DECAY = expf(-1.0f / (10.0f * FS_HZ));      // pulse size forgets over ~10 s (slow snail beats)

// Start over (sensor moved, samples lost). The beat count is kept.
static void pulseRestart() { pl.primed = false; pl.nIbi = 0; pl.iIbi = 0; pl.lastBeat = 0; }

static uint32_t medianIbi() {
  if (pl.nIbi < 2) return 0;
  uint32_t s[5]; uint8_t n = pl.nIbi;
  for (uint8_t i = 0; i < n; i++) s[i] = pl.ibi[i];
  for (uint8_t i = 1; i < n; i++)
    for (uint8_t j = i; j > 0 && s[j - 1] > s[j]; j--) { uint32_t t = s[j]; s[j] = s[j - 1]; s[j - 1] = t; }
  return s[n / 2];
}
static uint32_t bpmX10() {                 // 0 = no rate yet / pulse lost
  uint32_t m = medianIbi();
  if (!m) return 0;
  return (600000UL + m / 2) / m;
}

// One IR sample at t_ms. Returns true on a beat.
static bool pulseUpdate(uint16_t ir, uint32_t t) {
  pl.ir = ir;
  if (!pl.primed) { pl.dc = ir; pl.wave = 0; pl.size = 0; pl.primed = true; pl.armed = false; pl.t0 = t; }
  float d = (float)ir - pl.dc;
  if (fabsf(d) > 2000.0f + pl.dc / 8.0f) { pulseRestart(); return false; }  // pressed, moved or lifted
  pl.dc += A_DC * d;
  pl.wave += A_WAVE * ((pl.dc - (float)ir) - pl.wave);   // flipped: each beat becomes a peak
  pl.size *= SIZE_DECAY;
  if (fabsf(pl.wave) > pl.size) pl.size = fabsf(pl.wave);
  if (t - pl.t0 < 2000) return false;                    // let the 1 s average settle
  if (pl.lastBeat && t - pl.lastBeat > MAX_IBI_MS) { pl.nIbi = 0; pl.iIbi = 0; pl.lastBeat = 0; }   // rhythm lost
  if (ir < CONTACT_IR || pl.size < MIN_PULSE) return false;

  // Ready for the next beat once the wave falls back under a quarter of the pulse size. (Not "under 0":
  // while the IR level drifts, the 1 s average lags behind and the whole wave sits a bit high.)
  if (pl.wave < 0.25f * pl.size) pl.armed = true;
  if (!pl.armed || pl.wave < 0.5f * pl.size) return false;
  if (pl.lastBeat && t - pl.lastBeat < MIN_IBI_MS) return false;
  pl.armed = false;
  pl.beats++;
  if (pl.lastBeat) {
    pl.lastIbi = t - pl.lastBeat;
    pl.ibi[pl.iIbi] = pl.lastIbi;
    pl.iIbi = (uint8_t)((pl.iIbi + 1) % 5);
    if (pl.nIbi < 5) pl.nIbi++;
  } else {
    pl.lastIbi = 0;
  }
  pl.lastBeat = t;
  return true;
}

// ------------------------------ main ------------------------------
bool maxOk = false;
uint32_t sampleN = 0;                      // sample counter: time base = 10 ms per sample
uint32_t lostCount = 0, ledOffAt = 0;

void setup() {
  pinMode(PIN_LED, OUTPUT);
  UsbSerial.begin(115200);
  PayWire.begin();
  PayWire.setClock(100000);
  for (uint8_t i = 0; i < 20; i++) { digitalWrite(PIN_LED, i & 1); delay(100); }   // 2 s: open the monitor
  digitalWrite(PIN_LED, LOW);
  P("\r\n== MAX30100_Pulse: snail 2 heart rate ==\r\n");
  P("Keys: t text  p plotter  r reset count  + / - IR LED current\r\n");
  maxOk = maxBegin();
  if (maxOk) P("MAX30100 OK, IR LED %s mA. Put a fingertip or the shell on it.\r\n", x10(LED_MA10[ledCode]));
}

static void handleKeys() {
  while (UsbSerial.available()) {
    char c = (char)UsbSerial.read();
    if (c == 't' || c == 'T') plotMode = false;
    else if (c == 'p' || c == 'P') plotMode = true;
    else if (c == 'r' || c == 'R') { pl.beats = 0; pulseRestart(); P("Beat count reset.\r\n"); }
    else if ((c == '+' || c == '=' || c == '-' || c == '_') && maxOk) {
      if ((c == '+' || c == '=') && ledCode < 15) ledCode++;
      if ((c == '-' || c == '_') && ledCode > 1) ledCode--;
      wrReg(0x09, ledCode);
      pulseRestart();
      if (!plotMode) P("IR LED %s mA\r\n", x10(LED_MA10[ledCode]));
    }
  }
}

// Empties the FIFO (16 samples max = 0.16 s, called every 40 ms) and feeds every sample in.
static void drainFifo() {
  uint8_t p[3];                            // 0x02 WR_PTR, 0x03 OVF_COUNTER, 0x04 RD_PTR
  if (!rdReg(0x02, p, 3)) { maxOk = false; P("MAX30100 stopped answering: check the wires.\r\n"); return; }
  if (p[1] & 0x0F) { maxClearFifo(); pulseRestart(); lostCount++; return; }   // full: samples were dropped
  uint8_t n = (uint8_t)((p[0] - p[2]) & 0x0F);
  uint8_t s[28];
  while (n) {                              // bursts of up to 7 samples (28 bytes: the Wire buffer is 32)
    uint8_t k = n > 7 ? 7 : n;
    if (!rdReg(0x05, s, (uint8_t)(4 * k))) { maxOk = false; return; }
    for (uint8_t i = 0; i < k; i++) {
      uint16_t ir = (uint16_t)s[4 * i] << 8 | s[4 * i + 1];
      sampleN++;
      uint32_t t = sampleN * 10;
      bool beat = pulseUpdate(ir, t);
      if (beat) {
        digitalWrite(PIN_LED, HIGH); ledOffAt = millis() + 100;
        if (!plotMode) {
          if (pl.lastIbi) P("BEAT #%lu   interval %s s = %s bpm\r\n", (unsigned long)pl.beats,
                            x10((pl.lastIbi + 50) / 100), x10((600000UL + pl.lastIbi / 2) / pl.lastIbi));
          else P("BEAT #%lu   (first one: the rate comes with the next beats)\r\n", (unsigned long)pl.beats);
        }
      }
      if (plotMode)                        // Arduino IDE: Tools > Serial Plotter
        P("pulse:%d,half_size:%d,beat:%d\r\n", (int)pl.wave, (int)(0.5f * pl.size), beat ? (int)pl.size : 0);
    }
    n = (uint8_t)(n - k);
  }
}

static void statusLine() {
  if (!maxOk) return;
  uint32_t b = bpmX10();
  P("IR %5u | ", pl.ir);
  if (pl.ir < CONTACT_IR) P("nothing in front (IR under %u)", CONTACT_IR);
  else if (pl.ir > SAT_IR) P("TOO MUCH LIGHT: press '-' to dim the LED");
  else if (pl.size < MIN_PULSE) P("pulse %3d (too small, want > %d: move the sensor, keep still)", (int)pl.size, (int)MIN_PULSE);
  else P("pulse %4d", (int)pl.size);
  P(" | %s bpm | beats %lu", b ? x10(b) : "--", (unsigned long)pl.beats);
  if (lostCount) P(" | samples lost %lu", (unsigned long)lostCount);
  P("\r\n");
}

void loop() {
  static uint32_t tFifo = 0, tStatus = 0, tRetry = 0;
  uint32_t now = millis();
  handleKeys();
  if (!maxOk) {
    if (now - tRetry >= 2000) { tRetry = now; maxOk = maxBegin(); if (maxOk) { pulseRestart(); P("MAX30100 OK again.\r\n"); } }
    return;
  }
  if (now - tFifo >= 40) { tFifo = now; drainFifo(); }
  if (!plotMode && now - tStatus >= 1000) { tStatus = now; statusLine(); }
  if (ledOffAt && (int32_t)(now - ledOffAt) >= 0) { digitalWrite(PIN_LED, LOW); ledOffAt = 0; }
}
