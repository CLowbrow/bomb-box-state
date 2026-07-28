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

// Typed data ABI for native foreign-function callers. This version is independent of the
// legacy JSON response version returned by game_rules_api_version(). All enum-like values and
// counts have an explicit 32-bit representation.
uint32_t game_rules_data_api_version(void);

enum {
    GAME_RULES_CALL_OK = 0,
    GAME_RULES_CALL_INVALID_ENGINE = 1,
    GAME_RULES_CALL_INVALID_ARGUMENT = 2,
    GAME_RULES_CALL_ALLOCATION_FAILED = 3,
};

enum {
    GAME_RULES_HORIZONTAL_EAST = 0,
    GAME_RULES_HORIZONTAL_WEST = 1,
};

enum {
    GAME_RULES_VERTICAL_NORTH = 0,
    GAME_RULES_VERTICAL_SOUTH = 1,
};

enum {
    GAME_RULES_CELL_FLAT = 0,
    GAME_RULES_CELL_RAMP = 1,
};

enum {
    GAME_RULES_FIXTURE_SWITCH = 0,
    GAME_RULES_FIXTURE_DOOR = 1,
    GAME_RULES_FIXTURE_EXIT = 2,
};

enum {
    GAME_RULES_ENTITY_PLAYER = 0,
    GAME_RULES_ENTITY_BOX = 1,
    GAME_RULES_ENTITY_BARREL = 2,
};

enum {
    GAME_RULES_COLOR_RED = 0,
    GAME_RULES_COLOR_GREEN = 1,
    GAME_RULES_COLOR_BLUE = 2,
    GAME_RULES_COLOR_YELLOW = 3,
};

enum {
    GAME_RULES_OUTCOME_ONGOING = 0,
    GAME_RULES_OUTCOME_WON = 1,
    GAME_RULES_OUTCOME_LOST = 2,
};

enum {
    GAME_RULES_LOAD_LOADED = 0,
    GAME_RULES_LOAD_INVALID_LEVEL = 1,
};

enum {
    GAME_RULES_MOVE_MOVED = 0,
    GAME_RULES_MOVE_NO_LEVEL = 1,
    GAME_RULES_MOVE_INVALID_DIRECTION = 2,
    GAME_RULES_MOVE_WORLD_BOUNDARY = 3,
    GAME_RULES_MOVE_LEDGE = 4,
    GAME_RULES_MOVE_OCCUPIED = 5,
    GAME_RULES_MOVE_STACKED_PUSH_TARGET = 6,
    GAME_RULES_MOVE_CLOSED_DOOR = 7,
    GAME_RULES_MOVE_TELEPORTER_RESTRICTION = 8,
    GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY = 9,
    GAME_RULES_MOVE_LEVEL_TERMINAL = 10,
};

enum {
    GAME_RULES_REWIND_REWOUND = 0,
    GAME_RULES_REWIND_HISTORY_EMPTY = 1,
};

enum {
    GAME_RULES_VALIDATION_INVALID_DIMENSIONS = 0,
    GAME_RULES_VALIDATION_INVALID_COORDINATE_SYSTEM = 1,
    GAME_RULES_VALIDATION_CELL_COUNT_MISMATCH = 2,
    GAME_RULES_VALIDATION_CELL_OUT_OF_BOUNDS = 3,
    GAME_RULES_VALIDATION_DUPLICATE_CELL = 4,
    GAME_RULES_VALIDATION_INVALID_CELL_HEIGHT = 5,
    GAME_RULES_VALIDATION_INVALID_RAMP_DIRECTION = 6,
    GAME_RULES_VALIDATION_INVALID_RAMP_ENDPOINTS = 7,
    GAME_RULES_VALIDATION_FIXTURE_OUT_OF_BOUNDS = 8,
    GAME_RULES_VALIDATION_FIXTURE_ON_RAMP = 9,
    GAME_RULES_VALIDATION_DUPLICATE_FIXTURE = 10,
    GAME_RULES_VALIDATION_INVALID_FIXTURE_COLOR = 11,
    GAME_RULES_VALIDATION_ENTITY_OUT_OF_BOUNDS = 12,
    GAME_RULES_VALIDATION_DUPLICATE_ENTITY_ID = 13,
    GAME_RULES_VALIDATION_INVALID_ENTITY_KIND = 14,
    GAME_RULES_VALIDATION_ENTITY_BELOW_SURFACE = 15,
    GAME_RULES_VALIDATION_OVERLAPPING_ENTITIES = 16,
    GAME_RULES_VALIDATION_PLAYER_NOT_TOP_OF_STACK = 17,
    GAME_RULES_VALIDATION_PLAYER_COUNT_NOT_ONE = 18,
    GAME_RULES_VALIDATION_INVALID_TELEPORTER_OCCUPANCY = 19,
    GAME_RULES_VALIDATION_INVALID_ENTITY_ID = 20,
};

enum {
    GAME_RULES_EVENT_MOVE_BLOCKED = 0,
    GAME_RULES_EVENT_STATE_REWOUND = 1,
    GAME_RULES_EVENT_ENTITY_MOVED = 2,
    GAME_RULES_EVENT_BARREL_ARMED = 3,
    GAME_RULES_EVENT_BARREL_EXPLODED = 4,
    GAME_RULES_EVENT_PLAYER_CRUSHED = 5,
    GAME_RULES_EVENT_SWITCH_CHANGED = 6,
    GAME_RULES_EVENT_DOOR_OPENED = 7,
    GAME_RULES_EVENT_DOOR_CLOSED = 8,
    GAME_RULES_EVENT_LEVEL_WON = 9,
    GAME_RULES_EVENT_LEVEL_LOST = 10,
};

enum {
    GAME_RULES_MOVEMENT_PLAYER = 0,
    GAME_RULES_MOVEMENT_BLAST = 1,
    GAME_RULES_MOVEMENT_FALL = 2,
    GAME_RULES_MOVEMENT_SLIDE = 3,
};

typedef struct game_rules_coordinate {
    int32_t x;
    int32_t y;
} game_rules_coordinate;

typedef struct game_rules_coordinate_system {
    game_rules_coordinate origin;
    uint32_t positive_x;
    uint32_t positive_y;
} game_rules_coordinate_system;

// For a flat cell, elevation is used and low_direction is ignored. For a ramp, elevation is the
// low elevation and low_direction identifies its low end.
typedef struct game_rules_cell {
    game_rules_coordinate coordinate;
    uint32_t kind;
    int32_t elevation;
    uint32_t low_direction;
} game_rules_cell;

// color is ignored for GAME_RULES_FIXTURE_EXIT.
typedef struct game_rules_fixture {
    game_rules_coordinate coordinate;
    uint32_t kind;
    uint32_t color;
} game_rules_fixture;

typedef struct game_rules_entity {
    uint64_t id;
    uint32_t kind;
    game_rules_coordinate coordinate;
    int32_t bottom_half_steps;
} game_rules_entity;

// Input arrays are borrowed only for the duration of game_rules_engine_load_level_data(); the
// engine copies accepted level data. A count of zero permits a NULL pointer.
typedef struct game_rules_level_definition {
    game_rules_coordinate_system coordinates;
    uint32_t width;
    uint32_t height;
    const game_rules_cell* cells;
    uint32_t cell_count;
    const game_rules_fixture* fixtures;
    uint32_t fixture_count;
    const game_rules_entity* entities;
    uint32_t entity_count;
} game_rules_level_definition;

typedef struct game_rules_validation_error {
    uint32_t code;
    game_rules_coordinate coordinate;
    uint64_t entity_id;
} game_rules_validation_error;

typedef struct game_rules_resolved_state {
    const game_rules_entity* entities;
    uint32_t entity_count;
    const uint64_t* armed_barrel_ids;
    uint32_t armed_barrel_count;
    const uint32_t* active_switch_colors;
    uint32_t active_switch_color_count;
    const game_rules_coordinate* open_doors;
    uint32_t open_door_count;
    uint32_t outcome;
} game_rules_resolved_state;

typedef struct game_rules_level {
    game_rules_coordinate_system coordinates;
    uint32_t width;
    uint32_t height;
    const game_rules_cell* cells;
    uint32_t cell_count;
    const game_rules_fixture* fixtures;
    uint32_t fixture_count;
} game_rules_level;

typedef struct game_rules_snapshot {
    game_rules_level level;
    game_rules_resolved_state resolved;
} game_rules_snapshot;

// Events use one fixed layout to avoid exposing a C union. Only fields named below are meaningful:
// move-blocked: direction, move_status; entity-moved: entity_id, from, to,
// old_bottom_half_steps, new_bottom_half_steps, movement_cause; barrel-armed: entity_id;
// barrel-exploded: entity_id, coordinate, bottom_half_steps; player-crushed: entity_id (player),
// other_entity_id (crusher); switch-changed: color, active; door events: coordinate, color.
typedef struct game_rules_event {
    uint32_t kind;
    uint32_t direction;
    uint32_t move_status;
    uint64_t entity_id;
    uint64_t other_entity_id;
    game_rules_coordinate from;
    game_rules_coordinate to;
    game_rules_coordinate coordinate;
    int32_t old_bottom_half_steps;
    int32_t new_bottom_half_steps;
    int32_t bottom_half_steps;
    uint32_t movement_cause;
    uint32_t color;
    uint32_t active;
} game_rules_event;

typedef struct game_rules_tick {
    uint32_t index;
    const game_rules_event* events;
    uint32_t event_count;
    game_rules_resolved_state state_after;
} game_rules_tick;

// Result views and everything reachable from them are immutable and owned by owned_storage,
// which callers must treat as opaque. They remain valid independently of the engine until the
// matching dispose function is called. Result structs must not be copied while owned. Dispose
// accepts NULL and clears a non-NULL result for safe reuse.
typedef struct game_rules_state_result {
    uint32_t has_state;
    game_rules_snapshot state;
    void* owned_storage;
} game_rules_state_result;

typedef struct game_rules_load_result {
    uint32_t status;
    uint32_t accepted;
    const game_rules_validation_error* errors;
    uint32_t error_count;
    uint32_t has_initial_state;
    game_rules_resolved_state initial_state;
    const game_rules_tick* ticks;
    uint32_t tick_count;
    uint32_t has_final_state;
    game_rules_resolved_state final_state;
    uint32_t has_state;
    game_rules_snapshot state;
    uint32_t has_outcome;
    uint32_t outcome;
    void* owned_storage;
} game_rules_load_result;

typedef struct game_rules_move_result {
    uint32_t status;
    uint32_t accepted;
    uint32_t has_direction;
    uint32_t direction;
    const game_rules_event* events;
    uint32_t event_count;
    uint32_t has_initial_state;
    game_rules_resolved_state initial_state;
    const game_rules_tick* ticks;
    uint32_t tick_count;
    uint32_t has_final_state;
    game_rules_resolved_state final_state;
    uint32_t has_state;
    game_rules_snapshot state;
    uint32_t has_outcome;
    uint32_t outcome;
    void* owned_storage;
} game_rules_move_result;

typedef struct game_rules_rewind_result {
    uint32_t status;
    uint32_t accepted;
    const game_rules_event* events;
    uint32_t event_count;
    uint32_t has_restored_state;
    game_rules_resolved_state restored_state;
    uint32_t has_state;
    game_rules_snapshot state;
    uint32_t has_outcome;
    uint32_t outcome;
    void* owned_storage;
} game_rules_rewind_result;

uint32_t game_rules_engine_get_state_data(const game_rules_engine* engine,
                                          game_rules_state_result* out_result);
uint32_t game_rules_engine_load_level_data(game_rules_engine* engine,
                                           const game_rules_level_definition* level,
                                           game_rules_load_result* out_result);
uint32_t game_rules_engine_move_data(game_rules_engine* engine,
                                     uint32_t direction,
                                     game_rules_move_result* out_result);
uint32_t game_rules_engine_rewind_data(game_rules_engine* engine,
                                       game_rules_rewind_result* out_result);

void game_rules_state_result_dispose(game_rules_state_result* result);
void game_rules_load_result_dispose(game_rules_load_result* result);
void game_rules_move_result_dispose(game_rules_move_result* result);
void game_rules_rewind_result_dispose(game_rules_rewind_result* result);

#ifdef __cplusplus
} // extern "C"
#endif
