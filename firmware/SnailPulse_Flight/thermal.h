// Thermistor maths + heater controller (pure C++, unit-tested on PC)
#pragma once
#include <stdint.h>

// NTC on the bottom of a divider: 3V3 - R_fixed - node - NTC - GND.  ADC is ratiometric
// (same 3.3 V feeds the divider and the ADC), so supply drift cancels.
// Returns temperature in C, or NAN when the reading is open/short (sensor fault).
float ntc_celsius(uint16_t adc, uint16_t adc_max, float r_fixed, float r25, float beta);

// Box heater controller:
//  - mode 1: PI on box air temperature (SHT21), heater surface limited by NTC1
//  - mode 2: SHT21 failed -> keep heater surface (NTC1) at a moderate temperature
//  - mode 3: both failed -> fixed 20 % duty (~0.9 W), hardware LM393 cutoff still active
//  - mode 0: off (heater too hot)
class HeaterController {
 public:
  HeaterController(float box_target_c, float heater_max_c)
      : target_(box_target_c), hmax_(heater_max_c) {}
  // dt_s: time since last call. box_c / heater_c may be NAN (sensor fault).
  uint8_t update(float box_c, float heater_c, float dt_s);
  uint8_t duty() const { return duty_; }
  uint8_t mode() const { return mode_; }
 private:
  float target_, hmax_;
  float integ_ = 0;
  uint8_t duty_ = 0, mode_ = 0;
};
