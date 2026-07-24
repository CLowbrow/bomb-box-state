#include "../../src/explosions.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>

namespace {

using namespace game_rules;

TEST(ExplosionWavePlanner, ResolvesSimultaneouslyReadySourcesDeterministically) {
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

    const auto wave = detail::resolve_explosion_wave_tick(level, state, 0);
    ASSERT_TRUE(wave.has_value());
    EXPECT_EQ(wave->events, (std::vector<GameplayEvent>{
                                BarrelExplodedEvent{8, {0, 0}, Height{0}},
                                BarrelExplodedEvent{4, {2, 0}, Height{0}},
                            }));
    EXPECT_TRUE(wave->state_after.armed_barrels.empty());

    ResolvedState reversed = state;
    std::reverse(reversed.entities.begin(), reversed.entities.end());
    std::reverse(reversed.armed_barrels.begin(), reversed.armed_barrels.end());
    const auto reversed_wave = detail::resolve_explosion_wave_tick(level, reversed, 0);
    ASSERT_TRUE(reversed_wave.has_value());
    EXPECT_EQ(*reversed_wave, *wave);
}

} // namespace
