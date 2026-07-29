#include "c_api_internal.h"
#include "level_json_yyjson.h"

#include <inttypes.h>
#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define GAME_RULES_C_MAX_JSON_BYTES (16U * 1024U * 1024U)
#define GAME_RULES_C_MAX_JSON_DEPTH 32U

typedef struct decoder {
    const game_rules_c_allocator* allocator;
    game_rules_c_json_error error;
    uint32_t failed;
    uint32_t allocation_failed;
} decoder;

static int name_equals(yyjson_val* key, const char* expected)
{
    const size_t length = yyjson_get_len(key);
    return strlen(expected) == length && memcmp(yyjson_get_str(key), expected, length) == 0;
}

static int string_equals(yyjson_val* value, const char* expected)
{
    return yyjson_is_str(value) && name_equals(value, expected);
}

static void* yyjson_allocate(void* context, size_t size)
{
    return game_rules_c_allocate_owned((const game_rules_c_allocator*)context, size);
}

static void* yyjson_reallocate(void* context,
                               void* pointer,
                               size_t old_size,
                               size_t new_size)
{
    void* replacement;
    if (pointer == NULL) {
        return yyjson_allocate(context, new_size);
    }
    if (new_size == 0U) {
        game_rules_c_deallocate_owned(pointer);
        return NULL;
    }
    replacement = yyjson_allocate(context, new_size);
    if (replacement == NULL) {
        return NULL;
    }
    memcpy(replacement, pointer, old_size < new_size ? old_size : new_size);
    game_rules_c_deallocate_owned(pointer);
    return replacement;
}

static void yyjson_deallocate(void* context, void* pointer)
{
    (void)context;
    game_rules_c_deallocate_owned(pointer);
}

static int set_error_path(decoder* state, uint32_t code, const char* path, size_t length)
{
    char* copy = NULL;
    if (state->failed != 0U || state->allocation_failed != 0U) {
        return 0;
    }
    if (length != 0U) {
        if (length == SIZE_MAX) {
            state->allocation_failed = 1U;
            return 0;
        }
        copy = (char*)game_rules_c_allocate_owned(state->allocator, length + 1U);
        if (copy == NULL) {
            state->allocation_failed = 1U;
            return 0;
        }
        memcpy(copy, path, length);
        copy[length] = '\0';
    }
    state->error.code = code;
    state->error.byte_offset = 0U;
    state->error.path = copy;
    state->error.path_length = length;
    state->failed = 1U;
    return 0;
}

static int fail_known(decoder* state, uint32_t code, const char* path)
{
    return set_error_path(state, code, path, strlen(path));
}

static int fail_member(decoder* state,
                       uint32_t code,
                       const char* base,
                       yyjson_val* key)
{
    const char* name = yyjson_get_str(key);
    const size_t name_length = yyjson_get_len(key);
    const size_t base_length = strlen(base);
    size_t escaped_length = 0U;
    size_t index;
    size_t output_index;
    char* path;
    for (index = 0U; index < name_length; ++index) {
        const size_t add = name[index] == '~' || name[index] == '/' ? 2U : 1U;
        if (escaped_length > SIZE_MAX - add) {
            state->allocation_failed = 1U;
            return 0;
        }
        escaped_length += add;
    }
    if (base_length > SIZE_MAX - escaped_length - 2U) {
        state->allocation_failed = 1U;
        return 0;
    }
    path = (char*)game_rules_c_allocate_owned(
        state->allocator, base_length + 1U + escaped_length + 1U);
    if (path == NULL) {
        state->allocation_failed = 1U;
        return 0;
    }
    memcpy(path, base, base_length);
    output_index = base_length;
    path[output_index++] = '/';
    for (index = 0U; index < name_length; ++index) {
        if (name[index] == '~') {
            path[output_index++] = '~';
            path[output_index++] = '0';
        } else if (name[index] == '/') {
            path[output_index++] = '~';
            path[output_index++] = '1';
        } else {
            path[output_index++] = name[index];
        }
    }
    path[output_index] = '\0';
    state->error.code = code;
    state->error.byte_offset = 0U;
    state->error.path = path;
    state->error.path_length = output_index;
    state->failed = 1U;
    return 0;
}

static int member_index(yyjson_val* key, const char* const* allowed, size_t allowed_count)
{
    size_t index;
    for (index = 0U; index < allowed_count; ++index) {
        if (name_equals(key, allowed[index])) {
            return (int)index;
        }
    }
    return -1;
}

static int members_are(decoder* state,
                       yyjson_val* object,
                       const char* const* allowed,
                       size_t allowed_count,
                       const char* path)
{
    uint32_t seen = 0U;
    yyjson_obj_iter iterator = yyjson_obj_iter_with(object);
    yyjson_val* key;
    while ((key = yyjson_obj_iter_next(&iterator)) != NULL) {
        const int found = member_index(key, allowed, allowed_count);
        uint32_t bit;
        if (found < 0) {
            return fail_member(state, GAME_RULES_C_JSON_UNKNOWN_MEMBER, path, key);
        }
        bit = UINT32_C(1) << (uint32_t)found;
        if ((seen & bit) != 0U) {
            return fail_member(state, GAME_RULES_C_JSON_DUPLICATE_MEMBER, path, key);
        }
        seen |= bit;
    }
    return 1;
}

static yyjson_val* required(decoder* state,
                            yyjson_val* object,
                            const char* name,
                            const char* path)
{
    yyjson_obj_iter iterator = yyjson_obj_iter_with(object);
    yyjson_val* key;
    while ((key = yyjson_obj_iter_next(&iterator)) != NULL) {
        if (name_equals(key, name)) {
            return yyjson_obj_iter_get_val(key);
        }
    }
    {
        char buffer[160];
        const int written = snprintf(buffer, sizeof(buffer), "%s/%s", path, name);
        if (written < 0 || (size_t)written >= sizeof(buffer)) {
            state->allocation_failed = 1U;
            return NULL;
        }
        fail_known(state, GAME_RULES_C_JSON_MISSING_MEMBER, buffer);
    }
    return NULL;
}

static int expect_object(decoder* state, yyjson_val* value, const char* path)
{
    return yyjson_is_obj(value) || fail_known(state, GAME_RULES_C_JSON_INVALID_MEMBER_TYPE, path);
}

static int expect_array(decoder* state, yyjson_val* value, const char* path)
{
    return yyjson_is_arr(value) || fail_known(state, GAME_RULES_C_JSON_INVALID_MEMBER_TYPE, path);
}

static int expect_string(decoder* state, yyjson_val* value, const char* path)
{
    return yyjson_is_str(value) || fail_known(state, GAME_RULES_C_JSON_INVALID_MEMBER_TYPE, path);
}

static int read_signed(decoder* state, yyjson_val* value, int32_t* output, const char* path)
{
    if (yyjson_is_sint(value)) {
        const int64_t decoded = yyjson_get_sint(value);
        if (decoded < INT32_MIN || decoded > INT32_MAX) {
            return fail_known(state, GAME_RULES_C_JSON_INTEGER_OUT_OF_RANGE, path);
        }
        *output = (int32_t)decoded;
        return 1;
    }
    if (yyjson_is_uint(value)) {
        const uint64_t decoded = yyjson_get_uint(value);
        if (decoded > INT32_MAX) {
            return fail_known(state, GAME_RULES_C_JSON_INTEGER_OUT_OF_RANGE, path);
        }
        *output = (int32_t)decoded;
        return 1;
    }
    return fail_known(state,
                      yyjson_is_num(value) ? GAME_RULES_C_JSON_INTEGER_OUT_OF_RANGE
                                           : GAME_RULES_C_JSON_INVALID_MEMBER_TYPE,
                      path);
}

static int read_unsigned(decoder* state, yyjson_val* value, uint32_t* output, const char* path)
{
    uint64_t decoded;
    if (!yyjson_is_uint(value)) {
        return fail_known(state,
                          yyjson_is_num(value) ? GAME_RULES_C_JSON_INTEGER_OUT_OF_RANGE
                                               : GAME_RULES_C_JSON_INVALID_MEMBER_TYPE,
                          path);
    }
    decoded = yyjson_get_uint(value);
    if (decoded > UINT32_MAX) {
        return fail_known(state, GAME_RULES_C_JSON_INTEGER_OUT_OF_RANGE, path);
    }
    *output = (uint32_t)decoded;
    return 1;
}

static int read_entity_id(decoder* state, yyjson_val* value, uint64_t* output, const char* path)
{
    const char* string;
    size_t length;
    size_t index;
    uint64_t decoded = 0U;
    if (!expect_string(state, value, path)) {
        return 0;
    }
    string = yyjson_get_str(value);
    length = yyjson_get_len(value);
    if (length == 0U || string[0] == '0' || string[0] == '-') {
        return fail_known(state, GAME_RULES_C_JSON_INVALID_ENTITY_ID, path);
    }
    for (index = 0U; index < length; ++index) {
        const uint32_t digit = (uint32_t)(unsigned char)string[index] - (uint32_t)'0';
        if (digit > 9U || decoded > (UINT64_MAX - digit) / 10U) {
            return fail_known(state, GAME_RULES_C_JSON_INVALID_ENTITY_ID, path);
        }
        decoded = decoded * 10U + digit;
    }
    *output = decoded;
    return 1;
}

static int read_enum(decoder* state,
                     yyjson_val* value,
                     const char* const* names,
                     size_t count,
                     uint32_t* output,
                     const char* path)
{
    size_t index;
    if (!expect_string(state, value, path)) {
        return 0;
    }
    for (index = 0U; index < count; ++index) {
        if (string_equals(value, names[index])) {
            *output = (uint32_t)index;
            return 1;
        }
    }
    return fail_known(state, GAME_RULES_C_JSON_INVALID_ENUM_VALUE, path);
}

static int read_coordinate(decoder* state,
                           yyjson_val* value,
                           game_rules_coordinate* output,
                           const char* path)
{
    static const char* const members[] = {"x", "y"};
    char x_path[160];
    char y_path[160];
    yyjson_val* x;
    yyjson_val* y;
    if (!expect_object(state, value, path) ||
        !members_are(state, value, members, 2U, path)) {
        return 0;
    }
    x = required(state, value, "x", path);
    y = required(state, value, "y", path);
    if (state->failed != 0U || state->allocation_failed != 0U) {
        return 0;
    }
    if (snprintf(x_path, sizeof(x_path), "%s/x", path) < 0 ||
        snprintf(y_path, sizeof(y_path), "%s/y", path) < 0) {
        state->allocation_failed = 1U;
        return 0;
    }
    return read_signed(state, x, &output->x, x_path) &&
           read_signed(state, y, &output->y, y_path);
}

static int read_coordinate_system(decoder* state,
                                  yyjson_val* value,
                                  game_rules_coordinate_system* output)
{
    static const char* const members[] = {"origin", "positiveX", "positiveY"};
    static const char* const horizontal[] = {"east", "west"};
    static const char* const vertical[] = {"north", "south"};
    yyjson_val* origin;
    yyjson_val* positive_x;
    yyjson_val* positive_y;
    const char* path = "/coordinateSystem";
    if (!expect_object(state, value, path) ||
        !members_are(state, value, members, 3U, path)) {
        return 0;
    }
    origin = required(state, value, "origin", path);
    positive_x = required(state, value, "positiveX", path);
    positive_y = required(state, value, "positiveY", path);
    if (state->failed != 0U || state->allocation_failed != 0U) {
        return 0;
    }
    return read_coordinate(state, origin, &output->origin, "/coordinateSystem/origin") &&
           read_enum(state, positive_x, horizontal, 2U, &output->positive_x,
                     "/coordinateSystem/positiveX") &&
           read_enum(state, positive_y, vertical, 2U, &output->positive_y,
                     "/coordinateSystem/positiveY");
}

static int allocate_decoded_arrays(decoder* state,
                                   game_rules_c_owned_level* output,
                                   size_t cell_count,
                                   size_t fixture_count,
                                   size_t entity_count)
{
    if (cell_count > UINT32_MAX || fixture_count > UINT32_MAX || entity_count > UINT32_MAX) {
        state->allocation_failed = 1U;
        return 0;
    }
    if (cell_count != 0U) {
        if (cell_count > SIZE_MAX / sizeof(*output->cells)) {
            state->allocation_failed = 1U;
            return 0;
        }
        output->cells = (game_rules_cell*)game_rules_c_allocate_owned(
            state->allocator, cell_count * sizeof(*output->cells));
        if (output->cells == NULL) {
            state->allocation_failed = 1U;
            return 0;
        }
    }
    if (fixture_count != 0U) {
        if (fixture_count > SIZE_MAX / sizeof(*output->fixtures)) {
            state->allocation_failed = 1U;
            return 0;
        }
        output->fixtures = (game_rules_fixture*)game_rules_c_allocate_owned(
            state->allocator, fixture_count * sizeof(*output->fixtures));
        if (output->fixtures == NULL) {
            state->allocation_failed = 1U;
            return 0;
        }
    }
    if (entity_count != 0U) {
        if (entity_count > SIZE_MAX / sizeof(*output->entities)) {
            state->allocation_failed = 1U;
            return 0;
        }
        output->entities = (game_rules_entity*)game_rules_c_allocate_owned(
            state->allocator, entity_count * sizeof(*output->entities));
        if (output->entities == NULL) {
            state->allocation_failed = 1U;
            return 0;
        }
    }
    output->view.cells = output->cells;
    output->view.cell_count = (uint32_t)cell_count;
    output->view.fixtures = output->fixtures;
    output->view.fixture_count = (uint32_t)fixture_count;
    output->view.entities = output->entities;
    output->view.entity_count = (uint32_t)entity_count;
    return 1;
}

static int read_cells(decoder* state, yyjson_val* value, game_rules_c_owned_level* output)
{
    static const char* const broad_members[] = {
        "coordinate", "type", "elevation", "lowDirection", "lowElevation"};
    static const char* const flat_members[] = {"coordinate", "type", "elevation"};
    static const char* const ramp_members[] = {
        "coordinate", "type", "lowDirection", "lowElevation"};
    static const char* const directions[] = {"north", "east", "south", "west"};
    yyjson_arr_iter iterator;
    yyjson_val* item;
    size_t index = 0U;
    if (!expect_array(state, value, "/cells")) {
        return 0;
    }
    iterator = yyjson_arr_iter_with(value);
    while ((item = yyjson_arr_iter_next(&iterator)) != NULL) {
        char path[64];
        char coordinate_path[96];
        char type_path[96];
        yyjson_val* coordinate;
        yyjson_val* type;
        game_rules_cell* cell = &output->cells[index];
        memset(cell, 0, sizeof(*cell));
        snprintf(path, sizeof(path), "/cells/%zu", index);
        snprintf(coordinate_path, sizeof(coordinate_path), "%s/coordinate", path);
        snprintf(type_path, sizeof(type_path), "%s/type", path);
        if (!expect_object(state, item, path) ||
            !members_are(state, item, broad_members, 5U, path)) {
            return 0;
        }
        coordinate = required(state, item, "coordinate", path);
        type = required(state, item, "type", path);
        if (state->failed != 0U || state->allocation_failed != 0U ||
            !read_coordinate(state, coordinate, &cell->coordinate, coordinate_path) ||
            !expect_string(state, type, type_path)) {
            return 0;
        }
        if (string_equals(type, "flat")) {
            char elevation_path[96];
            yyjson_val* elevation;
            if (!members_are(state, item, flat_members, 3U, path)) {
                return 0;
            }
            elevation = required(state, item, "elevation", path);
            snprintf(elevation_path, sizeof(elevation_path), "%s/elevation", path);
            cell->kind = GAME_RULES_CELL_FLAT;
            if (state->failed != 0U ||
                !read_signed(state, elevation, &cell->elevation, elevation_path)) {
                return 0;
            }
        } else if (string_equals(type, "ramp")) {
            char direction_path[96];
            char elevation_path[96];
            yyjson_val* direction;
            yyjson_val* elevation;
            if (!members_are(state, item, ramp_members, 4U, path)) {
                return 0;
            }
            direction = required(state, item, "lowDirection", path);
            elevation = required(state, item, "lowElevation", path);
            snprintf(direction_path, sizeof(direction_path), "%s/lowDirection", path);
            snprintf(elevation_path, sizeof(elevation_path), "%s/lowElevation", path);
            cell->kind = GAME_RULES_CELL_RAMP;
            if (state->failed != 0U ||
                !read_enum(state, direction, directions, 4U, &cell->low_direction,
                           direction_path) ||
                !read_signed(state, elevation, &cell->elevation, elevation_path)) {
                return 0;
            }
        } else {
            return fail_known(state, GAME_RULES_C_JSON_INVALID_ENUM_VALUE, type_path);
        }
        ++index;
    }
    return 1;
}

static int read_fixtures(decoder* state, yyjson_val* value, game_rules_c_owned_level* output)
{
    static const char* const broad_members[] = {"coordinate", "type", "color"};
    static const char* const exit_members[] = {"coordinate", "type"};
    static const char* const colors[] = {"red", "green", "blue", "yellow"};
    yyjson_arr_iter iterator;
    yyjson_val* item;
    size_t index = 0U;
    if (!expect_array(state, value, "/fixtures")) {
        return 0;
    }
    iterator = yyjson_arr_iter_with(value);
    while ((item = yyjson_arr_iter_next(&iterator)) != NULL) {
        char path[64];
        char coordinate_path[96];
        char type_path[96];
        yyjson_val* coordinate;
        yyjson_val* type;
        game_rules_fixture* fixture = &output->fixtures[index];
        memset(fixture, 0, sizeof(*fixture));
        snprintf(path, sizeof(path), "/fixtures/%zu", index);
        snprintf(coordinate_path, sizeof(coordinate_path), "%s/coordinate", path);
        snprintf(type_path, sizeof(type_path), "%s/type", path);
        if (!expect_object(state, item, path) ||
            !members_are(state, item, broad_members, 3U, path)) {
            return 0;
        }
        coordinate = required(state, item, "coordinate", path);
        type = required(state, item, "type", path);
        if (state->failed != 0U ||
            !read_coordinate(state, coordinate, &fixture->coordinate, coordinate_path) ||
            !expect_string(state, type, type_path)) {
            return 0;
        }
        if (string_equals(type, "exit")) {
            if (!members_are(state, item, exit_members, 2U, path)) {
                return 0;
            }
            fixture->kind = GAME_RULES_FIXTURE_EXIT;
        } else if (string_equals(type, "switch") || string_equals(type, "door")) {
            char color_path[96];
            yyjson_val* color = required(state, item, "color", path);
            snprintf(color_path, sizeof(color_path), "%s/color", path);
            fixture->kind = string_equals(type, "switch") ? GAME_RULES_FIXTURE_SWITCH
                                                           : GAME_RULES_FIXTURE_DOOR;
            if (state->failed != 0U ||
                !read_enum(state, color, colors, 4U, &fixture->color, color_path)) {
                return 0;
            }
        } else {
            return fail_known(state, GAME_RULES_C_JSON_INVALID_ENUM_VALUE, type_path);
        }
        ++index;
    }
    return 1;
}

static int read_entities(decoder* state, yyjson_val* value, game_rules_c_owned_level* output)
{
    static const char* const members[] = {"id", "type", "coordinate", "bottomHalfSteps"};
    static const char* const kinds[] = {"player", "box", "barrel"};
    yyjson_arr_iter iterator;
    yyjson_val* item;
    size_t index = 0U;
    if (!expect_array(state, value, "/entities")) {
        return 0;
    }
    iterator = yyjson_arr_iter_with(value);
    while ((item = yyjson_arr_iter_next(&iterator)) != NULL) {
        char path[64];
        char id_path[96];
        char type_path[96];
        char coordinate_path[96];
        char bottom_path[96];
        yyjson_val* id;
        yyjson_val* type;
        yyjson_val* coordinate;
        yyjson_val* bottom;
        game_rules_entity* entity = &output->entities[index];
        memset(entity, 0, sizeof(*entity));
        snprintf(path, sizeof(path), "/entities/%zu", index);
        snprintf(id_path, sizeof(id_path), "%s/id", path);
        snprintf(type_path, sizeof(type_path), "%s/type", path);
        snprintf(coordinate_path, sizeof(coordinate_path), "%s/coordinate", path);
        snprintf(bottom_path, sizeof(bottom_path), "%s/bottomHalfSteps", path);
        if (!expect_object(state, item, path) ||
            !members_are(state, item, members, 4U, path)) {
            return 0;
        }
        id = required(state, item, "id", path);
        type = required(state, item, "type", path);
        coordinate = required(state, item, "coordinate", path);
        bottom = required(state, item, "bottomHalfSteps", path);
        if (state->failed != 0U ||
            !read_entity_id(state, id, &entity->id, id_path) ||
            !read_enum(state, type, kinds, 3U, &entity->kind, type_path) ||
            !read_coordinate(state, coordinate, &entity->coordinate, coordinate_path) ||
            !read_signed(state, bottom, &entity->bottom_half_steps, bottom_path)) {
            return 0;
        }
        ++index;
    }
    return 1;
}

static int exceeds_nesting_depth(yyjson_val* value, uint32_t depth, uint32_t maximum)
{
    if (!yyjson_is_arr(value) && !yyjson_is_obj(value)) {
        return 0;
    }
    if (depth >= maximum) {
        return 1;
    }
    if (yyjson_is_arr(value)) {
        yyjson_arr_iter iterator = yyjson_arr_iter_with(value);
        yyjson_val* child;
        while ((child = yyjson_arr_iter_next(&iterator)) != NULL) {
            if (exceeds_nesting_depth(child, depth + 1U, maximum)) {
                return 1;
            }
        }
    } else {
        yyjson_obj_iter iterator = yyjson_obj_iter_with(value);
        yyjson_val* key;
        while ((key = yyjson_obj_iter_next(&iterator)) != NULL) {
            if (exceeds_nesting_depth(yyjson_obj_iter_get_val(key), depth + 1U, maximum)) {
                return 1;
            }
        }
    }
    return 0;
}

static int decode_root(decoder* state, yyjson_val* root, game_rules_c_owned_level* output)
{
    static const char* const members[] = {
        "format", "version", "coordinateSystem", "width", "height",
        "cells", "fixtures", "entities"};
    yyjson_val* format;
    yyjson_val* version;
    yyjson_val* coordinates;
    yyjson_val* width;
    yyjson_val* height;
    yyjson_val* cells;
    yyjson_val* fixtures;
    yyjson_val* entities;
    uint32_t decoded_version = 0U;
    if (!yyjson_is_obj(root)) {
        return fail_known(state, GAME_RULES_C_JSON_ROOT_NOT_OBJECT, "");
    }
    if (!members_are(state, root, members, 8U, "")) {
        return 0;
    }
    format = required(state, root, "format", "");
    version = required(state, root, "version", "");
    coordinates = required(state, root, "coordinateSystem", "");
    width = required(state, root, "width", "");
    height = required(state, root, "height", "");
    cells = required(state, root, "cells", "");
    fixtures = required(state, root, "fixtures", "");
    entities = required(state, root, "entities", "");
    if (state->failed != 0U || state->allocation_failed != 0U) {
        return 0;
    }
    if (!expect_string(state, format, "/format")) {
        return 0;
    }
    if (!string_equals(format, "game-rules-level")) {
        return fail_known(state, GAME_RULES_C_JSON_INVALID_FORMAT, "/format");
    }
    if (!read_unsigned(state, version, &decoded_version, "/version")) {
        return 0;
    }
    if (decoded_version != 1U) {
        return fail_known(state, GAME_RULES_C_JSON_UNSUPPORTED_VERSION, "/version");
    }
    if (!read_coordinate_system(state, coordinates, &output->view.coordinates) ||
        !read_unsigned(state, width, &output->view.width, "/width") ||
        !read_unsigned(state, height, &output->view.height, "/height")) {
        return 0;
    }
    if (!expect_array(state, cells, "/cells") ||
        !expect_array(state, fixtures, "/fixtures") ||
        !expect_array(state, entities, "/entities")) {
        return 0;
    }
    if (!allocate_decoded_arrays(state, output, yyjson_arr_size(cells),
                                 yyjson_arr_size(fixtures), yyjson_arr_size(entities))) {
        return 0;
    }
    return read_cells(state, cells, output) &&
           read_fixtures(state, fixtures, output) &&
           read_entities(state, entities, output);
}

void game_rules_c_decode_level_json(const char* json,
                                    size_t json_length,
                                    const game_rules_c_allocator* allocator,
                                    game_rules_c_decode_result* result)
{
    yyjson_alc yy_allocator;
    yyjson_read_err read_error;
    yyjson_doc* document;
    decoder state;
    memset(result, 0, sizeof(*result));
    memset(&state, 0, sizeof(state));
    state.allocator = allocator;
    result->status = GAME_RULES_C_DECODE_JSON_ERROR;
    if (json_length > GAME_RULES_C_MAX_JSON_BYTES) {
        state.error.code = GAME_RULES_C_JSON_DOCUMENT_TOO_LARGE;
        state.failed = 1U;
        result->json_error = state.error;
        return;
    }
    yy_allocator.malloc = yyjson_allocate;
    yy_allocator.realloc = yyjson_reallocate;
    yy_allocator.free = yyjson_deallocate;
    yy_allocator.ctx = (void*)allocator;
    memset(&read_error, 0, sizeof(read_error));
    document = game_rules_c_yyjson_read(json, json_length, &yy_allocator, &read_error);
    if (document == NULL) {
        if (read_error.code == YYJSON_READ_ERROR_MEMORY_ALLOCATION) {
            result->status = GAME_RULES_C_DECODE_ALLOCATION_FAILED;
        } else {
            result->json_error.code = GAME_RULES_C_JSON_INVALID_JSON;
            result->json_error.byte_offset = read_error.pos;
        }
        return;
    }
    if (exceeds_nesting_depth(yyjson_doc_get_root(document), 0U,
                              GAME_RULES_C_MAX_JSON_DEPTH)) {
        result->json_error.code = GAME_RULES_C_JSON_NESTING_TOO_DEEP;
        game_rules_c_yyjson_free(document);
        return;
    }
    if (!decode_root(&state, yyjson_doc_get_root(document), &result->level)) {
        game_rules_c_yyjson_free(document);
        if (state.allocation_failed != 0U) {
            result->status = GAME_RULES_C_DECODE_ALLOCATION_FAILED;
            game_rules_c_deallocate_owned(state.error.path);
        } else {
            result->json_error = state.error;
        }
        return;
    }
    game_rules_c_yyjson_free(document);
    game_rules_c_validate_level(&result->level.view, allocator, &result->validation);
    if (result->validation.allocation_failed != 0U) {
        result->status = GAME_RULES_C_DECODE_ALLOCATION_FAILED;
        return;
    }
    if (result->validation.count != 0U) {
        result->status = GAME_RULES_C_DECODE_INVALID_LEVEL;
        return;
    }
    game_rules_c_canonicalize_level(&result->level);
    result->status = GAME_RULES_C_DECODE_OK;
}

void game_rules_c_decode_result_destroy(game_rules_c_decode_result* result)
{
    if (result == NULL) {
        return;
    }
    game_rules_c_deallocate_owned(result->json_error.path);
    game_rules_c_validation_result_destroy(&result->validation);
    game_rules_c_deallocate_owned(result->level.entities);
    game_rules_c_deallocate_owned(result->level.fixtures);
    game_rules_c_deallocate_owned(result->level.cells);
    memset(result, 0, sizeof(*result));
}

const char* game_rules_c_json_error_name(uint32_t code)
{
    static const char* const names[] = {
        "invalid_json", "document_too_large", "nesting_too_deep", "root_not_object",
        "missing_member", "unknown_member", "duplicate_member", "invalid_member_type",
        "integer_out_of_range", "invalid_enum_value", "invalid_format",
        "unsupported_version", "invalid_entity_id"
    };
    return code < sizeof(names) / sizeof(names[0]) ? names[code] : "unknown";
}
