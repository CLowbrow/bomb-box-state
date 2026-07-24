#include "game_rules/engine.hpp"

#include "explosions.hpp"
#include "fixtures.hpp"
#include "gravity.hpp"
#include "ramps.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <tuple>
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

[[nodiscard]] std::optional<Coordinate> step(const Coordinate coordinate,
                                             const Direction direction,
                                             const CoordinateSystem& system) noexcept
{
    std::int32_t dx = 0;
    std::int32_t dy = 0;
    switch (direction) {
    case Direction::north:
        dy = system.positive_y == VerticalAxisDirection::north ? 1 : -1;
        break;
    case Direction::east:
        dx = system.positive_x == HorizontalAxisDirection::east ? 1 : -1;
        break;
    case Direction::south:
        dy = system.positive_y == VerticalAxisDirection::south ? 1 : -1;
        break;
    case Direction::west:
        dx = system.positive_x == HorizontalAxisDirection::west ? 1 : -1;
        break;
    }

    const auto x = static_cast<std::int64_t>(coordinate.x) + dx;
    const auto y = static_cast<std::int64_t>(coordinate.y) + dy;
    if (x < std::numeric_limits<std::int32_t>::min()
        || x > std::numeric_limits<std::int32_t>::max()
        || y < std::numeric_limits<std::int32_t>::min()
        || y > std::numeric_limits<std::int32_t>::max()) {
        return std::nullopt;
    }
    return Coordinate{static_cast<std::int32_t>(x), static_cast<std::int32_t>(y)};
}

[[nodiscard]] bool in_bounds(const LevelDefinition& level, const Coordinate coordinate) noexcept
{
    const auto offset_x = static_cast<std::int64_t>(coordinate.x) - level.coordinates.origin.x;
    const auto offset_y = static_cast<std::int64_t>(coordinate.y) - level.coordinates.origin.y;
    return offset_x >= 0 && offset_y >= 0
        && offset_x < static_cast<std::int64_t>(level.width)
        && offset_y < static_cast<std::int64_t>(level.height);
}

[[nodiscard]] const Cell* find_cell(const LevelDefinition& level, const Coordinate coordinate) noexcept
{
    const auto cell = std::find_if(level.cells.begin(), level.cells.end(), [coordinate](const Cell& value) {
        return value.coordinate == coordinate;
    });
    return cell == level.cells.end() ? nullptr : &*cell;
}

[[nodiscard]] Direction opposite(const Direction direction) noexcept
{
    switch (direction) {
    case Direction::north: return Direction::south;
    case Direction::east: return Direction::west;
    case Direction::south: return Direction::north;
    case Direction::west: return Direction::east;
    }
    return direction;
}

[[nodiscard]] Height surface_height(const Cell& cell) noexcept
{
    if (const auto* const flat = std::get_if<FlatCell>(&cell.geometry)) {
        return Height::from_elevation(flat->elevation);
    }
    const RampCell& ramp = std::get<RampCell>(cell.geometry);
    return Height{static_cast<std::int32_t>(
        static_cast<std::int64_t>(ramp.low_elevation) * 2 + 1)};
}

void canonicalize_entities(std::vector<Entity>& entities)
{
    std::sort(entities.begin(), entities.end(), [](const Entity& lhs, const Entity& rhs) {
        return std::tuple{lhs.coordinate.y, lhs.coordinate.x, lhs.bottom.half_steps, lhs.id}
            < std::tuple{rhs.coordinate.y, rhs.coordinate.x, rhs.bottom.half_steps, rhs.id};
    });
}

void resolve_derived_ticks(const LevelDefinition& level,
                           ResolvedState& state,
                           std::vector<TickResult>& ticks)
{
    bool explosion_resolved = false;
    while (state.outcome == Outcome::ongoing) {
        std::optional<TickResult> derived = detail::resolve_falling_tick(
            level, state, static_cast<std::uint32_t>(ticks.size()));
        if (!derived.has_value()) {
            derived = detail::resolve_sliding_tick(
                level, state, static_cast<std::uint32_t>(ticks.size()));
        }
        if (!derived.has_value() && !explosion_resolved) {
            derived = detail::resolve_single_explosion_tick(
                level, state, static_cast<std::uint32_t>(ticks.size()));
            explosion_resolved = derived.has_value();
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

    const auto player = std::find_if(current.entities.begin(), current.entities.end(), [](const Entity& entity) {
        return entity.kind == EntityKind::player;
    });
    if (player == current.entities.end()) {
        return rejected_move(MoveStatus::no_level, direction, current);
    }

    const std::optional<Coordinate> destination = step(player->coordinate, direction, level_->coordinates);
    if (!destination.has_value() || !in_bounds(*level_, *destination)) {
        return rejected_move(MoveStatus::world_boundary, direction, current);
    }

    const Cell* const source_cell = find_cell(*level_, player->coordinate);
    const Cell* const destination_cell = find_cell(*level_, *destination);
    if (source_cell == nullptr || destination_cell == nullptr) {
        return rejected_move(MoveStatus::unsupported_geometry, direction, current);
    }

    const auto fixture = std::find_if(level_->fixtures.begin(), level_->fixtures.end(),
                                     [destination](const Fixture& value) {
                                         return value.coordinate == *destination;
                                     });
    const bool destination_is_exit = fixture != level_->fixtures.end()
        && std::holds_alternative<ExitTeleporter>(fixture->kind);
    if (fixture != level_->fixtures.end() && std::holds_alternative<Door>(fixture->kind)
        && !detail::is_effectively_open_door(*level_, current, *destination)) {
        return rejected_move(MoveStatus::closed_door, direction, current);
    }

    Height destination_support = surface_height(*destination_cell);
    std::size_t destination_entity_count = 0;
    const Entity* push_target = nullptr;
    for (const Entity& entity : current.entities) {
        if (entity.coordinate != *destination) {
            continue;
        }
        ++destination_entity_count;
        if (entity.bottom == player->bottom
            && (entity.kind == EntityKind::box || entity.kind == EntityKind::barrel)) {
            push_target = &entity;
        }
        const auto top_half_steps = static_cast<std::int64_t>(entity.bottom.half_steps) + 2;
        if (top_half_steps > destination_support.half_steps
            && top_half_steps <= std::numeric_limits<std::int32_t>::max()) {
            destination_support.half_steps = static_cast<std::int32_t>(top_half_steps);
        }
    }

    if (push_target != nullptr) {
        if (!std::holds_alternative<FlatCell>(destination_cell->geometry)) {
            return rejected_move(MoveStatus::unsupported_geometry, direction, current);
        }
        if (destination_entity_count != 1) {
            return rejected_move(MoveStatus::stacked_push_target, direction, current);
        }

        if (destination_is_exit) {
            return rejected_move(MoveStatus::teleporter_restriction, direction, current);
        }

        const std::optional<Coordinate> pushed_destination =
            step(push_target->coordinate, direction, level_->coordinates);
        if (!pushed_destination.has_value() || !in_bounds(*level_, *pushed_destination)) {
            return rejected_move(MoveStatus::world_boundary, direction, current);
        }

        const Cell* const pushed_destination_cell = find_cell(*level_, *pushed_destination);
        if (pushed_destination_cell == nullptr) {
            return rejected_move(MoveStatus::unsupported_geometry, direction, current);
        }

        Height pushed_new_bottom = push_target->bottom;
        if (const auto* const ramp =
                std::get_if<RampCell>(&pushed_destination_cell->geometry)) {
            const Height high_endpoint{
                static_cast<std::int32_t>(
                    static_cast<std::int64_t>(ramp->low_elevation) * 2 + 2)};
            if (direction != ramp->low_direction || push_target->bottom != high_endpoint) {
                return rejected_move(MoveStatus::unsupported_geometry, direction, current);
            }
            pushed_new_bottom = surface_height(*pushed_destination_cell);
        }

        const auto pushed_destination_fixture =
            std::find_if(level_->fixtures.begin(), level_->fixtures.end(),
                         [pushed_destination](const Fixture& value) {
                             return value.coordinate == *pushed_destination;
                         });
        if (pushed_destination_fixture != level_->fixtures.end()) {
            if (std::holds_alternative<ExitTeleporter>(pushed_destination_fixture->kind)) {
                return rejected_move(MoveStatus::teleporter_restriction, direction, current);
            }
            if (std::holds_alternative<Door>(pushed_destination_fixture->kind)
                && !detail::is_effectively_open_door(*level_, current,
                                                     *pushed_destination)) {
                return rejected_move(MoveStatus::closed_door, direction, current);
            }
        }

        const bool pushed_destination_occupied =
            std::any_of(current.entities.begin(), current.entities.end(),
                        [pushed_destination](const Entity& entity) {
                            return entity.coordinate == *pushed_destination;
                        });
        if (pushed_destination_occupied) {
            return rejected_move(MoveStatus::occupied, direction, current);
        }

        const Height pushed_destination_support = surface_height(*pushed_destination_cell);
        if (pushed_destination_support.half_steps > push_target->bottom.half_steps) {
            return rejected_move(MoveStatus::ledge, direction, current);
        }

        const ResolvedState initial = current;
        ResolvedState next = current;
        const auto next_player = std::find_if(
            next.entities.begin(), next.entities.end(), [player](const Entity& entity) {
                return entity.id == player->id;
            });
        const auto next_pushed = std::find_if(
            next.entities.begin(), next.entities.end(), [push_target](const Entity& entity) {
                return entity.id == push_target->id;
            });
        next_player->coordinate = *destination;
        next_pushed->coordinate = *pushed_destination;
        next_pushed->bottom = pushed_new_bottom;
        canonicalize_entities(next.entities);

        const EntityMovedEvent player_moved{
            player->id,
            player->coordinate,
            *destination,
            player->bottom,
            player->bottom,
            MovementCause::player,
        };
        const EntityMovedEvent entity_moved{
            push_target->id,
            push_target->coordinate,
            *pushed_destination,
            push_target->bottom,
            pushed_new_bottom,
            MovementCause::player,
        };
        TickResult movement_tick{
            0,
            {GameplayEvent{player_moved}, GameplayEvent{entity_moved}},
            next,
        };
        detail::resolve_fixtures_after_tick(*level_, movement_tick);
        std::vector<TickResult> ticks{movement_tick};
        ResolvedState resolved = movement_tick.state_after;
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

    Height player_new_bottom = player->bottom;
    const auto* const source_flat = std::get_if<FlatCell>(&source_cell->geometry);
    const auto* const source_ramp = std::get_if<RampCell>(&source_cell->geometry);
    const auto* const destination_flat =
        std::get_if<FlatCell>(&destination_cell->geometry);
    const auto* const destination_ramp =
        std::get_if<RampCell>(&destination_cell->geometry);

    if (source_flat != nullptr && destination_flat != nullptr) {
        if (destination_is_exit
            && (destination_entity_count != 0
                || player->bottom != Height::from_elevation(destination_flat->elevation))) {
            return rejected_move(MoveStatus::teleporter_restriction, direction, current);
        }
        if (destination_support != player->bottom) {
            const MoveStatus status =
                destination_entity_count != 0 ? MoveStatus::occupied : MoveStatus::ledge;
            return rejected_move(status, direction, current);
        }
    } else if (source_flat != nullptr && destination_ramp != nullptr) {
        if (destination_entity_count != 0
            || player->bottom != Height::from_elevation(source_flat->elevation)
            || (direction != destination_ramp->low_direction
                && direction != opposite(destination_ramp->low_direction))) {
            return rejected_move(MoveStatus::unsupported_geometry, direction, current);
        }
        player_new_bottom = surface_height(*destination_cell);
    } else if (source_ramp != nullptr && destination_flat != nullptr) {
        if (destination_entity_count != 0
            || player->bottom != surface_height(*source_cell)
            || (direction != source_ramp->low_direction
                && direction != opposite(source_ramp->low_direction))) {
            return rejected_move(MoveStatus::unsupported_geometry, direction, current);
        }
        player_new_bottom = Height::from_elevation(destination_flat->elevation);
    } else {
        return rejected_move(MoveStatus::unsupported_geometry, direction, current);
    }

    if (destination_is_exit) {
        const Height teleporter_floor = Height::from_elevation(destination_flat->elevation);
        if (destination_entity_count != 0 || player_new_bottom != teleporter_floor) {
            return rejected_move(MoveStatus::teleporter_restriction, direction, current);
        }
    }

    ResolvedState next = current;
    const auto next_player = std::find_if(next.entities.begin(), next.entities.end(), [player](const Entity& entity) {
        return entity.id == player->id;
    });
    next_player->coordinate = *destination;
    next_player->bottom = player_new_bottom;
    canonicalize_entities(next.entities);

    const EntityMovedEvent moved{
        player->id,
        player->coordinate,
        *destination,
        player->bottom,
        player_new_bottom,
        MovementCause::player,
    };
    const ResolvedState initial = current;
    TickResult tick{0, {GameplayEvent{moved}}, next};
    detail::resolve_fixtures_after_tick(*level_, tick);
    std::vector<TickResult> ticks{tick};
    ResolvedState resolved = tick.state_after;
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
