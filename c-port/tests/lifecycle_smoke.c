#include "game_rules/c_api.h"
#include "game_rules/c_allocator_api.h"

#include "c_api_internal.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static void expect_json(char* actual, const char* expected)
{
    assert(actual != NULL);
    assert(strcmp(actual, expected) == 0);
    game_rules_string_free(actual);
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

int main(void)
{
    game_rules_engine* engine;
    game_rules_state_result state = {0};
    game_rules_load_result load = {0};
    game_rules_move_result move = {0};
    game_rules_move_result invalid_move = {0};
    game_rules_rewind_result rewind = {0};
    char* state_json;

    assert(game_rules_api_version() == 1U);
    assert(game_rules_data_api_version() == 1U);
    assert(game_rules_allocator_api_version() == GAME_RULES_ALLOCATOR_API_VERSION_1);
    assert(strcmp(game_rules_engine_status(), "schema_ready") == 0);

    engine = game_rules_engine_create();
    assert(engine != NULL);
    expect_json(game_rules_engine_get_state(engine),
                "{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\"no_level\",\"state\":null}");
    expect_json(game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST),
                "{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"no_level\",\"accepted\":false,\"direction\":\"east\",\"events\":[{\"type\":\"moveBlocked\",\"direction\":\"east\",\"reason\":\"no_level\"}],\"initialState\":null,\"ticks\":[],\"state\":null,\"outcome\":null}");
    expect_json(game_rules_engine_move(engine, 99U),
                "{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"invalid_direction\",\"accepted\":false,\"direction\":null,\"events\":[],\"initialState\":null,\"ticks\":[],\"state\":null,\"outcome\":null}");
    expect_json(game_rules_engine_rewind(engine),
                "{\"apiVersion\":1,\"operation\":\"rewind\",\"status\":\"history_empty\",\"accepted\":false,\"events\":[],\"state\":null,\"outcome\":null}");
    expect_json(game_rules_engine_load_level(engine, "{}", 2U),
                "{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"invalid_json\",\"error\":{\"code\":\"missing_member\",\"byteOffset\":0,\"path\":\"/format\"},\"state\":null}");
    expect_json(game_rules_engine_load_level(engine, NULL, 0U),
                "{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"invalid_argument\",\"state\":null}");

    assert(game_rules_engine_get_state_data(engine, &state) == GAME_RULES_CALL_OK);
    assert(state.has_state == 0U);
    assert(state.owned_storage != NULL);

    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST, &move) ==
           GAME_RULES_CALL_OK);
    assert(move.status == GAME_RULES_MOVE_NO_LEVEL);
    assert(move.accepted == 0U);
    assert(move.has_direction == 1U);
    assert(move.direction == GAME_RULES_DIRECTION_WEST);
    assert(move.event_count == 1U);
    assert(move.events != NULL);
    assert(move.events[0].kind == GAME_RULES_EVENT_MOVE_BLOCKED);
    assert(move.events[0].direction == GAME_RULES_DIRECTION_WEST);
    assert(move.events[0].move_status == GAME_RULES_MOVE_NO_LEVEL);

    assert(game_rules_engine_move_data(engine, 99U, &invalid_move) == GAME_RULES_CALL_OK);
    assert(invalid_move.status == GAME_RULES_MOVE_INVALID_DIRECTION);
    assert(invalid_move.accepted == 0U);
    assert(invalid_move.has_direction == 0U);
    assert(invalid_move.events == NULL);
    assert(invalid_move.event_count == 0U);
    assert(invalid_move.owned_storage != NULL);

    assert(game_rules_engine_rewind_data(engine, &rewind) == GAME_RULES_CALL_OK);
    assert(rewind.status == GAME_RULES_REWIND_HISTORY_EMPTY);
    assert(rewind.accepted == 0U);
    assert(rewind.owned_storage != NULL);

    /* An independently allocated load result owner is safe to dispose. */
    load.status = GAME_RULES_LOAD_INVALID_LEVEL;
    load.owned_storage = game_rules_c_engine_allocate_result_storage(engine, 16U);
    assert(load.owned_storage != NULL);

    /* Disposed zeroed results can be reused by their matching operation. */
    game_rules_state_result_dispose(&state);
    assert(game_rules_engine_get_state_data(engine, &state) == GAME_RULES_CALL_OK);
    game_rules_move_result_dispose(&invalid_move);
    assert(game_rules_engine_move_data(engine, 99U, &invalid_move) == GAME_RULES_CALL_OK);
    game_rules_rewind_result_dispose(&rewind);
    assert(game_rules_engine_rewind_data(engine, &rewind) == GAME_RULES_CALL_OK);
    game_rules_load_result_dispose(&load);
    load.status = GAME_RULES_LOAD_INVALID_LEVEL;
    load.owned_storage = game_rules_c_engine_allocate_result_storage(engine, 16U);
    assert(load.owned_storage != NULL);

    /* Caller-owned results are independent of later calls and replacement. */
    state_json = game_rules_engine_get_state(engine);
    assert(state_json != NULL);
    assert(game_rules_c_engine_replace_session(engine, 17U, 8U, 12U) == GAME_RULES_CALL_OK);
    assert(game_rules_c_engine_session_marker(engine) == 17U);
    assert(move.events[0].direction == GAME_RULES_DIRECTION_WEST);
    game_rules_engine_destroy(engine);
    assert(strcmp(state_json,
                  "{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\"no_level\",\"state\":null}") == 0);
    assert(move.events[0].move_status == GAME_RULES_MOVE_NO_LEVEL);

    game_rules_string_free(state_json);
    game_rules_state_result_dispose(&state);
    game_rules_state_result_dispose(&state);
    assert(bytes_are_zero(&state, sizeof(state)));
    game_rules_move_result_dispose(&move);
    game_rules_move_result_dispose(&move);
    assert(bytes_are_zero(&move, sizeof(move)));
    game_rules_move_result_dispose(&invalid_move);
    game_rules_rewind_result_dispose(&rewind);
    game_rules_rewind_result_dispose(&rewind);
    assert(bytes_are_zero(&rewind, sizeof(rewind)));
    game_rules_load_result_dispose(&load);
    game_rules_load_result_dispose(&load);
    assert(bytes_are_zero(&load, sizeof(load)));

    /* Every matching disposer accepts a zero owner, clears, and is double-safe. */
    memset(&load, 0xA5, sizeof(load));
    load.owned_storage = NULL;
    game_rules_load_result_dispose(&load);
    game_rules_load_result_dispose(&load);
    assert(bytes_are_zero(&load, sizeof(load)));

    memset(&state, 0xA5, sizeof(state));
    assert(game_rules_engine_get_state_data(NULL, &state) == GAME_RULES_CALL_INVALID_ENGINE);
    assert(bytes_are_zero(&state, sizeof(state)));
    assert(game_rules_engine_get_state_data(NULL, NULL) == GAME_RULES_CALL_INVALID_ARGUMENT);

    memset(&move, 0xA5, sizeof(move));
    assert(game_rules_engine_move_data(NULL, GAME_RULES_DIRECTION_EAST, &move) ==
           GAME_RULES_CALL_INVALID_ENGINE);
    assert(bytes_are_zero(&move, sizeof(move)));
    assert(game_rules_engine_move_data(NULL, GAME_RULES_DIRECTION_EAST, NULL) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);

    memset(&rewind, 0xA5, sizeof(rewind));
    assert(game_rules_engine_rewind_data(NULL, &rewind) == GAME_RULES_CALL_INVALID_ENGINE);
    assert(bytes_are_zero(&rewind, sizeof(rewind)));
    assert(game_rules_engine_rewind_data(NULL, NULL) == GAME_RULES_CALL_INVALID_ARGUMENT);

    memset(&load, 0xA5, sizeof(load));
    assert(game_rules_engine_load_level_data(NULL, NULL, &load) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(bytes_are_zero(&load, sizeof(load)));
    assert(game_rules_engine_load_level_data(NULL, NULL, NULL) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);

    expect_json(game_rules_engine_get_state(NULL),
                "{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\"invalid_engine\",\"state\":null}");
    expect_json(game_rules_engine_move(NULL, GAME_RULES_DIRECTION_EAST),
                "{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"invalid_engine\",\"state\":null}");
    expect_json(game_rules_engine_rewind(NULL),
                "{\"apiVersion\":1,\"operation\":\"rewind\",\"status\":\"invalid_engine\",\"state\":null}");
    expect_json(game_rules_engine_load_level(NULL, NULL, 0U),
                "{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"invalid_engine\",\"state\":null}");

    game_rules_engine_destroy(NULL);
    game_rules_string_free(NULL);
    game_rules_state_result_dispose(NULL);
    game_rules_load_result_dispose(NULL);
    game_rules_move_result_dispose(NULL);
    game_rules_rewind_result_dispose(NULL);
    return 0;
}
