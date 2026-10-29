// DHT-11 decoding (datasheet: Aosong DHT11). Header-only and Arduino-free so it is unit-tested on a PC.
//
// After the start signal the sensor sends 40 bits. Each bit is ~50 us LOW, then HIGH for
// 26-28 us (a 0) or ~70 us (a 1). The 5 bytes are: RH integer, RH decimal, T integer,
// T decimal (bit 7 = negative on newer parts), checksum = low byte of the sum of the first four.
#pragma once
#include <stdint.h>

static const uint8_t DHT11_BIT_THRESHOLD_US = 45;   // between 28 us (0) and 70 us (1)

// high_us[40]: measured HIGH time of each bit, oldest first. Writes 5 bytes.
inline void dht11_bits_to_bytes(const uint8_t *high_us, uint8_t *out) {
  for (uint8_t i = 0; i < 5; i++) out[i] = 0;
  for (uint8_t i = 0; i < 40; i++)
    out[i / 8] = (uint8_t)((out[i / 8] << 1) | (high_us[i] > DHT11_BIT_THRESHOLD_US ? 1 : 0));
}

// Returns false on a checksum error or values a DHT-11 cannot report.
inline bool dht11_decode(const uint8_t *d, float &t_c, float &rh) {
  if ((uint8_t)(d[0] + d[1] + d[2] + d[3]) != d[4]) return false;
  if (d[0] == 0 && d[1] == 0 && d[2] == 0 && d[3] == 0) return false;   // all-zero frame = line stuck low
  float h = d[0] + d[1] * 0.1f;
  float t = d[2] + (d[3] & 0x7F) * 0.1f;
  if (d[3] & 0x80) t = -t;
  if (h > 100.0f || t < -40.0f || t > 80.0f) return false;
  t_c = t; rh = h;
  return true;
}
