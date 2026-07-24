#pragma once

#include <cstdint>

namespace game_rules {

using EntityId = std::uint64_t;

struct Coordinate final {
    std::int32_t x{};
    std::int32_t y{};

    [[nodiscard]] friend constexpr bool operator==(Coordinate, Coordinate) noexcept = default;
};

enum class Direction : std::uint8_t {
    north,
    east,
    south,
    west,
};

enum class Outcome : std::uint8_t {
    ongoing,
    won,
    lost,
};

// Heights in the current rules are always integer or half-integer values.
// Representing them as half-steps avoids cross-platform floating-point drift.
struct Height final {
    std::int32_t half_steps{};

    [[nodiscard]] static constexpr Height from_elevation(std::int32_t elevation) noexcept
    {
        return Height{elevation * 2};
    }

    [[nodiscard]] friend constexpr bool operator==(Height, Height) noexcept = default;
};

} // namespace game_rules

