#include "gravity.hpp"

#include <algorithm>
#include <cstdint>
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

[[nodiscard]] std::int64_t support_half_steps(const Cell& cell) noexcept
{
    if (const auto* flat = std::get_if<FlatCell>(&cell.geometry)) {
        return static_cast<std::int64_t>(flat->elevation) * 2;
    }
    return static_cast<std::int64_t>(std::get<RampCell>(cell.geometry).low_elevation) * 2 + 1;
}

[[nodiscard]] const Cell* find_cell(const LevelDefinition& level,
                                    const Coordinate coordinate) noexcept
{
    const auto found = std::find_if(level.cells.begin(), level.cells.end(),
                                    [coordinate](const Cell& cell) {
                                        return cell.coordinate == coordinate;
                                    });
    return found == level.cells.end() ? nullptr : &*found;
}

[[nodiscard]] bool is_armed(const ResolvedState& state, const EntityId id) noexcept
{
    return std::binary_search(state.armed_barrels.begin(), state.armed_barrels.end(), id);
}

void arm_barrel(ResolvedState& state, const EntityId id)
{
    const auto position = std::lower_bound(state.armed_barrels.begin(), state.armed_barrels.end(), id);
    if (position == state.armed_barrels.end() || *position != id) {
        state.armed_barrels.insert(position, id);
    }
}

} // namespace

std::optional<TickResult> resolve_falling_tick(const LevelDefinition& level,
                                               const ResolvedState& state,
                                               const std::uint32_t tick_index)
{
    if (state.outcome != Outcome::ongoing) {
        return std::nullopt;
    }

    std::map<Coordinate, std::vector<std::size_t>, CoordinateLess> columns;
    for (std::size_t index = 0; index < state.entities.size(); ++index) {
        columns[state.entities[index].coordinate].push_back(index);
    }
    for (auto& [coordinate, indices] : columns) {
        static_cast<void>(coordinate);
        std::sort(indices.begin(), indices.end(), [&state](const std::size_t lhs,
                                                          const std::size_t rhs) {
            const Entity& left = state.entities[lhs];
            const Entity& right = state.entities[rhs];
            return std::tie(left.bottom.half_steps, left.id)
                < std::tie(right.bottom.half_steps, right.id);
        });
    }

    ResolvedState next = state;
    std::vector<GameplayEvent> events;
    bool lost = false;

    for (const auto& [coordinate, indices] : columns) {
        const Cell* const cell = find_cell(level, coordinate);
        if (cell == nullptr) {
            continue;
        }

        std::int64_t landing_bottom = support_half_steps(*cell);
        const Entity* landed_below = nullptr;
        for (const std::size_t index : indices) {
            const Entity& before = state.entities[index];
            const std::int64_t old_bottom = before.bottom.half_steps;
            if (old_bottom <= landing_bottom) {
                landing_bottom = old_bottom + 2;
                landed_below = &before;
                continue;
            }

            if (landed_below != nullptr && landed_below->kind == EntityKind::player) {
                events.emplace_back(PlayerCrushedEvent{landed_below->id, before.id});
                lost = true;
                break;
            }

            Entity& after = next.entities[index];
            after.bottom.half_steps = static_cast<std::int32_t>(landing_bottom);
            events.emplace_back(EntityMovedEvent{
                before.id,
                before.coordinate,
                before.coordinate,
                before.bottom,
                after.bottom,
                MovementCause::fall,
            });

            if (before.kind == EntityKind::barrel && !is_armed(state, before.id)) {
                arm_barrel(next, before.id);
                events.emplace_back(BarrelArmedEvent{before.id});
            }
            if (before.kind == EntityKind::player && old_bottom - landing_bottom >= 2) {
                lost = true;
            }

            landing_bottom += 2;
            landed_below = &after;
        }
    }

    if (events.empty()) {
        return std::nullopt;
    }
    if (lost) {
        next.outcome = Outcome::lost;
        events.emplace_back(LevelLostEvent{});
    }
    return TickResult{tick_index, std::move(events), std::move(next)};
}

} // namespace game_rules::detail
