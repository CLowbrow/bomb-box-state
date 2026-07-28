#include "game_rules/c_api.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>
#include <memory>

namespace {

struct EngineDeleter final {
    void operator()(game_rules_engine* engine) const { game_rules_engine_destroy(engine); }
};

using EnginePointer = std::unique_ptr<game_rules_engine, EngineDeleter>;

constexpr game_rules_cell flat_cell(const std::int32_t x,
                                    const std::int32_t y,
                                    const std::int32_t elevation = 0)
{
    return {{x, y}, GAME_RULES_CELL_FLAT, elevation, GAME_RULES_DIRECTION_NORTH};
}

template <std::size_t CellCount, std::size_t FixtureCount, std::size_t EntityCount>
constexpr game_rules_level_definition level_from(
    const game_rules_cell (&cells)[CellCount],
    const game_rules_fixture (&fixtures)[FixtureCount],
    const game_rules_entity (&entities)[EntityCount],
    const std::uint32_t width,
    const std::uint32_t height)
{
    return {{{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
            width,
            height,
            cells,
            static_cast<std::uint32_t>(CellCount),
            fixtures,
            static_cast<std::uint32_t>(FixtureCount),
            entities,
            static_cast<std::uint32_t>(EntityCount)};
}

template <std::size_t CellCount, std::size_t EntityCount>
constexpr game_rules_level_definition level_without_fixtures(
    const game_rules_cell (&cells)[CellCount],
    const game_rules_entity (&entities)[EntityCount],
    const std::uint32_t width,
    const std::uint32_t height)
{
    return {{{0, 0}, GAME_RULES_HORIZONTAL_EAST, GAME_RULES_VERTICAL_NORTH},
            width,
            height,
            cells,
            static_cast<std::uint32_t>(CellCount),
            nullptr,
            0,
            entities,
            static_cast<std::uint32_t>(EntityCount)};
}

TEST(CDataApi, ReportsNoLevelAndRejectsInvalidBoundaryArguments)
{
    EXPECT_EQ(game_rules_data_api_version(), 1U);
    EnginePointer engine{game_rules_engine_create()};
    ASSERT_NE(engine, nullptr);

    game_rules_state_result state{};
    EXPECT_EQ(game_rules_engine_get_state_data(engine.get(), &state), GAME_RULES_CALL_OK);
    EXPECT_EQ(state.has_state, 0U);
    EXPECT_NE(state.owned_storage, nullptr);
    game_rules_state_result_dispose(&state);
    EXPECT_EQ(state.owned_storage, nullptr);

    state.has_state = 99U;
    EXPECT_EQ(game_rules_engine_get_state_data(nullptr, &state),
              GAME_RULES_CALL_INVALID_ENGINE);
    EXPECT_EQ(state.has_state, 0U);
    EXPECT_EQ(state.owned_storage, nullptr);
    EXPECT_EQ(game_rules_engine_get_state_data(engine.get(), nullptr),
              GAME_RULES_CALL_INVALID_ARGUMENT);

    game_rules_load_result loaded{};
    EXPECT_EQ(game_rules_engine_load_level_data(engine.get(), nullptr, &loaded),
              GAME_RULES_CALL_INVALID_ARGUMENT);
    EXPECT_EQ(game_rules_engine_load_level_data(nullptr, nullptr, nullptr),
              GAME_RULES_CALL_INVALID_ARGUMENT);

    game_rules_state_result_dispose(nullptr);
    game_rules_load_result_dispose(nullptr);
    game_rules_move_result_dispose(nullptr);
    game_rules_rewind_result_dispose(nullptr);
}

TEST(CDataApi, LoadsTypedLevelAndReturnsOwnedCanonicalSnapshot)
{
    game_rules_cell cells[] = {flat_cell(1, 0), flat_cell(0, 0)};
    game_rules_fixture fixtures[] = {
        {{0, 0}, GAME_RULES_FIXTURE_EXIT, GAME_RULES_COLOR_YELLOW}};
    game_rules_entity entities[] = {
        {std::numeric_limits<std::uint64_t>::max(), GAME_RULES_ENTITY_PLAYER, {0, 0}, 0}};
    const game_rules_level_definition level = level_from(cells, fixtures, entities, 2, 1);

    EnginePointer engine{game_rules_engine_create()};
    ASSERT_NE(engine, nullptr);
    game_rules_load_result loaded{};
    ASSERT_EQ(game_rules_engine_load_level_data(engine.get(), &level, &loaded),
              GAME_RULES_CALL_OK);
    ASSERT_EQ(loaded.status, GAME_RULES_LOAD_LOADED);
    EXPECT_EQ(loaded.accepted, 1U);
    EXPECT_EQ(loaded.error_count, 0U);
    EXPECT_EQ(loaded.errors, nullptr);
    ASSERT_EQ(loaded.has_initial_state, 1U);
    ASSERT_EQ(loaded.initial_state.entity_count, 1U);
    EXPECT_EQ(loaded.initial_state.entities[0].id,
              std::numeric_limits<std::uint64_t>::max());
    ASSERT_EQ(loaded.tick_count, 1U);
    ASSERT_EQ(loaded.ticks[0].event_count, 1U);
    EXPECT_EQ(loaded.ticks[0].events[0].kind, GAME_RULES_EVENT_LEVEL_WON);
    EXPECT_EQ(loaded.has_final_state, 1U);
    EXPECT_EQ(loaded.final_state.outcome, GAME_RULES_OUTCOME_WON);
    EXPECT_EQ(loaded.has_outcome, 1U);
    EXPECT_EQ(loaded.outcome, GAME_RULES_OUTCOME_WON);
    ASSERT_EQ(loaded.has_state, 1U);
    EXPECT_EQ(loaded.state.level.cell_count, 2U);
    EXPECT_EQ(loaded.state.level.cells[0].coordinate.x, 0);
    EXPECT_EQ(loaded.state.level.cells[1].coordinate.x, 1);
    EXPECT_EQ(loaded.state.level.fixture_count, 1U);
    EXPECT_EQ(loaded.state.level.fixtures[0].kind, GAME_RULES_FIXTURE_EXIT);

    // Input memory is borrowed only during the call and cannot affect either engine or result.
    cells[0].coordinate.x = 42;
    entities[0].id = 7;
    EXPECT_EQ(loaded.state.level.cells[1].coordinate.x, 1);
    EXPECT_EQ(loaded.state.resolved.entities[0].id,
              std::numeric_limits<std::uint64_t>::max());

    engine.reset();
    EXPECT_EQ(loaded.state.resolved.outcome, GAME_RULES_OUTCOME_WON);
    game_rules_load_result_dispose(&loaded);
    EXPECT_EQ(loaded.owned_storage, nullptr);
    EXPECT_EQ(loaded.has_state, 0U);
}

TEST(CDataApi, RejectsMalformedTypedInputWithoutReplacingTheLevel)
{
    constexpr game_rules_cell cells[] = {flat_cell(0, 0), flat_cell(1, 0)};
    constexpr game_rules_entity entities[] = {
        {1, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0}};
    const game_rules_level_definition valid =
        level_without_fixtures(cells, entities, 2, 1);

    EnginePointer engine{game_rules_engine_create()};
    ASSERT_NE(engine, nullptr);
    game_rules_load_result loaded{};
    ASSERT_EQ(game_rules_engine_load_level_data(engine.get(), &valid, &loaded),
              GAME_RULES_CALL_OK);
    ASSERT_EQ(loaded.accepted, 1U);
    game_rules_load_result_dispose(&loaded);

    game_rules_level_definition missing_cells = valid;
    missing_cells.cells = nullptr;
    EXPECT_EQ(game_rules_engine_load_level_data(engine.get(), &missing_cells, &loaded),
              GAME_RULES_CALL_INVALID_ARGUMENT);
    EXPECT_EQ(loaded.owned_storage, nullptr);

    game_rules_cell malformed_cells[] = {flat_cell(0, 0), flat_cell(1, 0)};
    malformed_cells[1].kind = 99U;
    game_rules_level_definition malformed = valid;
    malformed.cells = malformed_cells;
    EXPECT_EQ(game_rules_engine_load_level_data(engine.get(), &malformed, &loaded),
              GAME_RULES_CALL_INVALID_ARGUMENT);

    game_rules_level_definition invalid_level = valid;
    invalid_level.entities = nullptr;
    invalid_level.entity_count = 0;
    ASSERT_EQ(game_rules_engine_load_level_data(engine.get(), &invalid_level, &loaded),
              GAME_RULES_CALL_OK);
    EXPECT_EQ(loaded.status, GAME_RULES_LOAD_INVALID_LEVEL);
    EXPECT_EQ(loaded.accepted, 0U);
    ASSERT_EQ(loaded.error_count, 1U);
    EXPECT_EQ(loaded.errors[0].code, GAME_RULES_VALIDATION_PLAYER_COUNT_NOT_ONE);
    ASSERT_EQ(loaded.has_state, 1U);
    ASSERT_EQ(loaded.state.resolved.entity_count, 1U);
    EXPECT_EQ(loaded.state.resolved.entities[0].id, 1U);
    game_rules_load_result_dispose(&loaded);
}

TEST(CDataApi, ReturnsOrderedMoveTicksAndStableResultOwnership)
{
    constexpr game_rules_cell cells[] = {
        flat_cell(0, 0), flat_cell(1, 0), flat_cell(2, 0)};
    constexpr game_rules_entity entities[] = {
        {1, GAME_RULES_ENTITY_PLAYER, {0, 0}, 0},
        {9, GAME_RULES_ENTITY_BOX, {1, 0}, 0},
    };
    const game_rules_level_definition level =
        level_without_fixtures(cells, entities, 3, 1);

    EnginePointer engine{game_rules_engine_create()};
    ASSERT_NE(engine, nullptr);
    game_rules_load_result loaded{};
    ASSERT_EQ(game_rules_engine_load_level_data(engine.get(), &level, &loaded),
              GAME_RULES_CALL_OK);
    ASSERT_EQ(loaded.accepted, 1U);
    game_rules_load_result_dispose(&loaded);

    game_rules_move_result moved{};
    ASSERT_EQ(game_rules_engine_move_data(engine.get(), GAME_RULES_DIRECTION_EAST, &moved),
              GAME_RULES_CALL_OK);
    EXPECT_EQ(moved.status, GAME_RULES_MOVE_MOVED);
    EXPECT_EQ(moved.accepted, 1U);
    EXPECT_EQ(moved.has_direction, 1U);
    EXPECT_EQ(moved.direction, GAME_RULES_DIRECTION_EAST);
    EXPECT_EQ(moved.event_count, 0U);
    ASSERT_EQ(moved.tick_count, 1U);
    ASSERT_EQ(moved.ticks[0].event_count, 2U);
    const game_rules_event& player = moved.ticks[0].events[0];
    EXPECT_EQ(player.kind, GAME_RULES_EVENT_ENTITY_MOVED);
    EXPECT_EQ(player.entity_id, 1U);
    EXPECT_EQ(player.from.x, 0);
    EXPECT_EQ(player.to.x, 1);
    EXPECT_EQ(player.movement_cause, GAME_RULES_MOVEMENT_PLAYER);
    const game_rules_event& box = moved.ticks[0].events[1];
    EXPECT_EQ(box.kind, GAME_RULES_EVENT_ENTITY_MOVED);
    EXPECT_EQ(box.entity_id, 9U);
    EXPECT_EQ(box.from.x, 1);
    EXPECT_EQ(box.to.x, 2);
    ASSERT_EQ(moved.has_state, 1U);
    EXPECT_EQ(moved.state.resolved.entities[0].coordinate.x, 1);
    EXPECT_EQ(moved.state.resolved.entities[1].coordinate.x, 2);

    game_rules_rewind_result rewound{};
    ASSERT_EQ(game_rules_engine_rewind_data(engine.get(), &rewound), GAME_RULES_CALL_OK);
    EXPECT_EQ(rewound.status, GAME_RULES_REWIND_REWOUND);
    EXPECT_EQ(rewound.accepted, 1U);
    ASSERT_EQ(rewound.event_count, 1U);
    EXPECT_EQ(rewound.events[0].kind, GAME_RULES_EVENT_STATE_REWOUND);
    ASSERT_EQ(rewound.has_restored_state, 1U);
    EXPECT_EQ(rewound.restored_state.entities[0].coordinate.x, 0);

    // The earlier move result is a caller-owned snapshot, not an engine-borrowed view.
    EXPECT_EQ(moved.state.resolved.entities[0].coordinate.x, 1);
    engine.reset();
    EXPECT_EQ(moved.ticks[0].events[1].entity_id, 9U);
    EXPECT_EQ(rewound.state.resolved.entities[0].coordinate.x, 0);
    game_rules_rewind_result_dispose(&rewound);
    game_rules_move_result_dispose(&moved);
}

TEST(CDataApi, ReturnsRejectedMoveAndInitializationFallEvents)
{
    constexpr game_rules_cell cells[] = {flat_cell(0, 0), flat_cell(1, 0, 1)};
    constexpr game_rules_entity entities[] = {
        {8, GAME_RULES_ENTITY_BARREL, {0, 0}, 4},
        {1, GAME_RULES_ENTITY_PLAYER, {1, 0}, 2},
    };
    const game_rules_level_definition level =
        level_without_fixtures(cells, entities, 2, 1);

    EnginePointer engine{game_rules_engine_create()};
    ASSERT_NE(engine, nullptr);
    game_rules_load_result loaded{};
    ASSERT_EQ(game_rules_engine_load_level_data(engine.get(), &level, &loaded),
              GAME_RULES_CALL_OK);
    ASSERT_EQ(loaded.accepted, 1U);
    ASSERT_GE(loaded.tick_count, 2U);
    ASSERT_GE(loaded.ticks[0].event_count, 2U);
    EXPECT_EQ(loaded.ticks[0].events[0].kind, GAME_RULES_EVENT_ENTITY_MOVED);
    EXPECT_EQ(loaded.ticks[0].events[0].movement_cause, GAME_RULES_MOVEMENT_FALL);
    EXPECT_EQ(loaded.ticks[0].events[1].kind, GAME_RULES_EVENT_BARREL_ARMED);
    bool found_explosion = false;
    for (std::uint32_t tick_index = 0; tick_index < loaded.tick_count; ++tick_index) {
        for (std::uint32_t event_index = 0;
             event_index < loaded.ticks[tick_index].event_count;
             ++event_index) {
            const game_rules_event& event = loaded.ticks[tick_index].events[event_index];
            if (event.kind == GAME_RULES_EVENT_BARREL_EXPLODED) {
                found_explosion = true;
                EXPECT_EQ(event.entity_id, 8U);
                EXPECT_EQ(event.coordinate.x, 0);
                EXPECT_EQ(event.bottom_half_steps, 0);
            }
        }
    }
    EXPECT_TRUE(found_explosion);
    game_rules_load_result_dispose(&loaded);

    game_rules_move_result moved{};
    ASSERT_EQ(game_rules_engine_move_data(engine.get(), 99U, &moved), GAME_RULES_CALL_OK);
    EXPECT_EQ(moved.status, GAME_RULES_MOVE_INVALID_DIRECTION);
    EXPECT_EQ(moved.accepted, 0U);
    EXPECT_EQ(moved.has_direction, 0U);
    EXPECT_EQ(moved.event_count, 0U);
    game_rules_move_result_dispose(&moved);
}

} // namespace
