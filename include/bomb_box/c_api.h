#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stable, primitive-only boundary for WebAssembly and other foreign-function callers.
// Returned strings have static storage and must not be freed by the caller.
uint32_t bomb_box_api_version(void);
const char* bomb_box_engine_status(void);

#ifdef __cplusplus
} // extern "C"
#endif

