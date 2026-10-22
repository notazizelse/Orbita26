#include "protocol.h"
#include <string.h>

namespace sp {

uint16_t crc16_ccitt(const uint8_t *data, size_t len) {
  uint16_t crc = 0xFFFF;
  while (len--) {
    crc ^= (uint16_t)(*data++) << 8;
    for (uint8_t i = 0; i < 8; i++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

size_t build_frame(uint8_t type, uint8_t seq, const void *payload, size_t len, uint8_t *out) {
  if (len > MAX_PAYLOAD) return 0;
  out[0] = SYNC0; out[1] = SYNC1;
  out[2] = type; out[3] = seq; out[4] = (uint8_t)len;
  memcpy(out + 5, payload, len);
  uint16_t crc = crc16_ccitt(out + 2, len + 3);
  out[5 + len] = (uint8_t)(crc & 0xFF);
  out[6 + len] = (uint8_t)(crc >> 8);
  return len + FRAME_OVERHEAD;
}

bool FrameParser::feed(uint8_t b) {
  switch (state_) {
    case 0: if (b == SYNC0) state_ = 1; return false;
    case 1: state_ = (b == SYNC1) ? 2 : (b == SYNC0 ? 1 : 0); return false;
    case 2: buf_[0] = b; state_ = 3; return false;                 // type
    case 3: buf_[1] = b; state_ = 4; return false;                 // seq
    case 4:                                                        // len
      if (b > MAX_PAYLOAD) { state_ = 0; return false; }
      buf_[2] = b; idx_ = 0; state_ = b ? 5 : 6; return false;
    case 5: buf_[3 + idx_++] = b; if (idx_ == buf_[2]) state_ = 6; return false;
    case 6: crc_lo_ = b; state_ = 7; return false;
    case 7: {
      state_ = 0;
      uint16_t got = (uint16_t)crc_lo_ | ((uint16_t)b << 8);
      if (got != crc16_ccitt(buf_, buf_[2] + 3)) { crc_errors++; return false; }
      type = buf_[0]; seq = buf_[1]; len = buf_[2];
      memcpy(payload, buf_ + 3, len);
      return true;
    }
  }
  state_ = 0;
  return false;
}

}  // namespace sp
