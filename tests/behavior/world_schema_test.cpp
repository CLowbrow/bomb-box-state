#include "game_rules/engine.hpp"
#include "game_rules/world.hpp"
#include "support/game_rules_printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <optional>

namespace {

using namespace game_rules;

[[nodiscard]] bool contains(const ValidationResult& result, const ValidationErrorCode code)
{
    return std::any_of(result.errors.begin(), result.errors.end(), [code](const ValidationError& error) {
        return error.code == code;
    });
}

[[nodiscard]] bool contains(const LoadResult& result, const ValidationErrorCode code)
{
    return std::any_of(result.errors.begin(), result.errors.end(), [code](const ValidationError& error) {
        return error.code == code;
    });
}

[[nodiscard]] LevelDefinition flat_level(const Coordinate origin = {})
{
    LevelDefinition level;
    level.coordinates.origin = origin;
    level.width = 2;
    level.height = 1;
    level.cells = {
        Cell{origin, FlatCell{0}},
        Cell{Coordinate{origin.x + 1, origin.y}, FlatCell{0}},
    };
    level.entities = {
        Entity{1, EntityKind::player, origin, Height::from_elevation(0)},
    };
    return level;
}

[[nodiscard]] LevelDefinition ramp_level()
{
    LevelDefinition level;
    level.coordinates = CoordinateSystem{
        Coordinate{10, -4},
        HorizontalAxisDirection::west,
        VerticalAxisDirection::south,
    };
    level.width = 3;
    level.height = 1;
    level.cells = {
        Cell{Coordinate{12, -4}, FlatCell{1}},
        Cell{Coordinate{10, -4}, FlatCell{0}},
        Cell{Coordinate{11, -4}, RampCell{Direction::east, 0}},
    };
    level.fixtures = {
        Fixture{Coordinate{10, -4}, Switch{SwitchColor::blue}},
    };
    level.entities = {
        Entity{30, EntityKind::player, Coordinate{12, -4}, Height{4}},
        Entity{20, EntityKind::box, Coordinate{12, -4}, Height{2}},
        Entity{40, EntityKind::barrel, Coordinate{10, -4}, Height{6}},
    };
    return level;
}

TEST(WorldSchema, AcceptsValidSchemaStacksAndDeferredStabilization)
{
    EXPECT_TRUE(validate_level(ramp_level()).valid());

    LevelDefinition ramp_chain;
    ramp_chain.width = 4;
    ramp_chain.height = 1;
    ramp_chain.cells = {
        Cell{{0, 0}, FlatCell{0}},
        Cell{{1, 0}, RampCell{Direction::west, 0}},
        Cell{{2, 0}, RampCell{Direction::west, 1}},
        Cell{{3, 0}, FlatCell{2}},
    };
    ramp_chain.entities = {
        Entity{1, EntityKind::player, {0, 0}, Height{0}},
    };
    EXPECT_TRUE(validate_level(ramp_chain).valid());

    LevelDefinition unsupported = flat_level();
    unsupported.entities.push_back(
        Entity{2, EntityKind::box, Coordinate{1, 0}, Height::from_elevation(3)});
    EXPECT_TRUE(validate_level(unsupported).valid());
}

TEST(WorldSchema, RejectsInvalidStructure)
{
    {
        LevelDefinition level = flat_level();
        level.width = 0;
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::invalid_dimensions));
    }
    {
        LevelDefinition level = flat_level();
        level.coordinates.positive_x = static_cast<HorizontalAxisDirection>(99);
        EXPECT_TRUE(contains(validate_level(level),
                             ValidationErrorCode::invalid_coordinate_system));
    }
    {
        LevelDefinition level = flat_level();
        level.cells.pop_back();
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::cell_count_mismatch));
    }
    {
        LevelDefinition level = flat_level();
        level.cells[1].coordinate = level.cells[0].coordinate;
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::duplicate_cell));
    }
    {
        LevelDefinition level = flat_level();
        level.entities.front().id = 0;
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::invalid_entity_id));
    }
    {
        LevelDefinition level = flat_level();
        level.entities.push_back(Entity{1, EntityKind::box, Coordinate{1, 0}, Height{0}});
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::duplicate_entity_id));
    }
    {
        LevelDefinition level = flat_level();
        level.entities.front().kind = EntityKind::box;
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::player_count_not_one));
    }
    {
        LevelDefinition level = flat_level();
        level.entities.push_back(Entity{2, EntityKind::box, Coordinate{0, 0}, Height{1}});
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::overlapping_entities));
    }
    {
        LevelDefinition level = flat_level();
        level.entities.push_back(Entity{2, EntityKind::box, Coordinate{0, 0}, Height{2}});
        EXPECT_TRUE(contains(validate_level(level),
                             ValidationErrorCode::player_not_top_of_stack));
    }
    {
        LevelDefinition level = flat_level();
        level.cells[0].geometry = FlatCell{1};
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::entity_below_surface));
    }
    {
        LevelDefinition level = ramp_level();
        level.cells[2].geometry = RampCell{Direction::east, -1'073'741'825};
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::invalid_cell_height));
    }
    {
        LevelDefinition level = ramp_level();
        level.fixtures.front().coordinate = Coordinate{11, -4};
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::fixture_on_ramp));
    }
    {
        LevelDefinition level = flat_level();
        level.fixtures = {
            Fixture{Coordinate{1, 0}, Switch{SwitchColor::red}},
            Fixture{Coordinate{1, 0}, Door{SwitchColor::red}},
        };
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::duplicate_fixture));
    }
    {
        LevelDefinition level = flat_level();
        level.fixtures = {Fixture{Coordinate{1, 0}, ExitTeleporter{}}};
        level.entities.push_back(Entity{2, EntityKind::box, Coordinate{1, 0}, Height{0}});
        EXPECT_TRUE(contains(validate_level(level),
                             ValidationErrorCode::invalid_teleporter_occupancy));
    }
    {
        LevelDefinition level = ramp_level();
        level.cells[0].geometry = FlatCell{2};
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::invalid_ramp_endpoints));
    }
    {
        LevelDefinition level;
        level.width = 4;
        level.height = 1;
        level.cells = {
            Cell{{0, 0}, FlatCell{0}},
            Cell{{1, 0}, RampCell{Direction::west, 0}},
            Cell{{2, 0}, RampCell{Direction::west, 2}},
            Cell{{3, 0}, FlatCell{3}},
        };
        level.entities = {
            Entity{1, EntityKind::player, {0, 0}, Height{0}},
        };
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::invalid_ramp_endpoints));
    }
    {
        LevelDefinition level;
        level.width = 4;
        level.height = 1;
        level.cells = {
            Cell{{0, 0}, FlatCell{1}},
            Cell{{1, 0}, RampCell{Direction::east, 0}},
            Cell{{2, 0}, RampCell{Direction::west, 0}},
            Cell{{3, 0}, FlatCell{1}},
        };
        level.entities = {
            Entity{1, EntityKind::player, {0, 0}, Height{2}},
        };
        EXPECT_TRUE(contains(validate_level(level), ValidationErrorCode::invalid_ramp_endpoints));
    }
}

TEST(WorldSchema, CanonicalizesAndReplacesLevelAtomically)
{
    LevelDefinition first = ramp_level();
    LevelDefinition reordered = first;
    std::reverse(reordered.cells.begin(), reordered.cells.end());
    std::reverse(reordered.entities.begin(), reordered.entities.end());

    Engine first_engine;
    Engine second_engine;
    ASSERT_TRUE(first_engine.load_level(first).accepted());
    ASSERT_TRUE(second_engine.load_level(reordered).accepted());
    EXPECT_EQ(first_engine.loaded_level(), second_engine.loaded_level());

    auto caller_snapshot = first_engine.loaded_level();
    ASSERT_TRUE(caller_snapshot.has_value());
    caller_snapshot->entities.front().id = 999;
    EXPECT_NE(first_engine.loaded_level()->entities.front().id, 999U);

    const auto before_rejection = first_engine.loaded_level();
    LevelDefinition invalid = flat_level();
    invalid.entities.clear();
    const LoadResult rejected = first_engine.load_level(invalid);
    EXPECT_FALSE(rejected.accepted());
    EXPECT_TRUE(contains(rejected, ValidationErrorCode::player_count_not_one));
    EXPECT_EQ(first_engine.loaded_level(), before_rejection);

    const LevelDefinition second = flat_level(Coordinate{-7, 5});
    ASSERT_TRUE(first_engine.load_level(second).accepted());
    EXPECT_EQ(first_engine.loaded_level(), std::optional{canonicalize_level(second)});
}

} // namespace
