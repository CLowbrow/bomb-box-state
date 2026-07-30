#include "game_rules/c_api.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static game_rules_level_definition grid_level(game_rules_cell* cells,
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

static void expect_loaded(game_rules_engine* engine,
                          const game_rules_level_definition* level)
{
    game_rules_load_result loaded = {0};
    assert(game_rules_engine_load_level_data(engine, level, &loaded) ==
           GAME_RULES_CALL_OK);
    assert(loaded.status == GAME_RULES_LOAD_LOADED);
    assert(loaded.accepted == 1U);
    game_rules_load_result_dispose(&loaded);
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

static void expect_walk(game_rules_engine* engine,
                        uint32_t direction,
                        game_rules_coordinate from,
                        game_rules_coordinate to,
                        int32_t bottom)
{
    game_rules_move_result moved = {0};
    const game_rules_event* event;
    const game_rules_entity* initial_player;
    const game_rules_entity* tick_player;
    const game_rules_entity* final_player;
    const game_rules_entity* snapshot_player;
    assert(game_rules_engine_move_data(engine, direction, &moved) ==
           GAME_RULES_CALL_OK);
    assert(moved.status == GAME_RULES_MOVE_MOVED);
    assert(moved.accepted == 1U);
    assert(moved.has_direction == 1U && moved.direction == direction);
    assert(moved.events == NULL && moved.event_count == 0U);
    assert(moved.has_initial_state == 1U);
    initial_player = find_entity(&moved.initial_state, 17U);
    assert(initial_player != NULL);
    assert(initial_player->coordinate.x == from.x);
    assert(initial_player->coordinate.y == from.y);
    assert(moved.tick_count == 1U && moved.ticks != NULL);
    assert(moved.ticks[0].index == 0U);
    assert(moved.ticks[0].event_count == 1U);
    event = &moved.ticks[0].events[0];
    assert(event->kind == GAME_RULES_EVENT_ENTITY_MOVED);
    assert(event->entity_id == 17U);
    assert(event->from.x == from.x && event->from.y == from.y);
    assert(event->to.x == to.x && event->to.y == to.y);
    assert(event->old_bottom_half_steps == bottom);
    assert(event->new_bottom_half_steps == bottom);
    assert(event->movement_cause == GAME_RULES_MOVEMENT_PLAYER);
    tick_player = find_entity(&moved.ticks[0].state_after, 17U);
    assert(tick_player != NULL);
    assert(tick_player->coordinate.x == to.x);
    assert(tick_player->coordinate.y == to.y);
    assert(moved.has_final_state == 1U);
    final_player = find_entity(&moved.final_state, 17U);
    assert(final_player != NULL);
    assert(final_player->coordinate.x == to.x);
    assert(final_player->coordinate.y == to.y);
    assert(moved.has_state == 1U);
    snapshot_player = find_entity(&moved.state.resolved, 17U);
    assert(snapshot_player != NULL);
    assert(snapshot_player->coordinate.x == to.x);
    assert(snapshot_player->coordinate.y == to.y);
    assert(moved.has_outcome == 1U && moved.outcome == GAME_RULES_OUTCOME_ONGOING);
    game_rules_move_result_dispose(&moved);
}

static void expect_rejection(game_rules_engine* engine,
                             uint32_t direction,
                             uint32_t status,
                             game_rules_coordinate unchanged)
{
    game_rules_move_result moved = {0};
    assert(game_rules_engine_move_data(engine, direction, &moved) ==
           GAME_RULES_CALL_OK);
    assert(moved.status == status && moved.accepted == 0U);
    assert(moved.has_direction == 1U && moved.direction == direction);
    assert(moved.event_count == 1U && moved.events != NULL);
    assert(moved.events[0].kind == GAME_RULES_EVENT_MOVE_BLOCKED);
    assert(moved.events[0].direction == direction);
    assert(moved.events[0].move_status == status);
    assert(moved.has_initial_state == 0U && moved.tick_count == 0U);
    assert(moved.has_final_state == 1U);
    assert(moved.final_state.entities[0].coordinate.x == unchanged.x);
    assert(moved.final_state.entities[0].coordinate.y == unchanged.y);
    assert(moved.has_state == 1U);
    assert(moved.state.resolved.entities[0].coordinate.x == unchanged.x);
    assert(moved.state.resolved.entities[0].coordinate.y == unchanged.y);
    assert(moved.has_outcome == 1U);
    game_rules_move_result_dispose(&moved);
}

static void test_cardinal_orientation_and_repetition(void)
{
    game_rules_cell cells[9];
    game_rules_entity player =
        {17U, GAME_RULES_ENTITY_PLAYER, {11, -6}, 0};
    game_rules_level_definition level = grid_level(
        cells, 3U, 3U, (game_rules_coordinate){10, -7},
        GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH,
        NULL, 0U, &player, 1U);
    game_rules_engine* engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    expect_walk(engine, GAME_RULES_DIRECTION_NORTH, (game_rules_coordinate){11, -6},
                (game_rules_coordinate){11, -5}, 0);
    expect_walk(engine, GAME_RULES_DIRECTION_EAST, (game_rules_coordinate){11, -5},
                (game_rules_coordinate){12, -5}, 0);
    expect_walk(engine, GAME_RULES_DIRECTION_SOUTH, (game_rules_coordinate){12, -5},
                (game_rules_coordinate){12, -6}, 0);
    expect_walk(engine, GAME_RULES_DIRECTION_WEST, (game_rules_coordinate){12, -6},
                (game_rules_coordinate){11, -6}, 0);
    game_rules_engine_destroy(engine);

    level.coordinates.positive_x = GAME_RULES_HORIZONTAL_WEST;
    level.coordinates.positive_y = GAME_RULES_VERTICAL_SOUTH;
    engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    expect_walk(engine, GAME_RULES_DIRECTION_NORTH, (game_rules_coordinate){11, -6},
                (game_rules_coordinate){11, -7}, 0);
    expect_walk(engine, GAME_RULES_DIRECTION_EAST, (game_rules_coordinate){11, -7},
                (game_rules_coordinate){10, -7}, 0);
    expect_walk(engine, GAME_RULES_DIRECTION_SOUTH, (game_rules_coordinate){10, -7},
                (game_rules_coordinate){10, -6}, 0);
    expect_walk(engine, GAME_RULES_DIRECTION_WEST, (game_rules_coordinate){10, -6},
                (game_rules_coordinate){11, -6}, 0);
    game_rules_engine_destroy(engine);
}

static void test_boundaries_elevations_and_occupancy(void)
{
    game_rules_cell cells[3];
    game_rules_entity entities[3] = {
        {17U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
        {4U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
        {5U, GAME_RULES_ENTITY_BARREL, {2, 0}, 0}};
    game_rules_level_definition level = grid_level(
        cells, 3U, 1U, (game_rules_coordinate){0, 0},
        GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH,
        NULL, 0U, entities, 1U);
    game_rules_engine* engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_WEST,
                     GAME_RULES_MOVE_WORLD_BOUNDARY, (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    cells[1].elevation = 1;
    engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                     GAME_RULES_MOVE_LEDGE, (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    cells[0].elevation = 1;
    cells[1].elevation = 0;
    entities[0].bottom_half_steps = 2;
    engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                     GAME_RULES_MOVE_LEDGE, (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    cells[0].elevation = 0;
    entities[0].bottom_half_steps = 0;
    level.entity_count = 3U;
    engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                     GAME_RULES_MOVE_OCCUPIED, (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    cells[0].elevation = 1;
    entities[0].bottom_half_steps = 2;
    level.entity_count = 2U;
    engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    expect_walk(engine, GAME_RULES_DIRECTION_EAST, (game_rules_coordinate){0, 0},
                (game_rules_coordinate){1, 0}, 2);
    game_rules_engine_destroy(engine);
}

static void test_fixture_geometry_terminal_and_invalid_rejections(void)
{
    game_rules_cell cells[3];
    game_rules_entity player =
        {17U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0};
    game_rules_fixture fixture = {{1, 0}, GAME_RULES_FIXTURE_DOOR,
                                  GAME_RULES_COLOR_RED};
    game_rules_level_definition level = grid_level(
        cells, 3U, 1U, (game_rules_coordinate){0, 0},
        GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH,
        &fixture, 1U, &player, 1U);
    game_rules_engine* engine = game_rules_engine_create();
    game_rules_move_result invalid = {0};
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                     GAME_RULES_MOVE_CLOSED_DOOR, (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    fixture.kind = GAME_RULES_FIXTURE_EXIT;
    cells[0].elevation = 1;
    player.bottom_half_steps = 2;
    engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                     GAME_RULES_MOVE_TELEPORTER_RESTRICTION,
                     (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    fixture.kind = GAME_RULES_FIXTURE_DOOR;
    cells[0].elevation = 0;
    cells[1].kind = GAME_RULES_CELL_RAMP;
    cells[1].low_direction = GAME_RULES_DIRECTION_WEST;
    cells[2].elevation = 1;
    player.bottom_half_steps = 0;
    level.fixture_count = 0U;
    engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    {
        game_rules_move_result ramp_move = {0};
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &ramp_move) == GAME_RULES_CALL_OK);
        assert(ramp_move.accepted && ramp_move.tick_count == 1U);
        assert(ramp_move.ticks[0].event_count == 1U);
        assert(ramp_move.ticks[0].events[0].kind ==
               GAME_RULES_EVENT_ENTITY_MOVED);
        assert(ramp_move.ticks[0].events[0].old_bottom_half_steps == 0);
        assert(ramp_move.ticks[0].events[0].new_bottom_half_steps == 1);
        assert(ramp_move.ticks[0].events[0].movement_cause ==
               GAME_RULES_MOVEMENT_PLAYER);
        game_rules_move_result_dispose(&ramp_move);
    }
    game_rules_engine_destroy(engine);

    cells[1].kind = GAME_RULES_CELL_FLAT;
    fixture.kind = GAME_RULES_FIXTURE_EXIT;
    fixture.coordinate = (game_rules_coordinate){0, 0};
    level.fixture_count = 1U;
    engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    expect_rejection(engine, GAME_RULES_DIRECTION_EAST,
                     GAME_RULES_MOVE_LEVEL_TERMINAL,
                     (game_rules_coordinate){0, 0});
    game_rules_engine_destroy(engine);

    level.fixture_count = 0U;
    engine = game_rules_engine_create();
    expect_loaded(engine, &level);
    assert(game_rules_engine_move_data(engine, 99U, &invalid) == GAME_RULES_CALL_OK);
    assert(invalid.status == GAME_RULES_MOVE_INVALID_DIRECTION);
    assert(invalid.accepted == 0U && invalid.has_direction == 0U);
    assert(invalid.direction == 0U);
    assert(invalid.events == NULL && invalid.event_count == 0U);
    assert(invalid.has_initial_state == 0U && invalid.tick_count == 0U);
    assert(invalid.has_final_state == 1U && invalid.has_state == 1U);
    assert(invalid.final_state.entities[0].coordinate.x == 0);
    game_rules_move_result_dispose(&invalid);
    game_rules_engine_destroy(engine);
}

int main(void)
{
    test_cardinal_orientation_and_repetition();
    test_boundaries_elevations_and_occupancy();
    test_fixture_geometry_terminal_and_invalid_rejections();
    return 0;
}
