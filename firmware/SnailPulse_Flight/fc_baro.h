// Non-blocking MS5611 driver for the FCM barometer (I2C1, 0x77). Replaces the blocking
// IntroStratLib::MS5611 (80 ms of HAL_Delay per reading + wrong cold compensation).
#pragma once
#include <Arduino.h>
#include <Wire.h>
#include "ms5611_comp.h"

class FcBaro {
 public:
  explicit FcBaro(TwoWire &w, uint8_t addr = 0x77) : w_(w), addr_(addr) {}

  bool begin() {
    ok_ = cmd(0x1E);                       // reset
    delay(5);
    for (uint8_t i = 1; i <= 6 && ok_; i++) {
      w_.beginTransmission(addr_); w_.write((uint8_t)(0xA0 + 2 * i));
      if (w_.endTransmission() != 0 || w_.requestFrom(addr_, (uint8_t)2) != 2) { ok_ = false; break; }
      prom_.C[i] = (uint16_t)w_.read() << 8; prom_.C[i] |= w_.read();
    }
    if (!ok_) errors++;
    state_ = 0;
    return ok_;
  }

  // call every loop; a full T+P cycle takes ~22 ms (OSR 4096) and repeats every 250 ms
  void poll(uint32_t now) {
    uint32_t v;
    switch (state_) {
      case 0: if (now - t0_ >= 250) { if (cmd(0x58)) { t0_ = now; state_ = 1; } else fail(now); } break;  // D2
      case 1: if (now - t0_ >= 11) { if (adc(v)) { D2_ = v; state_ = 2; } else fail(now); } break;
      case 2: if (cmd(0x48)) { t0_ = now; state_ = 3; } else fail(now); break;                            // D1
      case 3: if (now - t0_ >= 11) {
                if (adc(v) && v && D2_) {
                  int32_t t, p; ms5611_compensate(prom_, v, D2_, t, p);
                  temp_c100_ = t; press_pa_ = p; ok_ = true; fresh_ = true;
                } else { fail(now); break; }
                t0_ = now; state_ = 0;
              } break;
    }
  }

  bool ok() const { return ok_; }
  int32_t temp_c100() const { return temp_c100_; }
  int32_t press_pa() const { return press_pa_; }
  float altitude_m() const { return isa_altitude_m((float)press_pa_); }
  uint16_t errors = 0;

 private:
  bool cmd(uint8_t c) { w_.beginTransmission(addr_); w_.write(c); return w_.endTransmission() == 0; }
  bool adc(uint32_t &v) {
    if (!cmd(0x00) || w_.requestFrom(addr_, (uint8_t)3) != 3) return false;
    v = (uint32_t)w_.read() << 16; v |= (uint32_t)w_.read() << 8; v |= w_.read();
    return true;
  }
  void fail(uint32_t now) { ok_ = false; errors++; t0_ = now; state_ = 0; }
  TwoWire &w_; uint8_t addr_;
  Ms5611Prom prom_ = {{0}};
  uint8_t state_ = 0; uint32_t t0_ = 0, D2_ = 0;
  bool ok_ = false, fresh_ = false;
  int32_t temp_c100_ = 0, press_pa_ = 0;
};
