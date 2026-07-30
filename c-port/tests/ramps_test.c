#include "game_rules/c_api.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static game_rules_coordinate cardinal_step(game_rules_coordinate coordinate,
                                           uint32_t direction,
                                           uint32_t positive_x,
                                           uint32_t positive_y)
{
    switch (direction) {
    case GAME_RULES_DIRECTION_NORTH:
        coordinate.y += positive_y == GAME_RULES_VERTICAL_NORTH ? 1 : -1;
        break;
    case GAME_RULES_DIRECTION_EAST:
        coordinate.x += positive_x == GAME_RULES_HORIZONTAL_EAST ? 1 : -1;
        break;
    case GAME_RULES_DIRECTION_SOUTH:
        coordinate.y += positive_y == GAME_RULES_VERTICAL_SOUTH ? 1 : -1;
        break;
    case GAME_RULES_DIRECTION_WEST:
        coordinate.x += positive_x == GAME_RULES_HORIZONTAL_WEST ? 1 : -1;
        break;
    default: assert(0);
    }
    return coordinate;
}

static uint32_t opposite(uint32_t direction)
{
    switch (direction) {
    case GAME_RULES_DIRECTION_NORTH: return GAME_RULES_DIRECTION_SOUTH;
    case GAME_RULES_DIRECTION_EAST: return GAME_RULES_DIRECTION_WEST;
    case GAME_RULES_DIRECTION_SOUTH: return GAME_RULES_DIRECTION_NORTH;
    case GAME_RULES_DIRECTION_WEST: return GAME_RULES_DIRECTION_EAST;
    default: assert(0); return direction;
    }
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

static void fill_grid(game_rules_cell* cells,
                      uint32_t width,
                      uint32_t height,
                      game_rules_coordinate origin)
{
    uint32_t x;
    uint32_t y;
    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            game_rules_cell* cell = &cells[y * width + x];
            memset(cell, 0, sizeof(*cell));
            cell->coordinate.x = origin.x + (int32_t)x;
            cell->coordinate.y = origin.y + (int32_t)y;
            cell->kind = GAME_RULES_CELL_FLAT;
        }
    }
}

static game_rules_cell* cell_at(game_rules_cell* cells,
                                uint32_t width,
                                game_rules_coordinate origin,
                                game_rules_coordinate coordinate)
{
    uint32_t x = (uint32_t)(coordinate.x - origin.x);
    uint32_t y = (uint32_t)(coordinate.y - origin.y);
    return &cells[y * width + x];
}

static void test_every_low_direction_and_coordinate_orientation(void)
{
    static const uint32_t horizontal[] = {
        GAME_RULES_HORIZONTAL_EAST, GAME_RULES_HORIZONTAL_WEST};
    static const uint32_t vertical[] = {
        GAME_RULES_VERTICAL_NORTH, GAME_RULES_VERTICAL_SOUTH};
    uint32_t horizontal_index;
    uint32_t vertical_index;
    uint32_t low_direction;

    for (horizontal_index = 0U; horizontal_index < 2U; ++horizontal_index) {
        for (vertical_index = 0U; vertical_index < 2U; ++vertical_index) {
            for (low_direction = GAME_RULES_DIRECTION_NORTH;
                 low_direction <= GAME_RULES_DIRECTION_WEST; ++low_direction) {
                const game_rules_coordinate origin = {10, -4};
                const game_rules_coordinate center = {11, -3};
                const game_rules_coordinate low = cardinal_step(
                    center, low_direction, horizontal[horizontal_index],
                    vertical[vertical_index]);
                const uint32_t uphill = opposite(low_direction);
                const game_rules_coordinate high = cardinal_step(
                    center, uphill, horizontal[horizontal_index],
                    vertical[vertical_index]);
                game_rules_cell cells[9];
                game_rules_entity player = {
                    1U, GAME_RULES_ENTITY_PLAYER, low, 0};
                game_rules_level_definition level;
                game_rules_engine* engine;
                game_rules_load_result loaded = {0};
                game_rules_move_result entered = {0};
                game_rules_move_result exited = {0};

                fill_grid(cells, 3U, 3U, origin);
                cell_at(cells, 3U, origin, center)->kind = GAME_RULES_CELL_RAMP;
                cell_at(cells, 3U, origin, center)->low_direction = low_direction;
                cell_at(cells, 3U, origin, high)->elevation = 1;
                memset(&level, 0, sizeof(level));
                level.coordinates.origin = origin;
                level.coordinates.positive_x = horizontal[horizontal_index];
                level.coordinates.positive_y = vertical[vertical_index];
                level.width = 3U;
                level.height = 3U;
                level.cells = cells;
                level.cell_count = 9U;
                level.entities = &player;
                level.entity_count = 1U;

                engine = game_rules_engine_create();
                assert(engine != NULL);
                assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
                       GAME_RULES_CALL_OK);
                assert(loaded.accepted && loaded.tick_count == 0U);
                assert(game_rules_engine_move_data(engine, uphill, &entered) ==
                       GAME_RULES_CALL_OK);
                assert(entered.accepted && entered.tick_count == 1U);
                assert(entered.ticks[0].index == 0U &&
                       entered.ticks[0].event_count == 1U);
                expect_move(&entered.ticks[0].events[0], 1U, low, center, 0, 1,
                            GAME_RULES_MOVEMENT_PLAYER);
                assert(find_entity(&entered.ticks[0].state_after, 1U)->
                           bottom_half_steps == 1);
                assert(game_rules_engine_move_data(engine, uphill, &exited) ==
                       GAME_RULES_CALL_OK);
                assert(exited.accepted && exited.tick_count == 1U);
                expect_move(&exited.ticks[0].events[0], 1U, center, high, 1, 2,
                            GAME_RULES_MOVEMENT_PLAYER);
                assert(find_entity(&exited.final_state, 1U)->bottom_half_steps == 2);

                game_rules_move_result_dispose(&exited);
                game_rules_move_result_dispose(&entered);
                game_rules_load_result_dispose(&loaded);
                game_rules_engine_destroy(engine);
            }
        }
    }
}

static void test_whole_stack_retries_after_endpoint_clears(void)
{
    game_rules_cell cells[6] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{0, 1}, GAME_RULES_CELL_FLAT, 0, 0},
        {{1, 1}, GAME_RULES_CELL_RAMP, 0, GAME_RULES_DIRECTION_WEST},
        {{2, 1}, GAME_RULES_CELL_FLAT, 1, 0}};
    const game_rules_entity entities[3] = {
        {1U, GAME_RULES_ENTITY_PLAYER, {0, 1}, 0},
        {8U, GAME_RULES_ENTITY_BOX, {1, 1}, 1},
        {9U, GAME_RULES_ENTITY_BARREL, {1, 1}, 3}};
    const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        3U, 2U, cells, 6U, NULL, 0U, entities, 3U};
    game_rules_engine* engine = game_rules_engine_create();
    game_rules_load_result loaded = {0};
    game_rules_move_result moved = {0};
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
           GAME_RULES_CALL_OK);
    assert(loaded.accepted && loaded.tick_count == 0U);

    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_SOUTH, &moved) ==
           GAME_RULES_CALL_OK);
    assert(moved.accepted && moved.tick_count == 2U);
    assert(moved.ticks[0].event_count == 1U);
    expect_move(&moved.ticks[0].events[0], 1U,
                (game_rules_coordinate){0, 1}, (game_rules_coordinate){0, 0},
                0, 0, GAME_RULES_MOVEMENT_PLAYER);
    assert(moved.ticks[1].index == 1U && moved.ticks[1].event_count == 2U);
    expect_move(&moved.ticks[1].events[0], 8U,
                (game_rules_coordinate){1, 1}, (game_rules_coordinate){0, 1},
                1, 0, GAME_RULES_MOVEMENT_SLIDE);
    expect_move(&moved.ticks[1].events[1], 9U,
                (game_rules_coordinate){1, 1}, (game_rules_coordinate){0, 1},
                3, 2, GAME_RULES_MOVEMENT_SLIDE);
    assert(find_entity(&moved.final_state, 8U)->coordinate.x == 0);
    assert(find_entity(&moved.final_state, 9U)->bottom_half_steps == 2);
    assert(moved.final_state.armed_barrel_count == 0U);

    game_rules_move_result_dispose(&moved);
    game_rules_load_result_dispose(&loaded);
    game_rules_engine_destroy(engine);
}

static void test_pushes_onto_connected_ramps_and_slides_to_the_bottom(void)
{
    game_rules_cell cells[5] = {
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
    game_rules_engine* engine = game_rules_engine_create();
    game_rules_load_result loaded = {0};
    game_rules_move_result moved = {0};
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
           GAME_RULES_CALL_OK);
    assert(loaded.accepted && loaded.tick_count == 0U);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST, &moved) ==
           GAME_RULES_CALL_OK);
    assert(moved.accepted && moved.tick_count == 3U);
    assert(moved.ticks[0].event_count == 2U);
    expect_move(&moved.ticks[0].events[0], 1U,
                (game_rules_coordinate){4, 0}, (game_rules_coordinate){3, 0},
                4, 4, GAME_RULES_MOVEMENT_PLAYER);
    expect_move(&moved.ticks[0].events[1], 8U,
                (game_rules_coordinate){3, 0}, (game_rules_coordinate){2, 0},
                4, 3, GAME_RULES_MOVEMENT_PLAYER);
    expect_move(&moved.ticks[1].events[0], 8U,
                (game_rules_coordinate){2, 0}, (game_rules_coordinate){1, 0},
                3, 1, GAME_RULES_MOVEMENT_SLIDE);
    expect_move(&moved.ticks[2].events[0], 8U,
                (game_rules_coordinate){1, 0}, (game_rules_coordinate){0, 0},
                1, 0, GAME_RULES_MOVEMENT_SLIDE);
    assert(find_entity(&moved.final_state, 8U)->coordinate.x == 0);

    game_rules_move_result_dispose(&moved);
    game_rules_load_result_dispose(&loaded);
    game_rules_engine_destroy(engine);
}

int main(void)
{
    test_every_low_direction_and_coordinate_orientation();
    test_whole_stack_retries_after_endpoint_clears();
    test_pushes_onto_connected_ramps_and_slides_to_the_bottom();
    return 0;
}
