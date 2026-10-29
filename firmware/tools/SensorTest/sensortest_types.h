// Types used by SensorTest.ino. They live in a header so the Arduino IDE's auto-generated
// function prototypes (inserted near the top of the .ino) can already see them.
#pragma once
#include <Arduino.h>

// STM32 board package 3.x renamed the UART class from HardwareSerial to Uart.
#if defined(STM32_CORE_VERSION_MAJOR) && (STM32_CORE_VERSION_MAJOR >= 3)
typedef Uart FcSerial;
#else
typedef HardwareSerial FcSerial;
#endif

enum Res : uint8_t { R_PASS, R_WARN, R_FAIL };
struct Row { const char *name; Res r; char info[80]; };
struct BmpCal { uint16_t T1; int16_t T2, T3; uint16_t P1; int16_t P2, P3, P4, P5, P6, P7, P8, P9; };
enum Mode : uint8_t { M_LIVE, M_PLOT, M_IDLE };
