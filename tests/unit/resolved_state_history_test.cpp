#include "bomb_box/engine.hpp"
#include "support/bomb_box_printers.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <optional>

namespace {

using namespace bomb_box;

[[nodiscard]] ResolvedState state_at(const std::int32_t x, const Outcome outcome = Outcome::ongoing)
{
    return ResolvedState{
        {Entity{1, EntityKind::player, Coordinate{x, 0}, Height::from_elevation(0)}},
        outcome,
    };
}

TEST(ResolvedStateHistoryUnit, SupportsRepeatedRewindAndBranching)
{
    detail::ResolvedStateHistory history;
    const ResolvedState initialized = state_at(0);
    const ResolvedState first = state_at(1);
    const ResolvedState abandoned = state_at(2, Outcome::won);
    const ResolvedState branch = state_at(-1, Outcome::lost);

    EXPECT_FALSE(history.commit(first));
    history.reset(initialized);
    EXPECT_TRUE(history.has_current());
    EXPECT_EQ(history.earlier_count(), 0U);
    EXPECT_TRUE(history.commit(first));
    EXPECT_TRUE(history.commit(abandoned));
    EXPECT_EQ(history.current(), std::optional{abandoned});
    EXPECT_EQ(history.earlier_count(), 2U);

    EXPECT_TRUE(history.rewind());
    EXPECT_EQ(history.current(), std::optional{first});
    EXPECT_EQ(history.earlier_count(), 1U);

    EXPECT_TRUE(history.commit(branch));
    EXPECT_EQ(history.current(), std::optional{branch});
    EXPECT_EQ(history.earlier_count(), 2U);
    EXPECT_TRUE(history.rewind());
    EXPECT_EQ(history.current(), std::optional{first});
    EXPECT_TRUE(history.rewind());
    EXPECT_EQ(history.current(), std::optional{initialized});
    EXPECT_FALSE(history.rewind());
    EXPECT_EQ(history.current(), std::optional{initialized});
}

TEST(ResolvedStateHistoryUnit, ResetReplacesCurrentStateAndEarlierHistory)
{
    detail::ResolvedStateHistory history;
    history.reset(state_at(0));
    ASSERT_TRUE(history.commit(state_at(1)));
    history.reset(state_at(10));
    EXPECT_EQ(history.current(), std::optional{state_at(10)});
    EXPECT_EQ(history.earlier_count(), 0U);
    EXPECT_FALSE(history.rewind());
}

} // namespace
