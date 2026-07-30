#include "c_api_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

typedef struct layout {
    unsigned char* base;
    size_t offset;
    size_t capacity;
    uint32_t failed;
} layout;

typedef struct builder {
    game_rules_c_allocator allocator;
    char* data;
    size_t length;
    size_t capacity;
    uint32_t failed;
} builder;

static int add_size(size_t left, size_t right, size_t* result)
{
    if (left > SIZE_MAX - right) return 0;
    *result = left + right;
    return 1;
}

static int multiply_size(size_t left, size_t right, size_t* result)
{
    if (right != 0U && left > SIZE_MAX / right) return 0;
    *result = left * right;
    return 1;
}

static void* take(layout* value, size_t count, size_t element_size, size_t alignment)
{
    size_t bytes;
    size_t padding;
    size_t start;
    size_t end;
    if (value->failed || alignment == 0U ||
        !multiply_size(count, element_size, &bytes)) {
        value->failed = 1U;
        return NULL;
    }
    padding = (alignment - value->offset % alignment) % alignment;
    if (!add_size(value->offset, padding, &start) || !add_size(start, bytes, &end) ||
        (value->base != NULL && end > value->capacity)) {
        value->failed = 1U;
        return NULL;
    }
    value->offset = end;
    return value->base == NULL || count == 0U ? NULL : value->base + start;
}

static int reserve(builder* value, size_t additional)
{
    size_t needed;
    size_t capacity;
    char* replacement;
    if (value->failed || !add_size(value->length, additional, &needed) ||
        !add_size(needed, 1U, &needed)) {
        value->failed = 1U;
        return 0;
    }
    if (needed <= value->capacity) return 1;
    capacity = value->capacity ? value->capacity : 512U;
    while (capacity < needed) {
        if (capacity > SIZE_MAX / 2U) {
            capacity = needed;
            break;
        }
        capacity *= 2U;
    }
    replacement = (char*)game_rules_c_allocate_owned(&value->allocator, capacity);
    if (replacement == NULL) {
        value->failed = 1U;
        return 0;
    }
    if (value->length) memcpy(replacement, value->data, value->length);
    game_rules_c_deallocate_owned(value->data);
    value->data = replacement;
    value->capacity = capacity;
    value->data[value->length] = '\0';
    return 1;
}

static void bytes(builder* value, const char* source, size_t length)
{
    if (!reserve(value, length)) return;
    memcpy(value->data + value->length, source, length);
    value->length += length;
    value->data[value->length] = '\0';
}

static void text(builder* value, const char* source)
{
    bytes(value, source, strlen(source));
}

static void number_u64(builder* value, uint64_t number)
{
    char reversed[32];
    char output[32];
    size_t length = 0U;
    size_t index;
    do {
        reversed[length++] = (char)('0' + number % 10U);
        number /= 10U;
    } while (number != 0U);
    for (index = 0U; index < length; ++index) output[index] = reversed[length - index - 1U];
    bytes(value, output, length);
}

static void number_i32(builder* value, int32_t number)
{
    uint64_t magnitude;
    if (number < 0) {
        text(value, "-");
        magnitude = (uint64_t)(-(int64_t)number);
    } else {
        magnitude = (uint64_t)number;
    }
    number_u64(value, magnitude);
}

static void string_value(builder* value, const char* source, size_t length)
{
    static const char hex[] = "0123456789abcdef";
    size_t index;
    text(value, "\"");
    for (index = 0U; index < length; ++index) {
        unsigned char character = (unsigned char)source[index];
        if (character == '"') text(value, "\\\"");
        else if (character == '\\') text(value, "\\\\");
        else if (character == '\b') text(value, "\\b");
        else if (character == '\f') text(value, "\\f");
        else if (character == '\n') text(value, "\\n");
        else if (character == '\r') text(value, "\\r");
        else if (character == '\t') text(value, "\\t");
        else if (character < 0x20U) {
            char escaped[6] = {'\\', 'u', '0', '0',
                               hex[character >> 4U], hex[character & 15U]};
            bytes(value, escaped, 6U);
        } else {
            bytes(value, source + index, 1U);
        }
    }
    text(value, "\"");
}

static char* finish(builder* value)
{
    char* result;
    if (!reserve(value, 0U)) {
        game_rules_c_deallocate_owned(value->data);
        return NULL;
    }
    result = value->data;
    value->data = NULL;
    return result;
}

static const char* direction_name(uint32_t value)
{
    static const char* const names[] = {"north", "east", "south", "west"};
    return value <= GAME_RULES_DIRECTION_WEST ? names[value] : "unknown";
}

static const char* entity_name(uint32_t value)
{
    static const char* const names[] = {"player", "box", "barrel"};
    return value <= GAME_RULES_ENTITY_BARREL ? names[value] : "unknown";
}

static const char* color_name(uint32_t value)
{
    static const char* const names[] = {"red", "green", "blue", "yellow"};
    return value <= GAME_RULES_COLOR_YELLOW ? names[value] : "unknown";
}

static const char* outcome_name(uint32_t value)
{
    static const char* const names[] = {"ongoing", "won", "lost"};
    return value <= GAME_RULES_OUTCOME_LOST ? names[value] : "unknown";
}

static const char* movement_name(uint32_t value)
{
    static const char* const names[] = {"player", "blast", "fall", "slide"};
    return value <= GAME_RULES_MOVEMENT_SLIDE ? names[value] : "unknown";
}

static const char* move_status_name(uint32_t value)
{
    static const char* const names[] = {
        "moved", "no_level", "invalid_direction", "world_boundary", "ledge",
        "occupied", "stacked_push_target", "closed_door", "teleporter_restriction",
        "unsupported_geometry", "level_terminal"};
    return value <= GAME_RULES_MOVE_LEVEL_TERMINAL ? names[value] : "unknown";
}

static void coordinate_json(builder* output, game_rules_coordinate coordinate)
{
    text(output, "{\"x\":");
    number_i32(output, coordinate.x);
    text(output, ",\"y\":");
    number_i32(output, coordinate.y);
    text(output, "}");
}

static void entity_json(builder* output, const game_rules_entity* entity)
{
    text(output, "{\"id\":\"");
    number_u64(output, entity->id);
    text(output, "\",\"type\":\"");
    text(output, entity_name(entity->kind));
    text(output, "\",\"coordinate\":");
    coordinate_json(output, entity->coordinate);
    text(output, ",\"bottomHalfSteps\":");
    number_i32(output, entity->bottom_half_steps);
    text(output, "}");
}

static void resolved_json(builder* output, const game_rules_c_state* state)
{
    uint32_t index;
    text(output, "{\"entities\":[");
    for (index = 0U; index < state->entity_count; ++index) {
        if (index) text(output, ",");
        entity_json(output, &state->entities[index]);
    }
    text(output, "],\"armedBarrelIds\":[");
    for (index = 0U; index < state->armed_barrel_count; ++index) {
        if (index) text(output, ",");
        text(output, "\"");
        number_u64(output, state->armed_barrel_ids[index]);
        text(output, "\"");
    }
    text(output, "],\"activeSwitchColors\":[");
    for (index = 0U; index < state->active_switch_color_count; ++index) {
        const char* name = color_name(state->active_switch_colors[index]);
        if (index) text(output, ",");
        string_value(output, name, strlen(name));
    }
    text(output, "],\"openDoorCoordinates\":[");
    for (index = 0U; index < state->open_door_count; ++index) {
        if (index) text(output, ",");
        coordinate_json(output, state->open_doors[index]);
    }
    text(output, "],\"outcome\":\"");
    text(output, outcome_name(state->outcome));
    text(output, "\"}");
}

static void cell_json(builder* output, const game_rules_cell* cell)
{
    text(output, "{\"coordinate\":");
    coordinate_json(output, cell->coordinate);
    if (cell->kind == GAME_RULES_CELL_FLAT) {
        text(output, ",\"type\":\"flat\",\"elevation\":");
        number_i32(output, cell->elevation);
    } else {
        text(output, ",\"type\":\"ramp\",\"lowDirection\":\"");
        text(output, direction_name(cell->low_direction));
        text(output, "\",\"lowElevation\":");
        number_i32(output, cell->elevation);
    }
    text(output, "}");
}

static void fixture_json(builder* output, const game_rules_fixture* fixture)
{
    text(output, "{\"coordinate\":");
    coordinate_json(output, fixture->coordinate);
    if (fixture->kind == GAME_RULES_FIXTURE_EXIT) {
        text(output, ",\"type\":\"exit\"}");
        return;
    }
    text(output, fixture->kind == GAME_RULES_FIXTURE_SWITCH
                     ? ",\"type\":\"switch\",\"color\":\""
                     : ",\"type\":\"door\",\"color\":\"");
    text(output, color_name(fixture->color));
    text(output, "\"}");
}

static void state_json_with_resolved(builder* output,
                                     const game_rules_session* session,
                                     const game_rules_c_state* resolved)
{
    uint32_t index;
    if (session == NULL || !session->has_level) {
        text(output, "null");
        return;
    }
    text(output, "{\"coordinateSystem\":{\"origin\":");
    coordinate_json(output, session->coordinates.origin);
    text(output, ",\"positiveX\":\"");
    text(output, session->coordinates.positive_x == GAME_RULES_HORIZONTAL_EAST
                     ? "east" : "west");
    text(output, "\",\"positiveY\":\"");
    text(output, session->coordinates.positive_y == GAME_RULES_VERTICAL_NORTH
                     ? "north" : "south");
    text(output, "\"},\"width\":");
    number_u64(output, session->width);
    text(output, ",\"height\":");
    number_u64(output, session->height);
    text(output, ",\"cells\":[");
    for (index = 0U; index < session->cell_count; ++index) {
        if (index) text(output, ",");
        cell_json(output, &session->cells[index]);
    }
    text(output, "],\"fixtures\":[");
    for (index = 0U; index < session->fixture_count; ++index) {
        if (index) text(output, ",");
        fixture_json(output, &session->fixtures[index]);
    }
    text(output, "],\"entities\":");
    {
        game_rules_c_state state = *resolved;
        text(output, "[");
        for (index = 0U; index < state.entity_count; ++index) {
            if (index) text(output, ",");
            entity_json(output, &state.entities[index]);
        }
        text(output, "],\"armedBarrelIds\":[");
        for (index = 0U; index < state.armed_barrel_count; ++index) {
            if (index) text(output, ",");
            text(output, "\"");
            number_u64(output, state.armed_barrel_ids[index]);
            text(output, "\"");
        }
        text(output, "],\"activeSwitchColors\":[");
        for (index = 0U; index < state.active_switch_color_count; ++index) {
            const char* name = color_name(state.active_switch_colors[index]);
            if (index) text(output, ",");
            string_value(output, name, strlen(name));
        }
        text(output, "],\"openDoorCoordinates\":[");
        for (index = 0U; index < state.open_door_count; ++index) {
            if (index) text(output, ",");
            coordinate_json(output, state.open_doors[index]);
        }
        text(output, "],\"outcome\":\"");
        text(output, outcome_name(state.outcome));
        text(output, "\"}");
    }
}

static void state_json(builder* output, const game_rules_session* session)
{
    state_json_with_resolved(output, session,
                             session == NULL ? NULL : &session->current_state);
}

static void event_json(builder* output, const game_rules_event* event)
{
    switch (event->kind) {
    case GAME_RULES_EVENT_MOVE_BLOCKED:
        text(output, "{\"type\":\"moveBlocked\",\"direction\":\"");
        text(output, direction_name(event->direction));
        text(output, "\",\"reason\":\"");
        text(output, move_status_name(event->move_status));
        text(output, "\"}");
        break;
    case GAME_RULES_EVENT_ENTITY_MOVED:
        text(output, "{\"type\":\"entityMoved\",\"entityId\":\"");
        number_u64(output, event->entity_id);
        text(output, "\",\"from\":");
        coordinate_json(output, event->from);
        text(output, ",\"to\":");
        coordinate_json(output, event->to);
        text(output, ",\"oldBottomHalfSteps\":");
        number_i32(output, event->old_bottom_half_steps);
        text(output, ",\"newBottomHalfSteps\":");
        number_i32(output, event->new_bottom_half_steps);
        text(output, ",\"cause\":\"");
        text(output, movement_name(event->movement_cause));
        text(output, "\"}");
        break;
    case GAME_RULES_EVENT_BARREL_ARMED:
        text(output, "{\"type\":\"barrelArmed\",\"entityId\":\"");
        number_u64(output, event->entity_id);
        text(output, "\"}");
        break;
    case GAME_RULES_EVENT_BARREL_EXPLODED:
        text(output, "{\"type\":\"barrelExploded\",\"entityId\":\"");
        number_u64(output, event->entity_id);
        text(output, "\",\"coordinate\":");
        coordinate_json(output, event->coordinate);
        text(output, ",\"bottomHalfSteps\":");
        number_i32(output, event->bottom_half_steps);
        text(output, "}");
        break;
    case GAME_RULES_EVENT_PLAYER_CRUSHED:
        text(output, "{\"type\":\"playerCrushed\",\"playerId\":\"");
        number_u64(output, event->entity_id);
        text(output, "\",\"crushingEntityId\":\"");
        number_u64(output, event->other_entity_id);
        text(output, "\"}");
        break;
    case GAME_RULES_EVENT_SWITCH_CHANGED:
        text(output, "{\"type\":\"switchChanged\",\"color\":\"");
        text(output, color_name(event->color));
        text(output, "\",\"active\":");
        text(output, event->active ? "true" : "false");
        text(output, "}");
        break;
    case GAME_RULES_EVENT_DOOR_OPENED:
    case GAME_RULES_EVENT_DOOR_CLOSED:
        text(output, event->kind == GAME_RULES_EVENT_DOOR_OPENED
                         ? "{\"type\":\"doorOpened\",\"coordinate\":"
                         : "{\"type\":\"doorClosed\",\"coordinate\":");
        coordinate_json(output, event->coordinate);
        text(output, ",\"color\":\"");
        text(output, color_name(event->color));
        text(output, "\"}");
        break;
    case GAME_RULES_EVENT_LEVEL_WON:
        text(output, "{\"type\":\"levelWon\"}");
        break;
    case GAME_RULES_EVENT_LEVEL_LOST:
        text(output, "{\"type\":\"levelLost\"}");
        break;
    default:
        text(output, "{}");
        break;
    }
}

static void event_array_json(builder* output,
                             const game_rules_event* events,
                             uint32_t count)
{
    uint32_t index;
    text(output, "[");
    for (index = 0U; index < count; ++index) {
        if (index) text(output, ",");
        event_json(output, &events[index]);
    }
    text(output, "]");
}

static void ticks_json(builder* output, const game_rules_session* session)
{
    uint32_t tick_index;
    text(output, "[");
    for (tick_index = 0U; tick_index < session->initialization_tick_count; ++tick_index) {
        const game_rules_c_tick* tick = &session->initialization_ticks[tick_index];
        uint32_t event_index;
        if (tick_index) text(output, ",");
        text(output, "{\"index\":");
        number_u64(output, tick->index);
        text(output, ",\"events\":[");
        for (event_index = 0U; event_index < tick->event_count; ++event_index) {
            if (event_index) text(output, ",");
            event_json(output, &tick->events[event_index]);
        }
        text(output, "],\"stateAfter\":");
        resolved_json(output, &tick->state_after);
        text(output, "}");
    }
    text(output, "]");
}

static void errors_json(builder* output,
                        const game_rules_validation_error* errors,
                        uint32_t count)
{
    uint32_t index;
    text(output, "[");
    for (index = 0U; index < count; ++index) {
        if (index) text(output, ",");
        text(output, "{\"code\":\"");
        text(output, game_rules_c_validation_error_name(errors[index].code));
        text(output, "\",\"coordinate\":");
        coordinate_json(output, errors[index].coordinate);
        text(output, ",\"entityId\":\"");
        number_u64(output, errors[index].entity_id);
        text(output, "\"}");
    }
    text(output, "]");
}

char* game_rules_c_stage03_get_state(game_rules_engine* engine)
{
    builder output = {0};
    output.allocator = engine->allocator;
    text(&output, "{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\"");
    text(&output, engine->session && engine->session->has_level ? "ok" : "no_level");
    text(&output, "\",\"state\":");
    state_json(&output, engine->session);
    text(&output, "}");
    return finish(&output);
}

char* game_rules_c_stage03_load_json(game_rules_engine* engine,
                                     const char* json,
                                     uint32_t length)
{
    game_rules_c_decode_result decoded;
    game_rules_session* replacement = NULL;
    builder output = {0};
    char* response;
    game_rules_c_decode_level_json(json, length, &engine->allocator, &decoded);
    if (decoded.status == GAME_RULES_C_DECODE_ALLOCATION_FAILED) {
        game_rules_c_decode_result_destroy(&decoded);
        return NULL;
    }
    if (decoded.status == GAME_RULES_C_DECODE_OK) {
        replacement = game_rules_c_build_resolved_session(
            &engine->allocator, &decoded.level.view);
        if (replacement == NULL) {
            game_rules_c_decode_result_destroy(&decoded);
            return NULL;
        }
    }
    output.allocator = engine->allocator;
    text(&output, "{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"");
    if (decoded.status == GAME_RULES_C_DECODE_JSON_ERROR) {
        text(&output, "invalid_json\",\"error\":{\"code\":\"");
        text(&output, game_rules_c_json_error_name(decoded.json_error.code));
        text(&output, "\",\"byteOffset\":");
        number_u64(&output, decoded.json_error.byte_offset);
        text(&output, ",\"path\":");
        string_value(&output, decoded.json_error.path ? decoded.json_error.path : "",
                     decoded.json_error.path_length);
        text(&output, "},\"state\":");
        state_json(&output, engine->session);
    } else if (decoded.status == GAME_RULES_C_DECODE_INVALID_LEVEL) {
        text(&output, "invalid_level\",\"errors\":");
        errors_json(&output, decoded.validation.errors, decoded.validation.count);
        text(&output, ",\"state\":");
        state_json(&output, engine->session);
    } else {
        text(&output, "loaded\",\"initialState\":");
        resolved_json(&output, &replacement->initial_state);
        text(&output, ",\"ticks\":");
        ticks_json(&output, replacement);
        text(&output, ",\"state\":");
        state_json(&output, replacement);
        text(&output, ",\"outcome\":\"");
        text(&output, outcome_name(replacement->current_state.outcome));
        text(&output, "\"");
    }
    text(&output, "}");
    response = finish(&output);
    if (response != NULL && replacement != NULL) {
        game_rules_session* previous = engine->session;
        engine->session = replacement;
        replacement = NULL;
        game_rules_c_destroy_session(previous);
    }
    game_rules_c_destroy_session(replacement);
    game_rules_c_decode_result_destroy(&decoded);
    return response;
}

static void plan_command(game_rules_engine* engine,
                         uint32_t direction,
                         game_rules_c_command_transaction* transaction,
                         game_rules_event* no_level_event)
{
    game_rules_c_plan_flat_move(engine->session, direction, transaction);
    if (transaction->status == GAME_RULES_MOVE_NO_LEVEL &&
        transaction->has_direction && transaction->event_count == 0U) {
        memset(no_level_event, 0, sizeof(*no_level_event));
        no_level_event->kind = GAME_RULES_EVENT_MOVE_BLOCKED;
        no_level_event->direction = direction;
        no_level_event->move_status = GAME_RULES_MOVE_NO_LEVEL;
        transaction->events = no_level_event;
        transaction->event_count = 1U;
    }
}

char* game_rules_c_stage04_move_json(game_rules_engine* engine, uint32_t direction)
{
    game_rules_c_command_transaction transaction;
    game_rules_event no_level_event;
    const game_rules_c_state* response_state;
    builder output = {0};
    char* response;
    plan_command(engine, direction, &transaction, &no_level_event);
    response_state = transaction.accepted
        ? transaction.final_state
        : (engine->session && engine->session->has_level
               ? &engine->session->current_state : NULL);

    output.allocator = engine->allocator;
    text(&output, "{\"apiVersion\":1,\"operation\":\"move\",\"status\":\"");
    text(&output, move_status_name(transaction.status));
    text(&output, "\",\"accepted\":");
    text(&output, transaction.accepted ? "true" : "false");
    text(&output, ",\"direction\":");
    if (transaction.has_direction) {
        string_value(&output, direction_name(direction), strlen(direction_name(direction)));
    } else {
        text(&output, "null");
    }
    text(&output, ",\"events\":");
    event_array_json(&output, transaction.events, transaction.event_count);
    text(&output, ",\"initialState\":");
    if (transaction.initial_state != NULL) {
        resolved_json(&output, transaction.initial_state);
    } else {
        text(&output, "null");
    }
    text(&output, ",\"ticks\":[");
    if (transaction.accepted) {
        text(&output, "{\"index\":0,\"events\":");
        event_array_json(&output, transaction.tick_events,
                         transaction.tick_event_count);
        text(&output, ",\"stateAfter\":");
        resolved_json(&output, transaction.final_state);
        text(&output, "}");
    }
    text(&output, "],\"state\":");
    if (response_state != NULL) {
        state_json_with_resolved(&output, engine->session, response_state);
    } else {
        text(&output, "null");
    }
    text(&output, ",\"outcome\":");
    if (response_state != NULL) {
        string_value(&output, outcome_name(response_state->outcome),
                     strlen(outcome_name(response_state->outcome)));
    } else {
        text(&output, "null");
    }
    text(&output, "}");
    response = finish(&output);
    if (response != NULL && transaction.accepted) {
        game_rules_c_commit_command(engine->session, &transaction);
    }
    return response;
}

static void copy_resolved(layout* arena,
                          const game_rules_c_state* source,
                          game_rules_resolved_state* output)
{
    game_rules_entity* entities = (game_rules_entity*)take(arena, source->entity_count,
        sizeof(game_rules_entity), _Alignof(game_rules_entity));
    uint64_t* armed = (uint64_t*)take(arena, source->armed_barrel_count,
        sizeof(uint64_t), _Alignof(uint64_t));
    uint32_t* colors = (uint32_t*)take(arena, source->active_switch_color_count,
        sizeof(uint32_t), _Alignof(uint32_t));
    game_rules_coordinate* doors = (game_rules_coordinate*)take(arena,
        source->open_door_count, sizeof(game_rules_coordinate),
        _Alignof(game_rules_coordinate));
    memset(output, 0, sizeof(*output));
    if (arena->base != NULL && !arena->failed) {
        if (source->entity_count) memcpy(entities, source->entities,
            source->entity_count * sizeof(game_rules_entity));
        if (source->armed_barrel_count) memcpy(armed, source->armed_barrel_ids,
            source->armed_barrel_count * sizeof(uint64_t));
        if (source->active_switch_color_count) memcpy(colors,
            source->active_switch_colors,
            source->active_switch_color_count * sizeof(uint32_t));
        if (source->open_door_count) memcpy(doors, source->open_doors,
            source->open_door_count * sizeof(game_rules_coordinate));
    }
    output->entities = entities;
    output->entity_count = source->entity_count;
    output->armed_barrel_ids = armed;
    output->armed_barrel_count = source->armed_barrel_count;
    output->active_switch_colors = colors;
    output->active_switch_color_count = source->active_switch_color_count;
    output->open_doors = doors;
    output->open_door_count = source->open_door_count;
    output->outcome = source->outcome;
}

static void copy_snapshot_resolved(layout* arena,
                                   const game_rules_session* session,
                                   const game_rules_c_state* resolved,
                                   game_rules_snapshot* output)
{
    game_rules_cell* cells = (game_rules_cell*)take(arena, session->cell_count,
        sizeof(game_rules_cell), _Alignof(game_rules_cell));
    game_rules_fixture* fixtures = (game_rules_fixture*)take(arena, session->fixture_count,
        sizeof(game_rules_fixture), _Alignof(game_rules_fixture));
    memset(output, 0, sizeof(*output));
    if (arena->base != NULL && !arena->failed) {
        if (session->cell_count) memcpy(cells, session->cells,
            session->cell_count * sizeof(game_rules_cell));
        if (session->fixture_count) memcpy(fixtures, session->fixtures,
            session->fixture_count * sizeof(game_rules_fixture));
    }
    output->level.coordinates = session->coordinates;
    output->level.width = session->width;
    output->level.height = session->height;
    output->level.cells = cells;
    output->level.cell_count = session->cell_count;
    output->level.fixtures = fixtures;
    output->level.fixture_count = session->fixture_count;
    copy_resolved(arena, resolved, &output->resolved);
}

static void copy_snapshot(layout* arena,
                          const game_rules_session* session,
                          game_rules_snapshot* output)
{
    copy_snapshot_resolved(arena, session, &session->current_state, output);
}

static void copy_ticks(layout* arena,
                       const game_rules_session* session,
                       game_rules_load_result* output)
{
    game_rules_tick* ticks = (game_rules_tick*)take(arena,
        session->initialization_tick_count, sizeof(game_rules_tick),
        _Alignof(game_rules_tick));
    uint32_t tick_index;
    output->ticks = ticks;
    output->tick_count = session->initialization_tick_count;
    for (tick_index = 0U; tick_index < session->initialization_tick_count; ++tick_index) {
        const game_rules_c_tick* source = &session->initialization_ticks[tick_index];
        game_rules_tick measured;
        game_rules_tick* target = arena->base != NULL ? &ticks[tick_index] : &measured;
        game_rules_event* events;
        memset(target, 0, sizeof(*target));
        events = (game_rules_event*)take(arena, source->event_count,
            sizeof(game_rules_event), _Alignof(game_rules_event));
        target->index = source->index;
        target->events = events;
        target->event_count = source->event_count;
        if (arena->base != NULL && source->event_count && !arena->failed) {
            memcpy(events, source->events, source->event_count * sizeof(game_rules_event));
        }
        copy_resolved(arena, &source->state_after, &target->state_after);
    }
}

static void load_graph(layout* arena,
                       const game_rules_session* current,
                       const game_rules_session* accepted,
                       const game_rules_validation_error* errors,
                       uint32_t error_count,
                       game_rules_load_result* output)
{
    game_rules_validation_error* copied = (game_rules_validation_error*)take(arena,
        error_count, sizeof(game_rules_validation_error),
        _Alignof(game_rules_validation_error));
    output->errors = copied;
    output->error_count = error_count;
    if (arena->base != NULL && error_count && !arena->failed)
        memcpy(copied, errors, error_count * sizeof(game_rules_validation_error));
    if (accepted == NULL) {
        output->status = GAME_RULES_LOAD_INVALID_LEVEL;
        if (current != NULL) {
            output->has_state = 1U;
            copy_snapshot(arena, current, &output->state);
        }
        return;
    }
    output->status = GAME_RULES_LOAD_LOADED;
    output->accepted = 1U;
    output->has_initial_state = 1U;
    copy_resolved(arena, &accepted->initial_state, &output->initial_state);
    copy_ticks(arena, accepted, output);
    output->has_final_state = 1U;
    copy_resolved(arena, &accepted->current_state, &output->final_state);
    output->has_state = 1U;
    copy_snapshot(arena, accepted, &output->state);
    output->has_outcome = 1U;
    output->outcome = accepted->current_state.outcome;
}

static uint32_t make_load_result(const game_rules_engine* engine,
                                 const game_rules_session* current,
                                 const game_rules_session* accepted,
                                 const game_rules_validation_error* errors,
                                 uint32_t error_count,
                                 game_rules_load_result* output)
{
    layout measure = {0};
    layout arena = {0};
    game_rules_load_result ignored = {0};
    void* owner;
    load_graph(&measure, current, accepted, errors, error_count, &ignored);
    if (measure.failed) return GAME_RULES_CALL_ALLOCATION_FAILED;
    owner = game_rules_c_allocate_owned(&engine->allocator, measure.offset);
    if (owner == NULL) return GAME_RULES_CALL_ALLOCATION_FAILED;
    arena.base = (unsigned char*)owner;
    arena.capacity = measure.offset;
    load_graph(&arena, current, accepted, errors, error_count, output);
    if (arena.failed) {
        game_rules_c_deallocate_owned(owner);
        memset(output, 0, sizeof(*output));
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    output->owned_storage = owner;
    return GAME_RULES_CALL_OK;
}

static int typed_header_valid(const game_rules_level_definition* level)
{
    return !((level->cell_count && level->cells == NULL) ||
             (level->fixture_count && level->fixtures == NULL) ||
             (level->entity_count && level->entities == NULL) ||
             level->coordinates.positive_x > GAME_RULES_HORIZONTAL_WEST ||
             level->coordinates.positive_y > GAME_RULES_VERTICAL_SOUTH);
}

static int typed_members_valid(const game_rules_level_definition* level)
{
    uint32_t index;
    for (index = 0U; index < level->cell_count; ++index) {
        if (level->cells[index].kind > GAME_RULES_CELL_RAMP ||
            (level->cells[index].kind == GAME_RULES_CELL_RAMP &&
             level->cells[index].low_direction > GAME_RULES_DIRECTION_WEST)) return 0;
    }
    for (index = 0U; index < level->fixture_count; ++index) {
        if (level->fixtures[index].kind > GAME_RULES_FIXTURE_EXIT ||
            (level->fixtures[index].kind != GAME_RULES_FIXTURE_EXIT &&
             level->fixtures[index].color > GAME_RULES_COLOR_YELLOW)) return 0;
    }
    for (index = 0U; index < level->entity_count; ++index) {
        if (level->entities[index].kind > GAME_RULES_ENTITY_BARREL) return 0;
    }
    return 1;
}

static int typed_counts_feasible(const game_rules_engine* engine,
                                 const game_rules_level_definition* level)
{
    size_t cells;
    size_t fixtures;
    size_t entities;
    size_t total;
    void* probe;
    if (!multiply_size(level->cell_count, sizeof(game_rules_cell), &cells) ||
        !multiply_size(level->fixture_count, sizeof(game_rules_fixture), &fixtures) ||
        !multiply_size(level->entity_count, sizeof(game_rules_entity), &entities) ||
        !add_size(cells, fixtures, &total) || !add_size(total, entities, &total)) return 0;
    if (total == 0U) return 1;
    probe = game_rules_c_allocate_owned(&engine->allocator, total);
    if (probe == NULL) return 0;
    game_rules_c_deallocate_owned(probe);
    return 1;
}

static game_rules_c_level_view level_view(const game_rules_level_definition* level)
{
    game_rules_c_level_view view;
    view.coordinates = level->coordinates;
    view.width = level->width;
    view.height = level->height;
    view.cells = level->cells;
    view.cell_count = level->cell_count;
    view.fixtures = level->fixtures;
    view.fixture_count = level->fixture_count;
    view.entities = level->entities;
    view.entity_count = level->entity_count;
    return view;
}

uint32_t game_rules_c_stage03_get_state_data(const game_rules_engine* engine,
                                             game_rules_state_result* result)
{
    layout measure = {0};
    layout arena = {0};
    game_rules_snapshot ignored;
    void* owner;
    if (engine->session && engine->session->has_level) {
        copy_snapshot(&measure, engine->session, &ignored);
        if (measure.failed) return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    owner = game_rules_c_allocate_owned(&engine->allocator, measure.offset);
    if (owner == NULL) return GAME_RULES_CALL_ALLOCATION_FAILED;
    if (engine->session && engine->session->has_level) {
        arena.base = (unsigned char*)owner;
        arena.capacity = measure.offset;
        result->has_state = 1U;
        copy_snapshot(&arena, engine->session, &result->state);
        if (arena.failed) {
            game_rules_c_deallocate_owned(owner);
            memset(result, 0, sizeof(*result));
            return GAME_RULES_CALL_ALLOCATION_FAILED;
        }
    }
    result->owned_storage = owner;
    return GAME_RULES_CALL_OK;
}

uint32_t game_rules_c_stage03_load_data(game_rules_engine* engine,
                                        const game_rules_level_definition* level,
                                        game_rules_load_result* result)
{
    game_rules_c_level_view view;
    game_rules_c_validation_result validation;
    game_rules_session* replacement = NULL;
    uint32_t call;
    if (!typed_header_valid(level)) return GAME_RULES_CALL_INVALID_ARGUMENT;
    if (!typed_counts_feasible(engine, level)) return GAME_RULES_CALL_ALLOCATION_FAILED;
    if (!typed_members_valid(level)) return GAME_RULES_CALL_INVALID_ARGUMENT;
    view = level_view(level);
    game_rules_c_validate_level(&view, &engine->allocator, &validation);
    if (validation.allocation_failed) {
        game_rules_c_validation_result_destroy(&validation);
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    if (validation.count == 0U) {
        replacement = game_rules_c_build_resolved_session(&engine->allocator, &view);
        if (replacement == NULL) {
            game_rules_c_validation_result_destroy(&validation);
            return GAME_RULES_CALL_ALLOCATION_FAILED;
        }
    }
    call = make_load_result(engine,
        engine->session && engine->session->has_level ? engine->session : NULL,
        replacement, validation.errors, validation.count, result);
    if (call == GAME_RULES_CALL_OK && replacement != NULL) {
        game_rules_session* previous = engine->session;
        engine->session = replacement;
        replacement = NULL;
        game_rules_c_destroy_session(previous);
    }
    game_rules_c_destroy_session(replacement);
    game_rules_c_validation_result_destroy(&validation);
    return call;
}

static void copy_events(layout* arena,
                        const game_rules_event* source,
                        uint32_t count,
                        const game_rules_event** output)
{
    game_rules_event* events = (game_rules_event*)take(
        arena, count, sizeof(game_rules_event), _Alignof(game_rules_event));
    if (arena->base != NULL && count && !arena->failed) {
        memcpy(events, source, count * sizeof(game_rules_event));
    }
    *output = events;
}

static void move_graph(layout* arena,
                       const game_rules_session* session,
                       const game_rules_c_command_transaction* transaction,
                       game_rules_move_result* output)
{
    const game_rules_c_state* response_state = transaction->accepted
        ? transaction->final_state
        : (session && session->has_level ? &session->current_state : NULL);
    output->status = transaction->status;
    output->accepted = transaction->accepted;
    output->has_direction = transaction->has_direction;
    output->direction = transaction->has_direction ? transaction->direction : 0U;
    copy_events(arena, transaction->events, transaction->event_count,
                &output->events);
    output->event_count = transaction->event_count;
    if (transaction->initial_state != NULL) {
        output->has_initial_state = 1U;
        copy_resolved(arena, transaction->initial_state, &output->initial_state);
    }
    if (transaction->accepted) {
        game_rules_tick measured;
        game_rules_tick* ticks = (game_rules_tick*)take(
            arena, 1U, sizeof(game_rules_tick), _Alignof(game_rules_tick));
        game_rules_tick* tick = arena->base != NULL ? ticks : &measured;
        memset(tick, 0, sizeof(*tick));
        output->ticks = ticks;
        output->tick_count = 1U;
        tick->index = 0U;
        copy_events(arena, transaction->tick_events,
                    transaction->tick_event_count, &tick->events);
        tick->event_count = transaction->tick_event_count;
        copy_resolved(arena, transaction->final_state, &tick->state_after);
    }
    if (transaction->final_state != NULL) {
        output->has_final_state = 1U;
        copy_resolved(arena, transaction->final_state, &output->final_state);
    }
    if (response_state != NULL) {
        output->has_state = 1U;
        copy_snapshot_resolved(arena, session, response_state, &output->state);
        output->has_outcome = 1U;
        output->outcome = response_state->outcome;
    }
}

uint32_t game_rules_c_stage04_move_data(game_rules_engine* engine,
                                        uint32_t direction,
                                        game_rules_move_result* result)
{
    game_rules_c_command_transaction transaction;
    game_rules_event no_level_event;
    game_rules_move_result ignored = {0};
    layout measure = {0};
    layout arena = {0};
    void* owner;
    plan_command(engine, direction, &transaction, &no_level_event);
    move_graph(&measure, engine->session, &transaction, &ignored);
    if (measure.failed) return GAME_RULES_CALL_ALLOCATION_FAILED;
    owner = game_rules_c_allocate_owned(&engine->allocator, measure.offset);
    if (owner == NULL) return GAME_RULES_CALL_ALLOCATION_FAILED;
    arena.base = (unsigned char*)owner;
    arena.capacity = measure.offset;
    move_graph(&arena, engine->session, &transaction, result);
    if (arena.failed) {
        game_rules_c_deallocate_owned(owner);
        memset(result, 0, sizeof(*result));
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    result->owned_storage = owner;
    if (transaction.accepted) {
        game_rules_c_commit_command(engine->session, &transaction);
    }
    return GAME_RULES_CALL_OK;
}
