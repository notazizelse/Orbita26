// MAX30100 optical pulse (pure C++, no Arduino: tested in tests/test_host.cpp).
//
// The MAX30100 is the second heartbeat sensor, next to the TCRT5000 analog path (PB0).
// It gives raw 16-bit IR samples at 100 Hz: a large DC level (how much light comes back)
// with a small ripple when blood/hemolymph pulses under it. This wrapper turns that into
// the same kind of signal the TCRT5000 amplifier makes, then uses the same beat detector:
//   - high-pass, tau 1 s (0.16 Hz, like the TCRT5000 amplifier's 1 uF x 1 M): removes the
//     DC level and slow drift (breathing, temperature, the sensor settling).
//   - contact: below CONTACT_IR almost no light comes back = nothing in front of it.
//   - restart on a jump (sensor pressed, moved or lifted) and after lost samples, so the
//     detector does not stay blind for ~20 s or measure a wrong interval.
// Like the TCRT5000 path, rates below ~10 bpm are weakened by the 0.16 Hz high-pass.
#pragma once
#include <stdint.h>
#include <math.h>
#include "heartbeat.h"

class OpticalPulse {
 public:
  static const uint16_t FS_HZ = 100;           // MAX30100 sample rate set in MAX30100::begin()
  static const uint16_t MIN_AMP_IR = 20;       // IR counts; smaller ripple = no pulse
  static const uint16_t SAT_IR = 65000;        // 16-bit ADC near full scale = too much light
  static const uint16_t CONTACT_IR = 1000;     // nothing in front: ~200; finger/shell: thousands
  static const uint16_t MID = 32768;           // the high-passed signal is centred here

  OpticalPulse() : det_(FS_HZ, 10, MIN_AMP_IR, 0, 65535, 8) {
    a_hp_ = 1.0f - expf(-1.0f / (1.0f * FS_HZ));      // tau 1 s
    a_fast_ = 1.0f - expf(-1.0f / (0.05f * FS_HZ));   // tau 50 ms
  }

  // One IR sample taken at t_ms. Returns true when a beat is detected.
  bool update(uint16_t ir, uint32_t t_ms) {
    last_ir_ = ir;
    if (ir > SAT_IR) sat_++;
    if (!primed_) { fast_ = level_ = ir; primed_ = true; }
    fast_ += a_fast_ * ((float)ir - fast_);
    float jump = fast_ - level_;
    if (jump < 0) jump = -jump;
    if (jump > 2000.0f + level_ / 8.0f) {              // pulse ripple is only ~1-2 % of the level
      level_ = fast_;
      det_.restart();
      restarts_++;
    }
    level_ += a_hp_ * ((float)ir - level_);
    float x = (float)MID + ((float)ir - level_);
    if (x < 0) x = 0;
    if (x > 65535) x = 65535;
    return det_.update((uint16_t)x, t_ms);
  }
  void samples_lost() { primed_ = false; det_.restart(); restarts_++; }

  bool contact() const { return primed_ && level_ >= CONTACT_IR; }
  bool saturated() const { return sat_ > 0; }
  void clear_saturation() { sat_ = 0; }
  uint16_t last_ir() const { return last_ir_; }
  uint16_t restarts() const { return restarts_; }
  HeartbeatDetector &det() { return det_; }
  const HeartbeatDetector &det() const { return det_; }

 private:
  HeartbeatDetector det_;
  float a_hp_, a_fast_, fast_ = 0, level_ = 0;
  bool primed_ = false;
  uint16_t last_ir_ = 0, restarts_ = 0, sat_ = 0;
};
