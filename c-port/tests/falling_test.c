#include "game_rules/c_api.h"

#include "c_api_internal.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static game_rules_level_definition line_level(game_rules_cell* cells,
                                              uint32_t width,
                                              const int32_t* elevations,
                                              const game_rules_entity* entities,
                                              uint32_t entity_count)
{
    game_rules_level_definition level;
    uint32_t index;
    memset(&level, 0, sizeof(level));
    for (index = 0U; index < width; ++index) {
        cells[index].coordinate.x = (int32_t)index;
        cells[index].coordinate.y = 0;
        cells[index].kind = GAME_RULES_CELL_FLAT;
        cells[index].elevation = elevations[index];
        cells[index].low_direction = GAME_RULES_DIRECTION_NORTH;
    }
    level.coordinates.positive_x = GAME_RULES_HORIZONTAL_EAST;
    level.coordinates.positive_y = GAME_RULES_VERTICAL_NORTH;
    level.width = width;
    level.height = 1U;
    level.cells = cells;
    level.cell_count = width;
    level.entities = entities;
    level.entity_count = entity_count;
    return level;
}

static const game_rules_entity* find_entity(const game_rules_resolved_state* state,
                                            uint64_t id)
{
    uint32_t index;
    for (index = 0U; index < state->entity_count; ++index) {
        if (state->entities[index].id == id) return &state->entities[index];
    }
    return NULL;
}

static void expect_move(const game_rules_event* event,
                        uint64_t id,
                        game_rules_coordinate from,
                        game_rules_coordinate to,
                        int32_t old_bottom,
                        int32_t new_bottom,
                        uint32_t cause)
{
    assert(event->kind == GAME_RULES_EVENT_ENTITY_MOVED);
    assert(event->entity_id == id);
    assert(event->from.x == from.x && event->from.y == from.y);
    assert(event->to.x == to.x && event->to.y == to.y);
    assert(event->old_bottom_half_steps == old_bottom);
    assert(event->new_bottom_half_steps == new_bottom);
    assert(event->movement_cause == cause);
}

static void load_ok(game_rules_engine* engine,
                    const game_rules_level_definition* level,
                    game_rules_load_result* result)
{
    assert(game_rules_engine_load_level_data(engine, level, result) ==
           GAME_RULES_CALL_OK);
    assert(result->status == GAME_RULES_LOAD_LOADED && result->accepted == 1U);
}

static void test_initial_columns_ramp_and_crushing(void)
{
    {
        const int32_t elevations[3] = {0, 0, 0};
        game_rules_cell cells[3];
        const game_rules_entity entities[4] = {
            {31U, GAME_RULES_ENTITY_BOX, {0, 0}, 8},
            {90U, GAME_RULES_ENTITY_PLAYER, {2, 0}, 0},
            {55U, GAME_RULES_ENTITY_BOX, {1, 0}, 6},
            {7U, GAME_RULES_ENTITY_BOX, {0, 0}, 4}};
        const game_rules_level_definition level = line_level(
            cells, 3U, elevations, entities, 4U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result load = {0};
        assert(engine != NULL);
        load_ok(engine, &level, &load);
        assert(load.tick_count == 1U && load.ticks[0].index == 0U);
        assert(load.ticks[0].event_count == 3U);
        expect_move(&load.ticks[0].events[0], 7U, (game_rules_coordinate){0, 0},
                    (game_rules_coordinate){0, 0}, 4, 0,
                    GAME_RULES_MOVEMENT_FALL);
        expect_move(&load.ticks[0].events[1], 31U, (game_rules_coordinate){0, 0},
                    (game_rules_coordinate){0, 0}, 8, 2,
                    GAME_RULES_MOVEMENT_FALL);
        expect_move(&load.ticks[0].events[2], 55U, (game_rules_coordinate){1, 0},
                    (game_rules_coordinate){1, 0}, 6, 0,
                    GAME_RULES_MOVEMENT_FALL);
        assert(find_entity(&load.final_state, 7U)->bottom_half_steps == 0);
        assert(find_entity(&load.final_state, 31U)->bottom_half_steps == 2);
        assert(find_entity(&load.final_state, 55U)->bottom_half_steps == 0);
        game_rules_load_result_dispose(&load);
        game_rules_engine_destroy(engine);
    }
    {
        game_rules_cell cells[3] = {
            {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
            {{1, 0}, GAME_RULES_CELL_RAMP, 0, GAME_RULES_DIRECTION_WEST},
            {{2, 0}, GAME_RULES_CELL_FLAT, 1, 0}};
        const game_rules_entity entities[2] = {
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
            {2U, GAME_RULES_ENTITY_BOX, {1, 0}, 5}};
        const game_rules_level_definition level = {
            {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
            3U, 1U, cells, 3U, NULL, 0U, entities, 2U};
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result load = {0};
        assert(engine != NULL);
        load_ok(engine, &level, &load);
        assert(load.tick_count == 1U && load.ticks[0].event_count == 1U);
        expect_move(&load.ticks[0].events[0], 2U, (game_rules_coordinate){1, 0},
                    (game_rules_coordinate){1, 0}, 5, 1,
                    GAME_RULES_MOVEMENT_FALL);
        game_rules_load_result_dispose(&load);
        game_rules_engine_destroy(engine);
    }
    {
        const int32_t elevations[2] = {0, 0};
        game_rules_cell cells[2];
        const game_rules_entity entities[3] = {
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
            {2U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
            {3U, GAME_RULES_ENTITY_BOX, {1, 0}, 2}};
        const game_rules_level_definition level = line_level(
            cells, 2U, elevations, entities, 3U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result load = {0};
        uint32_t event_count = 0U;
        assert(engine != NULL);
        load_ok(engine, &level, &load);
        assert(load.tick_count == 0U);
        engine->session->current_state.entities[1].coordinate.x = 0;
        engine->session->current_state.entities[1].bottom_half_steps = 4;
        assert(game_rules_c_resolve_falling_tick(engine->session, &event_count) == 1);
        assert(event_count == 3U);
        assert(engine->session->scratch_events[0].kind ==
               GAME_RULES_EVENT_PLAYER_CRUSHED);
        assert(engine->session->scratch_events[0].entity_id == 1U);
        assert(engine->session->scratch_events[0].other_entity_id == 2U);
        expect_move(&engine->session->scratch_events[1], 3U,
                    (game_rules_coordinate){1, 0},
                    (game_rules_coordinate){1, 0}, 2, 0,
                    GAME_RULES_MOVEMENT_FALL);
        assert(engine->session->scratch_events[2].kind == GAME_RULES_EVENT_LEVEL_LOST);
        assert(engine->session->scratch_state.outcome == GAME_RULES_OUTCOME_LOST);
        assert(engine->session->scratch_state.entities[1].bottom_half_steps == 4);
        game_rules_load_result_dispose(&load);
        game_rules_engine_destroy(engine);
    }
    {
        const int32_t elevations[1] = {0};
        game_rules_cell cells[1];
        game_rules_entity player = {
            1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 1};
        game_rules_level_definition level = line_level(
            cells, 1U, elevations, &player, 1U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result load = {0};
        game_rules_move_result move = {0};
        assert(engine != NULL);
        load_ok(engine, &level, &load);
        assert(load.tick_count == 1U);
        assert(load.final_state.outcome == GAME_RULES_OUTCOME_ONGOING);
        assert(find_entity(&load.final_state, 1U)->bottom_half_steps == 0);
        game_rules_load_result_dispose(&load);

        player.bottom_half_steps = 2;
        load_ok(engine, &level, &load);
        assert(load.tick_count == 1U && load.ticks[0].event_count == 2U);
        expect_move(&load.ticks[0].events[0], 1U, (game_rules_coordinate){0, 0},
                    (game_rules_coordinate){0, 0}, 2, 0,
                    GAME_RULES_MOVEMENT_FALL);
        assert(load.ticks[0].events[1].kind == GAME_RULES_EVENT_LEVEL_LOST);
        assert(load.final_state.outcome == GAME_RULES_OUTCOME_LOST);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &move) == GAME_RULES_CALL_OK);
        assert(move.status == GAME_RULES_MOVE_LEVEL_TERMINAL && !move.accepted);
        game_rules_move_result_dispose(&move);
        game_rules_engine_destroy(engine);
    }
}

static void test_box_falls_and_stacking(void)
{
    {
        const int32_t elevations[3] = {3, 3, 1};
        game_rules_cell cells[3];
        const game_rules_entity entities[3] = {
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 6},
            {2U, GAME_RULES_ENTITY_BOX, {1, 0}, 6},
            {3U, GAME_RULES_ENTITY_BOX, {2, 0}, 2}};
        const game_rules_level_definition level = line_level(
            cells, 3U, elevations, entities, 3U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result load = {0};
        game_rules_move_result move = {0};
        assert(engine != NULL);
        load_ok(engine, &level, &load);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &move) == GAME_RULES_CALL_OK);
        assert(move.accepted && move.tick_count == 2U);
        assert(move.ticks[0].event_count == 2U);
        expect_move(&move.ticks[0].events[0], 1U, (game_rules_coordinate){0, 0},
                    (game_rules_coordinate){1, 0}, 6, 6,
                    GAME_RULES_MOVEMENT_PLAYER);
        expect_move(&move.ticks[0].events[1], 2U, (game_rules_coordinate){1, 0},
                    (game_rules_coordinate){2, 0}, 6, 6,
                    GAME_RULES_MOVEMENT_PLAYER);
        expect_move(&move.ticks[1].events[0], 2U, (game_rules_coordinate){2, 0},
                    (game_rules_coordinate){2, 0}, 6, 4,
                    GAME_RULES_MOVEMENT_FALL);
        assert(find_entity(&move.ticks[0].state_after, 2U)->bottom_half_steps == 6);
        assert(find_entity(&move.ticks[1].state_after, 2U)->bottom_half_steps == 4);
        assert(find_entity(&move.final_state, 2U)->bottom_half_steps == 4);
        game_rules_move_result_dispose(&move);
        game_rules_engine_destroy(engine);
    }
    {
        const int32_t elevations[3] = {10, 10, 0};
        game_rules_cell cells[3];
        const game_rules_entity entities[2] = {
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 20},
            {2U, GAME_RULES_ENTITY_BOX, {1, 0}, 20}};
        const game_rules_level_definition level = line_level(
            cells, 3U, elevations, entities, 2U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result load = {0};
        game_rules_move_result move = {0};
        assert(engine != NULL);
        load_ok(engine, &level, &load);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &move) == GAME_RULES_CALL_OK);
        assert(move.tick_count == 2U);
        expect_move(&move.ticks[1].events[0], 2U, (game_rules_coordinate){2, 0},
                    (game_rules_coordinate){2, 0}, 20, 0,
                    GAME_RULES_MOVEMENT_FALL);
        game_rules_move_result_dispose(&move);
        game_rules_engine_destroy(engine);
    }
    {
        const int32_t elevations[3] = {1, 0, 0};
        game_rules_cell cells[3];
        const game_rules_entity entities[3] = {
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 2},
            {2U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
            {3U, GAME_RULES_ENTITY_BOX, {1, 0}, 2}};
        const game_rules_level_definition level = line_level(
            cells, 3U, elevations, entities, 3U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result load = {0};
        game_rules_move_result move = {0};
        assert(engine != NULL);
        load_ok(engine, &level, &load);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &move) == GAME_RULES_CALL_OK);
        assert(move.tick_count == 2U);
        expect_move(&move.ticks[1].events[0], 3U, (game_rules_coordinate){2, 0},
                    (game_rules_coordinate){2, 0}, 2, 0,
                    GAME_RULES_MOVEMENT_FALL);
        assert(find_entity(&move.final_state, 1U)->coordinate.x == 1);
        assert(find_entity(&move.final_state, 2U)->bottom_half_steps == 0);
        game_rules_move_result_dispose(&move);
        game_rules_engine_destroy(engine);
    }
}

static void test_barrel_closure_and_terminal_repetition(void)
{
    {
        const int32_t elevations[3] = {2, 2, 0};
        game_rules_cell cells[3];
        const game_rules_entity entities[2] = {
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 4},
            {8U, GAME_RULES_ENTITY_BARREL, {1, 0}, 4}};
        const game_rules_level_definition level = line_level(
            cells, 3U, elevations, entities, 2U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result load = {0};
        game_rules_move_result move = {0};
        assert(engine != NULL);
        load_ok(engine, &level, &load);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &move) == GAME_RULES_CALL_OK);
        assert(move.accepted && move.tick_count == 3U);
        assert(move.ticks[1].event_count == 2U);
        expect_move(&move.ticks[1].events[0], 8U, (game_rules_coordinate){2, 0},
                    (game_rules_coordinate){2, 0}, 4, 0,
                    GAME_RULES_MOVEMENT_FALL);
        assert(move.ticks[1].events[1].kind == GAME_RULES_EVENT_BARREL_ARMED);
        assert(move.ticks[1].state_after.armed_barrel_count == 1U);
        assert(move.ticks[2].event_count == 1U);
        assert(move.ticks[2].events[0].kind == GAME_RULES_EVENT_BARREL_EXPLODED);
        assert(find_entity(&move.final_state, 8U) == NULL);
        game_rules_move_result_dispose(&move);
        game_rules_engine_destroy(engine);
    }
    {
        const int32_t elevations[3] = {3, 3, 1};
        game_rules_cell cells[3];
        const game_rules_entity entities[3] = {
            {9U, GAME_RULES_ENTITY_BARREL, {2, 0}, 2},
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 6},
            {8U, GAME_RULES_ENTITY_BARREL, {1, 0}, 6}};
        const game_rules_level_definition level = line_level(
            cells, 3U, elevations, entities, 3U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result load = {0};
        game_rules_move_result move = {0};
        assert(engine != NULL);
        load_ok(engine, &level, &load);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &move) == GAME_RULES_CALL_OK);
        assert(move.tick_count == 4U);
        assert(move.ticks[1].events[0].entity_id == 8U);
        assert(move.ticks[1].events[1].kind == GAME_RULES_EVENT_BARREL_ARMED);
        assert(move.ticks[2].events[0].kind == GAME_RULES_EVENT_BARREL_EXPLODED);
        assert(move.ticks[2].events[1].kind == GAME_RULES_EVENT_BARREL_ARMED);
        assert(move.ticks[2].events[1].entity_id == 9U);
        assert(move.ticks[3].events[0].kind == GAME_RULES_EVENT_BARREL_EXPLODED);
        assert(move.final_state.entity_count == 1U);
        game_rules_move_result_dispose(&move);
        game_rules_engine_destroy(engine);
    }
    {
        const int32_t elevations[3] = {1, 0, 0};
        game_rules_cell cells[3];
        const game_rules_entity entities[3] = {
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 2},
            {9U, GAME_RULES_ENTITY_BARREL, {1, 0}, 0},
            {8U, GAME_RULES_ENTITY_BARREL, {1, 0}, 2}};
        const game_rules_level_definition level = line_level(
            cells, 3U, elevations, entities, 3U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result load = {0};
        game_rules_move_result move = {0};
        uint32_t repeat;
        assert(engine != NULL);
        load_ok(engine, &level, &load);
        game_rules_load_result_dispose(&load);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &move) == GAME_RULES_CALL_OK);
        assert(move.accepted && move.tick_count == 4U);
        assert(move.final_state.outcome == GAME_RULES_OUTCOME_LOST);
        assert(move.ticks[3].events[0].kind == GAME_RULES_EVENT_BARREL_EXPLODED);
        assert(move.ticks[3].events[1].kind == GAME_RULES_EVENT_LEVEL_LOST);
        game_rules_move_result_dispose(&move);
        for (repeat = 0U; repeat < 3U; ++repeat) {
            memset(&move, 0, sizeof(move));
            assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST,
                                               &move) == GAME_RULES_CALL_OK);
            assert(move.status == GAME_RULES_MOVE_LEVEL_TERMINAL && !move.accepted);
            assert(move.tick_count == 0U && move.final_state.outcome ==
                   GAME_RULES_OUTCOME_LOST);
            game_rules_move_result_dispose(&move);
        }
        game_rules_engine_destroy(engine);
    }
}

int main(void)
{
    test_initial_columns_ramp_and_crushing();
    test_box_falls_and_stacking();
    test_barrel_closure_and_terminal_repetition();
    return 0;
}
