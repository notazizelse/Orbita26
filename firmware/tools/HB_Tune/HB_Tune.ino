// HB_Tune - bench tool for setting up the SnailPulse sensor before flight.
// UPLOAD TO: the IntroSat flight controller (Generic STM32H7 -> STM32H750VBTx, ST-Link SWD).
// Streams HB_ADC (PB0) and the MQ-7 output (PB1) over the FCM USB Type-C port
// (UART2, PA3/PA2) at 50 Hz. Open Tools -> Serial Plotter at 115200.
//
// How to tune RV3 (50k phototransistor load):
//   1. Glue/hold the TCRT5000 over the snail's heart (shell, just behind the mantle edge).
//   2. The emitter (SENSE, J1 pin 4) is NOT wired to the MCU - measure it with a multimeter
//      to GND and turn RV3 until it reads ~0.8-2.0 V. Close to 3 V = phototransistor
//      saturated (RV3 too big); a few mV = RV3 too small / sensor not seeing the shell.
//   3. Wait ~5 s after each adjustment (the 1 uF x 1 M high-pass needs ~5 time constants).
//   4. "hb" should then show a slow, regular wave every 1-10 s around "hb_dc".
//      hb_dc = VREF + 101 x op-amp offset: anywhere ~1500-2600 is normal.
//   Flags: hb stuck near 0 or 4095 = amplifier clipping (offset or too much light).
//
// 29 Oct: rev C has no heater. PC6 is now the DHT-11 data line and PB1 the MQ-7 AO, so this
// sketch leaves PC6 alone (it used to hold it LOW, which blocks the DHT-11) and plots the
// MQ-7 voltage instead of the old heater thermistor. For a full sensor check use SensorTest.
#include <Arduino.h>

// STM32 board package 3.x renamed the UART class from HardwareSerial to Uart.
#if defined(STM32_CORE_VERSION_MAJOR) && (STM32_CORE_VERSION_MAJOR >= 3)
Uart UsbSerial(PA3, PA2);
#else
HardwareSerial UsbSerial(PA3, PA2);
#endif
const float VREF_COUNTS = 2048;

void setup() {
  pinMode(PC6, INPUT_PULLUP);   // DHT-11 data: never drive it
  UsbSerial.begin(115200);
  analogReadResolution(12);
}

void loop() {
  static uint32_t t = 0;
  static float dc = 2048;
  if (millis() - t < 20) return;
  t = millis();
  int hb = analogRead(PB0);
  int mq7 = analogRead(PB1);
  dc += 0.01f * (hb - dc);
  UsbSerial.print("hb:"); UsbSerial.print(hb);
  UsbSerial.print(",vref:"); UsbSerial.print((int)VREF_COUNTS);
  UsbSerial.print(",hb_dc:"); UsbSerial.print((int)dc);
  UsbSerial.print(",mq7_mV:"); UsbSerial.println((int)(mq7 * 3300L / 4095));
}
