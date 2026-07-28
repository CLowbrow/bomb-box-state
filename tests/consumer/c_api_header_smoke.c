#include "game_rules/c_api.h"

#include <stddef.h>

int main(void)
{
    game_rules_engine* engine = game_rules_engine_create();
    if (engine == NULL || GAME_RULES_DIRECTION_NORTH != 0 || GAME_RULES_DIRECTION_WEST != 3 ||
        game_rules_data_api_version() != 1) {
        game_rules_engine_destroy(engine);
        return 1;
    }

    game_rules_state_result typed_state = {0};
    if (game_rules_engine_get_state_data(engine, &typed_state) != GAME_RULES_CALL_OK ||
        typed_state.has_state != 0) {
        game_rules_state_result_dispose(&typed_state);
        game_rules_engine_destroy(engine);
        return 1;
    }
    game_rules_state_result_dispose(&typed_state);

    const game_rules_cell cells[] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, GAME_RULES_DIRECTION_NORTH},
        {{1, 0}, GAME_RULES_CELL_FLAT, 0, GAME_RULES_DIRECTION_NORTH},
    };
    const game_rules_entity entities[] = {
        {1, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
    };
    const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        2,
        1,
        cells,
        2,
        NULL,
        0,
        entities,
        1,
    };
    game_rules_load_result loaded = {0};
    if (game_rules_engine_load_level_data(engine, &level, &loaded) != GAME_RULES_CALL_OK ||
        loaded.accepted == 0 || loaded.has_state == 0 ||
        loaded.state.resolved.entity_count != 1 ||
        loaded.state.resolved.entities[0].id != 1) {
        game_rules_load_result_dispose(&loaded);
        game_rules_engine_destroy(engine);
        return 1;
    }
    game_rules_load_result_dispose(&loaded);

    game_rules_move_result moved = {0};
    if (game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &moved) !=
            GAME_RULES_CALL_OK ||
        moved.accepted == 0 || moved.tick_count != 1 || moved.ticks[0].event_count != 1 ||
        moved.ticks[0].events[0].kind != GAME_RULES_EVENT_ENTITY_MOVED) {
        game_rules_move_result_dispose(&moved);
        game_rules_engine_destroy(engine);
        return 1;
    }
    game_rules_move_result_dispose(&moved);

    char* state = game_rules_engine_get_state(engine);
    if (state == NULL) {
        game_rules_engine_destroy(engine);
        return 1;
    }

    game_rules_string_free(state);
    game_rules_engine_destroy(engine);
    return 0;
}
