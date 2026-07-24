#pragma once

#include "game_rules/engine.hpp"

#include <optional>

namespace game_rules::detail {

struct PlayerMovementPlan final {
    MoveStatus status{MoveStatus::unsupported_geometry};
    std::optional<TickResult> tick{};

    [[nodiscard]] bool accepted() const noexcept
    {
        return status == MoveStatus::moved && tick.has_value();
    }
};

// Plans the player's physical movement tick from one immutable command-boundary
// state. Command gating, fixture derivation, derived ticks, and history commit
// remain Engine orchestration concerns.
[[nodiscard]] PlayerMovementPlan plan_player_movement(
    const LevelDefinition& level,
    const ResolvedState& state,
    Direction direction);

} // namespace game_rules::detail
