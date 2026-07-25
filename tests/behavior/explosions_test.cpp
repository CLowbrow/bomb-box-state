#include "game_rules/engine.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace {

using namespace game_rules;

[[nodiscard]] LevelDefinition flat_line(const std::vector<std::int32_t>& elevations,
                                        const Entity player)
{
    LevelDefinition level;
    level.width = static_cast<std::uint32_t>(elevations.size());
    level.height = 1;
    for (std::size_t index = 0; index < elevations.size(); ++index) {
        level.cells.push_back(Cell{
            Coordinate{static_cast<std::int32_t>(index), 0},
            FlatCell{elevations[index]},
        });
    }
    level.entities.push_back(player);
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
    level.entities.push_back(player);
    return level;
}

[[nodiscard]] const Entity& entity(const ResolvedState& state, const EntityId id)
{
    const auto found = std::find_if(state.entities.begin(), state.entities.end(),
                                    [id](const Entity& value) { return value.id == id; });
    EXPECT_NE(found, state.entities.end());
    return *found;
}

[[nodiscard]] bool contains_entity(const ResolvedState& state, const EntityId id)
{
    return std::any_of(state.entities.begin(), state.entities.end(),
                       [id](const Entity& value) { return value.id == id; });
}

TEST(Explosions, PopsAnAdjacentBoxThenSettlesItsFallAndRewindsExactly)
{
    LevelDefinition level = flat_line(
        {2, 2, 0, 0, -1}, Entity{1, EntityKind::player, {0, 0}, Height{4}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {1, 0}, Height{4}});
    level.entities.push_back(Entity{3, EntityKind::box, {3, 0}, Height{0}});
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    const MoveResult moved = engine.move(Direction::east);

    ASSERT_TRUE(moved.accepted());
    ASSERT_EQ(moved.ticks.size(), 4U);
    EXPECT_EQ(moved.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {0, 0}, {1, 0}, Height{4}, Height{4},
                                   MovementCause::player},
                  EntityMovedEvent{8, {1, 0}, {2, 0}, Height{4}, Height{4},
                                   MovementCause::player},
              }));
    EXPECT_EQ(moved.ticks[1].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{8, {2, 0}, {2, 0}, Height{4}, Height{0},
                                   MovementCause::fall},
                  BarrelArmedEvent{8},
              }));
    EXPECT_EQ(moved.ticks[2].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {2, 0}, Height{0}},
                  EntityMovedEvent{3, {3, 0}, {4, 0}, Height{0}, Height{0},
                                   MovementCause::blast},
              }));
    EXPECT_EQ(moved.ticks[3].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  3, {4, 0}, {4, 0}, Height{0}, Height{-2}, MovementCause::fall}}));
    ASSERT_TRUE(moved.final_state.has_value());
    EXPECT_FALSE(contains_entity(*moved.final_state, 8));
    EXPECT_EQ(entity(*moved.final_state, 3),
              (Entity{3, EntityKind::box, {4, 0}, Height{-2}}));
    EXPECT_TRUE(moved.final_state->armed_barrels.empty());
    EXPECT_EQ(moved.outcome, std::optional{Outcome::ongoing});

    ASSERT_TRUE(engine.rewind().accepted());
    EXPECT_EQ(engine.move(Direction::east), moved);
}

TEST(Explosions, SelectsOnlyTheEntityAtBlastHeightAndDropsTheUpperStack)
{
    LevelDefinition level = flat_line(
        {0, 1, 0, 0, 0}, Entity{1, EntityKind::player, {4, 0}, Height{0}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {1, 0}, Height{4}});
    level.entities.push_back(Entity{2, EntityKind::box, {2, 0}, Height{0}});
    level.entities.push_back(Entity{3, EntityKind::box, {2, 0}, Height{2}});
    level.entities.push_back(Entity{4, EntityKind::box, {2, 0}, Height{4}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 3U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{8, {1, 0}, {1, 0}, Height{4}, Height{2},
                                   MovementCause::fall},
                  BarrelArmedEvent{8},
              }));
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {1, 0}, Height{2}},
                  EntityMovedEvent{3, {2, 0}, {3, 0}, Height{2}, Height{2},
                                   MovementCause::blast},
              }));
    EXPECT_EQ(loaded.ticks[2].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{4, {2, 0}, {2, 0}, Height{4}, Height{2},
                                   MovementCause::fall},
                  EntityMovedEvent{3, {3, 0}, {3, 0}, Height{2}, Height{0},
                                   MovementCause::fall},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_EQ(entity(*loaded.final_state, 2).bottom, Height{0});
    EXPECT_EQ(entity(*loaded.final_state, 4).bottom, Height{2});
    EXPECT_EQ(entity(*loaded.final_state, 3),
              (Entity{3, EntityKind::box, {3, 0}, Height{0}}));

    LevelDefinition reversed = level;
    std::reverse(reversed.cells.begin(), reversed.cells.end());
    std::reverse(reversed.entities.begin(), reversed.entities.end());
    Engine reversed_engine;
    EXPECT_EQ(reversed_engine.load_level(reversed), loaded);
}

TEST(Explosions, ChainsThroughDirectlyTouchingBarrelsInTheSourceCell)
{
    LevelDefinition level = flat_line(
        {0, 0}, Entity{1, EntityKind::player, {1, 0}, Height{0}});
    level.entities.push_back(Entity{9, EntityKind::barrel, {0, 0}, Height{0}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {0, 0}, Height{4}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 3U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{8, {0, 0}, {0, 0}, Height{4}, Height{2},
                                   MovementCause::fall},
                  BarrelArmedEvent{8},
              }));
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {0, 0}, Height{2}},
                  BarrelArmedEvent{9},
              }));
    EXPECT_EQ(loaded.ticks[2].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{9, {0, 0}, Height{0}},
                  LevelLostEvent{},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_FALSE(contains_entity(*loaded.final_state, 9));
    EXPECT_TRUE(loaded.final_state->armed_barrels.empty());
    EXPECT_EQ(loaded.outcome, std::optional{Outcome::lost});
}

TEST(Explosions, PoppingMiddleSupportCausesAFatalPlayerFallInTheNextTick)
{
    LevelDefinition level = flat_line(
        {0, 1, 0, 0}, Entity{1, EntityKind::player, {2, 0}, Height{4}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {1, 0}, Height{4}});
    level.entities.push_back(Entity{2, EntityKind::box, {2, 0}, Height{0}});
    level.entities.push_back(Entity{3, EntityKind::box, {2, 0}, Height{2}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 3U);
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {1, 0}, Height{2}},
                  EntityMovedEvent{3, {2, 0}, {3, 0}, Height{2}, Height{2},
                                   MovementCause::blast},
              }));
    EXPECT_EQ(loaded.ticks[2].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {2, 0}, {2, 0}, Height{4}, Height{2},
                                   MovementCause::fall},
                  EntityMovedEvent{3, {3, 0}, {3, 0}, Height{2}, Height{0},
                                   MovementCause::fall},
                  LevelLostEvent{},
              }));
    EXPECT_EQ(loaded.outcome, std::optional{Outcome::lost});
}

TEST(Explosions, ChainsThroughABarrelWhoseBlastMovementIsBlocked)
{
    LevelDefinition level = flat_line(
        {1, 0, 0, 0}, Entity{1, EntityKind::player, {0, 0}, Height{2}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {1, 0}, Height{2}});
    level.entities.push_back(Entity{9, EntityKind::barrel, {2, 0}, Height{0}});
    level.fixtures.push_back(Fixture{{3, 0}, Door{SwitchColor::red}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 3U);
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {1, 0}, Height{0}},
                  BarrelArmedEvent{9},
              }));
    EXPECT_EQ(loaded.ticks[2].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{9, {2, 0}, Height{0}},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_FALSE(contains_entity(*loaded.final_state, 9));
    EXPECT_TRUE(loaded.final_state->armed_barrels.empty());
}

TEST(Explosions, RecomputesSwitchesAndDoorsInTheExplosionTick)
{
    LevelDefinition level = flat_line(
        {0, 0, 0}, Entity{1, EntityKind::player, {2, 0}, Height{0}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {0, 0}, Height{2}});
    level.fixtures = {
        Fixture{{0, 0}, Switch{SwitchColor::red}},
        Fixture{{1, 0}, Door{SwitchColor::red}},
    };
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 2U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{8, {0, 0}, {0, 0}, Height{2}, Height{0},
                                   MovementCause::fall},
                  BarrelArmedEvent{8},
                  SwitchChangedEvent{SwitchColor::red, true},
                  DoorOpenedEvent{{1, 0}, SwitchColor::red},
              }));
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {0, 0}, Height{0}},
                  SwitchChangedEvent{SwitchColor::red, false},
                  DoorClosedEvent{{1, 0}, SwitchColor::red},
              }));
    EXPECT_TRUE(loaded.final_state->active_switch_colors.empty());
    EXPECT_TRUE(loaded.final_state->open_doors.empty());
}

TEST(Explosions, LetsAnArmedRampStackSettleBeforeTheSupportingBarrelExplodes)
{
    LevelDefinition level;
    level.width = 3;
    level.height = 1;
    level.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, RampCell{Direction::west, 0}},
        Cell{{2, 0}, FlatCell{1}},
    };
    level.entities = {
        Entity{8, EntityKind::barrel, {1, 0}, Height{2}},
        Entity{1, EntityKind::player, {1, 0}, Height{4}},
    };
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 3U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{8, {1, 0}, {1, 0}, Height{2}, Height{1},
                                   MovementCause::fall},
                  BarrelArmedEvent{8},
                  EntityMovedEvent{1, {1, 0}, {1, 0}, Height{4}, Height{3},
                                   MovementCause::fall},
              }));
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{8, {1, 0}, {0, 0}, Height{1}, Height{0},
                                   MovementCause::slide},
                  EntityMovedEvent{1, {1, 0}, {0, 0}, Height{3}, Height{2},
                                   MovementCause::slide},
              }));
    EXPECT_EQ(loaded.ticks[2].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {0, 0}, Height{0}},
                  LevelLostEvent{},
              }));
    EXPECT_EQ(loaded.outcome, std::optional{Outcome::lost});
    EXPECT_EQ(entity(*loaded.final_state, 1).bottom, Height{2});
}

TEST(Explosions, UsesRampEndpointConnectivityAndIgnoresPerpendicularEdges)
{
    LevelDefinition connected;
    connected.width = 3;
    connected.height = 1;
    connected.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, RampCell{Direction::west, 0}},
        Cell{{2, 0}, FlatCell{1}},
    };
    connected.entities = {
        Entity{2, EntityKind::box, {0, 0}, Height{0}},
        Entity{3, EntityKind::box, {1, 0}, Height{1}},
        Entity{1, EntityKind::player, {1, 0}, Height{3}},
        Entity{4, EntityKind::box, {2, 0}, Height{2}},
        Entity{8, EntityKind::barrel, {2, 0}, Height{6}},
    };
    Engine connected_engine;
    const LoadResult lost = connected_engine.load_level(connected);
    ASSERT_TRUE(lost.accepted());
    ASSERT_EQ(lost.ticks.size(), 2U);
    EXPECT_EQ(lost.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {2, 0}, Height{4}},
                  LevelLostEvent{},
              }));
    EXPECT_EQ(lost.outcome, std::optional{Outcome::lost});

    LevelDefinition perpendicular = flat_grid(
        3, 2, Entity{1, EntityKind::player, {1, 0}, Height{1}});
    perpendicular.cells[1].geometry = RampCell{Direction::west, 0};
    perpendicular.cells[2].geometry = FlatCell{1};
    perpendicular.entities.push_back(Entity{2, EntityKind::box, {0, 0}, Height{0}});
    perpendicular.entities.push_back(Entity{8, EntityKind::barrel, {1, 1}, Height{2}});
    Engine perpendicular_engine;
    const LoadResult survived = perpendicular_engine.load_level(perpendicular);
    ASSERT_TRUE(survived.accepted());
    EXPECT_EQ(survived.outcome, std::optional{Outcome::ongoing});
    EXPECT_EQ(entity(*survived.final_state, 1),
              (Entity{1, EntityKind::player, {1, 0}, Height{1}}));
}

TEST(Explosions, CarriesABlastAcrossConnectedRampCenters)
{
    LevelDefinition level;
    level.width = 4;
    level.height = 1;
    level.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, RampCell{Direction::west, 0}},
        Cell{{2, 0}, RampCell{Direction::west, 1}},
        Cell{{3, 0}, FlatCell{2}},
    };
    level.entities = {
        Entity{2, EntityKind::box, {0, 0}, Height{0}},
        Entity{8, EntityKind::barrel, {1, 0}, Height{3}},
        Entity{1, EntityKind::player, {2, 0}, Height{3}},
    };
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 2U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{8, {1, 0}, {1, 0}, Height{3}, Height{1},
                                   MovementCause::fall},
                  BarrelArmedEvent{8},
              }));
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {1, 0}, Height{1}},
                  LevelLostEvent{},
              }));
    EXPECT_EQ(loaded.outcome, std::optional{Outcome::lost});
}

TEST(ExplosionWaves, CancelsOpposingImpulsesFromSimultaneousSourcesDeterministically)
{
    LevelDefinition level = flat_grid(
        5, 2, Entity{1, EntityKind::player, {2, 1}, Height{0}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {1, 0}, Height{2}});
    level.entities.push_back(Entity{3, EntityKind::box, {2, 0}, Height{0}});
    level.entities.push_back(Entity{4, EntityKind::barrel, {3, 0}, Height{2}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 2U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{8, {1, 0}, {1, 0}, Height{2}, Height{0},
                                   MovementCause::fall},
                  BarrelArmedEvent{8},
                  EntityMovedEvent{4, {3, 0}, {3, 0}, Height{2}, Height{0},
                                   MovementCause::fall},
                  BarrelArmedEvent{4},
              }));
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {1, 0}, Height{0}},
                  BarrelExplodedEvent{4, {3, 0}, Height{0}},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_EQ(entity(*loaded.final_state, 3),
              (Entity{3, EntityKind::box, {2, 0}, Height{0}}));
    EXPECT_TRUE(loaded.final_state->armed_barrels.empty());

    LevelDefinition reversed = level;
    std::reverse(reversed.cells.begin(), reversed.cells.end());
    std::reverse(reversed.entities.begin(), reversed.entities.end());
    Engine reversed_engine;
    EXPECT_EQ(reversed_engine.load_level(reversed), loaded);
}

TEST(ExplosionWaves, CancelsOtherwiseValidMovementsWithOverlappingDestinations)
{
    LevelDefinition level = flat_grid(
        5, 2, Entity{1, EntityKind::player, {2, 1}, Height{0}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {0, 0}, Height{2}});
    level.entities.push_back(Entity{2, EntityKind::box, {1, 0}, Height{0}});
    level.entities.push_back(Entity{3, EntityKind::box, {3, 0}, Height{0}});
    level.entities.push_back(Entity{9, EntityKind::barrel, {4, 0}, Height{2}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 2U);
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {0, 0}, Height{0}},
                  BarrelExplodedEvent{9, {4, 0}, Height{0}},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_EQ(entity(*loaded.final_state, 2).coordinate, (Coordinate{1, 0}));
    EXPECT_EQ(entity(*loaded.final_state, 3).coordinate, (Coordinate{3, 0}));
}

TEST(ExplosionWaves, AllowsNonoverlappingArrivalsAtDifferentHeightsInOneCell)
{
    LevelDefinition level = flat_grid(
        5, 2, Entity{1, EntityKind::player, {2, 1}, Height{0}});
    level.cells[3].geometry = FlatCell{1};
    level.cells[4].geometry = FlatCell{1};
    level.entities.push_back(Entity{8, EntityKind::barrel, {0, 0}, Height{2}});
    level.entities.push_back(Entity{2, EntityKind::box, {1, 0}, Height{0}});
    level.entities.push_back(Entity{3, EntityKind::box, {3, 0}, Height{2}});
    level.entities.push_back(Entity{9, EntityKind::barrel, {4, 0}, Height{4}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 2U);
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {0, 0}, Height{0}},
                  BarrelExplodedEvent{9, {4, 0}, Height{2}},
                  EntityMovedEvent{2, {1, 0}, {2, 0}, Height{0}, Height{0},
                                   MovementCause::blast},
                  EntityMovedEvent{3, {3, 0}, {2, 0}, Height{2}, Height{2},
                                   MovementCause::blast},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_EQ(entity(*loaded.final_state, 2).coordinate, (Coordinate{2, 0}));
    EXPECT_EQ(entity(*loaded.final_state, 3).coordinate, (Coordinate{2, 0}));
}

TEST(ExplosionWaves, ResolvesDirectionConflictBeforeBlockedDestinationsAndStillChains)
{
    LevelDefinition level = flat_grid(
        4, 4, Entity{1, EntityKind::player, {3, 3}, Height{0}});
    level.entities.push_back(Entity{2, EntityKind::box, {1, 0}, Height{0}});
    level.entities.push_back(Entity{7, EntityKind::barrel, {1, 1}, Height{0}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {2, 1}, Height{2}});
    level.entities.push_back(Entity{9, EntityKind::barrel, {1, 2}, Height{2}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 3U);
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {2, 1}, Height{0}},
                  BarrelExplodedEvent{9, {1, 2}, Height{0}},
                  BarrelArmedEvent{7},
              }));
    EXPECT_EQ(loaded.ticks[2].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{7, {1, 1}, Height{0}},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_FALSE(contains_entity(*loaded.final_state, 7));
    EXPECT_EQ(entity(*loaded.final_state, 2).coordinate, (Coordinate{1, 0}));
}

TEST(ExplosionChains, SettlesBlastDrivenFallBeforeTheNextWaveAndRewindsExactly)
{
    LevelDefinition level = flat_line(
        {2, 2, 0, 0, -1}, Entity{1, EntityKind::player, {0, 0}, Height{4}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {1, 0}, Height{4}});
    level.entities.push_back(Entity{9, EntityKind::barrel, {3, 0}, Height{0}});
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    const MoveResult moved = engine.move(Direction::east);

    ASSERT_TRUE(moved.accepted());
    ASSERT_EQ(moved.ticks.size(), 5U);
    EXPECT_EQ(moved.ticks[2].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {2, 0}, Height{0}},
                  EntityMovedEvent{9, {3, 0}, {4, 0}, Height{0}, Height{0},
                                   MovementCause::blast},
                  BarrelArmedEvent{9},
              }));
    EXPECT_EQ(moved.ticks[3].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{9, {4, 0}, {4, 0}, Height{0}, Height{-2},
                                   MovementCause::fall},
              }));
    EXPECT_EQ(moved.ticks[4].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{9, {4, 0}, Height{-2}},
              }));
    ASSERT_TRUE(moved.final_state.has_value());
    EXPECT_TRUE(moved.final_state->armed_barrels.empty());

    ASSERT_TRUE(engine.rewind().accepted());
    EXPECT_EQ(engine.move(Direction::east), moved);
}

TEST(ExplosionChains, DetonatesMultipleNewlyArmedBarrelsTogetherInTheNextWave)
{
    LevelDefinition level = flat_grid(
        5, 5, Entity{1, EntityKind::player, {2, 4}, Height{0}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {2, 2}, Height{2}});
    level.entities.push_back(Entity{9, EntityKind::barrel, {1, 2}, Height{0}});
    level.entities.push_back(Entity{7, EntityKind::barrel, {3, 2}, Height{0}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 3U);
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {2, 2}, Height{0}},
                  EntityMovedEvent{9, {1, 2}, {0, 2}, Height{0}, Height{0},
                                   MovementCause::blast},
                  BarrelArmedEvent{9},
                  EntityMovedEvent{7, {3, 2}, {4, 2}, Height{0}, Height{0},
                                   MovementCause::blast},
                  BarrelArmedEvent{7},
              }));
    EXPECT_EQ(loaded.ticks[2].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{9, {0, 2}, Height{0}},
                  BarrelExplodedEvent{7, {4, 2}, Height{0}},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_TRUE(loaded.final_state->armed_barrels.empty());
    EXPECT_EQ(loaded.outcome, std::optional{Outcome::ongoing});
}

TEST(ExplosionChains, SettlesBlastDrivenRampFallAndSlideBeforeTheNextWave)
{
    LevelDefinition level = flat_grid(
        5, 2, Entity{1, EntityKind::player, {0, 1}, Height{0}});
    level.cells[0].geometry = FlatCell{1};
    level.cells[1].geometry = FlatCell{1};
    level.cells[2].geometry = FlatCell{1};
    level.cells[3].geometry = RampCell{Direction::east, 0};
    level.entities.push_back(Entity{8, EntityKind::barrel, {1, 0}, Height{4}});
    level.entities.push_back(Entity{9, EntityKind::barrel, {2, 0}, Height{2}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 5U);
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {1, 0}, Height{2}},
                  EntityMovedEvent{9, {2, 0}, {3, 0}, Height{2}, Height{2},
                                   MovementCause::blast},
                  BarrelArmedEvent{9},
              }));
    EXPECT_EQ(loaded.ticks[2].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{9, {3, 0}, {3, 0}, Height{2}, Height{1},
                                   MovementCause::fall},
              }));
    EXPECT_EQ(loaded.ticks[3].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{9, {3, 0}, {4, 0}, Height{1}, Height{0},
                                   MovementCause::slide},
              }));
    EXPECT_EQ(loaded.ticks[4].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{9, {4, 0}, Height{0}},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_TRUE(loaded.final_state->armed_barrels.empty());
}

} // namespace
