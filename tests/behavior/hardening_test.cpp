#include "game_rules/engine.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <vector>

namespace {

using namespace game_rules;

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

[[nodiscard]] LevelDefinition terminal_line()
{
    LevelDefinition level = flat_grid(
        3, 1, Entity{1, EntityKind::player, {0, 0}, Height{0}});
    level.fixtures.push_back(Fixture{{2, 0}, ExitTeleporter{}});
    return level;
}

TEST(EngineLifecycle, InvalidReplacementPreservesTerminalStateAndCompleteHistory)
{
    Engine engine;
    ASSERT_TRUE(engine.load_level(terminal_line()).accepted());
    ASSERT_TRUE(engine.move(Direction::east).accepted());
    const MoveResult won = engine.move(Direction::east);
    ASSERT_TRUE(won.accepted());
    ASSERT_EQ(won.outcome, std::optional{Outcome::won});
    const auto terminal_state = engine.resolved_state();

    LevelDefinition invalid = terminal_line();
    invalid.entities.clear();
    const LoadResult rejected = engine.load_level(invalid);

    EXPECT_FALSE(rejected.accepted());
    EXPECT_EQ(engine.resolved_state(), terminal_state);
    EXPECT_EQ(engine.move(Direction::west).status, MoveStatus::level_terminal);
    const RewindResult first = engine.rewind();
    ASSERT_TRUE(first.accepted());
    EXPECT_EQ(entity(*first.state, 1).coordinate, (Coordinate{1, 0}));
    const RewindResult second = engine.rewind();
    ASSERT_TRUE(second.accepted());
    EXPECT_EQ(entity(*second.state, 1).coordinate, (Coordinate{0, 0}));
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);
}

TEST(EngineLifecycle, ValidReplacementMatchesFreshInitializationAndClearsOldTerminalHistory)
{
    LevelDefinition replacement = flat_grid(
        4, 2, Entity{1, EntityKind::player, {3, 1}, Height{0}});
    replacement.entities.push_back(Entity{8, EntityKind::barrel, {0, 0}, Height{2}});
    replacement.entities.push_back(Entity{2, EntityKind::box, {1, 0}, Height{0}});

    Engine fresh;
    const LoadResult expected = fresh.load_level(replacement);
    ASSERT_TRUE(expected.accepted());
    ASSERT_FALSE(expected.ticks.empty());

    Engine dirty;
    ASSERT_TRUE(dirty.load_level(terminal_line()).accepted());
    ASSERT_TRUE(dirty.move(Direction::east).accepted());
    ASSERT_TRUE(dirty.move(Direction::east).accepted());
    ASSERT_EQ(dirty.resolved_state()->outcome, Outcome::won);

    EXPECT_EQ(dirty.load_level(replacement), expected);
    EXPECT_EQ(dirty.resolved_state(), fresh.resolved_state());
    EXPECT_EQ(dirty.rewind().status, RewindStatus::history_empty);
}

[[nodiscard]] LevelDefinition mixed_explosion_conflicts()
{
    LevelDefinition level = flat_grid(
        8, 2, Entity{1, EntityKind::player, {2, 1}, Height{0}});
    level.entities.push_back(Entity{8, EntityKind::barrel, {0, 0}, Height{2}});
    level.entities.push_back(Entity{2, EntityKind::box, {1, 0}, Height{0}});
    level.entities.push_back(Entity{3, EntityKind::box, {3, 0}, Height{0}});
    level.entities.push_back(Entity{9, EntityKind::barrel, {4, 0}, Height{2}});
    level.entities.push_back(Entity{6, EntityKind::box, {6, 0}, Height{0}});
    level.entities.push_back(Entity{7, EntityKind::barrel, {7, 0}, Height{2}});
    return level;
}

TEST(SimultaneousConflicts, IndependentBlastCommitsWhileOverlappingArrivalsCancel)
{
    const LevelDefinition level = mixed_explosion_conflicts();
    Engine engine;
    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 2U);
    EXPECT_EQ(loaded.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {0, 0}, Height{0}},
                  BarrelExplodedEvent{9, {4, 0}, Height{0}},
                  BarrelExplodedEvent{7, {7, 0}, Height{0}},
                  EntityMovedEvent{6, {6, 0}, {5, 0}, Height{0}, Height{0},
                                   MovementCause::blast},
              }));
    EXPECT_EQ(entity(*loaded.final_state, 2).coordinate, (Coordinate{1, 0}));
    EXPECT_EQ(entity(*loaded.final_state, 3).coordinate, (Coordinate{3, 0}));
    EXPECT_EQ(entity(*loaded.final_state, 6).coordinate, (Coordinate{5, 0}));

    LevelDefinition reversed = level;
    std::reverse(reversed.cells.begin(), reversed.cells.end());
    std::reverse(reversed.entities.begin(), reversed.entities.end());
    Engine reversed_engine;
    EXPECT_EQ(reversed_engine.load_level(reversed), loaded);

    LevelDefinition reassigned = level;
    std::swap(reassigned.entities[1].id, reassigned.entities[4].id);
    std::swap(reassigned.entities[2].id, reassigned.entities[3].id);
    Engine reassigned_engine;
    const LoadResult reassigned_result = reassigned_engine.load_level(reassigned);
    ASSERT_TRUE(reassigned_result.accepted());
    ASSERT_EQ(reassigned_result.ticks.size(), 2U);
    EXPECT_EQ(reassigned_result.ticks[1].events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{9, {0, 0}, Height{0}},
                  BarrelExplodedEvent{8, {4, 0}, Height{0}},
                  BarrelExplodedEvent{7, {7, 0}, Height{0}},
                  EntityMovedEvent{6, {6, 0}, {5, 0}, Height{0}, Height{0},
                                   MovementCause::blast},
              }));
    EXPECT_EQ(entity(*reassigned_result.final_state, 3).coordinate, (Coordinate{1, 0}));
    EXPECT_EQ(entity(*reassigned_result.final_state, 2).coordinate, (Coordinate{3, 0}));
}

[[nodiscard]] LevelDefinition mixed_slide_conflicts()
{
    LevelDefinition level = flat_grid(
        5, 2, Entity{99, EntityKind::player, {4, 1}, Height{0}});
    level.cells[0].geometry = FlatCell{1};
    level.cells[1].geometry = RampCell{Direction::east, 0};
    level.cells[3].geometry = RampCell{Direction::west, 0};
    level.cells[4].geometry = FlatCell{1};
    level.cells[5].geometry = FlatCell{1};
    level.cells[6].geometry = RampCell{Direction::east, 0};
    level.entities.push_back(Entity{10, EntityKind::box, {1, 0}, Height{1}});
    level.entities.push_back(Entity{11, EntityKind::barrel, {1, 0}, Height{3}});
    level.entities.push_back(Entity{20, EntityKind::box, {3, 0}, Height{1}});
    level.entities.push_back(Entity{21, EntityKind::box, {3, 0}, Height{3}});
    level.entities.push_back(Entity{30, EntityKind::box, {1, 1}, Height{1}});
    return level;
}

TEST(SimultaneousConflicts, IndependentSlideCommitsWhileWholeStackArrivalsCancel)
{
    const LevelDefinition level = mixed_slide_conflicts();
    Engine engine;
    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 1U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{EntityMovedEvent{
                  30, {1, 1}, {2, 1}, Height{1}, Height{0}, MovementCause::slide}}));
    EXPECT_EQ(entity(*loaded.final_state, 10).coordinate, (Coordinate{1, 0}));
    EXPECT_EQ(entity(*loaded.final_state, 11).coordinate, (Coordinate{1, 0}));
    EXPECT_EQ(entity(*loaded.final_state, 20).coordinate, (Coordinate{3, 0}));
    EXPECT_EQ(entity(*loaded.final_state, 21).coordinate, (Coordinate{3, 0}));
    EXPECT_EQ(entity(*loaded.final_state, 30).coordinate, (Coordinate{2, 1}));

    LevelDefinition reversed = level;
    std::reverse(reversed.cells.begin(), reversed.cells.end());
    std::reverse(reversed.entities.begin(), reversed.entities.end());
    Engine reversed_engine;
    EXPECT_EQ(reversed_engine.load_level(reversed), loaded);
}

[[nodiscard]] LevelDefinition rewind_stress_level()
{
    LevelDefinition level = flat_grid(
        7, 3, Entity{1, EntityKind::player, {0, 0}, Height{4}});
    constexpr std::array bottom_elevations{2, 2, 0, 0, -1, -1, -2};
    for (std::size_t x = 0; x < bottom_elevations.size(); ++x) {
        level.cells[x].geometry = FlatCell{bottom_elevations[x]};
    }
    for (std::size_t x = 0; x < 6; ++x) {
        level.cells[7 + x].geometry = FlatCell{2};
    }
    level.cells[13].geometry = FlatCell{0};
    for (std::size_t x = 0; x < 7; ++x) {
        level.cells[14 + x].geometry = FlatCell{2};
    }
    level.fixtures = {
        Fixture{{3, 1}, Switch{SwitchColor::red}},
        Fixture{{4, 1}, Door{SwitchColor::red}},
        Fixture{{6, 2}, ExitTeleporter{}},
    };
    level.entities.push_back(Entity{8, EntityKind::barrel, {1, 0}, Height{4}});
    level.entities.push_back(Entity{9, EntityKind::barrel, {3, 0}, Height{0}});
    level.entities.push_back(Entity{2, EntityKind::box, {5, 0}, Height{-2}});
    level.entities.push_back(Entity{7, EntityKind::barrel, {2, 1}, Height{4}});
    return level;
}

TEST(ResolvedStateHistoryBehavior, RewindsAndExactlyReplaysACompleteMultiTurnRulesSequence)
{
    constexpr std::array commands{
        Direction::east,
        Direction::north,
        Direction::east,
        Direction::east,
        Direction::east,
        Direction::east,
        Direction::north,
        Direction::east,
    };
    Engine engine;
    const LoadResult loaded = engine.load_level(rewind_stress_level());
    ASSERT_TRUE(loaded.accepted());
    ASSERT_TRUE(loaded.ticks.empty());

    std::vector<MoveResult> original_results;
    original_results.reserve(commands.size());
    for (const Direction command : commands) {
        original_results.push_back(engine.move(command));
        ASSERT_TRUE(original_results.back().accepted());
    }
    ASSERT_EQ(engine.resolved_state()->outcome, Outcome::won);

    std::size_t tick_count = 0;
    std::size_t movement_count = 0;
    std::size_t armed_count = 0;
    std::size_t explosion_count = 0;
    std::size_t switch_count = 0;
    std::size_t door_count = 0;
    std::size_t win_count = 0;
    for (const MoveResult& result : original_results) {
        tick_count += result.ticks.size();
        for (const TickResult& tick : result.ticks) {
            for (const GameplayEvent& event : tick.events) {
                movement_count += std::holds_alternative<EntityMovedEvent>(event) ? 1U : 0U;
                armed_count += std::holds_alternative<BarrelArmedEvent>(event) ? 1U : 0U;
                explosion_count += std::holds_alternative<BarrelExplodedEvent>(event) ? 1U : 0U;
                switch_count += std::holds_alternative<SwitchChangedEvent>(event) ? 1U : 0U;
                door_count += (std::holds_alternative<DoorOpenedEvent>(event)
                               || std::holds_alternative<DoorClosedEvent>(event))
                    ? 1U
                    : 0U;
                win_count += std::holds_alternative<LevelWonEvent>(event) ? 1U : 0U;
            }
        }
    }
    EXPECT_EQ(tick_count, 15U);
    EXPECT_EQ(movement_count, 19U);
    EXPECT_EQ(armed_count, 3U);
    EXPECT_EQ(explosion_count, 3U);
    EXPECT_EQ(switch_count, 2U);
    EXPECT_EQ(door_count, 2U);
    EXPECT_EQ(win_count, 1U);

    for (std::size_t step = commands.size(); step > 0; --step) {
        const RewindResult rewound = engine.rewind();
        ASSERT_TRUE(rewound.accepted());
        EXPECT_EQ(rewound.state, original_results[step - 1].initial_state);
        EXPECT_EQ(rewound.events, std::vector<GameplayEvent>{StateRewoundEvent{}});
        EXPECT_EQ(engine.resolved_state(), original_results[step - 1].initial_state);
    }
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);

    for (std::size_t step = 0; step < commands.size(); ++step) {
        EXPECT_EQ(engine.move(commands[step]), original_results[step]);
    }

    for (std::size_t count = 0; count < 3; ++count) {
        ASSERT_TRUE(engine.rewind().accepted());
    }
    EXPECT_EQ(engine.resolved_state(), original_results[5].initial_state);
    const MoveResult branch = engine.move(Direction::north);
    ASSERT_TRUE(branch.accepted());
    ASSERT_TRUE(engine.rewind().accepted());
    EXPECT_EQ(engine.resolved_state(), original_results[5].initial_state);
    for (std::size_t step = 5; step < commands.size(); ++step) {
        EXPECT_EQ(engine.move(commands[step]), original_results[step]);
    }
}

} // namespace
