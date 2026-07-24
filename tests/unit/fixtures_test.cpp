#include "../../src/fixtures.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <vector>

namespace {

using namespace game_rules;

TEST(FixtureResolution, AWinRemovesTheSameTicksLossAndTakesTerminalPrecedence)
{
    LevelDefinition level;
    level.width = 1;
    level.height = 1;
    level.cells = {Cell{{0, 0}, FlatCell{0}}};
    level.fixtures = {Fixture{{0, 0}, ExitTeleporter{}}};
    level.entities = {Entity{1, EntityKind::player, {0, 0}, Height{0}}};
    TickResult tick{
        4,
        {GameplayEvent{PlayerCrushedEvent{1, 2}}, GameplayEvent{LevelLostEvent{}}},
        ResolvedState{level.entities, Outcome::lost},
    };

    detail::resolve_fixtures_after_tick(level, tick);

    EXPECT_EQ(tick.events, (std::vector<GameplayEvent>{LevelWonEvent{}}));
    EXPECT_EQ(tick.state_after.outcome, Outcome::won);
}

} // namespace
