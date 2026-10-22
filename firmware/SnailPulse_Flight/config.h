// SnailPulse flight firmware - configuration
// Board: IntroSat.Platform Flight Controller Module (STM32H750VBT6)
// Arduino IDE: STM32 MCU based boards -> Generic STM32H7 -> STM32H750VBTx
#pragma once

#define SP_FW_VERSION 0x0100          // v1.0 (high byte = major, low = minor)

// ---------------- Pins (IntroBus_L, see IntroSat.Platform guide) ----------------
#define PIN_HB_ADC      PB0           // J6.1  heartbeat amplifier output (HB_ADC)
#define PIN_NTC1_ADC    PB1           // J6.2  heater thermistor divider (NTC1_ADC)
#define PIN_HEATER_PWM  PC6           // J6.3  -> R11 1k -> IRLZ44N gate
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

// Telemetry periods (ms). Air rate 2.4 kbit/s -> keep average well below ~250 B/s
#define PERIOD_SNAIL_MS   2000        // heartbeat packet (20 waveform samples @10 Hz = 2 s)
#define PERIOD_BASE_MS    2000        // FC service sensors
#define PERIOD_ENV_MS     4000        // box environment + heater
#define PERIOD_STATUS_MS  10000       // health / counters

// ---------------- Heartbeat ----------------
#define HB_SAMPLE_HZ      50          // ADC sampling rate of HB_ADC
#define HB_WAVE_DECIM     5           // 50 Hz / 5 = 10 Hz waveform sent to ground
#define ADC_BITS          12

// ---------------- Thermistors / heater ----------------
#define NTC_R_FIXED       10000.0f    // R13 (top resistor to 3V3)
#define NTC_R25           10000.0f    // MF58 10k
#define NTC_BETA          3950.0f
#define BOX_TARGET_C      20.0f       // snail box air target (SHT21)
#define HEATER_MAX_C      35.0f       // software limit on heater surface (NTC1); HW cutoff ~40 C
#define HEATER_PWM_HZ     1000

// ---------------- I2C addresses ----------------
#define ADDR_SHT21        0x40
#define ADDR_BMP280       0x76        // SDO tied to GND on J8.5
#define ADDR_MAX30100     0x57
#define ADDR_LSM6DS3      0x6A
#define ADDR_LIS3MDL      0x1C
#define ADDR_MS5611       0x77
#define ADDR_LM75A_FCM    0x4A
