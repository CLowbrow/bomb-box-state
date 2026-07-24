#pragma once

#include "game_rules/engine.hpp"

#include <cstdint>
#include <optional>

namespace game_rules::detail {

// Plans one explosion tick when exactly one settled armed barrel is ready.
// Simultaneous sources and subsequent chain waves are intentionally left for
// the explosion-wave phase.
[[nodiscard]] std::optional<TickResult> resolve_single_explosion_tick(
    const LevelDefinition& level,
    const ResolvedState& state,
    std::uint32_t tick_index);

} // namespace game_rules::detail
