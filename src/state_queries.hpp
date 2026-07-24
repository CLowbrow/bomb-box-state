#pragma once

#include "game_rules/engine.hpp"

#include <algorithm>
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
