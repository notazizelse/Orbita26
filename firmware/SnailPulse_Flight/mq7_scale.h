// MQ-7 voltage maths (no Arduino, unit-tested in tests/test_host.cpp).
// On a 5 V supply the MQ-7 AO swings up to 5 V, but PB1 is not 5 V-tolerant as an analog input.
// A divider in the AO wire (top resistor AO -> PB1, bottom resistor PB1 -> GND) scales it down;
// the firmware scales the reading back up, so mq7_mv is always the voltage AT THE MODULE.
#pragma once
#include <stdint.h>

// Average ADC count at PB1 -> PB1 voltage in mV.
inline uint32_t mq7_pb1_mv(uint32_t adc_avg, uint32_t adc_full, uint32_t vref_mv) {
  return adc_full ? adc_avg * vref_mv / adc_full : 0;
}

// PB1 voltage -> AO voltage at the module, in mV (top_ohm = 0: no divider). Clamped below 0xFFFF,
// which the ENV packet reserves for "MQ-7 disabled".
inline uint16_t mq7_ao_mv(uint32_t pb1_mv, uint32_t top_ohm, uint32_t bot_ohm) {
  uint32_t mv = (top_ohm && bot_ohm) ? (uint32_t)((uint64_t)pb1_mv * (top_ohm + bot_ohm) / bot_ohm) : pb1_mv;
  return (uint16_t)(mv > 0xFFFE ? 0xFFFE : mv);
}

// Usable reading? Judged at PB1: ~0 V = AO wire off or no power; >= 3.29 V = ADC at full scale
// (gas level off the scale, or the divider is missing).
inline bool mq7_pb1_ok(uint32_t pb1_mv) { return pb1_mv > 5 && pb1_mv < 3290; }
