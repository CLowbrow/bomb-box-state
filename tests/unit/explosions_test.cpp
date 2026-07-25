#include "../../src/explosions.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>

namespace {

using namespace game_rules;

TEST(ExplosionWavePlanner, DetonatesSimultaneouslyReadySourcesInSpatialOrder)
{
    LevelDefinition level;
    level.width = 3;
    level.height = 1;
    level.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, FlatCell{0}},
        Cell{{2, 0}, FlatCell{0}},
    };
    const ResolvedState state{
        {
            Entity{8, EntityKind::barrel, {0, 0}, Height{0}},
            Entity{4, EntityKind::barrel, {0, 0}, Height{2}},
            Entity{1, EntityKind::player, {2, 0}, Height{0}},
        },
        Outcome::ongoing,
        {4, 8},
    };

    const auto tick = detail::resolve_explosion_wave_tick(level, state, 0);
    ASSERT_TRUE(tick.has_value());
    EXPECT_EQ(tick->events,
              (std::vector<GameplayEvent>{
                  BarrelExplodedEvent{8, {0, 0}, Height{0}},
                  BarrelExplodedEvent{4, {0, 0}, Height{2}},
              }));
    EXPECT_EQ(tick->state_after.entities,
              (std::vector<Entity>{
                  Entity{1, EntityKind::player, {2, 0}, Height{0}},
              }));
    EXPECT_TRUE(tick->state_after.armed_barrels.empty());

    ResolvedState reversed = state;
    std::reverse(reversed.entities.begin(), reversed.entities.end());
    EXPECT_EQ(detail::resolve_explosion_wave_tick(level, reversed, 0), tick);
}

} // namespace
