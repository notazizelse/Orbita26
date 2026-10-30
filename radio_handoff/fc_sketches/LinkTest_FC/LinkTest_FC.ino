// LinkTest_FC - radio link test on the IntroSat flight controller. No SnailPulse payload board needed.
//
// UPLOAD TO: the flight controller (FC, STM32H750), the same way as the SnailPulse firmware:
//   Tools > Board > STM32 MCU based boards > Generic STM32H7 Series, Board part number: Generic H750VBTx,
//   Upload method: OpenOCD STLink (SWD) or STM32CubeProgrammer (SWD). ST-Link on the FC "PROG" header.
//
// What it does: builds SnailPulse test frames (SNAIL, BASE, ENV, STATUS: real format, made-up values)
// and sends them to the comm board on UART1 (PA9 TX / PA10 RX, 115200 8E1) with exactly the flight
// firmware's rule: one frame, wait for 0x06 (at most 1.5 s), then the next. Up to 8 frames wait in a
// queue; when it is full the oldest is dropped.
// Report on the FC USB-C (UART2, PA3/PA2) at 115200 every 5 s: Serial Monitor, any line ending.
//
// Keys:  n = flight timing (default): SNAIL + BASE every 2 s, ENV every 4 s, STATUS every 10 s
//        f = flood: a new frame as soon as the last one is ACKed (shows the real throughput)
//        x = stop sending        r = reset the counters
//        v = one line per frame on/off (type, seq, ACK time)
//        m = also copy every frame to the USB-C (binary, for ground/snailpulse_monitor.py on this port)
//
// How to recognise the test frames on the ground (snailpulse_monitor.py or snailpulse_ground.py):
//   snail 1 = 30.0 bpm, snail 2 = 45.0 bpm, altitude climbs 1.0 m per BASE frame from 100.0 m,
//   STATUS says fw 1.5 with boot count 0xC0DE (= 49374) and its own sent / no-ACK / dropped counters.
//
// Good result: "no ACK 0", "dropped 0", ACK time steady (with the stock comm board at air rate 2400
// it is the air time of one LoRa packet, a few hundred ms). "no ACK" climbing = the comm board is
// missing, unpowered, on another UART setting, or its firmware does not answer 0x06.
// NEVER power the comm board without its antenna: transmitting without one destroys the E32.

// Stop with one clear message if the IDE is set to another chip (otherwise: dozens of "PA10 not declared").
#if !defined(STM32H750xx)
#error "Wrong board. Tools > Board > STM32 MCU based boards > Generic STM32H7 Series, then Tools > Board part number > Generic H750VBTx"
#endif
#include <Arduino.h>
#include <math.h>
#include <stdarg.h>
#include "snailpulse_frames.h"     // a copy of radio_handoff/snailpulse_frames.h (kept identical by the PC tests)

// STM32 board package 3.x renamed the UART class from HardwareSerial to Uart.
#if defined(STM32_CORE_VERSION_MAJOR) && (STM32_CORE_VERSION_MAJOR >= 3)
Uart UsbSerial(PA3, PA2);
Uart Radio(PA10, PA9);
#else
HardwareSerial UsbSerial(PA3, PA2);
HardwareSerial Radio(PA10, PA9);
#endif

#define PIN_LED          PE10       // FC user LED: on while waiting for an ACK
const uint8_t  ACK_BYTE       = 0x06;
const uint32_t ACK_TIMEOUT_MS = 1500;
const uint32_t REPORT_MS      = 5000;

enum Mode { M_FLIGHT, M_FLOOD, M_STOP };
Mode mode = M_FLIGHT;
bool verbose = false, mirror = false;

// ------------------------------ printing ------------------------------
// printf without %f: the IDE's default C library cannot print floats.
static void P(const char *fmt, ...) {
  char b[200];
  va_list ap; va_start(ap, fmt); vsnprintf(b, sizeof b, fmt, ap); va_end(ap);
  UsbSerial.print(b);
}
static const char *typeName(uint8_t t) {
  return t == SP_PKT_SNAIL ? "SNAIL" : t == SP_PKT_BASE ? "BASE" : t == SP_PKT_ENV ? "ENV" : t == SP_PKT_STATUS ? "STATUS" : "?";
}

// ------------------------------ counters ------------------------------
struct Stats {
  uint32_t built, sent, acked, noAck, dropped, bytes, otherBytes;
  uint32_t ackMin, ackMax, ackSum;
  uint32_t since;
};
Stats st;
uint8_t otherSeen[8]; uint8_t otherN = 0;     // first bytes that were not 0x06 (what is the comm board saying?)
uint16_t allSent = 0, allNoAck = 0, allDropped = 0;   // since boot, for the STATUS frame (like the flight code)

static void resetStats(uint32_t now) { memset(&st, 0, sizeof st); st.ackMin = 0xFFFFFFFF; st.since = now; otherN = 0; }

// ------------------------------ frame queue (same rule as RadioLink in the flight code) ------------------------------
const uint8_t QN = 8;
struct Slot { uint8_t data[SP_LORA_PACKET]; uint8_t len, type, seq; };
Slot q[QN];
uint8_t qHead = 0, qTail = 0, qCount = 0, seqNo = 0;
bool waiting = false;
uint32_t sentAt = 0;
Slot inFlight;

static void queueFrame(uint8_t type, const void *payload, uint8_t len) {
  if (qCount == QN) { qTail = (uint8_t)((qTail + 1) % QN); qCount--; st.dropped++; allDropped++; }
  Slot &s = q[qHead];
  s.type = type; s.seq = seqNo;
  s.len = (uint8_t)sp_build_frame(type, seqNo++, payload, len, s.data);
  qHead = (uint8_t)((qHead + 1) % QN); qCount++;
  st.built++;
}

// ------------------------------ test payloads ------------------------------
uint32_t baseCount = 0;

static void makeSnail(uint32_t now) {
  sp_snail_t s; memset(&s, 0, sizeof s);
  s.t_ms = now;
  s.bpm_x10 = 300; s.last_ibi_ms = 2000; s.beats_total = (uint16_t)(now / 2000);
  s.quality = 90; s.flags = 1; s.amp_counts = 120; s.dc_counts = 2048;
  for (uint8_t i = 0; i < 20; i++)                       // one 30 bpm beat shape over the 2 s window
    s.wave[i] = (uint8_t)(128 + 40 * sinf(6.2832f * (float)i / 20.0f));
  s.wave_n = 20;
  s.max_bpm_x10 = 450; s.max_beats_total = (uint16_t)(now / 1333); s.max_quality = 80;
  s.max_flags = 1; s.max_amp = 200; s.max_ir = 20000;
  queueFrame(SP_PKT_SNAIL, &s, sizeof s);
}
static void makeBase(uint32_t now) {
  sp_base_t b; memset(&b, 0, sizeof b);
  b.t_ms = now; b.press_pa = 101325; b.baro_t_c100 = 2200;
  b.alt_dm = (int32_t)(1000 + 10 * baseCount++);        // 100.0 m, then +1.0 m per frame
  b.acc_mg[2] = 1000; b.mag_mgauss[0] = 200; b.fc_t_x8 = 25 * 8;
  queueFrame(SP_PKT_BASE, &b, sizeof b);
}
static void makeEnv(uint32_t now) {
  sp_env_t e; memset(&e, 0, sizeof e);
  e.t_ms = now;
  e.box_t_c100 = SP_INVALID_I16; e.box_rh_c100 = SP_INVALID_U16;   // like fw 1.5: no SHT21
  e.box_press_pa = 101300; e.bmp_t_c100 = 2150; e.heater_t_c100 = SP_INVALID_I16;
  e.max30100_ir = 20000; e.sensors_ok = 0x76;
  e.dht_t_c100 = 2200; e.dht_rh_c100 = 4500; e.mq7_mv = 1000;
  e.box2_press_pa = 0; e.bmp2_t_c100 = SP_INVALID_I16;
  queueFrame(SP_PKT_ENV, &e, sizeof e);
}
static void makeStatus(uint32_t now) {
  sp_status_t s; memset(&s, 0, sizeof s);
  s.t_ms = now; s.fw_version = 0x0105; s.boot_count = 0xC0DE;   // 0xC0DE marks link-test frames
  s.frames_sent = allSent; s.acks_missed = allNoAck; s.frames_dropped = allDropped;
  s.loop_max_ms = 1; s.fc_ok = 0;
  queueFrame(SP_PKT_STATUS, &s, sizeof s);
}

// ------------------------------ sending ------------------------------
static void pollRadio(uint32_t now) {
  while (Radio.available() > 0) {
    uint8_t b = (uint8_t)Radio.read();
    if (b == ACK_BYTE && waiting) {
      uint32_t dt = now - sentAt;
      waiting = false; st.acked++;
      if (dt < st.ackMin) st.ackMin = dt;
      if (dt > st.ackMax) st.ackMax = dt;
      st.ackSum += dt;
      if (verbose) P("tx %-6s seq %3u %2u B -> ACK after %lu ms\r\n", typeName(inFlight.type), inFlight.seq, inFlight.len, (unsigned long)dt);
    } else {
      st.otherBytes++;
      if (otherN < sizeof otherSeen) otherSeen[otherN++] = b;
    }
  }
  if (waiting && now - sentAt > ACK_TIMEOUT_MS) {
    waiting = false; st.noAck++; allNoAck++;
    if (verbose) P("tx %-6s seq %3u %2u B -> NO ACK within %lu ms\r\n", typeName(inFlight.type), inFlight.seq, inFlight.len, (unsigned long)ACK_TIMEOUT_MS);
  }
  if (!waiting && qCount) {
    inFlight = q[qTail];
    qTail = (uint8_t)((qTail + 1) % QN); qCount--;
    Radio.write(inFlight.data, inFlight.len);            // the bytes unchanged: one frame = one LoRa packet
    if (mirror) UsbSerial.write(inFlight.data, inFlight.len);
    waiting = true; sentAt = now;
    st.sent++; allSent++; st.bytes += inFlight.len;
  }
  digitalWrite(PIN_LED, waiting ? HIGH : LOW);
}

// ------------------------------ report ------------------------------
static void report(uint32_t now) {
  uint32_t secs = (now - st.since) / 1000; if (!secs) secs = 1;
  const char *m = mode == M_FLIGHT ? "flight timing" : mode == M_FLOOD ? "flood" : "stopped";
  P("\r\n[%lus] %s | sent %lu  ACKed %lu  no ACK %lu  dropped %lu  queued %u\r\n", (unsigned long)(now / 1000), m,
    (unsigned long)st.sent, (unsigned long)st.acked, (unsigned long)st.noAck, (unsigned long)st.dropped, qCount);
  if (st.acked)
    P("        ACK time min %lu / avg %lu / max %lu ms | %lu bytes/s, %lu.%lu frames/s\r\n",
      (unsigned long)st.ackMin, (unsigned long)(st.ackSum / st.acked), (unsigned long)st.ackMax,
      (unsigned long)(st.bytes / secs), (unsigned long)(st.acked * 10 / secs / 10), (unsigned long)(st.acked * 10 / secs % 10));
  else if (st.sent)
    P("        no ACK yet: comm board powered and in the stack? Its UART at 115200 8E1? Antenna on?\r\n");
  if (otherN) {
    P("        %lu other bytes from the comm board, first:", (unsigned long)st.otherBytes);
    for (uint8_t i = 0; i < otherN; i++) P(" %02X", otherSeen[i]);
    P("\r\n");
  }
}

static void help() {
  P("\r\n== LinkTest_FC: SnailPulse test frames -> UART1 (PA9/PA10, 115200 8E1) -> comm board ==\r\n");
  P("Keys: n flight timing  f flood  x stop  r reset counters  v per-frame lines  m mirror frames to USB-C\r\n");
}

// ------------------------------ main ------------------------------
void setup() {
  pinMode(PIN_LED, OUTPUT);
  UsbSerial.begin(115200);
  Radio.begin(115200, SERIAL_8E1);        // IntroSat comm board UART settings
  for (uint8_t i = 0; i < 20; i++) { digitalWrite(PIN_LED, i & 1); delay(100); }   // 2 s: open the monitor
  help();
  resetStats(millis());
  makeStatus(millis());                   // like the flight firmware: STATUS once at boot
}

void loop() {
  static uint32_t tSnail = 0, tEnv = 0, tStatus = 0, tReport = 0;
  static uint8_t floodNext = 0;
  uint32_t now = millis();

  while (UsbSerial.available()) {
    char c = (char)UsbSerial.read();
    if (c == 'n' || c == 'N') { mode = M_FLIGHT; P("flight timing\r\n"); }
    else if (c == 'f' || c == 'F') { mode = M_FLOOD; P("flood: next frame right after each ACK\r\n"); }
    else if (c == 'x' || c == 'X') { mode = M_STOP; P("stopped\r\n"); }
    else if (c == 'r' || c == 'R') { resetStats(now); P("counters reset\r\n"); }
    else if (c == 'v' || c == 'V') { verbose = !verbose; P("per-frame lines %s\r\n", verbose ? "on" : "off"); }
    else if (c == 'm' || c == 'M') { mirror = !mirror; P("mirror to USB-C %s\r\n", mirror ? "on" : "off"); }
    else if (c == 'h' || c == 'H' || c == '?') help();
  }

  if (mode == M_FLIGHT) {
    if (now - tSnail >= 2000) { tSnail = now; makeSnail(now); makeBase(now); }
    if (now - tEnv >= 4000) { tEnv = now; makeEnv(now); }
    if (now - tStatus >= 10000) { tStatus = now; makeStatus(now); }
  } else if (mode == M_FLOOD && qCount == 0 && !waiting) {
    switch (floodNext++ % 4) {            // the same mix of frame sizes as in flight
      case 0: makeSnail(now); break;
      case 1: makeBase(now); break;
      case 2: makeEnv(now); break;
      default: makeStatus(now); break;
    }
  }

  pollRadio(now);
  if (now - tReport >= REPORT_MS) { tReport = now; report(now); }
}
