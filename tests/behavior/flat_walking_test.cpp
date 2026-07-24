#include "game_rules/engine.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>
#include <vector>

namespace {

using namespace game_rules;

[[nodiscard]] LevelDefinition grid_level(const Coordinate player_coordinate = {1, 1})
{
    LevelDefinition level;
    level.width = 3;
    level.height = 3;
    for (std::int32_t y = 0; y < 3; ++y) {
        for (std::int32_t x = 0; x < 3; ++x) {
            level.cells.push_back(Cell{Coordinate{x, y}, FlatCell{0}});
        }
    }
    level.entities = {
        Entity{17, EntityKind::player, player_coordinate, Height::from_elevation(0)},
    };
    return level;
}

[[nodiscard]] ResolvedState player_state(const Coordinate coordinate,
                                         const Height bottom = Height::from_elevation(0))
{
    return ResolvedState{
        {Entity{17, EntityKind::player, coordinate, bottom}},
        Outcome::ongoing,
    };
}

[[nodiscard]] MoveResult expected_move(const Direction direction,
                                       const Coordinate from,
                                       const Coordinate to,
                                       const Height bottom = Height::from_elevation(0))
{
    const ResolvedState initial = player_state(from, bottom);
    const ResolvedState final = player_state(to, bottom);
    return MoveResult{
        MoveStatus::moved,
        direction,
        {},
        initial,
        {TickResult{
            0,
            {GameplayEvent{EntityMovedEvent{
                17, from, to, bottom, bottom, MovementCause::player,
            }}},
            final,
        }},
        final,
        Outcome::ongoing,
    };
}

[[nodiscard]] MoveResult expected_rejection(const MoveStatus reason,
                                            const Direction direction,
                                            const ResolvedState& state)
{
    return MoveResult{
        reason,
        direction,
        {GameplayEvent{MoveBlockedEvent{direction, reason}}},
        std::nullopt,
        {},
        state,
        state.outcome,
    };
}

TEST(FlatWalking, WalksInAllCardinalDirections)
{
    struct Scenario final {
        Direction direction;
        Coordinate destination;
    };
    const std::vector<Scenario> scenarios{
        {Direction::north, {1, 2}},
        {Direction::east, {2, 1}},
        {Direction::south, {1, 0}},
        {Direction::west, {0, 1}},
    };

    for (const Scenario& scenario : scenarios) {
        SCOPED_TRACE(::testing::PrintToString(scenario.direction));
        Engine engine;
        ASSERT_TRUE(engine.load_level(grid_level()).accepted());
        const MoveResult actual = engine.move(scenario.direction);
        EXPECT_EQ(actual, expected_move(scenario.direction, {1, 1}, scenario.destination));
        EXPECT_EQ(engine.resolved_state(), actual.final_state);
    }
}

TEST(FlatWalking, HonorsDeclaredAxisDirections)
{
    LevelDefinition level = grid_level();
    level.coordinates.positive_x = HorizontalAxisDirection::west;
    level.coordinates.positive_y = VerticalAxisDirection::south;

    Engine east_engine;
    ASSERT_TRUE(east_engine.load_level(level).accepted());
    EXPECT_EQ(east_engine.move(Direction::east),
              expected_move(Direction::east, {1, 1}, {0, 1}));

    Engine north_engine;
    ASSERT_TRUE(north_engine.load_level(level).accepted());
    EXPECT_EQ(north_engine.move(Direction::north),
              expected_move(Direction::north, {1, 1}, {1, 0}));
}

TEST(FlatWalking, RejectsWithoutMutatingStateOrHistory)
{
    Engine boundary_engine;
    const LevelDefinition boundary_level = grid_level({0, 0});
    ASSERT_TRUE(boundary_engine.load_level(boundary_level).accepted());
    const ResolvedState boundary_state = player_state({0, 0});
    EXPECT_EQ(boundary_engine.move(Direction::west),
              expected_rejection(MoveStatus::world_boundary, Direction::west, boundary_state));
    EXPECT_EQ(boundary_engine.rewind().status, RewindStatus::history_empty);

    LevelDefinition high_ledge = grid_level();
    high_ledge.cells[5].geometry = FlatCell{1};
    Engine high_engine;
    ASSERT_TRUE(high_engine.load_level(high_ledge).accepted());
    EXPECT_EQ(high_engine.move(Direction::east),
              expected_rejection(MoveStatus::ledge, Direction::east, player_state({1, 1})));

    LevelDefinition low_ledge = grid_level();
    low_ledge.cells[4].geometry = FlatCell{1};
    low_ledge.entities.front().bottom = Height::from_elevation(1);
    Engine low_engine;
    ASSERT_TRUE(low_engine.load_level(low_ledge).accepted());
    EXPECT_EQ(low_engine.move(Direction::east),
              expected_rejection(MoveStatus::ledge,
                                 Direction::east,
                                 player_state({1, 1}, Height::from_elevation(1))));

    LevelDefinition occupied = grid_level();
    occupied.entities.push_back(
        Entity{4, EntityKind::box, Coordinate{2, 1}, Height::from_elevation(0)});
    Engine occupied_engine;
    ASSERT_TRUE(occupied_engine.load_level(occupied).accepted());
    const MoveResult occupied_result = occupied_engine.move(Direction::east);
    const ResolvedState occupied_state{canonicalize_level(occupied).entities, Outcome::ongoing};
    EXPECT_EQ(occupied_result,
              expected_rejection(MoveStatus::occupied, Direction::east, occupied_state));
    EXPECT_EQ(occupied_engine.resolved_state(), std::optional{occupied_state});
}

TEST(FlatWalking, WalksOntoCompatibleStackSupport)
{
    LevelDefinition level;
    level.width = 2;
    level.height = 1;
    level.cells = {
        Cell{{0, 0}, FlatCell{1}},
        Cell{{1, 0}, FlatCell{0}},
    };
    level.entities = {
        Entity{17, EntityKind::player, {0, 0}, Height::from_elevation(1)},
        Entity{4, EntityKind::box, {1, 0}, Height::from_elevation(0)},
    };

    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());
    const MoveResult result = engine.move(Direction::east);
    ASSERT_TRUE(result.accepted());
    ASSERT_TRUE(result.final_state.has_value());
    ASSERT_EQ(result.final_state->entities.size(), 2U);
    EXPECT_EQ(result.final_state->entities[1].coordinate, (Coordinate{1, 0}));
    EXPECT_EQ(result.final_state->entities[1].bottom, Height::from_elevation(1));
}

TEST(FlatWalking, ValidatesInputAndReportsUnsupportedPhaseBoundaries)
{
    Engine empty;
    const MoveResult no_level = empty.move(Direction::north);
    EXPECT_EQ(no_level.status, MoveStatus::no_level);
    EXPECT_FALSE(no_level.final_state.has_value());
    EXPECT_TRUE(no_level.ticks.empty());

    Engine engine;
    ASSERT_TRUE(engine.load_level(grid_level()).accepted());
    const ResolvedState unchanged = player_state({1, 1});
    const Direction malformed = static_cast<Direction>(255);
    const MoveResult invalid = engine.move(malformed);
    EXPECT_EQ(invalid.status, MoveStatus::invalid_direction);
    EXPECT_TRUE(invalid.events.empty());
    EXPECT_EQ(invalid.final_state, std::optional{unchanged});
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);

    LevelDefinition ramp = grid_level();
    ramp.width = 3;
    ramp.height = 1;
    ramp.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, RampCell{Direction::west, 0}},
        Cell{{2, 0}, FlatCell{1}},
    };
    ramp.entities = {
        Entity{17, EntityKind::player, {0, 0}, Height::from_elevation(0)},
    };
    Engine ramp_engine;
    ASSERT_TRUE(ramp_engine.load_level(ramp).accepted());
    EXPECT_EQ(ramp_engine.move(Direction::east).status, MoveStatus::unsupported_geometry);

    LevelDefinition fixture = grid_level();
    fixture.fixtures = {
        Fixture{{2, 1}, Switch{SwitchColor::red}},
    };
    Engine fixture_engine;
    ASSERT_TRUE(fixture_engine.load_level(fixture).accepted());
    EXPECT_EQ(fixture_engine.move(Direction::east).status, MoveStatus::unsupported_fixture);

    EXPECT_EQ(to_string(MoveStatus::world_boundary), "world_boundary");
    EXPECT_EQ(to_string(MovementCause::player), "player");
}

TEST(FlatWalking, PreservesRewindAndDeterministicBranching)
{
    LevelDefinition line;
    line.width = 3;
    line.height = 1;
    line.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, FlatCell{0}},
        Cell{{2, 0}, FlatCell{0}},
    };
    line.entities = {
        Entity{17, EntityKind::player, {0, 0}, Height::from_elevation(0)},
    };

    Engine first;
    Engine second;
    ASSERT_TRUE(first.load_level(line).accepted());
    ASSERT_TRUE(second.load_level(line).accepted());
    const MoveResult first_result = first.move(Direction::east);
    const MoveResult second_result = second.move(Direction::east);
    EXPECT_EQ(first_result, second_result);

    ASSERT_TRUE(first.move(Direction::east).accepted());
    const RewindResult first_rewind = first.rewind();
    EXPECT_EQ(first_rewind.state, std::optional{player_state({1, 0})});
    EXPECT_EQ(first_rewind.events, std::vector<GameplayEvent>{StateRewoundEvent{}});
    EXPECT_EQ(first_rewind.outcome, std::optional{Outcome::ongoing});
    ASSERT_TRUE(first.move(Direction::west).accepted());
    EXPECT_EQ(first.rewind().state, std::optional{player_state({1, 0})});
    EXPECT_EQ(first.rewind().state, std::optional{player_state({0, 0})});
    EXPECT_EQ(first.rewind().status, RewindStatus::history_empty);
}

} // namespace
