// Payload sensors: SHT21, BMP280, MAX30100 (backup) on I2C4 (PD13/PD12), DHT-11 on a GPIO
// Small self-contained drivers so the flight code does not depend on extra libraries.
#pragma once
#include <Arduino.h>
#include <Wire.h>

// ---- SHT21 (GY-21 module), non-blocking: call poll() often; values update ~every 150 ms
class SHT21 {
 public:
  explicit SHT21(TwoWire &w, uint8_t addr = 0x40) : w_(w), addr_(addr) {}
  bool begin();
  void poll(uint32_t now_ms);
  bool ok() const { return ok_; }
  float temperature() const { return t_; }   // C (NAN if never read)
  float humidity() const { return rh_; }     // %RH
  uint16_t errors = 0;
 private:
  bool trigger(uint8_t cmd);
  bool read_raw(uint16_t &raw);
  TwoWire &w_; uint8_t addr_;
  uint8_t state_ = 0; uint32_t t0_ = 0;
  bool ok_ = false; float t_ = NAN, rh_ = NAN;
};

// ---- BMP280 (address 0x77 because SDO -> VCC since 29 Oct; 0x76 with SDO -> GND), normal mode,
// Bosch integer compensation
class BMP280 {
 public:
  explicit BMP280(TwoWire &w, uint8_t addr = 0x77) : w_(w), addr_(addr) {}
  bool begin();
  bool read();                 // updates pressure / temperature
  bool ok() const { return ok_; }
  uint32_t pressure_pa() const { return p_pa_; }
  float temperature() const { return t_c_; }
  uint16_t errors = 0;
 private:
  bool rd(uint8_t reg, uint8_t *buf, uint8_t n);
  bool wr(uint8_t reg, uint8_t v);
  TwoWire &w_; uint8_t addr_;
  bool ok_ = false;
  uint16_t T1_, P1_; int16_t T2_, T3_, P2_, P3_, P4_, P5_, P6_, P7_, P8_, P9_;
  uint32_t p_pa_ = 0; float t_c_ = NAN;
};

// ---- MAX30100 (backup optical pulse sensor) - HR mode, IR only
// NOTE: many GY-MAX30100 boards pull SDA/SCL up to 1.8 V; if the STM32 cannot read it,
// remove the module's 4.7k pull-ups (the other modules already pull up to 3.3 V).
class MAX30100 {
 public:
  explicit MAX30100(TwoWire &w, uint8_t addr = 0x57) : w_(w), addr_(addr) {}
  bool begin();
  bool read();                 // drains FIFO, keeps the newest IR sample
  bool ok() const { return ok_; }
  uint16_t ir() const { return ir_; }
  uint16_t errors = 0;
 private:
  TwoWire &w_; uint8_t addr_;
  bool ok_ = false; uint16_t ir_ = 0;
};

// ---- DHT-11 (temperature + humidity) on one GPIO, added 29 Oct 2026.
// Non-blocking start signal (pin held LOW >= 20 ms across several loop passes), then a ~5 ms
// blocking read of the 40 bits. poll() starts that read only when 'quiet' is true, i.e. the
// 50 Hz sampling interrupt has just run and will not cut into the bit timing.
class DHT11 {
 public:
  DHT11(uint32_t pin, uint32_t period_ms) : pin_(pin), period_(period_ms) {}
  void begin(uint32_t now_ms);
  void poll(uint32_t now_ms, bool quiet);
  bool ok() const { return ok_; }           // false after 3 failed reads in a row
  float temperature() const { return t_; }  // C, last good reading
  float humidity() const { return rh_; }    // %RH, last good reading
  uint16_t errors = 0;
 private:
  int8_t read_bits(uint8_t *bytes);
  uint32_t pin_, period_;
  uint8_t state_ = 0, fails_ = 0;
  uint32_t t0_ = 0;
  bool ok_ = false; float t_ = NAN, rh_ = NAN;
};

uint8_t i2c_read_reg(TwoWire &w, uint8_t addr, uint8_t reg, uint8_t *buf, uint8_t n);
bool    i2c_write_reg(TwoWire &w, uint8_t addr, uint8_t reg, uint8_t v);
