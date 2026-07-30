#include "game_rules/c_api.h"

#include "c_api_internal.h"

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

static int32_t player_x(const game_rules_resolved_state* state, uint64_t id)
{
    const game_rules_entity* const player = find_entity(state, id);
    assert(player != NULL);
    return player->coordinate.x;
}

static void expect_move(game_rules_engine* engine,
                        uint32_t direction,
                        int32_t expected_x,
                        game_rules_move_result* result)
{
    assert(game_rules_engine_move_data(engine, direction, result) == GAME_RULES_CALL_OK);
    assert(result->accepted == 1U && result->status == GAME_RULES_MOVE_MOVED);
    assert(result->tick_count >= 1U && result->has_state == 1U);
    assert(player_x(&result->state.resolved, 1U) == expected_x);
}

static void expect_rewind(game_rules_engine* engine,
                          int32_t expected_x,
                          game_rules_rewind_result* result)
{
    assert(game_rules_engine_rewind_data(engine, result) == GAME_RULES_CALL_OK);
    assert(result->status == GAME_RULES_REWIND_REWOUND && result->accepted == 1U);
    assert(result->event_count == 1U && result->events != NULL);
    assert(result->events[0].kind == GAME_RULES_EVENT_STATE_REWOUND);
    assert(result->has_restored_state == 1U && result->has_state == 1U);
    assert(player_x(&result->restored_state, 1U) == expected_x);
    assert(player_x(&result->state.resolved, 1U) == expected_x);
}

static void test_repeated_terminal_rewind_branch_and_lifetimes(void)
{
    static const game_rules_cell cells[4] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{3, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
    static const game_rules_fixture exit_fixture = {
        {3, 0}, GAME_RULES_FIXTURE_EXIT, 0U};
    static const game_rules_entity player = {
        1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0};
    static const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        4U, 1U, cells, 4U, &exit_fixture, 1U, &player, 1U};
    game_rules_engine* engine = game_rules_engine_create();
    game_rules_state_result before_move = {0};
    game_rules_load_result load = {0};
    game_rules_move_result first = {0};
    game_rules_move_result rejected = {0};
    game_rules_move_result second = {0};
    game_rules_move_result won = {0};
    game_rules_move_result blocked = {0};
    game_rules_move_result branch = {0};
    game_rules_rewind_result rewind_won = {0};
    game_rules_rewind_result rewind_second = {0};
    game_rules_rewind_result rewind_branch = {0};
    game_rules_rewind_result rewind_root = {0};
    game_rules_rewind_result empty = {0};

    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &load) == GAME_RULES_CALL_OK);
    assert(load.accepted == 1U && engine->session->history_count == 0U);
    assert(game_rules_engine_get_state_data(engine, &before_move) == GAME_RULES_CALL_OK);
    assert(before_move.has_state == 1U && player_x(&before_move.state.resolved, 1U) == 0);
    expect_move(engine, GAME_RULES_DIRECTION_EAST, 1, &first);
    assert(engine->session->history_count == 1U);
    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_NORTH, &rejected) ==
           GAME_RULES_CALL_OK);
    assert(rejected.accepted == 0U && rejected.status == GAME_RULES_MOVE_WORLD_BOUNDARY);
    assert(engine->session->history_count == 1U);
    expect_move(engine, GAME_RULES_DIRECTION_EAST, 2, &second);
    assert(engine->session->history_count == 2U);
    expect_move(engine, GAME_RULES_DIRECTION_EAST, 3, &won);
    assert(won.outcome == GAME_RULES_OUTCOME_WON && engine->session->history_count == 3U);

    assert(game_rules_engine_move_data(engine, GAME_RULES_DIRECTION_WEST, &blocked) ==
           GAME_RULES_CALL_OK);
    assert(blocked.status == GAME_RULES_MOVE_LEVEL_TERMINAL && blocked.accepted == 0U);
    assert(engine->session->history_count == 3U);

    expect_rewind(engine, 2, &rewind_won);
    assert(rewind_won.outcome == GAME_RULES_OUTCOME_ONGOING);
    expect_rewind(engine, 1, &rewind_second);
    assert(engine->session->history_count == 1U);

    expect_move(engine, GAME_RULES_DIRECTION_WEST, 0, &branch);
    assert(engine->session->history_count == 2U);
    expect_rewind(engine, 1, &rewind_branch);
    expect_rewind(engine, 0, &rewind_root);
    assert(engine->session->history_count == 0U);

    assert(game_rules_engine_rewind_data(engine, &empty) == GAME_RULES_CALL_OK);
    assert(empty.status == GAME_RULES_REWIND_HISTORY_EMPTY && empty.accepted == 0U);
    assert(empty.event_count == 0U && empty.has_restored_state == 0U);
    assert(empty.has_state == 1U && player_x(&empty.state.resolved, 1U) == 0);

    game_rules_engine_destroy(engine);
    /* Every earlier immutable result remains independent after destruction. */
    assert(player_x(&load.state.resolved, 1U) == 0);
    assert(player_x(&before_move.state.resolved, 1U) == 0);
    assert(player_x(&first.initial_state, 1U) == 0);
    assert(player_x(&won.state.resolved, 1U) == 3);
    assert(player_x(&rewind_won.state.resolved, 1U) == 2);
    assert(player_x(&rewind_branch.state.resolved, 1U) == 1);

    game_rules_rewind_result_dispose(&empty);
    game_rules_rewind_result_dispose(&rewind_root);
    game_rules_rewind_result_dispose(&rewind_branch);
    game_rules_rewind_result_dispose(&rewind_second);
    game_rules_rewind_result_dispose(&rewind_won);
    game_rules_move_result_dispose(&branch);
    game_rules_move_result_dispose(&blocked);
    game_rules_move_result_dispose(&won);
    game_rules_move_result_dispose(&second);
    game_rules_move_result_dispose(&rejected);
    game_rules_move_result_dispose(&first);
    game_rules_load_result_dispose(&load);
    game_rules_state_result_dispose(&before_move);
}

static void test_replacement_preserves_or_discards_history_atomically(void)
{
    static const game_rules_cell cells[3] = {
        {{0, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{1, 0}, GAME_RULES_CELL_FLAT, 0, 0},
        {{2, 0}, GAME_RULES_CELL_FLAT, 0, 0}};
    static const game_rules_entity player = {
        1U, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0};
    static const game_rules_entity replacement_player = {
        99U, GAME_RULES_ENTITY_PLAYER, {2, 0}, 0};
    static const game_rules_level_definition level = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        3U, 1U, cells, 3U, NULL, 0U, &player, 1U};
    static const game_rules_level_definition replacement = {
        {{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
        3U, 1U, cells, 3U, NULL, 0U, &replacement_player, 1U};
    game_rules_level_definition invalid = level;
    game_rules_engine* engine = game_rules_engine_create();
    game_rules_load_result loaded = {0};
    game_rules_load_result rejected = {0};
    game_rules_load_result replaced = {0};
    game_rules_move_result moved = {0};
    game_rules_move_result moved_again = {0};
    game_rules_rewind_result rewind = {0};
    game_rules_state_result before_replacement = {0};

    assert(engine != NULL);
    assert(game_rules_engine_load_level_data(engine, &level, &loaded) == GAME_RULES_CALL_OK);
    expect_move(engine, GAME_RULES_DIRECTION_EAST, 1, &moved);
    assert(engine->session->history_count == 1U);
    assert(game_rules_engine_get_state_data(engine, &before_replacement) ==
           GAME_RULES_CALL_OK);

    invalid.entity_count = 0U;
    invalid.entities = NULL;
    assert(game_rules_engine_load_level_data(engine, &invalid, &rejected) ==
           GAME_RULES_CALL_OK);
    assert(rejected.accepted == 0U && engine->session->history_count == 1U);
    expect_rewind(engine, 0, &rewind);
    game_rules_rewind_result_dispose(&rewind);
    expect_move(engine, GAME_RULES_DIRECTION_EAST, 1, &moved_again);
    assert(engine->session->history_count == 1U);

    assert(game_rules_engine_load_level_data(engine, &replacement, &replaced) ==
           GAME_RULES_CALL_OK);
    assert(replaced.accepted == 1U && engine->session->history_count == 0U);
    assert(find_entity(&replaced.state.resolved, 99U) != NULL);
    assert(game_rules_engine_rewind_data(engine, &rewind) == GAME_RULES_CALL_OK);
    assert(rewind.status == GAME_RULES_REWIND_HISTORY_EMPTY && rewind.has_state == 1U);
    assert(find_entity(&rewind.state.resolved, 99U) != NULL);

    game_rules_engine_destroy(engine);
    assert(player_x(&moved.state.resolved, 1U) == 1);
    assert(player_x(&before_replacement.state.resolved, 1U) == 1);
    assert(find_entity(&replaced.state.resolved, 99U) != NULL);
    game_rules_rewind_result_dispose(&rewind);
    game_rules_move_result_dispose(&moved_again);
    game_rules_move_result_dispose(&moved);
    game_rules_load_result_dispose(&replaced);
    game_rules_load_result_dispose(&rejected);
    game_rules_load_result_dispose(&loaded);
    game_rules_state_result_dispose(&before_replacement);
}

static void test_rewind_from_lost_state(void)
{
    static const char loss_level[] =
        "{\"format\":\"game-rules-level\",\"version\":1,"
        "\"coordinateSystem\":{\"origin\":{\"x\":0,\"y\":0},"
        "\"positiveX\":\"east\",\"positiveY\":\"north\"},"
        "\"width\":3,\"height\":1,\"cells\":["
        "{\"coordinate\":{\"x\":0,\"y\":0},\"type\":\"flat\",\"elevation\":1},"
        "{\"coordinate\":{\"x\":1,\"y\":0},\"type\":\"flat\",\"elevation\":0},"
        "{\"coordinate\":{\"x\":2,\"y\":0},\"type\":\"flat\",\"elevation\":0}],"
        "\"fixtures\":[],\"entities\":["
        "{\"id\":\"1\",\"type\":\"player\",\"coordinate\":{\"x\":0,\"y\":0},"
        "\"bottomHalfSteps\":2},"
        "{\"id\":\"9\",\"type\":\"barrel\",\"coordinate\":{\"x\":1,\"y\":0},"
        "\"bottomHalfSteps\":0},"
        "{\"id\":\"8\",\"type\":\"barrel\",\"coordinate\":{\"x\":1,\"y\":0},"
        "\"bottomHalfSteps\":2}]}";
    game_rules_engine* engine = game_rules_engine_create();
    char* loaded;
    char* lost;
    char* rewound;

    assert(engine != NULL);
    loaded = game_rules_engine_load_level(engine, loss_level,
                                          (uint32_t)strlen(loss_level));
    assert(loaded != NULL && strstr(loaded, "\"status\":\"loaded\"") != NULL);
    lost = game_rules_engine_move(engine, GAME_RULES_DIRECTION_EAST);
    assert(lost != NULL && strstr(lost, "\"outcome\":\"lost\"") != NULL);
    rewound = game_rules_engine_rewind(engine);
    assert(rewound != NULL && strstr(rewound, "\"status\":\"rewound\"") != NULL);
    assert(strstr(rewound, "\"outcome\":\"ongoing\"") != NULL);

    game_rules_engine_destroy(engine);
    assert(strstr(lost, "\"outcome\":\"lost\"") != NULL);
    game_rules_string_free(rewound);
    game_rules_string_free(lost);
    game_rules_string_free(loaded);
}

int main(void)
{
    test_repeated_terminal_rewind_branch_and_lifetimes();
    test_replacement_preserves_or_discards_history_atomically();
    test_rewind_from_lost_state();
    return 0;
}
