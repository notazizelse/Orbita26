// Snail heartbeat detector (pure C++, no Arduino dependency -> unit-tested on PC)
//
// Input : HB_ADC samples at HB_SAMPLE_HZ (already band-passed 0.16-3.4 Hz in hardware,
//         centred on VREF = 1.65 V ~ 2048 counts)
// Output: beats/min, last inter-beat interval, signal quality, 10 Hz waveform for telemetry
//
// Snail (Cornu aspersum) hearts beat slowly: tens of beats/min, much lower when cold,
// dormant or dehydrated, ~100 in young snails when warm. The detector accepts intervals
// from 0.4 s (150 bpm) to 20 s (3 bpm).
#pragma once
#include <stdint.h>

class HeartbeatDetector {
 public:
  explicit HeartbeatDetector(uint16_t fs_hz = 50, uint8_t wave_decim = 5);

  // Feed one sample. t_ms = sample time. Returns true when a beat is detected.
  bool update(uint16_t adc, uint32_t t_ms);

  uint16_t bpm_x10() const;            // 0 = no valid rate
  uint16_t last_ibi_ms() const { return last_ibi_; }
  uint16_t beats_total() const { return beats_; }
  uint8_t  quality() const;            // 0..100
  bool     saturated() const { return sat_count_ > 0; }
  bool     no_signal() const;
  uint16_t median_ibi() const;         // 0 if fewer than 2 intervals
  uint16_t amplitude() const { return (uint16_t)(2.0f * env_); }   // ~peak-to-peak counts
  uint16_t dc_level() const { return (uint16_t)baseline_; }

  // Waveform ring (10 Hz). Copies up to n newest samples, oldest first. Returns count.
  uint8_t  copy_wave(uint8_t *dst, uint8_t n) const;
  bool     beat_since_flag();          // true if a beat happened since the last call
  void     clear_saturation() { sat_count_ = 0; }

  static const uint32_t MIN_IBI_MS = 400;   // 150 bpm
  static const uint32_t MAX_IBI_MS = 20000;
  static const uint16_t MIN_AMP = 6;   // counts; below this the sensor sees no pulse

 private:
  float fs_, a_lp_, a_base_, env_decay_;
  float lp_ = 2048, baseline_ = 2048, env_ = 0;
  bool armed_ = true, primed_ = false, beat_flag_ = false;
  uint32_t last_beat_ms_ = 0, now_ms_ = 0;
  uint16_t last_ibi_ = 0, beats_ = 0, sat_count_ = 0;
  uint32_t n_samples_ = 0;
  static const uint8_t NIBI = 5;
  uint16_t ibi_[NIBI] = {0};
  uint8_t  ibi_n_ = 0, ibi_i_ = 0;
  // waveform decimation
  uint8_t decim_, dcount_ = 0;
  uint32_t dsum_ = 0;
  static const uint8_t WN = 64;
  uint8_t wave_[WN];
  uint8_t w_head_ = 0, w_count_ = 0;
};
