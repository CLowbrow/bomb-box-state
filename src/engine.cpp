#include "game_rules/engine.hpp"

#include "explosions.hpp"
#include "fixtures.hpp"
#include "gravity.hpp"
#include "player_movement.hpp"
#include "ramps.hpp"

#include <cstdint>
#include <utility>

namespace game_rules {
namespace {

[[nodiscard]] bool valid(const Direction direction) noexcept
{
    switch (direction) {
    case Direction::north:
    case Direction::east:
    case Direction::south:
    case Direction::west:
        return true;
    }
    return false;
}

void resolve_derived_ticks(const LevelDefinition& level,
                           ResolvedState& state,
                           std::vector<TickResult>& ticks)
{
    while (state.outcome == Outcome::ongoing) {
        std::optional<TickResult> derived = detail::resolve_falling_tick(
            level, state, static_cast<std::uint32_t>(ticks.size()));
        if (!derived.has_value()) {
            derived = detail::resolve_sliding_tick(
                level, state, static_cast<std::uint32_t>(ticks.size()));
        }
        if (!derived.has_value()) {
            derived = detail::resolve_explosion_wave_tick(
                level, state, static_cast<std::uint32_t>(ticks.size()));
        }
        if (!derived.has_value()) {
            break;
        }

        detail::resolve_fixtures_after_tick(level, *derived);
        state = derived->state_after;
        ticks.push_back(std::move(*derived));
    }
}

[[nodiscard]] MoveResult rejected_move(const MoveStatus status,
                                       const Direction direction,
                                       const std::optional<ResolvedState>& state,
                                       const bool gameplay_rejection = true)
{
    MoveResult result;
    result.status = status;
    result.direction = direction;
    if (gameplay_rejection) {
        result.events.emplace_back(MoveBlockedEvent{direction, status});
    }
    result.final_state = state;
    if (state.has_value()) {
        result.outcome = state->outcome;
    }
    return result;
}

} // namespace

std::string_view to_string(const EngineStatus status) noexcept
{
    switch (status) {
    case EngineStatus::schema_ready:
        return "schema_ready";
    }

    return "unknown";
}

EngineStatus Engine::status() const noexcept
{
    return EngineStatus::schema_ready;
}

std::string_view to_string(const LoadStatus status) noexcept
{
    switch (status) {
    case LoadStatus::loaded:
        return "loaded";
    case LoadStatus::invalid_level:
        return "invalid_level";
    }
    return "unknown";
}

std::string_view to_string(const RewindStatus status) noexcept
{
    switch (status) {
    case RewindStatus::rewound:
        return "rewound";
    case RewindStatus::history_empty:
        return "history_empty";
    }
    return "unknown";
}

std::string_view to_string(const MoveStatus status) noexcept
{
    switch (status) {
    case MoveStatus::moved: return "moved";
    case MoveStatus::no_level: return "no_level";
    case MoveStatus::invalid_direction: return "invalid_direction";
    case MoveStatus::world_boundary: return "world_boundary";
    case MoveStatus::ledge: return "ledge";
    case MoveStatus::occupied: return "occupied";
    case MoveStatus::stacked_push_target: return "stacked_push_target";
    case MoveStatus::closed_door: return "closed_door";
    case MoveStatus::teleporter_restriction: return "teleporter_restriction";
    case MoveStatus::unsupported_geometry: return "unsupported_geometry";
    case MoveStatus::level_terminal: return "level_terminal";
    }
    return "unknown";
}

std::string_view to_string(const MovementCause cause) noexcept
{
    switch (cause) {
    case MovementCause::player: return "player";
    case MovementCause::blast: return "blast";
    case MovementCause::fall: return "fall";
    case MovementCause::slide: return "slide";
    }
    return "unknown";
}

bool detail::ResolvedStateHistory::has_current() const noexcept
{
    return current_.has_value();
}

const std::optional<ResolvedState>& detail::ResolvedStateHistory::current() const noexcept
{
    return current_;
}

std::size_t detail::ResolvedStateHistory::earlier_count() const noexcept
{
    return earlier_.size();
}

void detail::ResolvedStateHistory::reset(ResolvedState initial_state)
{
    current_ = std::move(initial_state);
    earlier_.clear();
}

bool detail::ResolvedStateHistory::commit(ResolvedState next_state)
{
    if (!current_.has_value()) {
        return false;
    }

    earlier_.push_back(*current_);
    current_ = std::move(next_state);
    return true;
}

bool detail::ResolvedStateHistory::rewind()
{
    if (earlier_.empty()) {
        return false;
    }

    current_ = std::move(earlier_.back());
    earlier_.pop_back();
    return true;
}

bool Engine::has_level() const noexcept
{
    return level_.has_value();
}

std::optional<LevelDefinition> Engine::loaded_level() const
{
    return level_;
}

std::optional<ResolvedState> Engine::resolved_state() const
{
    return history_.current();
}

LoadResult Engine::load_level(const LevelDefinition& level)
{
    ValidationResult validation = validate_level(level);
    if (!validation.valid()) {
        return LoadResult{LoadStatus::invalid_level, std::move(validation.errors)};
    }

    LevelDefinition replacement = canonicalize_level(level);
    const ResolvedState initial{replacement.entities, Outcome::ongoing};
    ResolvedState stabilized = initial;
    std::vector<TickResult> ticks;
    if (const std::optional<TickResult> fixtures =
            detail::resolve_initial_fixture_tick(replacement, stabilized, 0)) {
        stabilized = fixtures->state_after;
        ticks.push_back(*fixtures);
    }
    resolve_derived_ticks(replacement, stabilized, ticks);

    detail::ResolvedStateHistory replacement_history;
    replacement_history.reset(stabilized);

    level_ = std::move(replacement);
    history_ = std::move(replacement_history);
    return LoadResult{
        LoadStatus::loaded,
        {},
        initial,
        std::move(ticks),
        history_.current(),
        history_.current()->outcome,
    };
}

MoveResult Engine::move(const Direction direction)
{
    if (!valid(direction)) {
        return rejected_move(MoveStatus::invalid_direction, direction, history_.current(), false);
    }
    if (!level_.has_value() || !history_.current().has_value()) {
        return rejected_move(MoveStatus::no_level, direction, std::nullopt);
    }

    const ResolvedState& current = *history_.current();
    if (current.outcome != Outcome::ongoing) {
        return rejected_move(MoveStatus::level_terminal, direction, current);
    }

    const ResolvedState initial = current;
    detail::PlayerMovementPlan movement =
        detail::plan_player_movement(*level_, current, direction);
    if (!movement.accepted()) {
        return rejected_move(movement.status, direction, current);
    }

    detail::resolve_fixtures_after_tick(*level_, *movement.tick);
    std::vector<TickResult> ticks;
    ticks.push_back(std::move(*movement.tick));
    ResolvedState resolved = ticks.back().state_after;
    resolve_derived_ticks(*level_, resolved, ticks);
    if (!history_.commit(std::move(resolved))) {
        return rejected_move(MoveStatus::no_level, direction, history_.current());
    }

    return MoveResult{
        MoveStatus::moved,
        direction,
        {},
        initial,
        std::move(ticks),
        history_.current(),
        history_.current()->outcome,
    };
}

RewindResult Engine::rewind()
{
    const bool accepted = history_.rewind();
    std::vector<GameplayEvent> events;
    if (accepted) {
        events.emplace_back(StateRewoundEvent{});
    }
    return RewindResult{
        accepted ? RewindStatus::rewound : RewindStatus::history_empty,
        history_.current(),
        std::move(events),
        history_.current().has_value() ? std::optional{history_.current()->outcome} : std::nullopt,
    };
}

} // namespace game_rules
