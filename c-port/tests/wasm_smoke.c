#include "game_rules/c_api.h"

#include <string.h>

int main(void)
{
    game_rules_state_result state = {0};
    game_rules_move_result move = {0};
    game_rules_rewind_result rewind = {0};
    game_rules_load_result load = {0};
    game_rules_state_result loaded_state = {0};
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

    {
        static const game_rules_cell cells[2] = {
            {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{1, 0}, GAME_RULES_CELL_FLAT, 1, 0}};
        static const game_rules_entity entities[2] = {
            {UINT64_MAX, GAME_RULES_ENTITY_BARREL, {0, 0}, 4},
            {1U, GAME_RULES_ENTITY_PLAYER, {1, 0}, 2}};
        const game_rules_level_definition level = {
            {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
            2U, 1U, cells, 2U, 0, 0U, entities, 2U};
        if (game_rules_engine_load_level_data(engine, &level, &load) !=
                GAME_RULES_CALL_OK ||
            load.tick_count != 2U ||
            load.ticks[0].state_after.armed_barrel_count != 1U ||
            load.ticks[0].state_after.armed_barrel_ids[0] != UINT64_MAX ||
            load.state.resolved.entity_count != 1U ||
            load.state.resolved.entities[0].id != 1U) {
            game_rules_rewind_result_dispose(&rewind);
            game_rules_move_result_dispose(&move);
            game_rules_state_result_dispose(&state);
            game_rules_engine_destroy(engine);
            return 6;
        }
        if (game_rules_engine_get_state_data(engine, &loaded_state) != GAME_RULES_CALL_OK ||
            !loaded_state.has_state || loaded_state.state.resolved.entity_count != 1U ||
            loaded_state.state.resolved.entities == load.state.resolved.entities) {
            game_rules_load_result_dispose(&load);
            game_rules_rewind_result_dispose(&rewind);
            game_rules_move_result_dispose(&move);
            game_rules_state_result_dispose(&state);
            game_rules_engine_destroy(engine);
            return 7;
        }
    }
    game_rules_engine_destroy(engine);
    if (move.events[0].move_status != GAME_RULES_MOVE_NO_LEVEL) {
        return 8;
    }
    if (load.ticks[0].state_after.armed_barrel_ids[0] != UINT64_MAX ||
        loaded_state.state.resolved.entities[0].id != 1U) {
        return 9;
    }
    game_rules_state_result_dispose(&loaded_state);
    game_rules_load_result_dispose(&load);
    game_rules_rewind_result_dispose(&rewind);
    game_rules_move_result_dispose(&move);
    game_rules_state_result_dispose(&state);
    return game_rules_api_version() == 1U ? 0 : 10;
}
