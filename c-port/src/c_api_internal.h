#pragma once

#include "game_rules/c_api.h"

#include <stddef.h>
#include <stdint.h>

typedef struct game_rules_c_allocator {
    void* context;
    void* (*allocate)(void* context, size_t size);
    void (*deallocate)(void* context, void* allocation);
} game_rules_c_allocator;

/* Canonical dynamic state. Counts never exceed the capacities owned by the session arena. */
typedef struct game_rules_c_state {
    game_rules_entity* entities;
    uint32_t entity_count;
    uint32_t entity_capacity;
    uint64_t* armed_barrel_ids;
    uint32_t armed_barrel_count;
    uint32_t armed_barrel_capacity;
    uint32_t* active_switch_colors;
    uint32_t active_switch_color_count;
    uint32_t active_switch_color_capacity;
    game_rules_coordinate* open_doors;
    uint32_t open_door_count;
    uint32_t open_door_capacity;
    uint32_t outcome;
} game_rules_c_state;

typedef struct game_rules_c_tick {
    uint32_t index;
    game_rules_event* events;
    uint32_t event_count;
    game_rules_c_state state_after;
    /* One private arena owns events and every array reachable from state_after. */
    void* owned_storage;
} game_rules_c_tick;

typedef struct game_rules_c_slide_candidate {
    game_rules_coordinate source;
    game_rules_coordinate destination;
    int32_t destination_bottom_half_steps;
    uint32_t first_entity;
    uint32_t entity_count;
} game_rules_c_slide_candidate;

typedef struct game_rules_c_blast_target {
    uint64_t id;
    uint32_t kind;
    game_rules_coordinate coordinate;
    int32_t bottom_half_steps;
    uint32_t impulses;
    game_rules_coordinate destination;
    uint32_t has_destination;
    uint32_t movement_is_valid;
    uint32_t destination_conflicts;
} game_rules_c_blast_target;

typedef struct game_rules_session {
    uint32_t marker;
    void* level_storage;
    void* history_storage;
    uint32_t has_level;
    game_rules_coordinate_system coordinates;
    uint32_t width;
    uint32_t height;
    game_rules_cell* cells;
    uint32_t cell_count;
    game_rules_fixture* fixtures;
    uint32_t fixture_count;
    uint32_t* fixture_index_by_cell;
    /* Initial is immutable; every physical tick reads current and writes scratch, then swaps. */
    game_rules_c_state initial_state;
    game_rules_c_state current_state;
    game_rules_c_state scratch_state;
    game_rules_event* scratch_events;
    uint32_t scratch_event_capacity;
    game_rules_event* scratch_terminal_events;
    uint32_t scratch_terminal_event_capacity;
    game_rules_c_slide_candidate* scratch_slides;
    game_rules_c_blast_target* scratch_targets;
    uint64_t* scratch_source_ids;
    game_rules_c_tick* initialization_ticks;
    uint32_t initialization_tick_count;
    uint32_t initialization_tick_capacity;
} game_rules_session;

struct game_rules_engine {
    game_rules_c_allocator allocator;
    game_rules_session* session;
};

typedef struct game_rules_c_level_view {
    game_rules_coordinate_system coordinates;
    uint32_t width;
    uint32_t height;
    const game_rules_cell* cells;
    uint32_t cell_count;
    const game_rules_fixture* fixtures;
    uint32_t fixture_count;
    const game_rules_entity* entities;
    uint32_t entity_count;
} game_rules_c_level_view;

typedef struct game_rules_c_owned_level {
    game_rules_c_level_view view;
    game_rules_cell* cells;
    game_rules_fixture* fixtures;
    game_rules_entity* entities;
} game_rules_c_owned_level;

typedef struct game_rules_c_validation_result {
    game_rules_validation_error* errors;
    uint32_t count;
    uint32_t capacity;
    uint32_t allocation_failed;
} game_rules_c_validation_result;

enum game_rules_c_json_error_code {
    GAME_RULES_C_JSON_INVALID_JSON = 0,
    GAME_RULES_C_JSON_DOCUMENT_TOO_LARGE = 1,
    GAME_RULES_C_JSON_NESTING_TOO_DEEP = 2,
    GAME_RULES_C_JSON_ROOT_NOT_OBJECT = 3,
    GAME_RULES_C_JSON_MISSING_MEMBER = 4,
    GAME_RULES_C_JSON_UNKNOWN_MEMBER = 5,
    GAME_RULES_C_JSON_DUPLICATE_MEMBER = 6,
    GAME_RULES_C_JSON_INVALID_MEMBER_TYPE = 7,
    GAME_RULES_C_JSON_INTEGER_OUT_OF_RANGE = 8,
    GAME_RULES_C_JSON_INVALID_ENUM_VALUE = 9,
    GAME_RULES_C_JSON_INVALID_FORMAT = 10,
    GAME_RULES_C_JSON_UNSUPPORTED_VERSION = 11,
    GAME_RULES_C_JSON_INVALID_ENTITY_ID = 12
};

typedef struct game_rules_c_json_error {
    uint32_t code;
    size_t byte_offset;
    char* path;
    size_t path_length;
} game_rules_c_json_error;

enum game_rules_c_decode_status {
    GAME_RULES_C_DECODE_OK = 0,
    GAME_RULES_C_DECODE_JSON_ERROR = 1,
    GAME_RULES_C_DECODE_INVALID_LEVEL = 2,
    GAME_RULES_C_DECODE_ALLOCATION_FAILED = 3
};

typedef struct game_rules_c_decode_result {
    uint32_t status;
    game_rules_c_owned_level level;
    game_rules_c_json_error json_error;
    game_rules_c_validation_result validation;
} game_rules_c_decode_result;

void* game_rules_c_allocate_owned(const game_rules_c_allocator* allocator,
                                  size_t payload_size);
void game_rules_c_deallocate_owned(void* payload);

void game_rules_c_validate_level(const game_rules_c_level_view* level,
                                 const game_rules_c_allocator* allocator,
                                 game_rules_c_validation_result* result);
void game_rules_c_validation_result_destroy(game_rules_c_validation_result* result);
void game_rules_c_canonicalize_level(game_rules_c_owned_level* level);

game_rules_session* game_rules_c_build_resolved_session(
    const game_rules_c_allocator* allocator,
    const game_rules_c_level_view* level);

void game_rules_c_decode_level_json(const char* json,
                                    size_t json_length,
                                    const game_rules_c_allocator* allocator,
                                    game_rules_c_decode_result* result);
void game_rules_c_decode_result_destroy(game_rules_c_decode_result* result);
const char* game_rules_c_json_error_name(uint32_t code);
const char* game_rules_c_validation_error_name(uint32_t code);

char* game_rules_c_stage03_load_json(game_rules_engine* engine,
                                     const char* json,
                                     uint32_t length);
char* game_rules_c_stage03_get_state(game_rules_engine* engine);
uint32_t game_rules_c_stage03_get_state_data(const game_rules_engine* engine,
                                             game_rules_state_result* result);
uint32_t game_rules_c_stage03_load_data(game_rules_engine* engine,
                                        const game_rules_level_definition* level,
                                        game_rules_load_result* result);
void game_rules_c_destroy_session(game_rules_session* session);

/* Private compatibility seams exercised by the candidate lifecycle tests. */
uint32_t game_rules_c_engine_replace_session(game_rules_engine* engine,
                                             uint32_t marker,
                                             size_t level_storage_size,
                                             size_t history_storage_size);

uint32_t game_rules_c_engine_session_marker(const game_rules_engine* engine);

void* game_rules_c_engine_allocate_result_storage(const game_rules_engine* engine,
                                                  size_t storage_size);
