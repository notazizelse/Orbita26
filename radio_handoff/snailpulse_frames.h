/* SnailPulse telemetry frames: everything a radio or ground program needs, in one header.
 * Plain C99, also compiles as C++. No Arduino, no dynamic memory. Same layout as
 * firmware/SnailPulse_Flight/protocol.h (the flight code is the reference).
 *
 *   Frame:  AA 55 | type | seq | len | payload[len] | crc16 lo | crc16 hi
 *   crc16 = CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no final XOR)
 *           over type, seq, len and the payload.  All numbers little-endian.
 *   A frame is at most 58 bytes = exactly one E32 LoRa packet.  Send it as it is.
 *
 * Use:
 *   sp_parser_t p; sp_parser_init(&p);
 *   for each received byte b:
 *     if (sp_parser_feed(&p, b)) {          // one complete frame with a good CRC
 *       if (p.type == SP_PKT_ENV && p.len >= sizeof(sp_env_t)) {
 *         sp_env_t env; memcpy(&env, p.payload, sizeof env);  ...
 *       }
 *     }
 */
#ifndef SNAILPULSE_FRAMES_H
#define SNAILPULSE_FRAMES_H

#include <stdint.h>
#include <stddef.h>
#include <string.h>

#define SP_SYNC0          0xAA
#define SP_SYNC1          0x55
#define SP_FRAME_OVERHEAD 7      /* 2 sync + type + seq + len + 2 crc */
#define SP_LORA_PACKET    58     /* the E32 sends data in 58-byte packets */
#define SP_MAX_PAYLOAD    51

/* Packet types and how often the flight controller sends them */
#define SP_PKT_BASE    0x01      /* every 2 s,  41-byte frame: FC barometer, IMU, magnetometer, board temp */
#define SP_PKT_SNAIL   0x10      /* every 2 s,  56-byte frame: both snails' heart rates + 2 s of snail 1 waveform */
#define SP_PKT_ENV     0x11      /* every 4 s,  43-byte frame: box sensors (SHT21, BMP280, DHT-11, MQ-7...) */
#define SP_PKT_STATUS  0x7F      /* every 10 s, 31-byte frame + once at boot: health counters */

/* "No valid reading" markers */
#define SP_INVALID_I16  (-32768)
#define SP_INVALID_U16  0xFFFF

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define SP_PACKED
#else
#define SP_PACKED __attribute__((packed))
#endif

typedef struct SP_PACKED {   /* 34 bytes */
  uint32_t t_ms;             /* ms since FC boot */
  uint32_t press_pa;         /* MS5611 pressure, Pa (0 = invalid) */
  int16_t  baro_t_c100;      /* MS5611 temperature, 0.01 C */
  int32_t  alt_dm;           /* altitude, 0.1 m */
  int16_t  acc_mg[3];        /* LSM6DS3, milli-g */
  int16_t  gyr_dps10[3];     /* LSM6DS3, 0.1 deg/s */
  int16_t  mag_mgauss[3];    /* LIS3MDL, milli-gauss */
  int16_t  fc_t_x8;          /* LM75A, C * 8 */
} sp_base_t;

typedef struct SP_PACKED {   /* 49 bytes (firmware before 1.3 sent only the first 39) */
  uint32_t t_ms;             /* time of the LAST waveform sample */
  /* snail 1: TCRT5000 -> analog amplifier -> PB0 */
  uint16_t bpm_x10;          /* heart rate * 10 (0 = no valid rate) */
  uint16_t last_ibi_ms;      /* last beat-to-beat interval */
  uint16_t beats_total;
  uint8_t  quality;          /* 0..100 % */
  uint8_t  flags;            /* bit0 beat seen, bit1 amplifier saturated, bit2 no signal */
  uint16_t amp_counts;       /* pulse amplitude, 12-bit ADC counts */
  uint16_t dc_counts;        /* mean level, 12-bit ADC counts (2048 = 1.65 V) */
  uint8_t  wave[20];         /* 10 Hz samples, 8-bit, oldest first */
  uint8_t  wave_n;
  uint8_t  rsv[2];
  /* snail 2: MAX30100 optical sensor (I2C 0x57), added in firmware 1.3 */
  uint16_t max_bpm_x10;      /* heart rate * 10 (0 = no valid rate) */
  uint16_t max_beats_total;
  uint8_t  max_quality;      /* 0..100 % */
  uint8_t  max_flags;        /* bit0 beat seen, bit1 too much light, bit2 no pulse, bit3 not answering, bit4 no contact */
  uint16_t max_amp;          /* pulse amplitude, IR counts (16-bit) */
  uint16_t max_ir;           /* IR level: ~200 = nothing in front, thousands = on the shell */
} sp_snail_t;

typedef struct SP_PACKED {   /* 36 bytes (firmware before 1.2 sent only the first 24) */
  uint32_t t_ms;
  int16_t  box_t_c100;       /* SHT21 temperature, 0.01 C (-32768 since firmware 1.5: not fitted, use bmp_t_c100) */
  uint16_t box_rh_c100;      /* SHT21 humidity, 0.01 % (0xFFFF since firmware 1.5: use dht_rh_c100) */
  uint32_t box_press_pa;     /* BMP280 (0x76) pressure, Pa (0 = invalid) */
  int16_t  bmp_t_c100;       /* BMP280 temperature */
  int16_t  heater_t_c100;    /* rev B heater only; -32768 on rev C */
  uint8_t  heater_duty;
  uint8_t  heater_mode;
  uint16_t max30100_ir;
  uint8_t  sensors_ok;       /* bit0 SHT21, 1 BMP280, 2 MAX30100, 3 heater NTC, 4 SD, 5 DHT-11, 6 MQ-7, 7 second BMP280 (unused) */
  uint8_t  rsv[3];
  int16_t  dht_t_c100;       /* DHT-11 temperature, 0.01 C */
  uint16_t dht_rh_c100;      /* DHT-11 humidity, 0.01 % */
  uint16_t mq7_mv;           /* MQ-7 output at the module, mV (0-5000 since firmware 1.4: 5 V supply) */
  uint32_t box2_press_pa;    /* second BMP280: removed in firmware 1.3, always 0 */
  int16_t  bmp2_t_c100;      /* second BMP280: removed in firmware 1.3, always -32768 */
} sp_env_t;

typedef struct SP_PACKED {   /* 24 bytes */
  uint32_t t_ms;
  uint16_t fw_version;       /* 0x0103 = 1.3 */
  uint16_t boot_count;
  uint16_t frames_sent;      /* frames handed to the comm board */
  uint16_t acks_missed;      /* no 0x06 within 1.5 s */
  uint16_t frames_dropped;   /* queue overflow */
  uint16_t i2c_errors;
  uint16_t sd_errors;
  uint16_t loop_max_ms;
  uint8_t  reset_cause;      /* 1 = watchdog */
  uint8_t  fc_ok;            /* bit0 LSM6DS3, 1 LIS3MDL, 2 LM75A, 3 MS5611 */
  uint8_t  rsv[2];
} sp_status_t;

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

typedef char sp_check_base[(sizeof(sp_base_t) == 34) ? 1 : -1];
typedef char sp_check_snail[(sizeof(sp_snail_t) == 49) ? 1 : -1];
typedef char sp_check_env[(sizeof(sp_env_t) == 36) ? 1 : -1];
typedef char sp_check_status[(sizeof(sp_status_t) == 24) ? 1 : -1];

static inline uint16_t sp_crc16(const uint8_t *d, size_t n) {
  uint16_t crc = 0xFFFF;
  while (n--) {
    uint8_t i;
    crc ^= (uint16_t)(*d++) << 8;
    for (i = 0; i < 8; i++) crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021) : (uint16_t)(crc << 1);
  }
  return crc;
}

/* Build a frame into out[] (room for len + 7 bytes). Returns the frame length, 0 if len > 51. */
static inline size_t sp_build_frame(uint8_t type, uint8_t seq, const void *payload, uint8_t len, uint8_t *out) {
  uint16_t crc;
  if (len > SP_MAX_PAYLOAD) return 0;
  out[0] = SP_SYNC0; out[1] = SP_SYNC1; out[2] = type; out[3] = seq; out[4] = len;
  memcpy(out + 5, payload, len);
  crc = sp_crc16(out + 2, (size_t)len + 3);
  out[5 + len] = (uint8_t)(crc & 0xFF);
  out[6 + len] = (uint8_t)(crc >> 8);
  return (size_t)len + SP_FRAME_OVERHEAD;
}

/* Byte-by-byte frame finder. Recovers by itself after noise or a cut frame. */
typedef struct {
  uint8_t  type, seq, len;              /* valid after sp_parser_feed() returned 1 */
  uint8_t  payload[SP_MAX_PAYLOAD];
  uint8_t  raw[SP_LORA_PACKET];         /* the whole frame as received (to forward it unchanged) */
  uint8_t  raw_len;
  uint32_t frames_ok, crc_errors;
  uint8_t  state_, idx_;
  uint8_t  buf_[SP_LORA_PACKET];
} sp_parser_t;

static inline void sp_parser_init(sp_parser_t *p) { memset(p, 0, sizeof *p); }

/* Returns 1 when b completes a frame with a correct CRC, else 0. */
static inline int sp_parser_feed(sp_parser_t *p, uint8_t b) {
  switch (p->state_) {
    case 0: if (b == SP_SYNC0) { p->buf_[0] = b; p->state_ = 1; } return 0;
    case 1:
      if (b == SP_SYNC1) { p->buf_[1] = b; p->idx_ = 2; p->state_ = 2; }
      else p->state_ = (b == SP_SYNC0) ? 1 : 0;
      return 0;
    case 2:                                           /* type, seq, len */
      p->buf_[p->idx_++] = b;
      if (p->idx_ == 5) {
        if (b > SP_MAX_PAYLOAD) { p->state_ = 0; return 0; }
        p->state_ = 3;
      }
      return 0;
    case 3: {                                         /* payload + 2 crc bytes */
      uint8_t n = p->buf_[4], total = (uint8_t)(n + SP_FRAME_OVERHEAD);
      uint16_t got;
      p->buf_[p->idx_++] = b;
      if (p->idx_ < total) return 0;
      p->state_ = 0;
      got = (uint16_t)(p->buf_[5 + n] | (p->buf_[6 + n] << 8));
      if (got != sp_crc16(p->buf_ + 2, (size_t)n + 3)) { p->crc_errors++; return 0; }
      p->type = p->buf_[2]; p->seq = p->buf_[3]; p->len = n;
      memcpy(p->payload, p->buf_ + 5, n);
      memcpy(p->raw, p->buf_, total); p->raw_len = total;
      p->frames_ok++;
      return 1;
    }
  }
  p->state_ = 0;
  return 0;
}

#endif /* SNAILPULSE_FRAMES_H */
