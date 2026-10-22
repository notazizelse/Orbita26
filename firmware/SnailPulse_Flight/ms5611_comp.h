// MS5611 compensation (TE datasheet MS5611-01BA03, first + second order) and ISA altitude.
// Why not IntroStratLib's MS5611::GetPressure()? In v0.1.2 the second-order correction
// mixes units (deg C vs 0.01 deg C) and the sign of T2, so below 20 C the temperature is
// wrong by tens to hundreds of degrees and pressure by ~1 %. The stratosphere is cold, so
// we use the datasheet formulas directly. Header-only, unit-tested on PC.
#pragma once
#include <stdint.h>
#include <math.h>

struct Ms5611Prom { uint16_t C[7]; };   // C[1]..C[6] used

// temp_c100: 0.01 C, press_pa100: 0.01 mbar (= Pa)
inline void ms5611_compensate(const Ms5611Prom &p, uint32_t D1, uint32_t D2,
                              int32_t &temp_c100, int32_t &press_pa) {
  int64_t dT   = (int64_t)D2 - ((int64_t)p.C[5] << 8);
  int64_t TEMP = 2000 + (dT * p.C[6]) / 8388608;                       // 2^23
  int64_t OFF  = ((int64_t)p.C[2] << 16) + ((int64_t)p.C[4] * dT) / 128;
  int64_t SENS = ((int64_t)p.C[1] << 15) + ((int64_t)p.C[3] * dT) / 256;
  if (TEMP < 2000) {                                                    // second order
    int64_t T2 = (dT * dT) / 2147483648LL;                              // 2^31
    int64_t d = TEMP - 2000;
    int64_t OFF2 = 5 * d * d / 2, SENS2 = 5 * d * d / 4;
    if (TEMP < -1500) {
      int64_t e = TEMP + 1500;
      OFF2 += 7 * e * e; SENS2 += 11 * e * e / 2;
    }
    TEMP -= T2; OFF -= OFF2; SENS -= SENS2;
  }
  temp_c100 = (int32_t)TEMP;
  press_pa  = (int32_t)((((int64_t)D1 * SENS) / 2097152 - OFF) / 32768);   // 2^21, 2^15
}

// International Standard Atmosphere altitude from pressure, valid 0-32 km.
// (The simple 44330*(1-(p/p0)^0.1903) formula only holds in the troposphere: at 30 mbar it
//  says 21.7 km instead of ~23.9 km.)
inline float isa_altitude_m(float p_pa) {
  if (p_pa <= 0) return NAN;
  if (p_pa > 22632.1f)                       // 0-11 km, lapse -6.5 K/km
    return 44330.8f * (1.0f - powf(p_pa / 101325.0f, 0.190263f));
  if (p_pa > 5474.89f)                       // 11-20 km, isothermal 216.65 K
    return 11000.0f - 6341.62f * logf(p_pa / 22632.1f);
  // 20-32 km, lapse +1 K/km
  return 20000.0f + 216650.0f * (powf(p_pa / 5474.89f, -1.0f / 34.1632f) - 1.0f);
}
