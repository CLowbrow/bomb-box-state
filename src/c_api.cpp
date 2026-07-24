#include "game_rules/c_api.h"

#include "game_rules/engine.hpp"
#include "game_rules/level_json.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <new>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <variant>

struct game_rules_engine final {
    game_rules::Engine value{};
};

namespace {

using namespace game_rules;

template <typename Integer>
void append_integer(std::string& output, const Integer value)
{
    std::array<char, std::numeric_limits<Integer>::digits10 + 4> buffer{};
    const auto result = std::to_chars(buffer.data(), buffer.data() + buffer.size(), value);
    output.append(buffer.data(), result.ptr);
}

void append_string(std::string& output, const std::string_view value)
{
    constexpr char hex[] = "0123456789abcdef";
    output.push_back('"');
    for (const char raw_character : value) {
        const auto character = static_cast<unsigned char>(raw_character);
        switch (character) {
        case '"': output += "\\\""; break;
        case '\\': output += "\\\\"; break;
        case '\b': output += "\\b"; break;
        case '\f': output += "\\f"; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default:
            if (character < 0x20U) {
                output += "\\u00";
                output.push_back(hex[(character >> 4U) & 0x0fU]);
                output.push_back(hex[character & 0x0fU]);
            } else {
                output.push_back(static_cast<char>(character));
            }
            break;
        }
    }
    output.push_back('"');
}

[[nodiscard]] std::string_view direction_name(const Direction direction) noexcept
{
    switch (direction) {
    case Direction::north: return "north";
    case Direction::east: return "east";
    case Direction::south: return "south";
    case Direction::west: return "west";
    }
    return {};
}

[[nodiscard]] std::string_view outcome_name(const Outcome outcome) noexcept
{
    switch (outcome) {
    case Outcome::ongoing: return "ongoing";
    case Outcome::won: return "won";
    case Outcome::lost: return "lost";
    }
    return "unknown";
}

[[nodiscard]] std::string_view entity_name(const EntityKind kind) noexcept
{
    switch (kind) {
    case EntityKind::player: return "player";
    case EntityKind::box: return "box";
    case EntityKind::barrel: return "barrel";
    }
    return "unknown";
}

[[nodiscard]] std::string_view color_name(const SwitchColor color) noexcept
{
    switch (color) {
    case SwitchColor::red: return "red";
    case SwitchColor::green: return "green";
    case SwitchColor::blue: return "blue";
    case SwitchColor::yellow: return "yellow";
    }
    return "unknown";
}

void append_coordinate(std::string& output, const Coordinate coordinate)
{
    output += "{\"x\":";
    append_integer(output, coordinate.x);
    output += ",\"y\":";
    append_integer(output, coordinate.y);
    output.push_back('}');
}

void append_entity(std::string& output, const Entity& entity)
{
    output += "{\"id\":\"";
    append_integer(output, entity.id);
    output += "\",\"type\":\"";
    output += entity_name(entity.kind);
    output += "\",\"coordinate\":";
    append_coordinate(output, entity.coordinate);
    output += ",\"bottomHalfSteps\":";
    append_integer(output, entity.bottom.half_steps);
    output.push_back('}');
}

void append_entities(std::string& output, const std::vector<Entity>& entities)
{
    output.push_back('[');
    for (std::size_t index = 0; index < entities.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        append_entity(output, entities[index]);
    }
    output.push_back(']');
}

void append_entity_ids(std::string& output, const std::vector<EntityId>& ids)
{
    output.push_back('[');
    for (std::size_t index = 0; index < ids.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        output.push_back('"');
        append_integer(output, ids[index]);
        output.push_back('"');
    }
    output.push_back(']');
}

void append_colors(std::string& output, const std::vector<SwitchColor>& colors)
{
    output.push_back('[');
    for (std::size_t index = 0; index < colors.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        append_string(output, color_name(colors[index]));
    }
    output.push_back(']');
}

void append_coordinates(std::string& output, const std::vector<Coordinate>& coordinates)
{
    output.push_back('[');
    for (std::size_t index = 0; index < coordinates.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        append_coordinate(output, coordinates[index]);
    }
    output.push_back(']');
}

void append_resolved_state(std::string& output, const ResolvedState& state)
{
    output += "{\"entities\":";
    append_entities(output, state.entities);
    output += ",\"armedBarrelIds\":";
    append_entity_ids(output, state.armed_barrels);
    output += ",\"activeSwitchColors\":";
    append_colors(output, state.active_switch_colors);
    output += ",\"openDoorCoordinates\":";
    append_coordinates(output, state.open_doors);
    output += ",\"outcome\":\"";
    output += outcome_name(state.outcome);
    output += "\"}";
}

void append_cell(std::string& output, const Cell& cell)
{
    output += "{\"coordinate\":";
    append_coordinate(output, cell.coordinate);
    if (const auto* flat = std::get_if<FlatCell>(&cell.geometry)) {
        output += ",\"type\":\"flat\",\"elevation\":";
        append_integer(output, flat->elevation);
    } else {
        const RampCell& ramp = std::get<RampCell>(cell.geometry);
        output += ",\"type\":\"ramp\",\"lowDirection\":\"";
        output += direction_name(ramp.low_direction);
        output += "\",\"lowElevation\":";
        append_integer(output, ramp.low_elevation);
    }
    output.push_back('}');
}

void append_fixture(std::string& output, const Fixture& fixture)
{
    output += "{\"coordinate\":";
    append_coordinate(output, fixture.coordinate);
    if (const auto* switch_fixture = std::get_if<Switch>(&fixture.kind)) {
        output += ",\"type\":\"switch\",\"color\":\"";
        output += color_name(switch_fixture->color);
        output += "\"}";
    } else if (const auto* door = std::get_if<Door>(&fixture.kind)) {
        output += ",\"type\":\"door\",\"color\":\"";
        output += color_name(door->color);
        output += "\"}";
    } else {
        output += ",\"type\":\"exit\"}";
    }
}

void append_renderable_state(std::string& output,
                             const LevelDefinition& level,
                             const ResolvedState& state)
{
    output += "{\"coordinateSystem\":{\"origin\":";
    append_coordinate(output, level.coordinates.origin);
    output += ",\"positiveX\":\"";
    output += level.coordinates.positive_x == HorizontalAxisDirection::east ? "east" : "west";
    output += "\",\"positiveY\":\"";
    output += level.coordinates.positive_y == VerticalAxisDirection::north ? "north" : "south";
    output += "\"},\"width\":";
    append_integer(output, level.width);
    output += ",\"height\":";
    append_integer(output, level.height);
    output += ",\"cells\":[";
    for (std::size_t index = 0; index < level.cells.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        append_cell(output, level.cells[index]);
    }
    output += "],\"fixtures\":[";
    for (std::size_t index = 0; index < level.fixtures.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        append_fixture(output, level.fixtures[index]);
    }
    output += "],\"entities\":";
    append_entities(output, state.entities);
    output += ",\"armedBarrelIds\":";
    append_entity_ids(output, state.armed_barrels);
    output += ",\"activeSwitchColors\":";
    append_colors(output, state.active_switch_colors);
    output += ",\"openDoorCoordinates\":";
    append_coordinates(output, state.open_doors);
    output += ",\"outcome\":\"";
    output += outcome_name(state.outcome);
    output += "\"}";
}

void append_current_state(std::string& output, const game_rules_engine* const engine)
{
    if (engine == nullptr) {
        output += "null";
        return;
    }
    const std::optional<LevelDefinition> level = engine->value.loaded_level();
    const std::optional<ResolvedState> state = engine->value.resolved_state();
    if (!level.has_value() || !state.has_value()) {
        output += "null";
        return;
    }
    append_renderable_state(output, *level, *state);
}

void append_event(std::string& output, const GameplayEvent& event)
{
    std::visit(
        [&output](const auto& value) {
            using Event = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<Event, MoveBlockedEvent>) {
                output += "{\"type\":\"moveBlocked\",\"direction\":\"";
                output += direction_name(value.direction);
                output += "\",\"reason\":\"";
                output += to_string(value.reason);
                output += "\"}";
            } else if constexpr (std::is_same_v<Event, StateRewoundEvent>) {
                output += "{\"type\":\"stateRewound\"}";
            } else if constexpr (std::is_same_v<Event, EntityMovedEvent>) {
                output += "{\"type\":\"entityMoved\",\"entityId\":\"";
                append_integer(output, value.entity_id);
                output += "\",\"from\":";
                append_coordinate(output, value.from);
                output += ",\"to\":";
                append_coordinate(output, value.to);
                output += ",\"oldBottomHalfSteps\":";
                append_integer(output, value.old_bottom.half_steps);
                output += ",\"newBottomHalfSteps\":";
                append_integer(output, value.new_bottom.half_steps);
                output += ",\"cause\":\"";
                output += to_string(value.cause);
                output += "\"}";
            } else if constexpr (std::is_same_v<Event, BarrelArmedEvent>) {
                output += "{\"type\":\"barrelArmed\",\"entityId\":\"";
                append_integer(output, value.entity_id);
                output += "\"}";
            } else if constexpr (std::is_same_v<Event, BarrelExplodedEvent>) {
                output += "{\"type\":\"barrelExploded\",\"entityId\":\"";
                append_integer(output, value.entity_id);
                output += "\",\"coordinate\":";
                append_coordinate(output, value.coordinate);
                output += ",\"bottomHalfSteps\":";
                append_integer(output, value.bottom.half_steps);
                output.push_back('}');
            } else if constexpr (std::is_same_v<Event, PlayerCrushedEvent>) {
                output += "{\"type\":\"playerCrushed\",\"playerId\":\"";
                append_integer(output, value.player_id);
                output += "\",\"crushingEntityId\":\"";
                append_integer(output, value.crushing_entity_id);
                output += "\"}";
            } else if constexpr (std::is_same_v<Event, SwitchChangedEvent>) {
                output += "{\"type\":\"switchChanged\",\"color\":\"";
                output += color_name(value.color);
                output += "\",\"active\":";
                output += value.active ? "true" : "false";
                output.push_back('}');
            } else if constexpr (std::is_same_v<Event, DoorOpenedEvent>) {
                output += "{\"type\":\"doorOpened\",\"coordinate\":";
                append_coordinate(output, value.coordinate);
                output += ",\"color\":\"";
                output += color_name(value.color);
                output += "\"}";
            } else if constexpr (std::is_same_v<Event, DoorClosedEvent>) {
                output += "{\"type\":\"doorClosed\",\"coordinate\":";
                append_coordinate(output, value.coordinate);
                output += ",\"color\":\"";
                output += color_name(value.color);
                output += "\"}";
            } else if constexpr (std::is_same_v<Event, LevelWonEvent>) {
                output += "{\"type\":\"levelWon\"}";
            } else if constexpr (std::is_same_v<Event, LevelLostEvent>) {
                output += "{\"type\":\"levelLost\"}";
            }
        },
        event);
}

void append_events(std::string& output, const std::vector<GameplayEvent>& events)
{
    output.push_back('[');
    for (std::size_t index = 0; index < events.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        append_event(output, events[index]);
    }
    output.push_back(']');
}

void append_validation_errors(std::string& output,
                              const std::vector<ValidationError>& errors)
{
    output.push_back('[');
    for (std::size_t index = 0; index < errors.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        const ValidationError& error = errors[index];
        output += "{\"code\":\"";
        output += to_string(error.code);
        output += "\",\"coordinate\":";
        append_coordinate(output, error.coordinate);
        output += ",\"entityId\":\"";
        append_integer(output, error.entity_id);
        output += "\"}";
    }
    output.push_back(']');
}

[[nodiscard]] char* copy_result(const std::string& value) noexcept
{
    void* const storage = std::malloc(value.size() + 1U);
    if (storage == nullptr) {
        return nullptr;
    }
    std::memcpy(storage, value.data(), value.size());
    static_cast<char*>(storage)[value.size()] = '\0';
    return static_cast<char*>(storage);
}

[[nodiscard]] char* simple_error(const std::string_view operation,
                                 const std::string_view status,
                                 const game_rules_engine* const engine)
{
    std::string output{"{\"apiVersion\":1,\"operation\":"};
    append_string(output, operation);
    output += ",\"status\":";
    append_string(output, status);
    output += ",\"state\":";
    append_current_state(output, engine);
    output.push_back('}');
    return copy_result(output);
}

[[nodiscard]] std::optional<Direction> decode_direction(const uint32_t direction) noexcept
{
    switch (direction) {
    case GAME_RULES_DIRECTION_NORTH: return Direction::north;
    case GAME_RULES_DIRECTION_EAST: return Direction::east;
    case GAME_RULES_DIRECTION_SOUTH: return Direction::south;
    case GAME_RULES_DIRECTION_WEST: return Direction::west;
    default: return std::nullopt;
    }
}

} // namespace

uint32_t game_rules_api_version(void)
{
    return game_rules::api_version;
}

const char* game_rules_engine_status(void)
{
    return "schema_ready";
}

game_rules_engine* game_rules_engine_create(void)
{
    return new (std::nothrow) game_rules_engine{};
}

void game_rules_engine_destroy(game_rules_engine* const engine)
{
    delete engine;
}

char* game_rules_engine_load_level(game_rules_engine* const engine,
                                   const char* const level_json,
                                   const uint32_t level_json_length)
{
    if (engine == nullptr) {
        return simple_error("loadLevel", "invalid_engine", nullptr);
    }
    if (level_json == nullptr) {
        return simple_error("loadLevel", "invalid_argument", engine);
    }

    const DecodeLevelJsonResult decoded =
        decode_level_json(std::string_view{level_json, level_json_length});
    if (!decoded.accepted()) {
        std::string output{"{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\""};
        if (decoded.json_error.has_value()) {
            output += "invalid_json\",\"error\":{\"code\":\"";
            output += to_string(decoded.json_error->code);
            output += "\",\"byteOffset\":";
            append_integer(output, decoded.json_error->byte_offset);
            output += ",\"path\":";
            append_string(output, decoded.json_error->path);
            output.push_back('}');
        } else {
            output += "invalid_level\",\"errors\":";
            append_validation_errors(output, decoded.validation_errors);
        }
        output += ",\"state\":";
        append_current_state(output, engine);
        output.push_back('}');
        return copy_result(output);
    }

    const LoadResult loaded = engine->value.load_level(*decoded.level);
    std::string output{"{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\""};
    output += to_string(loaded.status);
    if (!loaded.accepted()) {
        output += "\",\"errors\":";
        append_validation_errors(output, loaded.errors);
    } else {
        output += "\",\"initialState\":";
        append_resolved_state(output, *loaded.initial_state);
        output += ",\"ticks\":[";
        for (std::size_t index = 0; index < loaded.ticks.size(); ++index) {
            if (index != 0) {
                output.push_back(',');
            }
            const TickResult& tick = loaded.ticks[index];
            output += "{\"index\":";
            append_integer(output, tick.index);
            output += ",\"events\":";
            append_events(output, tick.events);
            output += ",\"stateAfter\":";
            append_resolved_state(output, tick.state_after);
            output.push_back('}');
        }
        output.push_back(']');
    }
    output += ",\"state\":";
    append_current_state(output, engine);
    if (loaded.accepted()) {
        output += ",\"outcome\":\"";
        output += outcome_name(*loaded.outcome);
        output.push_back('"');
    }
    output.push_back('}');
    return copy_result(output);
}

char* game_rules_engine_get_state(game_rules_engine* const engine)
{
    if (engine == nullptr) {
        return simple_error("getState", "invalid_engine", nullptr);
    }
    std::string output{"{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\""};
    output += engine->value.has_level() ? "ok" : "no_level";
    output += "\",\"state\":";
    append_current_state(output, engine);
    output.push_back('}');
    return copy_result(output);
}

char* game_rules_engine_move(game_rules_engine* const engine, const uint32_t direction)
{
    if (engine == nullptr) {
        return simple_error("move", "invalid_engine", nullptr);
    }
    const std::optional<Direction> decoded_direction = decode_direction(direction);
    const MoveResult moved = engine->value.move(
        decoded_direction.value_or(static_cast<Direction>(std::numeric_limits<std::uint8_t>::max())));

    std::string output{"{\"apiVersion\":1,\"operation\":\"move\",\"status\":\""};
    output += to_string(moved.status);
    output += "\",\"accepted\":";
    output += moved.accepted() ? "true" : "false";
    output += ",\"direction\":";
    if (decoded_direction.has_value()) {
        append_string(output, direction_name(*decoded_direction));
    } else {
        output += "null";
    }
    output += ",\"events\":";
    append_events(output, moved.events);
    output += ",\"initialState\":";
    if (moved.initial_state.has_value()) {
        append_resolved_state(output, *moved.initial_state);
    } else {
        output += "null";
    }
    output += ",\"ticks\":[";
    for (std::size_t index = 0; index < moved.ticks.size(); ++index) {
        if (index != 0) {
            output.push_back(',');
        }
        const TickResult& tick = moved.ticks[index];
        output += "{\"index\":";
        append_integer(output, tick.index);
        output += ",\"events\":";
        append_events(output, tick.events);
        output += ",\"stateAfter\":";
        append_resolved_state(output, tick.state_after);
        output.push_back('}');
    }
    output += "],\"state\":";
    append_current_state(output, engine);
    output += ",\"outcome\":";
    if (moved.outcome.has_value()) {
        append_string(output, outcome_name(*moved.outcome));
    } else {
        output += "null";
    }
    output.push_back('}');
    return copy_result(output);
}

char* game_rules_engine_rewind(game_rules_engine* const engine)
{
    if (engine == nullptr) {
        return simple_error("rewind", "invalid_engine", nullptr);
    }
    const RewindResult rewound = engine->value.rewind();
    std::string output{"{\"apiVersion\":1,\"operation\":\"rewind\",\"status\":\""};
    output += to_string(rewound.status);
    output += "\",\"accepted\":";
    output += rewound.accepted() ? "true" : "false";
    output += ",\"events\":";
    append_events(output, rewound.events);
    output += ",\"state\":";
    append_current_state(output, engine);
    output += ",\"outcome\":";
    if (rewound.outcome.has_value()) {
        append_string(output, outcome_name(*rewound.outcome));
    } else {
        output += "null";
    }
    output.push_back('}');
    return copy_result(output);
}

void game_rules_string_free(char* const result)
{
    std::free(result);
}
