#include "game_rules/c_api.h"
#include "game_rules/c_allocator_api.h"

#include "c_api_internal.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_TRACKED_ALLOCATIONS = 16384 };

static const char valid_level_json[] =
    "{\"format\":\"game-rules-level\",\"version\":1,"
    "\"coordinateSystem\":{\"origin\":{\"x\":0,\"y\":0},\"positiveX\":\"east\",\"positiveY\":\"north\"},"
    "\"width\":2,\"height\":1,\"cells\":[{\"coordinate\":{\"x\":0,\"y\":0},"
    "\"type\":\"flat\",\"elevation\":0},{\"coordinate\":{\"x\":1,\"y\":0},"
    "\"type\":\"flat\",\"elevation\":1}],\"fixtures\":[],"
    "\"entities\":[{\"id\":\"18446744073709551615\",\"type\":\"barrel\","
    "\"coordinate\":{\"x\":0,\"y\":0},\"bottomHalfSteps\":4},"
    "{\"id\":\"1\",\"type\":\"player\",\"coordinate\":{\"x\":1,\"y\":0},"
    "\"bottomHalfSteps\":2}]}";

static const char fixture_move_level_json[] =
    "{\"format\":\"game-rules-level\",\"version\":1,"
    "\"coordinateSystem\":{\"origin\":{\"x\":0,\"y\":0},\"positiveX\":\"east\",\"positiveY\":\"north\"},"
    "\"width\":4,\"height\":1,\"cells\":["
    "{\"coordinate\":{\"x\":0,\"y\":0},\"type\":\"flat\",\"elevation\":2},"
    "{\"coordinate\":{\"x\":1,\"y\":0},\"type\":\"flat\",\"elevation\":2},"
    "{\"coordinate\":{\"x\":2,\"y\":0},\"type\":\"flat\",\"elevation\":0},"
    "{\"coordinate\":{\"x\":3,\"y\":0},\"type\":\"flat\",\"elevation\":0}],"
    "\"fixtures\":[{\"coordinate\":{\"x\":2,\"y\":0},\"type\":\"switch\",\"color\":\"green\"},"
    "{\"coordinate\":{\"x\":3,\"y\":0},\"type\":\"door\",\"color\":\"green\"}],"
    "\"entities\":[{\"id\":\"1\",\"type\":\"player\",\"coordinate\":{\"x\":0,\"y\":0},\"bottomHalfSteps\":4},"
    "{\"id\":\"8\",\"type\":\"box\",\"coordinate\":{\"x\":1,\"y\":0},\"bottomHalfSteps\":4}]}";

typedef struct tracked_allocation {
    void* pointer;
    int live;
} tracked_allocation;

typedef struct allocation_tracker {
    size_t attempt;
    size_t fail_at;
    int failure_enabled;
    size_t live_count;
    size_t invalid_free_count;
    tracked_allocation allocations[MAX_TRACKED_ALLOCATIONS];
    size_t allocation_count;
} allocation_tracker;

static void tracker_begin_success(allocation_tracker* tracker)
{
    tracker->attempt = 0U;
    tracker->failure_enabled = 0;
}

static void tracker_begin_failure(allocation_tracker* tracker, size_t fail_at)
{
    tracker->attempt = 0U;
    tracker->fail_at = fail_at;
    tracker->failure_enabled = 1;
}

static void* tracked_allocate(void* context, size_t size)
{
    allocation_tracker* const tracker = (allocation_tracker*)context;
    void* allocation;
    const size_t index = tracker->attempt;
    tracker->attempt += 1U;
    if (tracker->failure_enabled && index == tracker->fail_at) {
        return NULL;
    }
    if (size > 1024U * 1024U * 1024U) {
        return NULL;
    }
    assert(tracker->allocation_count < MAX_TRACKED_ALLOCATIONS);
    allocation = malloc(size);
    assert(allocation != NULL);
    tracker->allocations[tracker->allocation_count].pointer = allocation;
    tracker->allocations[tracker->allocation_count].live = 1;
    tracker->allocation_count += 1U;
    tracker->live_count += 1U;
    return allocation;
}

static void tracked_deallocate(void* context, void* allocation)
{
    allocation_tracker* const tracker = (allocation_tracker*)context;
    size_t index;
    for (index = tracker->allocation_count; index > 0U; --index) {
        tracked_allocation* const record = &tracker->allocations[index - 1U];
        if (record->pointer == allocation && record->live) {
            record->live = 0;
            tracker->live_count -= 1U;
            free(allocation);
            return;
        }
    }
    tracker->invalid_free_count += 1U;
}

static game_rules_allocator_v1 allocator_for(allocation_tracker* tracker)
{
    game_rules_allocator_v1 allocator;
    allocator.api_version = GAME_RULES_ALLOCATOR_API_VERSION_1;
    allocator.struct_size = (uint32_t)sizeof(allocator);
    allocator.context = tracker;
    allocator.allocate = tracked_allocate;
    allocator.deallocate = tracked_deallocate;
    return allocator;
}

static int bytes_are_zero(const void* value, size_t size)
{
    const unsigned char* bytes = (const unsigned char*)value;
    size_t index;
    for (index = 0U; index < size; ++index) {
        if (bytes[index] != 0U) {
            return 0;
        }
    }
    return 1;
}

static void expect_one_allocation_failure_legacy(allocation_tracker* tracker,
                                                 game_rules_engine* engine)
{
    const size_t baseline = tracker->live_count;
    tracker_begin_failure(tracker, 0U);
    assert(game_rules_engine_get_state(engine) == NULL);
    assert(tracker->attempt == 1U);
    assert(tracker->live_count == baseline);

    tracker_begin_failure(tracker, 0U);
    assert(game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST) == NULL);
    assert(tracker->attempt == 1U);
    assert(tracker->live_count == baseline);

    tracker_begin_failure(tracker, 0U);
    assert(game_rules_engine_move(engine, 99U) == NULL);
    assert(tracker->attempt == 1U);
    assert(tracker->live_count == baseline);

    tracker_begin_failure(tracker, 0U);
    assert(game_rules_engine_rewind(engine) == NULL);
    assert(tracker->attempt == 1U);
    assert(tracker->live_count == baseline);

    tracker_begin_failure(tracker, 0U);
    assert(game_rules_engine_load_level(engine, NULL, 0U) == NULL);
    assert(tracker->attempt == 1U);
    assert(tracker->live_count == baseline);

    tracker_begin_failure(tracker, 0U);
    assert(game_rules_engine_load_level(engine, "{}", 2U) == NULL);
    assert(tracker->attempt == 1U);
    assert(tracker->live_count == baseline);
}

static void expect_one_allocation_failure_typed(allocation_tracker* tracker,
                                                game_rules_engine* engine)
{
    const size_t baseline = tracker->live_count;
    game_rules_state_result state;
    game_rules_move_result move;
    game_rules_rewind_result rewind;

    memset(&state, 0xA5, sizeof(state));
    tracker_begin_failure(tracker, 0U);
    assert(game_rules_engine_get_state_data(engine, &state) ==
           GAME_RULES_CALL_ALLOCATION_FAILED);
    assert(tracker->attempt == 1U);
    assert(bytes_are_zero(&state, sizeof(state)));
    assert(tracker->live_count == baseline);

    memset(&move, 0xA5, sizeof(move));
    tracker_begin_failure(tracker, 0U);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_NORTH, &move) ==
           GAME_RULES_CALL_ALLOCATION_FAILED);
    assert(tracker->attempt == 1U);
    assert(bytes_are_zero(&move, sizeof(move)));
    assert(tracker->live_count == baseline);

    memset(&move, 0xA5, sizeof(move));
    tracker_begin_failure(tracker, 0U);
    assert(game_rules_engine_move_data(engine, 99U, &move) ==
           GAME_RULES_CALL_ALLOCATION_FAILED);
    assert(tracker->attempt == 1U);
    assert(bytes_are_zero(&move, sizeof(move)));
    assert(tracker->live_count == baseline);

    memset(&rewind, 0xA5, sizeof(rewind));
    tracker_begin_failure(tracker, 0U);
    assert(game_rules_engine_rewind_data(engine, &rewind) ==
           GAME_RULES_CALL_ALLOCATION_FAILED);
    assert(tracker->attempt == 1U);
    assert(bytes_are_zero(&rewind, sizeof(rewind)));
    assert(tracker->live_count == baseline);

    tracker_begin_failure(tracker, 0U);
    assert(game_rules_c_engine_allocate_result_storage(engine, 16U) == NULL);
    assert(tracker->attempt == 1U);
    assert(tracker->live_count == baseline);
}

static void expect_atomic_replacement_failures(allocation_tracker* tracker,
                                               game_rules_engine* engine)
{
    size_t failure_index;
    size_t baseline;
    tracker_begin_success(tracker);
    assert(game_rules_c_engine_replace_session(engine, 11U, 17U, 23U) ==
           GAME_RULES_CALL_OK);
    assert(tracker->attempt == 3U);
    assert(game_rules_c_engine_session_marker(engine) == 11U);
    baseline = tracker->live_count;

    for (failure_index = 0U; failure_index < 3U; ++failure_index) {
        tracker_begin_failure(tracker, failure_index);
        assert(game_rules_c_engine_replace_session(engine, 99U, 17U, 23U) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(game_rules_c_engine_session_marker(engine) == 11U);
        assert(tracker->live_count == baseline);
        assert(tracker->invalid_free_count == 0U);
    }

    tracker_begin_success(tracker);
    assert(game_rules_c_engine_replace_session(engine, 99U, SIZE_MAX, 23U) ==
           GAME_RULES_CALL_ALLOCATION_FAILED);
    assert(game_rules_c_engine_session_marker(engine) == 11U);
    assert(tracker->live_count == baseline);

    tracker_begin_success(tracker);
    assert(game_rules_c_engine_replace_session(engine, 22U, 17U, 23U) ==
           GAME_RULES_CALL_OK);
    assert(game_rules_c_engine_session_marker(engine) == 22U);
    assert(tracker->live_count == baseline);
}

static void expect_stage03_load_failures(allocation_tracker* tracker,
                                         const game_rules_allocator_v1* allocator)
{
    const game_rules_cell cells[2] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 1, 0}};
    const game_rules_entity entities[2] = {
        {UINT64_MAX, GAME_RULES_ENTITY_BARREL, {0, 0}, 4},
        {1, GAME_RULES_ENTITY_PLAYER, {1, 0}, 2}};
    const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        2U, 1U, cells, 2U, NULL, 0U, entities, 2U};
    size_t json_attempts;
    size_t typed_attempts;
    size_t failure;
    game_rules_engine* engine;
    char* json;
    game_rules_load_result load;

    {
        game_rules_level_definition overflow = level;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        overflow.cell_count = UINT32_MAX;
        assert(game_rules_engine_load_level_data(engine, &overflow, &load) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(game_rules_c_engine_session_marker(engine) == 0U);

        overflow.cells = NULL;
        tracker_begin_success(tracker);
        assert(game_rules_engine_load_level_data(engine, &overflow, &load) ==
               GAME_RULES_CALL_INVALID_ARGUMENT);
        assert(tracker->attempt == 0U);
        assert(game_rules_c_engine_session_marker(engine) == 0U);
        game_rules_engine_destroy(engine);
    }

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    tracker_begin_success(tracker);
    json = game_rules_engine_load_level(engine, valid_level_json,
                                        (uint32_t)strlen(valid_level_json));
    assert(json != NULL);
    json_attempts = tracker->attempt;
    game_rules_string_free(json);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < json_attempts; ++failure) {
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_load_level(engine, valid_level_json,
                                            (uint32_t)strlen(valid_level_json)) == NULL);
        assert(game_rules_c_engine_session_marker(engine) == 0U);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    memset(&load, 0, sizeof(load));
    tracker_begin_success(tracker);
    assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
    assert(load.tick_count == 2U);
    assert(load.ticks[0].state_after.armed_barrel_count == 1U);
    assert(load.ticks[0].state_after.armed_barrel_ids[0] == UINT64_MAX);
    typed_attempts = tracker->attempt;
    game_rules_load_result_dispose(&load);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < typed_attempts; ++failure) {
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        memset(&load, 0xA5, sizeof(load));
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(bytes_are_zero(&load, sizeof(load)));
        assert(game_rules_c_engine_session_marker(engine) == 0U);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }
}

static void expect_stage04_move_failures(allocation_tracker* tracker,
                                         const game_rules_allocator_v1* allocator)
{
    const game_rules_cell cells[3] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
    const game_rules_entity player =
        {17U, GAME_RULES_ENTITY_PLAYER, {1, 0}, 0};
    const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        3U, 1U, cells, 3U, NULL, 0U, &player, 1U};
    game_rules_engine* engine;
    game_rules_load_result load = {0};
    game_rules_move_result move;
    char* json;
    size_t legacy_attempts;
    size_t failure;

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    tracker_begin_success(tracker);
    json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
    assert(json != NULL);
    legacy_attempts = tracker->attempt;
    game_rules_string_free(json);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < legacy_attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        baseline = tracker->live_count;
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST) == NULL);
        assert(engine->session->current_state.entities[0].coordinate.x == 1);
        assert(engine->session->history_count == 0U);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
        assert(json != NULL);
        assert(strstr(json, "\"from\":{\"x\":1,\"y\":0}") != NULL);
        assert(engine->session->history_count == 1U);
        game_rules_string_free(json);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    {
        const size_t baseline = tracker->live_count;
        memset(&move, 0xA5, sizeof(move));
        tracker_begin_failure(tracker, 0U);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(bytes_are_zero(&move, sizeof(move)));
        assert(engine->session->current_state.entities[0].coordinate.x == 1);
        assert(engine->session->history_count == 0U);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_OK);
        assert(move.accepted == 1U);
        assert(move.initial_state.entities[0].coordinate.x == 1);
        assert(move.final_state.entities[0].coordinate.x == 2);
        assert(engine->session->history_count == 1U);
        game_rules_move_result_dispose(&move);
    }
    game_rules_engine_destroy(engine);
    assert(tracker->invalid_free_count == 0U);
}

static void expect_stage05_push_failures(allocation_tracker* tracker,
                                         const game_rules_allocator_v1* allocator)
{
    const game_rules_cell cells[4] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{3, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
    const game_rules_entity entities[2] = {
        {17U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
        {4U, GAME_RULES_ENTITY_BARREL, {1, 0}, 0}};
    const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        4U, 1U, cells, 4U, NULL, 0U, entities, 2U};
    game_rules_engine* engine;
    game_rules_load_result load = {0};
    game_rules_move_result move;
    char* json;
    size_t attempts;
    size_t failure;

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    tracker_begin_success(tracker);
    json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
    assert(json != NULL);
    attempts = tracker->attempt;
    assert(strstr(json, "\"entityId\":\"17\"") != NULL);
    assert(strstr(json, "\"entityId\":\"4\"") != NULL);
    game_rules_string_free(json);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        baseline = tracker->live_count;
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST) == NULL);
        assert(engine->session->current_state.entities[0].id == 17U);
        assert(engine->session->current_state.entities[0].coordinate.x == 0);
        assert(engine->session->current_state.entities[1].id == 4U);
        assert(engine->session->current_state.entities[1].coordinate.x == 1);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
        assert(json != NULL);
        assert(strstr(json, "\"from\":{\"x\":0,\"y\":0}") != NULL);
        assert(strstr(json, "\"to\":{\"x\":2,\"y\":0}") != NULL);
        game_rules_string_free(json);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    {
        const size_t baseline = tracker->live_count;
        memset(&move, 0xA5, sizeof(move));
        tracker_begin_failure(tracker, 0U);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(bytes_are_zero(&move, sizeof(move)));
        assert(engine->session->current_state.entities[0].coordinate.x == 0);
        assert(engine->session->current_state.entities[1].coordinate.x == 1);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_OK);
        assert(move.accepted == 1U && move.tick_count == 1U);
        assert(move.ticks[0].event_count == 2U);
        assert(move.initial_state.entities[0].coordinate.x == 0);
        assert(move.final_state.entities[0].coordinate.x == 1);
        assert(move.final_state.entities[1].coordinate.x == 2);
        game_rules_move_result_dispose(&move);

        memset(&move, 0xA5, sizeof(move));
        /* The successful first move retained exactly one pre-command history entry. */
        {
            const size_t history_baseline = tracker->live_count;
            tracker_begin_failure(tracker, 0U);
            assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
                   GAME_RULES_CALL_ALLOCATION_FAILED);
            assert(bytes_are_zero(&move, sizeof(move)));
            assert(engine->session->current_state.entities[0].coordinate.x == 1);
            assert(engine->session->current_state.entities[1].coordinate.x == 2);
            assert(tracker->live_count == history_baseline);
            tracker_begin_success(tracker);
            assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
                   GAME_RULES_CALL_OK);
            assert(move.accepted == 1U && move.ticks[0].event_count == 2U);
            assert(move.initial_state.entities[0].coordinate.x == 1);
            assert(move.final_state.entities[0].coordinate.x == 2);
            assert(move.final_state.entities[1].coordinate.x == 3);
            game_rules_move_result_dispose(&move);
        }
    }
    game_rules_engine_destroy(engine);
    assert(tracker->invalid_free_count == 0U);
}

static void expect_stage06_fall_failures(allocation_tracker* tracker,
                                         const game_rules_allocator_v1* allocator)
{
    const game_rules_cell cells[3] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 2, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 2, 0},
        {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
    const game_rules_entity entities[2] = {
        {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 4},
        {8U, GAME_RULES_ENTITY_BARREL, {1, 0}, 4}};
    const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        3U, 1U, cells, 3U, NULL, 0U, entities, 2U};
    game_rules_engine* engine;
    game_rules_load_result load = {0};
    game_rules_move_result move;
    char* json;
    size_t attempts;
    size_t failure;

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    tracker_begin_success(tracker);
    json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
    assert(json != NULL);
    assert(strstr(json, "\"index\":2") != NULL);
    assert(strstr(json, "\"type\":\"barrelExploded\"") != NULL);
    attempts = tracker->attempt;
    game_rules_string_free(json);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        baseline = tracker->live_count;
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST) == NULL);
        assert(engine->session->current_state.entity_count == 2U);
        assert(engine->session->current_state.entities[0].id == 1U);
        assert(engine->session->current_state.entities[0].coordinate.x == 0);
        assert(engine->session->current_state.entities[1].id == 8U);
        assert(engine->session->current_state.entities[1].coordinate.x == 1);
        assert(engine->session->current_state.armed_barrel_count == 0U);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
        assert(json != NULL && strstr(json, "\"index\":2") != NULL);
        game_rules_string_free(json);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    tracker_begin_success(tracker);
    memset(&move, 0, sizeof(move));
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
           GAME_RULES_CALL_OK);
    assert(move.accepted && move.tick_count == 3U);
    attempts = tracker->attempt;
    game_rules_move_result_dispose(&move);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        baseline = tracker->live_count;
        memset(&move, 0xA5, sizeof(move));
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(bytes_are_zero(&move, sizeof(move)));
        assert(engine->session->current_state.entities[0].coordinate.x == 0);
        assert(engine->session->current_state.entities[1].coordinate.x == 1);
        assert(engine->session->current_state.armed_barrel_count == 0U);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_OK);
        assert(move.accepted && move.tick_count == 3U);
        game_rules_move_result_dispose(&move);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }
}

static void expect_stage07_ramp_failures(allocation_tracker* tracker,
                                         const game_rules_allocator_v1* allocator)
{
    const game_rules_cell cells[5] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{1, 0}, GAME_RULES_CELL_RAMP, 0, GAME_RULES_DIRECTION_WEST},
        {{2, 0}, GAME_RULES_CELL_RAMP, 1, GAME_RULES_DIRECTION_WEST},
        {{3, 0}, GAME_RULES_CELL_FLAT, 2, 0},
        {{4, 0}, GAME_RULES_CELL_FLAT, 2, 0}};
    const game_rules_entity entities[2] = {
        {8U, GAME_RULES_ENTITY_BOX, {3, 0}, 4},
        {1U, GAME_RULES_ENTITY_PLAYER, {4, 0}, 4}};
    const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        5U, 1U, cells, 5U, NULL, 0U, entities, 2U};
    game_rules_engine* engine;
    game_rules_load_result load = {0};
    game_rules_move_result move;
    char* json;
    size_t attempts;
    size_t failure;

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    tracker_begin_success(tracker);
    json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_WEST);
    assert(json != NULL && strstr(json, "\"index\":2") != NULL);
    assert(strstr(json, "\"cause\":\"slide\"") != NULL);
    attempts = tracker->attempt;
    game_rules_string_free(json);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        baseline = tracker->live_count;
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_move(engine, GAME_RULES_DIRECTION_WEST) == NULL);
        assert(engine->session->current_state.entity_count == 2U);
        assert(engine->session->current_state.entities[0].id == 8U);
        assert(engine->session->current_state.entities[0].coordinate.x == 3);
        assert(engine->session->current_state.entities[1].id == 1U);
        assert(engine->session->current_state.entities[1].coordinate.x == 4);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_WEST);
        assert(json != NULL && strstr(json, "\"index\":2") != NULL);
        game_rules_string_free(json);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    tracker_begin_success(tracker);
    memset(&move, 0, sizeof(move));
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST, &move) ==
           GAME_RULES_CALL_OK);
    assert(move.accepted && move.tick_count == 3U);
    attempts = tracker->attempt;
    game_rules_move_result_dispose(&move);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        baseline = tracker->live_count;
        memset(&move, 0xA5, sizeof(move));
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST, &move) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(bytes_are_zero(&move, sizeof(move)));
        assert(engine->session->current_state.entities[0].id == 8U);
        assert(engine->session->current_state.entities[0].coordinate.x == 3);
        assert(engine->session->current_state.entities[1].id == 1U);
        assert(engine->session->current_state.entities[1].coordinate.x == 4);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST, &move) ==
               GAME_RULES_CALL_OK);
        assert(move.accepted && move.tick_count == 3U);
        assert(move.ticks[1].events[0].movement_cause ==
               GAME_RULES_MOVEMENT_SLIDE);
        assert(move.ticks[2].events[0].movement_cause ==
               GAME_RULES_MOVEMENT_SLIDE);
        game_rules_move_result_dispose(&move);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }
}

static void assert_fixture_command_unmodified(const game_rules_engine* engine)
{
    assert(engine->session->current_state.entity_count == 2U);
    assert(engine->session->current_state.entities[0].id == 1U);
    assert(engine->session->current_state.entities[0].coordinate.x == 0);
    assert(engine->session->current_state.entities[0].bottom_half_steps == 4);
    assert(engine->session->current_state.entities[1].id == 8U);
    assert(engine->session->current_state.entities[1].coordinate.x == 1);
    assert(engine->session->current_state.entities[1].bottom_half_steps == 4);
    assert(engine->session->current_state.active_switch_color_count == 0U);
    assert(engine->session->current_state.open_door_count == 0U);
    assert(engine->session->current_state.outcome == GAME_RULES_OUTCOME_ONGOING);
}

static void expect_stage08_fixture_failures(allocation_tracker* tracker,
                                            const game_rules_allocator_v1* allocator)
{
    const game_rules_cell cells[4] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 2, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 2, 0},
        {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{3, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
    const game_rules_fixture fixtures[2] = {
        {{2, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_GREEN},
        {{3, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_GREEN}};
    const game_rules_entity entities[2] = {
        {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 4},
        {8U, GAME_RULES_ENTITY_BOX, {1, 0}, 4}};
    const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        4U, 1U, cells, 4U, fixtures, 2U, entities, 2U};
    game_rules_engine* engine;
    game_rules_load_result load = {0};
    game_rules_move_result move;
    char* json;
    size_t attempts;
    size_t failure;

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    tracker_begin_success(tracker);
    json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
    assert(json != NULL);
    assert(strstr(json, "\"index\":1") != NULL);
    assert(strstr(json, "\"type\":\"switchChanged\",\"color\":\"green\",\"active\":true") != NULL);
    assert(strstr(json, "\"type\":\"doorOpened\",\"coordinate\":{\"x\":3,\"y\":0},\"color\":\"green\"") != NULL);
    attempts = tracker->attempt;
    game_rules_string_free(json);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        baseline = tracker->live_count;
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST) == NULL);
        assert_fixture_command_unmodified(engine);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
        assert(json != NULL && strstr(json, "\"index\":1") != NULL);
        game_rules_string_free(json);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    tracker_begin_success(tracker);
    memset(&move, 0, sizeof(move));
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
           GAME_RULES_CALL_OK);
    assert(move.accepted && move.tick_count == 2U);
    assert(move.ticks[1].event_count == 3U);
    assert(move.final_state.active_switch_color_count == 1U);
    assert(move.final_state.active_switch_colors[0] == GAME_RULES_COLOR_GREEN);
    assert(move.final_state.open_door_count == 1U);
    assert(move.final_state.open_doors[0].x == 3);
    attempts = tracker->attempt;
    game_rules_move_result_dispose(&move);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        baseline = tracker->live_count;
        memset(&move, 0xA5, sizeof(move));
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(bytes_are_zero(&move, sizeof(move)));
        assert_fixture_command_unmodified(engine);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_OK);
        assert(move.accepted && move.tick_count == 2U);
        assert(move.final_state.active_switch_color_count == 1U);
        assert(move.final_state.open_door_count == 1U);
        game_rules_move_result_dispose(&move);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    /* Exercise fixture-bearing JSON load allocation sites as well as move sites. */
    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    tracker_begin_success(tracker);
    json = game_rules_engine_load_level(engine, fixture_move_level_json,
                                        (uint32_t)strlen(fixture_move_level_json));
    assert(json != NULL && strstr(json, "\"status\":\"loaded\"") != NULL);
    attempts = tracker->attempt;
    game_rules_string_free(json);
    game_rules_engine_destroy(engine);
    for (failure = 0U; failure < attempts; ++failure) {
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_load_level(engine, fixture_move_level_json,
                                            (uint32_t)strlen(fixture_move_level_json)) == NULL);
        assert(game_rules_c_engine_session_marker(engine) == 0U);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }
}

static void assert_stage09_explosion_command_unmodified(
    const game_rules_engine* engine)
{
    const game_rules_c_state* state = &engine->session->current_state;
    assert(state->entity_count == 4U);
    assert(state->entities[0].id == 1U);
    assert(state->entities[0].coordinate.x == 0);
    assert(state->entities[0].bottom_half_steps == 4);
    assert(state->entities[1].id == 8U);
    assert(state->entities[1].coordinate.x == 1);
    assert(state->entities[1].bottom_half_steps == 4);
    assert(state->entities[2].id == 9U);
    assert(state->entities[2].coordinate.x == 3);
    assert(state->entities[2].bottom_half_steps == 0);
    assert(state->entities[3].id == 2U);
    assert(state->entities[3].coordinate.x == 5);
    assert(state->entities[3].bottom_half_steps == -2);
    assert(state->armed_barrel_count == 0U);
    assert(state->active_switch_color_count == 0U);
    assert(state->open_door_count == 0U);
    assert(state->outcome == GAME_RULES_OUTCOME_ONGOING);
}

static void expect_stage09_explosion_failures(
    allocation_tracker* tracker,
    const game_rules_allocator_v1* allocator)
{
    const game_rules_cell cells[7] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 2, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 2, 0},
        {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{3, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{4, 0}, GAME_RULES_CELL_FLAT, -1, 0},
        {{5, 0}, GAME_RULES_CELL_FLAT, -1, 0},
        {{6, 0}, GAME_RULES_CELL_FLAT, -2, 0}};
    const game_rules_entity entities[4] = {
        {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 4},
        {8U, GAME_RULES_ENTITY_BARREL, {1, 0}, 4},
        {9U, GAME_RULES_ENTITY_BARREL, {3, 0}, 0},
        {2U, GAME_RULES_ENTITY_BOX, {5, 0}, -2}};
    const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        7U, 1U, cells, 7U, NULL, 0U, entities, 4U};
    game_rules_engine* engine;
    game_rules_load_result load = {0};
    game_rules_move_result move;
    char* json;
    size_t attempts;
    size_t failure;

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    tracker_begin_success(tracker);
    json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
    assert(json != NULL);
    assert(strstr(json, "\"index\":5") != NULL);
    assert(strstr(json, "\"entityId\":\"8\"") != NULL);
    assert(strstr(json, "\"entityId\":\"9\"") != NULL);
    attempts = tracker->attempt;
    game_rules_string_free(json);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        baseline = tracker->live_count;
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST) == NULL);
        assert_stage09_explosion_command_unmodified(engine);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
        assert(json != NULL && strstr(json, "\"index\":5") != NULL);
        game_rules_string_free(json);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) ==
           GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    tracker_begin_success(tracker);
    memset(&move, 0, sizeof(move));
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
           GAME_RULES_CALL_OK);
    assert(move.accepted && move.tick_count == 6U);
    attempts = tracker->attempt;
    game_rules_move_result_dispose(&move);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) ==
               GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        baseline = tracker->live_count;
        memset(&move, 0xA5, sizeof(move));
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(bytes_are_zero(&move, sizeof(move)));
        assert_stage09_explosion_command_unmodified(engine);
        assert(tracker->live_count == baseline);
        tracker_begin_success(tracker);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_OK);
        assert(move.accepted && move.tick_count == 6U);
        assert(move.ticks[2].events[0].kind == GAME_RULES_EVENT_BARREL_EXPLODED);
        assert(move.ticks[4].events[0].kind == GAME_RULES_EVENT_BARREL_EXPLODED);
        game_rules_move_result_dispose(&move);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }
}

static void expect_stage10_history_failures(
    allocation_tracker* tracker,
    const game_rules_allocator_v1* allocator)
{
    static const game_rules_cell cells[3] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
    static const game_rules_entity player = {
        1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0};
    static const game_rules_entity replacement_player = {
        99U, GAME_RULES_ENTITY_PLAYER, {2, 0}, 0};
    static const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        3U, 1U, cells, 3U, NULL, 0U, &player, 1U};
    static const game_rules_level_definition replacement = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        3U, 1U, cells, 3U, NULL, 0U, &replacement_player, 1U};
    game_rules_engine* engine;
    game_rules_load_result load;
    game_rules_move_result move;
    game_rules_rewind_result rewind;
    char* json;
    size_t attempts;
    size_t failure;

    /* Legacy rewind allocates its complete response before consuming history. */
    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    assert(engine != NULL);
    memset(&load, 0, sizeof(load));
    memset(&move, 0, sizeof(move));
    assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
           GAME_RULES_CALL_OK);
    game_rules_move_result_dispose(&move);
    tracker_begin_success(tracker);
    json = game_rules_engine_rewind(engine);
    assert(json != NULL && strstr(json, "\"status\":\"rewound\"") != NULL);
    attempts = tracker->attempt;
    game_rules_string_free(json);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        memset(&load, 0, sizeof(load));
        memset(&move, 0, sizeof(move));
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_OK);
        game_rules_move_result_dispose(&move);
        baseline = tracker->live_count;
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_rewind(engine) == NULL);
        assert(engine->session->current_state.entities[0].coordinate.x == 1);
        assert(engine->session->history_count == 1U);
        assert(tracker->live_count == baseline);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    /* The typed rewind arena has the same pre-commit rollback guarantee. */
    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    memset(&load, 0, sizeof(load));
    memset(&move, 0, sizeof(move));
    memset(&rewind, 0, sizeof(rewind));
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
           GAME_RULES_CALL_OK);
    game_rules_move_result_dispose(&move);
    tracker_begin_success(tracker);
    assert(game_rules_engine_rewind_data(engine, &rewind) == GAME_RULES_CALL_OK);
    attempts = tracker->attempt;
    game_rules_rewind_result_dispose(&rewind);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        memset(&load, 0, sizeof(load));
        memset(&move, 0, sizeof(move));
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_OK);
        game_rules_move_result_dispose(&move);
        baseline = tracker->live_count;
        memset(&rewind, 0xA5, sizeof(rewind));
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_rewind_data(engine, &rewind) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(bytes_are_zero(&rewind, sizeof(rewind)));
        assert(engine->session->current_state.entities[0].coordinate.x == 1);
        assert(engine->session->history_count == 1U);
        assert(tracker->live_count == baseline);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    /* Valid replacement failures preserve both current state and the complete old history. */
    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    memset(&load, 0, sizeof(load));
    memset(&move, 0, sizeof(move));
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
           GAME_RULES_CALL_OK);
    game_rules_move_result_dispose(&move);
    tracker_begin_success(tracker);
    json = game_rules_engine_load_level(engine, valid_level_json,
                                        (uint32_t)strlen(valid_level_json));
    assert(json != NULL && strstr(json, "\"status\":\"loaded\"") != NULL);
    attempts = tracker->attempt;
    game_rules_string_free(json);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        memset(&load, 0, sizeof(load));
        memset(&move, 0, sizeof(move));
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_OK);
        game_rules_move_result_dispose(&move);
        baseline = tracker->live_count;
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_load_level(engine, valid_level_json,
                                            (uint32_t)strlen(valid_level_json)) == NULL);
        assert(engine->session->current_state.entities[0].coordinate.x == 1);
        assert(engine->session->history_count == 1U);
        assert(tracker->live_count == baseline);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }

    tracker_begin_success(tracker);
    engine = game_rules_engine_create_with_allocator_v1(allocator);
    memset(&load, 0, sizeof(load));
    memset(&move, 0, sizeof(move));
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
           GAME_RULES_CALL_OK);
    game_rules_move_result_dispose(&move);
    tracker_begin_success(tracker);
    memset(&load, 0, sizeof(load));
    assert(game_rules_engine_load_level_data(engine, &replacement, &load) ==
           GAME_RULES_CALL_OK);
    attempts = tracker->attempt;
    game_rules_load_result_dispose(&load);
    game_rules_engine_destroy(engine);

    for (failure = 0U; failure < attempts; ++failure) {
        size_t baseline;
        tracker_begin_success(tracker);
        engine = game_rules_engine_create_with_allocator_v1(allocator);
        memset(&load, 0, sizeof(load));
        memset(&move, 0, sizeof(move));
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
               GAME_RULES_CALL_OK);
        game_rules_move_result_dispose(&move);
        baseline = tracker->live_count;
        memset(&load, 0xA5, sizeof(load));
        tracker_begin_failure(tracker, failure);
        assert(game_rules_engine_load_level_data(engine, &replacement, &load) ==
               GAME_RULES_CALL_ALLOCATION_FAILED);
        assert(bytes_are_zero(&load, sizeof(load)));
        assert(engine->session->current_state.entities[0].coordinate.x == 1);
        assert(engine->session->history_count == 1U);
        assert(tracker->live_count == baseline);
        game_rules_engine_destroy(engine);
        assert(tracker->invalid_free_count == 0U);
    }
}

int main(void)
{
    static allocation_tracker tracker;
    game_rules_allocator_v1 allocator = allocator_for(&tracker);
    game_rules_allocator_v1 invalid = allocator;
    game_rules_engine* engine;
    game_rules_engine* other;
    game_rules_load_result load = {0};
    game_rules_move_result move = {0};
    char* json;

    assert(game_rules_engine_create_with_allocator_v1(NULL) == NULL);
    invalid.api_version = 99U;
    assert(game_rules_engine_create_with_allocator_v1(&invalid) == NULL);
    invalid = allocator;
    invalid.struct_size = 0U;
    assert(game_rules_engine_create_with_allocator_v1(&invalid) == NULL);
    invalid = allocator;
    invalid.allocate = NULL;
    assert(game_rules_engine_create_with_allocator_v1(&invalid) == NULL);
    invalid = allocator;
    invalid.deallocate = NULL;
    assert(game_rules_engine_create_with_allocator_v1(&invalid) == NULL);
    assert(tracker.attempt == 0U);

    tracker_begin_failure(&tracker, 0U);
    assert(game_rules_engine_create_with_allocator_v1(&allocator) == NULL);
    assert(tracker.attempt == 1U);
    assert(tracker.live_count == 0U);

    tracker_begin_success(&tracker);
    engine = game_rules_engine_create_with_allocator_v1(&allocator);
    assert(engine != NULL);
    assert(tracker.attempt == 1U);
    assert(tracker.live_count == 1U);

    /* A second engine has separate state despite sharing an allocator. */
    tracker_begin_success(&tracker);
    other = game_rules_engine_create_with_allocator_v1(&allocator);
    assert(other != NULL);
    assert(game_rules_c_engine_replace_session(other, 77U, 0U, 0U) ==
           GAME_RULES_CALL_OK);
    assert(game_rules_c_engine_session_marker(engine) == 0U);
    assert(game_rules_c_engine_session_marker(other) == 77U);
    game_rules_engine_destroy(other);
    assert(tracker.invalid_free_count == 0U);

    expect_one_allocation_failure_legacy(&tracker, engine);
    expect_one_allocation_failure_typed(&tracker, engine);
    expect_atomic_replacement_failures(&tracker, engine);
    game_rules_engine_destroy(engine);
    assert(tracker.live_count == 0U);
    expect_stage03_load_failures(&tracker, &allocator);
    expect_stage04_move_failures(&tracker, &allocator);
    expect_stage05_push_failures(&tracker, &allocator);
    expect_stage06_fall_failures(&tracker, &allocator);
    expect_stage07_ramp_failures(&tracker, &allocator);
    expect_stage08_fixture_failures(&tracker, &allocator);
    expect_stage09_explosion_failures(&tracker, &allocator);
    expect_stage10_history_failures(&tracker, &allocator);

    tracker_begin_success(&tracker);
    engine = game_rules_engine_create_with_allocator_v1(&allocator);
    assert(engine != NULL);

    /* A result graph remains live through replacement and engine destruction. */
    tracker_begin_success(&tracker);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_SOUTH, &move) ==
           GAME_RULES_CALL_OK);
    load.owned_storage = game_rules_c_engine_allocate_result_storage(engine, 16U);
    assert(load.owned_storage != NULL);
    assert(move.events[0].direction == GAME_RULES_DIRECTION_SOUTH);
    assert(game_rules_c_engine_replace_session(engine, 33U, 4U, 4U) ==
           GAME_RULES_CALL_OK);
    game_rules_engine_destroy(engine);
    assert(move.events[0].move_status == GAME_RULES_MOVE_NO_LEVEL);
    assert(tracker.live_count == 2U);
    game_rules_move_result_dispose(&move);
    game_rules_move_result_dispose(&move);
    game_rules_load_result_dispose(&load);
    game_rules_load_result_dispose(&load);
    assert(tracker.live_count == 0U);
    assert(tracker.invalid_free_count == 0U);

    /* Legacy JSON carries its allocator owner after engine destruction too. */
    tracker_begin_success(&tracker);
    engine = game_rules_engine_create_with_allocator_v1(&allocator);
    assert(engine != NULL);
    json = game_rules_engine_get_state(engine);
    assert(json != NULL);
    game_rules_engine_destroy(engine);
    assert(strstr(json, "\"status\":\"no_level\"") != NULL);
    assert(tracker.live_count == 1U);
    game_rules_string_free(json);
    assert(tracker.live_count == 0U);
    assert(tracker.invalid_free_count == 0U);
    return 0;
}
