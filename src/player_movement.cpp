#include "player_movement.hpp"

#include "fixtures.hpp"
#include "state_queries.hpp"
#include "world_queries.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <utility>

namespace game_rules::detail {
namespace {

[[nodiscard]] PlayerMovementPlan rejected(const MoveStatus status)
{
    return PlayerMovementPlan{status, std::nullopt};
}

[[nodiscard]] PlayerMovementPlan accepted(TickResult tick)
{
    return PlayerMovementPlan{MoveStatus::moved, std::move(tick)};
}

} // namespace

PlayerMovementPlan plan_player_movement(const LevelDefinition& level,
                                        const ResolvedState& state,
                                        const Direction direction)
{
    const auto player = std::find_if(state.entities.begin(), state.entities.end(),
                                     [](const Entity& entity) {
                                         return entity.kind == EntityKind::player;
                                     });
    if (player == state.entities.end()) {
        return rejected(MoveStatus::no_level);
    }

    const std::optional<Coordinate> destination =
        step(player->coordinate, direction, level.coordinates);
    if (!destination.has_value() || !in_bounds(level, *destination)) {
        return rejected(MoveStatus::world_boundary);
    }

    const Cell* const source_cell = find_cell(level, player->coordinate);
    const Cell* const destination_cell = find_cell(level, *destination);
    if (source_cell == nullptr || destination_cell == nullptr) {
        return rejected(MoveStatus::unsupported_geometry);
    }

    const auto* const source_flat = std::get_if<FlatCell>(&source_cell->geometry);
    const auto* const source_ramp = std::get_if<RampCell>(&source_cell->geometry);
    const auto* const destination_flat =
        std::get_if<FlatCell>(&destination_cell->geometry);
    const auto* const destination_ramp =
        std::get_if<RampCell>(&destination_cell->geometry);

    std::optional<Height> push_contact_bottom;
    if (source_flat != nullptr && destination_flat != nullptr) {
        push_contact_bottom = player->bottom;
    } else if (source_ramp != nullptr && destination_flat != nullptr
               && player->bottom == surface_height(*source_cell)
               && (direction == source_ramp->low_direction
                   || direction == opposite(source_ramp->low_direction))) {
        push_contact_bottom = Height::from_elevation(destination_flat->elevation);
    }

    const Fixture* const fixture = find_fixture(level, *destination);
    const bool destination_is_exit = fixture != nullptr
        && std::holds_alternative<ExitTeleporter>(fixture->kind);
    if (fixture != nullptr && std::holds_alternative<Door>(fixture->kind)
        && !is_effectively_open_door(level, state, *destination)) {
        return rejected(MoveStatus::closed_door);
    }

    Height destination_support = surface_height(*destination_cell);
    std::size_t destination_entity_count = 0;
    const Entity* push_target = nullptr;
    const Entity* destination_top = nullptr;
    for (const Entity& entity : state.entities) {
        if (entity.coordinate != *destination) {
            continue;
        }
        ++destination_entity_count;
        if (destination_top == nullptr
            || entity.bottom.half_steps > destination_top->bottom.half_steps) {
            destination_top = &entity;
        }
        if (push_contact_bottom.has_value() && entity.bottom == *push_contact_bottom
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
            return rejected(MoveStatus::unsupported_geometry);
        }
        if (push_target != destination_top) {
            return rejected(MoveStatus::stacked_push_target);
        }
        if (destination_is_exit) {
            return rejected(MoveStatus::teleporter_restriction);
        }

        const std::optional<Coordinate> pushed_destination =
            step(push_target->coordinate, direction, level.coordinates);
        if (!pushed_destination.has_value() || !in_bounds(level, *pushed_destination)) {
            return rejected(MoveStatus::world_boundary);
        }

        const Cell* const pushed_destination_cell = find_cell(level, *pushed_destination);
        if (pushed_destination_cell == nullptr) {
            return rejected(MoveStatus::unsupported_geometry);
        }

        Height pushed_new_bottom = push_target->bottom;
        if (const auto* const ramp =
                std::get_if<RampCell>(&pushed_destination_cell->geometry)) {
            const Height high_endpoint{static_cast<std::int32_t>(
                static_cast<std::int64_t>(ramp->low_elevation) * 2 + 2)};
            if (direction != ramp->low_direction || push_target->bottom != high_endpoint) {
                return rejected(MoveStatus::unsupported_geometry);
            }
            pushed_new_bottom = surface_height(*pushed_destination_cell);
        }

        const Fixture* const pushed_destination_fixture =
            find_fixture(level, *pushed_destination);
        if (pushed_destination_fixture != nullptr) {
            if (std::holds_alternative<ExitTeleporter>(pushed_destination_fixture->kind)) {
                return rejected(MoveStatus::teleporter_restriction);
            }
            if (std::holds_alternative<Door>(pushed_destination_fixture->kind)
                && !is_effectively_open_door(level, state, *pushed_destination)) {
                return rejected(MoveStatus::closed_door);
            }
        }

        if (!volume_is_clear(state, *pushed_destination, pushed_new_bottom)) {
            return rejected(MoveStatus::occupied);
        }
        if (surface_height(*pushed_destination_cell).half_steps
            > push_target->bottom.half_steps) {
            return rejected(MoveStatus::ledge);
        }

        ResolvedState next = state;
        const auto next_player = std::find_if(
            next.entities.begin(), next.entities.end(), [player](const Entity& entity) {
                return entity.id == player->id;
            });
        const auto next_pushed = std::find_if(
            next.entities.begin(), next.entities.end(), [push_target](const Entity& entity) {
                return entity.id == push_target->id;
        });
        next_player->coordinate = *destination;
        next_player->bottom = *push_contact_bottom;
        next_pushed->coordinate = *pushed_destination;
        next_pushed->bottom = pushed_new_bottom;
        canonicalize_entities(next.entities);

        return accepted(TickResult{
            0,
            {
                GameplayEvent{EntityMovedEvent{
                    player->id,
                    player->coordinate,
                    *destination,
                    player->bottom,
                    *push_contact_bottom,
                    MovementCause::player,
                }},
                GameplayEvent{EntityMovedEvent{
                    push_target->id,
                    push_target->coordinate,
                    *pushed_destination,
                    push_target->bottom,
                    pushed_new_bottom,
                    MovementCause::player,
                }},
            },
            std::move(next),
        });
    }

    Height player_new_bottom = player->bottom;

    if (source_flat != nullptr && destination_flat != nullptr) {
        if (destination_is_exit
            && (destination_entity_count != 0
                || player->bottom != Height::from_elevation(destination_flat->elevation))) {
            return rejected(MoveStatus::teleporter_restriction);
        }
        if (destination_support != player->bottom) {
            return rejected(destination_entity_count != 0 ? MoveStatus::occupied
                                                          : MoveStatus::ledge);
        }
    } else if (source_flat != nullptr && destination_ramp != nullptr) {
        if (destination_entity_count != 0
            || player->bottom != Height::from_elevation(source_flat->elevation)
            || (direction != destination_ramp->low_direction
                && direction != opposite(destination_ramp->low_direction))) {
            return rejected(MoveStatus::unsupported_geometry);
        }
        player_new_bottom = surface_height(*destination_cell);
    } else if (source_ramp != nullptr && destination_flat != nullptr) {
        if (destination_entity_count != 0
            || player->bottom != surface_height(*source_cell)
            || (direction != source_ramp->low_direction
                && direction != opposite(source_ramp->low_direction))) {
            return rejected(MoveStatus::unsupported_geometry);
        }
        player_new_bottom = Height::from_elevation(destination_flat->elevation);
    } else if (source_ramp != nullptr && destination_ramp != nullptr) {
        if (destination_entity_count != 0
            || player->bottom != surface_height(*source_cell)
            || !ramps_connect(*source_ramp, *destination_ramp, direction)) {
            return rejected(MoveStatus::unsupported_geometry);
        }
        player_new_bottom = surface_height(*destination_cell);
    } else {
        return rejected(MoveStatus::unsupported_geometry);
    }

    if (destination_is_exit) {
        const Height teleporter_floor = Height::from_elevation(destination_flat->elevation);
        if (destination_entity_count != 0 || player_new_bottom != teleporter_floor) {
            return rejected(MoveStatus::teleporter_restriction);
        }
    }

    ResolvedState next = state;
    const auto next_player = std::find_if(
        next.entities.begin(), next.entities.end(), [player](const Entity& entity) {
            return entity.id == player->id;
        });
    next_player->coordinate = *destination;
    next_player->bottom = player_new_bottom;
    canonicalize_entities(next.entities);

    return accepted(TickResult{
        0,
        {GameplayEvent{EntityMovedEvent{
            player->id,
            player->coordinate,
            *destination,
            player->bottom,
            player_new_bottom,
            MovementCause::player,
        }}},
        std::move(next),
    });
}

} // namespace game_rules::detail
