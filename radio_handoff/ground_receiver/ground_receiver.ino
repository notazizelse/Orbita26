// SnailPulse ground receiver: an Arduino-type board + E32 LoRa module -> USB to the laptop.
// Only needed if the ground E32 is on a microcontroller. With a plain USB-UART adapter on the E32,
// run ground/snailpulse_monitor.py on the adapter's port directly instead.
//
// snailpulse_frames.h in this folder is a copy of ../snailpulse_frames.h (the PC tests check they match).
//
// Wiring (E32-433T30D):  TXD -> board RX1,  RXD -> board TX1,  M0 + M1 -> GND (normal mode),
//                        VCC -> 5 V (it draws ~1 A when transmitting), GND -> GND.
// The E32's UART is 3.3 V logic: on a 5 V board (Uno/Mega) put a divider (e.g. 1k/2k) in board TX -> E32 RXD.
//
// What comes out of USB (115200): every good frame, byte for byte, so the SnailPulse monitor works on it:
//     python3 ground/snailpulse_monitor.py --port <this board's port>
// PRINT_TEXT 1 adds a readable line per frame (the monitor shows it in its Console panel).
#include "snailpulse_frames.h"

#define E32_BAUD   9600     // the ground E32's UART rate (its RF settings), NOT 115200
#define PRINT_TEXT 0

#if defined(ARDUINO_ARCH_ESP32)
#define E32_RX 16           // ESP32: any free pins
#define E32_TX 17
#endif

sp_parser_t parser;

void setup() {
  Serial.begin(115200);
#if defined(ARDUINO_ARCH_ESP32)
  Serial1.begin(E32_BAUD, SERIAL_8N1, E32_RX, E32_TX);
#else
  Serial1.begin(E32_BAUD);
#endif
  sp_parser_init(&parser);
}

void loop() {
  while (Serial1.available() > 0) {
    if (!sp_parser_feed(&parser, (uint8_t)Serial1.read())) continue;
    Serial.write(parser.raw, parser.raw_len);           // forward unchanged
#if PRINT_TEXT
    Serial.print("\r\nframe type 0x"); Serial.print(parser.type, HEX);
    Serial.print(" seq "); Serial.print(parser.seq);
    if (parser.type == SP_PKT_ENV && parser.len >= sizeof(sp_env_t)) {
      sp_env_t e; memcpy(&e, parser.payload, sizeof e);
      Serial.print(" box "); Serial.print(e.box_t_c100 / 100.0); Serial.print(" C");
      Serial.print(" ok=0x"); Serial.print(e.sensors_ok, HEX);
    } else if (parser.type == SP_PKT_SNAIL && parser.len >= sizeof(sp_snail_t)) {
      sp_snail_t s; memcpy(&s, parser.payload, sizeof s);
      Serial.print(" heart "); Serial.print(s.bpm_x10 / 10.0); Serial.print(" bpm");
    }
    Serial.print(" (good "); Serial.print(parser.frames_ok);
    Serial.print(", CRC errors "); Serial.print(parser.crc_errors); Serial.println(")");
#endif
  }
}
