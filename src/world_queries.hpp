#pragma once

#include "game_rules/world.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>

namespace game_rules::detail {

struct CoordinateLess final {
    [[nodiscard]] bool operator()(const Coordinate lhs, const Coordinate rhs) const noexcept
    {
        return std::tie(lhs.y, lhs.x) < std::tie(rhs.y, rhs.x);
    }
};

[[nodiscard]] inline auto spatial_key(const Coordinate coordinate) noexcept
{
    return std::tuple{coordinate.y, coordinate.x};
}

[[nodiscard]] inline bool has_representable_extent(const LevelDefinition& level) noexcept
{
    if (level.width == 0 || level.height == 0) {
        return false;
    }

    const auto last_x = static_cast<std::int64_t>(level.coordinates.origin.x)
        + static_cast<std::int64_t>(level.width) - 1;
    const auto last_y = static_cast<std::int64_t>(level.coordinates.origin.y)
        + static_cast<std::int64_t>(level.height) - 1;
    return last_x <= std::numeric_limits<std::int32_t>::max()
        && last_y <= std::numeric_limits<std::int32_t>::max();
}

[[nodiscard]] inline bool in_bounds(const LevelDefinition& level,
                                    const Coordinate coordinate) noexcept
{
    if (!has_representable_extent(level)) {
        return false;
    }
    const auto offset_x = static_cast<std::int64_t>(coordinate.x) - level.coordinates.origin.x;
    const auto offset_y = static_cast<std::int64_t>(coordinate.y) - level.coordinates.origin.y;
    return offset_x >= 0 && offset_y >= 0
        && offset_x < static_cast<std::int64_t>(level.width)
        && offset_y < static_cast<std::int64_t>(level.height);
}

[[nodiscard]] inline std::optional<Coordinate> step(
    const Coordinate coordinate,
    const Direction direction,
    const CoordinateSystem& system) noexcept
{
    std::int32_t dx = 0;
    std::int32_t dy = 0;
    switch (direction) {
    case Direction::north:
        dy = system.positive_y == VerticalAxisDirection::north ? 1 : -1;
        break;
    case Direction::east:
        dx = system.positive_x == HorizontalAxisDirection::east ? 1 : -1;
        break;
    case Direction::south:
        dy = system.positive_y == VerticalAxisDirection::south ? 1 : -1;
        break;
    case Direction::west:
        dx = system.positive_x == HorizontalAxisDirection::west ? 1 : -1;
        break;
    }

    const auto x = static_cast<std::int64_t>(coordinate.x) + dx;
    const auto y = static_cast<std::int64_t>(coordinate.y) + dy;
    if (x < std::numeric_limits<std::int32_t>::min()
        || x > std::numeric_limits<std::int32_t>::max()
        || y < std::numeric_limits<std::int32_t>::min()
        || y > std::numeric_limits<std::int32_t>::max()) {
        return std::nullopt;
    }
    return Coordinate{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
}

[[nodiscard]] inline Direction opposite(const Direction direction) noexcept
{
    switch (direction) {
    case Direction::north: return Direction::south;
    case Direction::east: return Direction::west;
    case Direction::south: return Direction::north;
    case Direction::west: return Direction::east;
    }
    return direction;
}

[[nodiscard]] inline const Cell* find_cell(const LevelDefinition& level,
                                           const Coordinate coordinate) noexcept
{
    const auto found = std::find_if(level.cells.begin(), level.cells.end(),
                                    [coordinate](const Cell& cell) {
                                        return cell.coordinate == coordinate;
                                    });
    return found == level.cells.end() ? nullptr : &*found;
}

[[nodiscard]] inline const Fixture* find_fixture(const LevelDefinition& level,
                                                 const Coordinate coordinate) noexcept
{
    const auto found = std::find_if(level.fixtures.begin(), level.fixtures.end(),
                                    [coordinate](const Fixture& fixture) {
                                        return fixture.coordinate == coordinate;
                                    });
    return found == level.fixtures.end() ? nullptr : &*found;
}

[[nodiscard]] inline std::int64_t support_half_steps(const Cell& cell) noexcept
{
    if (const auto* const flat = std::get_if<FlatCell>(&cell.geometry)) {
        return static_cast<std::int64_t>(flat->elevation) * 2;
    }
    return static_cast<std::int64_t>(std::get<RampCell>(cell.geometry).low_elevation) * 2 + 1;
}

[[nodiscard]] inline Height surface_height(const Cell& cell) noexcept
{
    return Height{static_cast<std::int32_t>(support_half_steps(cell))};
}

} // namespace game_rules::detail
