// CommBridge_FC - talk to the IntroSat comm board from a laptop, through the flight controller.
//
// UPLOAD TO: the flight controller (FC, STM32H750), same settings as the SnailPulse firmware:
//   Tools > Board > STM32 MCU based boards > Generic STM32H7 Series, Board part number: Generic H750VBTx,
//   Upload method: OpenOCD STLink (SWD) or STM32CubeProgrammer (SWD).
//
// It joins two serial ports:
//   laptop <-USB-C (UART2, PA3/PA2, 115200 8N1)-> FC <-UART1 (PA9 TX / PA10 RX, 115200 8E1)-> comm board
// Everything you type in the Serial Monitor goes to the comm board unchanged (with the line ending
// chosen at the bottom of the Serial Monitor). Everything the comm board sends comes back: printable
// text as it is, other bytes as <XX> in hex, so the 0x06 ACK shows up as <06>.
//
// Use it to:
//   * read or change the comm board's radio settings, if its firmware takes commands on UART1. The
//     IntroSat guide names AT+LCHANNEL (channel, 410 + C MHz), AT+LAIRRATE (air rate) and AT+LTXPWR
//     (transmit power). Check the guide for the exact syntax and line ending.
//   * see whether the comm board answers at all, and what it sends after a frame (0x06 = done).
//   * ~f sends one SnailPulse test frame (an ENV frame, 43 bytes) to check the ACK by hand.
//
// Keys that stay on the FC (start with ~ so normal text is never caught):
//   ~f = send one test frame     ~h = this help
// Antenna on the comm board BEFORE it is powered: transmitting without one destroys the E32.

#if !defined(STM32H750xx)
#error "Wrong board. Tools > Board > STM32 MCU based boards > Generic STM32H7 Series, then Tools > Board part number > Generic H750VBTx"
#endif
#include <Arduino.h>
#include "snailpulse_frames.h"     // a copy of radio_handoff/snailpulse_frames.h (kept identical by the PC tests)

#if defined(STM32_CORE_VERSION_MAJOR) && (STM32_CORE_VERSION_MAJOR >= 3)
Uart UsbSerial(PA3, PA2);
Uart Radio(PA10, PA9);
#else
HardwareSerial UsbSerial(PA3, PA2);
HardwareSerial Radio(PA10, PA9);
#endif

#define PIN_LED PE10                // flashes on every byte from the comm board

uint8_t seqNo = 0;
uint32_t ledOffAt = 0;

static void help() {
  UsbSerial.print("\r\n== CommBridge_FC: USB-C <-> UART1 (PA9/PA10, 115200 8E1) <-> comm board ==\r\n");
  UsbSerial.print("Typed text goes to the comm board. Its replies come back; non-text bytes as <XX> (ACK = <06>).\r\n");
  UsbSerial.print("~f = send one SnailPulse test frame   ~h = help\r\n");
}

static void sendTestFrame() {
  sp_env_t e; memset(&e, 0, sizeof e);
  e.t_ms = millis();
  e.box_t_c100 = SP_INVALID_I16; e.box_rh_c100 = SP_INVALID_U16;
  e.box_press_pa = 101300; e.bmp_t_c100 = 2150; e.heater_t_c100 = SP_INVALID_I16;
  e.sensors_ok = 0x76; e.dht_t_c100 = 2200; e.dht_rh_c100 = 4500; e.mq7_mv = 1000; e.bmp2_t_c100 = SP_INVALID_I16;
  uint8_t f[SP_LORA_PACKET];
  size_t n = sp_build_frame(SP_PKT_ENV, seqNo++, &e, sizeof e, f);
  Radio.write(f, n);
  UsbSerial.print("\r\n[sent one ENV test frame, ");
  UsbSerial.print((unsigned)n);
  UsbSerial.print(" bytes: expect <06> when it has gone out]\r\n");
}

void setup() {
  pinMode(PIN_LED, OUTPUT);
  UsbSerial.begin(115200);
  Radio.begin(115200, SERIAL_8E1);  // IntroSat comm board UART settings
  for (uint8_t i = 0; i < 20; i++) { digitalWrite(PIN_LED, i & 1); delay(100); }   // 2 s: open the monitor
  digitalWrite(PIN_LED, LOW);
  help();
}

void loop() {
  static bool tilde = false;
  while (UsbSerial.available()) {   // laptop -> comm board
    uint8_t c = (uint8_t)UsbSerial.read();
    if (tilde) {
      tilde = false;
      if (c == 'f' || c == 'F') { sendTestFrame(); continue; }
      if (c == 'h' || c == 'H') { help(); continue; }
      Radio.write('~');             // not one of ours: pass the ~ on
    }
    if (c == '~') { tilde = true; continue; }
    Radio.write(c);
  }
  while (Radio.available()) {       // comm board -> laptop
    uint8_t c = (uint8_t)Radio.read();
    if ((c >= 0x20 && c < 0x7F) || c == '\r' || c == '\n' || c == '\t') {
      UsbSerial.write(c);
    } else {
      char b[6];
      snprintf(b, sizeof b, "<%02X>", c);
      UsbSerial.print(b);
    }
    digitalWrite(PIN_LED, HIGH); ledOffAt = millis() + 50;
  }
  if (ledOffAt && (int32_t)(millis() - ledOffAt) >= 0) { digitalWrite(PIN_LED, LOW); ledOffAt = 0; }
}
