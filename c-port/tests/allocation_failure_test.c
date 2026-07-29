#include "game_rules/c_api.h"
#include "game_rules/c_allocator_api.h"

#include "c_api_internal.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

enum { MAX_TRACKED_ALLOCATIONS = 64 };

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

int main(void)
{
    allocation_tracker tracker = {0};
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
