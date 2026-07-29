#include "game_rules/c_api.h"
#include "game_rules/c_allocator_api.h"

#include "c_api_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef union game_rules_owned_header {
    struct {
        game_rules_c_allocator allocator;
    } value;
    max_align_t alignment;
} game_rules_owned_header;

static void* system_allocate(void* context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void system_deallocate(void* context, void* allocation)
{
    (void)context;
    free(allocation);
}

static game_rules_c_allocator system_allocator(void)
{
    game_rules_c_allocator allocator;
    allocator.context = NULL;
    allocator.allocate = system_allocate;
    allocator.deallocate = system_deallocate;
    return allocator;
}

static int allocator_v1_is_valid(const game_rules_allocator_v1* allocator)
{
    const size_t required_size =
        offsetof(game_rules_allocator_v1, deallocate) + sizeof(allocator->deallocate);
    return allocator != NULL && allocator->api_version == GAME_RULES_ALLOCATOR_API_VERSION_1 &&
           allocator->struct_size >= required_size && allocator->allocate != NULL &&
           allocator->deallocate != NULL;
}

void* game_rules_c_allocate_owned(const game_rules_c_allocator* allocator, size_t payload_size)
{
    game_rules_owned_header* header;
    size_t allocation_size;
    if (allocator == NULL || allocator->allocate == NULL || allocator->deallocate == NULL) {
        return NULL;
    }
    if (payload_size == 0U) {
        payload_size = 1U;
    }
    if (payload_size > SIZE_MAX - sizeof(*header)) {
        return NULL;
    }
    allocation_size = sizeof(*header) + payload_size;
    header = (game_rules_owned_header*)allocator->allocate(allocator->context, allocation_size);
    if (header == NULL) {
        return NULL;
    }
    header->value.allocator = *allocator;
    return (void*)(header + 1);
}

void game_rules_c_deallocate_owned(void* payload)
{
    game_rules_owned_header* header;
    game_rules_c_allocator allocator;
    if (payload == NULL) {
        return;
    }
    header = ((game_rules_owned_header*)payload) - 1;
    allocator = header->value.allocator;
    allocator.deallocate(allocator.context, header);
}

static char* copy_json_with_allocator(const game_rules_c_allocator* allocator, const char* value)
{
    const size_t length = strlen(value);
    char* const copy = (char*)game_rules_c_allocate_owned(allocator, length + 1U);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, value, length + 1U);
    return copy;
}

static char* copy_json_for_engine(const game_rules_engine* engine, const char* value)
{
    const game_rules_c_allocator allocator =
        engine == NULL ? system_allocator() : engine->allocator;
    return copy_json_with_allocator(&allocator, value);
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

void game_rules_c_destroy_session(game_rules_session* session)
{
    if (session == NULL) {
        return;
    }
    game_rules_c_deallocate_owned(session->history_storage);
    game_rules_c_deallocate_owned(session->level_storage);
    game_rules_c_deallocate_owned(session);
}

static game_rules_engine* create_engine(const game_rules_c_allocator allocator)
{
    game_rules_engine* const engine =
        (game_rules_engine*)game_rules_c_allocate_owned(&allocator, sizeof(*engine));
    if (engine == NULL) {
        return NULL;
    }
    engine->allocator = allocator;
    engine->session = NULL;
    return engine;
}

uint32_t game_rules_api_version(void)
{
    return 1U;
}

const char* game_rules_engine_status(void)
{
    return "schema_ready";
}

uint32_t game_rules_allocator_api_version(void)
{
    return GAME_RULES_ALLOCATOR_API_VERSION_1;
}

game_rules_engine* game_rules_engine_create(void)
{
    return create_engine(system_allocator());
}

game_rules_engine*
game_rules_engine_create_with_allocator_v1(const game_rules_allocator_v1* allocator)
{
    game_rules_c_allocator internal;
    if (!allocator_v1_is_valid(allocator)) {
        return NULL;
    }
    internal.context = allocator->context;
    internal.allocate = allocator->allocate;
    internal.deallocate = allocator->deallocate;
    return create_engine(internal);
}

void game_rules_engine_destroy(game_rules_engine* engine)
{
    if (engine == NULL) {
        return;
    }
    game_rules_c_destroy_session(engine->session);
    engine->session = NULL;
    game_rules_c_deallocate_owned(engine);
}

uint32_t game_rules_c_engine_replace_session(game_rules_engine* engine,
                                             uint32_t marker,
                                             size_t level_storage_size,
                                             size_t history_storage_size)
{
    game_rules_session* replacement;
    game_rules_session* previous;
    if (engine == NULL) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }

    replacement =
        (game_rules_session*)game_rules_c_allocate_owned(&engine->allocator, sizeof(*replacement));
    if (replacement == NULL) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    replacement->marker = marker;
    replacement->level_storage = NULL;
    replacement->history_storage = NULL;

    if (level_storage_size != 0U) {
        replacement->level_storage = game_rules_c_allocate_owned(&engine->allocator, level_storage_size);
        if (replacement->level_storage == NULL) {
            game_rules_c_destroy_session(replacement);
            return GAME_RULES_CALL_ALLOCATION_FAILED;
        }
        memset(replacement->level_storage, 0, level_storage_size);
    }
    if (history_storage_size != 0U) {
        replacement->history_storage = game_rules_c_allocate_owned(&engine->allocator, history_storage_size);
        if (replacement->history_storage == NULL) {
            game_rules_c_destroy_session(replacement);
            return GAME_RULES_CALL_ALLOCATION_FAILED;
        }
        memset(replacement->history_storage, 0, history_storage_size);
    }

    previous = engine->session;
    engine->session = replacement;
    game_rules_c_destroy_session(previous);
    return GAME_RULES_CALL_OK;
}

uint32_t game_rules_c_engine_session_marker(const game_rules_engine* engine)
{
    return engine == NULL || engine->session == NULL ? 0U : engine->session->marker;
}

void* game_rules_c_engine_allocate_result_storage(const game_rules_engine* engine,
                                                  size_t storage_size)
{
    /* Public result graphs use one contiguous, independently owned arena. */
    return engine == NULL ? NULL : game_rules_c_allocate_owned(&engine->allocator, storage_size);
}

char* game_rules_engine_load_level(game_rules_engine* engine,
                                   const char* level_json,
                                   uint32_t level_json_length)
{
    if (engine == NULL) {
        return copy_json_for_engine(
            NULL,
            "{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"invalid_engine\",\"state\":null}");
    }
    if (level_json == NULL) {
        return copy_json_for_engine(
            engine,
            "{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"invalid_argument\",\"state\":null}");
    }
    return game_rules_c_stage02_load_json(engine, level_json, level_json_length);
}

char* game_rules_engine_get_state(game_rules_engine* engine)
{
    if (engine == NULL) {
        return copy_json_for_engine(
            NULL,
            "{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\"invalid_engine\",\"state\":null}");
    }
    return game_rules_c_stage02_get_state(engine);
}

char* game_rules_engine_move(game_rules_engine* engine, uint32_t direction)
{
    const char* name;
    char buffer[256];
    int written;
    if (engine == NULL) {
        return copy_json_for_engine(
            NULL,
            "{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"invalid_engine\",\"state\":null}");
    }
    name = direction_name(direction);
    if (name == NULL) {
        return copy_json_for_engine(
            engine,
            "{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"invalid_direction\",\"accepted\":false,\"direction\":null,\"events\":[],\"initialState\":null,\"ticks\":[],\"state\":null,\"outcome\":null}");
    }
    written = snprintf(buffer,
                       sizeof(buffer),
                       "{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"no_level\",\"accepted\":false,\"direction\":\"%s\",\"events\":[{\"type\":\"moveBlocked\",\"direction\":\"%s\",\"reason\":\"no_level\"}],\"initialState\":null,\"ticks\":[],\"state\":null,\"outcome\":null}",
                       name,
                       name);
    if (written < 0 || (size_t)written >= sizeof(buffer)) {
        return NULL;
    }
    return copy_json_for_engine(engine, buffer);
}

char* game_rules_engine_rewind(game_rules_engine* engine)
{
    if (engine == NULL) {
        return copy_json_for_engine(
            NULL,
            "{\"apiVersion\":1,\"operation\":\"rewind\",\"status\":\"invalid_engine\",\"state\":null}");
    }
    return copy_json_for_engine(
        engine,
        "{\"apiVersion\":1,\"operation\":\"rewind\",\"status\":\"history_empty\",\"accepted\":false,\"events\":[],\"state\":null,\"outcome\":null}");
}

void game_rules_string_free(char* result)
{
    game_rules_c_deallocate_owned(result);
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
    return game_rules_c_stage02_get_state_data(engine, out_result);
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
    return game_rules_c_stage02_load_data(engine, level, out_result);
}

uint32_t game_rules_engine_move_data(game_rules_engine* engine,
                                     uint32_t direction,
                                     game_rules_move_result* out_result)
{
    void* owner;
    if (out_result == NULL) {
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    if (engine == NULL) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }
    if (direction_name(direction) == NULL) {
        owner = game_rules_c_engine_allocate_result_storage(engine, 1U);
        if (owner == NULL) {
            return GAME_RULES_CALL_ALLOCATION_FAILED;
        }
        out_result->status = GAME_RULES_MOVE_INVALID_DIRECTION;
        out_result->owned_storage = owner;
        return GAME_RULES_CALL_OK;
    }

    owner = game_rules_c_engine_allocate_result_storage(engine, sizeof(game_rules_event));
    if (owner == NULL) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    memset(owner, 0, sizeof(game_rules_event));
    ((game_rules_event*)owner)->kind = GAME_RULES_EVENT_MOVE_BLOCKED;
    ((game_rules_event*)owner)->direction = direction;
    ((game_rules_event*)owner)->move_status = GAME_RULES_MOVE_NO_LEVEL;

    out_result->status = GAME_RULES_MOVE_NO_LEVEL;
    out_result->has_direction = 1U;
    out_result->direction = direction;
    out_result->events = (const game_rules_event*)owner;
    out_result->event_count = 1U;
    out_result->owned_storage = owner;
    return GAME_RULES_CALL_OK;
}

uint32_t game_rules_engine_rewind_data(game_rules_engine* engine,
                                       game_rules_rewind_result* out_result)
{
    void* owner;
    if (out_result == NULL) {
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    memset(out_result, 0, sizeof(*out_result));
    if (engine == NULL) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }
    owner = game_rules_c_engine_allocate_result_storage(engine, 1U);
    if (owner == NULL) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    out_result->status = GAME_RULES_REWIND_HISTORY_EMPTY;
    out_result->owned_storage = owner;
    return GAME_RULES_CALL_OK;
}

static void dispose_result(void* owned_storage, void* result, size_t result_size)
{
    if (result == NULL) {
        return;
    }
    game_rules_c_deallocate_owned(owned_storage);
    memset(result, 0, result_size);
}

void game_rules_state_result_dispose(game_rules_state_result* result)
{
    dispose_result(result == NULL ? NULL : result->owned_storage, result, sizeof(*result));
}

void game_rules_load_result_dispose(game_rules_load_result* result)
{
    dispose_result(result == NULL ? NULL : result->owned_storage, result, sizeof(*result));
}

void game_rules_move_result_dispose(game_rules_move_result* result)
{
    dispose_result(result == NULL ? NULL : result->owned_storage, result, sizeof(*result));
}

void game_rules_rewind_result_dispose(game_rules_rewind_result* result)
{
    dispose_result(result == NULL ? NULL : result->owned_storage, result, sizeof(*result));
}
