#include "heartbeat.h"
#include <math.h>

HeartbeatDetector::HeartbeatDetector(uint16_t fs_hz, uint8_t wave_decim)
    : fs_(fs_hz), decim_(wave_decim) {
  a_lp_      = 1.0f - expf(-1.0f / (0.08f * fs_));   // smoothing, tau 80 ms
  a_base_    = 1.0f - expf(-1.0f / (6.0f * fs_));    // baseline, tau 6 s
  env_decay_ = expf(-1.0f / (6.0f * fs_));           // envelope release, tau 6 s
}

bool HeartbeatDetector::update(uint16_t adc, uint32_t t_ms) {
  now_ms_ = t_ms;
  n_samples_++;
  if (adc < 40 || adc > 4055) sat_count_++;

  // --- waveform for telemetry: average of `decim_` samples, 8-bit ---
  dsum_ += adc;
  if (++dcount_ >= decim_) {
    wave_[w_head_] = (uint8_t)((dsum_ / dcount_) >> 4);
    w_head_ = (uint8_t)((w_head_ + 1) % WN);
    if (w_count_ < WN) w_count_++;
    dsum_ = 0; dcount_ = 0;
  }

  // --- filters ---
  if (!primed_) { lp_ = baseline_ = adc; primed_ = true; }
  lp_       += a_lp_ * ((float)adc - lp_);
  baseline_ += a_base_ * (lp_ - baseline_);
  float d = lp_ - baseline_;
  float ad = d < 0 ? -d : d;
  env_ = (ad > env_) ? ad : env_ * env_decay_;

  // need ~2 s of data before trusting the envelope
  if (n_samples_ < (uint32_t)(2 * fs_)) return false;

  // --- beat = upward crossing of +40 % of envelope, re-armed below 10 % ---
  float thr = 0.4f * env_;
  bool beat = false;
  if (env_ >= MIN_AMP) {
    if (armed_ && d > thr) {
      uint32_t ibi = t_ms - last_beat_ms_;
      // refractory: never faster than 150 bpm, and (once the rhythm is known) ignore
      // bumps earlier than 40 % of the usual interval - stops double-counting slow beats,
      // but still follows a sudden doubling of the rate (0.5 > 0.4)
      uint32_t refr = MIN_IBI_MS, med = median_ibi();
      if (med && 4 * med / 10 > refr) refr = 4 * med / 10;
      if (last_beat_ms_ == 0 || ibi >= refr) {
        if (last_beat_ms_ != 0 && ibi <= MAX_IBI_MS) {
          last_ibi_ = (uint16_t)ibi;
          ibi_[ibi_i_] = (uint16_t)ibi;
          ibi_i_ = (uint8_t)((ibi_i_ + 1) % NIBI);
          if (ibi_n_ < NIBI) ibi_n_++;
        }
        last_beat_ms_ = t_ms ? t_ms : 1;
        beats_++;
        beat = true; beat_flag_ = true;
        armed_ = false;
      }
    } else if (!armed_ && d < 0.1f * env_) {
      armed_ = true;
    }
  }
  return beat;
}

bool HeartbeatDetector::no_signal() const { return env_ < MIN_AMP; }

uint16_t HeartbeatDetector::median_ibi() const {
  if (ibi_n_ < 2) return 0;
  // median of the stored intervals (robust to one missed or extra beat)
  uint16_t s[NIBI];
  for (uint8_t i = 0; i < ibi_n_; i++) s[i] = ibi_[i];
  for (uint8_t i = 1; i < ibi_n_; i++)
    for (uint8_t j = i; j > 0 && s[j - 1] > s[j]; j--) { uint16_t t = s[j]; s[j] = s[j - 1]; s[j - 1] = t; }
  return s[ibi_n_ / 2];
}

uint16_t HeartbeatDetector::bpm_x10() const {
  if (no_signal()) return 0;
  if (now_ms_ - last_beat_ms_ > MAX_IBI_MS) return 0;   // heart stopped / lost contact
  uint16_t med = median_ibi();
  return med ? (uint16_t)(600000UL / med) : 0;
}

uint8_t HeartbeatDetector::quality() const {
  if (no_signal() || bpm_x10() == 0) return 0;
  // regularity of intervals: coefficient of variation
  float m = 0, v = 0;
  for (uint8_t i = 0; i < ibi_n_; i++) m += ibi_[i];
  m /= ibi_n_;
  for (uint8_t i = 0; i < ibi_n_; i++) v += (ibi_[i] - m) * (ibi_[i] - m);
  float cv = sqrtf(v / ibi_n_) / m;
  float q = 100.0f * (1.0f - 2.0f * cv);             // cv 0 -> 100, cv 0.5 -> 0
  if (env_ < 3 * MIN_AMP) q *= env_ / (3.0f * MIN_AMP);   // weak signal penalty
  if (sat_count_) q *= 0.5f;
  if (q < 0) q = 0;
  if (q > 100) q = 100;
  return (uint8_t)q;
}

uint8_t HeartbeatDetector::copy_wave(uint8_t *dst, uint8_t n) const {
  if (n > w_count_) n = w_count_;
  for (uint8_t i = 0; i < n; i++)
    dst[i] = wave_[(uint8_t)((w_head_ + WN - n + i) % WN)];
  return n;
}

bool HeartbeatDetector::beat_since_flag() {
  bool b = beat_flag_; beat_flag_ = false; return b;
}
