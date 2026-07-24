#include "../../src/gravity.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <optional>
#include <vector>

namespace {

using namespace game_rules;

TEST(GravityPlanner, AWouldBeLandingOnThePlayerCrushesWithoutCreatingAnIllegalStack)
{
    LevelDefinition level;
    level.width = 1;
    level.height = 1;
    level.cells = {Cell{{0, 0}, FlatCell{0}}};
    level.entities = {Entity{1, EntityKind::player, {0, 0}, Height{0}}};
    const ResolvedState runtime_state{
        {
            Entity{1, EntityKind::player, {0, 0}, Height{0}},
            Entity{2, EntityKind::box, {0, 0}, Height{4}},
        },
        Outcome::ongoing,
    };

    const std::optional<TickResult> tick = detail::resolve_falling_tick(level, runtime_state, 3);

    ASSERT_TRUE(tick.has_value());
    EXPECT_EQ(tick->index, 3U);
    EXPECT_EQ(tick->events,
              (std::vector<GameplayEvent>{PlayerCrushedEvent{1, 2}, LevelLostEvent{}}));
    EXPECT_EQ(tick->state_after, (ResolvedState{
                                     {
                                         Entity{1, EntityKind::player, {0, 0}, Height{0}},
                                         Entity{2, EntityKind::box, {0, 0}, Height{4}},
                                     },
                                     Outcome::lost,
                                 }));
}

} // namespace
