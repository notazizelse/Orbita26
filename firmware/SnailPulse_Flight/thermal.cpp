#include "thermal.h"
#include <math.h>

float ntc_celsius(uint16_t adc, uint16_t adc_max, float r_fixed, float r25, float beta) {
  // open thermistor -> node pulled to 3V3; shorted -> node at 0 V
  if (adc >= adc_max - 8 || adc <= 8) return NAN;
  float ratio = (float)adc / (float)adc_max;           // = R_ntc / (R_fixed + R_ntc)
  float r = r_fixed * ratio / (1.0f - ratio);
  float inv_t = 1.0f / 298.15f + logf(r / r25) / beta;  // Beta equation
  return 1.0f / inv_t - 273.15f;
}

static float clampf(float x, float lo, float hi) { return x < lo ? lo : (x > hi ? hi : x); }

uint8_t HeaterController::update(float box_c, float heater_c, float dt_s) {
  const float KP = 40.0f;      // duty counts per C of error
  const float KI = 0.5f;       // duty counts per C per second
  bool box_ok = !isnan(box_c), heat_ok = !isnan(heater_c);
  float out;

  if (box_ok) {
    mode_ = 1;
    float err = target_ - box_c;
    integ_ = clampf(integ_ + KI * err * dt_s, 0.0f, 255.0f);   // anti-windup
    out = KP * err + integ_;
  } else if (heat_ok) {
    mode_ = 2;
    integ_ = 0;
    out = KP * ((hmax_ - 10.0f) - heater_c);   // hold the plate ~10 C below its limit
  } else {
    mode_ = 3;
    integ_ = 0;
    out = 51;                                  // 20 % blind heating
  }
  out = clampf(out, 0.0f, 255.0f);

  // surface limit: taper over the last 3 C, off above the limit
  if (heat_ok) {
    if (heater_c >= hmax_) { out = 0; mode_ = 0; }
    else if (heater_c > hmax_ - 3.0f) out *= (hmax_ - heater_c) / 3.0f;
  } else if (out > 128.0f) {
    out = 128.0f;            // no surface sensor: never more than 50 % (~2.2 W)
  }
  duty_ = (uint8_t)(out + 0.5f);
  return duty_;
}
