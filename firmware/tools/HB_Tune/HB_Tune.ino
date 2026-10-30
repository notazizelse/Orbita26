// HB_Tune - bench tool for setting up the SNAIL 1 pulse sensor (TCRT5000 -> amplifier -> PB0).
// UPLOAD TO: the IntroSat flight controller (Generic STM32H7 Series -> Generic H750VBTx, ST-Link SWD).
// Streams snail 1 (HB_ADC, PB0) and the MQ-7 output (PB1) over the FCM USB Type-C port
// (UART2, PA3/PA2) at 50 Hz. Open Tools -> Serial Plotter at 115200.
//
// How to tune RV3 (50k phototransistor load):
//   1. Glue/hold the TCRT5000 over the snail's heart (shell, just behind the mantle edge).
//   2. The emitter (SENSE, J1 pin 4) is NOT wired to the MCU - measure it with a multimeter
//      to GND and turn RV3 until it reads ~0.8-2.0 V. Close to 3 V = phototransistor
//      saturated (RV3 too big); a few mV = RV3 too small / sensor not seeing the shell.
//   3. Wait ~5 s after each adjustment (the 1 uF x 1 M high-pass needs ~5 time constants).
//   4. "snail1_tcrt5000" should then show a slow, regular wave every 1-10 s around "snail1_dc".
//      snail1_dc = VREF + 101 x op-amp offset: anywhere ~1500-2600 is normal.
//   Flags: snail1_tcrt5000 stuck near 0 or 4095 = amplifier clipping (offset or too much light).
//
// 29 Oct: rev C has no heater. PC6 is now the DHT-11 data line and PB1 the MQ-7 AO, so this
// sketch leaves PC6 alone (it used to hold it LOW, which blocks the DHT-11) and plots the
// MQ-7 voltage instead of the old heater thermistor. For a full sensor check use SensorTest.
// Snail 2 (MAX30100 on I2C4) is not shown here: use SensorTest, key 'm'.
// 30 Oct: the MQ-7 runs on +5 V with a 10k/20k divider before PB1; mq7_mV is scaled back to the module.
#define MQ7_DIV_X100  150      // (10k + 20k) / 20k = 1.5. No divider: 100
// Stop with one clear message if the IDE is set to another chip (otherwise: dozens of "PA10 not declared").
#if !defined(STM32H750xx)
#error "Wrong board. Tools > Board > STM32 MCU based boards > Generic STM32H7 Series, then Tools > Board part number > Generic H750VBTx"
#endif
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
  UsbSerial.print("snail1_tcrt5000:"); UsbSerial.print(hb);
  UsbSerial.print(",vref:"); UsbSerial.print((int)VREF_COUNTS);
  UsbSerial.print(",snail1_dc:"); UsbSerial.print((int)dc);
  UsbSerial.print(",mq7_mV:"); UsbSerial.println((int)(mq7 * 3300L * MQ7_DIV_X100 / 100 / 4095));   // at the module
}
