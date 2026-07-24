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

[[nodiscard]] const Entity& entity(const ResolvedState& state, const EntityId id)
{
    const auto found = std::find_if(state.entities.begin(), state.entities.end(),
                                    [id](const Entity& value) { return value.id == id; });
    EXPECT_NE(found, state.entities.end());
    return *found;
}

TEST(Falling, StabilizesEveryInitialColumnBottomUpInOneTick)
{
    LevelDefinition level =
        flat_line({0, 0, 0}, Entity{90, EntityKind::player, {2, 0}, Height{0}});
    level.entities.push_back(Entity{31, EntityKind::box, {0, 0}, Height{8}});
    level.entities.push_back(Entity{7, EntityKind::box, {0, 0}, Height{4}});
    level.entities.push_back(Entity{55, EntityKind::box, {1, 0}, Height{6}});

    Engine engine;
    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_TRUE(loaded.initial_state.has_value());
    EXPECT_EQ(entity(*loaded.initial_state, 7).bottom, Height{4});
    EXPECT_EQ(entity(*loaded.initial_state, 31).bottom, Height{8});
    ASSERT_EQ(loaded.ticks.size(), 1U);
    EXPECT_EQ(loaded.ticks[0].index, 0U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{7, {0, 0}, {0, 0}, Height{4}, Height{0}, MovementCause::fall},
                  EntityMovedEvent{31, {0, 0}, {0, 0}, Height{8}, Height{2}, MovementCause::fall},
                  EntityMovedEvent{55, {1, 0}, {1, 0}, Height{6}, Height{0}, MovementCause::fall},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_EQ(entity(*loaded.final_state, 7).bottom, Height{0});
    EXPECT_EQ(entity(*loaded.final_state, 31).bottom, Height{2});
    EXPECT_EQ(entity(*loaded.final_state, 55).bottom, Height{0});
    EXPECT_EQ(loaded.ticks[0].state_after, *loaded.final_state);
    EXPECT_EQ(loaded.outcome, std::optional{Outcome::ongoing});
    EXPECT_EQ(engine.resolved_state(), loaded.final_state);
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);
}

TEST(Falling, ResolvesFallsOntoRampCenterBeforeTheRampPhase)
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
        Entity{1, EntityKind::player, {0, 0}, Height{0}},
        Entity{2, EntityKind::box, {1, 0}, Height{5}},
    };

    Engine engine;
    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_EQ(entity(*loaded.final_state, 2).bottom, Height{1});
    ASSERT_EQ(loaded.ticks.size(), 1U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  2, {1, 0}, {1, 0}, Height{5}, Height{1}, MovementCause::fall}}));
}

TEST(Falling, AppliesThePlayerFallThresholdAndTerminalGating)
{
    LevelDefinition survivable = flat_line({0}, Entity{1, EntityKind::player, {0, 0}, Height{1}});
    Engine survivor;
    const LoadResult survived = survivor.load_level(survivable);
    ASSERT_TRUE(survived.accepted());
    ASSERT_TRUE(survived.final_state.has_value());
    EXPECT_EQ(entity(*survived.final_state, 1).bottom, Height{0});
    EXPECT_EQ(survived.outcome, std::optional{Outcome::ongoing});

    LevelDefinition fatal = flat_line({0, 0}, Entity{1, EntityKind::player, {0, 0}, Height{2}});
    Engine casualty;
    const LoadResult lost = casualty.load_level(fatal);
    ASSERT_TRUE(lost.accepted());
    ASSERT_TRUE(lost.final_state.has_value());
    EXPECT_EQ(entity(*lost.final_state, 1).bottom, Height{0});
    EXPECT_EQ(lost.final_state->outcome, Outcome::lost);
    ASSERT_EQ(lost.ticks.size(), 1U);
    EXPECT_EQ(lost.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {0, 0}, {0, 0}, Height{2}, Height{0}, MovementCause::fall},
                  LevelLostEvent{},
              }));
    EXPECT_EQ(casualty.move(Direction::east).status, MoveStatus::level_terminal);
    EXPECT_EQ(casualty.rewind().status, RewindStatus::history_empty);
}

TEST(Falling, PushesOverALedgeThenFallsAndArmsABarrel)
{
    LevelDefinition level =
        flat_line({2, 2, 0}, Entity{1, EntityKind::player, {0, 0}, Height{4}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {1, 0}, Height{4}});
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    const MoveResult moved = engine.move(Direction::east);

    ASSERT_TRUE(moved.accepted());
    ASSERT_EQ(moved.ticks.size(), 2U);
    EXPECT_EQ(moved.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {0, 0}, {1, 0}, Height{4}, Height{4}, MovementCause::player},
                  EntityMovedEvent{8, {1, 0}, {2, 0}, Height{4}, Height{4}, MovementCause::player},
              }));
    EXPECT_EQ(moved.ticks[1].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{8, {2, 0}, {2, 0}, Height{4}, Height{0}, MovementCause::fall},
                  BarrelArmedEvent{8},
              }));
    ASSERT_TRUE(moved.final_state.has_value());
    EXPECT_EQ(moved.final_state->armed_barrels, std::vector<EntityId>{8});
    EXPECT_EQ(entity(*moved.final_state, 8).bottom, Height{0});

    const MoveResult first = moved;
    ASSERT_EQ(engine.rewind().status, RewindStatus::rewound);
    EXPECT_TRUE(engine.resolved_state()->armed_barrels.empty());
    EXPECT_EQ(engine.move(Direction::east), first);
}

TEST(Falling, InitializationAndReplacementAreOrderIndependentAndIsolated)
{
    LevelDefinition ordered = flat_line({0, 0}, Entity{50, EntityKind::player, {1, 0}, Height{0}});
    ordered.entities.push_back(Entity{4, EntityKind::barrel, {0, 0}, Height{6}});
    LevelDefinition reversed = ordered;
    std::reverse(reversed.cells.begin(), reversed.cells.end());
    std::reverse(reversed.entities.begin(), reversed.entities.end());

    Engine first;
    Engine second;
    const LoadResult first_load = first.load_level(ordered);
    const LoadResult second_load = second.load_level(reversed);
    EXPECT_EQ(first_load, second_load);

    LevelDefinition replacement = flat_line({0}, Entity{99, EntityKind::player, {0, 0}, Height{0}});
    const LoadResult replaced = first.load_level(replacement);
    ASSERT_TRUE(replaced.accepted());
    EXPECT_TRUE(replaced.ticks.empty());
    ASSERT_TRUE(replaced.final_state.has_value());
    EXPECT_TRUE(replaced.final_state->armed_barrels.empty());
    EXPECT_EQ(first.rewind().status, RewindStatus::history_empty);
}

} // namespace
