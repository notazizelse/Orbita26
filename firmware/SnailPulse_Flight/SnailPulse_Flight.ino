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
//    * two snails: snail 1 on the TCRT5000 (PB0, 50 Hz, timer interrupt), snail 2 on the MAX30100
//      (I2C4, 100 Hz); each has its own beat detector, both sent in the SNAIL packet
//    * box environment: BMP280 (0x76) on I2C4; DHT-11 on PC6; MQ-7 on PB1 (SHT21 only if SP_HAS_SHT21)
//    * (rev B only, SP_HEATER_ENABLED 1: heater NTC on PB1 + heater PWM on PC6)
//    * binary packets (protocol.h) that each fit one 58-byte LoRa packet
//
//  Arduino IDE setup: Board "Generic STM32H7 Series", Board part number "Generic H750VBTx",
//  STM32 board package 2.12.0, libraries: IntroStratLib, STM32SD (+FatFs). Upload via ST-Link (SWD).
// =====================================================================================
// Stop with one clear message if the IDE is set to another chip (otherwise: dozens of "PA10 not declared").
#if !defined(STM32H750xx)
#error "Wrong board. Tools > Board > STM32 MCU based boards > Generic STM32H7 Series, then Tools > Board part number > Generic H750VBTx"
#endif
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
#include "mq7_scale.h"
#include "optical_pulse.h"
#include "thermal.h"
#include "payload_sensors.h"
#include "fc_baro.h"
#include "sp_hooks.h"

using namespace IntroStratLib;

// ------------------------------ buses / objects ------------------------------
// STM32 board package 3.x renamed the UART class from HardwareSerial to Uart.
#if defined(STM32_CORE_VERSION_MAJOR) && (STM32_CORE_VERSION_MAJOR >= 3)
typedef Uart FcSerial;
#else
typedef HardwareSerial FcSerial;
#endif
FcSerial RadioSerial(PIN_RADIO_RX, PIN_RADIO_TX);         // UART1 -> comm board
#if SP_USB_MIRROR
FcSerial UsbSerial(PA3, PA2);                             // UART2 -> CH340X -> FC USB-C
#endif
TwoWire FcWire(PIN_I2C1_SDA, PIN_I2C1_SCL);               // I2C1 service bus
TwoWire PayloadWire(PIN_I2C4_SDA, PIN_I2C4_SCL);          // I2C4 payload bus

LSM6DS3 imu(FcWire, ADDR_LSM6DS3);
LIS3MDL mag(FcWire, ADDR_LIS3MDL);
LM75A   fcTemp(FcWire, ADDR_LM75A_FCM);
FcBaro  baro(FcWire, ADDR_MS5611);

SHT21    sht(PayloadWire, ADDR_SHT21);
BMP280   bmp(PayloadWire, ADDR_BMP280);      // SDO -> GND
#if SP_HAS_BMP280_2
BMP280   bmp2(PayloadWire, ADDR_BMP280_B);   // second one, SDO -> VCC
#endif
MAX30100 maxs(PayloadWire, ADDR_MAX30100);   // second pulse sensor (optical)
DHT11    dht(PIN_DHT11, DHT11_PERIOD_MS, !DHT11_VCC_5V);   // 5 V module: no internal pull-up on PC6

RadioLink<FcSerial> radio(RadioSerial, RADIO_ACK_BYTE, RADIO_ACK_TIMEOUT_MS);
HeartbeatDetector hb(HB_SAMPLE_HZ, HB_WAVE_DECIM);   // pulse sensor 1: TCRT5000 -> amplifier -> PB0
OpticalPulse      opt;                               // pulse sensor 2: MAX30100 IR, 100 Hz
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
static volatile uint32_t mq7_sum = 0;          // MQ-7 raw ADC, summed at 5 Hz, emptied once a second
static volatile uint16_t mq7_n = 0;
static volatile uint32_t last_tick_us = 0;     // when the ISR last ran (DHT-11 reads right after it)
HardwareTimer *sampleTimer;

void onSampleTick() {
  uint8_t next = (uint8_t)((hb_head + 1) % RING);
  uint16_t v = analogRead(PIN_HB_ADC);
  if (next != hb_tail) { hb_ring[hb_head] = v; hb_ring_t[hb_head] = millis(); hb_head = next; }
  else isr_overruns++;
#if SP_HEATER_ENABLED
  if (++ntc_div >= 10) { ntc_div = 0; ntc1_raw = analogRead(PIN_NTC1_ADC); }   // 5 Hz
#elif SP_HAS_MQ7
  if (++ntc_div >= 10) { ntc_div = 0; mq7_sum += analogRead(PIN_MQ7_AO); mq7_n++; }   // 5 Hz
#endif
  last_tick_us = micros();
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
  if (tlmFile) tlmFile.println("t_ms,type,fields...  (ENV adds dht_t_c100,dht_rh_c100,mq7_mv,box2_press_pa,bmp2_t_c100 since fw 1.2; SNAIL adds snail 2 / MAX30100: bpm_x10,beats,quality,flags,amp,ir since fw 1.3)");
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
uint16_t mq7_mv = 0xFFFF;      // 1 s average of MQ-7 AO at the module, mV (0xFFFF = disabled / no data yet)
uint16_t mq7_pin_mv = 0;       // the same, measured at PB1 (after the divider)
uint8_t reset_cause = 0;
uint32_t loop_max_ms = 0, ledOffAt = 0;
uint32_t tSnail = 0, tBase = 0, tEnv = 0, tStatus = 0, tSlow = 0, tFlush = 0, tMax = 0, tMaxRetry = 0;

static int16_t clamp16(float v) { return (int16_t)(v > 32767 ? 32767 : (v < -32768 ? -32768 : v)); }
static int16_t c100(float v) { return isnan(v) ? (int16_t)-32768 : clamp16(v * 100.0f); }   // -32768 = invalid

// ------------------------------ packets ------------------------------
// Every frame goes out through here: comm board queue (UART1), USB-C copy, and the hook
// for merged code (sp_hooks.h).
void sendFrame(uint8_t type, const void *payload, uint8_t len) {
  const uint8_t *f; uint8_t n;
#if SP_OWN_RADIO
  radio.send(type, payload, len);
  f = radio.last_frame(n);
#else
  static uint8_t seq = 0;
  static uint8_t buf[sp::LORA_PACKET];
  n = (uint8_t)sp::build_frame(type, seq++, payload, len, buf);
  f = buf;
#endif
#if SP_USB_MIRROR
  UsbSerial.write(f, n);
#endif
  sp_frame_out(f, n);
}

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
  sendFrame(sp::PKT_BASE, &p, sizeof p);
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
  // pulse sensor 2: MAX30100 (optical)
  HeartbeatDetector &m = opt.det();
  bool max_ok = maxs.ok();
  p.max_bpm_x10 = (max_ok && opt.contact()) ? m.bpm_x10() : 0;
  p.max_beats_total = m.beats_total();
  p.max_quality = (max_ok && opt.contact()) ? m.quality() : 0;
  p.max_flags = (m.beat_since_flag() ? 1 : 0) | (opt.saturated() ? 2 : 0) | (m.no_signal() ? 4 : 0)
              | (max_ok ? 0 : 8) | (opt.contact() ? 0 : 16);
  opt.clear_saturation();
  p.max_amp = m.amplitude();
  p.max_ir = max_ok ? opt.last_ir() : 0;
  sendFrame(sp::PKT_SNAIL, &p, sizeof p);
  char line[128];
  snprintf(line, sizeof line, "%lu,SNAIL,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u", (unsigned long)now, p.bpm_x10,
           p.last_ibi_ms, p.beats_total, p.quality, p.flags, p.amp_counts, p.dc_counts,
           p.max_bpm_x10, p.max_beats_total, p.max_quality, p.max_flags, p.max_amp, p.max_ir);
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
  p.dht_t_c100 = dht.ok() ? c100(dht.temperature()) : -32768;
  p.dht_rh_c100 = dht.ok() ? (uint16_t)(dht.humidity() * 100) : 0xFFFF;
  p.mq7_mv = mq7_mv;
#if SP_HAS_BMP280_2
  bool bmp2_ok = bmp2.ok();
  p.box2_press_pa = bmp2_ok ? bmp2.pressure_pa() : 0;
  p.bmp2_t_c100 = bmp2_ok ? c100(bmp2.temperature()) : -32768;
#else
  bool bmp2_ok = false;                // removed 30 Oct: fields stay in the packet as "invalid"
  p.box2_press_pa = 0;
  p.bmp2_t_c100 = -32768;
#endif
  bool mq7_ok = mq7_mv != 0xFFFF && mq7_pb1_ok(mq7_pin_mv);   // PB1 at 0 V = wire off, at 3.3 V = off the scale
  p.sensors_ok = (sht.ok() ? 1 : 0) | (bmp.ok() ? 2 : 0) | (maxs.ok() ? 4 : 0) | (!isnan(heater_c) ? 8 : 0) | (sd_ok ? 16 : 0)
               | (dht.ok() ? 32 : 0) | (mq7_ok ? 64 : 0) | (bmp2_ok ? 128 : 0);
  sendFrame(sp::PKT_ENV, &p, sizeof p);
  char line[160];
  snprintf(line, sizeof line, "%lu,ENV,%d,%u,%lu,%d,%d,%u,%u,%u,%u,%d,%u,%u,%lu,%d", (unsigned long)now, p.box_t_c100, p.box_rh_c100,
           (unsigned long)p.box_press_pa, p.bmp_t_c100, p.heater_t_c100, p.heater_duty, p.heater_mode,
           p.max30100_ir, p.sensors_ok, p.dht_t_c100, p.dht_rh_c100, p.mq7_mv,
           (unsigned long)p.box2_press_pa, p.bmp2_t_c100);
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
  p.i2c_errors = sht.errors + bmp.errors + maxs.errors + baro.errors;   // DHT-11 health: ENV sensors_ok bit 5
  p.sd_errors = sd_errors;
  p.loop_max_ms = (uint16_t)(loop_max_ms > 65535 ? 65535 : loop_max_ms);
  p.reset_cause = reset_cause;
  p.fc_ok = (imu_ok ? 1 : 0) | (mag_ok ? 2 : 0) | (lm75_ok ? 4 : 0) | (baro.ok() ? 8 : 0);
  loop_max_ms = 0;
  sendFrame(sp::PKT_STATUS, &p, sizeof p);
}

// ------------------------------ setup / loop ------------------------------
void setup() {
  // heater OFF first (R12 already holds the gate low during reset)
#if SP_HEATER_ENABLED
  pinMode(PIN_HEATER_PWM, OUTPUT);
  digitalWrite(PIN_HEATER_PWM, LOW);
  analogWriteResolution(8);
  analogWriteFrequency(HEATER_PWM_HZ);
#endif
  pinMode(PIN_USER_LED, OUTPUT);

  if (IWatchdog.isReset(true)) reset_cause = 1;

#if SP_OWN_RADIO
  RadioSerial.begin(RADIO_BAUD, SERIAL_8E1);
#endif
#if SP_USB_MIRROR
  UsbSerial.begin(USB_BAUD);
#endif
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
#if SP_HAS_SHT21
  sht.begin();                         // never started: sht.ok() stays false, ENV sends invalid values
#endif
  bmp.begin();
#if SP_HAS_BMP280_2
  bmp2.begin();
#endif
  maxs.begin();      // second pulse sensor; retried every 5 s if it does not answer
#if SP_HAS_DHT11
  dht.begin(millis());
#endif

  sdBegin();

  sampleTimer = new HardwareTimer(TIM6);
  sampleTimer->setOverflow(HB_SAMPLE_HZ, HERTZ_FORMAT);
  sampleTimer->attachInterrupt(onSampleTick);
  sampleTimer->resume();

  sp_ext_setup();   // merged code (sp_hooks.h); runs before the watchdog starts

#ifdef DBGMCU_APB4FZ1_DBG_IWDG1
  // Pause the watchdog while a debugger has the core halted (no effect in flight),
  // so stopping at a breakpoint doesn't reset the board after 4 s.
  DBGMCU->APB4FZ1 |= DBGMCU_APB4FZ1_DBG_IWDG1;
#endif
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

  // 2) pulse sensor 2: MAX30100. Empty its FIFO (~4 samples) and feed every sample to the
  //    detector. Sample times are 10 ms apart, the newest is "now". Re-init if it stopped answering.
  if (now - tMax >= MAX30100_POLL_MS) {
    tMax = now;
    if (maxs.ok()) {
      uint16_t ir[16];
      bool lost = false;
      uint8_t n = maxs.drain(ir, lost);
      if (lost) opt.samples_lost();
      for (uint8_t i = 0; i < n; i++) opt.update(ir[i], now - (uint32_t)(n - 1 - i) * (1000 / OpticalPulse::FS_HZ));
    } else if (now - tMaxRetry >= MAX30100_RETRY_MS) {
      tMaxRetry = now;
      if (maxs.begin()) opt.samples_lost();
    }
  }

  // 3) non-blocking sensors
#if SP_HAS_SHT21
  sht.poll(now);
#endif
#if SP_HAS_DHT11
  dht.poll(now, micros() - last_tick_us < 4000);   // >= 16 ms until the next 50 Hz tick
#endif
  baro.poll(now);

  // 4) once a second: slow sensors + heater control
  if (now - tSlow >= 1000) {
    float dt = (tSlow == 0) ? 1.0f : (now - tSlow) / 1000.0f;
    tSlow = now;
#if SP_HEATER_ENABLED
    heater_c = ntc_celsius(ntc1_raw, (1u << ADC_BITS) - 1, NTC_R_FIXED, NTC_R25, NTC_BETA);
#endif
    if (!bmp.ok() && bmp.errors < 100) bmp.begin(); else if (bmp.ok()) bmp.read();
#if SP_HAS_BMP280_2
    if (!bmp2.ok() && bmp2.errors < 100) bmp2.begin(); else if (bmp2.ok()) bmp2.read();
#endif
#if SP_HEATER_ENABLED
    float box = sht.ok() ? sht.temperature() : NAN;
    analogWrite(PIN_HEATER_PWM, heater.update(box, heater_c, dt));
#else
    (void)dt;   // no heater on rev C: heater_c stays NAN, duty and mode stay 0
#endif
#if SP_HAS_MQ7
    noInterrupts();
    uint32_t s = mq7_sum; uint16_t n = mq7_n; mq7_sum = 0; mq7_n = 0;
    interrupts();
    if (n) {
      mq7_pin_mv = (uint16_t)mq7_pb1_mv(s / n, (1u << ADC_BITS) - 1, 3300);
      mq7_mv = mq7_ao_mv(mq7_pin_mv, MQ7_DIV_TOP_OHM, MQ7_DIV_BOT_OHM);
    }
#endif
  }

  // 5) telemetry schedule
  if (now - tSnail  >= PERIOD_SNAIL_MS)  { tSnail = now;  sendSnail(now); }
  if (now - tBase   >= PERIOD_BASE_MS)   { tBase = now;   sendBase(now); }
  if (now - tEnv    >= PERIOD_ENV_MS)    { tEnv = now;    sendEnv(now); }
  if (now - tStatus >= PERIOD_STATUS_MS) { tStatus = now; sendStatus(now); }
#if SP_OWN_RADIO
  radio.poll(now);
#endif
  sp_ext_loop(now);   // merged code (sp_hooks.h), must not block

  // 6) SD flush once a second
  if (now - tFlush >= 1000) { tFlush = now; sdFlush(); }

  IWatchdog.reload();
  uint32_t dt = millis() - t_start;
  if (dt > loop_max_ms) loop_max_ms = dt;
}
