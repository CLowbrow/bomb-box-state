#include "game_rules/engine.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <optional>
#include <tuple>
#include <vector>

namespace {

using namespace game_rules;

[[nodiscard]] LevelDefinition flat_grid(const std::uint32_t width,
                                        const std::uint32_t height,
                                        const Coordinate player_coordinate)
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
    level.entities.push_back(
        Entity{17, EntityKind::player, player_coordinate, Height::from_elevation(0)});
    return level;
}

void canonicalize_entities(ResolvedState& state)
{
    std::sort(state.entities.begin(), state.entities.end(),
              [](const Entity& lhs, const Entity& rhs) {
                  return std::tuple{lhs.coordinate.y, lhs.coordinate.x,
                                    lhs.bottom.half_steps, lhs.id}
                      < std::tuple{rhs.coordinate.y, rhs.coordinate.x,
                                   rhs.bottom.half_steps, rhs.id};
              });
}

[[nodiscard]] MoveResult expected_push(const LevelDefinition& level,
                                       const Direction direction,
                                       const EntityId pushed_id,
                                       const Coordinate player_to,
                                       const Coordinate pushed_to)
{
    const LevelDefinition canonical = canonicalize_level(level);
    const ResolvedState initial{canonical.entities, Outcome::ongoing};
    ResolvedState final = initial;
    const auto player = std::find_if(final.entities.begin(), final.entities.end(),
                                     [](const Entity& entity) {
                                         return entity.kind == EntityKind::player;
                                     });
    const auto pushed = std::find_if(final.entities.begin(), final.entities.end(),
                                     [pushed_id](const Entity& entity) {
                                         return entity.id == pushed_id;
                                     });
    const Coordinate player_from = player->coordinate;
    const Coordinate pushed_from = pushed->coordinate;
    const Height player_bottom = player->bottom;
    const Height pushed_bottom = pushed->bottom;
    player->coordinate = player_to;
    pushed->coordinate = pushed_to;
    canonicalize_entities(final);

    return MoveResult{
        MoveStatus::moved,
        direction,
        {},
        initial,
        {TickResult{
            0,
            {
                GameplayEvent{EntityMovedEvent{
                    17,
                    player_from,
                    player_to,
                    player_bottom,
                    player_bottom,
                    MovementCause::player,
                }},
                GameplayEvent{EntityMovedEvent{
                    pushed_id,
                    pushed_from,
                    pushed_to,
                    pushed_bottom,
                    pushed_bottom,
                    MovementCause::player,
                }},
            },
            final,
        }},
        final,
        Outcome::ongoing,
    };
}

[[nodiscard]] MoveResult expected_rejection(const MoveStatus reason,
                                            const Direction direction,
                                            const LevelDefinition& level)
{
    const ResolvedState state{canonicalize_level(level).entities, Outcome::ongoing};
    return MoveResult{
        reason,
        direction,
        {GameplayEvent{MoveBlockedEvent{direction, reason}}},
        std::nullopt,
        {},
        state,
        Outcome::ongoing,
    };
}

TEST(PlayerPush, PushesOneBoxOrBarrelInEveryCardinalDirection)
{
    struct Scenario final {
        Direction direction;
        EntityKind kind;
        Coordinate target;
        Coordinate destination;
    };
    const std::vector<Scenario> scenarios{
        {Direction::north, EntityKind::box, {2, 3}, {2, 4}},
        {Direction::east, EntityKind::barrel, {3, 2}, {4, 2}},
        {Direction::south, EntityKind::box, {2, 1}, {2, 0}},
        {Direction::west, EntityKind::barrel, {1, 2}, {0, 2}},
    };

    for (const Scenario& scenario : scenarios) {
        SCOPED_TRACE(::testing::PrintToString(scenario.direction));
        LevelDefinition level = flat_grid(5, 5, {2, 2});
        level.entities.push_back(
            Entity{4, scenario.kind, scenario.target, Height::from_elevation(0)});
        Engine engine;
        ASSERT_TRUE(engine.load_level(level).accepted());

        const MoveResult result = engine.move(scenario.direction);

        EXPECT_EQ(result, expected_push(level, scenario.direction, 4, scenario.target,
                                        scenario.destination));
        EXPECT_EQ(engine.resolved_state(), result.final_state);
    }
}

TEST(PlayerPush, HonorsDeclaredAxisDirections)
{
    LevelDefinition level = flat_grid(3, 1, {2, 0});
    level.coordinates.positive_x = HorizontalAxisDirection::west;
    level.entities.push_back(Entity{4, EntityKind::box, {1, 0}, Height::from_elevation(0)});
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    EXPECT_EQ(engine.move(Direction::east),
              expected_push(level, Direction::east, 4, {1, 0}, {0, 0}));
}

TEST(PlayerPush, RejectsStackedTargetsAndRecursivePushesAtomically)
{
    LevelDefinition stacked = flat_grid(4, 1, {0, 0});
    stacked.entities.push_back(Entity{4, EntityKind::box, {1, 0}, Height::from_elevation(0)});
    stacked.entities.push_back(Entity{5, EntityKind::barrel, {1, 0}, Height::from_elevation(1)});
    Engine stacked_engine;
    ASSERT_TRUE(stacked_engine.load_level(stacked).accepted());
    EXPECT_EQ(stacked_engine.move(Direction::east),
              expected_rejection(MoveStatus::stacked_push_target, Direction::east, stacked));
    EXPECT_EQ(to_string(MoveStatus::stacked_push_target), "stacked_push_target");
    EXPECT_EQ(stacked_engine.resolved_state(),
              (std::optional{ResolvedState{
                  canonicalize_level(stacked).entities,
                  Outcome::ongoing,
              }}));
    EXPECT_EQ(stacked_engine.rewind().status, RewindStatus::history_empty);

    LevelDefinition occupied = flat_grid(4, 1, {0, 0});
    occupied.entities.push_back(Entity{4, EntityKind::box, {1, 0}, Height::from_elevation(0)});
    occupied.entities.push_back(Entity{5, EntityKind::barrel, {2, 0}, Height::from_elevation(0)});
    Engine occupied_engine;
    ASSERT_TRUE(occupied_engine.load_level(occupied).accepted());
    EXPECT_EQ(occupied_engine.move(Direction::east),
              expected_rejection(MoveStatus::occupied, Direction::east, occupied));
    EXPECT_EQ(occupied_engine.rewind().status, RewindStatus::history_empty);
}

TEST(PlayerPush, RejectsWorldAndDeferredRuleBoundariesWithoutPartialMovement)
{
    LevelDefinition boundary = flat_grid(2, 1, {0, 0});
    boundary.entities.push_back(Entity{4, EntityKind::box, {1, 0}, Height::from_elevation(0)});
    Engine boundary_engine;
    ASSERT_TRUE(boundary_engine.load_level(boundary).accepted());
    EXPECT_EQ(boundary_engine.move(Direction::east),
              expected_rejection(MoveStatus::world_boundary, Direction::east, boundary));

    LevelDefinition higher = flat_grid(3, 1, {0, 0});
    higher.entities.push_back(Entity{4, EntityKind::box, {1, 0}, Height::from_elevation(0)});
    higher.cells[2].geometry = FlatCell{1};
    Engine higher_engine;
    ASSERT_TRUE(higher_engine.load_level(higher).accepted());
    EXPECT_EQ(higher_engine.move(Direction::east),
              expected_rejection(MoveStatus::ledge, Direction::east, higher));

    LevelDefinition ramp = flat_grid(4, 1, {0, 0});
    ramp.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, FlatCell{0}},
        Cell{{2, 0}, RampCell{Direction::west, 0}},
        Cell{{3, 0}, FlatCell{1}},
    };
    ramp.entities.push_back(Entity{4, EntityKind::box, {1, 0}, Height::from_elevation(0)});
    Engine ramp_engine;
    ASSERT_TRUE(ramp_engine.load_level(ramp).accepted());
    EXPECT_EQ(ramp_engine.move(Direction::east),
              expected_rejection(MoveStatus::unsupported_geometry, Direction::east, ramp));

    LevelDefinition ramp_target;
    ramp_target.width = 3;
    ramp_target.height = 1;
    ramp_target.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, RampCell{Direction::west, 0}},
        Cell{{2, 0}, FlatCell{1}},
    };
    ramp_target.entities = {
        Entity{17, EntityKind::player, {0, 0}, Height::from_elevation(0)},
        Entity{4, EntityKind::box, {1, 0}, Height{1}},
    };
    Engine ramp_target_engine;
    ASSERT_TRUE(ramp_target_engine.load_level(ramp_target).accepted());
    EXPECT_EQ(ramp_target_engine.move(Direction::east),
              expected_rejection(MoveStatus::unsupported_geometry, Direction::east,
                                 ramp_target));

}

TEST(PlayerPush, PushesAtThePlayersCurrentHeight)
{
    LevelDefinition level = flat_grid(3, 1, {0, 0});
    level.cells[1].geometry = FlatCell{1};
    level.cells[2].geometry = FlatCell{1};
    level.entities[0].bottom = Height::from_elevation(1);
    level.entities.push_back(Entity{3, EntityKind::box, {0, 0}, Height::from_elevation(0)});
    level.entities.push_back(Entity{4, EntityKind::box, {1, 0}, Height::from_elevation(1)});
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    EXPECT_EQ(engine.move(Direction::east),
              expected_push(level, Direction::east, 4, {1, 0}, {2, 0}));
}

TEST(PlayerPush, RewindRestoresTheAtomicPrePushStateAndAllowsDeterministicReplay)
{
    LevelDefinition level = flat_grid(4, 1, {0, 0});
    level.entities.push_back(Entity{4, EntityKind::barrel, {1, 0}, Height::from_elevation(0)});
    const ResolvedState initial{canonicalize_level(level).entities, Outcome::ongoing};
    Engine engine;
    ASSERT_TRUE(engine.load_level(level).accepted());

    const MoveResult first = engine.move(Direction::east);
    ASSERT_TRUE(first.accepted());
    const RewindResult rewind = engine.rewind();
    EXPECT_EQ(rewind.state, std::optional{initial});
    EXPECT_EQ(rewind.events, std::vector<GameplayEvent>{StateRewoundEvent{}});
    EXPECT_EQ(engine.move(Direction::east), first);
}

TEST(PlayerPush, IsIndependentOfSuppliedContainerOrder)
{
    LevelDefinition ordered = flat_grid(3, 1, {0, 0});
    ordered.entities.push_back(
        Entity{4, EntityKind::box, {1, 0}, Height::from_elevation(0)});
    LevelDefinition reversed = ordered;
    std::reverse(reversed.cells.begin(), reversed.cells.end());
    std::reverse(reversed.entities.begin(), reversed.entities.end());

    Engine first;
    Engine second;
    ASSERT_TRUE(first.load_level(ordered).accepted());
    ASSERT_TRUE(second.load_level(reversed).accepted());

    EXPECT_EQ(first.move(Direction::east), second.move(Direction::east));
}

} // namespace
