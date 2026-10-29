// Hooks for code that is merged INTO this sketch, e.g. the radio technician's module.
//
// sp_hooks.cpp has empty default versions. To use a hook, define a function with the same
// name and signature in your own .cpp file inside this sketch folder: the linker then uses
// yours instead of the empty one. No change to SnailPulse_Flight.ino is needed.
// See radio_handoff/README.md.
#pragma once
#include <stdint.h>

// Called once at the end of setup(), before the 4 s watchdog starts.
void sp_ext_setup();

// Called on every loop() pass (a few hundred times a second). Must return quickly:
// the watchdog resets the board if one loop pass takes longer than 4 s.
void sp_ext_loop(uint32_t now_ms);

// Called with every SnailPulse frame the moment it is built (SNAIL, BASE, ENV, STATUS).
// frame = AA 55 type seq len payload crc16, len <= 58 bytes = exactly one E32 LoRa packet.
// The bytes are only valid during the call: copy them if you queue them.
// With SP_OWN_RADIO 1 this is extra (our code already sends to the comm board);
// with SP_OWN_RADIO 0 this is the ONLY way our data leaves the FC.
void sp_frame_out(const uint8_t *frame, uint8_t len);
