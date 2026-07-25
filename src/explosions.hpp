#pragma once

#include "game_rules/engine.hpp"

#include <cstdint>
#include <optional>

namespace game_rules::detail {

// Plans one simultaneous wave from every settled armed barrel in the shared
// pre-wave state. Later waves are scheduled by the engine only after any
// resulting falls and ramp slides settle.
[[nodiscard]] std::optional<TickResult> resolve_explosion_wave_tick(
    const LevelDefinition& level,
    const ResolvedState& state,
    std::uint32_t tick_index);

} // namespace game_rules::detail
