#include "explosions.hpp"

#include "fixtures.hpp"
#include "state_queries.hpp"
#include "world_queries.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <tuple>
#include <utility>
#include <vector>

namespace game_rules::detail {
namespace {

struct BlastTarget final {
    EntityId id{};
    EntityKind kind{EntityKind::box};
    Coordinate coordinate{};
    Height bottom{};
    std::array<bool, 4> impulses{};
    std::optional<Coordinate> destination{};
    bool movement_is_valid{};
    bool destination_conflicts{};
};

[[nodiscard]] constexpr std::size_t direction_index(const Direction direction) noexcept
{
    switch (direction) {
    case Direction::north: return 0;
    case Direction::east: return 1;
    case Direction::south: return 2;
    case Direction::west: return 3;
    }
    return 0;
}

[[nodiscard]] BlastTarget& find_or_add_target(std::vector<BlastTarget>& targets,
                                              const Entity& entity)
{
    const auto found = std::find_if(targets.begin(), targets.end(),
                                    [&entity](const BlastTarget& target) {
                                        return target.id == entity.id;
                                    });
    if (found != targets.end()) {
        return *found;
    }
    targets.push_back(BlastTarget{
        entity.id, entity.kind, entity.coordinate, entity.bottom});
    return targets.back();
}

[[nodiscard]] std::optional<Height> offset_height(const Height height,
                                                  const std::int32_t offset) noexcept
{
    const auto result = static_cast<std::int64_t>(height.half_steps) + offset;
    if (result < std::numeric_limits<std::int32_t>::min()
        || result > std::numeric_limits<std::int32_t>::max()) {
        return std::nullopt;
    }
    return Height{static_cast<std::int32_t>(result)};
}

[[nodiscard]] std::optional<Height> adjacent_target_bottom(
    const Cell& source_cell,
    const Height source_bottom,
    const Cell& target_cell,
    const Direction blast_direction) noexcept
{
    const auto* const source_ramp = std::get_if<RampCell>(&source_cell.geometry);
    const auto* const target_ramp = std::get_if<RampCell>(&target_cell.geometry);

    if (source_ramp == nullptr && target_ramp == nullptr) {
        return source_bottom;
    }
    if (source_ramp != nullptr && target_ramp != nullptr) {
        if (ramp_centers_align_laterally(
                *source_ramp, *target_ramp, blast_direction)) {
            return source_bottom;
        }
        if (!ramps_connect(*source_ramp, *target_ramp, blast_direction)) {
            return std::nullopt;
        }
    }

    std::optional<Height> target_bottom = source_bottom;
    if (source_ramp != nullptr) {
        if (blast_direction == source_ramp->low_direction) {
            target_bottom = offset_height(*target_bottom, -1);
        } else if (blast_direction == opposite(source_ramp->low_direction)) {
            target_bottom = offset_height(*target_bottom, 1);
        } else {
            return std::nullopt;
        }
        if (!target_bottom.has_value()) {
            return std::nullopt;
        }
    }

    if (target_ramp != nullptr) {
        const Direction target_to_source = opposite(blast_direction);
        if (target_to_source == target_ramp->low_direction) {
            target_bottom = offset_height(*target_bottom, 1);
        } else if (target_to_source == opposite(target_ramp->low_direction)) {
            target_bottom = offset_height(*target_bottom, -1);
        } else {
            return std::nullopt;
        }
    }
    return target_bottom;
}

[[nodiscard]] bool destination_fixture_blocks(const LevelDefinition& level,
                                               const ResolvedState& state,
                                               const Coordinate coordinate) noexcept
{
    const Fixture* const fixture = find_fixture(level, coordinate);
    if (fixture == nullptr) {
        return false;
    }
    if (std::holds_alternative<ExitTeleporter>(fixture->kind)) {
        return true;
    }
    return std::holds_alternative<Door>(fixture->kind)
        && !is_effectively_open_door(level, state, coordinate);
}

[[nodiscard]] bool can_pop(const LevelDefinition& level,
                           const ResolvedState& state,
                           const BlastTarget& target,
                           const Coordinate destination,
                           const Direction direction) noexcept
{
    if (!in_bounds(level, destination)
        || destination_fixture_blocks(level, state, destination)
        || !volume_is_clear(state, destination, target.bottom)) {
        return false;
    }
    const Cell* const source_cell = find_cell(level, target.coordinate);
    const Cell* const destination_cell = find_cell(level, destination);
    if (source_cell == nullptr || destination_cell == nullptr) {
        return false;
    }
    if (const auto* const source_ramp =
            std::get_if<RampCell>(&source_cell->geometry);
        source_ramp != nullptr
        && !ramp_endpoint_at(*source_ramp, direction).has_value()) {
        const auto* const destination_ramp =
            std::get_if<RampCell>(&destination_cell->geometry);
        if (destination_ramp == nullptr
            || !ramp_centers_align_laterally(
                *source_ramp, *destination_ramp, direction)) {
            return false;
        }
    }
    return support_half_steps(*destination_cell) <= target.bottom.half_steps;
}

[[nodiscard]] bool volumes_overlap(const BlastTarget& lhs,
                                   const BlastTarget& rhs) noexcept
{
    if (lhs.destination != rhs.destination) {
        return false;
    }
    const auto lhs_bottom = static_cast<std::int64_t>(lhs.bottom.half_steps);
    const auto rhs_bottom = static_cast<std::int64_t>(rhs.bottom.half_steps);
    return lhs_bottom < rhs_bottom + 2 && rhs_bottom < lhs_bottom + 2;
}

} // namespace

std::optional<TickResult> resolve_explosion_wave_tick(
    const LevelDefinition& level,
    const ResolvedState& state,
    const std::uint32_t tick_index)
{
    if (state.outcome != Outcome::ongoing) {
        return std::nullopt;
    }

    std::vector<const Entity*> sources;
    for (const Entity& entity : state.entities) {
        if (entity.kind == EntityKind::barrel && is_armed(state, entity.id)) {
            sources.push_back(&entity);
        }
    }
    if (sources.empty()) {
        return std::nullopt;
    }
    std::sort(sources.begin(), sources.end(), [](const Entity* const lhs,
                                                 const Entity* const rhs) {
        return std::tuple{lhs->coordinate.y, lhs->coordinate.x,
                          lhs->bottom.half_steps, lhs->id}
            < std::tuple{rhs->coordinate.y, rhs->coordinate.x,
                         rhs->bottom.half_steps, rhs->id};
    });
    std::vector<EntityId> source_ids;
    source_ids.reserve(sources.size());
    for (const Entity* const source : sources) {
        source_ids.push_back(source->id);
    }
    std::sort(source_ids.begin(), source_ids.end());
    const auto is_source = [&source_ids](const EntityId id) {
        return std::binary_search(source_ids.begin(), source_ids.end(), id);
    };

    std::vector<BlastTarget> targets;
    constexpr std::array directions{
        Direction::north, Direction::east, Direction::south, Direction::west};
    for (const Entity* const source : sources) {
        const Cell* const source_cell = find_cell(level, source->coordinate);
        if (source_cell == nullptr) {
            continue;
        }

        for (const Entity& entity : state.entities) {
            if (is_source(entity.id) || entity.coordinate != source->coordinate) {
                continue;
            }
            const auto difference = static_cast<std::int64_t>(entity.bottom.half_steps)
                - source->bottom.half_steps;
            if (difference == -2 || difference == 2) {
                static_cast<void>(find_or_add_target(targets, entity));
            }
        }

        for (const Direction direction : directions) {
            const std::optional<Coordinate> coordinate =
                step(source->coordinate, direction, level.coordinates);
            if (!coordinate.has_value() || !in_bounds(level, *coordinate)) {
                continue;
            }
            const Cell* const target_cell = find_cell(level, *coordinate);
            if (target_cell == nullptr) {
                continue;
            }
            const std::optional<Height> target_bottom = adjacent_target_bottom(
                *source_cell, source->bottom, *target_cell, direction);
            if (!target_bottom.has_value()) {
                continue;
            }
            const auto selected = std::find_if(state.entities.begin(), state.entities.end(),
                                               [coordinate, target_bottom](const Entity& entity) {
                                                   return entity.coordinate == *coordinate
                                                       && entity.bottom == *target_bottom;
                                               });
            if (selected != state.entities.end() && !is_source(selected->id)) {
                BlastTarget& target = find_or_add_target(targets, *selected);
                target.impulses[direction_index(direction)] = true;
            }
        }
    }

    std::sort(targets.begin(), targets.end(), [](const BlastTarget& lhs,
                                                 const BlastTarget& rhs) {
        return std::tuple{lhs.coordinate.y, lhs.coordinate.x,
                          lhs.bottom.half_steps, lhs.id}
            < std::tuple{rhs.coordinate.y, rhs.coordinate.x,
                         rhs.bottom.half_steps, rhs.id};
    });

    for (BlastTarget& target : targets) {
        const auto impulse_count = static_cast<std::size_t>(
            std::count(target.impulses.begin(), target.impulses.end(), true));
        if (target.kind == EntityKind::player || impulse_count != 1) {
            continue;
        }
        const auto impulse = std::find(target.impulses.begin(), target.impulses.end(), true);
        const Direction direction = directions[static_cast<std::size_t>(
            std::distance(target.impulses.begin(), impulse))];
        target.destination = step(target.coordinate, direction, level.coordinates);
        target.movement_is_valid = target.destination.has_value()
            && can_pop(level, state, target, *target.destination, direction);
    }
    for (std::size_t left = 0; left < targets.size(); ++left) {
        if (!targets[left].movement_is_valid) {
            continue;
        }
        for (std::size_t right = left + 1; right < targets.size(); ++right) {
            if (targets[right].movement_is_valid
                && volumes_overlap(targets[left], targets[right])) {
                targets[left].destination_conflicts = true;
                targets[right].destination_conflicts = true;
            }
        }
    }

    ResolvedState next = state;
    next.entities.erase(std::remove_if(next.entities.begin(), next.entities.end(),
                                       [&is_source](const Entity& entity) {
                                           return is_source(entity.id);
                                       }),
                        next.entities.end());
    next.armed_barrels.erase(
        std::remove_if(next.armed_barrels.begin(), next.armed_barrels.end(), is_source),
        next.armed_barrels.end());

    std::vector<GameplayEvent> events;
    events.reserve(sources.size() + targets.size() * 2);
    for (const Entity* const source : sources) {
        events.emplace_back(BarrelExplodedEvent{
            source->id, source->coordinate, source->bottom});
    }
    bool lost = false;
    for (const BlastTarget& target : targets) {
        if (target.kind == EntityKind::player) {
            lost = true;
            continue;
        }

        if (target.movement_is_valid && !target.destination_conflicts) {
            const auto entity = std::find_if(next.entities.begin(), next.entities.end(),
                                             [&target](const Entity& value) {
                                                 return value.id == target.id;
                                             });
            if (entity != next.entities.end()) {
                entity->coordinate = *target.destination;
                events.emplace_back(EntityMovedEvent{
                    target.id, target.coordinate, *target.destination,
                    target.bottom, target.bottom, MovementCause::blast});
            }
        }

        if (target.kind == EntityKind::barrel && !is_armed(state, target.id)) {
            arm_barrel(next, target.id);
            events.emplace_back(BarrelArmedEvent{target.id});
        }
    }

    if (lost) {
        next.outcome = Outcome::lost;
        events.emplace_back(LevelLostEvent{});
    }
    canonicalize_entities(next.entities);
    return TickResult{tick_index, std::move(events), std::move(next)};
}

} // namespace game_rules::detail
