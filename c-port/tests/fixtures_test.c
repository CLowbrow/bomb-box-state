#include "game_rules/c_api.h"

#include <assert.h>
#include <stdint.h>
#include <string.h>

static void fill_flat_cells(game_rules_cell* cells, uint32_t count)
{
    uint32_t index;
    for (index = 0U; index < count; ++index) {
        cells[index].coordinate.x = (int32_t)index;
        cells[index].coordinate.y = 0;
        cells[index].kind = GAME_RULES_CELL_FLAT;
        cells[index].elevation = 0;
        cells[index].low_direction = GAME_RULES_DIRECTION_NORTH;
    }
}

static game_rules_level_definition line_level(game_rules_cell* cells,
                                              uint32_t width,
                                              game_rules_fixture* fixtures,
                                              uint32_t fixture_count,
                                              game_rules_entity* entities,
                                              uint32_t entity_count)
{
    game_rules_level_definition level;
    memset(&level, 0, sizeof(level));
    fill_flat_cells(cells, width);
    level.coordinates.positive_x = GAME_RULES_HORIZONTAL_EAST;
    level.coordinates.positive_y = GAME_RULES_VERTICAL_NORTH;
    level.width = width;
    level.height = 1U;
    level.cells = cells;
    level.cell_count = width;
    level.fixtures = fixtures;
    level.fixture_count = fixture_count;
    level.entities = entities;
    level.entity_count = entity_count;
    return level;
}

static void assert_fixture_state(const game_rules_resolved_state* state,
                                 const uint32_t* colors,
                                 uint32_t color_count,
                                 const int32_t* door_xs,
                                 uint32_t door_count,
                                 uint32_t outcome)
{
    uint32_t index;
    assert(state->active_switch_color_count == color_count);
    assert((state->active_switch_colors == NULL) == (color_count == 0U));
    for (index = 0U; index < color_count; ++index)
        assert(state->active_switch_colors[index] == colors[index]);
    assert(state->open_door_count == door_count);
    assert((state->open_doors == NULL) == (door_count == 0U));
    for (index = 0U; index < door_count; ++index) {
        assert(state->open_doors[index].x == door_xs[index]);
        assert(state->open_doors[index].y == 0);
    }
    assert(state->outcome == outcome);
}

static void test_canonical_all_colors_and_owned_views(void)
{
    game_rules_cell cells[13];
    game_rules_fixture fixtures[10] = {
        {{9, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_RED},
        {{1, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED},
        {{5, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_YELLOW},
        {{3, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_BLUE},
        {{7, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_BLUE},
        {{0, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED},
        {{8, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_GREEN},
        {{4, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_YELLOW},
        {{6, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_RED},
        {{2, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_GREEN},
    };
    game_rules_entity entities[6] = {
        {40U, GAME_RULES_ENTITY_BOX, {4, 0}, 0},
        {1U, GAME_RULES_ENTITY_PLAYER, {12, 0}, 0},
        {20U, GAME_RULES_ENTITY_BARREL, {1, 0}, 0},
        {30U, GAME_RULES_ENTITY_BOX, {3, 0}, 0},
        {10U, GAME_RULES_ENTITY_BOX, {0, 0}, 0},
        {25U, GAME_RULES_ENTITY_BOX, {2, 0}, 0},
    };
    const uint32_t colors[4] = {
        GAME_RULES_COLOR_RED, GAME_RULES_COLOR_GREEN,
        GAME_RULES_COLOR_BLUE, GAME_RULES_COLOR_YELLOW};
    const int32_t doors[5] = {5, 6, 7, 8, 9};
    game_rules_level_definition level = line_level(
        cells, 13U, fixtures, 10U, entities, 6U);
    game_rules_engine* engine = game_rules_engine_create();
    game_rules_load_result loaded = {0};
    game_rules_state_result current = {0};
    uint32_t index;

    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
           GAME_RULES_CALL_OK);
    assert(loaded.accepted && loaded.tick_count == 1U);
    assert(loaded.ticks[0].event_count == 9U);
    for (index = 0U; index < 4U; ++index) {
        const game_rules_event* event = &loaded.ticks[0].events[index];
        assert(event->kind == GAME_RULES_EVENT_SWITCH_CHANGED);
        assert(event->color == colors[index] && event->active == 1U);
    }
    for (index = 0U; index < 5U; ++index) {
        const game_rules_event* event = &loaded.ticks[0].events[index + 4U];
        assert(event->kind == GAME_RULES_EVENT_DOOR_OPENED);
        assert(event->coordinate.x == doors[index] && event->coordinate.y == 0);
    }
    assert_fixture_state(&loaded.ticks[0].state_after, colors, 4U, doors, 5U,
                         GAME_RULES_OUTCOME_ONGOING);
    assert_fixture_state(&loaded.final_state, colors, 4U, doors, 5U,
                         GAME_RULES_OUTCOME_ONGOING);
    assert_fixture_state(&loaded.state.resolved, colors, 4U, doors, 5U,
                         GAME_RULES_OUTCOME_ONGOING);
    assert(loaded.state.level.fixture_count == 10U);
    for (index = 1U; index < loaded.state.level.fixture_count; ++index) {
        assert(loaded.state.level.fixtures[index - 1U].coordinate.x <
               loaded.state.level.fixtures[index].coordinate.x);
    }
    assert(loaded.ticks[0].state_after.active_switch_colors !=
           loaded.final_state.active_switch_colors);
    assert(loaded.final_state.active_switch_colors !=
           loaded.state.resolved.active_switch_colors);
    assert(loaded.ticks[0].state_after.open_doors != loaded.final_state.open_doors);
    assert(loaded.final_state.open_doors != loaded.state.resolved.open_doors);

    assert(game_rules_engine_get_state_data(engine, &current) == GAME_RULES_CALL_OK);
    assert(current.has_state);
    assert_fixture_state(&current.state.resolved, colors, 4U, doors, 5U,
                         GAME_RULES_OUTCOME_ONGOING);
    assert(current.state.resolved.active_switch_colors !=
           loaded.state.resolved.active_switch_colors);
    assert(current.state.resolved.open_doors != loaded.state.resolved.open_doors);
    assert(current.state.level.fixtures != loaded.state.level.fixtures);

    fixtures[0].coordinate.x = 99;
    entities[0].coordinate.x = 99;
    assert(loaded.state.level.fixtures[9].coordinate.x == 9);
    assert(loaded.state.resolved.entities[4].coordinate.x == 4);
    game_rules_engine_destroy(engine);
    assert_fixture_state(&current.state.resolved, colors, 4U, doors, 5U,
                         GAME_RULES_OUTCOME_ONGOING);
    assert_fixture_state(&loaded.final_state, colors, 4U, doors, 5U,
                         GAME_RULES_OUTCOME_ONGOING);
    game_rules_state_result_dispose(&current);
    game_rules_load_result_dispose(&loaded);
}

static void test_simultaneous_switch_and_door_order(void)
{
    game_rules_cell cells[6];
    game_rules_fixture fixtures[4] = {
        {{5, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_RED},
        {{0, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_BLUE},
        {{4, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_BLUE},
        {{2, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED},
    };
    game_rules_entity entities[2] = {
        {9U, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
        {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
    };
    const uint32_t blue[1] = {GAME_RULES_COLOR_BLUE};
    const uint32_t red[1] = {GAME_RULES_COLOR_RED};
    const int32_t blue_door[1] = {4};
    const int32_t red_door[1] = {5};
    game_rules_level_definition level = line_level(
        cells, 6U, fixtures, 4U, entities, 2U);
    game_rules_engine* engine = game_rules_engine_create();
    game_rules_load_result loaded = {0};
    game_rules_move_result moved = {0};
    const game_rules_event* events;

    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
           GAME_RULES_CALL_OK);
    assert(loaded.accepted && loaded.tick_count == 1U);
    assert_fixture_state(&loaded.final_state, blue, 1U, blue_door, 1U,
                         GAME_RULES_OUTCOME_ONGOING);
    game_rules_load_result_dispose(&loaded);

    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &moved) ==
           GAME_RULES_CALL_OK);
    assert(moved.accepted && moved.tick_count == 1U);
    assert(moved.ticks[0].event_count == 6U);
    events = moved.ticks[0].events;
    assert(events[0].kind == GAME_RULES_EVENT_ENTITY_MOVED &&
           events[0].entity_id == 1U);
    assert(events[1].kind == GAME_RULES_EVENT_ENTITY_MOVED &&
           events[1].entity_id == 9U);
    assert(events[2].kind == GAME_RULES_EVENT_SWITCH_CHANGED &&
           events[2].color == GAME_RULES_COLOR_RED && events[2].active == 1U);
    assert(events[3].kind == GAME_RULES_EVENT_SWITCH_CHANGED &&
           events[3].color == GAME_RULES_COLOR_BLUE && events[3].active == 0U);
    assert(events[4].kind == GAME_RULES_EVENT_DOOR_CLOSED &&
           events[4].coordinate.x == 4 && events[4].color == GAME_RULES_COLOR_BLUE);
    assert(events[5].kind == GAME_RULES_EVENT_DOOR_OPENED &&
           events[5].coordinate.x == 5 && events[5].color == GAME_RULES_COLOR_RED);
    assert_fixture_state(&moved.ticks[0].state_after, red, 1U, red_door, 1U,
                         GAME_RULES_OUTCOME_ONGOING);
    assert_fixture_state(&moved.final_state, red, 1U, red_door, 1U,
                         GAME_RULES_OUTCOME_ONGOING);
    assert_fixture_state(&moved.state.resolved, red, 1U, red_door, 1U,
                         GAME_RULES_OUTCOME_ONGOING);
    assert_fixture_state(&moved.initial_state, blue, 1U, blue_door, 1U,
                         GAME_RULES_OUTCOME_ONGOING);
    game_rules_move_result_dispose(&moved);
    game_rules_engine_destroy(engine);
}

static void test_multitick_gravity_and_held_door(void)
{
    game_rules_cell cells[4];
    game_rules_fixture fixtures[2] = {
        {{3, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_GREEN},
        {{2, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_GREEN},
    };
    game_rules_entity entities[2] = {
        {8U, GAME_RULES_ENTITY_BOX, {1, 0}, 4},
        {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 4},
    };
    const uint32_t green[1] = {GAME_RULES_COLOR_GREEN};
    const int32_t green_door[1] = {3};
    game_rules_level_definition level = line_level(
        cells, 4U, fixtures, 2U, entities, 2U);
    game_rules_engine* engine;
    game_rules_load_result loaded = {0};
    game_rules_move_result moved = {0};

    cells[0].elevation = 2;
    cells[1].elevation = 2;
    engine = game_rules_engine_create();
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
           GAME_RULES_CALL_OK);
    assert(loaded.accepted && loaded.tick_count == 0U);
    game_rules_load_result_dispose(&loaded);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &moved) ==
           GAME_RULES_CALL_OK);
    assert(moved.accepted && moved.tick_count == 2U);
    assert(moved.ticks[0].event_count == 2U);
    assert(moved.ticks[1].event_count == 3U);
    assert(moved.ticks[1].events[0].kind == GAME_RULES_EVENT_ENTITY_MOVED);
    assert(moved.ticks[1].events[0].movement_cause == GAME_RULES_MOVEMENT_FALL);
    assert(moved.ticks[1].events[1].kind == GAME_RULES_EVENT_SWITCH_CHANGED);
    assert(moved.ticks[1].events[2].kind == GAME_RULES_EVENT_DOOR_OPENED);
    assert_fixture_state(&moved.ticks[1].state_after, green, 1U,
                         green_door, 1U, GAME_RULES_OUTCOME_ONGOING);
    game_rules_move_result_dispose(&moved);
    game_rules_engine_destroy(engine);

    {
        game_rules_cell held_cells[3];
        game_rules_fixture held_fixture = {
            {1, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_YELLOW};
        game_rules_entity held_entities[2] = {
            {8U, GAME_RULES_ENTITY_BOX, {1, 0}, 4},
            {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
        };
        const int32_t held_door[1] = {1};
        game_rules_level_definition held_level = line_level(
            held_cells, 3U, &held_fixture, 1U, held_entities, 2U);
        engine = game_rules_engine_create();
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &held_level, &loaded) ==
               GAME_RULES_CALL_OK);
        assert(loaded.accepted && loaded.tick_count == 2U);
        assert(loaded.ticks[0].events[0].kind == GAME_RULES_EVENT_DOOR_OPENED);
        assert(loaded.ticks[1].events[0].movement_cause == GAME_RULES_MOVEMENT_FALL);
        assert_fixture_state(&loaded.final_state, NULL, 0U, held_door, 1U,
                             GAME_RULES_OUTCOME_ONGOING);
        game_rules_load_result_dispose(&loaded);
        game_rules_engine_destroy(engine);
    }
}

static void test_and_switches_door_closing_and_rejections(void)
{
    {
        game_rules_cell cells[4];
        game_rules_fixture fixtures[3] = {
            {{0, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED},
            {{1, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED},
            {{2, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_RED},
        };
        game_rules_entity entities[2] = {
            {2U, GAME_RULES_ENTITY_BOX, {0, 0}, 0},
            {1U, GAME_RULES_ENTITY_PLAYER, {3, 0}, 0},
        };
        game_rules_level_definition level = line_level(
            cells, 4U, fixtures, 3U, entities, 2U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result loaded = {0};
        game_rules_move_result rejected = {0};
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
               GAME_RULES_CALL_OK);
        assert(loaded.accepted && loaded.tick_count == 0U);
        assert(loaded.final_state.active_switch_color_count == 0U);
        assert(loaded.final_state.open_door_count == 0U);
        game_rules_load_result_dispose(&loaded);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST,
                                           &rejected) == GAME_RULES_CALL_OK);
        assert(!rejected.accepted && rejected.status == GAME_RULES_MOVE_CLOSED_DOOR);
        assert(rejected.tick_count == 0U && rejected.event_count == 1U);
        assert(rejected.final_state.active_switch_color_count == 0U);
        assert(rejected.final_state.open_door_count == 0U);
        game_rules_move_result_dispose(&rejected);
        game_rules_engine_destroy(engine);
    }
    {
        game_rules_cell cells[3];
        game_rules_fixture fixture = {
            {1, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_YELLOW};
        game_rules_entity player = {
            1U, GAME_RULES_ENTITY_PLAYER, {1, 0}, 0};
        game_rules_level_definition level = line_level(
            cells, 3U, &fixture, 1U, &player, 1U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result loaded = {0};
        game_rules_move_result vacated = {0};
        game_rules_move_result rejected = {0};
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
               GAME_RULES_CALL_OK);
        assert(loaded.accepted && loaded.tick_count == 1U);
        assert(loaded.ticks[0].event_count == 1U);
        assert(loaded.ticks[0].events[0].kind == GAME_RULES_EVENT_DOOR_OPENED);
        game_rules_load_result_dispose(&loaded);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &vacated) == GAME_RULES_CALL_OK);
        assert(vacated.accepted && vacated.tick_count == 1U);
        assert(vacated.ticks[0].event_count == 2U);
        assert(vacated.ticks[0].events[1].kind == GAME_RULES_EVENT_DOOR_CLOSED);
        assert(vacated.final_state.open_door_count == 0U);
        game_rules_move_result_dispose(&vacated);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST,
                                           &rejected) == GAME_RULES_CALL_OK);
        assert(!rejected.accepted && rejected.status == GAME_RULES_MOVE_CLOSED_DOOR);
        game_rules_move_result_dispose(&rejected);
        game_rules_engine_destroy(engine);
    }
    {
        game_rules_cell cells[4];
        game_rules_fixture fixtures[2] = {
            {{0, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_BLUE},
            {{2, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_BLUE},
        };
        game_rules_entity player = {
            1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0};
        game_rules_level_definition level = line_level(
            cells, 4U, fixtures, 2U, &player, 1U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result loaded = {0};
        game_rules_move_result moved = {0};
        game_rules_move_result rejected = {0};
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
               GAME_RULES_CALL_OK);
        assert(loaded.final_state.active_switch_color_count == 1U);
        assert(loaded.final_state.open_door_count == 1U);
        game_rules_load_result_dispose(&loaded);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &moved) == GAME_RULES_CALL_OK);
        assert(moved.accepted && moved.ticks[0].event_count == 3U);
        assert(moved.ticks[0].events[1].kind == GAME_RULES_EVENT_SWITCH_CHANGED);
        assert(moved.ticks[0].events[1].active == 0U);
        assert(moved.ticks[0].events[2].kind == GAME_RULES_EVENT_DOOR_CLOSED);
        game_rules_move_result_dispose(&moved);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &rejected) == GAME_RULES_CALL_OK);
        assert(!rejected.accepted && rejected.status == GAME_RULES_MOVE_CLOSED_DOOR);
        game_rules_move_result_dispose(&rejected);
        game_rules_engine_destroy(engine);
    }
}

static void test_fixture_changes_precede_loss_and_exit_height_is_restricted(void)
{
    {
        game_rules_cell cells[4];
        game_rules_fixture fixtures[2] = {
            {{0, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED},
            {{2, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_RED},
        };
        game_rules_entity entities[2] = {
            {1U, GAME_RULES_ENTITY_PLAYER, {3, 0}, 2},
            {2U, GAME_RULES_ENTITY_BOX, {0, 0}, 2},
        };
        game_rules_level_definition level = line_level(
            cells, 4U, fixtures, 2U, entities, 2U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result loaded = {0};
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
               GAME_RULES_CALL_OK);
        assert(loaded.accepted && loaded.tick_count == 1U);
        assert(loaded.ticks[0].event_count == 5U);
        assert(loaded.ticks[0].events[0].kind == GAME_RULES_EVENT_ENTITY_MOVED);
        assert(loaded.ticks[0].events[0].entity_id == 2U);
        assert(loaded.ticks[0].events[1].kind == GAME_RULES_EVENT_ENTITY_MOVED);
        assert(loaded.ticks[0].events[1].entity_id == 1U);
        assert(loaded.ticks[0].events[2].kind == GAME_RULES_EVENT_SWITCH_CHANGED);
        assert(loaded.ticks[0].events[3].kind == GAME_RULES_EVENT_DOOR_OPENED);
        assert(loaded.ticks[0].events[4].kind == GAME_RULES_EVENT_LEVEL_LOST);
        assert(loaded.final_state.outcome == GAME_RULES_OUTCOME_LOST);
        assert(loaded.final_state.active_switch_color_count == 1U);
        assert(loaded.final_state.open_door_count == 1U);
        game_rules_load_result_dispose(&loaded);
        game_rules_engine_destroy(engine);
    }
    {
        game_rules_cell cells[2];
        game_rules_fixture exit_fixture = {
            {1, 0}, GAME_RULES_FIXTURE_EXIT, GAME_RULES_COLOR_RED};
        game_rules_entity player = {
            1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 2};
        game_rules_level_definition level = line_level(
            cells, 2U, &exit_fixture, 1U, &player, 1U);
        game_rules_engine* engine = game_rules_engine_create();
        game_rules_load_result loaded = {0};
        game_rules_move_result rejected = {0};
        cells[0].elevation = 1;
        assert(engine != NULL);
        assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
               GAME_RULES_CALL_OK);
        assert(loaded.accepted && loaded.tick_count == 0U);
        game_rules_load_result_dispose(&loaded);
        assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST,
                                           &rejected) == GAME_RULES_CALL_OK);
        assert(!rejected.accepted &&
               rejected.status == GAME_RULES_MOVE_TELEPORTER_RESTRICTION);
        assert(rejected.tick_count == 0U);
        assert(rejected.final_state.outcome == GAME_RULES_OUTCOME_ONGOING);
        game_rules_move_result_dispose(&rejected);
        game_rules_engine_destroy(engine);
    }
}

static void test_exit_win_and_terminal_rejection(void)
{
    game_rules_cell cells[2];
    game_rules_fixture exit_fixture = {
        {1, 0}, GAME_RULES_FIXTURE_EXIT, GAME_RULES_COLOR_RED};
    game_rules_entity player = {
        1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0};
    game_rules_level_definition level = line_level(
        cells, 2U, &exit_fixture, 1U, &player, 1U);
    game_rules_engine* engine = game_rules_engine_create();
    game_rules_load_result loaded = {0};
    game_rules_move_result won = {0};
    game_rules_move_result rejected = {0};

    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) ==
           GAME_RULES_CALL_OK);
    assert(loaded.accepted && loaded.tick_count == 0U);
    game_rules_load_result_dispose(&loaded);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_EAST, &won) ==
           GAME_RULES_CALL_OK);
    assert(won.accepted && won.tick_count == 1U);
    assert(won.ticks[0].event_count == 2U);
    assert(won.ticks[0].events[0].kind == GAME_RULES_EVENT_ENTITY_MOVED);
    assert(won.ticks[0].events[1].kind == GAME_RULES_EVENT_LEVEL_WON);
    assert(won.final_state.outcome == GAME_RULES_OUTCOME_WON);
    assert(won.state.resolved.outcome == GAME_RULES_OUTCOME_WON);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST, &rejected) ==
           GAME_RULES_CALL_OK);
    assert(!rejected.accepted && rejected.status == GAME_RULES_MOVE_LEVEL_TERMINAL);
    assert(rejected.tick_count == 0U && rejected.event_count == 1U);
    assert(rejected.events[0].kind == GAME_RULES_EVENT_MOVE_BLOCKED);
    assert(rejected.final_state.outcome == GAME_RULES_OUTCOME_WON);
    assert(rejected.state.resolved.outcome == GAME_RULES_OUTCOME_WON);
    assert(won.state.resolved.entities[0].coordinate.x == 1);
    game_rules_move_result_dispose(&rejected);
    game_rules_move_result_dispose(&won);
    game_rules_engine_destroy(engine);
}

int main(void)
{
    test_canonical_all_colors_and_owned_views();
    test_simultaneous_switch_and_door_order();
    test_multitick_gravity_and_held_door();
    test_and_switches_door_closing_and_rejections();
    test_fixture_changes_precede_loss_and_exit_height_is_restricted();
    test_exit_win_and_terminal_rejection();
    return 0;
}
