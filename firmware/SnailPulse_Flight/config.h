// SnailPulse flight firmware - configuration
// Board: IntroSat.Platform Flight Controller Module (STM32H750VBT6)
// Arduino IDE: STM32 MCU based boards -> Generic STM32H7 Series -> Generic H750VBTx
#pragma once

#define SP_FW_VERSION 0x0103          // v1.3 = two snails: TCRT5000 (snail 1) + MAX30100 (snail 2), SNAIL 49 B;
                                      //        BMP280 #2 removed (30 Oct)
                                      // v1.2 = rev C, no heater, + DHT-11 / MQ-7 / second BMP280 (29 Oct)

// ---------------- Pins (IntroBus_L, see IntroSat.Platform guide) ----------------
#define PIN_HB_ADC      PB0           // J6.1  heartbeat amplifier output (HB_ADC)
#define PIN_MQ7_AO      PB1           // J6.2  MQ-7 module AO (CO gas), analog. Module VCC on +3.3 V
#define PIN_DHT11       PC6           // J6.3  DHT-11 DATA (one-wire, pull-up on the module)
#define PIN_NTC1_ADC    PB1           // rev B only: heater thermistor (same pin as the MQ-7)
#define PIN_HEATER_PWM  PC6           // rev B only: -> R11 1k -> IRLZ44N gate (same pin as the DHT-11)
#define PIN_I2C4_SDA    PD13          // J6.5  payload I2C bus (SHT21, BMP280, MAX30100)
#define PIN_I2C4_SCL    PD12          // J6.4
#define PIN_I2C1_SDA    PB7           // service bus (FC sensors) - read only for us
#define PIN_I2C1_SCL    PB6
#define PIN_RADIO_RX    PA10          // UART1 -> communication board (LoRa E32-433T30D)
#define PIN_RADIO_TX    PA9
#define PIN_USER_LED    PE10
#define PIN_SD_DETECT   PB8

// ---------------- Radio (UART1 -> comm board -> LoRa) ----------------
#define RADIO_BAUD        115200      // FCM <-> comm board UART (guide: 115200, 8E1)
#define RADIO_ACK_BYTE    0x06        // comm board answers ACK after each transmitted block
#define RADIO_ACK_TIMEOUT_MS 1500     // if no ACK, send the next frame anyway

// 1 = this firmware drives UART1 -> IntroSat comm board itself (normal).
// 0 = someone else's code owns the radio: we never touch UART1 and hand every frame to
//     sp_frame_out() instead (see sp_hooks.h and radio_handoff/README.md).
#define SP_OWN_RADIO      1

// 1 = also write every frame to the FC's USB-C (UART2, PA3/PA2, CH340X) at 115200, so
// ground/snailpulse_monitor.py can show all sensors on the bench with no radio at all.
// Harmless in flight (nothing listens; ~55 bytes/s).
#define SP_USB_MIRROR     1
#define USB_BAUD          115200

// Telemetry periods (ms). Air rate 2.4 kbit/s -> keep average well below ~250 B/s
#define PERIOD_SNAIL_MS   2000        // heartbeat packet (20 waveform samples @10 Hz = 2 s)
#define PERIOD_BASE_MS    2000        // FC service sensors
#define PERIOD_ENV_MS     4000        // box environment (SHT21, BMP280, DHT-11, MQ-7, MAX30100 IR level)
#define PERIOD_STATUS_MS  10000       // health / counters

// ---------------- Heartbeat ----------------
#define HB_SAMPLE_HZ      50          // ADC sampling rate of HB_ADC
#define HB_WAVE_DECIM     5           // 50 Hz / 5 = 10 Hz waveform sent to ground
#define ADC_BITS          12
// MAX30100 (second pulse sensor, optical): 100 Hz into a 16-sample FIFO that is full after
// 160 ms, so empty it every 40 ms. Thresholds: optical_pulse.h.
#define MAX30100_POLL_MS  40
#define MAX30100_RETRY_MS 5000        // not answering: try begin() again this often

// ---------------- Thermistors / heater ----------------
#define NTC_R_FIXED       10000.0f    // R13 (top resistor to 3V3)
#define NTC_R25           10000.0f    // MF58 10k
#define NTC_BETA          3950.0f
#define BOX_TARGET_C      20.0f       // snail box air target (SHT21)
#define HEATER_MAX_C      35.0f       // software limit on heater surface (NTC1); HW cutoff ~40 C
#define HEATER_PWM_HZ     1000

// Rev C (24 Oct 2026): SnailPulse has NO heater. 0 = PB1 and PC6 are never touched.
// Set to 1 only if a heater board is wired to PB1 (NTC) and PC6 (MOSFET gate).
#define SP_HEATER_ENABLED 0

// ---------------- DHT-11 + MQ-7 (added 29 Oct 2026) ----------------
#define SP_HAS_DHT11      1           // 0 = PC6 never touched
#define SP_HAS_MQ7        1           // 0 = PB1 never sampled
#define SP_HAS_BMP280_2   0           // second BMP280 at 0x77: removed 30 Oct. 0 = ENV sends its fields as invalid
#define DHT11_PERIOD_MS   2000        // DHT-11 allows at most one reading per second
// MQ-7 module VCC is +3.3 V: heater at ~45 % power, so mq7_mv shows changes, not calibrated ppm.
// On 5 V its AO needs a divider (10k AO->PB1, 20k PB1->GND) before PB1.

#if SP_HEATER_ENABLED && (SP_HAS_DHT11 || SP_HAS_MQ7)
#error "The heater uses PB1 and PC6, which are now the MQ-7 and the DHT-11. Enable only one of them."
#endif

// ---------------- I2C addresses ----------------
#define ADDR_SHT21        0x40
#define ADDR_BMP280       0x76        // BMP280: SDO tied to GND on J8.5
#define ADDR_BMP280_B     0x77        // second BMP280 (SDO -> VCC), only used if SP_HAS_BMP280_2 = 1
#define ADDR_MAX30100     0x57
#define ADDR_LSM6DS3      0x6A
#define ADDR_LIS3MDL      0x1C
#define ADDR_MS5611       0x77
#define ADDR_LM75A_FCM    0x4A
