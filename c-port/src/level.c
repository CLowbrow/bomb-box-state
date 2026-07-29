#include "c_api_internal.h"

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

typedef struct indexed_cell {
    const game_rules_cell* value;
    uint32_t index;
} indexed_cell;

typedef struct indexed_fixture {
    const game_rules_fixture* value;
    uint32_t index;
} indexed_fixture;

typedef struct indexed_entity {
    const game_rules_entity* value;
    uint32_t index;
} indexed_entity;

static int coordinate_compare(game_rules_coordinate left, game_rules_coordinate right)
{
    if (left.y != right.y) {
        return left.y < right.y ? -1 : 1;
    }
    if (left.x != right.x) {
        return left.x < right.x ? -1 : 1;
    }
    return 0;
}

static int coordinates_equal(game_rules_coordinate left, game_rules_coordinate right)
{
    return left.x == right.x && left.y == right.y;
}

static int compare_cells(const void* left, const void* right)
{
    const indexed_cell* a = (const indexed_cell*)left;
    const indexed_cell* b = (const indexed_cell*)right;
    const int coordinate = coordinate_compare(a->value->coordinate, b->value->coordinate);
    if (coordinate != 0) {
        return coordinate;
    }
    return a->index < b->index ? -1 : a->index != b->index;
}

static int compare_fixtures(const void* left, const void* right)
{
    const indexed_fixture* a = (const indexed_fixture*)left;
    const indexed_fixture* b = (const indexed_fixture*)right;
    const int coordinate = coordinate_compare(a->value->coordinate, b->value->coordinate);
    if (coordinate != 0) {
        return coordinate;
    }
    return a->index < b->index ? -1 : a->index != b->index;
}

static int compare_entities_spatial(const void* left, const void* right)
{
    const indexed_entity* a = (const indexed_entity*)left;
    const indexed_entity* b = (const indexed_entity*)right;
    int coordinate = coordinate_compare(a->value->coordinate, b->value->coordinate);
    if (coordinate != 0) {
        return coordinate;
    }
    if (a->value->bottom_half_steps != b->value->bottom_half_steps) {
        return a->value->bottom_half_steps < b->value->bottom_half_steps ? -1 : 1;
    }
    if (a->value->id != b->value->id) {
        return a->value->id < b->value->id ? -1 : 1;
    }
    return a->index < b->index ? -1 : a->index != b->index;
}

static int compare_entities_id(const void* left, const void* right)
{
    const indexed_entity* a = (const indexed_entity*)left;
    const indexed_entity* b = (const indexed_entity*)right;
    if (a->value->id != b->value->id) {
        return a->value->id < b->value->id ? -1 : 1;
    }
    return a->index < b->index ? -1 : a->index != b->index;
}

static int compare_validation_errors(const void* left, const void* right)
{
    const game_rules_validation_error* a = (const game_rules_validation_error*)left;
    const game_rules_validation_error* b = (const game_rules_validation_error*)right;
    if (a->code != b->code) {
        return a->code < b->code ? -1 : 1;
    }
    if (a->coordinate.y != b->coordinate.y) {
        return a->coordinate.y < b->coordinate.y ? -1 : 1;
    }
    if (a->coordinate.x != b->coordinate.x) {
        return a->coordinate.x < b->coordinate.x ? -1 : 1;
    }
    if (a->entity_id != b->entity_id) {
        return a->entity_id < b->entity_id ? -1 : 1;
    }
    return 0;
}

static int compare_public_cells(const void* left, const void* right)
{
    return coordinate_compare(((const game_rules_cell*)left)->coordinate,
                              ((const game_rules_cell*)right)->coordinate);
}

static int compare_public_fixtures(const void* left, const void* right)
{
    return coordinate_compare(((const game_rules_fixture*)left)->coordinate,
                              ((const game_rules_fixture*)right)->coordinate);
}

static int compare_public_entities(const void* left, const void* right)
{
    const game_rules_entity* a = (const game_rules_entity*)left;
    const game_rules_entity* b = (const game_rules_entity*)right;
    const int coordinate = coordinate_compare(a->coordinate, b->coordinate);
    if (coordinate != 0) {
        return coordinate;
    }
    if (a->bottom_half_steps != b->bottom_half_steps) {
        return a->bottom_half_steps < b->bottom_half_steps ? -1 : 1;
    }
    if (a->id != b->id) {
        return a->id < b->id ? -1 : 1;
    }
    return 0;
}

static int multiply_size(size_t count, size_t element_size, size_t* result)
{
    if (element_size != 0U && count > SIZE_MAX / element_size) {
        return 0;
    }
    *result = count * element_size;
    return 1;
}

static void* allocate_array(const game_rules_c_allocator* allocator,
                            size_t count,
                            size_t element_size)
{
    size_t size;
    if (count == 0U) {
        return NULL;
    }
    if (!multiply_size(count, element_size, &size)) {
        return NULL;
    }
    return game_rules_c_allocate_owned(allocator, size);
}

static int grow_errors(game_rules_c_validation_result* result,
                       const game_rules_c_allocator* allocator)
{
    uint32_t new_capacity;
    size_t size;
    game_rules_validation_error* replacement;
    if (result->count < result->capacity) {
        return 1;
    }
    new_capacity = result->capacity == 0U ? 16U : result->capacity * 2U;
    if (new_capacity < result->capacity ||
        !multiply_size(new_capacity, sizeof(*replacement), &size)) {
        result->allocation_failed = 1U;
        return 0;
    }
    replacement = (game_rules_validation_error*)game_rules_c_allocate_owned(allocator, size);
    if (replacement == NULL) {
        result->allocation_failed = 1U;
        return 0;
    }
    if (result->count != 0U) {
        memcpy(replacement, result->errors, result->count * sizeof(*replacement));
    }
    game_rules_c_deallocate_owned(result->errors);
    result->errors = replacement;
    result->capacity = new_capacity;
    return 1;
}

static void add_error(game_rules_c_validation_result* result,
                      const game_rules_c_allocator* allocator,
                      uint32_t code,
                      game_rules_coordinate coordinate,
                      uint64_t entity_id)
{
    if (result->allocation_failed != 0U || !grow_errors(result, allocator)) {
        return;
    }
    result->errors[result->count].code = code;
    result->errors[result->count].coordinate = coordinate;
    result->errors[result->count].entity_id = entity_id;
    ++result->count;
}

static int has_representable_extent(const game_rules_c_level_view* level)
{
    int64_t last_x;
    int64_t last_y;
    if (level->width == 0U || level->height == 0U) {
        return 0;
    }
    last_x = (int64_t)level->coordinates.origin.x + (int64_t)level->width - 1;
    last_y = (int64_t)level->coordinates.origin.y + (int64_t)level->height - 1;
    return last_x <= INT32_MAX && last_y <= INT32_MAX;
}

static int in_bounds(const game_rules_c_level_view* level,
                     game_rules_coordinate coordinate)
{
    int64_t offset_x;
    int64_t offset_y;
    if (!has_representable_extent(level)) {
        return 0;
    }
    offset_x = (int64_t)coordinate.x - level->coordinates.origin.x;
    offset_y = (int64_t)coordinate.y - level->coordinates.origin.y;
    return offset_x >= 0 && offset_y >= 0 &&
           offset_x < (int64_t)level->width && offset_y < (int64_t)level->height;
}

static int valid_direction(uint32_t direction)
{
    return direction <= GAME_RULES_DIRECTION_WEST;
}

static int opposite_direction(uint32_t direction, uint32_t* opposite)
{
    if (!valid_direction(direction)) {
        return 0;
    }
    *opposite = (direction + 2U) % 4U;
    return 1;
}

static int step_coordinate(game_rules_coordinate coordinate,
                           uint32_t direction,
                           const game_rules_coordinate_system* system,
                           game_rules_coordinate* result)
{
    int32_t dx = 0;
    int32_t dy = 0;
    int64_t x;
    int64_t y;
    switch (direction) {
    case GAME_RULES_DIRECTION_NORTH:
        dy = system->positive_y == GAME_RULES_VERTICAL_NORTH ? 1 : -1;
        break;
    case GAME_RULES_DIRECTION_EAST:
        dx = system->positive_x == GAME_RULES_HORIZONTAL_EAST ? 1 : -1;
        break;
    case GAME_RULES_DIRECTION_SOUTH:
        dy = system->positive_y == GAME_RULES_VERTICAL_SOUTH ? 1 : -1;
        break;
    case GAME_RULES_DIRECTION_WEST:
        dx = system->positive_x == GAME_RULES_HORIZONTAL_WEST ? 1 : -1;
        break;
    default: return 0;
    }
    x = (int64_t)coordinate.x + dx;
    y = (int64_t)coordinate.y + dy;
    if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX) {
        return 0;
    }
    result->x = (int32_t)x;
    result->y = (int32_t)y;
    return 1;
}

static int representable_cell_height(int32_t elevation, int ramp)
{
    const int64_t low = (int64_t)elevation * 2;
    const int64_t center = low + (ramp ? 1 : 0);
    const int64_t high = low + (ramp ? 2 : 0);
    return low >= INT32_MIN && center >= INT32_MIN && high <= INT32_MAX;
}

static const indexed_cell* find_indexed_cell(const indexed_cell* cells,
                                             uint32_t count,
                                             game_rules_coordinate coordinate)
{
    uint32_t low = 0U;
    uint32_t high = count;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2U;
        const int comparison = coordinate_compare(cells[middle].value->coordinate, coordinate);
        if (comparison < 0) {
            low = middle + 1U;
        } else {
            high = middle;
        }
    }
    if (low < count && coordinates_equal(cells[low].value->coordinate, coordinate)) {
        return &cells[low];
    }
    return NULL;
}

static const indexed_fixture* find_indexed_fixture(const indexed_fixture* fixtures,
                                                   uint32_t count,
                                                   game_rules_coordinate coordinate)
{
    uint32_t low = 0U;
    uint32_t high = count;
    while (low < high) {
        const uint32_t middle = low + (high - low) / 2U;
        const int comparison = coordinate_compare(fixtures[middle].value->coordinate, coordinate);
        if (comparison < 0) {
            low = middle + 1U;
        } else {
            high = middle;
        }
    }
    if (low < count && coordinates_equal(fixtures[low].value->coordinate, coordinate)) {
        return &fixtures[low];
    }
    return NULL;
}

static int ramp_endpoint_kind(const game_rules_cell* ramp,
                              uint32_t direction,
                              uint32_t* endpoint)
{
    uint32_t opposite;
    if (direction == ramp->low_direction) {
        *endpoint = 0U;
        return 1;
    }
    if (opposite_direction(ramp->low_direction, &opposite) && direction == opposite) {
        *endpoint = 1U;
        return 1;
    }
    return 0;
}

static int64_t ramp_endpoint_height(const game_rules_cell* ramp, uint32_t endpoint)
{
    return (int64_t)ramp->elevation * 2 + (endpoint != 0U ? 2 : 0);
}

static int endpoint_is_valid(const game_rules_cell* ramp,
                             uint32_t direction,
                             const game_rules_cell* neighbor)
{
    uint32_t source_endpoint;
    if (!ramp_endpoint_kind(ramp, direction, &source_endpoint)) {
        return 0;
    }
    if (neighbor->kind == GAME_RULES_CELL_FLAT) {
        return (int64_t)neighbor->elevation * 2 ==
               ramp_endpoint_height(ramp, source_endpoint);
    }
    if (neighbor->kind == GAME_RULES_CELL_RAMP) {
        uint32_t opposite;
        uint32_t destination_endpoint;
        if (!opposite_direction(direction, &opposite) ||
            !ramp_endpoint_kind(neighbor, opposite, &destination_endpoint)) {
            return 0;
        }
        return source_endpoint != destination_endpoint &&
               ramp_endpoint_height(ramp, source_endpoint) ==
                   ramp_endpoint_height(neighbor, destination_endpoint);
    }
    return 0;
}

static int64_t support_half_steps(const game_rules_cell* cell)
{
    return (int64_t)cell->elevation * 2 +
           (cell->kind == GAME_RULES_CELL_RAMP ? 1 : 0);
}

void game_rules_c_validate_level(const game_rules_c_level_view* level,
                                 const game_rules_c_allocator* allocator,
                                 game_rules_c_validation_result* result)
{
    indexed_cell* cells = NULL;
    indexed_fixture* fixtures = NULL;
    indexed_entity* entities_spatial = NULL;
    indexed_entity* entities_by_id = NULL;
    uint32_t cell_count = 0U;
    uint32_t fixture_count = 0U;
    uint32_t entity_count = 0U;
    uint32_t player_count = 0U;
    uint32_t index;
    memset(result, 0, sizeof(*result));

    if (!has_representable_extent(level)) {
        add_error(result, allocator, GAME_RULES_VALIDATION_INVALID_DIMENSIONS,
                  level->coordinates.origin, 0U);
    }
    if (level->coordinates.positive_x > GAME_RULES_HORIZONTAL_WEST ||
        level->coordinates.positive_y > GAME_RULES_VERTICAL_SOUTH) {
        add_error(result, allocator, GAME_RULES_VALIDATION_INVALID_COORDINATE_SYSTEM,
                  level->coordinates.origin, 0U);
    }
    if ((uint64_t)level->width * (uint64_t)level->height != level->cell_count) {
        add_error(result, allocator, GAME_RULES_VALIDATION_CELL_COUNT_MISMATCH,
                  level->coordinates.origin, 0U);
    }

    cells = (indexed_cell*)allocate_array(allocator, level->cell_count, sizeof(*cells));
    fixtures = (indexed_fixture*)allocate_array(allocator, level->fixture_count, sizeof(*fixtures));
    entities_spatial = (indexed_entity*)allocate_array(allocator, level->entity_count,
                                                       sizeof(*entities_spatial));
    entities_by_id = (indexed_entity*)allocate_array(allocator, level->entity_count,
                                                     sizeof(*entities_by_id));
    if ((level->cell_count != 0U && cells == NULL) ||
        (level->fixture_count != 0U && fixtures == NULL) ||
        (level->entity_count != 0U &&
         (entities_spatial == NULL || entities_by_id == NULL))) {
        result->allocation_failed = 1U;
        goto finish;
    }

    for (index = 0U; index < level->cell_count; ++index) {
        const game_rules_cell* cell = &level->cells[index];
        if (!in_bounds(level, cell->coordinate)) {
            add_error(result, allocator, GAME_RULES_VALIDATION_CELL_OUT_OF_BOUNDS,
                      cell->coordinate, 0U);
            continue;
        }
        cells[cell_count].value = cell;
        cells[cell_count].index = index;
        ++cell_count;
        if (cell->kind == GAME_RULES_CELL_FLAT) {
            if (!representable_cell_height(cell->elevation, 0)) {
                add_error(result, allocator, GAME_RULES_VALIDATION_INVALID_CELL_HEIGHT,
                          cell->coordinate, 0U);
            }
        } else if (cell->kind == GAME_RULES_CELL_RAMP) {
            if (!valid_direction(cell->low_direction)) {
                add_error(result, allocator, GAME_RULES_VALIDATION_INVALID_RAMP_DIRECTION,
                          cell->coordinate, 0U);
            }
            if (!representable_cell_height(cell->elevation, 1)) {
                add_error(result, allocator, GAME_RULES_VALIDATION_INVALID_CELL_HEIGHT,
                          cell->coordinate, 0U);
            }
        }
    }
    qsort(cells, cell_count, sizeof(*cells), compare_cells);
    for (index = 1U; index < cell_count; ++index) {
        if (coordinates_equal(cells[index - 1U].value->coordinate,
                              cells[index].value->coordinate)) {
            add_error(result, allocator, GAME_RULES_VALIDATION_DUPLICATE_CELL,
                      cells[index].value->coordinate, 0U);
        }
    }
    for (index = 0U; index < cell_count;) {
        const game_rules_cell* ramp = cells[index].value;
        uint32_t next = index + 1U;
        while (next < cell_count && coordinates_equal(cells[next].value->coordinate,
                                                       ramp->coordinate)) {
            ++next;
        }
        if (ramp->kind == GAME_RULES_CELL_RAMP && valid_direction(ramp->low_direction)) {
            game_rules_coordinate low_coordinate;
            game_rules_coordinate high_coordinate;
            uint32_t high_direction;
            const indexed_cell* low;
            const indexed_cell* high;
            const int has_high = opposite_direction(ramp->low_direction, &high_direction);
            if (!step_coordinate(ramp->coordinate, ramp->low_direction, &level->coordinates,
                                 &low_coordinate) ||
                !has_high ||
                !step_coordinate(ramp->coordinate, high_direction, &level->coordinates,
                                 &high_coordinate)) {
                add_error(result, allocator, GAME_RULES_VALIDATION_INVALID_RAMP_ENDPOINTS,
                          ramp->coordinate, 0U);
            } else {
                low = find_indexed_cell(cells, cell_count, low_coordinate);
                high = find_indexed_cell(cells, cell_count, high_coordinate);
                if (low == NULL || high == NULL ||
                    !endpoint_is_valid(ramp, ramp->low_direction, low->value) ||
                    !endpoint_is_valid(ramp, high_direction, high->value)) {
                    add_error(result, allocator, GAME_RULES_VALIDATION_INVALID_RAMP_ENDPOINTS,
                              ramp->coordinate, 0U);
                }
            }
        }
        index = next;
    }

    for (index = 0U; index < level->fixture_count; ++index) {
        const game_rules_fixture* fixture = &level->fixtures[index];
        const indexed_cell* cell;
        if (!in_bounds(level, fixture->coordinate)) {
            add_error(result, allocator, GAME_RULES_VALIDATION_FIXTURE_OUT_OF_BOUNDS,
                      fixture->coordinate, 0U);
            continue;
        }
        cell = find_indexed_cell(cells, cell_count, fixture->coordinate);
        if (cell == NULL) {
            add_error(result, allocator, GAME_RULES_VALIDATION_FIXTURE_OUT_OF_BOUNDS,
                      fixture->coordinate, 0U);
            continue;
        }
        if (cell->value->kind == GAME_RULES_CELL_RAMP) {
            add_error(result, allocator, GAME_RULES_VALIDATION_FIXTURE_ON_RAMP,
                      fixture->coordinate, 0U);
        }
        fixtures[fixture_count].value = fixture;
        fixtures[fixture_count].index = index;
        ++fixture_count;
        if ((fixture->kind == GAME_RULES_FIXTURE_SWITCH ||
             fixture->kind == GAME_RULES_FIXTURE_DOOR) &&
            fixture->color > GAME_RULES_COLOR_YELLOW) {
            add_error(result, allocator, GAME_RULES_VALIDATION_INVALID_FIXTURE_COLOR,
                      fixture->coordinate, 0U);
        }
    }
    qsort(fixtures, fixture_count, sizeof(*fixtures), compare_fixtures);
    for (index = 1U; index < fixture_count; ++index) {
        if (coordinates_equal(fixtures[index - 1U].value->coordinate,
                              fixtures[index].value->coordinate)) {
            add_error(result, allocator, GAME_RULES_VALIDATION_DUPLICATE_FIXTURE,
                      fixtures[index].value->coordinate, 0U);
        }
    }

    for (index = 0U; index < level->entity_count; ++index) {
        const game_rules_entity* entity = &level->entities[index];
        const indexed_cell* cell;
        if (entity->kind == GAME_RULES_ENTITY_PLAYER) {
            ++player_count;
        }
        if (entity->kind > GAME_RULES_ENTITY_BARREL) {
            add_error(result, allocator, GAME_RULES_VALIDATION_INVALID_ENTITY_KIND,
                      entity->coordinate, entity->id);
        }
        if (entity->id == 0U) {
            add_error(result, allocator, GAME_RULES_VALIDATION_INVALID_ENTITY_ID,
                      entity->coordinate, entity->id);
        }
        entities_by_id[index].value = entity;
        entities_by_id[index].index = index;
        if (!in_bounds(level, entity->coordinate)) {
            add_error(result, allocator, GAME_RULES_VALIDATION_ENTITY_OUT_OF_BOUNDS,
                      entity->coordinate, entity->id);
            continue;
        }
        cell = find_indexed_cell(cells, cell_count, entity->coordinate);
        if (cell == NULL) {
            add_error(result, allocator, GAME_RULES_VALIDATION_ENTITY_OUT_OF_BOUNDS,
                      entity->coordinate, entity->id);
            continue;
        }
        if ((int64_t)entity->bottom_half_steps < support_half_steps(cell->value)) {
            add_error(result, allocator, GAME_RULES_VALIDATION_ENTITY_BELOW_SURFACE,
                      entity->coordinate, entity->id);
        }
        entities_spatial[entity_count].value = entity;
        entities_spatial[entity_count].index = index;
        ++entity_count;
    }
    qsort(entities_by_id, level->entity_count, sizeof(*entities_by_id), compare_entities_id);
    for (index = 1U; index < level->entity_count; ++index) {
        if (entities_by_id[index - 1U].value->id == entities_by_id[index].value->id) {
            add_error(result, allocator, GAME_RULES_VALIDATION_DUPLICATE_ENTITY_ID,
                      entities_by_id[index].value->coordinate,
                      entities_by_id[index].value->id);
        }
    }
    if (player_count != 1U) {
        const game_rules_coordinate zero = {0, 0};
        add_error(result, allocator, GAME_RULES_VALIDATION_PLAYER_COUNT_NOT_ONE, zero, 0U);
    }

    qsort(entities_spatial, entity_count, sizeof(*entities_spatial), compare_entities_spatial);
    for (index = 0U; index < entity_count;) {
        uint32_t end = index + 1U;
        uint32_t member;
        const indexed_fixture* fixture;
        while (end < entity_count &&
               coordinates_equal(entities_spatial[index].value->coordinate,
                                 entities_spatial[end].value->coordinate)) {
            ++end;
        }
        for (member = index + 1U; member < end; ++member) {
            const int64_t previous_top =
                (int64_t)entities_spatial[member - 1U].value->bottom_half_steps + 2;
            if ((int64_t)entities_spatial[member].value->bottom_half_steps < previous_top) {
                add_error(result, allocator, GAME_RULES_VALIDATION_OVERLAPPING_ENTITIES,
                          entities_spatial[member].value->coordinate,
                          entities_spatial[member].value->id);
            }
        }
        for (member = index; member + 1U < end; ++member) {
            if (entities_spatial[member].value->kind == GAME_RULES_ENTITY_PLAYER) {
                add_error(result, allocator, GAME_RULES_VALIDATION_PLAYER_NOT_TOP_OF_STACK,
                          entities_spatial[member].value->coordinate,
                          entities_spatial[member].value->id);
                break;
            }
        }
        fixture = find_indexed_fixture(fixtures, fixture_count,
                                       entities_spatial[index].value->coordinate);
        if (fixture != NULL && fixture->value->kind == GAME_RULES_FIXTURE_EXIT) {
            const indexed_cell* cell = find_indexed_cell(
                cells, cell_count, entities_spatial[index].value->coordinate);
            const int sole_player = end - index == 1U &&
                entities_spatial[index].value->kind == GAME_RULES_ENTITY_PLAYER;
            const int at_floor = cell != NULL &&
                (int64_t)entities_spatial[index].value->bottom_half_steps ==
                    support_half_steps(cell->value);
            if (!sole_player || !at_floor) {
                add_error(result, allocator,
                          GAME_RULES_VALIDATION_INVALID_TELEPORTER_OCCUPANCY,
                          entities_spatial[index].value->coordinate,
                          entities_spatial[index].value->id);
            }
        }
        index = end;
    }

finish:
    game_rules_c_deallocate_owned(entities_by_id);
    game_rules_c_deallocate_owned(entities_spatial);
    game_rules_c_deallocate_owned(fixtures);
    game_rules_c_deallocate_owned(cells);
    if (result->allocation_failed == 0U && result->count > 1U) {
        qsort(result->errors, result->count, sizeof(*result->errors),
              compare_validation_errors);
    }
}

void game_rules_c_validation_result_destroy(game_rules_c_validation_result* result)
{
    if (result == NULL) {
        return;
    }
    game_rules_c_deallocate_owned(result->errors);
    memset(result, 0, sizeof(*result));
}

void game_rules_c_canonicalize_level(game_rules_c_owned_level* level)
{
    qsort(level->cells, level->view.cell_count, sizeof(*level->cells), compare_public_cells);
    qsort(level->fixtures, level->view.fixture_count, sizeof(*level->fixtures),
          compare_public_fixtures);
    qsort(level->entities, level->view.entity_count, sizeof(*level->entities),
          compare_public_entities);
}

const char* game_rules_c_validation_error_name(uint32_t code)
{
    static const char* const names[] = {
        "invalid_dimensions", "invalid_coordinate_system", "cell_count_mismatch",
        "cell_out_of_bounds", "duplicate_cell", "invalid_cell_height",
        "invalid_ramp_direction", "invalid_ramp_endpoints", "fixture_out_of_bounds",
        "fixture_on_ramp", "duplicate_fixture", "invalid_fixture_color",
        "entity_out_of_bounds", "duplicate_entity_id", "invalid_entity_kind",
        "entity_below_surface", "overlapping_entities", "player_not_top_of_stack",
        "player_count_not_one", "invalid_teleporter_occupancy", "invalid_entity_id"
    };
    return code < sizeof(names) / sizeof(names[0]) ? names[code] : "unknown";
}
