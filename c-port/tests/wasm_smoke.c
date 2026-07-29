#include "game_rules/c_api.h"

#include <string.h>

int main(void)
{
    game_rules_state_result state = {0};
    game_rules_move_result move = {0};
    game_rules_rewind_result rewind = {0};
    char* json;
    game_rules_engine* const engine = game_rules_engine_create();
    if (engine == 0) {
        return 1;
    }

    json = game_rules_engine_get_state(engine);
    if (json == 0 || strstr(json, "\"status\":\"no_level\"") == 0) {
        game_rules_string_free(json);
        game_rules_engine_destroy(engine);
        return 2;
    }
    game_rules_string_free(json);
    if (game_rules_engine_get_state_data(engine, &state) != GAME_RULES_CALL_OK ||
        state.has_state != 0U || state.owned_storage == 0) {
        game_rules_engine_destroy(engine);
        return 3;
    }
    if (game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) !=
            GAME_RULES_CALL_OK ||
        move.status != GAME_RULES_MOVE_NO_LEVEL || move.event_count != 1U) {
        game_rules_state_result_dispose(&state);
        game_rules_engine_destroy(engine);
        return 4;
    }
    if (game_rules_engine_rewind_data(engine, &rewind) != GAME_RULES_CALL_OK ||
        rewind.status != GAME_RULES_REWIND_HISTORY_EMPTY) {
        game_rules_move_result_dispose(&move);
        game_rules_state_result_dispose(&state);
        game_rules_engine_destroy(engine);
        return 5;
    }
    game_rules_engine_destroy(engine);
    if (move.events[0].move_status != GAME_RULES_MOVE_NO_LEVEL) {
        return 6;
    }
    game_rules_rewind_result_dispose(&rewind);
    game_rules_move_result_dispose(&move);
    game_rules_state_result_dispose(&state);
    return game_rules_api_version() == 1U ? 0 : 7;
}
