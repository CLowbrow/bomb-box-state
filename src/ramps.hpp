#pragma once

#include "game_rules/engine.hpp"

#include <cstdint>
#include <optional>

namespace game_rules::detail {

// Plans one simultaneous automatic ramp-slide tick from an immutable state.
// Blocked and conflicting stacks remain in place and produce no tick.
[[nodiscard]] std::optional<TickResult> resolve_sliding_tick(
    const LevelDefinition& level,
    const ResolvedState& state,
    std::uint32_t tick_index);

} // namespace game_rules::detail
