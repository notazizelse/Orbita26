// =====================================================================================
//  SnailPulse - flight firmware for the IntroSat.Platform flight controller (Orbita 2026)
//  Team NazarX / Azizbek N. - https://github.com/notazizelse/Orbita26
//
//  Built on the organizers' platform code:
//    * IntroStratLib (Obu-IntroSat) for the FC service sensors LSM6DS3, LIS3MDL, LM75A
//    * UART1 -> communication board -> LoRa E32-433T30D, 115200 8E1, ACK 0x06 per block
//    * microSD over SDMMC2 with STM32SD (pins from the IntroSat guide)
//  Added by SnailPulse:
//    * heartbeat sampling (PB0, 50 Hz, timer interrupt) + beat detection
//    * box environment: SHT21, BMP280, MAX30100 on I2C4; heater NTC on PB1
//    * heater PWM on PC6 with software limits (hardware LM393 cutoff is separate)
//    * binary packets (protocol.h) that each fit one 58-byte LoRa packet
//
//  Arduino IDE setup: Board "Generic STM32H7 Series", part "STM32H750VBTx",
//  libraries: IntroStratLib, STM32SD (+FatFs). Upload via ST-Link (SWD).
// =====================================================================================
#include <Wire.h>
#include <STM32SD.h>
#include <IWatchdog.h>
#include <LSM6DS3.h>
#include <LIS3MDL.h>
#include <LM75A.h>

#include "config.h"
#include "protocol.h"
#include "radio.h"
#include "heartbeat.h"
#include "thermal.h"
#include "payload_sensors.h"
#include "fc_baro.h"

using namespace IntroStratLib;

// ------------------------------ buses / objects ------------------------------
HardwareSerial RadioSerial(PIN_RADIO_RX, PIN_RADIO_TX);   // UART1 -> comm board
TwoWire FcWire(PIN_I2C1_SDA, PIN_I2C1_SCL);               // I2C1 service bus
TwoWire PayloadWire(PIN_I2C4_SDA, PIN_I2C4_SCL);          // I2C4 payload bus

LSM6DS3 imu(FcWire, ADDR_LSM6DS3);
LIS3MDL mag(FcWire, ADDR_LIS3MDL);
LM75A   fcTemp(FcWire, ADDR_LM75A_FCM);
FcBaro  baro(FcWire, ADDR_MS5611);

SHT21    sht(PayloadWire, ADDR_SHT21);
BMP280   bmp(PayloadWire, ADDR_BMP280);
MAX30100 maxs(PayloadWire, ADDR_MAX30100);

RadioLink<HardwareSerial> radio(RadioSerial, RADIO_ACK_BYTE, RADIO_ACK_TIMEOUT_MS);
HeartbeatDetector hb(HB_SAMPLE_HZ, HB_WAVE_DECIM);
HeaterController heater(BOX_TARGET_C, HEATER_MAX_C);

// ------------------------------ ADC sampling (timer ISR) ------------------------------
// All analogRead() calls happen here so SD writes or I2C delays in loop() cannot disturb
// the 50 Hz heartbeat timing. loop() drains the ring buffer.
static const uint8_t RING = 128;
static volatile uint16_t hb_ring[RING];
static volatile uint32_t hb_ring_t[RING];
static volatile uint8_t  hb_head = 0, hb_tail = 0;
static volatile uint16_t ntc1_raw = 0;
static volatile uint32_t isr_overruns = 0;
static volatile uint8_t  ntc_div = 0;
HardwareTimer *sampleTimer;

void onSampleTick() {
  uint8_t next = (uint8_t)((hb_head + 1) % RING);
  uint16_t v = analogRead(PIN_HB_ADC);
  if (next != hb_tail) { hb_ring[hb_head] = v; hb_ring_t[hb_head] = millis(); hb_head = next; }
  else isr_overruns++;
  if (++ntc_div >= 10) { ntc_div = 0; ntc1_raw = analogRead(PIN_NTC1_ADC); }   // 5 Hz
}

// ------------------------------ SD logging ------------------------------
bool sd_ok = false;
File hbFile, tlmFile;
uint16_t boot_count = 0, sd_errors = 0;
char hbBuf[1536]; size_t hbLen = 0;

void sdBegin() {
  SD.setDx(PC8, PC9, PC10, PC11);
  SD.setCMD(PD2);
  SD.setCK(PC12);
  for (uint8_t i = 0; i < 3 && !sd_ok; i++) { sd_ok = SD.begin(PIN_SD_DETECT); if (!sd_ok) delay(200); }
  if (!sd_ok) return;
  File f = SD.open("BOOT.TXT", FILE_READ);
  if (f) { boot_count = (uint16_t)f.parseInt(); f.close(); }
  boot_count++;
  SD.remove("BOOT.TXT");
  f = SD.open("BOOT.TXT", FILE_WRITE);
  if (f) { f.println(boot_count); f.close(); }
  char name[16];
  snprintf(name, sizeof name, "HB%03u.CSV", boot_count % 1000);
  hbFile = SD.open(name, FILE_WRITE);
  snprintf(name, sizeof name, "TLM%03u.CSV", boot_count % 1000);
  tlmFile = SD.open(name, FILE_WRITE);
  if (hbFile) hbFile.println("t_ms,hb_adc");
  if (tlmFile) tlmFile.println("t_ms,type,fields...");
  if (!hbFile || !tlmFile) { sd_ok = false; sd_errors++; }
}

void sdLogHb(uint32_t t, uint16_t adc) {
  if (!sd_ok) return;
  hbLen += snprintf(hbBuf + hbLen, sizeof(hbBuf) - hbLen, "%lu,%u\n", (unsigned long)t, adc);
  if (hbLen > sizeof(hbBuf) - 24) { if (hbFile.write((uint8_t *)hbBuf, hbLen) != hbLen) sd_errors++; hbLen = 0; }
}

void sdLogLine(const char *line) {
  if (!sd_ok) return;
  if (tlmFile.println(line) == 0) sd_errors++;
}

void sdFlush() {
  if (!sd_ok) return;
  if (hbLen) { hbFile.write((uint8_t *)hbBuf, hbLen); hbLen = 0; }
  hbFile.flush();
  tlmFile.flush();
}

// ------------------------------ state ------------------------------
bool imu_ok = false, mag_ok = false, lm75_ok = false;
float heater_c = NAN;
uint8_t reset_cause = 0;
uint32_t loop_max_ms = 0, ledOffAt = 0;
uint32_t tSnail = 0, tBase = 0, tEnv = 0, tStatus = 0, tSlow = 0, tFlush = 0;

static int16_t clamp16(float v) { return (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v)); }
static int16_t c100(float v) { return isnan(v) ? (int16_t)-32768 : clamp16(v * 100.0f); }   // -32768 = invalid

// ------------------------------ packets ------------------------------
void sendBase(uint32_t now) {
  sp::BasePayload p = {};
  p.t_ms = now;
  p.press_pa = baro.ok() ? (uint32_t)baro.press_pa() : 0;
  p.baro_t_c100 = baro.ok() ? (int16_t)baro.temp_c100() : -32768;
  p.alt_dm = baro.ok() ? (int32_t)(baro.altitude_m() * 10.0f) : 0;
  if (imu_ok) {
    p.acc_mg[0] = clamp16(imu.AX() * 1000); p.acc_mg[1] = clamp16(imu.AY() * 1000); p.acc_mg[2] = clamp16(imu.AZ() * 1000);
    p.gyr_dps10[0] = clamp16(imu.GX() * 10); p.gyr_dps10[1] = clamp16(imu.GY() * 10); p.gyr_dps10[2] = clamp16(imu.GZ() * 10);
  }
  if (mag_ok) { p.mag_mgauss[0] = clamp16(mag.MX() * 1000); p.mag_mgauss[1] = clamp16(mag.MY() * 1000); p.mag_mgauss[2] = clamp16(mag.MZ() * 1000); }
  p.fc_t_x8 = lm75_ok ? (int16_t)fcTemp.GetTemperatureTimes8() : -32768;
  radio.send(sp::PKT_BASE, &p, sizeof p);
  char line[160];
  snprintf(line, sizeof line, "%lu,BASE,%lu,%d,%ld,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
           (unsigned long)now, (unsigned long)p.press_pa, p.baro_t_c100, (long)p.alt_dm,
           p.acc_mg[0], p.acc_mg[1], p.acc_mg[2], p.gyr_dps10[0], p.gyr_dps10[1], p.gyr_dps10[2],
           p.mag_mgauss[0], p.mag_mgauss[1], p.mag_mgauss[2], p.fc_t_x8);
  sdLogLine(line);
}

void sendSnail(uint32_t now) {
  sp::SnailPayload p = {};
  p.t_ms = now;
  p.bpm_x10 = hb.bpm_x10();
  p.last_ibi_ms = hb.last_ibi_ms();
  p.beats_total = hb.beats_total();
  p.quality = hb.quality();
  p.flags = (hb.beat_since_flag() ? 1 : 0) | (hb.saturated() ? 2 : 0) | (hb.no_signal() ? 4 : 0);
  hb.clear_saturation();
  p.amp_counts = hb.amplitude();
  p.dc_counts = hb.dc_level();
  p.wave_n = hb.copy_wave(p.wave, sp::WAVE_N);
  radio.send(sp::PKT_SNAIL, &p, sizeof p);
  char line[96];
  snprintf(line, sizeof line, "%lu,SNAIL,%u,%u,%u,%u,%u,%u,%u", (unsigned long)now, p.bpm_x10, p.last_ibi_ms,
           p.beats_total, p.quality, p.flags, p.amp_counts, p.dc_counts);
  sdLogLine(line);
}

void sendEnv(uint32_t now) {
  sp::EnvPayload p = {};
  p.t_ms = now;
  p.box_t_c100 = sht.ok() ? c100(sht.temperature()) : -32768;
  p.box_rh_c100 = sht.ok() ? (uint16_t)(sht.humidity() * 100) : 0xFFFF;
  p.box_press_pa = bmp.ok() ? bmp.pressure_pa() : 0;
  p.bmp_t_c100 = bmp.ok() ? c100(bmp.temperature()) : -32768;
  p.heater_t_c100 = c100(heater_c);
  p.heater_duty = heater.duty();
  p.heater_mode = heater.mode();
  p.max30100_ir = maxs.ok() ? maxs.ir() : 0;
  p.sensors_ok = (sht.ok() ? 1 : 0) | (bmp.ok() ? 2 : 0) | (maxs.ok() ? 4 : 0) | (!isnan(heater_c) ? 8 : 0) | (sd_ok ? 16 : 0);
  radio.send(sp::PKT_ENV, &p, sizeof p);
  char line[112];
  snprintf(line, sizeof line, "%lu,ENV,%d,%u,%lu,%d,%d,%u,%u,%u,%u", (unsigned long)now, p.box_t_c100, p.box_rh_c100,
           (unsigned long)p.box_press_pa, p.bmp_t_c100, p.heater_t_c100, p.heater_duty, p.heater_mode,
           p.max30100_ir, p.sensors_ok);
  sdLogLine(line);
}

void sendStatus(uint32_t now) {
  sp::StatusPayload p = {};
  p.t_ms = now;
  p.fw_version = SP_FW_VERSION;
  p.boot_count = boot_count;
  p.frames_sent = radio.sent();
  p.acks_missed = radio.missed_acks();
  p.frames_dropped = radio.dropped();
  p.i2c_errors = sht.errors + bmp.errors + maxs.errors + baro.errors;
  p.sd_errors = sd_errors;
  p.loop_max_ms = (uint16_t)(loop_max_ms > 65535 ? 65535 : loop_max_ms);
  p.reset_cause = reset_cause;
  loop_max_ms = 0;
  radio.send(sp::PKT_STATUS, &p, sizeof p);
}

// ------------------------------ setup / loop ------------------------------
void setup() {
  // heater OFF first (R12 already holds the gate low during reset)
  pinMode(PIN_HEATER_PWM, OUTPUT);
  digitalWrite(PIN_HEATER_PWM, LOW);
  analogWriteResolution(8);
  analogWriteFrequency(HEATER_PWM_HZ);
  pinMode(PIN_USER_LED, OUTPUT);

  if (IWatchdog.isReset(true)) reset_cause = 1;

  RadioSerial.begin(RADIO_BAUD, SERIAL_8E1);
  analogReadResolution(ADC_BITS);

  FcWire.begin();
  delay(100);
  imu_ok  = (imu.InitGyro() == 0) & (imu.InitAccel() == 0);   // IntroStratLib returns HAL_OK (0)
  delay(100);
  mag_ok  = (mag.Init() == HAL_OK);
  delay(100);
  lm75_ok = (fcTemp.Init() == HAL_OK);
  baro.begin();

  PayloadWire.begin();
  PayloadWire.setClock(100000);
  sht.begin();
  bmp.begin();
  maxs.begin();      // backup sensor - flight continues without it

  sdBegin();

  sampleTimer = new HardwareTimer(TIM6);
  sampleTimer->setOverflow(HB_SAMPLE_HZ, HERTZ_FORMAT);
  sampleTimer->attachInterrupt(onSampleTick);
  sampleTimer->resume();

  IWatchdog.begin(4000000);   // 4 s: a hung I2C bus or SD card resets the MCU
  sendStatus(millis());
}

void loop() {
  uint32_t t_start = millis();
  uint32_t now = t_start;

  // 1) heartbeat samples from the ISR ring buffer
  while (hb_tail != hb_head) {
    uint16_t v = hb_ring[hb_tail];
    uint32_t t = hb_ring_t[hb_tail];
    hb_tail = (uint8_t)((hb_tail + 1) % RING);
    if (hb.update(v, t)) { digitalWrite(PIN_USER_LED, HIGH); ledOffAt = t + 150; }   // blink on each beat
    sdLogHb(t, v);
  }
  if (ledOffAt && (int32_t)(now - ledOffAt) >= 0) { digitalWrite(PIN_USER_LED, LOW); ledOffAt = 0; }

  // 2) non-blocking sensors
  sht.poll(now);
  baro.poll(now);

  // 3) once a second: slow sensors + heater control
  if (now - tSlow >= 1000) {
    float dt = (tSlow == 0) ? 1.0f : (now - tSlow) / 1000.0f;
    tSlow = now;
    heater_c = ntc_celsius(ntc1_raw, (1u << ADC_BITS) - 1, NTC_R_FIXED, NTC_R25, NTC_BETA);
    if (!bmp.ok() && bmp.errors < 100) bmp.begin(); else if (bmp.ok()) bmp.read();
    if (maxs.ok()) maxs.read();
    float box = sht.ok() ? sht.temperature() : NAN;
    analogWrite(PIN_HEATER_PWM, heater.update(box, heater_c, dt));
  }

  // 4) telemetry schedule
  if (now - tSnail  >= PERIOD_SNAIL_MS)  { tSnail = now;  sendSnail(now); }
  if (now - tBase   >= PERIOD_BASE_MS)   { tBase = now;   sendBase(now); }
  if (now - tEnv    >= PERIOD_ENV_MS)    { tEnv = now;    sendEnv(now); }
  if (now - tStatus >= PERIOD_STATUS_MS) { tStatus = now; sendStatus(now); }
  radio.poll(now);

  // 5) SD flush once a second
  if (now - tFlush >= 1000) { tFlush = now; sdFlush(); }

  IWatchdog.reload();
  uint32_t dt = millis() - t_start;
  if (dt > loop_max_ms) loop_max_ms = dt;
}
