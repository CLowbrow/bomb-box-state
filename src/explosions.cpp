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
    std::optional<Direction> movement_direction{};
};

[[nodiscard]] std::optional<Height> offset_height(const Height height,
                                                  const std::int32_t offset) noexcept {
    const auto result = static_cast<std::int64_t>(height.half_steps) + offset;
    if (result < std::numeric_limits<std::int32_t>::min() ||
        result > std::numeric_limits<std::int32_t>::max()) {
        return std::nullopt;
    }
    return Height{static_cast<std::int32_t>(result)};
}

[[nodiscard]] std::optional<Height>
adjacent_target_bottom(const Cell& source_cell, const Height source_bottom, const Cell& target_cell,
                       const Direction blast_direction) noexcept {
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
                                              const Coordinate coordinate) noexcept {
    const Fixture* const fixture = find_fixture(level, coordinate);
    if (fixture == nullptr) {
        return false;
    }
    if (std::holds_alternative<ExitTeleporter>(fixture->kind)) {
        return true;
    }
    return std::holds_alternative<Door>(fixture->kind) &&
           !is_effectively_open_door(level, state, coordinate);
}

[[nodiscard]] bool volume_is_clear(const ResolvedState& state, const Coordinate coordinate,
                                   const Height bottom) noexcept {
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

[[nodiscard]] bool can_pop(const LevelDefinition& level, const ResolvedState& state,
                           const BlastTarget& target, const Coordinate destination) noexcept {
    if (!in_bounds(level, destination) || destination_fixture_blocks(level, state, destination) ||
        !volume_is_clear(state, destination, target.bottom)) {
        return false;
    }
    const Cell* const cell = find_cell(level, destination);
    return cell != nullptr && support_half_steps(*cell) <= target.bottom.half_steps;
}

} // namespace

std::optional<TickResult> resolve_explosion_wave_tick(const LevelDefinition& level,
                                                      const ResolvedState& state,
                                                      const std::uint32_t tick_index) {
    if (state.outcome != Outcome::ongoing) {
        return std::nullopt;
    }

    std::vector<Entity> sources;
    for (const EntityId id : state.armed_barrels) {
        const auto entity =
            std::find_if(state.entities.begin(), state.entities.end(), [id](const Entity& value) {
                return value.id == id && value.kind == EntityKind::barrel;
            });
        if (entity != state.entities.end() && find_cell(level, entity->coordinate) != nullptr) {
            sources.push_back(*entity);
        }
    }
    if (sources.empty()) {
        return std::nullopt;
    }
    std::sort(sources.begin(), sources.end(), [](const Entity& lhs, const Entity& rhs) {
        return std::tuple{lhs.coordinate.y, lhs.coordinate.x, lhs.bottom.half_steps, lhs.id} <
               std::tuple{rhs.coordinate.y, rhs.coordinate.x, rhs.bottom.half_steps, rhs.id};
    });

    struct WaveTarget final {
        EntityId id{};
        EntityKind kind{EntityKind::box};
        Coordinate coordinate{};
        Height bottom{};
        bool same_cell_touch{false};
        std::vector<Direction> impulses{};
    };

    std::vector<WaveTarget> targets;
    const auto source_is_exploding = [&sources](const EntityId id) {
        return std::any_of(sources.begin(), sources.end(),
                           [id](const Entity& source) { return source.id == id; });
    };
    const auto add_target = [&targets](const Entity& entity, const bool same_cell_touch,
                                       const std::optional<Direction> impulse) {
        auto found =
            std::find_if(targets.begin(), targets.end(),
                         [&entity](const WaveTarget& target) { return target.id == entity.id; });
        if (found == targets.end()) {
            targets.push_back(WaveTarget{entity.id, entity.kind, entity.coordinate, entity.bottom});
            found = std::prev(targets.end());
        }
        found->same_cell_touch = found->same_cell_touch || same_cell_touch;
        if (impulse.has_value() && std::find(found->impulses.begin(), found->impulses.end(),
                                             *impulse) == found->impulses.end()) {
            found->impulses.push_back(*impulse);
        }
    };

    constexpr std::array directions{Direction::north, Direction::east, Direction::south,
                                    Direction::west};
    for (const Entity& source : sources) {
        const Cell* const source_cell = find_cell(level, source.coordinate);
        if (source_cell == nullptr) {
            continue;
        }
        for (const Entity& entity : state.entities) {
            if (entity.id == source.id || entity.coordinate != source.coordinate) {
                continue;
            }
            const auto difference =
                static_cast<std::int64_t>(entity.bottom.half_steps) - source.bottom.half_steps;
            if ((difference == -2 || difference == 2) && !source_is_exploding(entity.id)) {
                add_target(entity, true, std::nullopt);
            }
        }

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
            const std::optional<Height> target_bottom =
                adjacent_target_bottom(*source_cell, source.bottom, *target_cell, direction);
            if (!target_bottom.has_value()) {
                continue;
            }
            const auto selected = std::find_if(state.entities.begin(), state.entities.end(),
                                               [coordinate, target_bottom](const Entity& entity) {
                                                   return entity.coordinate == *coordinate &&
                                                          entity.bottom == *target_bottom;
                                               });
            if (selected != state.entities.end() && !source_is_exploding(selected->id)) {
                add_target(*selected, false, direction);
            }
        }
    }

    std::sort(targets.begin(), targets.end(), [](const WaveTarget& lhs, const WaveTarget& rhs) {
        return std::tuple{lhs.coordinate.y, lhs.coordinate.x, lhs.bottom.half_steps, lhs.id} <
               std::tuple{rhs.coordinate.y, rhs.coordinate.x, rhs.bottom.half_steps, rhs.id};
    });

    struct PlannedMove final {
        EntityId id{};
        Coordinate from{};
        Coordinate to{};
        Height bottom{};
    };
    std::vector<PlannedMove> planned_moves;
    for (const WaveTarget& target : targets) {
        if (target.kind != EntityKind::box && target.kind != EntityKind::barrel) {
            continue;
        }
        if (target.impulses.size() != 1U) {
            continue;
        }
        const std::optional<Coordinate> destination =
            step(target.coordinate, target.impulses.front(), level.coordinates);
        if (destination.has_value() &&
            can_pop(level, state,
                    BlastTarget{target.id, target.kind, target.coordinate, target.bottom,
                                target.impulses.front()},
                    *destination)) {
            planned_moves.push_back(
                PlannedMove{target.id, target.coordinate, *destination, target.bottom});
        }
    }
    std::vector<EntityId> conflicted_moves;
    for (std::size_t i = 0; i < planned_moves.size(); ++i) {
        for (std::size_t j = i + 1; j < planned_moves.size(); ++j) {
            if (planned_moves[i].to == planned_moves[j].to &&
                planned_moves[i].bottom.half_steps < planned_moves[j].bottom.half_steps + 2 &&
                planned_moves[j].bottom.half_steps < planned_moves[i].bottom.half_steps + 2) {
                conflicted_moves.push_back(planned_moves[i].id);
                conflicted_moves.push_back(planned_moves[j].id);
            }
        }
    }
    std::sort(conflicted_moves.begin(), conflicted_moves.end());
    conflicted_moves.erase(std::unique(conflicted_moves.begin(), conflicted_moves.end()),
                           conflicted_moves.end());

    ResolvedState next = state;
    for (const Entity& source : sources) {
        next.entities.erase(
            std::remove_if(next.entities.begin(), next.entities.end(),
                           [&source](const Entity& entity) { return entity.id == source.id; }),
            next.entities.end());
        const auto armed_source =
            std::lower_bound(next.armed_barrels.begin(), next.armed_barrels.end(), source.id);
        if (armed_source != next.armed_barrels.end() && *armed_source == source.id) {
            next.armed_barrels.erase(armed_source);
        }
    }

    std::vector<GameplayEvent> events;
    for (const Entity& source : sources) {
        events.emplace_back(BarrelExplodedEvent{source.id, source.coordinate, source.bottom});
    }

    bool lost = false;
    for (const WaveTarget& target : targets) {
        if (target.kind == EntityKind::player) {
            lost = true;
            continue;
        }

        const auto planned =
            std::find_if(planned_moves.begin(), planned_moves.end(),
                         [&target](const PlannedMove& move) { return move.id == target.id; });
        const bool movement_conflicted =
            std::binary_search(conflicted_moves.begin(), conflicted_moves.end(), target.id);
        if (planned != planned_moves.end() && !movement_conflicted) {
            const auto entity =
                std::find_if(next.entities.begin(), next.entities.end(),
                             [&target](const Entity& value) { return value.id == target.id; });
            if (entity != next.entities.end()) {
                entity->coordinate = planned->to;
                events.emplace_back(EntityMovedEvent{target.id, target.coordinate, planned->to,
                                                     target.bottom, target.bottom,
                                                     MovementCause::blast});
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
