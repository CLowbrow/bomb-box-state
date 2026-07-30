#include "game_rules/c_api.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static game_rules_level_definition grid_level(
    game_rules_cell* cells,
    uint32_t width,
    uint32_t height,
    game_rules_coordinate origin,
    uint32_t positive_x,
    uint32_t positive_y,
    const game_rules_fixture* fixtures,
    uint32_t fixture_count,
    const game_rules_entity* entities,
    uint32_t entity_count)
{
    game_rules_level_definition level;
    uint32_t x;
    uint32_t y;
    memset(&level, 0, sizeof(level));
    for (y = 0U; y < height; ++y) {
        for (x = 0U; x < width; ++x) {
            game_rules_cell* cell = &cells[y * width + x];
            cell->coordinate.x = origin.x + (int32_t)x;
            cell->coordinate.y = origin.y + (int32_t)y;
            cell->kind = GAME_RULES_CELL_FLAT;
            cell->elevation = 0;
            cell->low_direction = GAME_RULES_DIRECTION_NORTH;
        }
    }
    level.coordinates.origin = origin;
    level.coordinates.positive_x = positive_x;
    level.coordinates.positive_y = positive_y;
    level.width = width;
    level.height = height;
    level.cells = cells;
    level.cell_count = width * height;
    level.fixtures = fixtures;
    level.fixture_count = fixture_count;
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

static void expect_loaded(game_rules_engine* engine,
                          const game_rules_level_definition* level)
{
    game_rules_load_result loaded = {0};
    assert(game_rules_engine_load_level_data(engine, level, &loaded) ==
           GAME_RULES_CALL_OK);
    assert(loaded.status == GAME_RULES_LOAD_LOADED && loaded.accepted == 1U);
    assert(loaded.tick_count == 0U);
    game_rules_load_result_dispose(&loaded);
}

static void expect_push(game_rules_engine* engine,
                        uint32_t direction,
                        uint64_t player_id,
                        uint64_t pushed_id,
                        game_rules_coordinate player_from,
                        game_rules_coordinate player_to,
                        game_rules_coordinate pushed_to,
                        int32_t bottom)
{
    game_rules_move_result moved = {0};
    const game_rules_entity* entity;
    assert(game_rules_engine_move_data(engine, direction, &moved) ==
           GAME_RULES_CALL_OK);
    assert(moved.status == GAME_RULES_MOVE_MOVED && moved.accepted == 1U);
    assert(moved.has_direction == 1U && moved.direction == direction);
    assert(moved.events == NULL && moved.event_count == 0U);
    assert(moved.has_initial_state == 1U);
    entity = find_entity(&moved.initial_state, player_id);
    assert(entity != NULL && entity->coordinate.x == player_from.x &&
           entity->coordinate.y == player_from.y &&
           entity->bottom_half_steps == bottom);
    entity = find_entity(&moved.initial_state, pushed_id);
    assert(entity != NULL && entity->coordinate.x == player_to.x &&
           entity->coordinate.y == player_to.y &&
           entity->bottom_half_steps == bottom);
    assert(moved.tick_count == 1U && moved.ticks != NULL);
    assert(moved.ticks[0].index == 0U && moved.ticks[0].event_count == 2U);
    assert(moved.ticks[0].events != NULL);
    assert(moved.ticks[0].events[0].kind == GAME_RULES_EVENT_ENTITY_MOVED);
    assert(moved.ticks[0].events[0].entity_id == player_id);
    assert(moved.ticks[0].events[0].from.x == player_from.x);
    assert(moved.ticks[0].events[0].from.y == player_from.y);
    assert(moved.ticks[0].events[0].to.x == player_to.x);
    assert(moved.ticks[0].events[0].to.y == player_to.y);
    assert(moved.ticks[0].events[0].old_bottom_half_steps == bottom);
    assert(moved.ticks[0].events[0].new_bottom_half_steps == bottom);
    assert(moved.ticks[0].events[0].movement_cause == GAME_RULES_MOVEMENT_PLAYER);
    assert(moved.ticks[0].events[1].kind == GAME_RULES_EVENT_ENTITY_MOVED);
    assert(moved.ticks[0].events[1].entity_id == pushed_id);
    assert(moved.ticks[0].events[1].from.x == player_to.x);
    assert(moved.ticks[0].events[1].from.y == player_to.y);
    assert(moved.ticks[0].events[1].to.x == pushed_to.x);
    assert(moved.ticks[0].events[1].to.y == pushed_to.y);
    assert(moved.ticks[0].events[1].old_bottom_half_steps == bottom);
    assert(moved.ticks[0].events[1].new_bottom_half_steps == bottom);
    assert(moved.ticks[0].events[1].movement_cause == GAME_RULES_MOVEMENT_PLAYER);
    entity = find_entity(&moved.ticks[0].state_after, player_id);
    assert(entity != NULL && entity->coordinate.x == player_to.x &&
           entity->coordinate.y == player_to.y);
    entity = find_entity(&moved.ticks[0].state_after, pushed_id);
    assert(entity != NULL && entity->coordinate.x == pushed_to.x &&
           entity->coordinate.y == pushed_to.y);
    assert(moved.has_final_state == 1U && moved.has_state == 1U);
    entity = find_entity(&moved.final_state, player_id);
    assert(entity != NULL && entity->coordinate.x == player_to.x &&
           entity->coordinate.y == player_to.y);
    entity = find_entity(&moved.state.resolved, pushed_id);
    assert(entity != NULL && entity->coordinate.x == pushed_to.x &&
           entity->coordinate.y == pushed_to.y);
    assert(moved.has_outcome == 1U && moved.outcome == GAME_RULES_OUTCOME_ONGOING);
    assert(moved.owned_storage != NULL);
    game_rules_move_result_dispose(&moved);
}

static void expect_rejection(game_rules_engine* engine,
                             uint32_t direction,
                             uint32_t status,
                             uint64_t player_id,
                             game_rules_coordinate unchanged)
{
    game_rules_move_result moved = {0};
    game_rules_state_result after = {0};
    game_rules_rewind_result rewind = {0};
    const game_rules_entity* entity;
    assert(game_rules_engine_move_data(engine, direction, &moved) ==
           GAME_RULES_CALL_OK);
    assert(moved.status == status && moved.accepted == 0U);
    assert(moved.has_direction == 1U && moved.direction == direction);
    assert(moved.event_count == 1U && moved.events != NULL);
    assert(moved.events[0].kind == GAME_RULES_EVENT_MOVE_BLOCKED);
    assert(moved.events[0].direction == direction);
    assert(moved.events[0].move_status == status);
    assert(moved.has_initial_state == 0U && moved.tick_count == 0U);
    assert(moved.has_final_state == 1U && moved.has_state == 1U);
    entity = find_entity(&moved.final_state, player_id);
    assert(entity != NULL && entity->coordinate.x == unchanged.x &&
           entity->coordinate.y == unchanged.y);
    assert(game_rules_engine_get_state_data(engine, &after) == GAME_RULES_CALL_OK);
    entity = find_entity(&after.state.resolved, player_id);
    assert(entity != NULL && entity->coordinate.x == unchanged.x &&
           entity->coordinate.y == unchanged.y);
    assert(game_rules_engine_rewind_data(engine, &rewind) == GAME_RULES_CALL_OK);
    assert(rewind.status == GAME_RULES_REWIND_HISTORY_EMPTY && rewind.accepted == 0U);
    game_rules_rewind_result_dispose(&rewind);
    game_rules_state_result_dispose(&after);
    game_rules_move_result_dispose(&moved);
}

static void test_boxes_and_barrels_in_all_directions(void)
{
    static const struct scenario {
        uint32_t direction;
        uint32_t kind;
        game_rules_coordinate target;
        game_rules_coordinate pushed_to;
    } scenarios[] = {
        {GAME_RULES_DIRECTION_NORTH, GAME_RULES_ENTITY_BOX, {2, 3}, {2, 4}},
        {GAME_RULES_DIRECTION_EAST, GAME_RULES_ENTITY_BARREL, {3, 2}, {4, 2}},
        {GAME_RULES_DIRECTION_SOUTH, GAME_RULES_ENTITY_BOX, {2, 1}, {2, 0}},
        {GAME_RULES_DIRECTION_WEST, GAME_RULES_ENTITY_BARREL, {1, 2}, {0, 2}},
    };
    uint32_t scenario_index;
    for (scenario_index = 0U;
         scenario_index < sizeof(scenarios) / sizeof(scenarios[0]);
         ++scenario_index) {
        game_rules_cell cells[25];
        game_rules_entity entities[2] = {
            {UINT64_MAX, scenarios[scenario_index].kind,
             scenarios[scenario_index].target, 0},
            {17U, GAME_RULES_ENTITY_PLAYER, {2, 2}, 0},
        };
        game_rules_level_definition level = grid_level(
            cells, 5U, 5U, (game_rules_coordinate){0, 0},
            GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH,
            NULL, 0U, entities, 2U);
        game_rules_engine* engine = game_rules_engine_create();
        assert(engine != NULL);
        expect_loaded(engine, &level);
        expect_push(engine, scenarios[scenario_index].direction, 17U, UINT64_MAX,
                    (game_rules_coordinate){2, 2},
                    scenarios[scenario_index].target,
                    scenarios[scenario_index].pushed_to, 0);
        game_rules_engine_destroy(engine);
    }
}

static void test_orientations_repetition_and_id_order(void)
{
    game_rules_cell cells[5];
    game_rules_entity entities[2] = {
        {91U, GAME_RULES_ENTITY_BARREL, {10, -4}, 0},
        {900U, GAME_RULES_ENTITY_PLAYER, {11, -4}, 0},
    };
    game_rules_level_definition level = grid_level(
        cells, 5U, 1U, (game_rules_coordinate){8, -4},
        GAME_RULES_HORIZONTAL_WEST, GAME_RULES_VERTICAL_SOUTH,
        NULL, 0U, entities, 2U);
    game_rules_engine* engine = game_rules_engine_create();
    assert(engine != NULL);
    expect_loaded(engine, &level);
    expect_push(engine, GAME_RULES_DIRECTION_EAST, 900U, 91U,
                (game_rules_coordinate){11, -4}, (game_rules_coordinate){10, -4},
                (game_rules_coordinate){9, -4}, 0);
    expect_push(engine, GAME_RULES_DIRECTION_EAST, 900U, 91U,
                (game_rules_coordinate){10, -4}, (game_rules_coordinate){9, -4},
                (game_rules_coordinate){8, -4}, 0);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                     GAME_RULES_MOVE_WORLD_BOUNDARY, 900U,
                     (game_rules_coordinate){9, -4});
    game_rules_engine_destroy(engine);

    {
        game_rules_cell ordered_cells[3];
        game_rules_entity reordered[2] = {
            {2U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
            {99U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
        };
        game_rules_level_definition reordered_level = grid_level(
            ordered_cells, 3U, 1U, (game_rules_coordinate){0, 0},
            GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH,
            NULL, 0U, reordered, 2U);
        engine = game_rules_engine_create();
        assert(engine != NULL);
        expect_loaded(engine, &reordered_level);
        expect_push(engine, GAME_RULES_DIRECTION_EAST, 99U, 2U,
                    (game_rules_coordinate){0, 0}, (game_rules_coordinate){1, 0},
                    (game_rules_coordinate){2, 0}, 0);
        game_rules_engine_destroy(engine);
    }
}

static void test_stacks_and_bottom_half_steps(void)
{
    uint32_t top_kind;
    for (top_kind = GAME_RULES_ENTITY_BOX;
         top_kind <= GAME_RULES_ENTITY_BARREL; ++top_kind) {
        game_rules_cell cells[3];
        game_rules_entity entities[4] = {
            {17U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 4},
            {40U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
            {2U, GAME_RULES_ENTITY_BARREL, {1, 0}, 2},
            {88U, top_kind, {1, 0}, 4},
        };
        game_rules_level_definition level = grid_level(
            cells, 3U, 1U, (game_rules_coordinate){0, 0},
            GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH,
            NULL, 0U, entities, 4U);
        game_rules_engine* engine;
        cells[0].elevation = 2;
        cells[2].elevation = 2;
        engine = game_rules_engine_create();
        assert(engine != NULL);
        expect_loaded(engine, &level);
        expect_push(engine, GAME_RULES_DIRECTION_EAST, 17U, 88U,
                    (game_rules_coordinate){0, 0}, (game_rules_coordinate){1, 0},
                    (game_rules_coordinate){2, 0}, 4);
        {
            game_rules_state_result state = {0};
            assert(game_rules_engine_get_state_data(engine, &state) ==
                   GAME_RULES_CALL_OK);
            assert(state.state.resolved.entity_count == 4U);
            assert(state.state.resolved.entities[0].id == 40U);
            assert(state.state.resolved.entities[1].id == 2U);
            assert(state.state.resolved.entities[2].id == 17U);
            assert(state.state.resolved.entities[3].id == 88U);
            game_rules_state_result_dispose(&state);
        }
        game_rules_engine_destroy(engine);
    }

    {
        game_rules_cell cells[3];
        game_rules_entity entities[3] = {
            {17U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 2},
            {4U, GAME_RULES_ENTITY_BOX, {1, 0}, 2},
            {5U, GAME_RULES_ENTITY_BARREL, {2, 0}, 0},
        };
        game_rules_level_definition level = grid_level(
            cells, 3U, 1U, (game_rules_coordinate){0, 0},
            GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH,
            NULL, 0U, entities, 3U);
        game_rules_engine* engine;
        cells[0].elevation = 1;
        cells[1].elevation = 1;
        engine = game_rules_engine_create();
        assert(engine != NULL);
        expect_loaded(engine, &level);
        expect_push(engine, GAME_RULES_DIRECTION_EAST, 17U, 4U,
                    (game_rules_coordinate){0, 0}, (game_rules_coordinate){1, 0},
                    (game_rules_coordinate){2, 0}, 2);
        game_rules_engine_destroy(engine);
    }
}

static void test_rejection_matrix(void)
{
    game_rules_cell cells[4];
    game_rules_entity entities[3] = {
        {17U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
        {4U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
        {5U, GAME_RULES_ENTITY_BARREL, {2, 0}, 0},
    };
    game_rules_level_definition level = grid_level(
        cells, 4U, 1U, (game_rules_coordinate){0, 0},
        GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH,
        NULL, 0U, entities, 3U);
    game_rules_engine* engine = game_rules_engine_create();
    assert(engine != NULL);
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST, GAME_RULES_MOVE_OCCUPIED,
                     17U, (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    level.entity_count = 2U;
    level.width = 2U;
    level.cell_count = 2U;
    engine = game_rules_engine_create();
    assert(engine != NULL);
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                     GAME_RULES_MOVE_WORLD_BOUNDARY, 17U,
                     (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    level.width = 3U;
    level.cell_count = 3U;
    cells[2].elevation = 1;
    engine = game_rules_engine_create();
    assert(engine != NULL);
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST, GAME_RULES_MOVE_LEDGE,
                     17U, (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    cells[0].elevation = 2;
    cells[1].elevation = 2;
    cells[2].elevation = 0;
    entities[0].bottom_half_steps = 4;
    entities[1].bottom_half_steps = 4;
    engine = game_rules_engine_create();
    assert(engine != NULL);
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                     GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY, 17U,
                     (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    {
        game_rules_fixture fixture = {{2, 0}, GAME_RULES_FIXTURE_DOOR,
                                      GAME_RULES_COLOR_GREEN};
        cells[0].elevation = 0;
        cells[1].elevation = 0;
        cells[2].elevation = 0;
        entities[0].bottom_half_steps = 0;
        entities[1].bottom_half_steps = 0;
        level.fixtures = &fixture;
        level.fixture_count = 1U;
        engine = game_rules_engine_create();
        assert(engine != NULL);
        expect_loaded(engine, &level);
        expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                         GAME_RULES_MOVE_CLOSED_DOOR, 17U,
                         (game_rules_coordinate){0, 0});
        game_rules_engine_destroy(engine);

        fixture.kind = GAME_RULES_FIXTURE_EXIT;
        engine = game_rules_engine_create();
        assert(engine != NULL);
        expect_loaded(engine, &level);
        expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                         GAME_RULES_MOVE_TELEPORTER_RESTRICTION, 17U,
                         (game_rules_coordinate){0, 0});
        game_rules_engine_destroy(engine);

        fixture.kind = GAME_RULES_FIXTURE_SWITCH;
        engine = game_rules_engine_create();
        assert(engine != NULL);
        expect_loaded(engine, &level);
        expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                         GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY, 17U,
                         (game_rules_coordinate){0, 0});
        game_rules_engine_destroy(engine);
    }

    level.fixtures = NULL;
    level.fixture_count = 0U;
    cells[2].kind = GAME_RULES_CELL_RAMP;
    cells[2].low_direction = GAME_RULES_DIRECTION_WEST;
    cells[3].elevation = 1;
    level.width = 4U;
    level.cell_count = 4U;
    engine = game_rules_engine_create();
    assert(engine != NULL);
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                     GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY, 17U,
                     (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    {
        game_rules_entity stacked[3] = {
            {17U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
            {4U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
            {5U, GAME_RULES_ENTITY_BARREL, {1, 0}, 2},
        };
        game_rules_level_definition stacked_level = grid_level(
            cells, 4U, 1U, (game_rules_coordinate){0, 0},
            GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH,
            NULL, 0U, stacked, 3U);
        engine = game_rules_engine_create();
        assert(engine != NULL);
        expect_loaded(engine, &stacked_level);
        expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                         GAME_RULES_MOVE_STACKED_PUSH_TARGET, 17U,
                         (game_rules_coordinate){0, 0});
        game_rules_engine_destroy(engine);
    }
}

int main(void)
{
    test_boxes_and_barrels_in_all_directions();
    test_orientations_repetition_and_id_order();
    test_stacks_and_bottom_half_steps();
    test_rejection_matrix();
    return 0;
}
