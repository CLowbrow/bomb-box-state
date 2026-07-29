#include "game_rules/c_api.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void expect_json(char* actual, const char* expected)
{
    assert(actual != NULL);
    assert(strcmp(actual, expected) == 0);
    game_rules_string_free(actual);
}

int main(void)
{
    game_rules_engine* engine;
    game_rules_state_result state = {0};
    game_rules_move_result move = {0};
    game_rules_rewind_result rewind = {0};

    assert(game_rules_api_version() == 1U);
    assert(game_rules_data_api_version() == 1U);
    assert(strcmp(game_rules_engine_status(), "c17_skeleton") == 0);

    engine = game_rules_engine_create();
    assert(engine != NULL);
    expect_json(game_rules_engine_get_state(engine),
                "{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\"no_level\",\"state\":null}");
    expect_json(game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST),
                "{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"no_level\",\"accepted\":false,\"direction\":\"east\",\"events\":[{\"type\":\"moveBlocked\",\"direction\":\"east\",\"reason\":\"no_level\"}],\"initialState\":null,\"ticks\":[],\"state\":null,\"outcome\":null}");
    expect_json(game_rules_engine_rewind(engine),
                "{\"apiVersion\":1,\"operation\":\"rewind\",\"status\":\"history_empty\",\"accepted\":false,\"events\":[],\"state\":null,\"outcome\":null}");
    expect_json(game_rules_engine_load_level(engine, "{}", 2U),
                "{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"not_implemented\",\"state\":null}");
    expect_json(game_rules_engine_load_level(engine, NULL, 0U),
                "{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"invalid_argument\",\"state\":null}");

    assert(game_rules_engine_get_state_data(engine, &state) == GAME_RULES_CALL_OK);
    assert(state.has_state == 0U);
    assert(state.owned_storage != NULL);
    game_rules_state_result_dispose(&state);
    assert(state.owned_storage == NULL);

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
    game_rules_move_result_dispose(&move);

    assert(game_rules_engine_rewind_data(engine, &rewind) == GAME_RULES_CALL_OK);
    assert(rewind.status == GAME_RULES_REWIND_HISTORY_EMPTY);
    assert(rewind.accepted == 0U);
    game_rules_rewind_result_dispose(&rewind);

    game_rules_engine_destroy(engine);
    expect_json(game_rules_engine_get_state(NULL),
                "{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\"invalid_engine\",\"state\":null}");
    game_rules_engine_destroy(NULL);
    game_rules_string_free(NULL);
    game_rules_state_result_dispose(NULL);
    game_rules_load_result_dispose(NULL);
    game_rules_move_result_dispose(NULL);
    game_rules_rewind_result_dispose(NULL);
    return 0;
}
