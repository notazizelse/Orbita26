#include "payload_sensors.h"
#include "bmp280_comp.h"
#include "max30100_fifo.h"

uint8_t i2c_read_reg(TwoWire &w, uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n) {
  w.beginTransmission(addr);
  w.write(reg);
  if (w.endTransmission(false) != 0) return 0;
  uint8_t got = w.requestFrom(addr, n);
  for (uint8_t i = 0; i < got; i++) buf[i] = w.read();
  return got;
}

bool i2c_write_reg(TwoWire &w, uint8_t addr, uint8_t reg, uint8_t v) {
  w.beginTransmission(addr);
  w.write(reg);
  w.write(v);
  return w.endTransmission() == 0;
}

// ============================ SHT21 ============================
static uint8_t sht_crc(const uint8_t *d, uint8_t n) {     // poly x^8+x^5+x^4+1 (0x31), init 0
  uint8_t crc = 0;
  for (uint8_t i = 0; i < n; i++) {
    crc ^= d[i];
    for (uint8_t b = 0; b < 8; b++) crc = (crc & 0x80) ? (uint8_t)((crc << 1) ^ 0x31) : (uint8_t)(crc << 1);
  }
  return crc;
}

bool SHT21::trigger(uint8_t cmd) {
  w_.beginTransmission(addr_);
  w_.write(cmd);
  return w_.endTransmission() == 0;
}

bool SHT21::read_raw(uint16_t &raw) {
  uint8_t b[3];
  if (w_.requestFrom(addr_, (uint8_t)3) != 3) return false;   // NACK = still measuring
  for (uint8_t i = 0; i < 3; i++) b[i] = w_.read();
  if (sht_crc(b, 2) != b[2]) return false;
  raw = ((uint16_t)b[0] << 8 | b[1]) & 0xFFFC;                 // clear status bits
  return true;
}

bool SHT21::begin() {
  ok_ = trigger(0xFE);          // soft reset
  delay(20);
  state_ = 0;
  return ok_;
}

void SHT21::poll(uint32_t now) {
  uint16_t raw;
  switch (state_) {
    case 0:  // start temperature (no-hold master, 14 bit, max 85 ms)
      if (trigger(0xF3)) { t0_ = now; state_ = 1; } else { ok_ = false; errors++; t0_ = now; state_ = 9; }
      break;
    case 1:
      if (now - t0_ < 90) break;
      if (read_raw(raw)) { t_ = -46.85f + 175.72f * raw / 65536.0f; state_ = 2; }
      else { ok_ = false; errors++; t0_ = now; state_ = 9; }
      break;
    case 2:  // start humidity (12 bit, max 29 ms)
      if (trigger(0xF5)) { t0_ = now; state_ = 3; } else { ok_ = false; errors++; t0_ = now; state_ = 9; }
      break;
    case 3:
      if (now - t0_ < 35) break;
      if (read_raw(raw)) { rh_ = -6.0f + 125.0f * raw / 65536.0f; ok_ = true; t0_ = now; state_ = 4; }
      else { ok_ = false; errors++; t0_ = now; state_ = 9; }
      break;
    case 4:  // rest so the sensor does not self-heat
      if (now - t0_ >= 500) state_ = 0;
      break;
    case 9:  // error back-off
      if (now - t0_ >= 1000) state_ = 0;
      break;
  }
}

// ============================ BMP280 ============================
bool BMP280::rd(uint8_t reg, uint8_t *buf, uint8_t n) { return i2c_read_reg(w_, addr_, reg, buf, n) == n; }
bool BMP280::wr(uint8_t reg, uint8_t v) { return i2c_write_reg(w_, addr_, reg, v); }

bool BMP280::begin() {
  uint8_t id = 0;
  ok_ = false;
  if (!rd(0xD0, &id, 1) || id != 0x58) { errors++; return false; }
  uint8_t c[24];
  if (!rd(0x88, c, 24)) { errors++; return false; }
  T1_ = c[0] | c[1] << 8;  T2_ = c[2] | c[3] << 8;  T3_ = c[4] | c[5] << 8;
  P1_ = c[6] | c[7] << 8;  P2_ = c[8] | c[9] << 8;  P3_ = c[10] | c[11] << 8;
  P4_ = c[12] | c[13] << 8; P5_ = c[14] | c[15] << 8; P6_ = c[16] | c[17] << 8;
  P7_ = c[18] | c[19] << 8; P8_ = c[20] | c[21] << 8; P9_ = c[22] | c[23] << 8;
  // config: standby 125 ms, IIR filter x4 ; ctrl_meas: temp x2, press x16, normal mode
  ok_ = wr(0xF5, 0x48) && wr(0xF4, 0x57);
  if (!ok_) errors++;
  return ok_;
}

bool BMP280::read() {
  uint8_t d[6];
  if (!rd(0xF7, d, 6)) { ok_ = false; errors++; return false; }
  int32_t adc_P = ((int32_t)d[0] << 12) | ((int32_t)d[1] << 4) | (d[2] >> 4);
  int32_t adc_T = ((int32_t)d[3] << 12) | ((int32_t)d[4] << 4) | (d[5] >> 4);
  Bmp280Calib c = {T1_, T2_, T3_, P1_, P2_, P3_, P4_, P5_, P6_, P7_, P8_, P9_};
  int32_t t_fine;
  t_c_ = bmp280_comp_t(c, adc_T, t_fine) / 100.0f;
  p_pa_ = bmp280_comp_p(c, adc_P, t_fine);
  ok_ = true;
  return true;
}

// ============================ MAX30100 ============================
bool MAX30100::begin() {
  uint8_t id = 0;
  ok_ = false;
  if (i2c_read_reg(w_, addr_, 0xFF, &id, 1) != 1 || id != 0x11) { errors++; return false; }
  ok_ = i2c_write_reg(w_, addr_, 0x06, 0x40)            // reset
        && (delay(10), true)
        && i2c_write_reg(w_, addr_, 0x07, 0x47)         // SpO2 cfg: hi-res, 100 Hz, 1600 us pulse
        && i2c_write_reg(w_, addr_, 0x09, 0x08)         // LED: IR 27.1 mA, red off
        && i2c_write_reg(w_, addr_, 0x06, 0x02);        // mode: heart-rate only (IR)
  if (!ok_) errors++;
  return ok_;
}

// Empties the FIFO: every sample since the last call, oldest first, up to 16 (0.16 s).
// Called every MAX30100_POLL_MS (40 ms), so normally 4 samples. If it was full anyway
// (loop was late), the stored samples are old: clear the FIFO and report lost = true.
uint8_t MAX30100::drain(uint16_t *ir, bool &lost) {
  uint8_t p[3];                                         // 0x02 WR_PTR, 0x03 OVF_COUNTER, 0x04 RD_PTR
  lost = false;
  if (i2c_read_reg(w_, addr_, 0x02, p, 3) != 3) { ok_ = false; errors++; return 0; }
  uint8_t n = max30100_waiting(p[0], p[2], p[1], lost);
  if (lost) {
    if (!i2c_write_reg(w_, addr_, 0x02, 0) || !i2c_write_reg(w_, addr_, 0x03, 0) || !i2c_write_reg(w_, addr_, 0x04, 0)) {
      ok_ = false; errors++;
    }
    return 0;
  }
  uint8_t got = 0, s[28];
  while (got < n) {                                     // burst reads of up to 7 samples (28 B)
    uint8_t k = (uint8_t)(n - got > 7 ? 7 : n - got);
    if (i2c_read_reg(w_, addr_, 0x05, s, (uint8_t)(4 * k)) != 4 * k) { ok_ = false; errors++; break; }
    for (uint8_t i = 0; i < k; i++) ir[got + i] = (uint16_t)s[4 * i] << 8 | s[4 * i + 1];
    got = (uint8_t)(got + k);
  }
  if (got) ir_ = ir[got - 1];
  return got;
}

// ============================ DHT-11 ============================
#include "dht11_decode.h"

// Time spent at 'level' in microseconds, or -1 if it lasts longer than timeout_us.
static int32_t dht_wait(uint32_t pin, int level, uint32_t timeout_us) {
  uint32_t t0 = micros();
  while (digitalRead(pin) == level) {
    if (micros() - t0 > timeout_us) return -1;
  }
  return (int32_t)(micros() - t0);
}

void DHT11::begin(uint32_t now) {
  pinMode(pin_, INPUT_PULLUP);   // idle HIGH; the module's own pull-up does the real work
  state_ = 0;
  t0_ = now;                     // first reading one period after boot (sensor needs >= 1 s)
}

// 0 = ok, 1 = no response, 2 = lost in the handshake, 3 = lost in the data bits
int8_t DHT11::read_bits(uint8_t *bytes) {
  uint8_t hi[40];
  pinMode(pin_, INPUT_PULLUP);                                    // end of the start signal
  if (dht_wait(pin_, HIGH, 250) < 0) return 1;                    // sensor answers after 20-40 us
  if (dht_wait(pin_, LOW, 150) < 0 || dht_wait(pin_, HIGH, 150) < 0) return 2;   // 80 us + 80 us
  for (uint8_t i = 0; i < 40; i++) {
    if (dht_wait(pin_, LOW, 120) < 0) return 3;                   // 50 us before every bit
    int32_t h = dht_wait(pin_, HIGH, 150);                        // 26-28 us = 0, 70 us = 1
    if (h < 0) return 3;
    hi[i] = (uint8_t)(h > 255 ? 255 : h);
  }
  dht11_bits_to_bytes(hi, bytes);
  return 0;
}

void DHT11::poll(uint32_t now, bool quiet) {
  if (state_ == 0) {                                              // idle: time for a new reading?
    if (now - t0_ < period_) return;
    pinMode(pin_, OUTPUT);
    digitalWrite(pin_, LOW);                                      // start signal (>= 18 ms)
    t0_ = now; state_ = 1;
    return;
  }
  // start signal running. Read once it has lasted 20 ms and the sampling ISR just ran;
  // after 60 ms read anyway so a slow loop pass can't hold the sensor low forever.
  if (now - t0_ < 20) return;
  if (!quiet && now - t0_ < 60) return;
  uint8_t d[5];
  float t, rh;
  bool good = read_bits(d) == 0 && dht11_decode(d, t, rh);
  pinMode(pin_, INPUT_PULLUP);
  state_ = 0; t0_ = now;
  if (good) { t_ = t; rh_ = rh; ok_ = true; fails_ = 0; }
  else { errors++; if (++fails_ >= 3) ok_ = false; }
}
