#include "game_rules/engine.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <vector>

namespace {

using namespace game_rules;

[[nodiscard]] LevelDefinition flat_line(const std::uint32_t width,
                                        const Coordinate player_coordinate = {0, 0})
{
    LevelDefinition level;
    level.width = width;
    level.height = 1;
    for (std::uint32_t x = 0; x < width; ++x) {
        level.cells.push_back(Cell{
            Coordinate{static_cast<std::int32_t>(x), 0},
            FlatCell{0},
        });
    }
    level.entities.push_back(Entity{1, EntityKind::player, player_coordinate, Height{0}});
    return level;
}

[[nodiscard]] const Entity& entity(const ResolvedState& state, const EntityId id)
{
    const auto found = std::find_if(state.entities.begin(), state.entities.end(),
                                    [id](const Entity& value) { return value.id == id; });
    EXPECT_NE(found, state.entities.end());
    return *found;
}

TEST(Fixtures, InitializationDerivesAndSwitchesAndEffectivelyOpenDoors)
{
    LevelDefinition level = flat_line(7, {6, 0});
    level.fixtures = {
        Fixture{{0, 0}, Switch{SwitchColor::red}},
        Fixture{{1, 0}, Switch{SwitchColor::red}},
        Fixture{{2, 0}, Door{SwitchColor::red}},
        Fixture{{3, 0}, Door{SwitchColor::green}},
    };
    level.entities.push_back(Entity{2, EntityKind::box, {0, 0}, Height{0}});
    level.entities.push_back(Entity{3, EntityKind::barrel, {1, 0}, Height{0}});
    level.entities.push_back(Entity{4, EntityKind::box, {3, 0}, Height{0}});

    Engine engine;
    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 1U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  SwitchChangedEvent{SwitchColor::red, true},
                  DoorOpenedEvent{{2, 0}, SwitchColor::red},
                  DoorOpenedEvent{{3, 0}, SwitchColor::green},
              }));
    ASSERT_TRUE(loaded.final_state.has_value());
    EXPECT_EQ(loaded.final_state->active_switch_colors,
              std::vector<SwitchColor>{SwitchColor::red});
    EXPECT_EQ(loaded.final_state->open_doors,
              (std::vector<Coordinate>{{2, 0}, {3, 0}}));
    EXPECT_EQ(loaded.ticks[0].state_after, *loaded.final_state);

    LevelDefinition reordered = level;
    std::reverse(reordered.cells.begin(), reordered.cells.end());
    std::reverse(reordered.fixtures.begin(), reordered.fixtures.end());
    std::reverse(reordered.entities.begin(), reordered.entities.end());
    Engine reordered_engine;
    EXPECT_EQ(reordered_engine.load_level(reordered), loaded);
}

TEST(Fixtures, AColorIsInactiveUntilEverySwitchOfThatColorIsPressed)
{
    LevelDefinition level = flat_line(4, {3, 0});
    level.fixtures = {
        Fixture{{0, 0}, Switch{SwitchColor::red}},
        Fixture{{1, 0}, Switch{SwitchColor::red}},
        Fixture{{2, 0}, Door{SwitchColor::red}},
    };
    level.entities.push_back(Entity{2, EntityKind::box, {0, 0}, Height{0}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    EXPECT_TRUE(loaded.ticks.empty());
    EXPECT_TRUE(loaded.final_state->active_switch_colors.empty());
    EXPECT_TRUE(loaded.final_state->open_doors.empty());
    EXPECT_EQ(engine.move(Direction::west).status, MoveStatus::closed_door);
}

TEST(Fixtures, WalkingOffASwitchClosesItsDoorAndRewindRestoresPassability)
{
    LevelDefinition level = flat_line(4);
    level.fixtures = {
        Fixture{{0, 0}, Switch{SwitchColor::blue}},
        Fixture{{2, 0}, Door{SwitchColor::blue}},
    };
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    const MoveResult first = engine.move(Direction::east);

    ASSERT_TRUE(first.accepted());
    ASSERT_EQ(first.ticks.size(), 1U);
    EXPECT_EQ(first.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {0, 0}, {1, 0}, Height{0}, Height{0}, MovementCause::player},
                  SwitchChangedEvent{SwitchColor::blue, false},
                  DoorClosedEvent{{2, 0}, SwitchColor::blue},
              }));
    EXPECT_EQ(first.final_state->active_switch_colors, std::vector<SwitchColor>{});
    EXPECT_EQ(first.final_state->open_doors, std::vector<Coordinate>{});
    EXPECT_EQ(engine.move(Direction::east).status, MoveStatus::closed_door);
    EXPECT_EQ(to_string(MoveStatus::closed_door), "closed_door");

    const RewindResult rewound = engine.rewind();
    ASSERT_TRUE(rewound.accepted());
    EXPECT_EQ(rewound.state->active_switch_colors,
              std::vector<SwitchColor>{SwitchColor::blue});
    EXPECT_EQ(rewound.state->open_doors, (std::vector<Coordinate>{{2, 0}}));
    EXPECT_EQ(engine.move(Direction::east), first);
}

TEST(Fixtures, AnInactiveOccupiedDoorStaysOpenUntilItsCellBecomesEmpty)
{
    LevelDefinition level = flat_line(3, {1, 0});
    level.fixtures = {Fixture{{1, 0}, Door{SwitchColor::yellow}}};
    Engine engine;
    const LoadResult loaded = engine.load_level(level);
    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 1U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{DoorOpenedEvent{{1, 0}, SwitchColor::yellow}}));

    const MoveResult vacated = engine.move(Direction::east);
    ASSERT_TRUE(vacated.accepted());
    EXPECT_EQ(vacated.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {1, 0}, {2, 0}, Height{0}, Height{0}, MovementCause::player},
                  DoorClosedEvent{{1, 0}, SwitchColor::yellow},
              }));
    EXPECT_EQ(engine.move(Direction::west).status, MoveStatus::closed_door);
}

TEST(Fixtures, PushesCanPressSwitchesAndUseHeldOpenDoorsButCannotEnterTeleporters)
{
    LevelDefinition closed = flat_line(3);
    closed.entities.push_back(Entity{2, EntityKind::box, {1, 0}, Height{0}});
    closed.fixtures = {Fixture{{2, 0}, Door{SwitchColor::green}}};
    Engine closed_engine;
    ASSERT_TRUE(closed_engine.load_level(closed).accepted());
    EXPECT_EQ(closed_engine.move(Direction::east).status, MoveStatus::closed_door);

    LevelDefinition level = flat_line(5);
    level.entities.push_back(Entity{2, EntityKind::box, {1, 0}, Height{0}});
    level.fixtures = {
        Fixture{{2, 0}, Switch{SwitchColor::green}},
        Fixture{{3, 0}, Door{SwitchColor::green}},
        Fixture{{4, 0}, ExitTeleporter{}},
    };
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    const MoveResult pressed = engine.move(Direction::east);
    ASSERT_TRUE(pressed.accepted());
    EXPECT_EQ(pressed.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {0, 0}, {1, 0}, Height{0}, Height{0}, MovementCause::player},
                  EntityMovedEvent{2, {1, 0}, {2, 0}, Height{0}, Height{0}, MovementCause::player},
                  SwitchChangedEvent{SwitchColor::green, true},
                  DoorOpenedEvent{{3, 0}, SwitchColor::green},
              }));

    const MoveResult entered_door = engine.move(Direction::east);
    ASSERT_TRUE(entered_door.accepted());
    EXPECT_EQ(entered_door.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {1, 0}, {2, 0}, Height{0}, Height{0}, MovementCause::player},
                  EntityMovedEvent{2, {2, 0}, {3, 0}, Height{0}, Height{0}, MovementCause::player},
              }));
    EXPECT_EQ(entered_door.final_state->open_doors,
              (std::vector<Coordinate>{{3, 0}}));

    const MoveResult left_switch = engine.move(Direction::west);
    ASSERT_TRUE(left_switch.accepted());
    EXPECT_EQ(left_switch.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {2, 0}, {1, 0}, Height{0}, Height{0}, MovementCause::player},
                  SwitchChangedEvent{SwitchColor::green, false},
              }));
    EXPECT_EQ(left_switch.final_state->open_doors,
              (std::vector<Coordinate>{{3, 0}}));
    ASSERT_TRUE(engine.move(Direction::east).accepted());

    const ResolvedState before_rejection = *engine.resolved_state();
    const MoveResult rejected = engine.move(Direction::east);
    EXPECT_EQ(rejected.status, MoveStatus::teleporter_restriction);
    EXPECT_EQ(to_string(MoveStatus::teleporter_restriction), "teleporter_restriction");
    EXPECT_EQ(rejected.final_state, std::optional{before_rejection});
    EXPECT_TRUE(rejected.ticks.empty());
}

TEST(Fixtures, GravityCanPressASwitchAndOpenADoorInTheSameDerivedTick)
{
    LevelDefinition level = flat_line(4, {3, 0});
    level.fixtures = {
        Fixture{{0, 0}, Switch{SwitchColor::red}},
        Fixture{{2, 0}, Door{SwitchColor::red}},
    };
    level.entities.push_back(Entity{2, EntityKind::box, {0, 0}, Height{2}});
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 1U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{2, {0, 0}, {0, 0}, Height{2}, Height{0}, MovementCause::fall},
                  SwitchChangedEvent{SwitchColor::red, true},
                  DoorOpenedEvent{{2, 0}, SwitchColor::red},
              }));
    EXPECT_EQ(entity(*loaded.final_state, 2).bottom, Height{0});
}

TEST(Fixtures, FixtureChangesPrecedeATerminalLossFromTheSameGravityTick)
{
    LevelDefinition level = flat_line(4, {3, 0});
    level.entities.front().bottom = Height{2};
    level.entities.push_back(Entity{2, EntityKind::box, {0, 0}, Height{2}});
    level.fixtures = {
        Fixture{{0, 0}, Switch{SwitchColor::red}},
        Fixture{{2, 0}, Door{SwitchColor::red}},
    };
    Engine engine;

    const LoadResult loaded = engine.load_level(level);

    ASSERT_TRUE(loaded.accepted());
    ASSERT_EQ(loaded.ticks.size(), 1U);
    EXPECT_EQ(loaded.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{2, {0, 0}, {0, 0}, Height{2}, Height{0}, MovementCause::fall},
                  EntityMovedEvent{1, {3, 0}, {3, 0}, Height{2}, Height{0}, MovementCause::fall},
                  SwitchChangedEvent{SwitchColor::red, true},
                  DoorOpenedEvent{{2, 0}, SwitchColor::red},
                  LevelLostEvent{},
              }));
    EXPECT_EQ(loaded.outcome, std::optional{Outcome::lost});
}

TEST(Fixtures, TeleporterWinsAreTerminalRewindableAndDeterministic)
{
    LevelDefinition level = flat_line(3);
    level.fixtures = {Fixture{{1, 0}, ExitTeleporter{}}};
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    const MoveResult won = engine.move(Direction::east);

    ASSERT_TRUE(won.accepted());
    ASSERT_EQ(won.ticks.size(), 1U);
    EXPECT_EQ(won.ticks[0].events,
              (std::vector<GameplayEvent>{
                  EntityMovedEvent{1, {0, 0}, {1, 0}, Height{0}, Height{0}, MovementCause::player},
                  LevelWonEvent{},
              }));
    EXPECT_EQ(won.outcome, std::optional{Outcome::won});
    EXPECT_EQ(won.final_state->outcome, Outcome::won);
    EXPECT_EQ(engine.move(Direction::east).status, MoveStatus::level_terminal);

    const RewindResult rewound = engine.rewind();
    ASSERT_TRUE(rewound.accepted());
    EXPECT_EQ(rewound.outcome, std::optional{Outcome::ongoing});
    EXPECT_EQ(engine.move(Direction::east), won);
}

TEST(Fixtures, InitializationCanWinAndReplacementClearsTheTerminalLevel)
{
    LevelDefinition won_level = flat_line(1);
    won_level.fixtures = {Fixture{{0, 0}, ExitTeleporter{}}};
    Engine engine;

    const LoadResult won = engine.load_level(won_level);

    ASSERT_TRUE(won.accepted());
    EXPECT_EQ(won.initial_state->outcome, Outcome::ongoing);
    ASSERT_EQ(won.ticks.size(), 1U);
    EXPECT_EQ(won.ticks[0].events,
              (std::vector<GameplayEvent>{LevelWonEvent{}}));
    EXPECT_EQ(won.outcome, std::optional{Outcome::won});
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);

    const LoadResult replaced = engine.load_level(flat_line(2));
    ASSERT_TRUE(replaced.accepted());
    EXPECT_EQ(replaced.outcome, std::optional{Outcome::ongoing});
    EXPECT_TRUE(replaced.ticks.empty());
    EXPECT_TRUE(replaced.final_state->active_switch_colors.empty());
    EXPECT_TRUE(replaced.final_state->open_doors.empty());
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);
}

TEST(Fixtures, TeleporterEntryRequiresThePlayerAtItsFloorHeight)
{
    LevelDefinition level = flat_line(2);
    level.cells[0].geometry = FlatCell{1};
    level.entities.front().bottom = Height{2};
    level.fixtures = {Fixture{{1, 0}, ExitTeleporter{}}};
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    EXPECT_EQ(engine.move(Direction::east).status, MoveStatus::teleporter_restriction);
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);
}

} // namespace
