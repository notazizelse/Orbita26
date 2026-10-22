// HB_Tune - bench tool for setting up the SnailPulse sensor before flight.
// Streams HB_ADC (PB0) and the heater thermistor (PB1) over the FCM USB Type-C port
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
#include <Arduino.h>

HardwareSerial UsbSerial(PA3, PA2);
const float VREF_COUNTS = 2048;

void setup() {
  pinMode(PC6, OUTPUT); digitalWrite(PC6, LOW);   // keep the heater off on the bench
  UsbSerial.begin(115200);
  analogReadResolution(12);
}

void loop() {
  static uint32_t t = 0;
  static float dc = 2048;
  if (millis() - t < 20) return;
  t = millis();
  int hb = analogRead(PB0);
  int ntc = analogRead(PB1);
  dc += 0.01f * (hb - dc);
  float ratio = ntc / 4095.0f;
  float r = 10000.0f * ratio / (1.0f - ratio);
  float tc = 1.0f / (1.0f / 298.15f + logf(r / 10000.0f) / 3950.0f) - 273.15f;
  UsbSerial.print("hb:"); UsbSerial.print(hb);
  UsbSerial.print(",vref:"); UsbSerial.print((int)VREF_COUNTS);
  UsbSerial.print(",hb_dc:"); UsbSerial.print((int)dc);
  UsbSerial.print(",heater_C_x10:"); UsbSerial.println((int)(tc * 10));
}
