#include "game_rules/engine.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <optional>

namespace {

using namespace game_rules;

[[nodiscard]] LevelDefinition flat_level(const Coordinate origin = {}, const EntityId player_id = 1)
{
    LevelDefinition level;
    level.coordinates.origin = origin;
    level.width = 3;
    level.height = 1;
    level.cells = {
        Cell{origin, FlatCell{0}},
        Cell{Coordinate{origin.x + 1, origin.y}, FlatCell{0}},
        Cell{Coordinate{origin.x + 2, origin.y}, FlatCell{0}},
    };
    level.entities = {
        Entity{player_id, EntityKind::player, origin, Height::from_elevation(0)},
    };
    return level;
}

TEST(ResolvedStateHistoryBehavior, EstablishesInitializedEngineBoundary)
{
    Engine engine;
    EXPECT_FALSE(engine.resolved_state().has_value());

    const RewindResult before_load = engine.rewind();
    EXPECT_FALSE(before_load.accepted());
    EXPECT_EQ(before_load.status, RewindStatus::history_empty);
    EXPECT_FALSE(before_load.state.has_value());
    EXPECT_TRUE(before_load.events.empty());
    EXPECT_FALSE(before_load.outcome.has_value());

    const LevelDefinition level = flat_level();
    ASSERT_TRUE(engine.load_level(level).accepted());

    const ResolvedState expected{canonicalize_level(level).entities, Outcome::ongoing};
    EXPECT_EQ(engine.resolved_state(), std::optional{expected});

    auto caller_state = engine.resolved_state();
    ASSERT_TRUE(caller_state.has_value());
    caller_state->entities.front().coordinate.x = 99;
    EXPECT_EQ(engine.resolved_state(), std::optional{expected});

    const RewindResult empty = engine.rewind();
    EXPECT_FALSE(empty.accepted());
    EXPECT_EQ(empty.status, RewindStatus::history_empty);
    EXPECT_EQ(empty.state, std::optional{expected});
    EXPECT_TRUE(empty.events.empty());
    EXPECT_EQ(empty.outcome, std::optional{Outcome::ongoing});
    EXPECT_EQ(to_string(empty.status), "history_empty");
    EXPECT_EQ(to_string(RewindStatus::rewound), "rewound");
}

TEST(ResolvedStateHistoryBehavior, ReplacesLevelAndHistoryAtomically)
{
    Engine engine;
    const LevelDefinition first = flat_level();
    ASSERT_TRUE(engine.load_level(first).accepted());
    const auto before_level = engine.loaded_level();
    const auto before_state = engine.resolved_state();

    LevelDefinition invalid = flat_level();
    invalid.entities.clear();
    EXPECT_FALSE(engine.load_level(invalid).accepted());
    EXPECT_EQ(engine.loaded_level(), before_level);
    EXPECT_EQ(engine.resolved_state(), before_state);
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);

    const LevelDefinition replacement = flat_level(Coordinate{20, -3}, 99);
    ASSERT_TRUE(engine.load_level(replacement).accepted());
    const ResolvedState expected{canonicalize_level(replacement).entities, Outcome::ongoing};
    EXPECT_EQ(engine.resolved_state(), std::optional{expected});
    EXPECT_EQ(engine.rewind().status, RewindStatus::history_empty);
}

} // namespace
