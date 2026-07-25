#include "game_rules/world.hpp"

#include "world_queries.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <optional>
#include <set>
#include <tuple>

namespace game_rules {
namespace {

[[nodiscard]] bool valid(const HorizontalAxisDirection direction) noexcept
{
    return direction == HorizontalAxisDirection::east || direction == HorizontalAxisDirection::west;
}

[[nodiscard]] bool valid(const VerticalAxisDirection direction) noexcept
{
    return direction == VerticalAxisDirection::north || direction == VerticalAxisDirection::south;
}

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

[[nodiscard]] bool valid(const SwitchColor color) noexcept
{
    switch (color) {
    case SwitchColor::red:
    case SwitchColor::green:
    case SwitchColor::blue:
    case SwitchColor::yellow:
        return true;
    }
    return false;
}

[[nodiscard]] bool valid(const EntityKind kind) noexcept
{
    return kind == EntityKind::player || kind == EntityKind::box || kind == EntityKind::barrel;
}

[[nodiscard]] bool representable_cell_height(const std::int32_t elevation,
                                             const bool needs_high_endpoint = false) noexcept
{
    const auto low_half_steps = static_cast<std::int64_t>(elevation) * 2;
    const auto highest_half_steps = low_half_steps + (needs_high_endpoint ? 2 : 0);
    const auto ramp_center_half_steps = low_half_steps + (needs_high_endpoint ? 1 : 0);
    return low_half_steps >= std::numeric_limits<std::int32_t>::min()
        && ramp_center_half_steps >= std::numeric_limits<std::int32_t>::min()
        && highest_half_steps <= std::numeric_limits<std::int32_t>::max();
}

void add_error(ValidationResult& result,
               const ValidationErrorCode code,
               const Coordinate coordinate = {},
               const EntityId entity_id = 0)
{
    result.errors.push_back(ValidationError{code, coordinate, entity_id});
}

} // namespace

std::string_view to_string(const ValidationErrorCode code) noexcept
{
    switch (code) {
    case ValidationErrorCode::invalid_dimensions: return "invalid_dimensions";
    case ValidationErrorCode::invalid_coordinate_system: return "invalid_coordinate_system";
    case ValidationErrorCode::cell_count_mismatch: return "cell_count_mismatch";
    case ValidationErrorCode::cell_out_of_bounds: return "cell_out_of_bounds";
    case ValidationErrorCode::duplicate_cell: return "duplicate_cell";
    case ValidationErrorCode::invalid_cell_height: return "invalid_cell_height";
    case ValidationErrorCode::invalid_ramp_direction: return "invalid_ramp_direction";
    case ValidationErrorCode::invalid_ramp_endpoints: return "invalid_ramp_endpoints";
    case ValidationErrorCode::fixture_out_of_bounds: return "fixture_out_of_bounds";
    case ValidationErrorCode::fixture_on_ramp: return "fixture_on_ramp";
    case ValidationErrorCode::duplicate_fixture: return "duplicate_fixture";
    case ValidationErrorCode::invalid_fixture_color: return "invalid_fixture_color";
    case ValidationErrorCode::entity_out_of_bounds: return "entity_out_of_bounds";
    case ValidationErrorCode::duplicate_entity_id: return "duplicate_entity_id";
    case ValidationErrorCode::invalid_entity_kind: return "invalid_entity_kind";
    case ValidationErrorCode::entity_below_surface: return "entity_below_surface";
    case ValidationErrorCode::overlapping_entities: return "overlapping_entities";
    case ValidationErrorCode::player_not_top_of_stack: return "player_not_top_of_stack";
    case ValidationErrorCode::player_count_not_one: return "player_count_not_one";
    case ValidationErrorCode::invalid_teleporter_occupancy: return "invalid_teleporter_occupancy";
    case ValidationErrorCode::invalid_entity_id: return "invalid_entity_id";
    }
    return "unknown";
}

ValidationResult validate_level(const LevelDefinition& level)
{
    ValidationResult result;
    if (!detail::has_representable_extent(level)) {
        add_error(result, ValidationErrorCode::invalid_dimensions, level.coordinates.origin);
    }
    if (!valid(level.coordinates.positive_x) || !valid(level.coordinates.positive_y)) {
        add_error(result, ValidationErrorCode::invalid_coordinate_system, level.coordinates.origin);
    }

    const auto expected_cells = static_cast<std::uint64_t>(level.width)
        * static_cast<std::uint64_t>(level.height);
    if (expected_cells != static_cast<std::uint64_t>(level.cells.size())) {
        add_error(result, ValidationErrorCode::cell_count_mismatch, level.coordinates.origin);
    }

    std::map<Coordinate, const Cell*, detail::CoordinateLess> cells;
    for (const Cell& cell : level.cells) {
        if (!detail::in_bounds(level, cell.coordinate)) {
            add_error(result, ValidationErrorCode::cell_out_of_bounds, cell.coordinate);
            continue;
        }
        const auto [iterator, inserted] = cells.emplace(cell.coordinate, &cell);
        static_cast<void>(iterator);
        if (!inserted) {
            add_error(result, ValidationErrorCode::duplicate_cell, cell.coordinate);
        }

        if (const auto* flat = std::get_if<FlatCell>(&cell.geometry)) {
            if (!representable_cell_height(flat->elevation)) {
                add_error(result, ValidationErrorCode::invalid_cell_height, cell.coordinate);
            }
        } else if (const auto* ramp = std::get_if<RampCell>(&cell.geometry)) {
            if (!valid(ramp->low_direction)) {
                add_error(result, ValidationErrorCode::invalid_ramp_direction, cell.coordinate);
            }
            if (!representable_cell_height(ramp->low_elevation, true)) {
                add_error(result, ValidationErrorCode::invalid_cell_height, cell.coordinate);
            }
        }
    }

    for (const auto& [coordinate, cell] : cells) {
        const auto* ramp = std::get_if<RampCell>(&cell->geometry);
        if (ramp == nullptr || !valid(ramp->low_direction)) {
            continue;
        }
        const auto low_coordinate = detail::step(
            coordinate, ramp->low_direction, level.coordinates);
        const auto high_coordinate = detail::step(
            coordinate, detail::opposite(ramp->low_direction), level.coordinates);
        if (!low_coordinate || !high_coordinate) {
            add_error(result, ValidationErrorCode::invalid_ramp_endpoints, coordinate);
            continue;
        }
        const auto endpoint_is_valid = [ramp](const Direction direction,
                                              const Cell& neighbor) {
            const std::optional<detail::RampEndpoint> endpoint =
                detail::ramp_endpoint_at(*ramp, direction);
            if (!endpoint.has_value()) {
                return false;
            }
            if (const auto* flat = std::get_if<FlatCell>(&neighbor.geometry)) {
                return static_cast<std::int64_t>(flat->elevation) * 2
                    == detail::ramp_endpoint_half_steps(*ramp, *endpoint);
            }
            return detail::ramps_connect(
                *ramp, std::get<RampCell>(neighbor.geometry), direction);
        };
        const auto low = cells.find(*low_coordinate);
        const auto high = cells.find(*high_coordinate);
        if (low == cells.end() || high == cells.end()
            || !endpoint_is_valid(ramp->low_direction, *low->second)
            || !endpoint_is_valid(detail::opposite(ramp->low_direction), *high->second)) {
            add_error(result, ValidationErrorCode::invalid_ramp_endpoints, coordinate);
        }
    }

    std::map<Coordinate, const Fixture*, detail::CoordinateLess> fixtures;
    for (const Fixture& fixture : level.fixtures) {
        if (!detail::in_bounds(level, fixture.coordinate)) {
            add_error(result, ValidationErrorCode::fixture_out_of_bounds, fixture.coordinate);
            continue;
        }
        const auto cell_iterator = cells.find(fixture.coordinate);
        if (cell_iterator == cells.end()) {
            add_error(result, ValidationErrorCode::fixture_out_of_bounds, fixture.coordinate);
            continue;
        }
        if (std::holds_alternative<RampCell>(cell_iterator->second->geometry)) {
            add_error(result, ValidationErrorCode::fixture_on_ramp, fixture.coordinate);
        }
        const auto [fixture_iterator, inserted] = fixtures.emplace(fixture.coordinate, &fixture);
        static_cast<void>(fixture_iterator);
        if (!inserted) {
            add_error(result, ValidationErrorCode::duplicate_fixture, fixture.coordinate);
        }
        if (const auto* switch_fixture = std::get_if<Switch>(&fixture.kind)) {
            if (!valid(switch_fixture->color)) {
                add_error(result, ValidationErrorCode::invalid_fixture_color, fixture.coordinate);
            }
        } else if (const auto* door = std::get_if<Door>(&fixture.kind); door != nullptr && !valid(door->color)) {
            add_error(result, ValidationErrorCode::invalid_fixture_color, fixture.coordinate);
        }
    }

    std::set<EntityId> entity_ids;
    std::map<Coordinate, std::vector<const Entity*>, detail::CoordinateLess> columns;
    std::size_t player_count = 0;
    for (const Entity& entity : level.entities) {
        if (entity.kind == EntityKind::player) {
            ++player_count;
        }
        if (!valid(entity.kind)) {
            add_error(result, ValidationErrorCode::invalid_entity_kind, entity.coordinate, entity.id);
        }
        if (entity.id == 0) {
            add_error(result, ValidationErrorCode::invalid_entity_id, entity.coordinate, entity.id);
        }
        if (!entity_ids.insert(entity.id).second) {
            add_error(result, ValidationErrorCode::duplicate_entity_id, entity.coordinate, entity.id);
        }
        if (!detail::in_bounds(level, entity.coordinate)) {
            add_error(result, ValidationErrorCode::entity_out_of_bounds, entity.coordinate, entity.id);
            continue;
        }
        const auto cell = cells.find(entity.coordinate);
        if (cell == cells.end() || cell->second == nullptr) {
            add_error(result, ValidationErrorCode::entity_out_of_bounds, entity.coordinate, entity.id);
            continue;
        }
        if (static_cast<std::int64_t>(entity.bottom.half_steps)
            < detail::support_half_steps(*cell->second)) {
            add_error(result, ValidationErrorCode::entity_below_surface, entity.coordinate, entity.id);
        }
        columns[entity.coordinate].push_back(&entity);
    }
    if (player_count != 1) {
        add_error(result, ValidationErrorCode::player_count_not_one);
    }

    for (auto& [coordinate, column] : columns) {
        std::sort(column.begin(), column.end(), [](const Entity* lhs, const Entity* rhs) {
            return std::tie(lhs->bottom.half_steps, lhs->id)
                < std::tie(rhs->bottom.half_steps, rhs->id);
        });
        for (std::size_t index = 1; index < column.size(); ++index) {
            const auto previous_top = static_cast<std::int64_t>(column[index - 1]->bottom.half_steps) + 2;
            if (column[index]->bottom.half_steps < previous_top) {
                add_error(result, ValidationErrorCode::overlapping_entities,
                          coordinate, column[index]->id);
            }
        }
        const auto player = std::find_if(column.begin(), column.end(), [](const Entity* entity) {
            return entity->kind == EntityKind::player;
        });
        if (player != column.end() && std::next(player) != column.end()) {
            add_error(result, ValidationErrorCode::player_not_top_of_stack,
                      coordinate, (*player)->id);
        }

        const auto fixture = fixtures.find(coordinate);
        if (fixture != fixtures.end() && std::holds_alternative<ExitTeleporter>(fixture->second->kind)) {
            const auto cell = cells.find(coordinate);
            const bool sole_player = column.size() == 1 && column.front()->kind == EntityKind::player;
            const bool at_floor = cell != cells.end()
                && column.front()->bottom.half_steps
                    == detail::support_half_steps(*cell->second);
            if (!sole_player || !at_floor) {
                add_error(result, ValidationErrorCode::invalid_teleporter_occupancy, coordinate,
                          column.front()->id);
            }
        }
    }

    std::sort(result.errors.begin(), result.errors.end(), [](const ValidationError lhs,
                                                            const ValidationError rhs) {
        return std::tuple{lhs.code, lhs.coordinate.y, lhs.coordinate.x, lhs.entity_id}
            < std::tuple{rhs.code, rhs.coordinate.y, rhs.coordinate.x, rhs.entity_id};
    });
    return result;
}

LevelDefinition canonicalize_level(LevelDefinition level)
{
    std::sort(level.cells.begin(), level.cells.end(), [](const Cell& lhs, const Cell& rhs) {
        return detail::spatial_key(lhs.coordinate) < detail::spatial_key(rhs.coordinate);
    });
    std::sort(level.fixtures.begin(), level.fixtures.end(), [](const Fixture& lhs, const Fixture& rhs) {
        return detail::spatial_key(lhs.coordinate) < detail::spatial_key(rhs.coordinate);
    });
    std::sort(level.entities.begin(), level.entities.end(), [](const Entity& lhs, const Entity& rhs) {
        return std::tuple{lhs.coordinate.y, lhs.coordinate.x, lhs.bottom.half_steps, lhs.id}
            < std::tuple{rhs.coordinate.y, rhs.coordinate.x, rhs.bottom.half_steps, rhs.id};
    });
    return level;
}

} // namespace game_rules
