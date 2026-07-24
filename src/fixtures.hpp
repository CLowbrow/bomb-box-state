#pragma once

#include "game_rules/engine.hpp"

#include <cstdint>
#include <optional>

namespace game_rules::detail {

[[nodiscard]] bool is_effectively_open_door(const LevelDefinition& level,
                                            const ResolvedState& state,
                                            Coordinate coordinate) noexcept;

// Produces the standalone initialization tick for an immediate teleporter win
// or initial switch/door derivation. No tick is produced when nothing changes.
[[nodiscard]] std::optional<TickResult> resolve_initial_fixture_tick(
    const LevelDefinition& level,
    const ResolvedState& state,
    std::uint32_t tick_index);

// Applies post-tick teleporter precedence and fixture derivation to an already
// planned physical tick. Fixture events are appended deterministically.
void resolve_fixtures_after_tick(const LevelDefinition& level, TickResult& tick);

} // namespace game_rules::detail
