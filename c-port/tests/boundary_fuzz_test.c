#include "game_rules/c_api.h"

#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static const char valid_level[] =
    "{\"format\":\"game-rules-level\",\"version\":1,"
    "\"coordinateSystem\":{\"origin\":{\"x\":0,\"y\":0},"
    "\"positiveX\":\"east\",\"positiveY\":\"north\"},"
    "\"width\":1,\"height\":1,"
    "\"cells\":[{\"coordinate\":{\"x\":0,\"y\":0},"
    "\"type\":\"flat\",\"elevation\":0}],\"fixtures\":[],"
    "\"entities\":[{\"id\":\"1\",\"type\":\"player\","
    "\"coordinate\":{\"x\":0,\"y\":0},\"bottomHalfSteps\":0}]}";

static void assert_state_unchanged(game_rules_engine* engine, const char* expected)
{
    char* const state = game_rules_engine_get_state(engine);
    assert(state != NULL);
    assert(strcmp(state, expected) == 0);
    game_rules_string_free(state);
}

static void test_truncation_utf8_nesting_nul_and_integer_boundaries(void)
{
    static const char* const numeric_cases[] = {
        "{\"format\":\"game-rules-level\",\"version\":4294967296}",
        "{\"format\":\"game-rules-level\",\"version\":-2147483649}",
        "{\"format\":\"game-rules-level\",\"version\":1e0}",
        "{\"format\":\"game-rules-level\",\"version\":1.0}",
        "{\"format\":\"game-rules-level\",\"version\":18446744073709551616}",
    };
    game_rules_engine* const engine = game_rules_engine_create();
    char* loaded;
    char* baseline;
    size_t index;
    assert(engine != NULL);
    loaded = game_rules_engine_load_level(engine, valid_level, (uint32_t)strlen(valid_level));
    assert(loaded != NULL && strstr(loaded, "\"status\":\"loaded\"") != NULL);
    game_rules_string_free(loaded);
    baseline = game_rules_engine_get_state(engine);
    assert(baseline != NULL);

    for (index = 0U; index < strlen(valid_level); ++index) {
        char* const result = game_rules_engine_load_level(engine, valid_level, (uint32_t)index);
        assert(result != NULL);
        assert(strstr(result, "\"status\":\"invalid_json\"") != NULL);
        game_rules_string_free(result);
        assert_state_unchanged(engine, baseline);
    }
    for (index = 0U; index < sizeof(numeric_cases) / sizeof(numeric_cases[0]); ++index) {
        char* const result = game_rules_engine_load_level(
            engine, numeric_cases[index], (uint32_t)strlen(numeric_cases[index]));
        assert(result != NULL);
        game_rules_string_free(result);
        assert_state_unchanged(engine, baseline);
    }
    {
        const char embedded_nul[] = {'{', '}', '\0', '{', '}'};
        const char malformed_utf8_1[] = {'{', '"', 'x', '"', ':', '"', (char)0xc0,
                                         (char)0xaf, '"', '}'};
        const char malformed_utf8_2[] = {'{', '"', 'x', '"', ':', '"', (char)0xed,
                                         (char)0xa0, (char)0x80, '"', '}'};
        const char malformed_utf8_3[] = {'{', '"', 'x', '"', ':', '"', (char)0xf4,
                                         (char)0x90, (char)0x80, (char)0x80, '"', '}'};
        const struct {
            const char* bytes;
            uint32_t count;
        } cases[] = {
            {embedded_nul, (uint32_t)sizeof(embedded_nul)},
            {malformed_utf8_1, (uint32_t)sizeof(malformed_utf8_1)},
            {malformed_utf8_2, (uint32_t)sizeof(malformed_utf8_2)},
            {malformed_utf8_3, (uint32_t)sizeof(malformed_utf8_3)},
        };
        for (index = 0U; index < sizeof(cases) / sizeof(cases[0]); ++index) {
            char* const result = game_rules_engine_load_level(
                engine, cases[index].bytes, cases[index].count);
            assert(result != NULL);
            assert(strstr(result, "\"code\":\"invalid_json\"") != NULL);
            game_rules_string_free(result);
            assert_state_unchanged(engine, baseline);
        }
    }
    {
        char nested[96];
        size_t length = 0U;
        for (index = 0U; index < 40U; ++index) nested[length++] = '[';
        for (index = 0U; index < 40U; ++index) nested[length++] = ']';
        loaded = game_rules_engine_load_level(engine, nested, (uint32_t)length);
        assert(loaded != NULL);
        assert(strstr(loaded, "\"code\":\"nesting_too_deep\"") != NULL);
        game_rules_string_free(loaded);
        assert_state_unchanged(engine, baseline);
    }
    game_rules_string_free(baseline);
    game_rules_engine_destroy(engine);
}

static uint32_t next_random(uint32_t* state)
{
    *state = *state * UINT32_C(1664525) + UINT32_C(1013904223);
    return *state;
}

static void test_seeded_parser_bytes(void)
{
    uint32_t seed = UINT32_C(0xC17F022);
    uint32_t iteration;
    for (iteration = 0U; iteration < 4096U; ++iteration) {
        unsigned char input[257];
        const uint32_t length = next_random(&seed) % 257U;
        uint32_t index;
        game_rules_engine* const engine = game_rules_engine_create();
        char* result;
        assert(engine != NULL);
        for (index = 0U; index < length; ++index) {
            input[index] = (unsigned char)(next_random(&seed) >> 24U);
        }
        result = game_rules_engine_load_level(engine, (const char*)input, length);
        assert(result != NULL);
        game_rules_string_free(result);
        game_rules_engine_destroy(engine);
    }
}

static void test_typed_tags_counts_nulls_and_result_backed_input(void)
{
    game_rules_cell cell = {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0};
    game_rules_fixture fixture = {{0, 0}, GAME_RULES_FIXTURE_SWITCH, GAME_RULES_COLOR_RED};
    game_rules_entity player = {1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0};
    game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        1U, 1U, &cell, 1U, NULL, 0U, &player, 1U};
    game_rules_engine* source = game_rules_engine_create();
    game_rules_engine* destination = game_rules_engine_create();
    game_rules_load_result loaded = {0};
    game_rules_load_result copied = {0};
    game_rules_state_result state = {0};
    game_rules_move_result move = {0};
    game_rules_rewind_result rewind = {0};
    union {
        game_rules_level_definition level;
        game_rules_load_result result;
    } aliased = {0};
    assert(source != NULL && destination != NULL);

    assert(game_rules_engine_get_state_data(NULL, NULL) == GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(game_rules_engine_move_data(NULL, 0U, NULL) == GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(game_rules_engine_rewind_data(NULL, NULL) == GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(game_rules_engine_load_level_data(NULL, NULL, NULL) == GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(game_rules_engine_get_state_data(source, NULL) == GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(game_rules_engine_move_data(source, 0U, NULL) == GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(game_rules_engine_rewind_data(source, NULL) == GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(game_rules_engine_load_level_data(source, NULL, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(game_rules_engine_load_level_data(source, &level, NULL) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    assert(game_rules_engine_get_state_data(NULL, &state) == GAME_RULES_CALL_INVALID_ENGINE);
    assert(game_rules_engine_move_data(NULL, 0U, &move) == GAME_RULES_CALL_INVALID_ENGINE);
    assert(game_rules_engine_rewind_data(NULL, &rewind) == GAME_RULES_CALL_INVALID_ENGINE);

    level.cells = NULL;
    assert(game_rules_engine_load_level_data(source, &level, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    level.cells = &cell;
    level.entities = NULL;
    assert(game_rules_engine_load_level_data(source, &level, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    level.entities = &player;
    level.cell_count = UINT32_MAX;
    level.cells = NULL;
    assert(game_rules_engine_load_level_data(source, &level, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    level.cell_count = 1U;
    level.cells = &cell;

    level.coordinates.positive_x = UINT32_MAX;
    assert(game_rules_engine_load_level_data(source, &level, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    level.coordinates.positive_x = GAME_RULES_HORIZONTAL_EAST;
    level.coordinates.positive_y = UINT32_MAX;
    assert(game_rules_engine_load_level_data(source, &level, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    level.coordinates.positive_y = GAME_RULES_VERTICAL_NORTH;
    cell.kind = UINT32_MAX;
    assert(game_rules_engine_load_level_data(source, &level, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    cell.kind = GAME_RULES_CELL_RAMP;
    cell.low_direction = UINT32_MAX;
    assert(game_rules_engine_load_level_data(source, &level, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    cell.kind = GAME_RULES_CELL_FLAT;
    level.fixtures = &fixture;
    level.fixture_count = 1U;
    fixture.kind = UINT32_MAX;
    assert(game_rules_engine_load_level_data(source, &level, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    fixture.kind = GAME_RULES_FIXTURE_SWITCH;
    fixture.color = UINT32_MAX;
    assert(game_rules_engine_load_level_data(source, &level, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    fixture.color = GAME_RULES_COLOR_RED;
    level.fixtures = NULL;
    level.fixture_count = 0U;
    player.kind = UINT32_MAX;
    assert(game_rules_engine_load_level_data(source, &level, &loaded) ==
           GAME_RULES_CALL_INVALID_ARGUMENT);
    player.kind = GAME_RULES_ENTITY_PLAYER;

    assert(game_rules_engine_load_level_data(source, &level, &loaded) == GAME_RULES_CALL_OK);
    assert(loaded.accepted != 0U);
    assert(game_rules_engine_move_data(source, UINT32_MAX, &move) == GAME_RULES_CALL_OK);
    assert(move.accepted == 0U && move.status == GAME_RULES_MOVE_INVALID_DIRECTION);
    game_rules_move_result_dispose(&move);

    /* Overlapping input/output storage must remain bounded and preserve the loaded session. */
    aliased.level = level;
    assert(game_rules_engine_load_level_data(source, &aliased.level, &aliased.result) ==
           GAME_RULES_CALL_OK);
    assert(aliased.result.accepted == 0U && aliased.result.error_count != 0U);
    game_rules_load_result_dispose(&aliased.result);
    assert(game_rules_engine_get_state_data(source, &state) == GAME_RULES_CALL_OK);
    assert(state.has_state != 0U && state.state.resolved.entities[0].id == 1U);
    game_rules_state_result_dispose(&state);

    level.coordinates = loaded.state.level.coordinates;
    level.width = loaded.state.level.width;
    level.height = loaded.state.level.height;
    level.cells = loaded.state.level.cells;
    level.cell_count = loaded.state.level.cell_count;
    level.fixtures = loaded.state.level.fixtures;
    level.fixture_count = loaded.state.level.fixture_count;
    level.entities = loaded.state.resolved.entities;
    level.entity_count = loaded.state.resolved.entity_count;
    assert(game_rules_engine_load_level_data(destination, &level, &copied) == GAME_RULES_CALL_OK);
    assert(copied.accepted != 0U);
    game_rules_load_result_dispose(&loaded);
    assert(game_rules_engine_get_state_data(destination, &state) == GAME_RULES_CALL_OK);
    assert(state.has_state != 0U && state.state.resolved.entities[0].id == 1U);

    game_rules_state_result_dispose(&state);
    game_rules_load_result_dispose(&copied);
    game_rules_load_result_dispose(&loaded);
    game_rules_move_result_dispose(&move);
    game_rules_rewind_result_dispose(&rewind);
    game_rules_engine_destroy(destination);
    game_rules_engine_destroy(source);
}

int main(void)
{
    test_truncation_utf8_nesting_nul_and_integer_boundaries();
    test_seeded_parser_bytes();
    test_typed_tags_counts_nulls_and_result_backed_input();
    return 0;
}
