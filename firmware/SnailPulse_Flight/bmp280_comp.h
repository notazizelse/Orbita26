// BMP280 compensation formulas (Bosch BST-BMP280-DS001, section 8.2, integer version)
// Header-only and Arduino-free so it is unit-tested against the datasheet example.
#pragma once
#include <stdint.h>

struct Bmp280Calib {
  uint16_t T1; int16_t T2, T3;
  uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9;
};

// returns temperature in 0.01 C, writes t_fine
inline int32_t bmp280_comp_t(const Bmp280Calib &c, int32_t adc_T, int32_t &t_fine) {
  int32_t var1 = ((((adc_T >> 3) - ((int32_t)c.T1 << 1))) * ((int32_t)c.T2)) >> 11;
  int32_t var2 = (((((adc_T >> 4) - ((int32_t)c.T1)) * ((adc_T >> 4) - ((int32_t)c.T1))) >> 12) *
                  ((int32_t)c.T3)) >> 14;
  t_fine = var1 + var2;
  return (t_fine * 5 + 128) >> 8;
}

// returns pressure in Pa (rounded down)
inline uint32_t bmp280_comp_p(const Bmp280Calib &c, int32_t adc_P, int32_t t_fine) {
  int64_t var1 = ((int64_t)t_fine) - 128000;
  int64_t var2 = var1 * var1 * (int64_t)c.P6;
  var2 = var2 + ((var1 * (int64_t)c.P5) * 131072);           // << 17
  var2 = var2 + (((int64_t)c.P4) * 34359738368LL);           // << 35
  var1 = ((var1 * var1 * (int64_t)c.P3) >> 8) + ((var1 * (int64_t)c.P2) * 4096);  // << 12
  var1 = ((((int64_t)1) << 47) + var1) * ((int64_t)c.P1) >> 33;
  if (var1 == 0) return 0;
  int64_t p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((int64_t)c.P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((int64_t)c.P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((int64_t)c.P7) << 4);
  return (uint32_t)(p >> 8);                                  // Q24.8 -> Pa
}
