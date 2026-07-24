#include "bomb_box/engine.hpp"
#include "bomb_box/world.hpp"

#include <algorithm>
#include <iostream>
#include <string_view>

namespace {

using namespace bomb_box;

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

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

void valid_schema_and_stacks()
{
    const LevelDefinition level = ramp_level();
    const ValidationResult result = validate_level(level);
    expect(result.valid(), "a complete level with explicit axes, a ramp, fixture, IDs, and stack is valid");

    LevelDefinition unsupported = flat_level();
    unsupported.entities.push_back(
        Entity{2, EntityKind::box, Coordinate{1, 0}, Height::from_elevation(3)});
    expect(validate_level(unsupported).valid(),
           "structural validation permits an unsupported initial entity for later stabilization");
}

void schema_rejections()
{
    {
        LevelDefinition level = flat_level();
        level.width = 0;
        expect(contains(validate_level(level), ValidationErrorCode::invalid_dimensions),
               "zero-width boards are rejected");
    }
    {
        LevelDefinition level = flat_level();
        level.coordinates.positive_x = static_cast<HorizontalAxisDirection>(99);
        expect(contains(validate_level(level), ValidationErrorCode::invalid_coordinate_system),
               "malformed coordinate-axis values are rejected");
    }
    {
        LevelDefinition level = flat_level();
        level.cells.pop_back();
        expect(contains(validate_level(level), ValidationErrorCode::cell_count_mismatch),
               "incomplete cell rectangles are rejected");
    }
    {
        LevelDefinition level = flat_level();
        level.cells[1].coordinate = level.cells[0].coordinate;
        expect(contains(validate_level(level), ValidationErrorCode::duplicate_cell),
               "duplicate cell coordinates are rejected");
    }
    {
        LevelDefinition level = flat_level();
        level.entities.front().id = 0;
        expect(contains(validate_level(level), ValidationErrorCode::invalid_entity_id),
               "entity ID zero is reserved for API no-entity sentinels");
    }
    {
        LevelDefinition level = flat_level();
        level.entities.push_back(Entity{1, EntityKind::box, Coordinate{1, 0}, Height{0}});
        expect(contains(validate_level(level), ValidationErrorCode::duplicate_entity_id),
               "entity IDs must be unique");
    }
    {
        LevelDefinition level = flat_level();
        level.entities.front().kind = EntityKind::box;
        expect(contains(validate_level(level), ValidationErrorCode::player_count_not_one),
               "a level must contain exactly one player");
    }
    {
        LevelDefinition level = flat_level();
        level.entities.push_back(Entity{2, EntityKind::box, Coordinate{0, 0}, Height{1}});
        expect(contains(validate_level(level), ValidationErrorCode::overlapping_entities),
               "overlapping entity volumes are rejected");
    }
    {
        LevelDefinition level = flat_level();
        level.entities.push_back(Entity{2, EntityKind::box, Coordinate{0, 0}, Height{2}});
        expect(contains(validate_level(level), ValidationErrorCode::player_not_top_of_stack),
               "the player cannot have an entity above it");
    }
    {
        LevelDefinition level = flat_level();
        level.cells[0].geometry = FlatCell{1};
        expect(contains(validate_level(level), ValidationErrorCode::entity_below_surface),
               "entities cannot intersect a cell surface");
    }
    {
        LevelDefinition level = ramp_level();
        level.cells[2].geometry = RampCell{Direction::east, -1'073'741'825};
        expect(contains(validate_level(level), ValidationErrorCode::invalid_cell_height),
               "a ramp center below the representable half-step range is rejected");
    }
    {
        LevelDefinition level = ramp_level();
        level.fixtures.front().coordinate = Coordinate{11, -4};
        expect(contains(validate_level(level), ValidationErrorCode::fixture_on_ramp),
               "fixtures cannot be placed on ramps");
    }
    {
        LevelDefinition level = flat_level();
        level.fixtures = {
            Fixture{Coordinate{1, 0}, Switch{SwitchColor::red}},
            Fixture{Coordinate{1, 0}, Door{SwitchColor::red}},
        };
        expect(contains(validate_level(level), ValidationErrorCode::duplicate_fixture),
               "a cell cannot contain multiple fixtures");
    }
    {
        LevelDefinition level = flat_level();
        level.fixtures = {Fixture{Coordinate{1, 0}, ExitTeleporter{}}};
        level.entities.push_back(Entity{2, EntityKind::box, Coordinate{1, 0}, Height{0}});
        expect(contains(validate_level(level), ValidationErrorCode::invalid_teleporter_occupancy),
               "non-player entities cannot occupy teleporter columns");
    }
    {
        LevelDefinition level = ramp_level();
        level.cells[0].geometry = FlatCell{2};
        expect(contains(validate_level(level), ValidationErrorCode::invalid_ramp_endpoints),
               "ramp endpoint elevations must match the ramp geometry");
    }
}

void canonical_and_replaceable_lifetime()
{
    LevelDefinition first = ramp_level();
    LevelDefinition reordered = first;
    std::reverse(reordered.cells.begin(), reordered.cells.end());
    std::reverse(reordered.entities.begin(), reordered.entities.end());

    Engine first_engine;
    Engine second_engine;
    expect(first_engine.load_level(first).accepted(), "the first valid level loads");
    expect(second_engine.load_level(reordered).accepted(), "the reordered valid level loads");
    expect(first_engine.loaded_level() == second_engine.loaded_level(),
           "input container order does not affect the stored authoritative level");

    auto caller_snapshot = first_engine.loaded_level();
    expect(caller_snapshot.has_value(), "a loaded level is available as an owned snapshot");
    caller_snapshot->entities.front().id = 999;
    expect(first_engine.loaded_level()->entities.front().id != 999,
           "mutating a caller-owned snapshot cannot mutate the engine");

    const auto before_rejection = first_engine.loaded_level();
    LevelDefinition invalid = flat_level();
    invalid.entities.clear();
    const LoadResult rejected = first_engine.load_level(invalid);
    expect(!rejected.accepted(), "an invalid replacement is rejected");
    expect(contains(rejected, ValidationErrorCode::player_count_not_one),
           "a rejected replacement reports stable validation codes");
    expect(first_engine.loaded_level() == before_rejection,
           "an invalid replacement leaves the current level exactly unchanged");

    const LevelDefinition second = flat_level(Coordinate{-7, 5});
    expect(first_engine.load_level(second).accepted(), "a later valid replacement succeeds");
    expect(first_engine.loaded_level() == std::optional{canonicalize_level(second)},
           "a valid replacement completely replaces the prior level data");
}

} // namespace

int main()
{
    valid_schema_and_stacks();
    schema_rejections();
    canonical_and_replaceable_lifetime();

    if (failures == 0) {
        std::cout << "All world-schema behavior checks passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
