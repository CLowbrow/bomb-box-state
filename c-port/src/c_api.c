#include "game_rules/c_api.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct game_rules_engine {
    uint32_t lifecycle_marker;
};

static char* copy_json(const char* value)
{
    const size_t length = strlen(value);
    char* const copy = (char*)malloc(length + 1U);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, value, length + 1U);
    return copy;
}

static const char* direction_name(uint32_t direction)
{
    switch (direction) {
    case GAME_RULES_DIRECTION_NORTH: return "north";
    case GAME_RULES_DIRECTION_EAST: return "east";
    case GAME_RULES_DIRECTION_SOUTH: return "south";
    case GAME_RULES_DIRECTION_WEST: return "west";
    default: return NULL;
    }
}

static void* allocate_result_owner(size_t size)
{
    return malloc(size);
}

uint32_t game_rules_api_version(void)
{
    return 1U;
}

const char* game_rules_engine_status(void)
{
    return "c17_skeleton";
}

game_rules_engine* game_rules_engine_create(void)
{
    game_rules_engine* const engine = (game_rules_engine*)malloc(sizeof(*engine));
    if (engine != NULL) {
        engine->lifecycle_marker = 0x47525343U;
    }
    return engine;
}

void game_rules_engine_destroy(game_rules_engine* engine)
{
    free(engine);
}

char* game_rules_engine_load_level(game_rules_engine* engine,
                                   const char* level_json,
                                   uint32_t level_json_length)
{
    (void)level_json_length;
    if (engine == NULL) {
        return copy_json("{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"invalid_engine\",\"state\":null}");
    }
    if (level_json == NULL) {
        return copy_json("{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"invalid_argument\",\"state\":null}");
    }
    return copy_json("{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"not_implemented\",\"state\":null}");
}

char* game_rules_engine_get_state(game_rules_engine* engine)
{
    if (engine == NULL) {
        return copy_json("{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\"invalid_engine\",\"state\":null}");
    }
    return copy_json("{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\"no_level\",\"state\":null}");
}

char* game_rules_engine_move(game_rules_engine* engine, uint32_t direction)
{
    const char* name;
    char buffer[224];
    int written;
    if (engine == NULL) {
        return copy_json("{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"invalid_engine\",\"state\":null}");
    }
    name = direction_name(direction);
    if (name == NULL) {
        return copy_json("{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"invalid_direction\",\"accepted\":false,\"direction\":null,\"events\":[],\"initialState\":null,\"ticks\":[],\"state\":null,\"outcome\":null}");
    }
    written = snprintf(buffer,
                       sizeof(buffer),
                       "{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"no_level\",\"accepted\":false,\"direction\":\"%s\",\"events\":[{\"type\":\"moveBlocked\",\"direction\":\"%s\",\"reason\":\"no_level\"}],\"initialState\":null,\"ticks\":[],\"state\":null,\"outcome\":null}",
                       name,
                       name);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return NULL;
    }
    return copy_json(buffer);
}

char* game_rules_engine_rewind(game_rules_engine* engine)
{
    if (engine == NULL) {
        return copy_json("{\"apiVersion\":1,\"operation\":\"rewind\",\"status\":\"invalid_engine\",\"state\":null}");
    }
    return copy_json("{\"apiVersion\":1,\"operation\":\"rewind\",\"status\":\"history_empty\",\"accepted\":false,\"events\":[],\"state\":null,\"outcome\":null}");
}

void game_rules_string_free(char* result)
{
    free(result);
}

uint32_t game_rules_data_api_version(void)
{
    return 1U;
}

uint32_t game_rules_engine_get_state_data(const game_rules_engine* engine,
                                          game_rules_state_result* out_result)
{
    if (out_result == NULL) {
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    if (engine == NULL) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }
    out_result->owned_storage = allocate_result_owner(1U);
    if (out_result->owned_storage == NULL) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    return GAME_RULES_CALL_OK;
}

uint32_t game_rules_engine_load_level_data(game_rules_engine* engine,
                                           const game_rules_level_definition* level,
                                           game_rules_load_result* out_result)
{
    if (out_result == NULL || level == NULL) {
        if (out_result != NULL) {
            memset(out_result, 0, sizeof(*out_result));
        }
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    if (engine == NULL) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }
    /* The frozen typed ABI has no not-implemented status. Until stage 01,
       typed level loading is rejected at the call boundary without mutation. */
    return GAME_RULES_CALL_INVALID_ARGUMENT;
}

uint32_t game_rules_engine_move_data(game_rules_engine* engine,
                                     uint32_t direction,
                                     game_rules_move_result* out_result)
{
    if (out_result == NULL) {
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    if (engine == NULL) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }
    if (direction_name(direction) == NULL) {
        out_result->owned_storage = allocate_result_owner(1U);
        if (out_result->owned_storage == NULL) {
            return GAME_RULES_CALL_ALLOCATION_FAILED;
        }
        out_result->status = GAME_RULES_MOVE_INVALID_DIRECTION;
        return GAME_RULES_CALL_OK;
    }
    out_result->owned_storage = allocate_result_owner(sizeof(game_rules_event));
    if (out_result->owned_storage == NULL) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    out_result->status = GAME_RULES_MOVE_NO_LEVEL;
    out_result->has_direction = 1U;
    out_result->direction = direction;
    out_result->events = (const game_rules_event*)out_result->owned_storage;
    out_result->event_count = 1U;
    memset(out_result->owned_storage, 0, sizeof(game_rules_event));
    ((game_rules_event*)out_result->owned_storage)->kind = GAME_RULES_EVENT_MOVE_BLOCKED;
    ((game_rules_event*)out_result->owned_storage)->direction = direction;
    ((game_rules_event*)out_result->owned_storage)->move_status = GAME_RULES_MOVE_NO_LEVEL;
    return GAME_RULES_CALL_OK;
}

uint32_t game_rules_engine_rewind_data(game_rules_engine* engine,
                                       game_rules_rewind_result* out_result)
{
    if (out_result == NULL) {
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    if (engine == NULL) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }
    out_result->owned_storage = allocate_result_owner(1U);
    if (out_result->owned_storage == NULL) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    out_result->status = GAME_RULES_REWIND_HISTORY_EMPTY;
    return GAME_RULES_CALL_OK;
}

void game_rules_state_result_dispose(game_rules_state_result* result)
{
    if (result != NULL) {
        free(result->owned_storage);
        memset(result, 0, sizeof(*result));
    }
}

void game_rules_load_result_dispose(game_rules_load_result* result)
{
    if (result != NULL) {
        free(result->owned_storage);
        memset(result, 0, sizeof(*result));
    }
}

void game_rules_move_result_dispose(game_rules_move_result* result)
{
    if (result != NULL) {
        free(result->owned_storage);
        memset(result, 0, sizeof(*result));
    }
}

void game_rules_rewind_result_dispose(game_rules_rewind_result* result)
{
    if (result != NULL) {
        free(result->owned_storage);
        memset(result, 0, sizeof(*result));
    }
}
