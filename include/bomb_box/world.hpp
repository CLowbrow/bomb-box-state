#pragma once

#include "bomb_box/types.hpp"

#include <cstdint>
#include <string_view>
#include <variant>
#include <vector>

namespace bomb_box {

enum class HorizontalAxisDirection : std::uint8_t {
    east,
    west,
};

enum class VerticalAxisDirection : std::uint8_t {
    north,
    south,
};

// Coordinates occupy the inclusive/exclusive rectangle beginning at origin.
// The axis directions describe how increasing numeric coordinates map into the
// game's cardinal directions.
struct CoordinateSystem final {
    Coordinate origin{};
    HorizontalAxisDirection positive_x{HorizontalAxisDirection::east};
    VerticalAxisDirection positive_y{VerticalAxisDirection::north};

    [[nodiscard]] friend constexpr bool operator==(const CoordinateSystem&,
                                                   const CoordinateSystem&) noexcept = default;
};

struct FlatCell final {
    std::int32_t elevation{};

    [[nodiscard]] friend constexpr bool operator==(FlatCell, FlatCell) noexcept = default;
};

struct RampCell final {
    Direction low_direction{Direction::south};
    std::int32_t low_elevation{};

    [[nodiscard]] friend constexpr bool operator==(RampCell, RampCell) noexcept = default;
};

using CellGeometry = std::variant<FlatCell, RampCell>;

struct Cell final {
    Coordinate coordinate{};
    CellGeometry geometry{FlatCell{}};

    [[nodiscard]] friend bool operator==(const Cell&, const Cell&) noexcept = default;
};

enum class SwitchColor : std::uint8_t {
    red,
    green,
    blue,
    yellow,
};

struct Switch final {
    SwitchColor color{SwitchColor::red};

    [[nodiscard]] friend constexpr bool operator==(Switch, Switch) noexcept = default;
};

struct Door final {
    SwitchColor color{SwitchColor::red};

    [[nodiscard]] friend constexpr bool operator==(Door, Door) noexcept = default;
};

struct ExitTeleporter final {
    [[nodiscard]] friend constexpr bool operator==(ExitTeleporter, ExitTeleporter) noexcept = default;
};

using FixtureKind = std::variant<Switch, Door, ExitTeleporter>;

struct Fixture final {
    Coordinate coordinate{};
    FixtureKind kind{Switch{}};

    [[nodiscard]] friend bool operator==(const Fixture&, const Fixture&) noexcept = default;
};

enum class EntityKind : std::uint8_t {
    player,
    box,
    barrel,
};

struct Entity final {
    EntityId id{};
    EntityKind kind{EntityKind::box};
    Coordinate coordinate{};
    Height bottom{};

    [[nodiscard]] friend constexpr bool operator==(const Entity&, const Entity&) noexcept = default;
};

struct LevelDefinition final {
    CoordinateSystem coordinates{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<Cell> cells{};
    std::vector<Fixture> fixtures{};
    std::vector<Entity> entities{};

    [[nodiscard]] friend bool operator==(const LevelDefinition&, const LevelDefinition&) = default;
};

enum class ValidationErrorCode : std::uint8_t {
    invalid_dimensions,
    invalid_coordinate_system,
    cell_count_mismatch,
    cell_out_of_bounds,
    duplicate_cell,
    invalid_cell_height,
    invalid_ramp_direction,
    invalid_ramp_endpoints,
    fixture_out_of_bounds,
    fixture_on_ramp,
    duplicate_fixture,
    invalid_fixture_color,
    entity_out_of_bounds,
    duplicate_entity_id,
    invalid_entity_kind,
    entity_below_surface,
    overlapping_entities,
    player_not_top_of_stack,
    player_count_not_one,
    invalid_teleporter_occupancy,
    invalid_entity_id,
};

struct ValidationError final {
    ValidationErrorCode code{ValidationErrorCode::invalid_dimensions};
    Coordinate coordinate{};
    EntityId entity_id{};

    [[nodiscard]] friend constexpr bool operator==(const ValidationError&,
                                                   const ValidationError&) noexcept = default;
};

struct ValidationResult final {
    std::vector<ValidationError> errors{};

    [[nodiscard]] bool valid() const noexcept { return errors.empty(); }
};

[[nodiscard]] std::string_view to_string(ValidationErrorCode code) noexcept;
[[nodiscard]] ValidationResult validate_level(const LevelDefinition& level);

// Produces the stable storage order used by Engine after validation succeeds.
// Cells are row-major; fixtures and entities are ordered spatially, with stack
// members bottom-to-top.
[[nodiscard]] LevelDefinition canonicalize_level(LevelDefinition level);

} // namespace bomb_box
