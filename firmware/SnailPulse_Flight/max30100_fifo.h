// MAX30100 FIFO helper (Arduino-free, tested in tests/test_host.cpp).
//
// The MAX30100 keeps 16 samples. At 100 Hz it is full after 0.16 s, and when full it DROPS
// new samples (it does not overwrite old ones). So the flight code empties it every 40 ms
// and feeds every sample to the pulse detector (optical_pulse.h).
#pragma once
#include <stdint.h>

// wr_ptr = reg 0x02, ovf = reg 0x03 (OVF_COUNTER), rd_ptr = reg 0x04.
// Returns how many samples are waiting (0..16). lost = true if samples were dropped since the
// last read (the FIFO was full): the stored ones are then old and should be skipped.
inline uint8_t max30100_waiting(uint8_t wr_ptr, uint8_t rd_ptr, uint8_t ovf, bool &lost) {
  wr_ptr &= 0x0F;
  rd_ptr &= 0x0F;
  lost = (ovf & 0x0F) != 0;
  uint8_t n = (uint8_t)((wr_ptr - rd_ptr) & 0x0F);
  if (n == 0 && lost) n = 16;
  return n;
}
