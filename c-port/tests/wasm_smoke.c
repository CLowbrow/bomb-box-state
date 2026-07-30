#include "game_rules/c_api.h"

#include <string.h>

int main(void)
{
    game_rules_state_result state = {0};
    game_rules_move_result move = {0};
    game_rules_move_result walk = {0};
    game_rules_move_result push = {0};
    game_rules_move_result fall = {0};
    game_rules_move_result fixture_walk = {0};
    game_rules_move_result fixture_win = {0};
    game_rules_move_result terminal = {0};
    game_rules_rewind_result rewind = {0};
    game_rules_load_result load = {0};
    game_rules_load_result chain = {0};
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
    {
        static const game_rules_cell cells[2] = {
            {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
        static const game_rules_entity player =
            {17U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0};
        const game_rules_level_definition level = {
            {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
            2U, 1U, cells, 2U, 0, 0U, &player, 1U};
        game_rules_load_result replacement = {0};
        if (game_rules_engine_load_level_data(engine, &level, &replacement) !=
                GAME_RULES_CALL_OK || !replacement.accepted) {
            return 10;
        }
        game_rules_load_result_dispose(&replacement);
        json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
        if (json == 0 || strstr(json, "\"status\":\"moved\"") == 0 ||
            strstr(json, "\"cause\":\"player\"") == 0) {
            game_rules_string_free(json);
            return 11;
        }
        game_rules_string_free(json);
        if (game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST, &walk) !=
                GAME_RULES_CALL_OK || !walk.accepted || walk.tick_count != 1U ||
            walk.ticks[0].events[0].movement_cause != GAME_RULES_MOVEMENT_PLAYER ||
            walk.state.resolved.entities[0].coordinate.x != 0) {
            return 12;
        }
    }
    {
        static const game_rules_cell cells[4] = {
            {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{3, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
        static const game_rules_entity entities[2] = {
            {17U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
            {4U, GAME_RULES_ENTITY_BOX, {1, 0}, 0}};
        const game_rules_level_definition level = {
            {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
            4U, 1U, cells, 4U, 0, 0U, entities, 2U};
        game_rules_load_result replacement = {0};
        if (game_rules_engine_load_level_data(engine, &level, &replacement) !=
                GAME_RULES_CALL_OK || !replacement.accepted) {
            return 14;
        }
        game_rules_load_result_dispose(&replacement);
        json = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
        if (json == 0 || strstr(json, "\"status\":\"moved\"") == 0 ||
            strstr(json, "\"entityId\":\"17\"") == 0 ||
            strstr(json, "\"entityId\":\"4\"") == 0) {
            game_rules_string_free(json);
            return 15;
        }
        game_rules_string_free(json);
        if (game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &push) !=
                GAME_RULES_CALL_OK || !push.accepted || push.tick_count != 1U ||
            push.ticks[0].event_count != 2U ||
            push.ticks[0].events[0].entity_id != 17U ||
            push.ticks[0].events[1].entity_id != 4U ||
            push.state.resolved.entities[0].coordinate.x != 2 ||
            push.state.resolved.entities[1].coordinate.x != 3) {
            return 16;
        }
    }
    {
        static const game_rules_cell cells[3] = {
            {{0, 0}, GAME_RULES_CELL_FLAT, 2, 0},
            {{1, 0}, GAME_RULES_CELL_FLAT, 2, 0},
            {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
        static const game_rules_entity entities[2] = {
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 4},
            {8U, GAME_RULES_ENTITY_BARREL, {1, 0}, 4}};
        const game_rules_level_definition level = {
            {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
            3U, 1U, cells, 3U, 0, 0U, entities, 2U};
        game_rules_load_result replacement = {0};
        if (game_rules_engine_load_level_data(engine, &level, &replacement) !=
                GAME_RULES_CALL_OK || !replacement.accepted) {
            return 17;
        }
        game_rules_load_result_dispose(&replacement);
        if (game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &fall) !=
                GAME_RULES_CALL_OK || !fall.accepted || fall.tick_count != 3U ||
            fall.ticks[1].events[0].movement_cause != GAME_RULES_MOVEMENT_FALL ||
            fall.ticks[1].events[1].kind != GAME_RULES_EVENT_BARREL_ARMED ||
            fall.ticks[2].events[0].kind != GAME_RULES_EVENT_BARREL_EXPLODED ||
            fall.state.resolved.entity_count != 1U) {
            return 18;
        }
    }
    {
        static const game_rules_cell cells[8] = {
            {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{3, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{0, 1}, GAME_RULES_CELL_FLAT, 0, 0},
            {{1, 1}, GAME_RULES_CELL_FLAT, 0, 0},
            {{2, 1}, GAME_RULES_CELL_FLAT, 0, 0},
            {{3, 1}, GAME_RULES_CELL_FLAT, 0, 0}};
        static const game_rules_entity entities[3] = {
            {8U, GAME_RULES_ENTITY_BARREL, {0, 0}, 2},
            {9U, GAME_RULES_ENTITY_BARREL, {1, 0}, 0},
            {1U, GAME_RULES_ENTITY_PLAYER, {3, 1}, 0}};
        const game_rules_level_definition level = {
            {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
            4U, 2U, cells, 8U, 0, 0U, entities, 3U};
        if (game_rules_engine_load_level_data(engine, &level, &chain) !=
                GAME_RULES_CALL_OK || !chain.accepted || chain.tick_count != 3U ||
            chain.ticks[0].event_count != 2U ||
            chain.ticks[0].events[1].kind != GAME_RULES_EVENT_BARREL_ARMED ||
            chain.ticks[1].event_count != 3U ||
            chain.ticks[1].events[0].kind != GAME_RULES_EVENT_BARREL_EXPLODED ||
            chain.ticks[1].events[0].entity_id != 8U ||
            chain.ticks[1].events[1].movement_cause != GAME_RULES_MOVEMENT_BLAST ||
            chain.ticks[1].events[2].kind != GAME_RULES_EVENT_BARREL_ARMED ||
            chain.ticks[1].events[2].entity_id != 9U ||
            chain.ticks[2].event_count != 1U ||
            chain.ticks[2].events[0].kind != GAME_RULES_EVENT_BARREL_EXPLODED ||
            chain.ticks[2].events[0].entity_id != 9U ||
            chain.ticks[2].events[0].coordinate.x != 2 ||
            chain.final_state.armed_barrel_count != 0U ||
            chain.final_state.entity_count != 1U) {
            return 30;
        }
    }
    {
        static const game_rules_cell cells[3] = {
            {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
        static const game_rules_fixture fixtures[3] = {
            {{2, 0}, GAME_RULES_FIXTURE_EXIT, GAME_RULES_COLOR_YELLOW},
            {{1, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_RED},
            {{0, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED}};
        static const game_rules_entity player =
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0};
        const game_rules_level_definition level = {
            {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
            3U, 1U, cells, 3U, fixtures, 3U, &player, 1U};
        game_rules_load_result replacement = {0};
        if (game_rules_engine_load_level_data(engine, &level, &replacement) !=
                GAME_RULES_CALL_OK || !replacement.accepted ||
            replacement.tick_count != 1U ||
            replacement.ticks[0].event_count != 2U ||
            replacement.ticks[0].events[0].kind !=
                GAME_RULES_EVENT_SWITCH_CHANGED ||
            replacement.ticks[0].events[1].kind != GAME_RULES_EVENT_DOOR_OPENED ||
            replacement.final_state.active_switch_color_count != 1U ||
            replacement.final_state.open_door_count != 1U) {
            return 19;
        }
        game_rules_load_result_dispose(&replacement);
        if (game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                        &fixture_walk) != GAME_RULES_CALL_OK ||
            !fixture_walk.accepted || fixture_walk.tick_count != 1U ||
            fixture_walk.final_state.open_door_count != 1U) {
            return 20;
        }
        if (game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                        &fixture_win) != GAME_RULES_CALL_OK ||
            !fixture_win.accepted || fixture_win.tick_count != 1U ||
            fixture_win.ticks[0].event_count != 2U ||
            fixture_win.ticks[0].events[1].kind != GAME_RULES_EVENT_LEVEL_WON ||
            fixture_win.final_state.outcome != GAME_RULES_OUTCOME_WON) {
            return 21;
        }
        if (game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST,
                                        &terminal) != GAME_RULES_CALL_OK ||
            terminal.accepted || terminal.status != GAME_RULES_MOVE_LEVEL_TERMINAL ||
            terminal.state.resolved.outcome != GAME_RULES_OUTCOME_WON) {
            return 22;
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
    game_rules_load_result_dispose(&chain);
    game_rules_rewind_result_dispose(&rewind);
    game_rules_move_result_dispose(&move);
    game_rules_move_result_dispose(&walk);
    game_rules_move_result_dispose(&push);
    game_rules_move_result_dispose(&fall);
    game_rules_move_result_dispose(&fixture_walk);
    game_rules_move_result_dispose(&fixture_win);
    game_rules_move_result_dispose(&terminal);
    game_rules_state_result_dispose(&state);
    return game_rules_api_version() == 1U ? 0 : 13;
}
