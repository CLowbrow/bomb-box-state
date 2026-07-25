#pragma once

#include "game_rules/engine.hpp"

#include <algorithm>
#include <cstdint>
#include <tuple>

namespace game_rules::detail {

[[nodiscard]] inline bool occupied(const ResolvedState& state,
                                   const Coordinate coordinate) noexcept
{
    return std::any_of(state.entities.begin(), state.entities.end(),
                       [coordinate](const Entity& entity) {
                           return entity.coordinate == coordinate;
                       });
}

[[nodiscard]] inline bool volume_is_clear(const ResolvedState& state,
                                          const Coordinate coordinate,
                                          const Height bottom) noexcept
{
    const auto arrival_bottom = static_cast<std::int64_t>(bottom.half_steps);
    const auto arrival_top = arrival_bottom + 2;
    return std::none_of(state.entities.begin(), state.entities.end(),
                        [coordinate, arrival_bottom, arrival_top](const Entity& entity) {
                            if (entity.coordinate != coordinate) {
                                return false;
                            }
                            const auto entity_bottom =
                                static_cast<std::int64_t>(entity.bottom.half_steps);
                            const auto entity_top = entity_bottom + 2;
                            return entity_bottom < arrival_top && arrival_bottom < entity_top;
                        });
}

inline void canonicalize_entities(std::vector<Entity>& entities)
{
    std::sort(entities.begin(), entities.end(), [](const Entity& lhs, const Entity& rhs) {
        return std::tuple{lhs.coordinate.y, lhs.coordinate.x, lhs.bottom.half_steps, lhs.id}
            < std::tuple{rhs.coordinate.y, rhs.coordinate.x, rhs.bottom.half_steps, rhs.id};
    });
}

[[nodiscard]] inline bool is_armed(const ResolvedState& state, const EntityId id) noexcept
{
    return std::binary_search(state.armed_barrels.begin(), state.armed_barrels.end(), id);
}

inline void arm_barrel(ResolvedState& state, const EntityId id)
{
    const auto position = std::lower_bound(state.armed_barrels.begin(),
                                           state.armed_barrels.end(), id);
    if (position == state.armed_barrels.end() || *position != id) {
        state.armed_barrels.insert(position, id);
    }
}

} // namespace game_rules::detail
