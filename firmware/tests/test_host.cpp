// PC unit tests for the Arduino-independent parts of SnailPulse_Flight.
// Build (from firmware/tests):
//   g++ -std=c++17 -O1 -Wall -I../SnailPulse_Flight test_host.cpp ../SnailPulse_Flight/protocol.cpp
//       ../SnailPulse_Flight/heartbeat.cpp ../SnailPulse_Flight/thermal.cpp -o test_host && ./test_host
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <cstring>
#include <vector>
#include <random>
#include "protocol.h"
#include "heartbeat.h"
#include "thermal.h"
#include "bmp280_comp.h"
#include "ms5611_comp.h"
#include "radio.h"
#include "dht11_decode.h"
#include "max30100_fifo.h"
#include "optical_pulse.h"
#include "mq7_scale.h"

static int fails = 0;
#define CHECK(cond, ...) do { if (!(cond)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

// ---------------------------------------------------------------- protocol
static void test_protocol() {
  // CRC-16/CCITT-FALSE check value
  CHECK(sp::crc16_ccitt((const uint8_t *)"123456789", 9) == 0x29B1, "crc check value");
  sp::SnailPayload p = {};
  p.t_ms = 123456; p.bpm_x10 = 312; p.quality = 88; p.wave_n = 20;
  for (int i = 0; i < 20; i++) p.wave[i] = (uint8_t)(100 + i);
  uint8_t f[64];
  size_t n = sp::build_frame(sp::PKT_SNAIL, 7, &p, sizeof p, f);
  CHECK(n == sizeof p + 7 && n <= sp::LORA_PACKET, "frame len %zu", n);
  sp::FrameParser par;
  // garbage + frame + corrupted frame + frame
  std::vector<uint8_t> s = {0x00, 0xAA, 0x12, 0x55};
  s.insert(s.end(), f, f + n);
  std::vector<uint8_t> bad(f, f + n); bad[10] ^= 0xFF;
  s.insert(s.end(), bad.begin(), bad.end());
  s.insert(s.end(), f, f + n);
  int got = 0;
  for (uint8_t b : s) if (par.feed(b)) {
    got++;
    sp::SnailPayload q; memcpy(&q, par.payload, sizeof q);
    CHECK(par.type == sp::PKT_SNAIL && par.seq == 7 && q.bpm_x10 == 312 && q.wave[19] == 119, "decode");
  }
  CHECK(got == 2 && par.crc_errors == 1, "got %d frames, %u crc errors", got, par.crc_errors);
  printf("payload sizes: BASE %zu SNAIL %zu ENV %zu STATUS %zu (max %zu)\n", sizeof(sp::BasePayload),
         sizeof(sp::SnailPayload), sizeof(sp::EnvPayload), sizeof(sp::StatusPayload), sp::MAX_PAYLOAD);
}

// ---------------------------------------------------------------- heartbeat
// Synthetic HB_ADC: band-passed IR plethysmogram of a snail heart. Each beat = fast
// systole bump + slower decay, amplitude A counts, plus white noise and slow drift.
static float pulse_shape(float ph) {           // ph in [0,1)
  float s = expf(-powf((ph - 0.12f) / 0.06f, 2)) - 0.35f * expf(-powf((ph - 0.45f) / 0.15f, 2));
  return s;
}
static void run_hb(float bpm, float amp, float noise, float jitter, uint32_t seconds,
                   float &bpm_out, int &q_out, int &beats, unsigned seed = 1) {
  std::mt19937 rng(seed);
  std::normal_distribution<float> nz(0, noise), jit(0, jitter);
  HeartbeatDetector hb(50, 5);
  float period = 60.0f / bpm, ph = 0, cur_period = period;
  for (uint32_t i = 0; i < seconds * 50; i++) {
    float t = i / 50.0f;
    float v = 2048 + amp * pulse_shape(ph) + 30 * sinf(2 * 3.14159f * 0.01f * t) + nz(rng);
    ph += (1.0f / 50.0f) / cur_period;
    if (ph >= 1) { ph -= 1; cur_period = period * (1 + jit(rng)); }
    int a = (int)lroundf(v); if (a < 0) a = 0; if (a > 4095) a = 4095;
    hb.update((uint16_t)a, (uint32_t)(t * 1000) + 1);
  }
  bpm_out = hb.bpm_x10() / 10.0f; q_out = hb.quality(); beats = hb.beats_total();
}
static void test_heartbeat() {
  struct C { float bpm, amp, noise, jit; } cases[] = {
      {30, 200, 5, 0.03f}, {8, 150, 5, 0.05f}, {60, 120, 8, 0.03f}, {90, 300, 5, 0.02f}, {20, 40, 4, 0.05f},
      {120, 200, 5, 0.02f}, {4, 150, 5, 0.05f}};
  for (auto &c : cases) {
    float b; int q, n;
    run_hb(c.bpm, c.amp, c.noise, c.jit, 180, b, q, n);
    float err = fabsf(b - c.bpm) / c.bpm;
    printf("heartbeat %5.1f bpm (amp %3.0f, noise %2.0f): measured %5.1f bpm, quality %3d, beats %d\n",
           c.bpm, c.amp, c.noise, b, q, n);
    CHECK(err < 0.08f, "bpm %.1f vs %.1f", b, c.bpm);
    CHECK(q > 30, "quality %d", q);
  }
  {  // rate step: 10 bpm for 3 min, then 25 bpm (snail warming up) - must follow, not lock at half
    HeartbeatDetector hb(50, 5);
    std::mt19937 rng(3); std::normal_distribution<float> nz(0, 5);
    float ph = 0;
    for (uint32_t i = 0; i < 360 * 50; i++) {
      float t = i / 50.0f, bpm = t < 180 ? 10 : 25;
      float v = 2048 + 150 * pulse_shape(ph) + nz(rng);
      ph += (1.0f / 50.0f) * bpm / 60.0f; if (ph >= 1) ph -= 1;
      hb.update((uint16_t)lroundf(v), (uint32_t)(t * 1000) + 1);
      if (i == 179 * 50) printf("step test: before step %.1f bpm\n", hb.bpm_x10() / 10.0f);
    }
    printf("step test: 3 min after step to 25 bpm -> %.1f bpm\n", hb.bpm_x10() / 10.0f);
    CHECK(fabsf(hb.bpm_x10() / 10.0f - 25) < 2, "rate step");
  }
  float b; int q, n;
  run_hb(30, 0, 2, 0, 120, b, q, n);            // sensor off the shell: noise only
  printf("noise only: %.1f bpm, quality %d, beats %d\n", b, q, n);
  CHECK(b == 0 && q == 0, "noise must give no rate");
  // waveform ring
  HeartbeatDetector hb(50, 5);
  for (uint32_t i = 0; i < 200; i++) hb.update((uint16_t)(i * 16), i * 20);
  uint8_t w[20]; uint8_t k = hb.copy_wave(w, 20);
  CHECK(k == 20 && w[19] > w[0], "wave order");
}

// ---------------------------------------------------------------- thermal
static void test_thermal() {
  // node voltage for 40 C with B3950: R=5.30k -> ratio 0.3465 -> 1.143 V
  float r40 = 10000.0f * expf(3950.0f * (1 / 313.15f - 1 / 298.15f));
  uint16_t adc = (uint16_t)lroundf(4095 * r40 / (10000 + r40));
  float t = ntc_celsius(adc, 4095, 10000, 10000, 3950);
  printf("NTC: 40 C -> R=%.0f ohm, node %.3f V, adc %u -> %.2f C\n", r40, 3.3f * r40 / (10000 + r40), adc, t);
  CHECK(fabsf(t - 40) < 0.2f, "ntc 40C");
  CHECK(fabsf(ntc_celsius(2048, 4095, 10000, 10000, 3950) - 25) < 0.1f, "ntc 25C");
  CHECK(std::isnan(ntc_celsius(4095, 4095, 10000, 10000, 3950)), "open");
  CHECK(std::isnan(ntc_celsius(0, 4095, 10000, 10000, 3950)), "short");

  HeaterController h(20, 35);
  CHECK(h.update(10, 15, 1) > 200, "cold box -> heat hard");
  CHECK(h.update(10, 35.5f, 1) == 0 && h.mode() == 0, "hot plate -> off");
  HeaterController h2(20, 35);
  CHECK(h2.update(NAN, 30, 1) < 255 && h2.mode() == 2, "SHT fail -> plate mode");
  HeaterController h3(20, 35);
  CHECK(h3.update(NAN, NAN, 1) == 51 && h3.mode() == 3, "blind 20%%");
  HeaterController h4(20, 35);
  CHECK(h4.update(0, NAN, 1) <= 128, "no plate sensor cap");

  // crude thermal simulation: box with heater, -40 C outside
  HeaterController hc(20, 35);
  float box = 5, plate = 5, outside = -40;
  for (int s = 0; s < 3 * 3600; s++) {
    float duty = hc.update(box, plate, 1) / 255.0f;
    float P = duty * 4.4f;                                   // W
    plate += (P - (plate - box) / 2.0f) / 15.0f;             // plate: 15 J/K, 2 K/W to box
    box   += ((plate - box) / 2.0f - (box - outside) / 15.0f) / 300.0f;   // box: 300 J/K, 15 K/W to outside
  }
  printf("thermal sim after 3 h at -40 C outside: box %.1f C, plate %.1f C, duty %u\n", box, plate, hc.duty());
  CHECK(fabsf(box - 20) < 1.5f && plate < 35, "thermal sim");
}

// ---------------------------------------------------------------- sensor maths
static void test_sensors() {
  Bmp280Calib c = {27504, 26435, -1000, 36477, -10685, 3024, 2855, 140, -7, 15500, -14600, 6000};
  int32_t tf; int32_t T = bmp280_comp_t(c, 519888, tf); uint32_t P = bmp280_comp_p(c, 415148, tf);
  printf("BMP280 datasheet example: T=%d (exp 2508), P=%u Pa (exp 100653)\n", T, P);
  CHECK(T == 2508 && P == 100653, "bmp280");
  Ms5611Prom pr = {{0, 40127, 36924, 23317, 23282, 33464, 28312}};
  int32_t t5, p5; ms5611_compensate(pr, 9085466, 8569150, t5, p5);
  printf("MS5611 datasheet example: T=%d (exp 2007), P=%d (exp 100009)\n", t5, p5);
  CHECK(t5 == 2007 && p5 == 100009, "ms5611");
  int32_t D2c = (int32_t)(33464 * 256 + (-2000) * 8388608.0 / 28312);   // ~0 C
  ms5611_compensate(pr, 9085466, D2c, t5, p5);
  printf("MS5611 cold case: T=%.2f C P=%.2f mbar (IntroStratLib 0.1.2 gives -163.5 C / 948.1 mbar)\n", t5 / 100.0, p5 / 100.0);
  CHECK(t5 > -300 && t5 < 0, "ms5611 cold");
  float a1 = isa_altitude_m(101325), a2 = isa_altitude_m(22632), a3 = isa_altitude_m(3000);
  printf("ISA altitude: 1013 mbar %.0f m, 226 mbar %.0f m, 30 mbar %.0f m\n", a1, a2, a3);
  CHECK(fabsf(a1) < 1 && fabsf(a2 - 11000) < 5 && fabsf(a3 - 23850) < 150, "isa");
}

// ---------------------------------------------------------------- DHT-11
// Builds the 40 HIGH-pulse widths the sensor would send for 5 bytes (0 -> 27 us, 1 -> 70 us).
static void dht_pulses(const uint8_t *bytes, uint8_t *hi) {
  for (int i = 0; i < 40; i++) hi[i] = ((bytes[i / 8] >> (7 - i % 8)) & 1) ? 70 : 27;
}
static void test_dht11() {
  uint8_t frame[5] = {55, 0, 23, 4, (uint8_t)(55 + 0 + 23 + 4)};   // 55 %RH, 23.4 C
  uint8_t hi[40], got[5];
  dht_pulses(frame, hi);
  dht11_bits_to_bytes(hi, got);
  CHECK(memcmp(frame, got, 5) == 0, "dht bits -> bytes");
  float t = 0, rh = 0;
  CHECK(dht11_decode(got, t, rh) && fabsf(t - 23.4f) < 0.01f && fabsf(rh - 55.0f) < 0.01f, "dht decode %.2f %.2f", t, rh);
  // jitter: a 0 stretched to 40 us or a 1 shortened to 52 us still decodes
  // (55 = 0b00110111: bit index 0 is a 0, index 2 is a 1)
  CHECK(hi[0] == 27 && hi[2] == 70, "test setup");
  hi[0] = 40; hi[2] = 52;
  dht11_bits_to_bytes(hi, got);
  CHECK(memcmp(frame, got, 5) == 0, "dht tolerates jitter");
  // a 0 stretched past the threshold flips a bit -> checksum catches it
  // (index 24 = top bit of the T decimal byte 4 = 0b00000100, a 0)
  dht_pulses(frame, hi); CHECK(hi[24] == 27, "test setup"); hi[24] = 80;
  dht11_bits_to_bytes(hi, got);
  CHECK(!dht11_decode(got, t, rh), "dht checksum catches a flipped bit");
  uint8_t neg[5] = {40, 0, 5, 0x82, (uint8_t)(40 + 5 + 0x82)};   // -5.2 C (sign bit on newer parts)
  CHECK(dht11_decode(neg, t, rh) && fabsf(t + 5.2f) < 0.01f, "dht negative %.2f", t);
  uint8_t zero[5] = {0, 0, 0, 0, 0};
  CHECK(!dht11_decode(zero, t, rh), "dht all-zero frame (line stuck low) rejected");
  uint8_t silly[5] = {150, 0, 20, 0, (uint8_t)170};
  CHECK(!dht11_decode(silly, t, rh), "dht RH > 100 rejected");
}

// ---------------------------------------------------------------- MAX30100 FIFO
// MQ-7 on 5 V: 10k/20k divider before PB1, reading scaled back to the module (fw 1.4)
static void test_mq7_scale() {
  CHECK(mq7_pb1_mv(4095, 4095, 3300) == 3300 && mq7_pb1_mv(0, 4095, 3300) == 0, "MQ-7 ADC -> mV");
  CHECK(mq7_pb1_mv(2048, 4095, 3300) == 1650, "MQ-7 mid-scale %u", (unsigned)mq7_pb1_mv(2048, 4095, 3300));
  CHECK(mq7_ao_mv(3300, 10000, 20000) == 4950, "5 V module: 3.30 V at PB1 = 4.95 V at AO");
  CHECK(mq7_ao_mv(1000, 10000, 20000) == 1500, "1.0 V at PB1 = 1.5 V at AO");
  CHECK(mq7_ao_mv(1234, 0, 20000) == 1234, "no divider: unchanged");
  CHECK(mq7_ao_mv(60000, 10000, 20000) == 0xFFFE, "clamped below the 'disabled' code 0xFFFF");
  CHECK(!mq7_pb1_ok(0) && !mq7_pb1_ok(3295) && mq7_pb1_ok(700), "MQ-7 ok range at PB1");
  printf("MQ-7 5 V divider: PB1 1.00 V -> AO %u mV, PB1 3.30 V -> AO %u mV\n",
         (unsigned)mq7_ao_mv(1000, 10000, 20000), (unsigned)mq7_ao_mv(3300, 10000, 20000));
}

static void test_max30100_fifo() {
  bool lost = true;
  CHECK(max30100_waiting(0, 0, 0, lost) == 0 && !lost, "max fifo: empty");
  CHECK(max30100_waiting(4, 0, 0, lost) == 4 && !lost, "max fifo: 4 waiting");
  CHECK(max30100_waiting(2, 13, 0, lost) == 5 && !lost, "max fifo: write pointer wrapped -> 5");
  CHECK(max30100_waiting(7, 7, 3, lost) == 16 && lost, "max fifo: full and overflowed -> 16, lost");
  CHECK(max30100_waiting(0x14, 0x10, 0, lost) == 4, "max fifo: upper bits ignored");
}

// ---------------------------------------------------------------- MAX30100 optical pulse
// Simulated IR at 100 Hz: DC level + pulse ripple (same shape as the TCRT5000 tests) + noise.
// Returns bpm at the end; 'from_s' = when the sensor is put on (IR 200 = nothing in front before).
static float run_optical(float bpm, float dc, float amp, float noise, uint32_t seconds, OpticalPulse &op,
                         float from_s = 0, unsigned seed = 3) {
  std::mt19937 rng(seed);
  std::normal_distribution<float> nd(0, noise);
  float ph = 0;
  for (uint32_t i = 0; i < seconds * 100; i++) {
    float t = i / 100.0f, v;
    ph += bpm / 60.0f / 100.0f; if (ph >= 1) ph -= 1;
    if (t < from_s) v = 200 + nd(rng) * 0.3f;
    else v = dc + amp * pulse_shape(ph) + nd(rng) + 0.02f * dc * sinf(t / 7.0f);   // + slow drift
    if (v < 0) v = 0; if (v > 65535) v = 65535;
    op.update((uint16_t)v, i * 10);
  }
  return op.det().bpm_x10() / 10.0f;
}
static void test_optical_pulse() {
  { OpticalPulse op; float b = run_optical(72, 30000, 400, 15, 60, op);      // fingertip
    printf("MAX30100 finger 72 bpm -> %.1f bpm, quality %u\n", b, op.det().quality());
    CHECK(fabsf(b - 72) < 3 && op.contact(), "optical: finger 72 bpm, got %.1f", b); }
  { OpticalPulse op; float b = run_optical(30, 9000, 80, 8, 90, op);         // snail shell: slow, weak
    printf("MAX30100 snail 30 bpm  -> %.1f bpm, quality %u\n", b, op.det().quality());
    CHECK(fabsf(b - 30) < 2, "optical: snail 30 bpm, got %.1f", b); }
  { OpticalPulse op; float b = run_optical(72, 30000, 0, 15, 30, op, 1000);  // nothing in front, ever
    CHECK(b == 0 && !op.contact() && op.det().no_signal(), "optical: nothing in front = no rate, no contact"); }
  { OpticalPulse op; float b = run_optical(72, 30000, 0, 6, 60, op);        // pressed on, no pulse at all
    CHECK(b == 0 && op.contact(), "optical: contact without pulse = no rate (got %.1f)", b); }
  { OpticalPulse op;                                                          // put on after 10 s
    float b = run_optical(60, 25000, 350, 15, 30, op, 10);
    printf("MAX30100 put on at 10 s -> %.1f bpm at 30 s (restarts %u)\n", b, op.restarts());
    CHECK(fabsf(b - 60) < 3 && op.restarts() >= 1, "optical: recovers after being put on, got %.1f", b); }
}

// ---------------------------------------------------------------- radio queue
struct FakeStream {
  std::vector<uint8_t> tx, rx;
  int available() { return (int)rx.size(); }
  int read() { int b = rx.front(); rx.erase(rx.begin()); return b; }
  size_t write(const uint8_t *d, size_t n) { tx.insert(tx.end(), d, d + n); return n; }
};
static void test_radio() {
  FakeStream s;
  RadioLink<FakeStream> r(s, 0x06, 1500);
  uint8_t pl[10] = {1, 2, 3};
  r.send(sp::PKT_STATUS, pl, 10);
  { uint8_t n = 0; const uint8_t *lf = r.last_frame(n);
    CHECK(n == 17 && lf[0] == 0xAA && lf[1] == 0x55 && lf[2] == sp::PKT_STATUS && lf[3] == 0 && lf[4] == 10, "last_frame"); }
  r.send(sp::PKT_STATUS, pl, 10);
  { uint8_t n = 0; const uint8_t *lf = r.last_frame(n); CHECK(n == 17 && lf[3] == 1, "last_frame seq 1"); }
  r.poll(0);
  CHECK(s.tx.size() == 17, "first frame only %zu", s.tx.size());
  r.poll(100);
  CHECK(s.tx.size() == 17, "waits for ack");
  s.rx.push_back(0x06); r.poll(200);
  CHECK(s.tx.size() == 34, "second after ack");
  r.poll(2000);                                          // no ack -> timeout counted
  CHECK(r.missed_acks() == 1, "timeout");
  for (int i = 0; i < 12; i++) r.send(sp::PKT_STATUS, pl, 10);
  CHECK(r.dropped() == 4 && r.queued() == 8, "overflow drops oldest");
  // dump real frames for the Python ground-station test
  FILE *f = fopen("frames.bin", "wb");
  sp::SnailPayload sn = {}; sn.t_ms = 5000; sn.bpm_x10 = 245; sn.quality = 77; sn.wave_n = 20;
  sn.max_bpm_x10 = 251; sn.max_beats_total = 40; sn.max_quality = 64; sn.max_flags = 1; sn.max_amp = 180; sn.max_ir = 9100;
  CHECK(sizeof sn == 49 && sp::SNAIL_V1_SIZE == 39, "SNAIL size %zu", sizeof sn);
  for (int i = 0; i < 20; i++) sn.wave[i] = (uint8_t)(128 + 40 * sinf(i * 0.6f));
  sp::EnvPayload en = {}; en.t_ms = 6000; en.box_t_c100 = 1987; en.box_rh_c100 = 6543; en.box_press_pa = 101100;
  en.heater_t_c100 = -32768; en.heater_duty = 0; en.heater_mode = 0;
  en.dht_t_c100 = 2100; en.dht_rh_c100 = 5500; en.mq7_mv = 412; en.sensors_ok = 0x77;   // bit 7 (second BMP280) clear since fw 1.3
  en.bmp_t_c100 = 2010; en.box2_press_pa = 0; en.bmp2_t_c100 = -32768;          // second BMP280 removed in fw 1.3
  CHECK(sizeof en == 36 && sp::ENV_V1_SIZE == 24, "ENV size %zu", sizeof en);
  sp::BasePayload ba = {}; ba.t_ms = 7000; ba.press_pa = 3000; ba.baro_t_c100 = -1234; ba.alt_dm = 238500; ba.acc_mg[2] = 1000;
  sp::StatusPayload st = {}; st.t_ms = 8000; st.fw_version = 0x0103; st.boot_count = 3; st.fc_ok = 0x0F;
  uint8_t fr[64]; size_t n;
  n = sp::build_frame(sp::PKT_SNAIL, 1, &sn, sizeof sn, fr); fwrite(fr, 1, n, f);
  fputc(0x06, f);                                        // stray byte between frames
  n = sp::build_frame(sp::PKT_ENV, 2, &en, sizeof en, fr); fwrite(fr, 1, n, f);
  n = sp::build_frame(sp::PKT_BASE, 3, &ba, sizeof ba, fr); fwrite(fr, 1, n, f);
  n = sp::build_frame(sp::PKT_STATUS, 4, &st, sizeof st, fr); fwrite(fr, 1, n, f);
  n = sp::build_frame(sp::PKT_ENV, 5, &en, sp::ENV_V1_SIZE, fr); fwrite(fr, 1, n, f);   // fw 1.1 frame
  n = sp::build_frame(sp::PKT_SNAIL, 6, &sn, sp::SNAIL_V1_SIZE, fr); fwrite(fr, 1, n, f); // fw 1.2 frame
  fclose(f);
}

int main() {
  test_protocol();
  test_heartbeat();
  test_thermal();
  test_sensors();
  test_dht11();
  test_mq7_scale();
  test_max30100_fifo();
  test_optical_pulse();
  test_radio();
  printf(fails ? "\n%d CHECK(S) FAILED\n" : "\nALL TESTS PASSED\n", fails);
  return fails ? 1 : 0;
}
