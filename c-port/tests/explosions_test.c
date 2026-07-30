#include "game_rules/c_api.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static const game_rules_entity* find_entity(const game_rules_resolved_state* state,
                                             uint64_t id)
{
    uint32_t index;
    for (index = 0U; index < state->entity_count; ++index) {
        if (state->entities[index].id == id) return &state->entities[index];
    }
    return NULL;
}

static void fill_flat_grid(game_rules_cell* cells,
                           uint32_t width,
                           uint32_t height,
                           int32_t elevation)
{
    uint32_t y;
    uint32_t x;
    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            game_rules_cell* cell = &cells[y * width + x];
            cell->coordinate.x = (int32_t)x;
            cell->coordinate.y = (int32_t)y;
            cell->kind = GAME_RULES_CELL_FLAT;
            cell->elevation = elevation;
            cell->low_direction = 0U;
        }
    }
}

static void expect_event(const game_rules_event* event,
                         uint32_t kind,
                         uint64_t entity_id)
{
    assert(event->kind == kind);
    if (kind == GAME_RULES_EVENT_ENTITY_MOVED ||
        kind == GAME_RULES_EVENT_BARREL_ARMED ||
        kind == GAME_RULES_EVENT_BARREL_EXPLODED) {
        assert(event->entity_id == entity_id);
    }
}

static void test_six_tick_blast_fall_chain(void)
{
    game_rules_cell cells[7];
    const int32_t elevations[7] = {2, 2, 0, 0, -1, -1, -2};
    const game_rules_entity entities[4] = {
        {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 4},
        {8U, GAME_RULES_ENTITY_BARREL, {1, 0}, 4},
        {9U, GAME_RULES_ENTITY_BARREL, {3, 0}, 0},
        {2U, GAME_RULES_ENTITY_BOX, {5, 0}, -2}};
    game_rules_level_definition level;
    game_rules_load_result load = {0};
    game_rules_move_result move = {0};
    game_rules_engine* engine;
    uint32_t index;

    fill_flat_grid(cells, 7U, 1U, 0);
    for (index = 0U; index < 7U; ++index) cells[index].elevation = elevations[index];
    memset(&level, 0, sizeof(level));
    level.coordinates.positive_x = GAME_RULES_HORIZONTAL_EAST;
    level.coordinates.positive_y = GAME_RULES_VERTICAL_NORTH;
    level.width = 7U;
    level.height = 1U;
    level.cells = cells;
    level.cell_count = 7U;
    level.entities = entities;
    level.entity_count = 4U;

    engine = game_rules_engine_create();
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
    assert(load.accepted && load.tick_count == 0U);
    game_rules_load_result_dispose(&load);

    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &move) ==
           GAME_RULES_CALL_OK);
    assert(move.accepted && move.tick_count == 6U);
    assert(move.initial_state.entity_count == 4U);

    assert(move.ticks[0].event_count == 2U);
    expect_event(&move.ticks[0].events[0], GAME_RULES_EVENT_ENTITY_MOVED, 1U);
    expect_event(&move.ticks[0].events[1], GAME_RULES_EVENT_ENTITY_MOVED, 8U);
    assert(move.ticks[0].events[0].movement_cause == GAME_RULES_MOVEMENT_PLAYER);
    assert(move.ticks[0].events[1].movement_cause == GAME_RULES_MOVEMENT_PLAYER);

    assert(move.ticks[1].event_count == 2U);
    expect_event(&move.ticks[1].events[0], GAME_RULES_EVENT_ENTITY_MOVED, 8U);
    assert(move.ticks[1].events[0].old_bottom_half_steps == 4);
    assert(move.ticks[1].events[0].new_bottom_half_steps == 0);
    assert(move.ticks[1].events[0].movement_cause == GAME_RULES_MOVEMENT_FALL);
    expect_event(&move.ticks[1].events[1], GAME_RULES_EVENT_BARREL_ARMED, 8U);
    assert(move.ticks[1].state_after.armed_barrel_count == 1U);
    assert(move.ticks[1].state_after.armed_barrel_ids[0] == 8U);

    assert(move.ticks[2].event_count == 3U);
    expect_event(&move.ticks[2].events[0], GAME_RULES_EVENT_BARREL_EXPLODED, 8U);
    assert(move.ticks[2].events[0].coordinate.x == 2);
    assert(move.ticks[2].events[0].bottom_half_steps == 0);
    expect_event(&move.ticks[2].events[1], GAME_RULES_EVENT_ENTITY_MOVED, 9U);
    assert(move.ticks[2].events[1].from.x == 3 && move.ticks[2].events[1].to.x == 4);
    assert(move.ticks[2].events[1].movement_cause == GAME_RULES_MOVEMENT_BLAST);
    expect_event(&move.ticks[2].events[2], GAME_RULES_EVENT_BARREL_ARMED, 9U);
    assert(move.ticks[2].state_after.armed_barrel_count == 1U);
    assert(move.ticks[2].state_after.armed_barrel_ids[0] == 9U);
    assert(find_entity(&move.ticks[2].state_after, 8U) == NULL);

    assert(move.ticks[3].event_count == 1U);
    expect_event(&move.ticks[3].events[0], GAME_RULES_EVENT_ENTITY_MOVED, 9U);
    assert(move.ticks[3].events[0].movement_cause == GAME_RULES_MOVEMENT_FALL);
    assert(move.ticks[3].events[0].new_bottom_half_steps == -2);
    assert(move.ticks[3].state_after.armed_barrel_count == 1U);

    assert(move.ticks[4].event_count == 2U);
    expect_event(&move.ticks[4].events[0], GAME_RULES_EVENT_BARREL_EXPLODED, 9U);
    assert(move.ticks[4].events[0].coordinate.x == 4);
    assert(move.ticks[4].events[0].bottom_half_steps == -2);
    expect_event(&move.ticks[4].events[1], GAME_RULES_EVENT_ENTITY_MOVED, 2U);
    assert(move.ticks[4].events[1].movement_cause == GAME_RULES_MOVEMENT_BLAST);
    assert(move.ticks[4].events[1].from.x == 5 && move.ticks[4].events[1].to.x == 6);

    assert(move.ticks[5].event_count == 1U);
    expect_event(&move.ticks[5].events[0], GAME_RULES_EVENT_ENTITY_MOVED, 2U);
    assert(move.ticks[5].events[0].movement_cause == GAME_RULES_MOVEMENT_FALL);
    assert(move.ticks[5].events[0].new_bottom_half_steps == -4);
    assert(move.final_state.entity_count == 2U);
    assert(move.final_state.armed_barrel_count == 0U);
    assert(find_entity(&move.final_state, 8U) == NULL);
    assert(find_entity(&move.final_state, 9U) == NULL);
    assert(find_entity(&move.final_state, 2U)->coordinate.x == 6);
    assert(find_entity(&move.final_state, 2U)->bottom_half_steps == -4);
    assert(move.final_state.outcome == GAME_RULES_OUTCOME_ONGOING);

    game_rules_move_result_dispose(&move);
    game_rules_engine_destroy(engine);
}

static void load_conflict_level(const game_rules_entity* entities,
                                game_rules_load_result* load)
{
    game_rules_cell cells[16];
    game_rules_level_definition level;
    game_rules_engine* engine = game_rules_engine_create();
    assert(engine != NULL);
    fill_flat_grid(cells, 8U, 2U, 0);
    memset(&level, 0, sizeof(level));
    level.coordinates.positive_x = GAME_RULES_HORIZONTAL_EAST;
    level.coordinates.positive_y = GAME_RULES_VERTICAL_NORTH;
    level.width = 8U;
    level.height = 2U;
    level.cells = cells;
    level.cell_count = 16U;
    level.entities = entities;
    level.entity_count = 7U;
    assert(game_rules_engine_load_level_data(engine, &level, load) == GAME_RULES_CALL_OK);
    assert(load->accepted);
    game_rules_engine_destroy(engine);
}

static void assert_conflict_result(const game_rules_load_result* load,
                                   uint64_t left_source,
                                   uint64_t middle_source,
                                   uint64_t left_box,
                                   uint64_t right_box)
{
    assert(load->tick_count == 2U);
    assert(load->ticks[1].event_count == 4U);
    expect_event(&load->ticks[1].events[0], GAME_RULES_EVENT_BARREL_EXPLODED,
                 left_source);
    expect_event(&load->ticks[1].events[1], GAME_RULES_EVENT_BARREL_EXPLODED,
                 middle_source);
    expect_event(&load->ticks[1].events[2], GAME_RULES_EVENT_BARREL_EXPLODED, 7U);
    expect_event(&load->ticks[1].events[3], GAME_RULES_EVENT_ENTITY_MOVED, 6U);
    assert(load->ticks[1].events[3].movement_cause == GAME_RULES_MOVEMENT_BLAST);
    assert(load->ticks[1].events[3].from.x == 6);
    assert(load->ticks[1].events[3].to.x == 5);
    assert(find_entity(&load->final_state, left_box)->coordinate.x == 1);
    assert(find_entity(&load->final_state, right_box)->coordinate.x == 3);
    assert(find_entity(&load->final_state, 6U)->coordinate.x == 5);
}

static void test_simultaneous_conflicts_ignore_order_and_ids(void)
{
    const game_rules_entity canonical[7] = {
        {8U, GAME_RULES_ENTITY_BARREL, {0, 0}, 2},
        {2U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
        {3U, GAME_RULES_ENTITY_BOX, {3, 0}, 0},
        {9U, GAME_RULES_ENTITY_BARREL, {4, 0}, 2},
        {6U, GAME_RULES_ENTITY_BOX, {6, 0}, 0},
        {7U, GAME_RULES_ENTITY_BARREL, {7, 0}, 2},
        {1U, GAME_RULES_ENTITY_PLAYER, {2, 1}, 0}};
    const game_rules_entity reversed[7] = {
        {1U, GAME_RULES_ENTITY_PLAYER, {2, 1}, 0},
        {7U, GAME_RULES_ENTITY_BARREL, {7, 0}, 2},
        {6U, GAME_RULES_ENTITY_BOX, {6, 0}, 0},
        {9U, GAME_RULES_ENTITY_BARREL, {4, 0}, 2},
        {3U, GAME_RULES_ENTITY_BOX, {3, 0}, 0},
        {2U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
        {8U, GAME_RULES_ENTITY_BARREL, {0, 0}, 2}};
    const game_rules_entity reassigned[7] = {
        {9U, GAME_RULES_ENTITY_BARREL, {0, 0}, 2},
        {3U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
        {2U, GAME_RULES_ENTITY_BOX, {3, 0}, 0},
        {8U, GAME_RULES_ENTITY_BARREL, {4, 0}, 2},
        {6U, GAME_RULES_ENTITY_BOX, {6, 0}, 0},
        {7U, GAME_RULES_ENTITY_BARREL, {7, 0}, 2},
        {1U, GAME_RULES_ENTITY_PLAYER, {2, 1}, 0}};
    game_rules_load_result first = {0};
    game_rules_load_result second = {0};
    game_rules_load_result third = {0};

    load_conflict_level(canonical, &first);
    load_conflict_level(reversed, &second);
    load_conflict_level(reassigned, &third);
    assert_conflict_result(&first, 8U, 9U, 2U, 3U);
    assert_conflict_result(&second, 8U, 9U, 2U, 3U);
    assert_conflict_result(&third, 9U, 8U, 3U, 2U);
    assert(second.ticks[1].events[0].coordinate.x == 0);
    assert(third.ticks[1].events[0].coordinate.x == 0);
    assert(first.final_state.entity_count == second.final_state.entity_count);
    assert(first.final_state.armed_barrel_count == 0U);
    assert(second.final_state.armed_barrel_count == 0U);
    assert(third.final_state.armed_barrel_count == 0U);

    game_rules_load_result_dispose(&first);
    game_rules_load_result_dispose(&second);
    game_rules_load_result_dispose(&third);
}

static void test_same_cell_wave_uses_bottom_to_top_source_order(void)
{
    game_rules_cell cells[2];
    const game_rules_entity entities[3] = {
        {4U, GAME_RULES_ENTITY_BARREL, {0, 0}, 4},
        {1U, GAME_RULES_ENTITY_PLAYER, {1, 0}, 0},
        {8U, GAME_RULES_ENTITY_BARREL, {0, 0}, 2}};
    game_rules_level_definition level;
    game_rules_load_result load = {0};
    game_rules_engine* engine = game_rules_engine_create();
    assert(engine != NULL);
    fill_flat_grid(cells, 2U, 1U, 0);
    memset(&level, 0, sizeof(level));
    level.coordinates.positive_x = GAME_RULES_HORIZONTAL_EAST;
    level.coordinates.positive_y = GAME_RULES_VERTICAL_NORTH;
    level.width = 2U;
    level.height = 1U;
    level.cells = cells;
    level.cell_count = 2U;
    level.entities = entities;
    level.entity_count = 3U;

    assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
    assert(load.accepted && load.tick_count == 2U);
    assert(load.ticks[0].event_count == 4U);
    expect_event(&load.ticks[0].events[0], GAME_RULES_EVENT_ENTITY_MOVED, 8U);
    expect_event(&load.ticks[0].events[1], GAME_RULES_EVENT_BARREL_ARMED, 8U);
    expect_event(&load.ticks[0].events[2], GAME_RULES_EVENT_ENTITY_MOVED, 4U);
    expect_event(&load.ticks[0].events[3], GAME_RULES_EVENT_BARREL_ARMED, 4U);
    assert(load.ticks[1].event_count == 3U);
    expect_event(&load.ticks[1].events[0], GAME_RULES_EVENT_BARREL_EXPLODED, 8U);
    expect_event(&load.ticks[1].events[1], GAME_RULES_EVENT_BARREL_EXPLODED, 4U);
    assert(load.ticks[1].events[2].kind == GAME_RULES_EVENT_LEVEL_LOST);
    assert(load.final_state.outcome == GAME_RULES_OUTCOME_LOST);
    assert(load.final_state.entity_count == 1U);

    game_rules_load_result_dispose(&load);
    game_rules_engine_destroy(engine);
}

static void test_vertical_chain_fixture_and_loss_order(void)
{
    game_rules_cell cells[4];
    const game_rules_fixture fixtures[2] = {
        {{0, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED},
        {{3, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_RED}};
    const game_rules_entity entities[3] = {
        {9U, GAME_RULES_ENTITY_BARREL, {0, 0}, 0},
        {8U, GAME_RULES_ENTITY_BARREL, {0, 0}, 4},
        {1U, GAME_RULES_ENTITY_PLAYER, {1, 0}, 0}};
    game_rules_level_definition level;
    game_rules_load_result load = {0};
    game_rules_engine* engine = game_rules_engine_create();
    assert(engine != NULL);
    fill_flat_grid(cells, 4U, 1U, 0);
    memset(&level, 0, sizeof(level));
    level.coordinates.positive_x = GAME_RULES_HORIZONTAL_EAST;
    level.coordinates.positive_y = GAME_RULES_VERTICAL_NORTH;
    level.width = 4U;
    level.height = 1U;
    level.cells = cells;
    level.cell_count = 4U;
    level.fixtures = fixtures;
    level.fixture_count = 2U;
    level.entities = entities;
    level.entity_count = 3U;

    assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
    assert(load.accepted && load.tick_count == 4U);
    assert(load.ticks[0].event_count == 2U);
    assert(load.ticks[0].events[0].kind == GAME_RULES_EVENT_SWITCH_CHANGED);
    assert(load.ticks[0].events[1].kind == GAME_RULES_EVENT_DOOR_OPENED);
    assert(load.ticks[1].event_count == 2U);
    expect_event(&load.ticks[1].events[0], GAME_RULES_EVENT_ENTITY_MOVED, 8U);
    expect_event(&load.ticks[1].events[1], GAME_RULES_EVENT_BARREL_ARMED, 8U);
    assert(load.ticks[2].event_count == 2U);
    expect_event(&load.ticks[2].events[0], GAME_RULES_EVENT_BARREL_EXPLODED, 8U);
    expect_event(&load.ticks[2].events[1], GAME_RULES_EVENT_BARREL_ARMED, 9U);
    assert(load.ticks[3].event_count == 4U);
    expect_event(&load.ticks[3].events[0], GAME_RULES_EVENT_BARREL_EXPLODED, 9U);
    assert(load.ticks[3].events[1].kind == GAME_RULES_EVENT_SWITCH_CHANGED);
    assert(load.ticks[3].events[2].kind == GAME_RULES_EVENT_DOOR_CLOSED);
    assert(load.ticks[3].events[3].kind == GAME_RULES_EVENT_LEVEL_LOST);
    assert(load.final_state.outcome == GAME_RULES_OUTCOME_LOST);
    assert(load.final_state.armed_barrel_count == 0U);

    game_rules_load_result_dispose(&load);
    game_rules_engine_destroy(engine);
}

static void test_blast_fall_slide_before_secondary_detonation(void)
{
    game_rules_cell cells[10];
    const game_rules_entity entities[3] = {
        {1U, GAME_RULES_ENTITY_PLAYER, {0, 1}, 0},
        {8U, GAME_RULES_ENTITY_BARREL, {1, 0}, 4},
        {9U, GAME_RULES_ENTITY_BARREL, {2, 0}, 2}};
    game_rules_level_definition level;
    game_rules_load_result load = {0};
    game_rules_engine* engine = game_rules_engine_create();
    assert(engine != NULL);
    fill_flat_grid(cells, 5U, 2U, 0);
    cells[0].elevation = 1;
    cells[1].elevation = 1;
    cells[2].elevation = 1;
    cells[3].kind = GAME_RULES_CELL_RAMP;
    cells[3].elevation = 0;
    cells[3].low_direction = GAME_RULES_DIRECTION_EAST;
    memset(&level, 0, sizeof(level));
    level.coordinates.positive_x = GAME_RULES_HORIZONTAL_EAST;
    level.coordinates.positive_y = GAME_RULES_VERTICAL_NORTH;
    level.width = 5U;
    level.height = 2U;
    level.cells = cells;
    level.cell_count = 10U;
    level.entities = entities;
    level.entity_count = 3U;

    assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
    assert(load.accepted && load.tick_count == 5U);
    expect_event(&load.ticks[1].events[0], GAME_RULES_EVENT_BARREL_EXPLODED, 8U);
    expect_event(&load.ticks[1].events[1], GAME_RULES_EVENT_ENTITY_MOVED, 9U);
    assert(load.ticks[1].events[1].movement_cause == GAME_RULES_MOVEMENT_BLAST);
    expect_event(&load.ticks[1].events[2], GAME_RULES_EVENT_BARREL_ARMED, 9U);
    expect_event(&load.ticks[2].events[0], GAME_RULES_EVENT_ENTITY_MOVED, 9U);
    assert(load.ticks[2].events[0].movement_cause == GAME_RULES_MOVEMENT_FALL);
    expect_event(&load.ticks[3].events[0], GAME_RULES_EVENT_ENTITY_MOVED, 9U);
    assert(load.ticks[3].events[0].movement_cause == GAME_RULES_MOVEMENT_SLIDE);
    expect_event(&load.ticks[4].events[0], GAME_RULES_EVENT_BARREL_EXPLODED, 9U);
    assert(load.ticks[4].events[0].coordinate.x == 4);
    assert(load.ticks[4].events[0].bottom_half_steps == 0);
    assert(load.final_state.outcome == GAME_RULES_OUTCOME_ONGOING);
    assert(load.final_state.armed_barrel_count == 0U);

    game_rules_load_result_dispose(&load);
    game_rules_engine_destroy(engine);
}

int main(void)
{
    test_six_tick_blast_fall_chain();
    test_simultaneous_conflicts_ignore_order_and_ids();
    test_same_cell_wave_uses_bottom_to_top_source_order();
    test_vertical_chain_fixture_and_loss_order();
    test_blast_fall_slide_before_secondary_detonation();
    return 0;
}
