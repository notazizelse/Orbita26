// SnailPulse radio protocol - binary frames, each fits in ONE 58-byte LoRa packet
//
// Frame:  0xAA 0x55 | type | seq | len | payload[len] | crc16 (LE)
//         crc16 = CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over type..payload
// All multi-byte fields are little-endian (native on STM32). Structs are packed.
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace sp {

static const uint8_t SYNC0 = 0xAA, SYNC1 = 0x55;
static const size_t FRAME_OVERHEAD = 7;        // 2 sync + type + seq + len + 2 crc
static const size_t LORA_PACKET = 58;          // E32 splits data into 58-byte packets
static const size_t MAX_PAYLOAD = LORA_PACKET - FRAME_OVERHEAD;   // 51

enum PacketType : uint8_t {
  PKT_BASE   = 0x01,   // FC service sensors (organizers' platform telemetry)
  PKT_SNAIL  = 0x10,   // both pulse sensors: TCRT5000 result + 2 s of its waveform, MAX30100 result
  PKT_ENV    = 0x11,   // snail box environment + heater
  PKT_STATUS = 0x7F,   // health, counters, firmware version
};

#pragma pack(push, 1)
struct BasePayload {        // 34 bytes
  uint32_t t_ms;            // millis() since boot
  uint32_t press_pa;        // MS5611 pressure, Pa
  int16_t  baro_t_c100;     // MS5611 temperature, 0.01 C
  int32_t  alt_dm;          // MS5611 altitude estimate, 0.1 m
  int16_t  acc_mg[3];       // LSM6DS3 acceleration, milli-g
  int16_t  gyr_dps10[3];    // LSM6DS3 rate, 0.1 deg/s
  int16_t  mag_mgauss[3];   // LIS3MDL field, milli-gauss
  int16_t  fc_t_x8;         // LM75A board temperature * 8
};

static const uint8_t WAVE_N = 20;
struct SnailPayload {       // 49 bytes (fw 1.0-1.2 sent the first 39)
  uint32_t t_ms;            // time of the LAST waveform sample
  // ---- pulse sensor 1: TCRT5000 -> analog band-pass amplifier -> HB_ADC (PB0)
  uint16_t bpm_x10;         // heart rate, beats/min * 10 (0 = no valid rate)
  uint16_t last_ibi_ms;     // last inter-beat interval
  uint16_t beats_total;     // beats counted since boot
  uint8_t  quality;         // 0..100 signal quality
  uint8_t  flags;           // bit0 beat in this window, bit1 signal saturated, bit2 no signal
  uint16_t amp_counts;      // pulse amplitude, ADC counts (12-bit)
  uint16_t dc_counts;       // mean HB_ADC level, ADC counts
  uint8_t  wave[WAVE_N];    // HB_ADC at 10 Hz, 8-bit (adc >> 4), oldest first
  uint8_t  wave_n;          // valid samples in wave[] (normally 20)
  uint8_t  rsv[2];
  // ---- pulse sensor 2: MAX30100 optical (I2C4, 0x57), added in fw 1.3 (30 Oct 2026).
  // Appended so 39-byte frames from older firmware still decode.
  uint16_t max_bpm_x10;     // heart rate, beats/min * 10 (0 = no valid rate)
  uint16_t max_beats_total; // beats counted since boot
  uint8_t  max_quality;     // 0..100 signal quality
  uint8_t  max_flags;       // bit0 beat in this window, bit1 saturated (too much light), bit2 no pulse,
                            // bit3 MAX30100 not answering, bit4 no contact (nothing in front of it)
  uint16_t max_amp;         // pulse amplitude, IR counts (16-bit)
  uint16_t max_ir;          // newest IR sample: ~200 = nothing in front, thousands = contact
};
static const size_t SNAIL_V1_SIZE = 39;   // SnailPayload size before fw 1.3

struct EnvPayload {         // 36 bytes (fw 1.0-1.1 sent the first 24)
  uint32_t t_ms;
  int16_t  box_t_c100;      // SHT21 temperature (-32768 since fw 1.5: SHT21 not fitted, use bmp_t_c100)
  uint16_t box_rh_c100;     // SHT21 humidity, 0.01 % (0xFFFF since fw 1.5: use dht_rh_c100)
  uint32_t box_press_pa;    // BMP280 (0x76, SDO -> GND) pressure, Pa (0 = invalid)
  int16_t  bmp_t_c100;      // BMP280 temperature
  int16_t  heater_t_c100;   // NTC1 (heater surface); -32768 on rev C (no heater)
  uint8_t  heater_duty;     // 0..255 PWM duty commanded
  uint8_t  heater_mode;     // 0 off, 1 box control (SHT21), 2 heater-only (NTC1), 3 blind 20%
  uint16_t max30100_ir;     // backup optical sensor, raw IR
  uint8_t  sensors_ok;      // bit0 SHT21, bit1 BMP280, bit2 MAX30100, bit3 NTC1, bit4 SD,
                            // bit5 DHT-11, bit6 MQ-7, bit7 second BMP280 (always 0 since fw 1.3)
  uint8_t  rsv[3];
  // ---- added in fw 1.2 (29 Oct 2026). Appended so 24-byte frames from older firmware still decode.
  int16_t  dht_t_c100;      // DHT-11 temperature, 0.01 C (-32768 = no valid reading)
  uint16_t dht_rh_c100;     // DHT-11 humidity, 0.01 % (0xFFFF = no valid reading)
  uint16_t mq7_mv;          // MQ-7 AO at the module, mV, 1 s average (0xFFFF = MQ-7 disabled).
                            // fw 1.4+: 0-5000 (5 V module, PB1 reading scaled back through the divider)
  uint32_t box2_press_pa;   // second BMP280 (0x77) pressure, Pa (0 = invalid). Removed in fw 1.3: always 0
  int16_t  bmp2_t_c100;     // second BMP280 temperature (-32768 = invalid). Removed in fw 1.3: always -32768
};
static const size_t ENV_V1_SIZE = 24;     // EnvPayload size before fw 1.2

struct StatusPayload {      // 24 bytes
  uint32_t t_ms;
  uint16_t fw_version;
  uint16_t boot_count;      // from SD (0 if no SD)
  uint16_t frames_sent;
  uint16_t acks_missed;
  uint16_t frames_dropped;  // queue overflow
  uint16_t i2c_errors;
  uint16_t sd_errors;
  uint16_t loop_max_ms;     // worst loop time in the last period
  uint8_t  reset_cause;     // 1 = watchdog
  uint8_t  fc_ok;           // FC service sensors found at boot: bit0 LSM6DS3, bit1 LIS3MDL, bit2 LM75A,
                            // bit3 MS5611 (live). Was reserved (0) before fw 1.2 build of 30 Oct 2026.
  uint8_t  rsv[2];
};
#pragma pack(pop)

static_assert(sizeof(BasePayload)   <= MAX_PAYLOAD, "BASE too big");
static_assert(sizeof(SnailPayload)  <= MAX_PAYLOAD, "SNAIL too big");
static_assert(sizeof(EnvPayload)    <= MAX_PAYLOAD, "ENV too big");
static_assert(sizeof(SnailPayload) == 49, "SNAIL layout changed: update ground/snailpulse_ground.py too");
static_assert(sizeof(EnvPayload) == 36, "ENV layout changed: update ground/snailpulse_ground.py too");
static_assert(sizeof(StatusPayload) <= MAX_PAYLOAD, "STATUS too big");

uint16_t crc16_ccitt(const uint8_t *data, size_t len);

// Builds a frame into out[] (must hold len+7 bytes). Returns frame length, 0 on error.
size_t build_frame(uint8_t type, uint8_t seq, const void *payload, size_t len, uint8_t *out);

// Streaming parser (used by tests; the ground station has a Python twin).
class FrameParser {
 public:
  // Feed one byte; returns true when a valid frame is complete (read type/seq/payload/len).
  bool feed(uint8_t b);
  uint8_t type = 0, seq = 0, len = 0;
  uint8_t payload[MAX_PAYLOAD];
  uint32_t crc_errors = 0;
 private:
  uint8_t state_ = 0, idx_ = 0, crc_lo_ = 0;
  uint8_t buf_[MAX_PAYLOAD + 3];
};

}  // namespace sp
