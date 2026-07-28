#include "game_rules/c_api.h"

#include "c_api_internal.hpp"

#include <cstdint>
#include <limits>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace {

using namespace game_rules;

[[nodiscard]] game_rules_coordinate copy_coordinate(const Coordinate value) noexcept
{
    return {value.x, value.y};
}

[[nodiscard]] Coordinate copy_coordinate(const game_rules_coordinate value) noexcept
{
    return {value.x, value.y};
}

template <typename Value>
[[nodiscard]] const Value* data_or_null(const std::vector<Value>& values) noexcept
{
    return values.empty() ? nullptr : values.data();
}

[[nodiscard]] std::uint32_t count_of(const std::size_t size) noexcept
{
    return static_cast<std::uint32_t>(size);
}

[[nodiscard]] std::uint32_t copy_outcome(const Outcome value) noexcept
{
    switch (value) {
    case Outcome::ongoing: return GAME_RULES_OUTCOME_ONGOING;
    case Outcome::won: return GAME_RULES_OUTCOME_WON;
    case Outcome::lost: return GAME_RULES_OUTCOME_LOST;
    }
    return GAME_RULES_OUTCOME_ONGOING;
}

[[nodiscard]] std::uint32_t copy_direction(const Direction value) noexcept
{
    switch (value) {
    case Direction::north: return GAME_RULES_DIRECTION_NORTH;
    case Direction::east: return GAME_RULES_DIRECTION_EAST;
    case Direction::south: return GAME_RULES_DIRECTION_SOUTH;
    case Direction::west: return GAME_RULES_DIRECTION_WEST;
    }
    return GAME_RULES_DIRECTION_NORTH;
}

[[nodiscard]] std::optional<Direction> read_direction(const std::uint32_t value) noexcept
{
    switch (value) {
    case GAME_RULES_DIRECTION_NORTH: return Direction::north;
    case GAME_RULES_DIRECTION_EAST: return Direction::east;
    case GAME_RULES_DIRECTION_SOUTH: return Direction::south;
    case GAME_RULES_DIRECTION_WEST: return Direction::west;
    default: return std::nullopt;
    }
}

[[nodiscard]] std::uint32_t copy_color(const SwitchColor value) noexcept
{
    switch (value) {
    case SwitchColor::red: return GAME_RULES_COLOR_RED;
    case SwitchColor::green: return GAME_RULES_COLOR_GREEN;
    case SwitchColor::blue: return GAME_RULES_COLOR_BLUE;
    case SwitchColor::yellow: return GAME_RULES_COLOR_YELLOW;
    }
    return GAME_RULES_COLOR_RED;
}

[[nodiscard]] std::optional<SwitchColor> read_color(const std::uint32_t value) noexcept
{
    switch (value) {
    case GAME_RULES_COLOR_RED: return SwitchColor::red;
    case GAME_RULES_COLOR_GREEN: return SwitchColor::green;
    case GAME_RULES_COLOR_BLUE: return SwitchColor::blue;
    case GAME_RULES_COLOR_YELLOW: return SwitchColor::yellow;
    default: return std::nullopt;
    }
}

[[nodiscard]] std::uint32_t copy_entity_kind(const EntityKind value) noexcept
{
    switch (value) {
    case EntityKind::player: return GAME_RULES_ENTITY_PLAYER;
    case EntityKind::box: return GAME_RULES_ENTITY_BOX;
    case EntityKind::barrel: return GAME_RULES_ENTITY_BARREL;
    }
    return GAME_RULES_ENTITY_BOX;
}

[[nodiscard]] std::optional<EntityKind> read_entity_kind(const std::uint32_t value) noexcept
{
    switch (value) {
    case GAME_RULES_ENTITY_PLAYER: return EntityKind::player;
    case GAME_RULES_ENTITY_BOX: return EntityKind::box;
    case GAME_RULES_ENTITY_BARREL: return EntityKind::barrel;
    default: return std::nullopt;
    }
}

[[nodiscard]] std::uint32_t copy_move_status(const MoveStatus value) noexcept
{
    switch (value) {
    case MoveStatus::moved: return GAME_RULES_MOVE_MOVED;
    case MoveStatus::no_level: return GAME_RULES_MOVE_NO_LEVEL;
    case MoveStatus::invalid_direction: return GAME_RULES_MOVE_INVALID_DIRECTION;
    case MoveStatus::world_boundary: return GAME_RULES_MOVE_WORLD_BOUNDARY;
    case MoveStatus::ledge: return GAME_RULES_MOVE_LEDGE;
    case MoveStatus::occupied: return GAME_RULES_MOVE_OCCUPIED;
    case MoveStatus::stacked_push_target: return GAME_RULES_MOVE_STACKED_PUSH_TARGET;
    case MoveStatus::closed_door: return GAME_RULES_MOVE_CLOSED_DOOR;
    case MoveStatus::teleporter_restriction: return GAME_RULES_MOVE_TELEPORTER_RESTRICTION;
    case MoveStatus::unsupported_geometry: return GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY;
    case MoveStatus::level_terminal: return GAME_RULES_MOVE_LEVEL_TERMINAL;
    }
    return GAME_RULES_MOVE_NO_LEVEL;
}

[[nodiscard]] std::uint32_t copy_movement_cause(const MovementCause value) noexcept
{
    switch (value) {
    case MovementCause::player: return GAME_RULES_MOVEMENT_PLAYER;
    case MovementCause::blast: return GAME_RULES_MOVEMENT_BLAST;
    case MovementCause::fall: return GAME_RULES_MOVEMENT_FALL;
    case MovementCause::slide: return GAME_RULES_MOVEMENT_SLIDE;
    }
    return GAME_RULES_MOVEMENT_PLAYER;
}

[[nodiscard]] std::uint32_t copy_validation_code(const ValidationErrorCode value) noexcept
{
    switch (value) {
    case ValidationErrorCode::invalid_dimensions: return GAME_RULES_VALIDATION_INVALID_DIMENSIONS;
    case ValidationErrorCode::invalid_coordinate_system:
        return GAME_RULES_VALIDATION_INVALID_COORDINATE_SYSTEM;
    case ValidationErrorCode::cell_count_mismatch:
        return GAME_RULES_VALIDATION_CELL_COUNT_MISMATCH;
    case ValidationErrorCode::cell_out_of_bounds:
        return GAME_RULES_VALIDATION_CELL_OUT_OF_BOUNDS;
    case ValidationErrorCode::duplicate_cell: return GAME_RULES_VALIDATION_DUPLICATE_CELL;
    case ValidationErrorCode::invalid_cell_height:
        return GAME_RULES_VALIDATION_INVALID_CELL_HEIGHT;
    case ValidationErrorCode::invalid_ramp_direction:
        return GAME_RULES_VALIDATION_INVALID_RAMP_DIRECTION;
    case ValidationErrorCode::invalid_ramp_endpoints:
        return GAME_RULES_VALIDATION_INVALID_RAMP_ENDPOINTS;
    case ValidationErrorCode::fixture_out_of_bounds:
        return GAME_RULES_VALIDATION_FIXTURE_OUT_OF_BOUNDS;
    case ValidationErrorCode::fixture_on_ramp:
        return GAME_RULES_VALIDATION_FIXTURE_ON_RAMP;
    case ValidationErrorCode::duplicate_fixture:
        return GAME_RULES_VALIDATION_DUPLICATE_FIXTURE;
    case ValidationErrorCode::invalid_fixture_color:
        return GAME_RULES_VALIDATION_INVALID_FIXTURE_COLOR;
    case ValidationErrorCode::entity_out_of_bounds:
        return GAME_RULES_VALIDATION_ENTITY_OUT_OF_BOUNDS;
    case ValidationErrorCode::duplicate_entity_id:
        return GAME_RULES_VALIDATION_DUPLICATE_ENTITY_ID;
    case ValidationErrorCode::invalid_entity_kind:
        return GAME_RULES_VALIDATION_INVALID_ENTITY_KIND;
    case ValidationErrorCode::entity_below_surface:
        return GAME_RULES_VALIDATION_ENTITY_BELOW_SURFACE;
    case ValidationErrorCode::overlapping_entities:
        return GAME_RULES_VALIDATION_OVERLAPPING_ENTITIES;
    case ValidationErrorCode::player_not_top_of_stack:
        return GAME_RULES_VALIDATION_PLAYER_NOT_TOP_OF_STACK;
    case ValidationErrorCode::player_count_not_one:
        return GAME_RULES_VALIDATION_PLAYER_COUNT_NOT_ONE;
    case ValidationErrorCode::invalid_teleporter_occupancy:
        return GAME_RULES_VALIDATION_INVALID_TELEPORTER_OCCUPANCY;
    case ValidationErrorCode::invalid_entity_id:
        return GAME_RULES_VALIDATION_INVALID_ENTITY_ID;
    }
    return GAME_RULES_VALIDATION_INVALID_DIMENSIONS;
}

[[nodiscard]] game_rules_entity copy_entity(const Entity& value) noexcept
{
    return {value.id,
            copy_entity_kind(value.kind),
            copy_coordinate(value.coordinate),
            value.bottom.half_steps};
}

[[nodiscard]] game_rules_event copy_event(const GameplayEvent& event) noexcept
{
    game_rules_event output{};
    std::visit(
        [&output](const auto& value) {
            using Event = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Event, MoveBlockedEvent>) {
                output.kind = GAME_RULES_EVENT_MOVE_BLOCKED;
                output.direction = copy_direction(value.direction);
                output.move_status = copy_move_status(value.reason);
            } else if constexpr (std::is_same_v<Event, StateRewoundEvent>) {
                output.kind = GAME_RULES_EVENT_STATE_REWOUND;
            } else if constexpr (std::is_same_v<Event, EntityMovedEvent>) {
                output.kind = GAME_RULES_EVENT_ENTITY_MOVED;
                output.entity_id = value.entity_id;
                output.from = copy_coordinate(value.from);
                output.to = copy_coordinate(value.to);
                output.old_bottom_half_steps = value.old_bottom.half_steps;
                output.new_bottom_half_steps = value.new_bottom.half_steps;
                output.movement_cause = copy_movement_cause(value.cause);
            } else if constexpr (std::is_same_v<Event, BarrelArmedEvent>) {
                output.kind = GAME_RULES_EVENT_BARREL_ARMED;
                output.entity_id = value.entity_id;
            } else if constexpr (std::is_same_v<Event, BarrelExplodedEvent>) {
                output.kind = GAME_RULES_EVENT_BARREL_EXPLODED;
                output.entity_id = value.entity_id;
                output.coordinate = copy_coordinate(value.coordinate);
                output.bottom_half_steps = value.bottom.half_steps;
            } else if constexpr (std::is_same_v<Event, PlayerCrushedEvent>) {
                output.kind = GAME_RULES_EVENT_PLAYER_CRUSHED;
                output.entity_id = value.player_id;
                output.other_entity_id = value.crushing_entity_id;
            } else if constexpr (std::is_same_v<Event, SwitchChangedEvent>) {
                output.kind = GAME_RULES_EVENT_SWITCH_CHANGED;
                output.color = copy_color(value.color);
                output.active = value.active ? 1U : 0U;
            } else if constexpr (std::is_same_v<Event, DoorOpenedEvent>) {
                output.kind = GAME_RULES_EVENT_DOOR_OPENED;
                output.coordinate = copy_coordinate(value.coordinate);
                output.color = copy_color(value.color);
            } else if constexpr (std::is_same_v<Event, DoorClosedEvent>) {
                output.kind = GAME_RULES_EVENT_DOOR_CLOSED;
                output.coordinate = copy_coordinate(value.coordinate);
                output.color = copy_color(value.color);
            } else if constexpr (std::is_same_v<Event, LevelWonEvent>) {
                output.kind = GAME_RULES_EVENT_LEVEL_WON;
            } else if constexpr (std::is_same_v<Event, LevelLostEvent>) {
                output.kind = GAME_RULES_EVENT_LEVEL_LOST;
            }
        },
        event);
    return output;
}

struct StateStorage final {
    std::vector<game_rules_entity> entities{};
    std::vector<std::uint64_t> armed_barrels{};
    std::vector<std::uint32_t> active_switch_colors{};
    std::vector<game_rules_coordinate> open_doors{};
    std::uint32_t outcome{};

    void assign(const ResolvedState& state)
    {
        entities.reserve(state.entities.size());
        for (const Entity& entity : state.entities) {
            entities.push_back(copy_entity(entity));
        }
        armed_barrels = state.armed_barrels;
        active_switch_colors.reserve(state.active_switch_colors.size());
        for (const SwitchColor color : state.active_switch_colors) {
            active_switch_colors.push_back(copy_color(color));
        }
        open_doors.reserve(state.open_doors.size());
        for (const Coordinate coordinate : state.open_doors) {
            open_doors.push_back(copy_coordinate(coordinate));
        }
        outcome = copy_outcome(state.outcome);
    }

    [[nodiscard]] game_rules_resolved_state view() const noexcept
    {
        return {data_or_null(entities),
                count_of(entities.size()),
                data_or_null(armed_barrels),
                count_of(armed_barrels.size()),
                data_or_null(active_switch_colors),
                count_of(active_switch_colors.size()),
                data_or_null(open_doors),
                count_of(open_doors.size()),
                outcome};
    }
};

struct LevelStorage final {
    game_rules_coordinate_system coordinates{};
    std::uint32_t width{};
    std::uint32_t height{};
    std::vector<game_rules_cell> cells{};
    std::vector<game_rules_fixture> fixtures{};

    void assign(const LevelDefinition& level)
    {
        coordinates.origin = copy_coordinate(level.coordinates.origin);
        coordinates.positive_x = level.coordinates.positive_x == HorizontalAxisDirection::east
                                     ? GAME_RULES_HORIZONTAL_EAST
                                     : GAME_RULES_HORIZONTAL_WEST;
        coordinates.positive_y = level.coordinates.positive_y == VerticalAxisDirection::north
                                     ? GAME_RULES_VERTICAL_NORTH
                                     : GAME_RULES_VERTICAL_SOUTH;
        width = level.width;
        height = level.height;
        cells.reserve(level.cells.size());
        for (const Cell& cell : level.cells) {
            game_rules_cell copied{};
            copied.coordinate = copy_coordinate(cell.coordinate);
            if (const auto* flat = std::get_if<FlatCell>(&cell.geometry)) {
                copied.kind = GAME_RULES_CELL_FLAT;
                copied.elevation = flat->elevation;
            } else {
                const RampCell& ramp = std::get<RampCell>(cell.geometry);
                copied.kind = GAME_RULES_CELL_RAMP;
                copied.elevation = ramp.low_elevation;
                copied.low_direction = copy_direction(ramp.low_direction);
            }
            cells.push_back(copied);
        }
        fixtures.reserve(level.fixtures.size());
        for (const Fixture& fixture : level.fixtures) {
            game_rules_fixture copied{};
            copied.coordinate = copy_coordinate(fixture.coordinate);
            if (const auto* switch_fixture = std::get_if<Switch>(&fixture.kind)) {
                copied.kind = GAME_RULES_FIXTURE_SWITCH;
                copied.color = copy_color(switch_fixture->color);
            } else if (const auto* door = std::get_if<Door>(&fixture.kind)) {
                copied.kind = GAME_RULES_FIXTURE_DOOR;
                copied.color = copy_color(door->color);
            } else {
                copied.kind = GAME_RULES_FIXTURE_EXIT;
            }
            fixtures.push_back(copied);
        }
    }

    [[nodiscard]] game_rules_level view() const noexcept
    {
        return {coordinates,
                width,
                height,
                data_or_null(cells),
                count_of(cells.size()),
                data_or_null(fixtures),
                count_of(fixtures.size())};
    }
};

struct SnapshotStorage final {
    LevelStorage level{};
    StateStorage resolved{};

    void assign(const LevelDefinition& source_level, const ResolvedState& source_state)
    {
        level.assign(source_level);
        resolved.assign(source_state);
    }

    [[nodiscard]] game_rules_snapshot view() const noexcept
    {
        return {level.view(), resolved.view()};
    }
};

struct TickStorage final {
    std::uint32_t index{};
    std::vector<game_rules_event> events{};
    StateStorage state_after{};

    void assign(const TickResult& tick)
    {
        index = tick.index;
        events.reserve(tick.events.size());
        for (const GameplayEvent& event : tick.events) {
            events.push_back(copy_event(event));
        }
        state_after.assign(tick.state_after);
    }

    [[nodiscard]] game_rules_tick view() const noexcept
    {
        return {index, data_or_null(events), count_of(events.size()), state_after.view()};
    }
};

struct TicksStorage final {
    std::vector<TickStorage> storage{};
    std::vector<game_rules_tick> views{};

    void assign(const std::vector<TickResult>& ticks)
    {
        storage.reserve(ticks.size());
        for (const TickResult& tick : ticks) {
            storage.emplace_back();
            storage.back().assign(tick);
        }
        views.reserve(storage.size());
        for (const TickStorage& tick : storage) {
            views.push_back(tick.view());
        }
    }
};

struct StateResultStorage final {
    SnapshotStorage state{};
};

struct LoadResultStorage final {
    std::vector<game_rules_validation_error> errors{};
    StateStorage initial_state{};
    TicksStorage ticks{};
    StateStorage final_state{};
    SnapshotStorage state{};
};

struct MoveResultStorage final {
    std::vector<game_rules_event> events{};
    StateStorage initial_state{};
    TicksStorage ticks{};
    StateStorage final_state{};
    SnapshotStorage state{};
};

struct RewindResultStorage final {
    std::vector<game_rules_event> events{};
    StateStorage restored_state{};
    SnapshotStorage state{};
};

template <typename Result>
void clear_result(Result* const result) noexcept
{
    if (result != nullptr) {
        *result = {};
    }
}

template <typename Storage, typename Result>
void dispose_result(Result* const result) noexcept
{
    if (result == nullptr) {
        return;
    }
    delete static_cast<Storage*>(result->owned_storage);
    *result = {};
}

[[nodiscard]] bool pointers_are_valid(const game_rules_level_definition& level) noexcept
{
    return (level.cell_count == 0U || level.cells != nullptr) &&
           (level.fixture_count == 0U || level.fixtures != nullptr) &&
           (level.entity_count == 0U || level.entities != nullptr);
}

[[nodiscard]] bool read_level(const game_rules_level_definition& input,
                              LevelDefinition& output)
{
    if (!pointers_are_valid(input)) {
        return false;
    }
    switch (input.coordinates.positive_x) {
    case GAME_RULES_HORIZONTAL_EAST:
        output.coordinates.positive_x = HorizontalAxisDirection::east;
        break;
    case GAME_RULES_HORIZONTAL_WEST:
        output.coordinates.positive_x = HorizontalAxisDirection::west;
        break;
    default: return false;
    }
    switch (input.coordinates.positive_y) {
    case GAME_RULES_VERTICAL_NORTH:
        output.coordinates.positive_y = VerticalAxisDirection::north;
        break;
    case GAME_RULES_VERTICAL_SOUTH:
        output.coordinates.positive_y = VerticalAxisDirection::south;
        break;
    default: return false;
    }
    output.coordinates.origin = copy_coordinate(input.coordinates.origin);
    output.width = input.width;
    output.height = input.height;

    output.cells.reserve(input.cell_count);
    for (std::uint32_t index = 0; index < input.cell_count; ++index) {
        const game_rules_cell& cell = input.cells[index];
        Cell copied{};
        copied.coordinate = copy_coordinate(cell.coordinate);
        if (cell.kind == GAME_RULES_CELL_FLAT) {
            copied.geometry = FlatCell{cell.elevation};
        } else if (cell.kind == GAME_RULES_CELL_RAMP) {
            const std::optional<Direction> direction = read_direction(cell.low_direction);
            if (!direction.has_value()) {
                return false;
            }
            copied.geometry = RampCell{*direction, cell.elevation};
        } else {
            return false;
        }
        output.cells.push_back(copied);
    }

    output.fixtures.reserve(input.fixture_count);
    for (std::uint32_t index = 0; index < input.fixture_count; ++index) {
        const game_rules_fixture& fixture = input.fixtures[index];
        Fixture copied{};
        copied.coordinate = copy_coordinate(fixture.coordinate);
        if (fixture.kind == GAME_RULES_FIXTURE_EXIT) {
            copied.kind = ExitTeleporter{};
        } else {
            const std::optional<SwitchColor> color = read_color(fixture.color);
            if (!color.has_value()) {
                return false;
            }
            if (fixture.kind == GAME_RULES_FIXTURE_SWITCH) {
                copied.kind = Switch{*color};
            } else if (fixture.kind == GAME_RULES_FIXTURE_DOOR) {
                copied.kind = Door{*color};
            } else {
                return false;
            }
        }
        output.fixtures.push_back(copied);
    }

    output.entities.reserve(input.entity_count);
    for (std::uint32_t index = 0; index < input.entity_count; ++index) {
        const game_rules_entity& entity = input.entities[index];
        const std::optional<EntityKind> kind = read_entity_kind(entity.kind);
        if (!kind.has_value()) {
            return false;
        }
        output.entities.push_back(
            Entity{entity.id,
                   *kind,
                   copy_coordinate(entity.coordinate),
                   Height{entity.bottom_half_steps}});
    }
    return true;
}

[[nodiscard]] bool assign_current_snapshot(const game_rules_engine& engine,
                                           SnapshotStorage& storage)
{
    const std::optional<LevelDefinition> level = engine.value.loaded_level();
    const std::optional<ResolvedState> state = engine.value.resolved_state();
    if (!level.has_value() || !state.has_value()) {
        return false;
    }
    storage.assign(*level, *state);
    return true;
}

template <typename Storage>
[[nodiscard]] Storage* allocate_storage() noexcept
{
    return new (std::nothrow) Storage{};
}

} // namespace

std::uint32_t game_rules_data_api_version(void)
{
    return 1U;
}

std::uint32_t game_rules_engine_get_state_data(const game_rules_engine* const engine,
                                               game_rules_state_result* const out_result)
{
    if (out_result == nullptr) {
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    clear_result(out_result);
    if (engine == nullptr) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }
    StateResultStorage* const storage = allocate_storage<StateResultStorage>();
    if (storage == nullptr) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    out_result->has_state = assign_current_snapshot(*engine, storage->state) ? 1U : 0U;
    if (out_result->has_state != 0U) {
        out_result->state = storage->state.view();
    }
    out_result->owned_storage = storage;
    return GAME_RULES_CALL_OK;
}

std::uint32_t game_rules_engine_load_level_data(game_rules_engine* const engine,
                                                const game_rules_level_definition* const level,
                                                game_rules_load_result* const out_result)
{
    if (out_result == nullptr || level == nullptr) {
        clear_result(out_result);
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    clear_result(out_result);
    if (engine == nullptr) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }
    LevelDefinition decoded{};
    if (!read_level(*level, decoded)) {
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    LoadResultStorage* const storage = allocate_storage<LoadResultStorage>();
    if (storage == nullptr) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }

    const LoadResult loaded = engine->value.load_level(decoded);
    out_result->status = loaded.status == LoadStatus::loaded ? GAME_RULES_LOAD_LOADED
                                                            : GAME_RULES_LOAD_INVALID_LEVEL;
    out_result->accepted = loaded.accepted() ? 1U : 0U;
    storage->errors.reserve(loaded.errors.size());
    for (const ValidationError& error : loaded.errors) {
        storage->errors.push_back(
            {copy_validation_code(error.code), copy_coordinate(error.coordinate), error.entity_id});
    }
    out_result->errors = data_or_null(storage->errors);
    out_result->error_count = count_of(storage->errors.size());
    if (loaded.initial_state.has_value()) {
        storage->initial_state.assign(*loaded.initial_state);
        out_result->has_initial_state = 1U;
        out_result->initial_state = storage->initial_state.view();
    }
    storage->ticks.assign(loaded.ticks);
    out_result->ticks = data_or_null(storage->ticks.views);
    out_result->tick_count = count_of(storage->ticks.views.size());
    if (loaded.final_state.has_value()) {
        storage->final_state.assign(*loaded.final_state);
        out_result->has_final_state = 1U;
        out_result->final_state = storage->final_state.view();
    }
    out_result->has_state = assign_current_snapshot(*engine, storage->state) ? 1U : 0U;
    if (out_result->has_state != 0U) {
        out_result->state = storage->state.view();
    }
    if (loaded.outcome.has_value()) {
        out_result->has_outcome = 1U;
        out_result->outcome = copy_outcome(*loaded.outcome);
    }
    out_result->owned_storage = storage;
    return GAME_RULES_CALL_OK;
}

std::uint32_t game_rules_engine_move_data(game_rules_engine* const engine,
                                          const std::uint32_t direction,
                                          game_rules_move_result* const out_result)
{
    if (out_result == nullptr) {
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    clear_result(out_result);
    if (engine == nullptr) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }
    MoveResultStorage* const storage = allocate_storage<MoveResultStorage>();
    if (storage == nullptr) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }

    const std::optional<Direction> decoded = read_direction(direction);
    const MoveResult moved = engine->value.move(
        decoded.value_or(static_cast<Direction>(std::numeric_limits<std::uint8_t>::max())));
    out_result->status = copy_move_status(moved.status);
    out_result->accepted = moved.accepted() ? 1U : 0U;
    if (decoded.has_value()) {
        out_result->has_direction = 1U;
        out_result->direction = copy_direction(*decoded);
    }
    storage->events.reserve(moved.events.size());
    for (const GameplayEvent& event : moved.events) {
        storage->events.push_back(copy_event(event));
    }
    out_result->events = data_or_null(storage->events);
    out_result->event_count = count_of(storage->events.size());
    if (moved.initial_state.has_value()) {
        storage->initial_state.assign(*moved.initial_state);
        out_result->has_initial_state = 1U;
        out_result->initial_state = storage->initial_state.view();
    }
    storage->ticks.assign(moved.ticks);
    out_result->ticks = data_or_null(storage->ticks.views);
    out_result->tick_count = count_of(storage->ticks.views.size());
    if (moved.final_state.has_value()) {
        storage->final_state.assign(*moved.final_state);
        out_result->has_final_state = 1U;
        out_result->final_state = storage->final_state.view();
    }
    out_result->has_state = assign_current_snapshot(*engine, storage->state) ? 1U : 0U;
    if (out_result->has_state != 0U) {
        out_result->state = storage->state.view();
    }
    if (moved.outcome.has_value()) {
        out_result->has_outcome = 1U;
        out_result->outcome = copy_outcome(*moved.outcome);
    }
    out_result->owned_storage = storage;
    return GAME_RULES_CALL_OK;
}

std::uint32_t game_rules_engine_rewind_data(game_rules_engine* const engine,
                                            game_rules_rewind_result* const out_result)
{
    if (out_result == nullptr) {
        return GAME_RULES_CALL_INVALID_ARGUMENT;
    }
    clear_result(out_result);
    if (engine == nullptr) {
        return GAME_RULES_CALL_INVALID_ENGINE;
    }
    RewindResultStorage* const storage = allocate_storage<RewindResultStorage>();
    if (storage == nullptr) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }

    const RewindResult rewound = engine->value.rewind();
    out_result->status = rewound.status == RewindStatus::rewound ? GAME_RULES_REWIND_REWOUND
                                                                : GAME_RULES_REWIND_HISTORY_EMPTY;
    out_result->accepted = rewound.accepted() ? 1U : 0U;
    storage->events.reserve(rewound.events.size());
    for (const GameplayEvent& event : rewound.events) {
        storage->events.push_back(copy_event(event));
    }
    out_result->events = data_or_null(storage->events);
    out_result->event_count = count_of(storage->events.size());
    if (rewound.state.has_value()) {
        storage->restored_state.assign(*rewound.state);
        out_result->has_restored_state = 1U;
        out_result->restored_state = storage->restored_state.view();
    }
    out_result->has_state = assign_current_snapshot(*engine, storage->state) ? 1U : 0U;
    if (out_result->has_state != 0U) {
        out_result->state = storage->state.view();
    }
    if (rewound.outcome.has_value()) {
        out_result->has_outcome = 1U;
        out_result->outcome = copy_outcome(*rewound.outcome);
    }
    out_result->owned_storage = storage;
    return GAME_RULES_CALL_OK;
}

void game_rules_state_result_dispose(game_rules_state_result* const result)
{
    dispose_result<StateResultStorage>(result);
}

void game_rules_load_result_dispose(game_rules_load_result* const result)
{
    dispose_result<LoadResultStorage>(result);
}

void game_rules_move_result_dispose(game_rules_move_result* const result)
{
    dispose_result<MoveResultStorage>(result);
}

void game_rules_rewind_result_dispose(game_rules_rewind_result* const result)
{
    dispose_result<RewindResultStorage>(result);
}
