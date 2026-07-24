#include "../../src/explosions.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>

namespace {

using namespace game_rules;

TEST(SingleExplosionPlanner, DoesNotChooseBetweenSimultaneouslyReadySources)
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
            Entity{1, EntityKind::player, {1, 0}, Height{2}},
            Entity{4, EntityKind::barrel, {2, 0}, Height{0}},
        },
        Outcome::ongoing,
        {4, 8},
    };

    EXPECT_FALSE(detail::resolve_single_explosion_tick(level, state, 0).has_value());

    ResolvedState reversed = state;
    std::reverse(reversed.entities.begin(), reversed.entities.end());
    std::reverse(reversed.armed_barrels.begin(), reversed.armed_barrels.end());
    EXPECT_FALSE(detail::resolve_single_explosion_tick(level, reversed, 0).has_value());
}

} // namespace
