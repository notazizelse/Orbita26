// Empty default hooks (weak: a definition in any other .cpp of this sketch replaces them).
#include "sp_hooks.h"

__attribute__((weak)) void sp_ext_setup() {}
__attribute__((weak)) void sp_ext_loop(uint32_t now_ms) { (void)now_ms; }
__attribute__((weak)) void sp_frame_out(const uint8_t *frame, uint8_t len) { (void)frame; (void)len; }
