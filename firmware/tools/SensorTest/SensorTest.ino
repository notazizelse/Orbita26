// =====================================================================================
//  SensorTest - bench check of every sensor SnailPulse uses. Prints PASS / WARN / FAIL.
//
//  UPLOAD TO: the IntroSat FLIGHT CONTROLLER module (FCM, STM32H750VBT6).
//             Not the SnailPulse perfboard (it has no processor), not the communication
//             board, not the power board.
//
//  Arduino IDE:  Tools > Board > STM32 MCU based boards > Generic STM32H7 Series
//                Board part number: Generic H750VBTx
//                Upload method:     OpenOCD STLink (SWD)  (or STM32CubeProgrammer (SWD) if installed)
//  ST-Link -> FCM "PROG" header: SWCLK->CLK, SWDIO->SWD, GND->GND. BOOT0 jumper on 0.
//  No extra libraries: only Wire, which comes with the STM32 board package.
//
//  Output: FCM USB Type-C (UART2, PA3/PA2), 115200 baud. Serial Monitor, any line ending.
//  Keys:  t = run the full test again   s = I2C scan only
//         l = live values every 2 s (default after the test)
//         p = stream snail 1 (TCRT5000, PB0) + MQ-7 at 50 Hz for the Serial Plotter
//         m = stream snail 2 (MAX30100 IR) at 100 Hz for the Serial Plotter   x = stop printing
//
//  What is tested (wiring: hardware/perfboard/SnailPulse_perfboard_build.html, steps 9-11):
//    SnailPulse payload
//      PB0  SNAIL 1 pulse: TCRT5000 -> amplifier (HB_ADC)   analog
//      PB1  MQ-7 module AO (CO gas)                   analog, module VCC on +5 V, AO through a 10k/20k divider
//      PC6  DHT-11 DATA                               digital, sensor VCC on +5 V (PC6 is 5 V-tolerant)
//      I2C4 PD13 SDA / PD12 SCL:
//           0x57 MAX30100 = SNAIL 2 pulse  0x76 BMP280 (SDO->GND)
//           (0x40 SHT21 / GY-21 removed on 30 Oct: set HAVE_SHT21 1 to test one again)
//           (the second BMP280 at 0x77 was removed on 30 Oct)
//    Flight controller's own sensors, I2C1 PB7 SDA / PB6 SCL (read only)
//           0x6A LSM6DS3   0x1C LIS3MDL   0x77 MS5611   0x4A LM75A
// =====================================================================================
// Stop with one clear message if the IDE is set to another chip (otherwise: dozens of "PA10 not declared").
#if !defined(STM32H750xx)
#error "Wrong board. Tools > Board > STM32 MCU based boards > Generic STM32H7 Series, then Tools > Board part number > Generic H750VBTx"
#endif
#include <Arduino.h>
#include <Wire.h>
#include <stdarg.h>
#include "sensortest_types.h"

// ---------------- change these if you wired the new sensors to other pins ----------------
#define PIN_HB_ADC    PB0      // heartbeat amplifier (J6.1)
#define PIN_MQ7_AO    PB1      // MQ-7 analog out    (J6.2, free since rev C)
#define PIN_DHT       PC6      // DHT-11 data        (J6.3, free since rev C)
#define PIN_LED       PE10     // FCM user LED
#define MQ7_VCC       5.0f     // what the MQ-7 module VCC is wired to (3.3 or 5.0)
#define MQ7_RL_OHM    1000.0f  // MQ-7 module load resistor: "102" on it = 1000, "103" = 10000 (unplug the divider to measure AO<->GND)
#define MQ7_DIV_TOP   10000.0f // divider in the AO wire: AO -> 10k -> PB1 -> 20k -> GND. Needed on 5 V: PB1 is not 5 V-tolerant
#define MQ7_DIV_BOT   20000.0f //   as an analog input. No divider (3.3 V module only): MQ7_DIV_TOP 0
#define HAVE_SHT21    0        // GY-21 (SHT21, 0x40) removed 30 Oct; box air = BMP280 temperature + DHT-11 humidity
#define DHT_VCC_5V    1        // 1 = DHT-11 on +5 V: its pull-up goes to 5 V, so PC6's internal pull-up (to 3.3 V) stays off
#define MIRROR_UART1  0        // 1 = also print on UART1 (PA10/PA9, 115200 8E1): Bluetooth module / comm board
#define ADC_VREF      3.3f
#if DHT_VCC_5V
#define DHT_IDLE      INPUT
#else
#define DHT_IDLE      INPUT_PULLUP
#endif

FcSerial UsbSerial(PA3, PA2);             // FCM USB Type-C
FcSerial Uart1(PA10, PA9);                // only used if MIRROR_UART1 = 1
TwoWire FcWire(PB7, PB6);                 // I2C1 service bus
TwoWire PayWire(PD13, PD12);              // I2C4 payload bus

// ------------------------------ printing ------------------------------
// printf without %f: newlib-nano (the IDE default) cannot print floats, so floats go through ff().
static void P(const char *fmt, ...) {
  char b[200];
  va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  UsbSerial.print(b);
#if MIRROR_UART1
  Uart1.print(b);
#endif
}
static void flushOut() {
  UsbSerial.flush();
#if MIRROR_UART1
  Uart1.flush();
#endif
}
// float -> text with 'dec' decimals. 12 rotating buffers, enough for one line.
static const char *ff(float v, uint8_t dec = 2) {
  static char bufs[12][16]; static uint8_t k = 0;
  char *b = bufs[k]; k = (uint8_t)((k + 1) % 12);
  if (isnan(v) || isinf(v) || fabsf(v) > 1e9f) { strcpy(b, "--"); return b; }
  uint32_t mul = 1;
  for (uint8_t i = 0; i < dec; i++) mul *= 10;
  bool neg = v < 0; if (neg) v = -v;
  uint32_t whole = (uint32_t)v;
  uint32_t frac = (uint32_t)((v - whole) * mul + 0.5f);
  if (frac >= mul) { whole++; frac -= mul; }
  if (dec) snprintf(b, 16, "%s%lu.%0*lu", neg ? "-" : "", (unsigned long)whole, (int)dec, (unsigned long)frac);
  else     snprintf(b, 16, "%s%lu", neg ? "-" : "", (unsigned long)(whole + (frac ? 1 : 0)));
  return b;
}
static const char *sg(float v, uint8_t dec = 2) {   // with sign
  static char bufs[4][18]; static uint8_t k = 0;
  char *b = bufs[k]; k = (uint8_t)((k + 1) % 4);
  snprintf(b, 18, "%s%s", v >= 0 ? "+" : "", ff(v, dec));
  return b;
}

// ------------------------------ result bookkeeping ------------------------------
static Row rows[24];
static uint8_t nRows = 0;
static const char *resName(Res r) { return r == R_PASS ? "PASS" : r == R_WARN ? "WARN" : "FAIL"; }

static void report(const char *name, Res r, const char *fmt, ...) {
  char b[80];
  va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  if (nRows < 24) { rows[nRows].name = name; rows[nRows].r = r; memcpy(rows[nRows].info, b, sizeof b); nRows++; }
  P("  [%s] %-17s %s\r\n", resName(r), name, b);
}
static void hint(const char *s) { P("         -> %s\r\n", s); }

// ------------------------------ helpers ------------------------------
static bool i2cPing(TwoWire &w, uint8_t a) { w.beginTransmission(a); return w.endTransmission() == 0; }

static bool rdReg(TwoWire &w, uint8_t a, uint8_t reg, uint8_t *b, uint8_t n) {    // repeated start
  w.beginTransmission(a);
  w.write(reg);
  if (w.endTransmission(false) != 0) return false;
  if (w.requestFrom((uint8_t)a, (uint8_t)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) b[i] = w.read();
  return true;
}
static bool wrReg(TwoWire &w, uint8_t a, uint8_t reg, uint8_t v) {
  w.beginTransmission(a); w.write(reg); w.write(v);
  return w.endTransmission() == 0;
}
static bool sendCmd(TwoWire &w, uint8_t a, uint8_t c) { w.beginTransmission(a); w.write(c); return w.endTransmission() == 0; }
static bool rdBytes(TwoWire &w, uint8_t a, uint8_t *b, uint8_t n) {
  if (w.requestFrom((uint8_t)a, (uint8_t)n) != n) return false;
  for (uint8_t i = 0; i < n; i++) b[i] = w.read();
  return true;
}

static float adcVolts(uint32_t pin, uint16_t n = 64) {
  uint32_t s = 0;
  for (uint16_t i = 0; i < n; i++) s += analogRead(pin);
  return (s / (float)n) * ADC_VREF / 4095.0f;
}

// Is something driving this analog pin, or is the wire missing?
// Charge the pin through the internal pull-up, read it, then through the pull-down, read again.
// A floating pin keeps that charge (big difference); a driven pin reads the same both times.
static bool pinDriven(uint32_t pin, float &swing) {
  pinMode(pin, INPUT_PULLUP);   delay(3); int up = analogRead(pin);
  pinMode(pin, INPUT_PULLDOWN); delay(3); int dn = analogRead(pin);
  pinMode(pin, INPUT_ANALOG);
  swing = (up - dn) * ADC_VREF / 4095.0f;
  return fabsf(swing) < 0.5f;
}

// ------------------------------ values kept for the live view ------------------------------
static float shtT = NAN, shtRH = NAN, msP = NAN, dhtT = NAN, dhtRH = NAN;
static float bmpT[2] = {NAN, NAN}, bmpP[2] = {NAN, NAN};   // [0] = BMP280 at 0x76 ([1] unused since 30 Oct)
static bool  haveSht = false, haveMax = false, haveMs = false;
static uint8_t bmpAddr[2];
static uint8_t nBmp = 0;

// ================================ I2C scan ================================
static const char *knownName(bool payload, uint8_t a) {
  if (payload) {
    switch (a) {
      case 0x40: return HAVE_SHT21 ? "SHT21 (GY-21)" : "SHT21 (GY-21) - not used by the firmware";
      case 0x57: return "MAX30100 (snail 2 pulse)";
      case 0x76: return "BMP280 (SDO->GND)";
      case 0x77: return "a second BMP280? (not used since 30 Oct)";
    }
  } else {
    switch (a) {
      case 0x1C: case 0x1E: return "LIS3MDL magnetometer";
      case 0x4A: return "LM75A FCM temperature";
      case 0x4C: return "LM75A comm-board temperature";
      case 0x6A: case 0x6B: return "LSM6DS3 accel/gyro";
      case 0x77: return "MS5611 barometer";
    }
  }
  return "unknown";
}

static uint8_t scanBus(TwoWire &w, bool payload, const char *title) {
  P("  %s:", title);
  uint8_t n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    if (i2cPing(w, a)) { P("\r\n     0x%02X  %s", a, knownName(payload, a)); n++; }
  }
  P(n ? "\r\n" : " nothing answers\r\n");
  return n;
}

// Before Wire starts: an idle I2C line must read HIGH (pulled up to 3.3 V by the modules)
static void busIdleCheck() {
  pinMode(PD13, INPUT); pinMode(PD12, INPUT); delay(2);
  int sda = digitalRead(PD13), scl = digitalRead(PD12);
  if (sda && scl) { report("I2C4 idle level", R_PASS, "SDA and SCL idle HIGH (pull-ups present)"); return; }
  report("I2C4 idle level", R_FAIL, "SDA=%s SCL=%s while idle", sda ? "HIGH" : "LOW", scl ? "HIGH" : "LOW");
  hint("LOW = no pull-up, a wire shorted to GND, or only the MAX30100's 1.8 V pull-ups. Measure SDA/SCL: 3.0-3.3 V");
}

// ================================ SHT21 (GY-21) ================================
static uint8_t shtCrc(const uint8_t *d, uint8_t n) {
  uint8_t c = 0;
  for (uint8_t i = 0; i < n; i++) {
    c ^= d[i];
    for (uint8_t b = 0; b < 8; b++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
  }
  return c;
}
static bool shtMeasure(uint8_t c, uint16_t waitMs, uint16_t &raw) {
  if (!sendCmd(PayWire, 0x40, c)) return false;
  delay(waitMs);
  uint8_t b[3];
  if (!rdBytes(PayWire, 0x40, b, 3) || shtCrc(b, 2) != b[2]) return false;
  raw = ((uint16_t)b[0] << 8 | b[1]) & 0xFFFC;
  return true;
}
static bool shtRead() {
  uint16_t rt, rh;
  if (!shtMeasure(0xF3, 90, rt) || !shtMeasure(0xF5, 35, rh)) return false;
  shtT = -46.85f + 175.72f * rt / 65536.0f;
  shtRH = -6.0f + 125.0f * rh / 65536.0f;
  return true;
}
static void testSHT21() {
  haveSht = false;
  if (!HAVE_SHT21) {
    P("  [ -- ] SHT21 0x40        not fitted (HAVE_SHT21 0): box air = BMP280 temperature + DHT-11 humidity\r\n");
    return;
  }
  if (!i2cPing(PayWire, 0x40)) { report("SHT21 0x40", R_FAIL, "no answer"); hint("check GY-21 VIN = 3.3 V, GND, SCL -> PD12, SDA -> PD13"); return; }
  sendCmd(PayWire, 0x40, 0xFE); delay(20);           // soft reset
  if (!shtRead()) { report("SHT21 0x40", R_FAIL, "answers, but the reading or its CRC failed"); return; }
  haveSht = true;
  bool ok = shtT > -40 && shtT < 85 && shtRH >= 0 && shtRH <= 100;
  report("SHT21 0x40", ok ? R_PASS : R_WARN, "T %s C  RH %s %%", ff(shtT), ff(shtRH, 1));
}

// ================================ BMP280 ================================
static BmpCal bcal[2];

static bool bmpBegin(uint8_t a, BmpCal &c, uint8_t &id) {
  id = 0;
  if (!rdReg(PayWire, a, 0xD0, &id, 1)) return false;
  if (id != 0x58 && id != 0x60 && id != 0x56 && id != 0x57) return false;   // BMP280 / BME280 / samples
  wrReg(PayWire, a, 0xE0, 0xB6); delay(5);                                 // soft reset
  uint8_t b[24];
  if (!rdReg(PayWire, a, 0x88, b, 24)) return false;
  c.T1 = b[0] | b[1] << 8;  c.T2 = b[2] | b[3] << 8;  c.T3 = b[4] | b[5] << 8;
  c.P1 = b[6] | b[7] << 8;  c.P2 = b[8] | b[9] << 8;  c.P3 = b[10] | b[11] << 8;
  c.P4 = b[12] | b[13] << 8; c.P5 = b[14] | b[15] << 8; c.P6 = b[16] | b[17] << 8;
  c.P7 = b[18] | b[19] << 8; c.P8 = b[20] | b[21] << 8; c.P9 = b[22] | b[23] << 8;
  // standby 125 ms, IIR x4 ; temp x2, press x16, normal mode (same settings as the flight code)
  return wrReg(PayWire, a, 0xF5, 0x48) && wrReg(PayWire, a, 0xF4, 0x57);
}
static bool bmpRead(uint8_t a, const BmpCal &c, float &tC, float &pHpa) {
  uint8_t d[6];
  if (!rdReg(PayWire, a, 0xF7, d, 6)) return false;
  int32_t adcP = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
  int32_t adcT = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
  if (adcT == 0x80000) return false;                                       // no measurement yet
  // Bosch BMP280 datasheet section 8.2, integer version (same as firmware/SnailPulse_Flight/bmp280_comp.h)
  int32_t v1 = ((((adcT >> 3) - ((int32_t)c.T1 << 1))) * c.T2) >> 11;
  int32_t v2 = (((((adcT >> 4) - (int32_t)c.T1) * ((adcT >> 4) - (int32_t)c.T1)) >> 12) * c.T3) >> 14;
  int32_t tFine = v1 + v2;
  tC = ((tFine * 5 + 128) >> 8) / 100.0f;
  int64_t a1 = (int64_t)tFine - 128000;
  int64_t a2 = a1 * a1 * (int64_t)c.P6;
  a2 += (a1 * (int64_t)c.P5) * 131072;
  a2 += (int64_t)c.P4 * 34359738368LL;
  a1 = ((a1 * a1 * (int64_t)c.P3) >> 8) + ((a1 * (int64_t)c.P2) * 4096);
  a1 = ((((int64_t)1) << 47) + a1) * (int64_t)c.P1 >> 33;
  if (a1 == 0) return false;
  int64_t p = 1048576 - adcP;
  p = (((p << 31) - a2) * 3125) / a1;
  a1 = ((int64_t)c.P9 * (p >> 13) * (p >> 13)) >> 25;
  a2 = ((int64_t)c.P8 * p) >> 19;
  p = ((p + a1 + a2) >> 8) + ((int64_t)c.P7 << 4);
  pHpa = (float)(p / 256) / 100.0f;
  return true;
}
// One BMP280 on I2C4 with SDO -> GND (0x76, the J8 one). The second one (0x77) was removed
// on 30 Oct; set N_BMP to 2 to test it again.
#define N_BMP 1
static void testBMP280() {
  nBmp = 0;
  const uint8_t addrs[2] = {0x76, 0x77};
  const char *names[2] = {"BMP280 0x76", "BMP280 #2 0x77"};
  const char *sdo[2] = {"GND", "VCC"};
  for (uint8_t k = 0; k < N_BMP; k++) {
    uint8_t a = addrs[k];
    bmpT[k] = bmpP[k] = NAN;
    if (!i2cPing(PayWire, a)) {
      report(names[k], R_FAIL, "no answer (the module with SDO -> %s)", sdo[k]);
      if (i2cPing(PayWire, addrs[1 - k])) hint("a BMP280 answers at the other address: its SDO is on the wrong rail");
      else hint("check VCC = 3.3 V, GND, SCL -> PD12, SDA -> PD13, CSB not on GND");
      continue;
    }
    uint8_t id;
    if (!bmpBegin(a, bcal[nBmp], id)) { report(names[k], R_FAIL, "answers, chip id 0x%02X (want 0x58)", id); continue; }
    delay(150);
    float t, p;
    if (!bmpRead(a, bcal[nBmp], t, p)) { report(names[k], R_FAIL, "no valid reading"); continue; }
    bmpAddr[nBmp++] = a;
    bmpT[k] = t; bmpP[k] = p;
    bool ok = p > 300 && p < 1100 && t > -40 && t < 85;
    report(names[k], ok ? R_PASS : R_WARN, "%s hPa  %s C%s", ff(p), ff(t), id == 0x60 ? "  (BME280 chip)" : "");
  }
}

// ================================ MAX30100 ================================
// The FIFO holds 16 samples (0.16 s at 100 Hz) and DROPS new ones when full, so a plain read
// returns an old sample. Clear it, let 2 fresh samples in, then read the newest.
static bool maxReadIr(uint16_t &ir) {
  if (!wrReg(PayWire, 0x57, 0x02, 0) || !wrReg(PayWire, 0x57, 0x03, 0) || !wrReg(PayWire, 0x57, 0x04, 0)) return false;
  delay(25);
  uint8_t wr = 0, s[4];
  if (!rdReg(PayWire, 0x57, 0x02, &wr, 1) || (wr & 0x0F) == 0) return false;
  if (!wrReg(PayWire, 0x57, 0x04, (uint8_t)((wr - 1) & 0x0F))) return false;
  if (!rdReg(PayWire, 0x57, 0x05, s, 4)) return false;
  ir = (uint16_t)s[0] << 8 | s[1];
  return true;
}
// 'm' stream: every IR sample (100 Hz) in Serial Plotter format.
static void maxClear() { wrReg(PayWire, 0x57, 0x02, 0); wrReg(PayWire, 0x57, 0x03, 0); wrReg(PayWire, 0x57, 0x04, 0); }
static void maxStream() {
  if (!haveMax) { P("snail2_max30100_ir:0\r\n"); return; }
  uint8_t p[3];
  if (!rdReg(PayWire, 0x57, 0x02, p, 3)) return;          // WR_PTR, OVF_COUNTER, RD_PTR
  if (p[1]) { maxClear(); return; }                        // overflowed: skip the old samples
  uint8_t n = (uint8_t)((p[0] - p[2]) & 0x0F), s[4];
  while (n--) {
    if (!rdReg(PayWire, 0x57, 0x05, s, 4)) return;
    P("snail2_max30100_ir:%u\r\n", (unsigned)((uint16_t)s[0] << 8 | s[1]));
  }
}

static void testMAX30100() {
  haveMax = false;
  uint8_t id = 0;
  if (!i2cPing(PayWire, 0x57) || !rdReg(PayWire, 0x57, 0xFF, &id, 1)) {
    report("Snail2 MAX30100", R_FAIL, "no answer");
    hint("its board may pull SDA/SCL to 1.8 V: remove its three 4.7k pull-ups (guide, step 11)");
    return;
  }
  if (id != 0x11) { report("Snail2 MAX30100", R_FAIL, "part id 0x%02X (want 0x11)", id); return; }
  bool ok = wrReg(PayWire, 0x57, 0x06, 0x40);                // reset
  delay(10);
  ok = ok && wrReg(PayWire, 0x57, 0x02, 0) && wrReg(PayWire, 0x57, 0x03, 0) && wrReg(PayWire, 0x57, 0x04, 0)
          && wrReg(PayWire, 0x57, 0x07, 0x47)                 // hi-res, 100 Hz, 1600 us
          && wrReg(PayWire, 0x57, 0x09, 0x08)                 // IR 27 mA, red off
          && wrReg(PayWire, 0x57, 0x06, 0x02);                // heart-rate mode (IR only)
  delay(120);
  uint8_t wp = 0;
  rdReg(PayWire, 0x57, 0x02, &wp, 1);
  if (!ok || wp == 0) { report("Snail2 MAX30100", R_FAIL, "id OK but its FIFO does not fill"); return; }
  uint16_t ir = 0;
  maxReadIr(ir);
  haveMax = true;
  report("Snail2 MAX30100", R_PASS, "IR %u (~200 = nothing in front)", ir);
  hint("pulse sensor of snail 2. On a shell or fingertip IR jumps to thousands; key 'm' + Serial Plotter shows the pulse");
}

// ================================ DHT-11 ================================
static int waitWhile(int level, uint32_t timeoutUs) {      // time spent at 'level', or -1 on timeout
  uint32_t t0 = micros();
  while (digitalRead(PIN_DHT) == level) {
    if (micros() - t0 > timeoutUs) return -1;
  }
  return (int)(micros() - t0);
}
// 0 = ok, 1 = no response, 2 = lost in handshake, 3 = lost in data, 4 = checksum
static int dhtRead(float &t, float &rh) {
  uint8_t d[5] = {0, 0, 0, 0, 0};
  flushOut();                                              // no UART interrupts while timing bits
  pinMode(PIN_DHT, OUTPUT); digitalWrite(PIN_DHT, LOW); delay(20);   // start signal >= 18 ms
  pinMode(PIN_DHT, DHT_IDLE);
  int r = 0;
  if (waitWhile(HIGH, 250) < 0) r = 1;                     // sensor pulls low 20-40 us later
  else if (waitWhile(LOW, 150) < 0 || waitWhile(HIGH, 150) < 0) r = 2;   // 80 us low, 80 us high
  else {
    for (uint8_t i = 0; i < 40 && !r; i++) {
      if (waitWhile(LOW, 120) < 0) { r = 3; break; }       // 50 us low before each bit
      int h = waitWhile(HIGH, 150);                        // high 26-28 us = 0, 70 us = 1
      if (h < 0) { r = 3; break; }
      d[i / 8] = (uint8_t)((d[i / 8] << 1) | (h > 45 ? 1 : 0));
    }
  }
  if (!r && (uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) r = 4;
  if (r) return r;
  rh = d[0] + d[1] * 0.1f;
  t = d[2] + (d[3] & 0x7F) * 0.1f;
  if (d[3] & 0x80) t = -t;
  return 0;
}
static void testDHT11() {
  pinMode(PIN_DHT, INPUT); delay(2);
  int idle = digitalRead(PIN_DHT);
  int e = -1;
  for (uint8_t tries = 0; tries < 3 && e != 0; tries++) {
    if (tries) delay(1200);                                // DHT-11: at most one reading per second
    e = dhtRead(dhtT, dhtRH);
  }
  pinMode(PIN_DHT, DHT_IDLE);
  if (e == 0) {
    bool ok = dhtT >= 0 && dhtT <= 50 && dhtRH >= 5 && dhtRH <= 95;
    report("DHT-11 PC6", ok ? R_PASS : R_WARN, "T %s C  RH %s %%", ff(dhtT, 1), ff(dhtRH, 0));
    return;
  }
  dhtT = dhtRH = NAN;
  static const char *why[] = {"", "no response at all", "handshake broke off", "data bits cut off", "checksum wrong"};
  report("DHT-11 PC6", R_FAIL, "%s (DATA idle %s)", why[e], idle ? "HIGH" : "LOW");
  if (!idle) hint("DATA idles LOW: no pull-up (bare 4-pin DHT-11 needs 10k DATA->its VCC), DATA on GND, or no power");
  else if (e == 1) hint(DHT_VCC_5V ? "check DHT VCC = 5 V, GND, and that DATA really goes to PC6" : "check DHT VCC = 3.3 V, GND, and that DATA really goes to PC6");
  else hint("long or noisy wire: keep it under ~1 m, add a 4.7-10k pull-up DATA->its VCC");
}

// ================================ MQ-7 ================================
static_assert(MQ7_VCC < 3.6f || MQ7_DIV_TOP > 0, "MQ-7 on 5 V needs the AO divider (10k AO->PB1, 20k PB1->GND): 5 V on PB1 can damage the STM32");
// PB1 voltage -> AO voltage at the module (undo the divider)
static float mq7Ao(float vPin) { return MQ7_DIV_TOP > 0 ? vPin * (MQ7_DIV_TOP + MQ7_DIV_BOT) / MQ7_DIV_BOT : vPin; }
// Sensor resistance from the AO voltage. The divider sits in parallel with the module's load resistor.
static float mq7Rs(float vAo) {
  float rl = MQ7_DIV_TOP > 0 ? MQ7_RL_OHM * (MQ7_DIV_TOP + MQ7_DIV_BOT) / (MQ7_RL_OHM + MQ7_DIV_TOP + MQ7_DIV_BOT) : MQ7_RL_OHM;
  return vAo > 0.005f ? rl * (MQ7_VCC - vAo) / vAo : NAN;
}
static void testMQ7() {
  float v = adcVolts(PIN_MQ7_AO, 128), ao = mq7Ao(v);
  if (MQ7_DIV_TOP > 0) {
    // the divider's 20k holds PB1 at 0 V when the AO wire is off, so "floating" can't happen here
    if (v < 0.02f) {
      report("MQ-7 AO PB1", R_FAIL, "PB1 at %s V", ff(v, 3));
      hint("AO wire or the 10k divider resistor not reaching PB1, or module not powered (its power LED should be on)");
      return;
    }
  } else {
    float swing;
    if (!pinDriven(PIN_MQ7_AO, swing)) {
      report("MQ-7 AO PB1", R_FAIL, "PB1 is floating (swing %s V)", ff(swing));
      hint("AO wire not reaching PB1, or module not powered (its power LED should be on)");
      return;
    }
  }
  if (v > 3.2f) {
    report("MQ-7 AO PB1", R_WARN, "PB1 %s V: top of the range", ff(v));
    hint(MQ7_VCC > 3.6f ? "gas present, or the divider is missing/wrong: measure PB1, it must stay below 3.3 V!"
                        : "gas present, or AO/VCC swapped");
    return;
  }
  report("MQ-7 AO PB1", R_PASS, "AO %s V (PB1 %s V)  Rs %s kOhm (RL %s)", ff(ao, 3), ff(v, 3), ff(mq7Rs(ao) / 1000.0f, 1), ff(MQ7_RL_OHM, 0));
  if (millis() < 180000UL) hint("heater still warming up: the value drifts for ~3 min (longer on a new sensor)");
  if (MQ7_VCC < 4.0f) hint("on 3.3 V the heater runs at ~45 % power: reacts to gas, but ppm values are not calibrated");
}

// ================================ heartbeat PB0 ================================
static void testHeartbeat() {
  float swing;
  bool driven = pinDriven(PIN_HB_ADC, swing);
  uint16_t mn = 4095, mx = 0;
  uint32_t sum = 0;
  const uint16_t N = 150;                                  // 3 s at 50 Hz
  for (uint16_t i = 0; i < N; i++) {
    uint16_t v = analogRead(PIN_HB_ADC);
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    sum += v;
    delay(20);
  }
  float mean = (sum / (float)N) * ADC_VREF / 4095.0f;
  float pp = (mx - mn) * ADC_VREF / 4095.0f;
  if (!driven) { report("Snail1 TCRT PB0", R_FAIL, "PB0 floating (swing %s V)", ff(swing)); hint("HB_ADC wire (R3/C3 node) not reaching PB0"); return; }
  if (mn < 30 || mx > 4065) {
    report("Snail1 TCRT PB0", R_WARN, "clipping: mean %s V, %u-%u counts", ff(mean), mn, mx);
    hint("amp saturated: wait 10 s for C11 to settle, then check RV3 (SENSE ~1 V) and R16/R17");
    return;
  }
  bool ok = mean > 1.0f && mean < 2.3f;
  report("Snail1 TCRT PB0", ok ? R_PASS : R_WARN, "mean %s V (want 1.2-2.1)  p-p %s mV", ff(mean, 3), ff(pp * 1000, 0));
  hint("pulse sensor of snail 1. TCRT5000 on the shell (or a finger), key 'p', Tools > Serial Plotter: a slow regular wave");
}

// ================================ flight-controller service sensors (I2C1) ================================
static bool msCmdRead(uint8_t c, uint8_t *b, uint8_t n) {  // MS5611: command, STOP, then read
  return sendCmd(FcWire, 0x77, c) && rdBytes(FcWire, 0x77, b, n);
}
static bool msConvert(uint8_t c, uint32_t &v) {
  if (!sendCmd(FcWire, 0x77, c)) return false;
  delay(11);                                               // OSR 4096: 9.04 ms
  uint8_t r[3];
  if (!msCmdRead(0x00, r, 3)) return false;
  v = (uint32_t)r[0] << 16 | (uint32_t)r[1] << 8 | r[2];
  return v != 0;
}
static void testFcSensors() {
  // LSM6DS3: WHO_AM_I 0x0F = 0x69 (LSM6DS3) or 0x6A (LSM6DS3TR-C / LSM6DSL)
  bool found = false;
  const uint8_t lsmA[2] = {0x6A, 0x6B};
  for (uint8_t k = 0; k < 2 && !found; k++) {
    uint8_t a = lsmA[k], who = 0;
    if (!rdReg(FcWire, a, 0x0F, &who, 1) || !(who == 0x69 || who == 0x6A || who == 0x6C)) continue;
    found = true;
    wrReg(FcWire, a, 0x10, 0x40);                          // accel 104 Hz, +-2 g
    delay(60);
    uint8_t b[6]; float g = NAN;
    if (rdReg(FcWire, a, 0x28, b, 6)) {
      float x = (int16_t)(b[1] << 8 | b[0]) * 0.061e-3f, y = (int16_t)(b[3] << 8 | b[2]) * 0.061e-3f, z = (int16_t)(b[5] << 8 | b[4]) * 0.061e-3f;
      g = sqrtf(x * x + y * y + z * z);
    }
    report("FC LSM6DS3", (g > 0.85f && g < 1.15f) ? R_PASS : R_WARN, "0x%02X id 0x%02X  |a| %s g (lying still: 1.00)", a, who, ff(g, 3));
  }
  if (!found) report("FC LSM6DS3", R_FAIL, "no answer at 0x6A / 0x6B");

  // LIS3MDL: WHO_AM_I 0x0F = 0x3D
  found = false;
  const uint8_t lisA[2] = {0x1C, 0x1E};
  for (uint8_t k = 0; k < 2 && !found; k++) {
    uint8_t a = lisA[k], who = 0;
    if (!rdReg(FcWire, a, 0x0F, &who, 1) || who != 0x3D) continue;
    found = true;
    wrReg(FcWire, a, 0x20, 0x70); wrReg(FcWire, a, 0x21, 0x00); wrReg(FcWire, a, 0x23, 0x0C); wrReg(FcWire, a, 0x22, 0x00);
    delay(150);
    uint8_t b[6]; float B = NAN;
    if (rdReg(FcWire, a, 0x28 | 0x80, b, 6)) {             // bit 7 = auto-increment
      float x = (int16_t)(b[1] << 8 | b[0]) / 6842.0f, y = (int16_t)(b[3] << 8 | b[2]) / 6842.0f, z = (int16_t)(b[5] << 8 | b[4]) / 6842.0f;
      B = sqrtf(x * x + y * y + z * z);
    }
    report("FC LIS3MDL", (B > 0.1f && B < 1.5f) ? R_PASS : R_WARN, "0x%02X  |B| %s gauss (Earth ~0.5)", a, ff(B));
  }
  if (!found) report("FC LIS3MDL", R_FAIL, "no answer at 0x1C / 0x1E");

  // LM75A board temperature
  uint8_t b[2];
  if (rdReg(FcWire, 0x4A, 0x00, b, 2)) {
    float t = ((int16_t)(b[0] << 8 | b[1]) >> 5) * 0.125f;
    report("FC LM75A 0x4A", (t > -10 && t < 70) ? R_PASS : R_WARN, "%s C", ff(t));
  } else report("FC LM75A 0x4A", R_FAIL, "no answer");

  // MS5611: PROM CRC + one pressure reading (datasheet formulas incl. 2nd order)
  haveMs = false; msP = NAN;
  if (!sendCmd(FcWire, 0x77, 0x1E)) { report("FC MS5611 0x77", R_FAIL, "no answer"); return; }
  delay(5);
  uint16_t C[8];
  for (uint8_t i = 0; i < 8; i++) {
    uint8_t r[2];
    if (!msCmdRead(0xA0 + 2 * i, r, 2)) { report("FC MS5611 0x77", R_FAIL, "PROM read failed"); return; }
    C[i] = (uint16_t)(r[0] << 8 | r[1]);
  }
  uint16_t rem = 0, crcRead = C[7] & 0x000F, c7 = C[7];
  C[7] &= 0xFF00;
  for (uint8_t cnt = 0; cnt < 16; cnt++) {                 // TE AN520 CRC-4
    rem ^= (cnt & 1) ? (C[cnt >> 1] & 0x00FF) : (C[cnt >> 1] >> 8);
    for (uint8_t bit = 8; bit > 0; bit--) rem = (rem & 0x8000) ? (uint16_t)((rem << 1) ^ 0x3000) : (uint16_t)(rem << 1);
  }
  C[7] = c7;
  if (((rem >> 12) & 0xF) != crcRead) { report("FC MS5611 0x77", R_FAIL, "PROM CRC mismatch"); return; }
  uint32_t D1, D2;
  if (!msConvert(0x48, D1) || !msConvert(0x58, D2)) { report("FC MS5611 0x77", R_FAIL, "conversion failed"); return; }
  int64_t dT = (int64_t)D2 - ((int64_t)C[5] << 8);
  int64_t TEMP = 2000 + (dT * C[6]) / 8388608;
  int64_t OFF = ((int64_t)C[2] << 16) + ((int64_t)C[4] * dT) / 128;
  int64_t SENS = ((int64_t)C[1] << 15) + ((int64_t)C[3] * dT) / 256;
  if (TEMP < 2000) {
    int64_t T2 = (dT * dT) / 2147483648LL, d = TEMP - 2000, OFF2 = 5 * d * d / 2, SENS2 = 5 * d * d / 4;
    if (TEMP < -1500) { int64_t e = TEMP + 1500; OFF2 += 7 * e * e; SENS2 += 11 * e * e / 2; }
    TEMP -= T2; OFF -= OFF2; SENS -= SENS2;
  }
  msP = (float)(((int64_t)D1 * SENS / 2097152 - OFF) / 32768) / 100.0f;
  haveMs = true;
  report("FC MS5611 0x77", (msP > 300 && msP < 1100) ? R_PASS : R_WARN, "%s hPa  %s C", ff(msP), ff(TEMP / 100.0f));
}

// ================================ cross-checks ================================
static void crossChecks() {
  for (uint8_t k = 0; k < N_BMP; k++) {
    if (isnan(bmpP[k]) || !haveMs) continue;
    float d = bmpP[k] - msP;
    report(k ? "BMP #2 vs MS5611" : "BMP280 vs MS5611", fabsf(d) < 3 ? R_PASS : R_WARN, "pressure differs by %s hPa (want < 3)", sg(d));
  }
  float bt = !isnan(bmpT[0]) ? bmpT[0] : bmpT[1];
  if (haveSht && !isnan(bt)) {
    float d = bt - shtT;
    report("BMP280 vs SHT21", fabsf(d) < 3 ? R_PASS : R_WARN, "temperature differs by %s C (want < 3)", sg(d));
  }
  if (!haveSht && !isnan(bt) && !isnan(dhtT)) {            // no SHT21: the two box thermometers check each other
    float d = dhtT - bt;
    report("DHT-11 vs BMP280", fabsf(d) < 3 ? R_PASS : R_WARN, "temperature differs by %s C (want < 3; DHT-11 is +-2 C)", sg(d, 1));
  }
  if (haveSht && !isnan(dhtT)) {
    float dt = dhtT - shtT, dh = dhtRH - shtRH;
    report("DHT-11 vs SHT21", (fabsf(dt) < 3 && fabsf(dh) < 12) ? R_PASS : R_WARN, "dT %s C  dRH %s %% (DHT-11: +-2 C, +-5 %%)", sg(dt, 1), sg(dh, 0));
  }
}

// ================================ full test ================================
static void runAll() {
  nRows = 0;
  P("\r\n================ SnailPulse sensor test ================\r\n");
  P("uptime %lu s   core clock %lu MHz\r\n", (unsigned long)(millis() / 1000), (unsigned long)(SystemCoreClock / 1000000));

  P("\r\n-- I2C buses\r\n");
  PayWire.end();
  busIdleCheck();
  PayWire.begin(); PayWire.setClock(100000);
  scanBus(PayWire, true, "I2C4 payload (PD13/PD12)");
  scanBus(FcWire, false, "I2C1 FC service (PB7/PB6)");

  P("\r\n-- Payload sensors (I2C4)\r\n");
  testSHT21();
  testBMP280();

  P("\r\n-- New sensors on IntroBus_L pins\r\n");
  testDHT11();
  testMQ7();

  P("\r\n-- Pulse sensors: snail 1 = TCRT5000 (PB0), snail 2 = MAX30100 (I2C4 0x57)\r\n");
  testHeartbeat();
  testMAX30100();

  P("\r\n-- Flight controller's own sensors (I2C1)\r\n");
  testFcSensors();

  P("\r\n-- Cross-checks (all sensors in the same room air)\r\n");
  crossChecks();

  uint8_t np = 0, nw = 0, nf = 0;
  for (uint8_t i = 0; i < nRows; i++) { if (rows[i].r == R_PASS) np++; else if (rows[i].r == R_WARN) nw++; else nf++; }
  P("\r\n======================== SUMMARY ========================\r\n");
  for (uint8_t i = 0; i < nRows; i++) P("  %s  %-17s %s\r\n", resName(rows[i].r), rows[i].name, rows[i].info);
  P("  %u PASS, %u WARN, %u FAIL  ->  %s\r\n", np, nw, nf, nf ? "fix the FAIL lines first" : (nw ? "works; read the WARN lines" : "ALL GOOD"));
  P("=========================================================\r\n");
  P("keys: t = test again, s = scan, l = live values, p = snail 1 plot, m = snail 2 plot, x = stop\r\n");
}

// ================================ live view ================================
static Mode viewMode = M_LIVE;

static void liveLine() {
  P("t=%4lus |", (unsigned long)(millis() / 1000));
  if (haveSht && shtRead()) P(" SHT %sC %s%% |", ff(shtT), ff(shtRH, 1));
  for (uint8_t k = 0; k < nBmp; k++) {
    float t, p;
    if (bmpRead(bmpAddr[k], bcal[k], t, p)) P(" BMP%02X %shPa %sC |", bmpAddr[k], ff(p), ff(t));
  }

  float t, rh;
  if (dhtRead(t, rh) == 0) P(" DHT %sC %s%% |", ff(t, 1), ff(rh, 0)); else P(" DHT -- |");
  pinMode(PIN_DHT, DHT_IDLE);
  float v = mq7Ao(adcVolts(PIN_MQ7_AO));
  P(" MQ7 AO %sV Rs %sk |", ff(v, 3), ff(mq7Rs(v) / 1000.0f, 1));
  P(" SNAIL1 TCRT5000 %sV |", ff(adcVolts(PIN_HB_ADC, 16), 3));
  uint16_t ir;
  if (haveMax && maxReadIr(ir)) P(" SNAIL2 MAX30100 IR %u\r\n", ir); else P(" SNAIL2 MAX30100 --\r\n");
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  pinMode(PIN_DHT, DHT_IDLE);            // PC6 was the heater PWM in rev B: this sketch never drives it high
  UsbSerial.begin(115200);
#if MIRROR_UART1
  Uart1.begin(115200, SERIAL_8E1);
#endif
  analogReadResolution(12);
  FcWire.begin(); FcWire.setClock(100000);
  PayWire.begin(); PayWire.setClock(100000);
  for (uint8_t i = 0; i < 20; i++) { digitalWrite(PIN_LED, i & 1); delay(100); }   // 2 s: open the monitor; DHT-11 power-up
  digitalWrite(PIN_LED, LOW);
  runAll();
}

void loop() {
  static uint32_t tLive = 0, tPlot = 0, tLed = 0;
  uint32_t now = millis();

  if (UsbSerial.available()) {
    char c = (char)UsbSerial.read();
    if (c == 't' || c == 'T') runAll();
    else if (c == 's' || c == 'S') { P("\r\n"); scanBus(PayWire, true, "I2C4 payload"); scanBus(FcWire, false, "I2C1 FC service"); }
    else if (c == 'l' || c == 'L') viewMode = M_LIVE;
    else if (c == 'p' || c == 'P') viewMode = M_PLOT;
    else if (c == 'm' || c == 'M') { viewMode = M_PLOT_MAX; maxClear(); }
    else if (c == 'x' || c == 'X') viewMode = M_IDLE;
  }

  if (viewMode == M_LIVE && now - tLive >= 2000) { tLive = now; liveLine(); }
  if (viewMode == M_PLOT && now - tPlot >= 20) {           // 50 Hz, Serial Plotter format
    tPlot = now;
    P("snail1_tcrt5000:%d,mq7_mV:%d\r\n", analogRead(PIN_HB_ADC), (int)(mq7Ao(analogRead(PIN_MQ7_AO) * ADC_VREF / 4095.0f) * 1000.0f));
  }
  if (viewMode == M_PLOT_MAX && now - tPlot >= 40) {        // MAX30100 FIFO: ~4 new samples every 40 ms
    tPlot = now;
    maxStream();
  }
  if (now - tLed >= 500) { tLed = now; digitalWrite(PIN_LED, !digitalRead(PIN_LED)); }   // alive blink
}
