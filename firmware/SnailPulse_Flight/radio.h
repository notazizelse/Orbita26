// ACK-gated frame queue for the IntroSat communication board.
// The comm board buffers up to 4096 bytes and answers 0x06 (ACK) after each block has
// been transmitted over LoRa. We send ONE frame (<= 58 bytes = one LoRa packet), wait for
// the ACK (or a timeout), then send the next. Template so it works with HardwareSerial on
// the STM32 and with a fake stream in the PC unit tests.
#pragma once
#include <stdint.h>
#include <string.h>
#include "protocol.h"

template <class StreamT>
class RadioLink {
 public:
  RadioLink(StreamT &s, uint8_t ack_byte, uint32_t ack_timeout_ms)
      : s_(s), ack_(ack_byte), timeout_(ack_timeout_ms) {}

  // Queue a payload as a frame. Returns false (and counts a drop) if it replaced the oldest.
  bool send(uint8_t type, const void *payload, uint8_t len) {
    bool dropped = false;
    if (count_ == QN) { tail_ = (uint8_t)((tail_ + 1) % QN); count_--; dropped_++; dropped = true; }
    Slot &sl = q_[head_];
    sl.len = (uint8_t)sp::build_frame(type, seq_++, payload, len, sl.data);
    head_ = (uint8_t)((head_ + 1) % QN); count_++;
    return !dropped;
  }

  // Call every loop.
  void poll(uint32_t now_ms) {
    while (s_.available() > 0) {
      int b = s_.read();
      if (b == ack_ && waiting_) { waiting_ = false; }
      // other bytes = uplink data from the ground station (not used in this mission)
    }
    if (waiting_ && (now_ms - sent_at_) > timeout_) { waiting_ = false; missed_++; }
    if (!waiting_ && count_) {
      Slot &sl = q_[tail_];
      s_.write(sl.data, sl.len);
      tail_ = (uint8_t)((tail_ + 1) % QN); count_--;
      waiting_ = true; sent_at_ = now_ms; sent_++;
    }
  }

  uint16_t sent() const { return sent_; }
  uint16_t missed_acks() const { return missed_; }
  uint16_t dropped() const { return dropped_; }
  uint8_t  queued() const { return count_; }

 private:
  static const uint8_t QN = 8;
  struct Slot { uint8_t data[sp::LORA_PACKET]; uint8_t len; };
  StreamT &s_;
  uint8_t ack_; uint32_t timeout_;
  Slot q_[QN];
  uint8_t head_ = 0, tail_ = 0, count_ = 0, seq_ = 0;
  bool waiting_ = false; uint32_t sent_at_ = 0;
  uint16_t sent_ = 0, missed_ = 0, dropped_ = 0;
};
