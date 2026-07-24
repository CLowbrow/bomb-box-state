#pragma once

#include "game_rules/engine.hpp"

#include <cstdint>
#include <optional>

namespace game_rules::detail {

// Plans one explosion wave from all currently settled armed barrels.
[[nodiscard]] std::optional<TickResult> resolve_explosion_wave_tick(const LevelDefinition& level,
                                                                    const ResolvedState& state,
                                                                    std::uint32_t tick_index);

} // namespace game_rules::detail
