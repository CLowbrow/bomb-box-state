#include "ramps.hpp"

#include "fixtures.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace game_rules::detail {
namespace {

struct CoordinateLess final {
    [[nodiscard]] bool operator()(const Coordinate lhs, const Coordinate rhs) const noexcept
    {
        return std::tie(lhs.y, lhs.x) < std::tie(rhs.y, rhs.x);
    }
};

struct SlideCandidate final {
    Coordinate source{};
    Coordinate destination{};
    std::vector<std::size_t> entity_indices{};
};

[[nodiscard]] std::optional<Coordinate> step(const Coordinate coordinate,
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

[[nodiscard]] bool occupied(const ResolvedState& state,
                            const Coordinate coordinate) noexcept
{
    return std::any_of(state.entities.begin(), state.entities.end(),
                       [coordinate](const Entity& entity) {
                           return entity.coordinate == coordinate;
                       });
}

[[nodiscard]] bool destination_fixture_blocks(const LevelDefinition& level,
                                               const ResolvedState& state,
                                               const Coordinate coordinate) noexcept
{
    const auto fixture = std::find_if(level.fixtures.begin(), level.fixtures.end(),
                                      [coordinate](const Fixture& value) {
                                          return value.coordinate == coordinate;
                                      });
    if (fixture == level.fixtures.end()) {
        return false;
    }
    if (std::holds_alternative<ExitTeleporter>(fixture->kind)) {
        return true;
    }
    return std::holds_alternative<Door>(fixture->kind)
        && !is_effectively_open_door(level, state, coordinate);
}

void canonicalize_entities(std::vector<Entity>& entities)
{
    std::sort(entities.begin(), entities.end(), [](const Entity& lhs, const Entity& rhs) {
        return std::tuple{lhs.coordinate.y, lhs.coordinate.x, lhs.bottom.half_steps, lhs.id}
            < std::tuple{rhs.coordinate.y, rhs.coordinate.x, rhs.bottom.half_steps, rhs.id};
    });
}

} // namespace

std::optional<TickResult> resolve_sliding_tick(const LevelDefinition& level,
                                               const ResolvedState& state,
                                               const std::uint32_t tick_index)
{
    if (state.outcome != Outcome::ongoing) {
        return std::nullopt;
    }

    std::vector<SlideCandidate> candidates;
    for (const Cell& cell : level.cells) {
        const auto* const ramp = std::get_if<RampCell>(&cell.geometry);
        if (ramp == nullptr) {
            continue;
        }

        std::vector<std::size_t> indices;
        for (std::size_t index = 0; index < state.entities.size(); ++index) {
            if (state.entities[index].coordinate == cell.coordinate) {
                indices.push_back(index);
            }
        }
        std::sort(indices.begin(), indices.end(), [&state](const std::size_t lhs,
                                                          const std::size_t rhs) {
            const Entity& left = state.entities[lhs];
            const Entity& right = state.entities[rhs];
            return std::tie(left.bottom.half_steps, left.id)
                < std::tie(right.bottom.half_steps, right.id);
        });
        if (indices.empty()) {
            continue;
        }

        const Entity& bottom = state.entities[indices.front()];
        const auto ramp_surface = static_cast<std::int64_t>(ramp->low_elevation) * 2 + 1;
        if ((bottom.kind != EntityKind::box && bottom.kind != EntityKind::barrel)
            || bottom.bottom.half_steps != ramp_surface) {
            continue;
        }

        const std::optional<Coordinate> destination =
            step(cell.coordinate, ramp->low_direction, level.coordinates);
        if (!destination.has_value() || occupied(state, *destination)
            || destination_fixture_blocks(level, state, *destination)) {
            continue;
        }
        candidates.push_back(SlideCandidate{cell.coordinate, *destination, std::move(indices)});
    }

    std::map<Coordinate, std::size_t, CoordinateLess> destination_counts;
    for (const SlideCandidate& candidate : candidates) {
        ++destination_counts[candidate.destination];
    }

    ResolvedState next = state;
    std::vector<GameplayEvent> events;
    for (const SlideCandidate& candidate : candidates) {
        if (destination_counts[candidate.destination] != 1) {
            continue;
        }
        for (const std::size_t index : candidate.entity_indices) {
            const Entity& before = state.entities[index];
            Entity& after = next.entities[index];
            after.coordinate = candidate.destination;
            --after.bottom.half_steps;
            events.emplace_back(EntityMovedEvent{
                before.id,
                candidate.source,
                candidate.destination,
                before.bottom,
                after.bottom,
                MovementCause::slide,
            });
        }
    }

    if (events.empty()) {
        return std::nullopt;
    }
    canonicalize_entities(next.entities);
    return TickResult{tick_index, std::move(events), std::move(next)};
}

} // namespace game_rules::detail
