#include "explosions.hpp"

#include "fixtures.hpp"

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
    std::optional<Direction> movement_direction{};
};

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

[[nodiscard]] bool in_bounds(const LevelDefinition& level,
                             const Coordinate coordinate) noexcept
{
    const auto offset_x = static_cast<std::int64_t>(coordinate.x) - level.coordinates.origin.x;
    const auto offset_y = static_cast<std::int64_t>(coordinate.y) - level.coordinates.origin.y;
    return offset_x >= 0 && offset_y >= 0
        && offset_x < static_cast<std::int64_t>(level.width)
        && offset_y < static_cast<std::int64_t>(level.height);
}

[[nodiscard]] const Cell* find_cell(const LevelDefinition& level,
                                    const Coordinate coordinate) noexcept
{
    const auto found = std::find_if(level.cells.begin(), level.cells.end(),
                                    [coordinate](const Cell& cell) {
                                        return cell.coordinate == coordinate;
                                    });
    return found == level.cells.end() ? nullptr : &*found;
}

[[nodiscard]] std::int64_t support_half_steps(const Cell& cell) noexcept
{
    if (const auto* const flat = std::get_if<FlatCell>(&cell.geometry)) {
        return static_cast<std::int64_t>(flat->elevation) * 2;
    }
    return static_cast<std::int64_t>(std::get<RampCell>(cell.geometry).low_elevation) * 2 + 1;
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
        return std::nullopt;
    }

    if (source_ramp != nullptr) {
        if (blast_direction == source_ramp->low_direction) {
            return offset_height(source_bottom, -1);
        }
        if (blast_direction == opposite(source_ramp->low_direction)) {
            return offset_height(source_bottom, 1);
        }
        return std::nullopt;
    }

    const Direction target_to_source = opposite(blast_direction);
    if (target_to_source == target_ramp->low_direction) {
        return offset_height(source_bottom, 1);
    }
    if (target_to_source == opposite(target_ramp->low_direction)) {
        return offset_height(source_bottom, -1);
    }
    return std::nullopt;
}

[[nodiscard]] bool destination_fixture_blocks(const LevelDefinition& level,
                                               const ResolvedState& state,
                                               const Coordinate coordinate) noexcept
{
    const auto fixture = std::find_if(level.fixtures.begin(), level.fixtures.end(),
                                      [coordinate](const Fixture& value) {
                                          return value.coordinate == coordinate;
                                      });
    if (fixture == level.fixtures.end()) {
        return false;
    }
    if (std::holds_alternative<ExitTeleporter>(fixture->kind)) {
        return true;
    }
    return std::holds_alternative<Door>(fixture->kind)
        && !is_effectively_open_door(level, state, coordinate);
}

[[nodiscard]] bool volume_is_clear(const ResolvedState& state,
                                   const Coordinate coordinate,
                                   const Height bottom) noexcept
{
    const auto arrival_bottom = static_cast<std::int64_t>(bottom.half_steps);
    const auto arrival_top = arrival_bottom + 2;
    return std::none_of(state.entities.begin(), state.entities.end(),
                        [coordinate, arrival_bottom, arrival_top](const Entity& entity) {
                            if (entity.coordinate != coordinate) {
                                return false;
                            }
                            const auto entity_bottom =
                                static_cast<std::int64_t>(entity.bottom.half_steps);
                            const auto entity_top = entity_bottom + 2;
                            return entity_bottom < arrival_top && arrival_bottom < entity_top;
                        });
}

[[nodiscard]] bool can_pop(const LevelDefinition& level,
                           const ResolvedState& state,
                           const BlastTarget& target,
                           const Coordinate destination) noexcept
{
    if (!in_bounds(level, destination)
        || destination_fixture_blocks(level, state, destination)
        || !volume_is_clear(state, destination, target.bottom)) {
        return false;
    }
    const Cell* const cell = find_cell(level, destination);
    return cell != nullptr && support_half_steps(*cell) <= target.bottom.half_steps;
}

[[nodiscard]] bool is_armed(const ResolvedState& state, const EntityId id) noexcept
{
    return std::binary_search(state.armed_barrels.begin(), state.armed_barrels.end(), id);
}

void arm_barrel(ResolvedState& state, const EntityId id)
{
    const auto position = std::lower_bound(state.armed_barrels.begin(),
                                           state.armed_barrels.end(), id);
    if (position == state.armed_barrels.end() || *position != id) {
        state.armed_barrels.insert(position, id);
    }
}

void canonicalize_entities(std::vector<Entity>& entities)
{
    std::sort(entities.begin(), entities.end(), [](const Entity& lhs, const Entity& rhs) {
        return std::tuple{lhs.coordinate.y, lhs.coordinate.x, lhs.bottom.half_steps, lhs.id}
            < std::tuple{rhs.coordinate.y, rhs.coordinate.x, rhs.bottom.half_steps, rhs.id};
    });
}

} // namespace

std::optional<TickResult> resolve_single_explosion_tick(
    const LevelDefinition& level,
    const ResolvedState& state,
    const std::uint32_t tick_index)
{
    if (state.outcome != Outcome::ongoing) {
        return std::nullopt;
    }

    std::vector<const Entity*> ready;
    for (const EntityId id : state.armed_barrels) {
        const auto entity = std::find_if(state.entities.begin(), state.entities.end(),
                                         [id](const Entity& value) {
                                             return value.id == id
                                                 && value.kind == EntityKind::barrel;
                                         });
        if (entity != state.entities.end()) {
            ready.push_back(&*entity);
        }
    }
    if (ready.size() != 1) {
        return std::nullopt;
    }

    const Entity source = *ready.front();
    const Cell* const source_cell = find_cell(level, source.coordinate);
    if (source_cell == nullptr) {
        return std::nullopt;
    }

    std::vector<BlastTarget> targets;
    for (const Entity& entity : state.entities) {
        if (entity.id == source.id || entity.coordinate != source.coordinate) {
            continue;
        }
        const auto difference = static_cast<std::int64_t>(entity.bottom.half_steps)
            - source.bottom.half_steps;
        if (difference == -2 || difference == 2) {
            targets.push_back(BlastTarget{
                entity.id, entity.kind, entity.coordinate, entity.bottom, std::nullopt});
        }
    }

    constexpr std::array directions{
        Direction::north, Direction::east, Direction::south, Direction::west};
    for (const Direction direction : directions) {
        const std::optional<Coordinate> coordinate =
            step(source.coordinate, direction, level.coordinates);
        if (!coordinate.has_value() || !in_bounds(level, *coordinate)) {
            continue;
        }
        const Cell* const target_cell = find_cell(level, *coordinate);
        if (target_cell == nullptr) {
            continue;
        }
        const std::optional<Height> target_bottom = adjacent_target_bottom(
            *source_cell, source.bottom, *target_cell, direction);
        if (!target_bottom.has_value()) {
            continue;
        }
        const auto selected = std::find_if(state.entities.begin(), state.entities.end(),
                                           [coordinate, target_bottom](const Entity& entity) {
                                               return entity.coordinate == *coordinate
                                                   && entity.bottom == *target_bottom;
                                           });
        if (selected != state.entities.end()) {
            targets.push_back(BlastTarget{selected->id, selected->kind,
                                          selected->coordinate, selected->bottom, direction});
        }
    }

    std::sort(targets.begin(), targets.end(), [](const BlastTarget& lhs,
                                                 const BlastTarget& rhs) {
        return std::tuple{lhs.coordinate.y, lhs.coordinate.x,
                          lhs.bottom.half_steps, lhs.id}
            < std::tuple{rhs.coordinate.y, rhs.coordinate.x,
                         rhs.bottom.half_steps, rhs.id};
    });

    ResolvedState next = state;
    next.entities.erase(std::remove_if(next.entities.begin(), next.entities.end(),
                                       [&source](const Entity& entity) {
                                           return entity.id == source.id;
                                       }),
                        next.entities.end());
    const auto armed_source = std::lower_bound(next.armed_barrels.begin(),
                                               next.armed_barrels.end(), source.id);
    if (armed_source != next.armed_barrels.end() && *armed_source == source.id) {
        next.armed_barrels.erase(armed_source);
    }

    std::vector<GameplayEvent> events{
        GameplayEvent{BarrelExplodedEvent{source.id, source.coordinate, source.bottom}}};
    bool lost = false;
    for (const BlastTarget& target : targets) {
        if (target.kind == EntityKind::player) {
            lost = true;
            continue;
        }

        if (target.movement_direction.has_value()) {
            const std::optional<Coordinate> destination = step(
                target.coordinate, *target.movement_direction, level.coordinates);
            if (destination.has_value() && can_pop(level, state, target, *destination)) {
                const auto entity = std::find_if(next.entities.begin(), next.entities.end(),
                                                 [&target](const Entity& value) {
                                                     return value.id == target.id;
                                                 });
                if (entity != next.entities.end()) {
                    entity->coordinate = *destination;
                    events.emplace_back(EntityMovedEvent{
                        target.id, target.coordinate, *destination,
                        target.bottom, target.bottom, MovementCause::blast});
                }
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
