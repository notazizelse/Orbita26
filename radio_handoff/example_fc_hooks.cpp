// TEMPLATE for route C in README.md: your radio code runs INSIDE the SnailPulse flight firmware.
//
// 1. Copy this file into firmware/SnailPulse_Flight/ (next to SnailPulse_Flight.ino), e.g. as radio_tech.cpp.
//    Put your own .cpp/.h files there too.
// 2. In firmware/SnailPulse_Flight/config.h set  #define SP_OWN_RADIO 0
//    SnailPulse then never touches UART1 (PA10/PA9): it is yours.
// 3. Rename your old setup()/loop() to e.g. tech_setup()/tech_loop() and call them below.
//
// The linker picks these functions instead of the empty ones in sp_hooks.cpp.
// Rules: never block (the 4 s watchdog resets the board), never call analogRead(),
// don't use PB0, PB1, PC6, PD12, PD13, PA2, PA3 or TIM6. Details: README.md.
#include <Arduino.h>
#include "sp_hooks.h"
#include "protocol.h"

#if defined(STM32_CORE_VERSION_MAJOR) && (STM32_CORE_VERSION_MAJOR >= 3)
static Uart TechRadio(PA10, PA9);              // UART1 -> comm board (free when SP_OWN_RADIO is 0)
#else
static HardwareSerial TechRadio(PA10, PA9);
#endif

// Frames wait here so a slow radio never stalls SnailPulse. Full queue: the oldest is dropped.
static const uint8_t QN = 8;
static uint8_t q[QN][sp::LORA_PACKET], qlen[QN], qhead = 0, qtail = 0, qcount = 0;

// SnailPulse calls this with every frame it builds (SNAIL, BASE, ENV, STATUS; <= 58 bytes).
void sp_frame_out(const uint8_t *frame, uint8_t len) {
  if (qcount == QN) { qtail = (uint8_t)((qtail + 1) % QN); qcount--; }
  memcpy(q[qhead], frame, len);
  qlen[qhead] = len;
  qhead = (uint8_t)((qhead + 1) % QN); qcount++;
}

void sp_ext_setup() {
  TechRadio.begin(115200, SERIAL_8E1);         // IntroSat comm board settings; change for your radio
  // tech_setup();                             // your old setup() code
}

void sp_ext_loop(uint32_t now_ms) {
  // Same rule as the stock comm board: send one frame, wait for 0x06 (max 1.5 s), send the next.
  static uint32_t sent_at = 0;
  static bool waiting = false;
  while (TechRadio.available() > 0) {
    if (TechRadio.read() == 0x06) waiting = false;
  }
  if (waiting && now_ms - sent_at > 1500) waiting = false;
  if (!waiting && qcount) {
    TechRadio.write(q[qtail], qlen[qtail]);    // send the bytes unchanged: one frame = one LoRa packet
    qtail = (uint8_t)((qtail + 1) % QN); qcount--;
    waiting = true; sent_at = now_ms;
  }
  // tech_loop();                              // your old loop() code; must return quickly
}
