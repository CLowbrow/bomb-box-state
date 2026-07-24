#pragma once

#include "game_rules/engine.hpp"

#include <cstdint>
#include <optional>

namespace game_rules::detail {

// Plans one simultaneous gravity tick from an immutable state. This is an
// internal rule seam, exposed only to focused white-box tests for states that
// later movement phases can create but current public commands cannot yet.
[[nodiscard]] std::optional<TickResult> resolve_falling_tick(
    const LevelDefinition& level,
    const ResolvedState& state,
    std::uint32_t tick_index);

} // namespace game_rules::detail
