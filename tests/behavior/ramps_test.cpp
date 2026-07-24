#include "game_rules/engine.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace {

using namespace game_rules;

[[nodiscard]] LevelDefinition west_low_ramp_line(const Entity player)
{
    LevelDefinition level;
    level.width = 3;
    level.height = 1;
    level.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, RampCell{Direction::west, 0}},
        Cell{{2, 0}, FlatCell{1}},
    };
    level.entities = {player};
    return level;
}

[[nodiscard]] LevelDefinition flat_grid(const std::uint32_t width,
                                        const std::uint32_t height,
                                        const Entity player)
{
    LevelDefinition level;
    level.width = width;
    level.height = height;
    for (std::uint32_t y = 0; y < height; ++y) {
        for (std::uint32_t x = 0; x < width; ++x) {
            level.cells.push_back(Cell{
                Coordinate{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)},
                FlatCell{0},
            });
        }
    }
    level.entities = {player};
    return level;
}

[[nodiscard]] const Entity& entity(const ResolvedState& state, const EntityId id)
{
    const auto found = std::find_if(state.entities.begin(), state.entities.end(),
                                    [id](const Entity& value) { return value.id == id; });
    EXPECT_NE(found, state.entities.end());
    return *found;
}

TEST(Ramps, PlayerTraversesBothEndpointsInHalfStepsAndRewindsExactly)
{
    LevelDefinition uphill = west_low_ramp_line(
        Entity{1, EntityKind::player, {0, 0}, Height{0}});
    Engine uphill_engine;
    ASSERT_TRUE(uphill_engine.load_level(uphill).accepted());

    const MoveResult entered_uphill = uphill_engine.move(Direction::east);
    ASSERT_TRUE(entered_uphill.accepted());
    ASSERT_EQ(entered_uphill.ticks.size(), 1U);
    EXPECT_EQ(entered_uphill.ticks[0].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  1, {0, 0}, {1, 0}, Height{0}, Height{1}, MovementCause::player}}));
    EXPECT_EQ(entity(*entered_uphill.final_state, 1),
              (Entity{1, EntityKind::player, {1, 0}, Height{1}}));

    const MoveResult exited_uphill = uphill_engine.move(Direction::east);
    ASSERT_TRUE(exited_uphill.accepted());
    EXPECT_EQ(exited_uphill.ticks[0].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  1, {1, 0}, {2, 0}, Height{1}, Height{2}, MovementCause::player}}));

    ASSERT_TRUE(uphill_engine.rewind().accepted());
    EXPECT_EQ(uphill_engine.resolved_state(), entered_uphill.final_state);
    EXPECT_EQ(uphill_engine.move(Direction::east), exited_uphill);

    LevelDefinition downhill = west_low_ramp_line(
        Entity{1, EntityKind::player, {2, 0}, Height{2}});
    Engine downhill_engine;
    ASSERT_TRUE(downhill_engine.load_level(downhill).accepted());
    const MoveResult entered_downhill = downhill_engine.move(Direction::west);
    ASSERT_TRUE(entered_downhill.accepted());
    EXPECT_EQ(entered_downhill.ticks[0].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  1, {2, 0}, {1, 0}, Height{2}, Height{1}, MovementCause::player}}));
    const MoveResult exited_downhill = downhill_engine.move(Direction::west);
    ASSERT_TRUE(exited_downhill.accepted());
    EXPECT_EQ(exited_downhill.ticks[0].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  1, {1, 0}, {0, 0}, Height{1}, Height{0}, MovementCause::player}}));
}

TEST(Ramps, RejectsPerpendicularTraversalWithoutStartingAHistoryEntry)
{
    LevelDefinition level = flat_grid(
        3, 3, Entity{1, EntityKind::player, {1, 0}, Height{0}});
    level.cells[4].geometry = RampCell{Direction::west, 0};
    level.cells[5].geometry = FlatCell{1};
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    const MoveResult rejected = engine.move(Direction::north);

    EXPECT_EQ(rejected.status, MoveStatus::unsupported_geometry);
    EXPECT_EQ(rejected.events,
              (std::vector<GameplayEvent>{MoveBlockedEvent{
                  Direction::north, MoveStatus::unsupported_geometry}}));
    EXPECT_TRUE(rejected.ticks.empty());
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);
}

TEST(Ramps, HonorsDeclaredAxisDirectionsForTraversalAndSliding)
{
    LevelDefinition traversal = west_low_ramp_line(
        Entity{1, EntityKind::player, {0, 0}, Height{0}});
    traversal.coordinates.positive_x = HorizontalAxisDirection::west;
    traversal.cells[1].geometry = RampCell{Direction::east, 0};
    Engine traversal_engine;
    ASSERT_TRUE(traversal_engine.load_level(traversal).accepted());

    const MoveResult entered = traversal_engine.move(Direction::west);
    ASSERT_TRUE(entered.accepted());
    EXPECT_EQ(entered.ticks[0].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  1, {0, 0}, {1, 0}, Height{0}, Height{1}, MovementCause::player}}));

    LevelDefinition sliding = traversal;
    sliding.entities = {
        Entity{1, EntityKind::player, {2, 0}, Height{2}},
        Entity{8, EntityKind::box, {1, 0}, Height{1}},
    };
    Engine sliding_engine;
    const LoadResult loaded = sliding_engine.load_level(sliding);
    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 1U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  8, {1, 0}, {0, 0}, Height{1}, Height{0}, MovementCause::slide}}));
}

TEST(Ramps, PushesBoxesAndBarrelsDownhillThenSlidesWithoutArming)
{
    for (const EntityKind kind : {EntityKind::box, EntityKind::barrel}) {
        SCOPED_TRACE(kind == EntityKind::box ? "box" : "barrel");
        LevelDefinition level;
        level.width = 4;
        level.height = 1;
        level.cells = {
            Cell{{0, 0}, FlatCell{0}},
            Cell{{1, 0}, RampCell{Direction::west, 0}},
            Cell{{2, 0}, FlatCell{1}},
            Cell{{3, 0}, FlatCell{1}},
        };
        level.entities = {
            Entity{1, EntityKind::player, {3, 0}, Height{2}},
            Entity{8, kind, {2, 0}, Height{2}},
        };
        Engine engine;
        ASSERT_TRUE(engine.load_level(level).accepted());

        const MoveResult moved = engine.move(Direction::west);

        ASSERT_TRUE(moved.accepted());
        ASSERT_EQ(moved.ticks.size(), 2U);
        EXPECT_EQ(moved.ticks[0].events,
                  (std::vector<GameplayEvent>{
                      EntityMovedEvent{1, {3, 0}, {2, 0}, Height{2}, Height{2},
                                       MovementCause::player},
                      EntityMovedEvent{8, {2, 0}, {1, 0}, Height{2}, Height{1},
                                       MovementCause::player},
                  }));
        EXPECT_EQ(moved.ticks[1].events,
                  (std::vector<GameplayEvent>{EntityMovedEvent{
                      8, {1, 0}, {0, 0}, Height{1}, Height{0}, MovementCause::slide}}));
        EXPECT_TRUE(moved.final_state->armed_barrels.empty());
        EXPECT_EQ(entity(*moved.final_state, 8).coordinate, (Coordinate{0, 0}));
        EXPECT_EQ(entity(*moved.final_state, 8).bottom, Height{0});
    }

    LevelDefinition uphill = west_low_ramp_line(
        Entity{1, EntityKind::player, {0, 0}, Height{0}});
    uphill.entities.push_back(Entity{8, EntityKind::box, {1, 0}, Height{1}});
    Engine uphill_engine;
    ASSERT_TRUE(uphill_engine.load_level(uphill).accepted());
    EXPECT_EQ(uphill_engine.move(Direction::east).status,
              MoveStatus::unsupported_geometry);
}

TEST(Ramps, InitializationSlidesWholeStacksInCanonicalOrder)
{
    LevelDefinition ordered;
    ordered.width = 3;
    ordered.height = 2;
    ordered.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, RampCell{Direction::west, 0}},
        Cell{{2, 0}, FlatCell{1}},
        Cell{{0, 1}, FlatCell{0}},
        Cell{{1, 1}, RampCell{Direction::west, 0}},
        Cell{{2, 1}, FlatCell{1}},
    };
    ordered.entities = {
        Entity{9, EntityKind::box, {1, 0}, Height{1}},
        Entity{4, EntityKind::barrel, {1, 0}, Height{3}},
        Entity{7, EntityKind::box, {1, 1}, Height{1}},
        Entity{1, EntityKind::player, {1, 1}, Height{3}},
    };
    ordered.fixtures = {
        Fixture{{0, 1}, Switch{SwitchColor::red}},
        Fixture{{2, 0}, Door{SwitchColor::red}},
    };
    Engine engine;

    const LoadResult loaded = engine.load_level(ordered);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 1U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{9, {1, 0}, {0, 0}, Height{1}, Height{0},
                                   MovementCause::slide},
                  EntityMovedEvent{4, {1, 0}, {0, 0}, Height{3}, Height{2},
                                   MovementCause::slide},
                  EntityMovedEvent{7, {1, 1}, {0, 1}, Height{1}, Height{0},
                                   MovementCause::slide},
                  EntityMovedEvent{1, {1, 1}, {0, 1}, Height{3}, Height{2},
                                   MovementCause::slide},
                  SwitchChangedEvent{SwitchColor::red, true},
                  DoorOpenedEvent{{2, 0}, SwitchColor::red},
              }));
    EXPECT_TRUE(loaded.final_state->armed_barrels.empty());
    EXPECT_EQ(loaded.final_state->active_switch_colors,
              (std::vector<SwitchColor>{SwitchColor::red}));
    EXPECT_EQ(loaded.final_state->open_doors,
              (std::vector<Coordinate>{{2, 0}}));
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);

    LevelDefinition reversed = ordered;
    std::reverse(reversed.cells.begin(), reversed.cells.end());
    std::reverse(reversed.fixtures.begin(), reversed.fixtures.end());
    std::reverse(reversed.entities.begin(), reversed.entities.end());
    Engine reversed_engine;
    EXPECT_EQ(reversed_engine.load_level(reversed), loaded);
}

TEST(Ramps, RetriesABlockedSlideAfterThePlayerClearsTheLowEndpoint)
{
    LevelDefinition level = flat_grid(
        4, 3, Entity{1, EntityKind::player, {1, 1}, Height{0}});
    level.cells[6].geometry = RampCell{Direction::west, 0};
    level.cells[7].geometry = FlatCell{1};
    level.entities.push_back(Entity{3, EntityKind::box, {2, 1}, Height{1}});
    Engine engine;
    const LoadResult loaded = engine.load_level(level);
    ASSERT_TRUE(loaded.accepted());
    EXPECT_TRUE(loaded.ticks.empty());

    const MoveResult moved = engine.move(Direction::south);

    ASSERT_TRUE(moved.accepted());
    ASSERT_EQ(moved.ticks.size(), 2U);
    EXPECT_EQ(moved.ticks[0].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  1, {1, 1}, {1, 0}, Height{0}, Height{0}, MovementCause::player}}));
    EXPECT_EQ(moved.ticks[1].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  3, {2, 1}, {1, 1}, Height{1}, Height{0}, MovementCause::slide}}));
}

TEST(Ramps, RechecksDoorsAfterFixturesChangeAndBlocksExitTeleporters)
{
    LevelDefinition door_level = flat_grid(
        4, 3, Entity{1, EntityKind::player, {0, 0}, Height{0}});
    door_level.cells[6].geometry = RampCell{Direction::west, 0};
    door_level.cells[7].geometry = FlatCell{1};
    door_level.entities.push_back(Entity{3, EntityKind::box, {2, 1}, Height{1}});
    door_level.fixtures = {
        Fixture{{1, 0}, Switch{SwitchColor::red}},
        Fixture{{1, 1}, Door{SwitchColor::red}},
    };
    Engine door_engine;
    ASSERT_TRUE(door_engine.load_level(door_level).accepted());

    const MoveResult opened = door_engine.move(Direction::east);

    ASSERT_TRUE(opened.accepted());
    ASSERT_EQ(opened.ticks.size(), 2U);
    EXPECT_EQ(opened.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {0, 0}, {1, 0}, Height{0}, Height{0},
                                   MovementCause::player},
                  SwitchChangedEvent{SwitchColor::red, true},
                  DoorOpenedEvent{{1, 1}, SwitchColor::red},
              }));
    EXPECT_EQ(opened.ticks[1].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  3, {2, 1}, {1, 1}, Height{1}, Height{0}, MovementCause::slide}}));
    EXPECT_EQ(opened.final_state->open_doors,
              (std::vector<Coordinate>{{1, 1}}));

    LevelDefinition exit_level = west_low_ramp_line(
        Entity{1, EntityKind::player, {2, 0}, Height{2}});
    exit_level.entities.push_back(Entity{3, EntityKind::box, {1, 0}, Height{1}});
    exit_level.fixtures.push_back(Fixture{{0, 0}, ExitTeleporter{}});
    Engine exit_engine;
    const LoadResult blocked = exit_engine.load_level(exit_level);
    ASSERT_TRUE(blocked.accepted());
    EXPECT_TRUE(blocked.ticks.empty());
    EXPECT_EQ(entity(*blocked.final_state, 3).coordinate, (Coordinate{1, 0}));
}

TEST(Ramps, PlayerCanTraverseOntoAnEligibleExitAndRewindFromTheWin)
{
    LevelDefinition level = west_low_ramp_line(
        Entity{1, EntityKind::player, {1, 0}, Height{1}});
    level.fixtures.push_back(Fixture{{0, 0}, ExitTeleporter{}});
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    const MoveResult won = engine.move(Direction::west);

    ASSERT_TRUE(won.accepted());
    EXPECT_EQ(won.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {1, 0}, {0, 0}, Height{1}, Height{0},
                                   MovementCause::player},
                  LevelWonEvent{},
              }));
    EXPECT_EQ(won.outcome, std::optional{Outcome::won});
    EXPECT_EQ(engine.move(Direction::east).status, MoveStatus::level_terminal);
    ASSERT_TRUE(engine.rewind().accepted());
    EXPECT_EQ(engine.move(Direction::west), won);
}

TEST(Ramps, FallsOntoARampBeforeSlidingAndPreservesBarrelArming)
{
    LevelDefinition level = west_low_ramp_line(
        Entity{1, EntityKind::player, {2, 0}, Height{2}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {1, 0}, Height{5}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 2U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{8, {1, 0}, {1, 0}, Height{5}, Height{1},
                                   MovementCause::fall},
                  BarrelArmedEvent{8},
              }));
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  8, {1, 0}, {0, 0}, Height{1}, Height{0}, MovementCause::slide}}));
    EXPECT_EQ(loaded.final_state->armed_barrels, (std::vector<EntityId>{8}));
}

TEST(Ramps, ConflictingSlidesIntoOneEndpointRemainBlocked)
{
    LevelDefinition level = flat_grid(
        5, 2, Entity{1, EntityKind::player, {2, 0}, Height{0}});
    level.cells[0].geometry = FlatCell{1};
    level.cells[1].geometry = RampCell{Direction::east, 0};
    level.cells[3].geometry = RampCell{Direction::west, 0};
    level.cells[4].geometry = FlatCell{1};
    level.entities.push_back(Entity{2, EntityKind::box, {1, 0}, Height{1}});
    level.entities.push_back(Entity{3, EntityKind::box, {3, 0}, Height{1}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    EXPECT_TRUE(loaded.ticks.empty());
    EXPECT_EQ(entity(*loaded.final_state, 2).coordinate, (Coordinate{1, 0}));
    EXPECT_EQ(entity(*loaded.final_state, 3).coordinate, (Coordinate{3, 0}));
}

} // namespace
