#include "ramps.hpp"

#include "fixtures.hpp"
#include "state_queries.hpp"
#include "world_queries.hpp"

#include <algorithm>
#include <cstdint>
#include <map>
#include <tuple>
#include <utility>
#include <vector>

namespace game_rules::detail {
namespace {

struct SlideCandidate final {
    Coordinate source{};
    Coordinate destination{};
    Height destination_bottom{};
    std::vector<std::size_t> entity_indices{};
};

[[nodiscard]] bool destination_fixture_blocks(const LevelDefinition& level,
                                               const ResolvedState& state,
                                               const Coordinate coordinate) noexcept
{
    const Fixture* const fixture = find_fixture(level, coordinate);
    if (fixture == nullptr) {
        return false;
    }
    if (std::holds_alternative<ExitTeleporter>(fixture->kind)) {
        return true;
    }
    return std::holds_alternative<Door>(fixture->kind)
        && !is_effectively_open_door(level, state, coordinate);
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
        const Cell* const destination_cell = find_cell(level, *destination);
        if (destination_cell == nullptr) {
            continue;
        }
        if (const auto* destination_ramp =
                std::get_if<RampCell>(&destination_cell->geometry);
            destination_ramp != nullptr
            && !ramps_connect(*ramp, *destination_ramp, ramp->low_direction)) {
            continue;
        }
        candidates.push_back(SlideCandidate{
            cell.coordinate,
            *destination,
            surface_height(*destination_cell),
            std::move(indices),
        });
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
        const Entity& source_bottom = state.entities[candidate.entity_indices.front()];
        const auto bottom_change = static_cast<std::int64_t>(
            candidate.destination_bottom.half_steps) - source_bottom.bottom.half_steps;
        for (const std::size_t index : candidate.entity_indices) {
            const Entity& before = state.entities[index];
            Entity& after = next.entities[index];
            after.coordinate = candidate.destination;
            after.bottom.half_steps = static_cast<std::int32_t>(
                static_cast<std::int64_t>(before.bottom.half_steps) + bottom_change);
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
