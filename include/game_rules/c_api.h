#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Stable, primitive-only boundary for WebAssembly and other foreign-function callers.
uint32_t game_rules_api_version(void);

// This legacy capability string has static storage and must not be freed.
const char* game_rules_engine_status(void);

// Opaque engine instances own all mutable level, state, and rewind history.
typedef struct game_rules_engine game_rules_engine;

enum {
    GAME_RULES_DIRECTION_NORTH = 0,
    GAME_RULES_DIRECTION_EAST = 1,
    GAME_RULES_DIRECTION_SOUTH = 2,
    GAME_RULES_DIRECTION_WEST = 3,
};

// Creation returns NULL only when the instance cannot be allocated. Destruction accepts NULL.
game_rules_engine* game_rules_engine_create(void);
void game_rules_engine_destroy(game_rules_engine* engine);

// Every non-NULL string returned by these operations is a UTF-8 JSON document owned by the
// caller. Release it exactly once with game_rules_string_free(). A NULL return means response
// memory could not be allocated. Passing a NULL engine produces an invalid_engine JSON response.
char* game_rules_engine_load_level(game_rules_engine* engine,
                                   const char* level_json,
                                   uint32_t level_json_length);
char* game_rules_engine_get_state(game_rules_engine* engine);
char* game_rules_engine_move(game_rules_engine* engine, uint32_t direction);
char* game_rules_engine_rewind(game_rules_engine* engine);
void game_rules_string_free(char* result);

#ifdef __cplusplus
} // extern "C"
#endif
