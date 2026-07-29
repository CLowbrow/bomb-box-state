#include "game_rules/c_api.h"
#include "c_api_internal.h"

#include <assert.h>
#include <limits.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char minimal[] =
    "{\"format\":\"game-rules-level\",\"version\":1,"
    "\"coordinateSystem\":{\"origin\":{\"x\":0,\"y\":0},\"positiveX\":\"east\",\"positiveY\":\"north\"},"
    "\"width\":1,\"height\":1,"
    "\"cells\":[{\"coordinate\":{\"x\":0,\"y\":0},\"type\":\"flat\",\"elevation\":0}],"
    "\"fixtures\":[],\"entities\":[{\"id\":\"1\",\"type\":\"player\","
    "\"coordinate\":{\"x\":0,\"y\":0},\"bottomHalfSteps\":0}]}";

static void expect_contains(char* json, const char* fragment)
{
    assert(json != NULL);
    assert(strstr(json, fragment) != NULL);
    game_rules_string_free(json);
}

static void* test_allocate(void* context, size_t size)
{
    (void)context;
    return malloc(size);
}

static void test_deallocate(void* context, void* value)
{
    (void)context;
    free(value);
}

static game_rules_c_allocator allocator(void)
{
    game_rules_c_allocator value;
    value.context = NULL;
    value.allocate = test_allocate;
    value.deallocate = test_deallocate;
    return value;
}

static int has_code(const game_rules_c_level_view* level, uint32_t code)
{
    game_rules_c_validation_result result;
    game_rules_c_allocator memory = allocator();
    uint32_t i;
    int found = 0;
    game_rules_c_validate_level(level, &memory, &result);
    assert(!result.allocation_failed);
    for (i = 0U; i < result.count; ++i) if (result.errors[i].code == code) found = 1;
    game_rules_c_validation_result_destroy(&result);
    return found;
}

static game_rules_c_level_view base_view(game_rules_cell* cells,
                                         game_rules_fixture* fixtures,
                                         uint32_t fixture_count,
                                         game_rules_entity* entities,
                                         uint32_t entity_count)
{
    game_rules_c_level_view level;
    memset(&level, 0, sizeof(level));
    level.coordinates.positive_x = GAME_RULES_HORIZONTAL_EAST;
    level.coordinates.positive_y = GAME_RULES_VERTICAL_NORTH;
    level.width = 2U;
    level.height = 1U;
    level.cells = cells;
    level.cell_count = 2U;
    level.fixtures = fixtures;
    level.fixture_count = fixture_count;
    level.entities = entities;
    level.entity_count = entity_count;
    return level;
}

static void test_json_format_errors(void)
{
    game_rules_engine* engine = game_rules_engine_create();
    static const char* const inputs[] = {
        "[", "[]", "{}",
        "{\"format\":\"game-rules-level\",\"format\":\"game-rules-level\",\"version\":1,\"coordinateSystem\":{},\"width\":1,\"height\":1,\"cells\":[],\"fixtures\":[],\"entities\":[]}",
        "{\"format\":\"wrong\",\"version\":1,\"coordinateSystem\":{},\"width\":1,\"height\":1,\"cells\":[],\"fixtures\":[],\"entities\":[]}",
        "{\"format\":\"game-rules-level\",\"version\":2,\"coordinateSystem\":{},\"width\":1,\"height\":1,\"cells\":[],\"fixtures\":[],\"entities\":[]}",
        "{\"format\":\"game-rules-level\",\"version\":1.0,\"coordinateSystem\":{},\"width\":1,\"height\":1,\"cells\":[],\"fixtures\":[],\"entities\":[]}",
        "[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[[]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]]"
    };
    static const char* const codes[] = {
        "invalid_json", "root_not_object", "missing_member", "duplicate_member",
        "invalid_format", "unsupported_version", "integer_out_of_range", "nesting_too_deep"
    };
    size_t i;
    assert(engine != NULL);
    for (i = 0U; i < sizeof(inputs) / sizeof(inputs[0]); ++i) {
        char fragment[96];
        char* response = game_rules_engine_load_level(engine, inputs[i], (uint32_t)strlen(inputs[i]));
        assert(response != NULL);
        strcpy(fragment, "\"code\":\"");
        strcat(fragment, codes[i]);
        strcat(fragment, "\"");
        assert(strstr(response, fragment) != NULL);
        game_rules_string_free(response);
    }
    {
        const char with_nul[] = {'{', '}', '\0', '{', '}'};
        expect_contains(game_rules_engine_load_level(engine, with_nul, sizeof(with_nul)),
                        "\"code\":\"invalid_json\"");
    }
    {
        const char bad_utf8[] = {'{', '"', 'x', '"', ':', '"', (char)0xc3, '"', '}'};
        expect_contains(game_rules_engine_load_level(engine, bad_utf8, sizeof(bad_utf8)),
                        "\"code\":\"invalid_json\"");
    }
    {
        const size_t length = 16U * 1024U * 1024U + 1U;
        char* oversized = (char*)malloc(length);
        assert(oversized != NULL);
        memset(oversized, ' ', length);
        expect_contains(game_rules_engine_load_level(engine, oversized, (uint32_t)length),
                        "\"code\":\"document_too_large\"");
        free(oversized);
    }
    expect_contains(game_rules_engine_load_level(engine, minimal, (uint32_t)strlen(minimal)),
                    "\"status\":\"loaded\"");
    game_rules_engine_destroy(engine);
}

static void test_typed_boundary_and_ownership(void)
{
    game_rules_cell cells[2] = {
        {{1, 0}, GAME_RULES_CELL_FLAT, 0, 99U},
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, 99U}};
    game_rules_fixture fixtures[1] = {{{0, 0}, GAME_RULES_FIXTURE_EXIT, 99U}};
    game_rules_entity entities[1] = {{UINT64_MAX, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0}};
    game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        2U, 1U, cells, 2U, fixtures, 1U, entities, 1U};
    game_rules_engine* engine = game_rules_engine_create();
    game_rules_load_result loaded = {0};
    game_rules_state_result state = {0};
    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) == GAME_RULES_CALL_OK);
    assert(loaded.accepted && loaded.status == GAME_RULES_LOAD_LOADED);
    assert(loaded.tick_count == 1U && loaded.ticks[0].event_count == 1U);
    assert(loaded.ticks[0].events[0].kind == GAME_RULES_EVENT_LEVEL_WON);
    assert(loaded.state.level.cells[0].coordinate.x == 0);
    cells[1].coordinate.x = 77;
    entities[0].id = 7U;
    assert(loaded.state.level.cells[0].coordinate.x == 0);
    assert(loaded.state.resolved.entities[0].id == UINT64_MAX);
    game_rules_engine_destroy(engine);
    assert(loaded.state.resolved.outcome == GAME_RULES_OUTCOME_WON);
    game_rules_load_result_dispose(&loaded);

    engine = game_rules_engine_create();
    level.cells = NULL;
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) == GAME_RULES_CALL_INVALID_ARGUMENT);
    level.cells = cells;
    cells[0].kind = 99U;
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) == GAME_RULES_CALL_INVALID_ARGUMENT);
    cells[0].kind = GAME_RULES_CELL_RAMP;
    cells[0].low_direction = 99U;
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) == GAME_RULES_CALL_INVALID_ARGUMENT);
    cells[0].kind = GAME_RULES_CELL_FLAT;
    fixtures[0].kind = 99U;
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) == GAME_RULES_CALL_INVALID_ARGUMENT);
    fixtures[0].kind = GAME_RULES_FIXTURE_SWITCH;
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) == GAME_RULES_CALL_INVALID_ARGUMENT);
    fixtures[0].kind = GAME_RULES_FIXTURE_EXIT;
    entities[0].kind = 99U;
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) == GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(game_rules_engine_get_state_data(engine, &state) == GAME_RULES_CALL_OK && !state.has_state);
    game_rules_state_result_dispose(&state);
    game_rules_engine_destroy(engine);
}

static void test_all_validation_codes(void)
{
    game_rules_cell cells[4] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{3, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
    game_rules_fixture fixtures[2] = {{{1, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED},
                                      {{1, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_RED}};
    game_rules_entity entities[3] = {{1, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
                                     {2, GAME_RULES_ENTITY_BOX, {0, 0}, 1},
                                     {1, GAME_RULES_ENTITY_BOX, {3, 0}, 0}};
    game_rules_c_level_view level = base_view(cells, fixtures, 2U, entities, 3U);
    level.width = 0U;
    level.coordinates.positive_x = 99U;
    cells[0].coordinate.x = 8;
    cells[1].coordinate.x = 2;
    cells[0].elevation = INT32_MIN;
    fixtures[0].coordinate.x = 8;
    fixtures[1].color = 99U;
    entities[2].coordinate.x = 8;
    entities[0].id = 0U;
    entities[0].kind = 99U;
    assert(has_code(&level, GAME_RULES_VALIDATION_INVALID_DIMENSIONS));
    assert(has_code(&level, GAME_RULES_VALIDATION_INVALID_COORDINATE_SYSTEM));
    assert(has_code(&level, GAME_RULES_VALIDATION_CELL_COUNT_MISMATCH));
    assert(has_code(&level, GAME_RULES_VALIDATION_CELL_OUT_OF_BOUNDS));
    assert(has_code(&level, GAME_RULES_VALIDATION_FIXTURE_OUT_OF_BOUNDS));
    assert(has_code(&level, GAME_RULES_VALIDATION_ENTITY_OUT_OF_BOUNDS));
    assert(has_code(&level, GAME_RULES_VALIDATION_INVALID_ENTITY_KIND));
    assert(has_code(&level, GAME_RULES_VALIDATION_INVALID_ENTITY_ID));

    cells[0] = (game_rules_cell){{0, 0}, GAME_RULES_CELL_FLAT, 1, 0};
    cells[1] = (game_rules_cell){{0, 0}, GAME_RULES_CELL_FLAT, 0, 0};
    entities[0] = (game_rules_entity){1, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0};
    entities[1] = (game_rules_entity){2, GAME_RULES_ENTITY_BOX, {0, 0}, 1};
    entities[2] = (game_rules_entity){1, GAME_RULES_ENTITY_BOX, {1, 0}, 0};
    fixtures[0] = (game_rules_fixture){{0, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED};
    fixtures[1] = (game_rules_fixture){{0, 0}, GAME_RULES_FIXTURE_DOOR, GAME_RULES_COLOR_RED};
    level = base_view(cells, fixtures, 2U, entities, 3U);
    cells[0].elevation = INT32_MIN;
    assert(has_code(&level, GAME_RULES_VALIDATION_INVALID_CELL_HEIGHT));
    cells[0].elevation = 1;
    fixtures[1].color = 99U;
    assert(has_code(&level, GAME_RULES_VALIDATION_INVALID_FIXTURE_COLOR));
    fixtures[1].color = GAME_RULES_COLOR_RED;
    assert(has_code(&level, GAME_RULES_VALIDATION_DUPLICATE_CELL));
    assert(has_code(&level, GAME_RULES_VALIDATION_DUPLICATE_FIXTURE));
    assert(has_code(&level, GAME_RULES_VALIDATION_DUPLICATE_ENTITY_ID));
    assert(has_code(&level, GAME_RULES_VALIDATION_ENTITY_BELOW_SURFACE));
    assert(has_code(&level, GAME_RULES_VALIDATION_OVERLAPPING_ENTITIES));
    assert(has_code(&level, GAME_RULES_VALIDATION_PLAYER_NOT_TOP_OF_STACK));

    entities[0].kind = GAME_RULES_ENTITY_BOX;
    assert(has_code(&level, GAME_RULES_VALIDATION_PLAYER_COUNT_NOT_ONE));
    entities[0].kind = GAME_RULES_ENTITY_PLAYER;
    fixtures[0] = (game_rules_fixture){{0, 0}, GAME_RULES_FIXTURE_EXIT, 0};
    level.fixture_count = 1U;
    assert(has_code(&level, GAME_RULES_VALIDATION_INVALID_TELEPORTER_OCCUPANCY));

    cells[0] = (game_rules_cell){{0, 0}, GAME_RULES_CELL_FLAT, 0, 0};
    cells[1] = (game_rules_cell){{1, 0}, GAME_RULES_CELL_RAMP, 0, 99U};
    level = base_view(cells, NULL, 0U, entities, 1U);
    assert(has_code(&level, GAME_RULES_VALIDATION_INVALID_RAMP_DIRECTION));
    cells[1].low_direction = GAME_RULES_DIRECTION_WEST;
    assert(has_code(&level, GAME_RULES_VALIDATION_INVALID_RAMP_ENDPOINTS));
    fixtures[0] = (game_rules_fixture){{1, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED};
    level.fixtures = fixtures;
    level.fixture_count = 1U;
    assert(has_code(&level, GAME_RULES_VALIDATION_FIXTURE_ON_RAMP));
}

int main(void)
{
    test_json_format_errors();
    test_typed_boundary_and_ownership();
    test_all_validation_codes();
    return 0;
}
