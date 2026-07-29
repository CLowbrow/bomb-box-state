#include "c_api_internal.h"

#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
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

static int add_size(size_t a, size_t b, size_t* result)
{
    if (a > SIZE_MAX - b) return 0;
    *result = a + b;
    return 1;
}

static int multiply_size(size_t a, size_t b, size_t* result)
{
    if (b != 0U && a > SIZE_MAX / b) return 0;
    *result = a * b;
    return 1;
}

static void* take(layout* value, size_t count, size_t element_size, size_t alignment)
{
    size_t bytes;
    size_t padding;
    size_t start;
    size_t end;
    if (value->failed || alignment == 0U || !multiply_size(count, element_size, &bytes)) {
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

static int same_coordinate(game_rules_coordinate a, game_rules_coordinate b)
{
    return a.x == b.x && a.y == b.y;
}

static const game_rules_cell* find_cell(const game_rules_session* session,
                                        game_rules_coordinate coordinate)
{
    uint32_t i;
    for (i = 0U; i < session->cell_count; ++i) {
        if (same_coordinate(session->cells[i].coordinate, coordinate)) return &session->cells[i];
    }
    return NULL;
}

static int occupied(const game_rules_session* session, game_rules_coordinate coordinate)
{
    uint32_t i;
    for (i = 0U; i < session->entity_count; ++i) {
        if (same_coordinate(session->entities[i].coordinate, coordinate)) return 1;
    }
    return 0;
}

static int switch_pressed(const game_rules_session* session,
                          const game_rules_fixture* fixture)
{
    const game_rules_cell* cell = find_cell(session, fixture->coordinate);
    uint32_t i;
    if (cell == NULL || cell->kind != GAME_RULES_CELL_FLAT) return 0;
    for (i = 0U; i < session->entity_count; ++i) {
        if (same_coordinate(session->entities[i].coordinate, fixture->coordinate) &&
            (int64_t)session->entities[i].bottom_half_steps == (int64_t)cell->elevation * 2) {
            return 1;
        }
    }
    return 0;
}

static int color_active(const game_rules_session* session, uint32_t color)
{
    uint32_t i;
    for (i = 0U; i < session->active_color_count; ++i) {
        if (session->active_colors[i] == color) return 1;
    }
    return 0;
}

static int player_on_exit(const game_rules_session* session)
{
    uint32_t i;
    uint32_t j;
    for (i = 0U; i < session->entity_count; ++i) {
        const game_rules_entity* entity = &session->entities[i];
        if (entity->kind != GAME_RULES_ENTITY_PLAYER) continue;
        for (j = 0U; j < session->fixture_count; ++j) {
            const game_rules_fixture* fixture = &session->fixtures[j];
            if (fixture->kind == GAME_RULES_FIXTURE_EXIT &&
                same_coordinate(fixture->coordinate, entity->coordinate)) {
                const game_rules_cell* cell = find_cell(session, entity->coordinate);
                return cell != NULL && cell->kind == GAME_RULES_CELL_FLAT &&
                    (int64_t)entity->bottom_half_steps == (int64_t)cell->elevation * 2;
            }
        }
    }
    return 0;
}

static void derive_initial(game_rules_session* session)
{
    uint32_t color;
    uint32_t i;
    session->outcome = GAME_RULES_OUTCOME_ONGOING;
    if (player_on_exit(session)) {
        session->initial_events[0].kind = GAME_RULES_EVENT_LEVEL_WON;
        session->initial_event_count = 1U;
        session->outcome = GAME_RULES_OUTCOME_WON;
        return;
    }
    for (color = GAME_RULES_COLOR_RED; color <= GAME_RULES_COLOR_YELLOW; ++color) {
        int any = 0;
        int all = 1;
        for (i = 0U; i < session->fixture_count; ++i) {
            const game_rules_fixture* fixture = &session->fixtures[i];
            if (fixture->kind == GAME_RULES_FIXTURE_SWITCH && fixture->color == color) {
                any = 1;
                if (!switch_pressed(session, fixture)) all = 0;
            }
        }
        if (any && all) {
            game_rules_event* event;
            session->active_colors[session->active_color_count++] = color;
            event = &session->initial_events[session->initial_event_count++];
            event->kind = GAME_RULES_EVENT_SWITCH_CHANGED;
            event->color = color;
            event->active = 1U;
        }
    }
    for (i = 0U; i < session->fixture_count; ++i) {
        const game_rules_fixture* fixture = &session->fixtures[i];
        if (fixture->kind == GAME_RULES_FIXTURE_DOOR &&
            (color_active(session, fixture->color) || occupied(session, fixture->coordinate))) {
            game_rules_event* event;
            session->open_doors[session->open_door_count++] = fixture->coordinate;
            event = &session->initial_events[session->initial_event_count++];
            event->kind = GAME_RULES_EVENT_DOOR_OPENED;
            event->coordinate = fixture->coordinate;
            event->color = fixture->color;
        }
    }
}

static game_rules_session* build_session(const game_rules_c_allocator* allocator,
                                         const game_rules_c_level_view* level)
{
    layout measure = {0};
    layout storage = {0};
    game_rules_session* session;
    game_rules_c_owned_level canonical = {0};
    take(&measure, level->cell_count, sizeof(game_rules_cell), _Alignof(game_rules_cell));
    take(&measure, level->fixture_count, sizeof(game_rules_fixture), _Alignof(game_rules_fixture));
    take(&measure, level->entity_count, sizeof(game_rules_entity), _Alignof(game_rules_entity));
    take(&measure, 4U, sizeof(uint32_t), _Alignof(uint32_t));
    take(&measure, level->fixture_count, sizeof(game_rules_coordinate),
         _Alignof(game_rules_coordinate));
    take(&measure, (size_t)level->fixture_count + 5U, sizeof(game_rules_event),
         _Alignof(game_rules_event));
    if (measure.failed) return NULL;
    session = (game_rules_session*)game_rules_c_allocate_owned(allocator, sizeof(*session));
    if (session == NULL) return NULL;
    memset(session, 0, sizeof(*session));
    session->marker = 1U;
    session->level_storage = game_rules_c_allocate_owned(allocator, measure.offset);
    if (session->level_storage == NULL) {
        game_rules_c_destroy_session(session);
        return NULL;
    }
    session->history_storage = game_rules_c_allocate_owned(allocator, 1U);
    if (session->history_storage == NULL) {
        game_rules_c_destroy_session(session);
        return NULL;
    }
    storage.base = (unsigned char*)session->level_storage;
    storage.capacity = measure.offset;
    session->cells = (game_rules_cell*)take(&storage, level->cell_count,
                                             sizeof(game_rules_cell), _Alignof(game_rules_cell));
    session->fixtures = (game_rules_fixture*)take(&storage, level->fixture_count,
        sizeof(game_rules_fixture), _Alignof(game_rules_fixture));
    session->entities = (game_rules_entity*)take(&storage, level->entity_count,
        sizeof(game_rules_entity), _Alignof(game_rules_entity));
    session->active_colors = (uint32_t*)take(&storage, 4U, sizeof(uint32_t), _Alignof(uint32_t));
    session->open_doors = (game_rules_coordinate*)take(&storage, level->fixture_count,
        sizeof(game_rules_coordinate), _Alignof(game_rules_coordinate));
    session->initial_events = (game_rules_event*)take(&storage,
        (size_t)level->fixture_count + 5U, sizeof(game_rules_event), _Alignof(game_rules_event));
    if (storage.failed) {
        game_rules_c_destroy_session(session);
        return NULL;
    }
    session->has_level = 1U;
    session->coordinates = level->coordinates;
    session->width = level->width;
    session->height = level->height;
    session->cell_count = level->cell_count;
    session->fixture_count = level->fixture_count;
    session->entity_count = level->entity_count;
    if (level->cell_count) memcpy(session->cells, level->cells,
                                  level->cell_count * sizeof(*session->cells));
    if (level->fixture_count) memcpy(session->fixtures, level->fixtures,
                                     level->fixture_count * sizeof(*session->fixtures));
    if (level->entity_count) memcpy(session->entities, level->entities,
                                    level->entity_count * sizeof(*session->entities));
    canonical.cells = session->cells;
    canonical.fixtures = session->fixtures;
    canonical.entities = session->entities;
    canonical.view.cell_count = session->cell_count;
    canonical.view.fixture_count = session->fixture_count;
    canonical.view.entity_count = session->entity_count;
    game_rules_c_canonicalize_level(&canonical);
    derive_initial(session);
    return session;
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
        if (capacity > SIZE_MAX / 2U) { capacity = needed; break; }
        capacity *= 2U;
    }
    replacement = (char*)game_rules_c_allocate_owned(&value->allocator, capacity);
    if (replacement == NULL) { value->failed = 1U; return 0; }
    if (value->length) memcpy(replacement, value->data, value->length);
    game_rules_c_deallocate_owned(value->data);
    value->data = replacement;
    value->capacity = capacity;
    value->data[value->length] = '\0';
    return 1;
}

static void bytes(builder* value, const char* text, size_t length)
{
    if (!reserve(value, length)) return;
    memcpy(value->data + value->length, text, length);
    value->length += length;
    value->data[value->length] = '\0';
}

static void text(builder* value, const char* source) { bytes(value, source, strlen(source)); }

static void number_u64(builder* value, uint64_t number)
{
    char buffer[32];
    int written = snprintf(buffer, sizeof(buffer), "%llu", (unsigned long long)number);
    if (written < 0) value->failed = 1U; else bytes(value, buffer, (size_t)written);
}

static void number_i32(builder* value, int32_t number)
{
    char buffer[32];
    int written = snprintf(buffer, sizeof(buffer), "%d", (int)number);
    if (written < 0) value->failed = 1U; else bytes(value, buffer, (size_t)written);
}

static void string_value(builder* value, const char* source, size_t length)
{
    static const char hex[] = "0123456789abcdef";
    size_t i;
    text(value, "\"");
    for (i = 0U; i < length; ++i) {
        const unsigned char c = (unsigned char)source[i];
        if (c == '"') text(value, "\\\"");
        else if (c == '\\') text(value, "\\\\");
        else if (c == '\b') text(value, "\\b");
        else if (c == '\f') text(value, "\\f");
        else if (c == '\n') text(value, "\\n");
        else if (c == '\r') text(value, "\\r");
        else if (c == '\t') text(value, "\\t");
        else if (c < 0x20U) {
            char escaped[6] = {'\\', 'u', '0', '0', hex[c >> 4U], hex[c & 15U]};
            bytes(value, escaped, 6U);
        } else bytes(value, source + i, 1U);
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

static void coordinate_json(builder* out, game_rules_coordinate coordinate)
{
    text(out, "{\"x\":"); number_i32(out, coordinate.x);
    text(out, ",\"y\":"); number_i32(out, coordinate.y); text(out, "}");
}

static void entity_json(builder* out, const game_rules_entity* entity)
{
    text(out, "{\"id\":\""); number_u64(out, entity->id);
    text(out, "\",\"type\":\""); text(out, entity_name(entity->kind));
    text(out, "\",\"coordinate\":"); coordinate_json(out, entity->coordinate);
    text(out, ",\"bottomHalfSteps\":"); number_i32(out, entity->bottom_half_steps);
    text(out, "}");
}

static void entities_json(builder* out, const game_rules_session* session)
{
    uint32_t i;
    text(out, "[");
    for (i = 0U; i < session->entity_count; ++i) {
        if (i) text(out, ",");
        entity_json(out, &session->entities[i]);
    }
    text(out, "]");
}

static void resolved_json(builder* out, const game_rules_session* session, int initial)
{
    uint32_t i;
    text(out, "{\"entities\":"); entities_json(out, session);
    text(out, ",\"armedBarrelIds\":[],\"activeSwitchColors\":[");
    if (!initial) for (i = 0U; i < session->active_color_count; ++i) {
        if (i) text(out, ",");
        string_value(out, color_name(session->active_colors[i]),
                     strlen(color_name(session->active_colors[i])));
    }
    text(out, "],\"openDoorCoordinates\":[");
    if (!initial) for (i = 0U; i < session->open_door_count; ++i) {
        if (i) text(out, ",");
        coordinate_json(out, session->open_doors[i]);
    }
    text(out, "],\"outcome\":\"");
    text(out, initial ? "ongoing" : outcome_name(session->outcome));
    text(out, "\"}");
}

static void cell_json(builder* out, const game_rules_cell* cell)
{
    text(out, "{\"coordinate\":"); coordinate_json(out, cell->coordinate);
    if (cell->kind == GAME_RULES_CELL_FLAT) {
        text(out, ",\"type\":\"flat\",\"elevation\":"); number_i32(out, cell->elevation);
    } else {
        text(out, ",\"type\":\"ramp\",\"lowDirection\":\"");
        text(out, direction_name(cell->low_direction));
        text(out, "\",\"lowElevation\":"); number_i32(out, cell->elevation);
    }
    text(out, "}");
}

static void fixture_json(builder* out, const game_rules_fixture* fixture)
{
    text(out, "{\"coordinate\":"); coordinate_json(out, fixture->coordinate);
    if (fixture->kind == GAME_RULES_FIXTURE_SWITCH) text(out, ",\"type\":\"switch\",\"color\":\"");
    else if (fixture->kind == GAME_RULES_FIXTURE_DOOR) text(out, ",\"type\":\"door\",\"color\":\"");
    else { text(out, ",\"type\":\"exit\"}"); return; }
    text(out, color_name(fixture->color)); text(out, "\"}");
}

static void state_json(builder* out, const game_rules_session* session)
{
    uint32_t i;
    if (session == NULL || !session->has_level) { text(out, "null"); return; }
    text(out, "{\"coordinateSystem\":{\"origin\":"); coordinate_json(out, session->coordinates.origin);
    text(out, ",\"positiveX\":\"");
    text(out, session->coordinates.positive_x == GAME_RULES_HORIZONTAL_EAST ? "east" : "west");
    text(out, "\",\"positiveY\":\"");
    text(out, session->coordinates.positive_y == GAME_RULES_VERTICAL_NORTH ? "north" : "south");
    text(out, "\"},\"width\":"); number_u64(out, session->width);
    text(out, ",\"height\":"); number_u64(out, session->height);
    text(out, ",\"cells\":[");
    for (i = 0U; i < session->cell_count; ++i) { if (i) text(out, ","); cell_json(out, &session->cells[i]); }
    text(out, "],\"fixtures\":[");
    for (i = 0U; i < session->fixture_count; ++i) { if (i) text(out, ","); fixture_json(out, &session->fixtures[i]); }
    text(out, "],\"entities\":"); entities_json(out, session);
    text(out, ",\"armedBarrelIds\":[],\"activeSwitchColors\":[");
    for (i = 0U; i < session->active_color_count; ++i) {
        if (i) text(out, ",");
        string_value(out, color_name(session->active_colors[i]), strlen(color_name(session->active_colors[i])));
    }
    text(out, "],\"openDoorCoordinates\":[");
    for (i = 0U; i < session->open_door_count; ++i) { if (i) text(out, ","); coordinate_json(out, session->open_doors[i]); }
    text(out, "],\"outcome\":\""); text(out, outcome_name(session->outcome)); text(out, "\"}");
}

static void event_json(builder* out, const game_rules_event* event)
{
    if (event->kind == GAME_RULES_EVENT_LEVEL_WON) text(out, "{\"type\":\"levelWon\"}");
    else if (event->kind == GAME_RULES_EVENT_SWITCH_CHANGED) {
        text(out, "{\"type\":\"switchChanged\",\"color\":\""); text(out, color_name(event->color));
        text(out, "\",\"active\":true}");
    } else if (event->kind == GAME_RULES_EVENT_DOOR_OPENED) {
        text(out, "{\"type\":\"doorOpened\",\"coordinate\":"); coordinate_json(out, event->coordinate);
        text(out, ",\"color\":\""); text(out, color_name(event->color)); text(out, "\"}");
    }
}

static void ticks_json(builder* out, const game_rules_session* session)
{
    uint32_t i;
    if (!session->initial_event_count) { text(out, "[]"); return; }
    text(out, "[{\"index\":0,\"events\":[");
    for (i = 0U; i < session->initial_event_count; ++i) { if (i) text(out, ","); event_json(out, &session->initial_events[i]); }
    text(out, "],\"stateAfter\":"); resolved_json(out, session, 0); text(out, "}]");
}

static void errors_json(builder* out, const game_rules_validation_error* errors, uint32_t count)
{
    uint32_t i;
    text(out, "[");
    for (i = 0U; i < count; ++i) {
        if (i) text(out, ",");
        text(out, "{\"code\":\""); text(out, game_rules_c_validation_error_name(errors[i].code));
        text(out, "\",\"coordinate\":"); coordinate_json(out, errors[i].coordinate);
        text(out, ",\"entityId\":\""); number_u64(out, errors[i].entity_id); text(out, "\"}");
    }
    text(out, "]");
}

char* game_rules_c_stage02_get_state(game_rules_engine* engine)
{
    builder out = {0};
    out.allocator = engine->allocator;
    text(&out, "{\"apiVersion\":1,\"operation\":\"getState\",\"status\":\"");
    text(&out, engine->session && engine->session->has_level ? "ok" : "no_level");
    text(&out, "\",\"state\":"); state_json(&out, engine->session); text(&out, "}");
    return finish(&out);
}

char* game_rules_c_stage02_load_json(game_rules_engine* engine,
                                     const char* json,
                                     uint32_t length)
{
    game_rules_c_decode_result decoded;
    game_rules_session* replacement = NULL;
    builder out = {0};
    char* response;
    game_rules_c_decode_level_json(json, length, &engine->allocator, &decoded);
    if (decoded.status == GAME_RULES_C_DECODE_ALLOCATION_FAILED) {
        game_rules_c_decode_result_destroy(&decoded);
        return NULL;
    }
    if (decoded.status == GAME_RULES_C_DECODE_OK) {
        replacement = build_session(&engine->allocator, &decoded.level.view);
        if (replacement == NULL) { game_rules_c_decode_result_destroy(&decoded); return NULL; }
    }
    out.allocator = engine->allocator;
    text(&out, "{\"apiVersion\":1,\"operation\":\"loadLevel\",\"status\":\"");
    if (decoded.status == GAME_RULES_C_DECODE_JSON_ERROR) {
        text(&out, "invalid_json\",\"error\":{\"code\":\"");
        text(&out, game_rules_c_json_error_name(decoded.json_error.code));
        text(&out, "\",\"byteOffset\":"); number_u64(&out, decoded.json_error.byte_offset);
        text(&out, ",\"path\":");
        string_value(&out, decoded.json_error.path ? decoded.json_error.path : "",
                     decoded.json_error.path_length);
        text(&out, "},\"state\":"); state_json(&out, engine->session);
    } else if (decoded.status == GAME_RULES_C_DECODE_INVALID_LEVEL) {
        text(&out, "invalid_level\",\"errors\":");
        errors_json(&out, decoded.validation.errors, decoded.validation.count);
        text(&out, ",\"state\":"); state_json(&out, engine->session);
    } else {
        text(&out, "loaded\",\"initialState\":"); resolved_json(&out, replacement, 1);
        text(&out, ",\"ticks\":"); ticks_json(&out, replacement);
        text(&out, ",\"state\":"); state_json(&out, replacement);
        text(&out, ",\"outcome\":\""); text(&out, outcome_name(replacement->outcome)); text(&out, "\"");
    }
    text(&out, "}");
    response = finish(&out);
    if (response != NULL && replacement != NULL) {
        game_rules_session* old = engine->session;
        engine->session = replacement;
        replacement = NULL;
        game_rules_c_destroy_session(old);
    }
    game_rules_c_destroy_session(replacement);
    game_rules_c_decode_result_destroy(&decoded);
    return response;
}

static void copy_resolved(layout* arena,
                          const game_rules_session* session,
                          int initial,
                          game_rules_resolved_state* output)
{
    game_rules_entity* entities = (game_rules_entity*)take(
        arena, session->entity_count, sizeof(game_rules_entity), _Alignof(game_rules_entity));
    uint32_t color_count = initial ? 0U : session->active_color_count;
    uint32_t door_count = initial ? 0U : session->open_door_count;
    uint32_t* colors = (uint32_t*)take(arena, color_count, sizeof(uint32_t), _Alignof(uint32_t));
    game_rules_coordinate* doors = (game_rules_coordinate*)take(
        arena, door_count, sizeof(game_rules_coordinate), _Alignof(game_rules_coordinate));
    memset(output, 0, sizeof(*output));
    if (arena->base && !arena->failed) {
        if (session->entity_count) memcpy(entities, session->entities,
                                          session->entity_count * sizeof(*entities));
        if (color_count) memcpy(colors, session->active_colors, color_count * sizeof(*colors));
        if (door_count) memcpy(doors, session->open_doors, door_count * sizeof(*doors));
    }
    output->entities = entities;
    output->entity_count = session->entity_count;
    output->active_switch_colors = colors;
    output->active_switch_color_count = color_count;
    output->open_doors = doors;
    output->open_door_count = door_count;
    output->outcome = initial ? GAME_RULES_OUTCOME_ONGOING : session->outcome;
}

static void copy_snapshot(layout* arena,
                          const game_rules_session* session,
                          game_rules_snapshot* output)
{
    game_rules_cell* cells = (game_rules_cell*)take(
        arena, session->cell_count, sizeof(game_rules_cell), _Alignof(game_rules_cell));
    game_rules_fixture* fixtures = (game_rules_fixture*)take(
        arena, session->fixture_count, sizeof(game_rules_fixture), _Alignof(game_rules_fixture));
    memset(output, 0, sizeof(*output));
    if (arena->base && !arena->failed) {
        if (session->cell_count) memcpy(cells, session->cells,
                                        session->cell_count * sizeof(*cells));
        if (session->fixture_count) memcpy(fixtures, session->fixtures,
                                           session->fixture_count * sizeof(*fixtures));
    }
    output->level.coordinates = session->coordinates;
    output->level.width = session->width;
    output->level.height = session->height;
    output->level.cells = cells;
    output->level.cell_count = session->cell_count;
    output->level.fixtures = fixtures;
    output->level.fixture_count = session->fixture_count;
    copy_resolved(arena, session, 0, &output->resolved);
}

static void load_graph(layout* arena,
                       const game_rules_session* current,
                       const game_rules_session* accepted,
                       const game_rules_validation_error* errors,
                       uint32_t error_count,
                       game_rules_load_result* output)
{
    game_rules_validation_error* copied = (game_rules_validation_error*)take(
        arena, error_count, sizeof(game_rules_validation_error),
        _Alignof(game_rules_validation_error));
    output->errors = copied;
    output->error_count = error_count;
    if (arena->base && error_count && !arena->failed) {
        memcpy(copied, errors, error_count * sizeof(*copied));
    }
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
    copy_resolved(arena, accepted, 1, &output->initial_state);
    if (accepted->initial_event_count) {
        game_rules_tick measured_tick;
        game_rules_tick* tick = (game_rules_tick*)take(
            arena, 1U, sizeof(game_rules_tick), _Alignof(game_rules_tick));
        game_rules_event* events = (game_rules_event*)take(
            arena, accepted->initial_event_count, sizeof(game_rules_event),
            _Alignof(game_rules_event));
        game_rules_tick* target = arena->base ? tick : &measured_tick;
        memset(target, 0, sizeof(*target));
        target->events = events;
        target->event_count = accepted->initial_event_count;
        if (arena->base && !arena->failed) {
            memcpy(events, accepted->initial_events,
                   accepted->initial_event_count * sizeof(*events));
        }
        copy_resolved(arena, accepted, 0, &target->state_after);
        output->ticks = tick;
        output->tick_count = 1U;
    }
    output->has_final_state = 1U;
    copy_resolved(arena, accepted, 0, &output->final_state);
    output->has_state = 1U;
    copy_snapshot(arena, accepted, &output->state);
    output->has_outcome = 1U;
    output->outcome = accepted->outcome;
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
    if ((level->cell_count && level->cells == NULL) ||
        (level->fixture_count && level->fixtures == NULL) ||
        (level->entity_count && level->entities == NULL) ||
        level->coordinates.positive_x > GAME_RULES_HORIZONTAL_WEST ||
        level->coordinates.positive_y > GAME_RULES_VERTICAL_SOUTH) return 0;
    return 1;
}

static int typed_members_valid(const game_rules_level_definition* level)
{
    uint32_t i;
    for (i = 0U; i < level->cell_count; ++i) {
        if (level->cells[i].kind > GAME_RULES_CELL_RAMP ||
            (level->cells[i].kind == GAME_RULES_CELL_RAMP &&
             level->cells[i].low_direction > GAME_RULES_DIRECTION_WEST)) return 0;
    }
    for (i = 0U; i < level->fixture_count; ++i) {
        if (level->fixtures[i].kind > GAME_RULES_FIXTURE_EXIT ||
            (level->fixtures[i].kind != GAME_RULES_FIXTURE_EXIT &&
             level->fixtures[i].color > GAME_RULES_COLOR_YELLOW)) return 0;
    }
    for (i = 0U; i < level->entity_count; ++i) {
        if (level->entities[i].kind > GAME_RULES_ENTITY_BARREL) return 0;
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

uint32_t game_rules_c_stage02_get_state_data(const game_rules_engine* engine,
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

uint32_t game_rules_c_stage02_load_data(game_rules_engine* engine,
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
    if (!validation.count) {
        replacement = build_session(&engine->allocator, &view);
        if (replacement == NULL) {
            game_rules_c_validation_result_destroy(&validation);
            return GAME_RULES_CALL_ALLOCATION_FAILED;
        }
    }
    call = make_load_result(engine,
        engine->session && engine->session->has_level ? engine->session : NULL,
        replacement, validation.errors, validation.count, result);
    if (call == GAME_RULES_CALL_OK && replacement != NULL) {
        game_rules_session* old = engine->session;
        engine->session = replacement;
        replacement = NULL;
        game_rules_c_destroy_session(old);
    }
    game_rules_c_destroy_session(replacement);
    game_rules_c_validation_result_destroy(&validation);
    return call;
}
