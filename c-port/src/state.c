#include "c_api_internal.h"

#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define GAME_RULES_C_NO_INDEX UINT32_MAX

typedef struct state_layout {
    unsigned char* base;
    size_t offset;
    size_t capacity;
    uint32_t failed;
} state_layout;

typedef struct game_rules_c_command_plan {
    game_rules_session working;
    void* state_storage;
} game_rules_c_command_plan;

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

static void* take(state_layout* value, size_t count, size_t size, size_t alignment)
{
    size_t bytes;
    size_t padding;
    size_t start;
    size_t end;
    if (value->failed || alignment == 0U || !multiply_size(count, size, &bytes)) {
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

static int same_coordinate(game_rules_coordinate left, game_rules_coordinate right)
{
    return left.x == right.x && left.y == right.y;
}

static int coordinate_compare(game_rules_coordinate left, game_rules_coordinate right)
{
    if (left.y != right.y) return left.y < right.y ? -1 : 1;
    if (left.x != right.x) return left.x < right.x ? -1 : 1;
    return 0;
}

static int entity_compare(const void* left_value, const void* right_value)
{
    const game_rules_entity* left = (const game_rules_entity*)left_value;
    const game_rules_entity* right = (const game_rules_entity*)right_value;
    int spatial = coordinate_compare(left->coordinate, right->coordinate);
    if (spatial != 0) return spatial;
    if (left->bottom_half_steps != right->bottom_half_steps) {
        return left->bottom_half_steps < right->bottom_half_steps ? -1 : 1;
    }
    if (left->id != right->id) return left->id < right->id ? -1 : 1;
    return 0;
}

static int target_compare(const void* left_value, const void* right_value)
{
    const game_rules_c_blast_target* left =
        (const game_rules_c_blast_target*)left_value;
    const game_rules_c_blast_target* right =
        (const game_rules_c_blast_target*)right_value;
    int spatial = coordinate_compare(left->coordinate, right->coordinate);
    if (spatial != 0) return spatial;
    if (left->bottom_half_steps != right->bottom_half_steps) {
        return left->bottom_half_steps < right->bottom_half_steps ? -1 : 1;
    }
    if (left->id != right->id) return left->id < right->id ? -1 : 1;
    return 0;
}

static int u64_compare(const void* left_value, const void* right_value)
{
    uint64_t left = *(const uint64_t*)left_value;
    uint64_t right = *(const uint64_t*)right_value;
    if (left == right) return 0;
    return left < right ? -1 : 1;
}

static uint32_t cell_index(const game_rules_session* session,
                           game_rules_coordinate coordinate)
{
    int64_t offset_x = (int64_t)coordinate.x - session->coordinates.origin.x;
    int64_t offset_y = (int64_t)coordinate.y - session->coordinates.origin.y;
    uint64_t index;
    if (offset_x < 0 || offset_y < 0 ||
        offset_x >= (int64_t)session->width || offset_y >= (int64_t)session->height) {
        return GAME_RULES_C_NO_INDEX;
    }
    index = (uint64_t)offset_y * session->width + (uint64_t)offset_x;
    return index > UINT32_MAX ? GAME_RULES_C_NO_INDEX : (uint32_t)index;
}

static const game_rules_cell* find_cell(const game_rules_session* session,
                                        game_rules_coordinate coordinate)
{
    uint32_t index = cell_index(session, coordinate);
    return index == GAME_RULES_C_NO_INDEX || index >= session->cell_count
        ? NULL : &session->cells[index];
}

static const game_rules_fixture* find_fixture(const game_rules_session* session,
                                              game_rules_coordinate coordinate)
{
    uint32_t index = cell_index(session, coordinate);
    uint32_t fixture_index;
    if (index == GAME_RULES_C_NO_INDEX || index >= session->cell_count) return NULL;
    fixture_index = session->fixture_index_by_cell[index];
    return fixture_index == GAME_RULES_C_NO_INDEX ? NULL : &session->fixtures[fixture_index];
}

static int step(const game_rules_session* session,
                game_rules_coordinate coordinate,
                uint32_t direction,
                game_rules_coordinate* result)
{
    int32_t dx = 0;
    int32_t dy = 0;
    int64_t x;
    int64_t y;
    switch (direction) {
    case GAME_RULES_DIRECTION_NORTH:
        dy = session->coordinates.positive_y == GAME_RULES_VERTICAL_NORTH ? 1 : -1;
        break;
    case GAME_RULES_DIRECTION_EAST:
        dx = session->coordinates.positive_x == GAME_RULES_HORIZONTAL_EAST ? 1 : -1;
        break;
    case GAME_RULES_DIRECTION_SOUTH:
        dy = session->coordinates.positive_y == GAME_RULES_VERTICAL_SOUTH ? 1 : -1;
        break;
    case GAME_RULES_DIRECTION_WEST:
        dx = session->coordinates.positive_x == GAME_RULES_HORIZONTAL_WEST ? 1 : -1;
        break;
    default: return 0;
    }
    x = (int64_t)coordinate.x + dx;
    y = (int64_t)coordinate.y + dy;
    if (x < INT32_MIN || x > INT32_MAX || y < INT32_MIN || y > INT32_MAX) return 0;
    result->x = (int32_t)x;
    result->y = (int32_t)y;
    return cell_index(session, *result) != GAME_RULES_C_NO_INDEX;
}

static uint32_t opposite(uint32_t direction)
{
    switch (direction) {
    case GAME_RULES_DIRECTION_NORTH: return GAME_RULES_DIRECTION_SOUTH;
    case GAME_RULES_DIRECTION_EAST: return GAME_RULES_DIRECTION_WEST;
    case GAME_RULES_DIRECTION_SOUTH: return GAME_RULES_DIRECTION_NORTH;
    case GAME_RULES_DIRECTION_WEST: return GAME_RULES_DIRECTION_EAST;
    default: return direction;
    }
}

static int ramp_endpoint(const game_rules_cell* ramp,
                         uint32_t direction,
                         uint32_t* high)
{
    if (direction == ramp->low_direction) {
        *high = 0U;
        return 1;
    }
    if (direction == opposite(ramp->low_direction)) {
        *high = 1U;
        return 1;
    }
    return 0;
}

static int64_t ramp_endpoint_half_steps(const game_rules_cell* ramp, uint32_t high)
{
    return (int64_t)ramp->elevation * 2 + (high ? 2 : 0);
}

static int ramps_connect(const game_rules_cell* source,
                         const game_rules_cell* destination,
                         uint32_t direction)
{
    uint32_t source_high;
    uint32_t destination_high;
    if (!ramp_endpoint(source, direction, &source_high) ||
        !ramp_endpoint(destination, opposite(direction), &destination_high)) return 0;
    return source_high != destination_high &&
        ramp_endpoint_half_steps(source, source_high) ==
            ramp_endpoint_half_steps(destination, destination_high);
}

static int ramps_align_laterally(const game_rules_cell* source,
                                 const game_rules_cell* destination,
                                 uint32_t direction)
{
    uint32_t ignored;
    return source->low_direction == destination->low_direction &&
        source->elevation == destination->elevation &&
        !ramp_endpoint(source, direction, &ignored);
}

static int32_t support_half_steps(const game_rules_cell* cell)
{
    return cell->elevation * 2 + (cell->kind == GAME_RULES_CELL_RAMP ? 1 : 0);
}

static int state_contains_id(const game_rules_c_state* state, uint64_t id)
{
    uint32_t left = 0U;
    uint32_t right = state->armed_barrel_count;
    while (left < right) {
        uint32_t middle = left + (right - left) / 2U;
        uint64_t value = state->armed_barrel_ids[middle];
        if (value < id) left = middle + 1U;
        else right = middle;
    }
    return left < state->armed_barrel_count && state->armed_barrel_ids[left] == id;
}

static int arm_barrel(game_rules_c_state* state, uint64_t id)
{
    uint32_t position = 0U;
    while (position < state->armed_barrel_count &&
           state->armed_barrel_ids[position] < id) ++position;
    if (position < state->armed_barrel_count && state->armed_barrel_ids[position] == id) return 1;
    if (state->armed_barrel_count >= state->armed_barrel_capacity) return 0;
    memmove(&state->armed_barrel_ids[position + 1U],
            &state->armed_barrel_ids[position],
            (state->armed_barrel_count - position) * sizeof(uint64_t));
    state->armed_barrel_ids[position] = id;
    ++state->armed_barrel_count;
    return 1;
}

static int occupied(const game_rules_c_state* state, game_rules_coordinate coordinate)
{
    uint32_t index;
    for (index = 0U; index < state->entity_count; ++index) {
        if (same_coordinate(state->entities[index].coordinate, coordinate)) return 1;
    }
    return 0;
}

static int volume_is_clear(const game_rules_c_state* state,
                           game_rules_coordinate coordinate,
                           int32_t bottom)
{
    uint32_t index;
    int64_t arrival_bottom = bottom;
    int64_t arrival_top = arrival_bottom + 2;
    for (index = 0U; index < state->entity_count; ++index) {
        const game_rules_entity* entity = &state->entities[index];
        int64_t entity_bottom;
        if (!same_coordinate(entity->coordinate, coordinate)) continue;
        entity_bottom = entity->bottom_half_steps;
        if (entity_bottom < arrival_top && arrival_bottom < entity_bottom + 2) return 0;
    }
    return 1;
}

static int state_copy(game_rules_c_state* destination, const game_rules_c_state* source)
{
    if (source->entity_count > destination->entity_capacity ||
        source->armed_barrel_count > destination->armed_barrel_capacity ||
        source->active_switch_color_count > destination->active_switch_color_capacity ||
        source->open_door_count > destination->open_door_capacity) return 0;
    if (source->entity_count) memcpy(destination->entities, source->entities,
                                     source->entity_count * sizeof(game_rules_entity));
    if (source->armed_barrel_count) memcpy(destination->armed_barrel_ids,
        source->armed_barrel_ids, source->armed_barrel_count * sizeof(uint64_t));
    if (source->active_switch_color_count) memcpy(destination->active_switch_colors,
        source->active_switch_colors, source->active_switch_color_count * sizeof(uint32_t));
    if (source->open_door_count) memcpy(destination->open_doors, source->open_doors,
        source->open_door_count * sizeof(game_rules_coordinate));
    destination->entity_count = source->entity_count;
    destination->armed_barrel_count = source->armed_barrel_count;
    destination->active_switch_color_count = source->active_switch_color_count;
    destination->open_door_count = source->open_door_count;
    destination->outcome = source->outcome;
    return 1;
}

static game_rules_event* add_event(game_rules_session* session,
                                   uint32_t* count,
                                   uint32_t kind)
{
    game_rules_event* event;
    if (*count >= session->scratch_event_capacity) return NULL;
    event = &session->scratch_events[*count];
    ++*count;
    memset(event, 0, sizeof(*event));
    event->kind = kind;
    return event;
}

static int state_snapshot_layout(state_layout* layout,
                                 const game_rules_c_state* source,
                                 game_rules_c_state* target)
{
    memset(target, 0, sizeof(*target));
    target->entities = (game_rules_entity*)take(layout, source->entity_count,
        sizeof(game_rules_entity), _Alignof(game_rules_entity));
    target->armed_barrel_ids = (uint64_t*)take(layout, source->armed_barrel_count,
        sizeof(uint64_t), _Alignof(uint64_t));
    target->active_switch_colors = (uint32_t*)take(
        layout, source->active_switch_color_count, sizeof(uint32_t), _Alignof(uint32_t));
    target->open_doors = (game_rules_coordinate*)take(layout, source->open_door_count,
        sizeof(game_rules_coordinate), _Alignof(game_rules_coordinate));
    target->entity_count = target->entity_capacity = source->entity_count;
    target->armed_barrel_count = target->armed_barrel_capacity = source->armed_barrel_count;
    target->active_switch_color_count = target->active_switch_color_capacity =
        source->active_switch_color_count;
    target->open_door_count = target->open_door_capacity = source->open_door_count;
    target->outcome = source->outcome;
    if (layout->base != NULL && !layout->failed) return state_copy(target, source);
    return !layout->failed;
}

static game_rules_c_history_entry* create_history_entry(
    const game_rules_c_allocator* allocator,
    const game_rules_c_state* state)
{
    state_layout measure = {0};
    state_layout arena = {0};
    game_rules_c_state ignored;
    game_rules_c_history_entry* entry;
    void* owner;

    take(&measure, 1U, sizeof(game_rules_c_history_entry),
         _Alignof(game_rules_c_history_entry));
    if (!state_snapshot_layout(&measure, state, &ignored) || measure.failed) return NULL;
    owner = game_rules_c_allocate_owned(allocator, measure.offset);
    if (owner == NULL) return NULL;
    memset(owner, 0, measure.offset);
    arena.base = (unsigned char*)owner;
    arena.capacity = measure.offset;
    entry = (game_rules_c_history_entry*)take(
        &arena, 1U, sizeof(game_rules_c_history_entry),
        _Alignof(game_rules_c_history_entry));
    if (entry == NULL ||
        !state_snapshot_layout(&arena, state, &entry->state) || arena.failed) {
        game_rules_c_deallocate_owned(owner);
        return NULL;
    }
    return entry;
}

static int reserve_ticks(game_rules_session* session,
                         const game_rules_c_allocator* allocator,
                         uint32_t needed)
{
    uint32_t capacity;
    size_t bytes;
    game_rules_c_tick* replacement;
    if (needed <= session->initialization_tick_capacity) return 1;
    capacity = session->initialization_tick_capacity
        ? session->initialization_tick_capacity : 4U;
    while (capacity < needed) {
        if (capacity > UINT32_MAX / 2U) {
            capacity = needed;
            break;
        }
        capacity *= 2U;
    }
    if (!multiply_size(capacity, sizeof(game_rules_c_tick), &bytes)) return 0;
    replacement = (game_rules_c_tick*)game_rules_c_allocate_owned(allocator, bytes);
    if (replacement == NULL) return 0;
    memset(replacement, 0, bytes);
    if (session->initialization_tick_count) {
        memcpy(replacement, session->initialization_ticks,
               session->initialization_tick_count * sizeof(game_rules_c_tick));
    }
    game_rules_c_deallocate_owned(session->initialization_ticks);
    session->initialization_ticks = replacement;
    session->initialization_tick_capacity = capacity;
    return 1;
}

static int append_tick(game_rules_session* session,
                       const game_rules_c_allocator* allocator,
                       uint32_t event_count,
                       const game_rules_c_state* state)
{
    state_layout measure = {0};
    state_layout arena = {0};
    game_rules_c_state ignored;
    game_rules_c_tick tick;
    void* storage;
    if (session->initialization_tick_count == UINT32_MAX) return 0;
    memset(&tick, 0, sizeof(tick));
    take(&measure, event_count, sizeof(game_rules_event), _Alignof(game_rules_event));
    if (!state_snapshot_layout(&measure, state, &ignored) || measure.failed) return 0;
    /* Retained ticks cannot alias either mutable session state buffer. */
    storage = game_rules_c_allocate_owned(allocator, measure.offset);
    if (storage == NULL) return 0;
    arena.base = (unsigned char*)storage;
    arena.capacity = measure.offset;
    tick.index = session->initialization_tick_count;
    tick.events = (game_rules_event*)take(&arena, event_count,
        sizeof(game_rules_event), _Alignof(game_rules_event));
    tick.event_count = event_count;
    if (event_count) memcpy(tick.events, session->scratch_events,
                            event_count * sizeof(game_rules_event));
    if (!state_snapshot_layout(&arena, state, &tick.state_after) || arena.failed ||
        !reserve_ticks(session, allocator, session->initialization_tick_count + 1U)) {
        game_rules_c_deallocate_owned(storage);
        return 0;
    }
    tick.owned_storage = storage;
    session->initialization_ticks[session->initialization_tick_count++] = tick;
    return 1;
}

static void swap_current_and_scratch(game_rules_session* session)
{
    game_rules_c_state temporary = session->current_state;
    session->current_state = session->scratch_state;
    session->scratch_state = temporary;
}

static int contains_color(const game_rules_c_state* state, uint32_t color)
{
    uint32_t index;
    for (index = 0U; index < state->active_switch_color_count; ++index) {
        if (state->active_switch_colors[index] == color) return 1;
    }
    return 0;
}

static int contains_door(const game_rules_c_state* state, game_rules_coordinate coordinate)
{
    uint32_t index;
    for (index = 0U; index < state->open_door_count; ++index) {
        if (same_coordinate(state->open_doors[index], coordinate)) return 1;
    }
    return 0;
}

static void reject_flat_move(game_rules_session* session,
                             game_rules_c_command_transaction* transaction,
                             uint32_t status)
{
    game_rules_event* event = &session->scratch_events[0];
    memset(event, 0, sizeof(*event));
    event->kind = GAME_RULES_EVENT_MOVE_BLOCKED;
    event->direction = transaction->direction;
    event->move_status = status;
    transaction->status = status;
    transaction->final_state = &session->current_state;
    transaction->events = event;
    transaction->event_count = 1U;
}

void game_rules_c_plan_player_move(game_rules_session* session,
                                   uint32_t direction,
                                   game_rules_c_command_transaction* transaction)
{
    const game_rules_entity* player = NULL;
    const game_rules_entity* push_target = NULL;
    const game_rules_entity* destination_top = NULL;
    game_rules_entity* next_player = NULL;
    game_rules_entity* next_pushed = NULL;
    const game_rules_cell* source_cell;
    const game_rules_cell* destination_cell;
    const game_rules_cell* pushed_destination_cell;
    const game_rules_fixture* destination_fixture;
    const game_rules_fixture* pushed_destination_fixture;
    game_rules_coordinate destination;
    game_rules_coordinate pushed_destination;
    int32_t destination_support;
    int32_t push_contact_bottom = 0;
    int32_t player_new_bottom;
    int has_push_contact = 0;
    int lateral_ramp_push;
    uint32_t destination_entity_count = 0U;
    uint32_t index;
    game_rules_event* moved;

    memset(transaction, 0, sizeof(*transaction));
    transaction->direction = direction;
    if (session != NULL && session->has_level) {
        transaction->final_state = &session->current_state;
    }
    if (direction > GAME_RULES_DIRECTION_WEST) {
        transaction->status = GAME_RULES_MOVE_INVALID_DIRECTION;
        return;
    }
    transaction->has_direction = 1U;
    if (session == NULL || !session->has_level) {
        transaction->status = GAME_RULES_MOVE_NO_LEVEL;
        return;
    }
    if (session->current_state.outcome != GAME_RULES_OUTCOME_ONGOING) {
        reject_flat_move(session, transaction, GAME_RULES_MOVE_LEVEL_TERMINAL);
        return;
    }
    for (index = 0U; index < session->current_state.entity_count; ++index) {
        if (session->current_state.entities[index].kind == GAME_RULES_ENTITY_PLAYER) {
            player = &session->current_state.entities[index];
            break;
        }
    }
    if (player == NULL) {
        reject_flat_move(session, transaction, GAME_RULES_MOVE_NO_LEVEL);
        return;
    }
    if (!step(session, player->coordinate, direction, &destination)) {
        reject_flat_move(session, transaction, GAME_RULES_MOVE_WORLD_BOUNDARY);
        return;
    }
    source_cell = find_cell(session, player->coordinate);
    destination_cell = find_cell(session, destination);
    if (source_cell == NULL || destination_cell == NULL) {
        reject_flat_move(session, transaction, GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
        return;
    }

    lateral_ramp_push = source_cell->kind == GAME_RULES_CELL_RAMP &&
        destination_cell->kind == GAME_RULES_CELL_RAMP &&
        player->bottom_half_steps == support_half_steps(source_cell) &&
        ramps_align_laterally(source_cell, destination_cell, direction);
    if (source_cell->kind == GAME_RULES_CELL_FLAT &&
        destination_cell->kind == GAME_RULES_CELL_FLAT) {
        has_push_contact = 1;
        push_contact_bottom = player->bottom_half_steps;
    } else if (source_cell->kind == GAME_RULES_CELL_RAMP &&
               destination_cell->kind == GAME_RULES_CELL_FLAT &&
               player->bottom_half_steps == support_half_steps(source_cell) &&
               (direction == source_cell->low_direction ||
                direction == opposite(source_cell->low_direction))) {
        has_push_contact = 1;
        push_contact_bottom = support_half_steps(destination_cell);
    } else if (lateral_ramp_push) {
        has_push_contact = 1;
        push_contact_bottom = support_half_steps(destination_cell);
    }

    destination_fixture = find_fixture(session, destination);
    if (destination_fixture != NULL &&
        destination_fixture->kind == GAME_RULES_FIXTURE_DOOR &&
        !contains_door(&session->current_state, destination)) {
        reject_flat_move(session, transaction, GAME_RULES_MOVE_CLOSED_DOOR);
        return;
    }

    destination_support = support_half_steps(destination_cell);
    for (index = 0U; index < session->current_state.entity_count; ++index) {
        const game_rules_entity* entity = &session->current_state.entities[index];
        int64_t top;
        if (!same_coordinate(entity->coordinate, destination)) continue;
        ++destination_entity_count;
        if (destination_top == NULL ||
            entity->bottom_half_steps > destination_top->bottom_half_steps) {
            destination_top = entity;
        }
        if (has_push_contact && entity->bottom_half_steps == push_contact_bottom &&
            (entity->kind == GAME_RULES_ENTITY_BOX ||
             entity->kind == GAME_RULES_ENTITY_BARREL)) {
            push_target = entity;
        }
        top = (int64_t)entity->bottom_half_steps + 2;
        if (top > destination_support && top <= INT32_MAX) {
            destination_support = (int32_t)top;
        }
    }

    if (push_target != NULL) {
        game_rules_event* pushed;
        int32_t pushed_new_bottom = push_target->bottom_half_steps;
        if (destination_cell->kind != GAME_RULES_CELL_FLAT && !lateral_ramp_push) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
            return;
        }
        if (push_target != destination_top) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_STACKED_PUSH_TARGET);
            return;
        }
        if (destination_fixture != NULL &&
            destination_fixture->kind == GAME_RULES_FIXTURE_EXIT) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_TELEPORTER_RESTRICTION);
            return;
        }
        if (!step(session, push_target->coordinate, direction,
                  &pushed_destination)) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_WORLD_BOUNDARY);
            return;
        }
        pushed_destination_cell = find_cell(session, pushed_destination);
        if (pushed_destination_cell == NULL) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
            return;
        }
        if (lateral_ramp_push &&
            pushed_destination_cell->kind != GAME_RULES_CELL_RAMP) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
            return;
        }
        if (pushed_destination_cell->kind == GAME_RULES_CELL_RAMP) {
            if (lateral_ramp_push) {
                if (!ramps_align_laterally(destination_cell,
                                            pushed_destination_cell, direction) ||
                    push_target->bottom_half_steps !=
                        support_half_steps(destination_cell)) {
                    reject_flat_move(session, transaction,
                                     GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
                    return;
                }
            } else {
                int64_t high_endpoint =
                    (int64_t)pushed_destination_cell->elevation * 2 + 2;
                if (direction != pushed_destination_cell->low_direction ||
                    push_target->bottom_half_steps != high_endpoint) {
                    reject_flat_move(session, transaction,
                                     GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
                    return;
                }
            }
            pushed_new_bottom = support_half_steps(pushed_destination_cell);
        }
        pushed_destination_fixture = find_fixture(session, pushed_destination);
        if (pushed_destination_fixture != NULL &&
            pushed_destination_fixture->kind == GAME_RULES_FIXTURE_EXIT) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_TELEPORTER_RESTRICTION);
            return;
        }
        if (pushed_destination_fixture != NULL &&
            pushed_destination_fixture->kind == GAME_RULES_FIXTURE_DOOR &&
            !contains_door(&session->current_state, pushed_destination)) {
            reject_flat_move(session, transaction, GAME_RULES_MOVE_CLOSED_DOOR);
            return;
        }
        if (!volume_is_clear(&session->current_state, pushed_destination,
                             pushed_new_bottom)) {
            reject_flat_move(session, transaction, GAME_RULES_MOVE_OCCUPIED);
            return;
        }
        if (support_half_steps(pushed_destination_cell) >
            push_target->bottom_half_steps) {
            reject_flat_move(session, transaction, GAME_RULES_MOVE_LEDGE);
            return;
        }
        if (!state_copy(&session->scratch_state, &session->current_state)) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
            return;
        }
        for (index = 0U; index < session->scratch_state.entity_count; ++index) {
            game_rules_entity* entity = &session->scratch_state.entities[index];
            if (entity->id == player->id) next_player = entity;
            if (entity->id == push_target->id) next_pushed = entity;
        }
        if (next_player == NULL || next_pushed == NULL) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
            return;
        }
        next_player->coordinate = destination;
        next_player->bottom_half_steps = push_contact_bottom;
        next_pushed->coordinate = pushed_destination;
        next_pushed->bottom_half_steps = pushed_new_bottom;
        qsort(session->scratch_state.entities, session->scratch_state.entity_count,
              sizeof(game_rules_entity), entity_compare);

        moved = &session->scratch_events[0];
        memset(moved, 0, sizeof(*moved));
        moved->kind = GAME_RULES_EVENT_ENTITY_MOVED;
        moved->entity_id = player->id;
        moved->from = player->coordinate;
        moved->to = destination;
        moved->old_bottom_half_steps = player->bottom_half_steps;
        moved->new_bottom_half_steps = push_contact_bottom;
        moved->movement_cause = GAME_RULES_MOVEMENT_PLAYER;

        pushed = &session->scratch_events[1];
        memset(pushed, 0, sizeof(*pushed));
        pushed->kind = GAME_RULES_EVENT_ENTITY_MOVED;
        pushed->entity_id = push_target->id;
        pushed->from = push_target->coordinate;
        pushed->to = pushed_destination;
        pushed->old_bottom_half_steps = push_target->bottom_half_steps;
        pushed->new_bottom_half_steps = pushed_new_bottom;
        pushed->movement_cause = GAME_RULES_MOVEMENT_PLAYER;

        transaction->status = GAME_RULES_MOVE_MOVED;
        transaction->accepted = 1U;
        transaction->initial_state = &session->current_state;
        transaction->final_state = &session->scratch_state;
        transaction->tick_events = moved;
        transaction->tick_event_count = 2U;
        return;
    }

    player_new_bottom = player->bottom_half_steps;
    if (source_cell->kind == GAME_RULES_CELL_FLAT &&
        destination_cell->kind == GAME_RULES_CELL_FLAT) {
        if (destination_fixture != NULL &&
            destination_fixture->kind == GAME_RULES_FIXTURE_EXIT &&
            (destination_entity_count != 0U ||
             player->bottom_half_steps != support_half_steps(destination_cell))) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_TELEPORTER_RESTRICTION);
            return;
        }
        if (destination_support != player->bottom_half_steps) {
            reject_flat_move(session, transaction,
                             destination_entity_count != 0U
                                 ? GAME_RULES_MOVE_OCCUPIED : GAME_RULES_MOVE_LEDGE);
            return;
        }
    } else if (source_cell->kind == GAME_RULES_CELL_FLAT &&
               destination_cell->kind == GAME_RULES_CELL_RAMP) {
        if (destination_entity_count != 0U ||
            player->bottom_half_steps != support_half_steps(source_cell) ||
            (direction != destination_cell->low_direction &&
             direction != opposite(destination_cell->low_direction))) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
            return;
        }
        player_new_bottom = support_half_steps(destination_cell);
    } else if (source_cell->kind == GAME_RULES_CELL_RAMP &&
               destination_cell->kind == GAME_RULES_CELL_FLAT) {
        if (destination_entity_count != 0U ||
            player->bottom_half_steps != support_half_steps(source_cell) ||
            (direction != source_cell->low_direction &&
             direction != opposite(source_cell->low_direction))) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
            return;
        }
        player_new_bottom = support_half_steps(destination_cell);
    } else if (source_cell->kind == GAME_RULES_CELL_RAMP &&
               destination_cell->kind == GAME_RULES_CELL_RAMP) {
        if (destination_entity_count != 0U ||
            player->bottom_half_steps != support_half_steps(source_cell) ||
            (!ramps_connect(source_cell, destination_cell, direction) &&
             !ramps_align_laterally(source_cell, destination_cell, direction))) {
            reject_flat_move(session, transaction,
                             GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
            return;
        }
        player_new_bottom = support_half_steps(destination_cell);
    } else {
        reject_flat_move(session, transaction,
                         GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
        return;
    }
    if (destination_fixture != NULL &&
        destination_fixture->kind == GAME_RULES_FIXTURE_EXIT &&
        (destination_entity_count != 0U ||
         player_new_bottom != support_half_steps(destination_cell))) {
        reject_flat_move(session, transaction,
                         GAME_RULES_MOVE_TELEPORTER_RESTRICTION);
        return;
    }
    if (!state_copy(&session->scratch_state, &session->current_state)) {
        reject_flat_move(session, transaction, GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
        return;
    }
    for (index = 0U; index < session->scratch_state.entity_count; ++index) {
        if (session->scratch_state.entities[index].id == player->id) {
            next_player = &session->scratch_state.entities[index];
            break;
        }
    }
    if (next_player == NULL) {
        reject_flat_move(session, transaction, GAME_RULES_MOVE_UNSUPPORTED_GEOMETRY);
        return;
    }
    next_player->coordinate = destination;
    next_player->bottom_half_steps = player_new_bottom;
    qsort(session->scratch_state.entities, session->scratch_state.entity_count,
          sizeof(game_rules_entity), entity_compare);

    moved = &session->scratch_events[0];
    memset(moved, 0, sizeof(*moved));
    moved->kind = GAME_RULES_EVENT_ENTITY_MOVED;
    moved->entity_id = player->id;
    moved->from = player->coordinate;
    moved->to = destination;
    moved->old_bottom_half_steps = player->bottom_half_steps;
    moved->new_bottom_half_steps = player_new_bottom;
    moved->movement_cause = GAME_RULES_MOVEMENT_PLAYER;

    transaction->status = GAME_RULES_MOVE_MOVED;
    transaction->accepted = 1U;
    transaction->initial_state = &session->current_state;
    transaction->final_state = &session->scratch_state;
    transaction->tick_events = moved;
    transaction->tick_event_count = 1U;
}

static int switch_pressed(const game_rules_session* session,
                          const game_rules_c_state* state,
                          const game_rules_fixture* fixture)
{
    const game_rules_cell* cell = find_cell(session, fixture->coordinate);
    uint32_t index;
    int64_t floor;
    if (cell == NULL || cell->kind != GAME_RULES_CELL_FLAT) return 0;
    floor = (int64_t)cell->elevation * 2;
    for (index = 0U; index < state->entity_count; ++index) {
        if (same_coordinate(state->entities[index].coordinate, fixture->coordinate) &&
            state->entities[index].bottom_half_steps == floor) return 1;
    }
    return 0;
}

static int player_touches_exit(const game_rules_session* session,
                               const game_rules_c_state* state)
{
    uint32_t index;
    for (index = 0U; index < state->entity_count; ++index) {
        const game_rules_entity* player = &state->entities[index];
        const game_rules_fixture* fixture;
        const game_rules_cell* cell;
        if (player->kind != GAME_RULES_ENTITY_PLAYER) continue;
        fixture = find_fixture(session, player->coordinate);
        if (fixture == NULL || fixture->kind != GAME_RULES_FIXTURE_EXIT) return 0;
        cell = find_cell(session, player->coordinate);
        return cell != NULL && cell->kind == GAME_RULES_CELL_FLAT &&
            (int64_t)player->bottom_half_steps == (int64_t)cell->elevation * 2;
    }
    return 0;
}

static int derive_fixture_state(game_rules_session* session,
                                const game_rules_c_state* previous,
                                game_rules_c_state* next,
                                uint32_t* event_count)
{
    uint32_t color;
    uint32_t fixture_index;
    uint32_t active_count = 0U;
    uint32_t active[4] = {0U, 0U, 0U, 0U};
    uint32_t open_count = 0U;
    for (color = GAME_RULES_COLOR_RED; color <= GAME_RULES_COLOR_YELLOW; ++color) {
        int any = 0;
        int all = 1;
        for (fixture_index = 0U; fixture_index < session->fixture_count; ++fixture_index) {
            const game_rules_fixture* fixture = &session->fixtures[fixture_index];
            if (fixture->kind == GAME_RULES_FIXTURE_SWITCH && fixture->color == color) {
                any = 1;
                if (!switch_pressed(session, next, fixture)) all = 0;
            }
        }
        if (any && all) active[active_count++] = color;
    }
    for (color = GAME_RULES_COLOR_RED; color <= GAME_RULES_COLOR_YELLOW; ++color) {
        int was_active = contains_color(previous, color);
        int is_active = 0;
        uint32_t index;
        for (index = 0U; index < active_count; ++index) {
            if (active[index] == color) is_active = 1;
        }
        if (was_active != is_active) {
            game_rules_event* event = add_event(
                session, event_count, GAME_RULES_EVENT_SWITCH_CHANGED);
            if (event == NULL) return 0;
            event->color = color;
            event->active = is_active ? 1U : 0U;
        }
    }
    for (fixture_index = 0U; fixture_index < session->fixture_count; ++fixture_index) {
        const game_rules_fixture* fixture = &session->fixtures[fixture_index];
        int open;
        int was_open;
        uint32_t index;
        if (fixture->kind != GAME_RULES_FIXTURE_DOOR) continue;
        open = occupied(next, fixture->coordinate);
        for (index = 0U; !open && index < active_count; ++index) {
            if (active[index] == fixture->color) open = 1;
        }
        was_open = contains_door(previous, fixture->coordinate);
        if (open) {
            if (open_count >= next->open_door_capacity) return 0;
            next->open_doors[open_count++] = fixture->coordinate;
        }
        if (open != was_open) {
            game_rules_event* event = add_event(session, event_count,
                open ? GAME_RULES_EVENT_DOOR_OPENED : GAME_RULES_EVENT_DOOR_CLOSED);
            if (event == NULL) return 0;
            event->coordinate = fixture->coordinate;
            event->color = fixture->color;
        }
    }
    if (active_count > next->active_switch_color_capacity) return 0;
    if (active_count) memcpy(next->active_switch_colors, active,
                             active_count * sizeof(uint32_t));
    next->active_switch_color_count = active_count;
    next->open_door_count = open_count;
    return 1;
}

static int is_terminal_event(uint32_t kind)
{
    return kind == GAME_RULES_EVENT_PLAYER_CRUSHED ||
        kind == GAME_RULES_EVENT_LEVEL_LOST;
}

static int resolve_fixtures_after_physical(game_rules_session* session,
                                           const game_rules_c_state* previous,
                                           game_rules_c_state* next,
                                           uint32_t* event_count)
{
    uint32_t read;
    uint32_t nonterminal = 0U;
    uint32_t terminal = 0U;
    if (player_touches_exit(session, next)) {
        for (read = 0U; read < *event_count; ++read) {
            if (!is_terminal_event(session->scratch_events[read].kind)) {
                session->scratch_events[nonterminal++] = session->scratch_events[read];
            }
        }
        *event_count = nonterminal;
        next->outcome = GAME_RULES_OUTCOME_WON;
        return add_event(session, event_count, GAME_RULES_EVENT_LEVEL_WON) != NULL;
    }
    for (read = 0U; read < *event_count; ++read) {
        if (is_terminal_event(session->scratch_events[read].kind)) {
            if (terminal >= session->scratch_terminal_event_capacity) return 0;
            session->scratch_terminal_events[terminal++] = session->scratch_events[read];
        } else {
            session->scratch_events[nonterminal++] = session->scratch_events[read];
        }
    }
    *event_count = nonterminal;
    if (!derive_fixture_state(session, previous, next, event_count)) return 0;
    if (*event_count > session->scratch_event_capacity - terminal) return 0;
    if (terminal) memcpy(&session->scratch_events[*event_count],
                         session->scratch_terminal_events,
                         terminal * sizeof(game_rules_event));
    *event_count += terminal;
    return 1;
}

static int resolve_initial_fixture_tick(game_rules_session* session,
                                        const game_rules_c_allocator* allocator)
{
    uint32_t event_count = 0U;
    if (!state_copy(&session->scratch_state, &session->current_state)) return 0;
    if (player_touches_exit(session, &session->scratch_state)) {
        session->scratch_state.outcome = GAME_RULES_OUTCOME_WON;
        if (add_event(session, &event_count, GAME_RULES_EVENT_LEVEL_WON) == NULL) return 0;
    } else if (!derive_fixture_state(session, &session->current_state,
                                     &session->scratch_state, &event_count)) return 0;
    if (event_count == 0U) return 1;
    if (!append_tick(session, allocator, event_count, &session->scratch_state)) return 0;
    swap_current_and_scratch(session);
    return 1;
}

int game_rules_c_resolve_falling_tick(game_rules_session* session,
                                      uint32_t* event_count)
{
    const game_rules_c_state* current = &session->current_state;
    game_rules_c_state* next = &session->scratch_state;
    uint32_t start = 0U;
    int lost = 0;
    *event_count = 0U;
    if (current->outcome != GAME_RULES_OUTCOME_ONGOING || !state_copy(next, current)) return 0;
    while (start < current->entity_count) {
        uint32_t end = start + 1U;
        const game_rules_cell* cell;
        int64_t landing;
        const game_rules_entity* landed_below = NULL;
        uint32_t index;
        while (end < current->entity_count && same_coordinate(
            current->entities[end].coordinate, current->entities[start].coordinate)) ++end;
        cell = find_cell(session, current->entities[start].coordinate);
        if (cell == NULL) {
            start = end;
            continue;
        }
        landing = support_half_steps(cell);
        for (index = start; index < end; ++index) {
            const game_rules_entity* before = &current->entities[index];
            game_rules_entity* after;
            game_rules_event* event;
            int64_t old_bottom = before->bottom_half_steps;
            if (old_bottom <= landing) {
                landing = old_bottom + 2;
                landed_below = before;
                continue;
            }
            if (landed_below != NULL && landed_below->kind == GAME_RULES_ENTITY_PLAYER) {
                event = add_event(session, event_count, GAME_RULES_EVENT_PLAYER_CRUSHED);
                if (event == NULL) return -1;
                event->entity_id = landed_below->id;
                event->other_entity_id = before->id;
                lost = 1;
                break;
            }
            after = &next->entities[index];
            after->bottom_half_steps = (int32_t)landing;
            event = add_event(session, event_count, GAME_RULES_EVENT_ENTITY_MOVED);
            if (event == NULL) return -1;
            event->entity_id = before->id;
            event->from = before->coordinate;
            event->to = before->coordinate;
            event->old_bottom_half_steps = before->bottom_half_steps;
            event->new_bottom_half_steps = after->bottom_half_steps;
            event->movement_cause = GAME_RULES_MOVEMENT_FALL;
            if (before->kind == GAME_RULES_ENTITY_BARREL &&
                !state_contains_id(current, before->id)) {
                if (!arm_barrel(next, before->id)) return -1;
                event = add_event(session, event_count, GAME_RULES_EVENT_BARREL_ARMED);
                if (event == NULL) return -1;
                event->entity_id = before->id;
            }
            if (before->kind == GAME_RULES_ENTITY_PLAYER && old_bottom - landing >= 2) lost = 1;
            landing += 2;
            landed_below = after;
        }
        start = end;
    }
    if (*event_count == 0U) return 0;
    if (lost) {
        next->outcome = GAME_RULES_OUTCOME_LOST;
        if (add_event(session, event_count, GAME_RULES_EVENT_LEVEL_LOST) == NULL) return -1;
    }
    return 1;
}

static int door_is_open(const game_rules_c_state* state, game_rules_coordinate coordinate)
{
    return contains_door(state, coordinate);
}

static int destination_fixture_blocks(const game_rules_session* session,
                                      const game_rules_c_state* state,
                                      game_rules_coordinate coordinate)
{
    const game_rules_fixture* fixture = find_fixture(session, coordinate);
    if (fixture == NULL) return 0;
    if (fixture->kind == GAME_RULES_FIXTURE_EXIT) return 1;
    return fixture->kind == GAME_RULES_FIXTURE_DOOR && !door_is_open(state, coordinate);
}

static int resolve_sliding(game_rules_session* session, uint32_t* event_count)
{
    const game_rules_c_state* current = &session->current_state;
    game_rules_c_state* next = &session->scratch_state;
    uint32_t candidate_count = 0U;
    uint32_t cell_number;
    *event_count = 0U;
    if (current->outcome != GAME_RULES_OUTCOME_ONGOING || !state_copy(next, current)) return 0;
    for (cell_number = 0U; cell_number < session->cell_count; ++cell_number) {
        const game_rules_cell* cell = &session->cells[cell_number];
        const game_rules_entity* bottom;
        game_rules_coordinate destination;
        const game_rules_cell* destination_cell;
        uint32_t first = 0U;
        uint32_t end;
        if (cell->kind != GAME_RULES_CELL_RAMP) continue;
        while (first < current->entity_count &&
               coordinate_compare(current->entities[first].coordinate, cell->coordinate) < 0) ++first;
        if (first == current->entity_count ||
            !same_coordinate(current->entities[first].coordinate, cell->coordinate)) continue;
        end = first + 1U;
        while (end < current->entity_count && same_coordinate(
            current->entities[end].coordinate, cell->coordinate)) ++end;
        bottom = &current->entities[first];
        if ((bottom->kind != GAME_RULES_ENTITY_BOX && bottom->kind != GAME_RULES_ENTITY_BARREL) ||
            bottom->bottom_half_steps != support_half_steps(cell)) continue;
        if (!step(session, cell->coordinate, cell->low_direction, &destination) ||
            occupied(current, destination) ||
            destination_fixture_blocks(session, current, destination)) continue;
        destination_cell = find_cell(session, destination);
        if (destination_cell == NULL ||
            (destination_cell->kind == GAME_RULES_CELL_RAMP &&
             !ramps_connect(cell, destination_cell, cell->low_direction))) continue;
        if (candidate_count >= session->cell_count) return -1;
        session->scratch_slides[candidate_count].source = cell->coordinate;
        session->scratch_slides[candidate_count].destination = destination;
        session->scratch_slides[candidate_count].destination_bottom_half_steps =
            support_half_steps(destination_cell);
        session->scratch_slides[candidate_count].first_entity = first;
        session->scratch_slides[candidate_count].entity_count = end - first;
        ++candidate_count;
    }
    for (cell_number = 0U; cell_number < candidate_count; ++cell_number) {
        const game_rules_c_slide_candidate* candidate = &session->scratch_slides[cell_number];
        uint32_t other;
        uint32_t index;
        int conflict = 0;
        int64_t bottom_change;
        for (other = 0U; other < candidate_count; ++other) {
            if (other != cell_number && same_coordinate(
                candidate->destination, session->scratch_slides[other].destination)) {
                conflict = 1;
                break;
            }
        }
        if (conflict) continue;
        bottom_change = (int64_t)candidate->destination_bottom_half_steps -
            current->entities[candidate->first_entity].bottom_half_steps;
        for (index = candidate->first_entity;
             index < candidate->first_entity + candidate->entity_count; ++index) {
            const game_rules_entity* before = &current->entities[index];
            game_rules_entity* after = &next->entities[index];
            game_rules_event* event;
            after->coordinate = candidate->destination;
            after->bottom_half_steps = (int32_t)(before->bottom_half_steps + bottom_change);
            event = add_event(session, event_count, GAME_RULES_EVENT_ENTITY_MOVED);
            if (event == NULL) return -1;
            event->entity_id = before->id;
            event->from = candidate->source;
            event->to = candidate->destination;
            event->old_bottom_half_steps = before->bottom_half_steps;
            event->new_bottom_half_steps = after->bottom_half_steps;
            event->movement_cause = GAME_RULES_MOVEMENT_SLIDE;
        }
    }
    if (*event_count == 0U) return 0;
    qsort(next->entities, next->entity_count, sizeof(game_rules_entity), entity_compare);
    return 1;
}

static int offset_height(int32_t height, int32_t offset, int32_t* result)
{
    int64_t value = (int64_t)height + offset;
    if (value < INT32_MIN || value > INT32_MAX) return 0;
    *result = (int32_t)value;
    return 1;
}

static int adjacent_target_bottom(const game_rules_cell* source,
                                  int32_t source_bottom,
                                  const game_rules_cell* target,
                                  uint32_t direction,
                                  int32_t* result)
{
    int source_ramp = source->kind == GAME_RULES_CELL_RAMP;
    int target_ramp = target->kind == GAME_RULES_CELL_RAMP;
    int32_t bottom = source_bottom;
    if (!source_ramp && !target_ramp) {
        *result = bottom;
        return 1;
    }
    if (source_ramp && target_ramp) {
        if (ramps_align_laterally(source, target, direction)) {
            *result = bottom;
            return 1;
        }
        if (!ramps_connect(source, target, direction)) return 0;
    }
    if (source_ramp) {
        if (direction == source->low_direction) {
            if (!offset_height(bottom, -1, &bottom)) return 0;
        } else if (direction == opposite(source->low_direction)) {
            if (!offset_height(bottom, 1, &bottom)) return 0;
        } else return 0;
    }
    if (target_ramp) {
        uint32_t target_to_source = opposite(direction);
        if (target_to_source == target->low_direction) {
            if (!offset_height(bottom, 1, &bottom)) return 0;
        } else if (target_to_source == opposite(target->low_direction)) {
            if (!offset_height(bottom, -1, &bottom)) return 0;
        } else return 0;
    }
    *result = bottom;
    return 1;
}

static int source_id_contains(const game_rules_session* session,
                              uint32_t source_count,
                              uint64_t id)
{
    return bsearch(&id, session->scratch_source_ids, source_count,
                   sizeof(uint64_t), u64_compare) != NULL;
}

static game_rules_c_blast_target* find_or_add_target(game_rules_session* session,
                                                      uint32_t* target_count,
                                                      const game_rules_entity* entity)
{
    uint32_t index;
    game_rules_c_blast_target* target;
    for (index = 0U; index < *target_count; ++index) {
        if (session->scratch_targets[index].id == entity->id)
            return &session->scratch_targets[index];
    }
    if (*target_count >= session->current_state.entity_capacity) return NULL;
    target = &session->scratch_targets[*target_count];
    ++*target_count;
    memset(target, 0, sizeof(*target));
    target->id = entity->id;
    target->kind = entity->kind;
    target->coordinate = entity->coordinate;
    target->bottom_half_steps = entity->bottom_half_steps;
    return target;
}

static int can_pop(const game_rules_session* session,
                   const game_rules_c_state* state,
                   const game_rules_c_blast_target* target,
                   game_rules_coordinate destination,
                   uint32_t direction)
{
    const game_rules_cell* source;
    const game_rules_cell* destination_cell;
    uint32_t ignored;
    if (cell_index(session, destination) == GAME_RULES_C_NO_INDEX ||
        destination_fixture_blocks(session, state, destination) ||
        !volume_is_clear(state, destination, target->bottom_half_steps)) return 0;
    source = find_cell(session, target->coordinate);
    destination_cell = find_cell(session, destination);
    if (source == NULL || destination_cell == NULL) return 0;
    if (source->kind == GAME_RULES_CELL_RAMP &&
        !ramp_endpoint(source, direction, &ignored)) {
        if (destination_cell->kind != GAME_RULES_CELL_RAMP ||
            !ramps_align_laterally(source, destination_cell, direction)) return 0;
    }
    return support_half_steps(destination_cell) <= target->bottom_half_steps;
}

static int target_volumes_overlap(const game_rules_c_blast_target* left,
                                  const game_rules_c_blast_target* right)
{
    int64_t left_bottom;
    int64_t right_bottom;
    if (!same_coordinate(left->destination, right->destination)) return 0;
    left_bottom = left->bottom_half_steps;
    right_bottom = right->bottom_half_steps;
    return left_bottom < right_bottom + 2 && right_bottom < left_bottom + 2;
}

static game_rules_entity* find_entity_by_id(game_rules_c_state* state, uint64_t id)
{
    uint32_t index;
    for (index = 0U; index < state->entity_count; ++index) {
        if (state->entities[index].id == id) return &state->entities[index];
    }
    return NULL;
}

static int resolve_explosions(game_rules_session* session, uint32_t* event_count)
{
    static const uint32_t directions[4] = {
        GAME_RULES_DIRECTION_NORTH, GAME_RULES_DIRECTION_EAST,
        GAME_RULES_DIRECTION_SOUTH, GAME_RULES_DIRECTION_WEST};
    const game_rules_c_state* current = &session->current_state;
    game_rules_c_state* next = &session->scratch_state;
    uint32_t source_count = 0U;
    uint32_t target_count = 0U;
    uint32_t entity_index;
    uint32_t target_index;
    int lost = 0;
    *event_count = 0U;
    if (current->outcome != GAME_RULES_OUTCOME_ONGOING || !state_copy(next, current)) return 0;
    for (entity_index = 0U; entity_index < current->entity_count; ++entity_index) {
        const game_rules_entity* entity = &current->entities[entity_index];
        if (entity->kind == GAME_RULES_ENTITY_BARREL &&
            state_contains_id(current, entity->id)) {
            session->scratch_source_ids[source_count++] = entity->id;
        }
    }
    if (source_count == 0U) return 0;
    qsort(session->scratch_source_ids, source_count, sizeof(uint64_t), u64_compare);
    for (entity_index = 0U; entity_index < current->entity_count; ++entity_index) {
        const game_rules_entity* source = &current->entities[entity_index];
        const game_rules_cell* source_cell;
        uint32_t other;
        uint32_t direction_index;
        if (!source_id_contains(session, source_count, source->id)) continue;
        source_cell = find_cell(session, source->coordinate);
        if (source_cell == NULL) continue;
        for (other = 0U; other < current->entity_count; ++other) {
            const game_rules_entity* entity = &current->entities[other];
            int64_t difference;
            if (source_id_contains(session, source_count, entity->id) ||
                !same_coordinate(entity->coordinate, source->coordinate)) continue;
            difference = (int64_t)entity->bottom_half_steps - source->bottom_half_steps;
            if ((difference == -2 || difference == 2) &&
                find_or_add_target(session, &target_count, entity) == NULL) return -1;
        }
        for (direction_index = 0U; direction_index < 4U; ++direction_index) {
            game_rules_coordinate coordinate;
            const game_rules_cell* target_cell;
            int32_t target_bottom;
            if (!step(session, source->coordinate, directions[direction_index], &coordinate))
                continue;
            target_cell = find_cell(session, coordinate);
            if (target_cell == NULL || !adjacent_target_bottom(source_cell,
                source->bottom_half_steps, target_cell, directions[direction_index],
                &target_bottom)) continue;
            for (other = 0U; other < current->entity_count; ++other) {
                const game_rules_entity* entity = &current->entities[other];
                if (same_coordinate(entity->coordinate, coordinate) &&
                    entity->bottom_half_steps == target_bottom &&
                    !source_id_contains(session, source_count, entity->id)) {
                    game_rules_c_blast_target* target =
                        find_or_add_target(session, &target_count, entity);
                    if (target == NULL) return -1;
                    target->impulses |= 1U << direction_index;
                    break;
                }
            }
        }
    }
    qsort(session->scratch_targets, target_count,
          sizeof(game_rules_c_blast_target), target_compare);
    for (target_index = 0U; target_index < target_count; ++target_index) {
        game_rules_c_blast_target* target = &session->scratch_targets[target_index];
        uint32_t count = 0U;
        uint32_t direction_index;
        for (direction_index = 0U; direction_index < 4U; ++direction_index) {
            if (target->impulses & (1U << direction_index)) ++count;
        }
        if (target->kind == GAME_RULES_ENTITY_PLAYER || count != 1U) continue;
        for (direction_index = 0U; direction_index < 4U; ++direction_index) {
            if (target->impulses & (1U << direction_index)) break;
        }
        target->has_destination = step(session, target->coordinate,
            directions[direction_index], &target->destination) ? 1U : 0U;
        target->movement_is_valid = target->has_destination && can_pop(
            session, current, target, target->destination, directions[direction_index]);
    }
    for (target_index = 0U; target_index < target_count; ++target_index) {
        uint32_t other;
        if (!session->scratch_targets[target_index].movement_is_valid) continue;
        for (other = target_index + 1U; other < target_count; ++other) {
            if (session->scratch_targets[other].movement_is_valid && target_volumes_overlap(
                &session->scratch_targets[target_index], &session->scratch_targets[other])) {
                session->scratch_targets[target_index].destination_conflicts = 1U;
                session->scratch_targets[other].destination_conflicts = 1U;
            }
        }
    }
    {
        uint32_t write = 0U;
        for (entity_index = 0U; entity_index < next->entity_count; ++entity_index) {
            if (!source_id_contains(session, source_count, next->entities[entity_index].id)) {
                next->entities[write++] = next->entities[entity_index];
            }
        }
        next->entity_count = write;
        write = 0U;
        for (entity_index = 0U; entity_index < next->armed_barrel_count; ++entity_index) {
            if (!source_id_contains(session, source_count,
                                    next->armed_barrel_ids[entity_index])) {
                next->armed_barrel_ids[write++] = next->armed_barrel_ids[entity_index];
            }
        }
        next->armed_barrel_count = write;
    }
    for (entity_index = 0U; entity_index < current->entity_count; ++entity_index) {
        const game_rules_entity* source = &current->entities[entity_index];
        if (source_id_contains(session, source_count, source->id)) {
            game_rules_event* event = add_event(
                session, event_count, GAME_RULES_EVENT_BARREL_EXPLODED);
            if (event == NULL) return -1;
            event->entity_id = source->id;
            event->coordinate = source->coordinate;
            event->bottom_half_steps = source->bottom_half_steps;
        }
    }
    for (target_index = 0U; target_index < target_count; ++target_index) {
        const game_rules_c_blast_target* target = &session->scratch_targets[target_index];
        if (target->kind == GAME_RULES_ENTITY_PLAYER) {
            lost = 1;
            continue;
        }
        if (target->movement_is_valid && !target->destination_conflicts) {
            game_rules_entity* entity = find_entity_by_id(next, target->id);
            if (entity != NULL) {
                game_rules_event* event;
                entity->coordinate = target->destination;
                event = add_event(session, event_count, GAME_RULES_EVENT_ENTITY_MOVED);
                if (event == NULL) return -1;
                event->entity_id = target->id;
                event->from = target->coordinate;
                event->to = target->destination;
                event->old_bottom_half_steps = target->bottom_half_steps;
                event->new_bottom_half_steps = target->bottom_half_steps;
                event->movement_cause = GAME_RULES_MOVEMENT_BLAST;
            }
        }
        if (target->kind == GAME_RULES_ENTITY_BARREL &&
            !state_contains_id(current, target->id)) {
            game_rules_event* event;
            if (!arm_barrel(next, target->id)) return -1;
            event = add_event(session, event_count, GAME_RULES_EVENT_BARREL_ARMED);
            if (event == NULL) return -1;
            event->entity_id = target->id;
        }
    }
    if (lost) {
        next->outcome = GAME_RULES_OUTCOME_LOST;
        if (add_event(session, event_count, GAME_RULES_EVENT_LEVEL_LOST) == NULL) return -1;
    }
    qsort(next->entities, next->entity_count, sizeof(game_rules_entity), entity_compare);
    return 1;
}

static int resolve_initialization(game_rules_session* session,
                                  const game_rules_c_allocator* allocator)
{
    if (!resolve_initial_fixture_tick(session, allocator)) return 0;
    /* This is the frozen reference order: fall, otherwise slide, otherwise explode. */
    while (session->current_state.outcome == GAME_RULES_OUTCOME_ONGOING) {
        uint32_t event_count = 0U;
        int resolved = game_rules_c_resolve_falling_tick(session, &event_count);
        if (resolved == 0) resolved = resolve_sliding(session, &event_count);
        if (resolved == 0) resolved = resolve_explosions(session, &event_count);
        if (resolved < 0) return 0;
        if (resolved == 0) break;
        if (!resolve_fixtures_after_physical(session, &session->current_state,
                                              &session->scratch_state, &event_count) ||
            !append_tick(session, allocator, event_count, &session->scratch_state)) return 0;
        swap_current_and_scratch(session);
    }
    return 1;
}

static int allocate_state_arrays(state_layout* storage,
                                 game_rules_c_state* state,
                                 uint32_t entity_capacity,
                                 uint32_t fixture_capacity,
                                 int derived)
{
    memset(state, 0, sizeof(*state));
    state->entities = (game_rules_entity*)take(storage, entity_capacity,
        sizeof(game_rules_entity), _Alignof(game_rules_entity));
    state->entity_capacity = entity_capacity;
    if (derived) {
        state->armed_barrel_ids = (uint64_t*)take(storage, entity_capacity,
            sizeof(uint64_t), _Alignof(uint64_t));
        state->active_switch_colors = (uint32_t*)take(storage, 4U,
            sizeof(uint32_t), _Alignof(uint32_t));
        state->open_doors = (game_rules_coordinate*)take(storage, fixture_capacity,
            sizeof(game_rules_coordinate), _Alignof(game_rules_coordinate));
        state->armed_barrel_capacity = entity_capacity;
        state->active_switch_color_capacity = 4U;
        state->open_door_capacity = fixture_capacity;
    }
    state->outcome = GAME_RULES_OUTCOME_ONGOING;
    return !storage->failed;
}

static void destroy_command_plan(game_rules_c_command_plan* plan)
{
    uint32_t index;
    if (plan == NULL) return;
    for (index = 0U; index < plan->working.initialization_tick_count; ++index) {
        game_rules_c_deallocate_owned(
            plan->working.initialization_ticks[index].owned_storage);
    }
    game_rules_c_deallocate_owned(plan->working.initialization_ticks);
    game_rules_c_deallocate_owned(plan->state_storage);
    game_rules_c_deallocate_owned(plan);
}

uint32_t game_rules_c_plan_resolved_command(
    game_rules_session* session,
    const game_rules_c_allocator* allocator,
    uint32_t direction,
    game_rules_c_command_transaction* transaction)
{
    state_layout measure = {0};
    state_layout storage = {0};
    game_rules_c_state ignored;
    game_rules_c_command_plan* plan;
    uint32_t event_count;

    game_rules_c_plan_player_move(session, direction, transaction);
    if (!transaction->accepted) return GAME_RULES_CALL_OK;
    if (session->history_count == UINT32_MAX) return GAME_RULES_CALL_ALLOCATION_FAILED;

    transaction->prepared_history = create_history_entry(allocator,
                                                          &session->current_state);
    if (transaction->prepared_history == NULL) {
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }

    allocate_state_arrays(&measure, &ignored,
                          session->current_state.entity_capacity,
                          session->fixture_count, 1);
    allocate_state_arrays(&measure, &ignored,
                          session->current_state.entity_capacity,
                          session->fixture_count, 1);
    if (measure.failed) return GAME_RULES_CALL_ALLOCATION_FAILED;

    plan = (game_rules_c_command_plan*)game_rules_c_allocate_owned(
        allocator, sizeof(*plan));
    if (plan == NULL) return GAME_RULES_CALL_ALLOCATION_FAILED;
    memset(plan, 0, sizeof(*plan));
    plan->state_storage = game_rules_c_allocate_owned(allocator, measure.offset);
    if (plan->state_storage == NULL) {
        destroy_command_plan(plan);
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }

    plan->working = *session;
    memset(&plan->working.current_state, 0, sizeof(plan->working.current_state));
    memset(&plan->working.scratch_state, 0, sizeof(plan->working.scratch_state));
    plan->working.initialization_ticks = NULL;
    plan->working.initialization_tick_count = 0U;
    plan->working.initialization_tick_capacity = 0U;
    storage.base = (unsigned char*)plan->state_storage;
    storage.capacity = measure.offset;
    allocate_state_arrays(&storage, &plan->working.current_state,
                          session->current_state.entity_capacity,
                          session->fixture_count, 1);
    allocate_state_arrays(&storage, &plan->working.scratch_state,
                          session->current_state.entity_capacity,
                          session->fixture_count, 1);
    if (storage.failed ||
        !state_copy(&plan->working.current_state, &session->current_state) ||
        !state_copy(&plan->working.scratch_state, &session->scratch_state)) {
        destroy_command_plan(plan);
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }

    event_count = transaction->tick_event_count;
    if (!resolve_fixtures_after_physical(
            &plan->working, &plan->working.current_state,
            &plan->working.scratch_state, &event_count) ||
        !append_tick(&plan->working, allocator, event_count,
                     &plan->working.scratch_state)) {
        destroy_command_plan(plan);
        return GAME_RULES_CALL_ALLOCATION_FAILED;
    }
    swap_current_and_scratch(&plan->working);

    while (plan->working.current_state.outcome == GAME_RULES_OUTCOME_ONGOING) {
        int resolved;
        event_count = 0U;
        resolved = game_rules_c_resolve_falling_tick(&plan->working, &event_count);
        if (resolved == 0)
            resolved = resolve_sliding(&plan->working, &event_count);
        if (resolved == 0)
            resolved = resolve_explosions(&plan->working, &event_count);
        if (resolved < 0) {
            destroy_command_plan(plan);
            return GAME_RULES_CALL_ALLOCATION_FAILED;
        }
        if (resolved == 0) break;
        if (!resolve_fixtures_after_physical(
                &plan->working, &plan->working.current_state,
                &plan->working.scratch_state, &event_count) ||
            !append_tick(&plan->working, allocator, event_count,
                         &plan->working.scratch_state)) {
            destroy_command_plan(plan);
            return GAME_RULES_CALL_ALLOCATION_FAILED;
        }
        swap_current_and_scratch(&plan->working);
    }

    transaction->initial_state = &session->current_state;
    transaction->final_state = &plan->working.current_state;
    transaction->ticks = plan->working.initialization_ticks;
    transaction->tick_count = plan->working.initialization_tick_count;
    transaction->owned_plan = plan;
    return GAME_RULES_CALL_OK;
}

int game_rules_c_commit_command(game_rules_session* session,
                                game_rules_c_command_transaction* transaction)
{
    if (session != NULL && transaction != NULL && transaction->accepted &&
        transaction->initial_state == &session->current_state &&
        transaction->final_state != NULL &&
        transaction->prepared_history != NULL &&
        session->history_count != UINT32_MAX &&
        state_copy(&session->scratch_state, transaction->final_state)) {
        transaction->prepared_history->previous = session->history_top;
        session->history_top = transaction->prepared_history;
        transaction->prepared_history = NULL;
        ++session->history_count;
        swap_current_and_scratch(session);
        return 1;
    }
    return 0;
}

int game_rules_c_commit_rewind(game_rules_session* session)
{
    game_rules_c_history_entry* restored;
    if (session == NULL || session->history_top == NULL ||
        session->history_count == 0U) return 0;
    restored = session->history_top;
    if (!state_copy(&session->scratch_state, &restored->state)) return 0;
    swap_current_and_scratch(session);
    session->history_top = restored->previous;
    --session->history_count;
    game_rules_c_deallocate_owned(restored);
    return 1;
}

void game_rules_c_command_transaction_destroy(
    game_rules_c_command_transaction* transaction)
{
    if (transaction == NULL) return;
    destroy_command_plan((game_rules_c_command_plan*)transaction->owned_plan);
    game_rules_c_deallocate_owned(transaction->prepared_history);
    memset(transaction, 0, sizeof(*transaction));
}

game_rules_session* game_rules_c_build_resolved_session(
    const game_rules_c_allocator* allocator,
    const game_rules_c_level_view* level)
{
    state_layout measure = {0};
    state_layout storage = {0};
    game_rules_session* session;
    game_rules_c_owned_level canonical = {0};
    size_t event_capacity_size;
    uint32_t event_capacity;
    uint32_t index;
    if (!multiply_size(level->entity_count, 3U, &event_capacity_size) ||
        !add_size(event_capacity_size, level->fixture_count, &event_capacity_size) ||
        !add_size(event_capacity_size, 6U, &event_capacity_size) ||
        event_capacity_size > UINT32_MAX) return NULL;
    event_capacity = (uint32_t)event_capacity_size;
    take(&measure, level->cell_count, sizeof(game_rules_cell), _Alignof(game_rules_cell));
    take(&measure, level->fixture_count, sizeof(game_rules_fixture), _Alignof(game_rules_fixture));
    take(&measure, level->cell_count, sizeof(uint32_t), _Alignof(uint32_t));
    {
        game_rules_c_state ignored;
        allocate_state_arrays(&measure, &ignored, level->entity_count,
                              level->fixture_count, 0);
        allocate_state_arrays(&measure, &ignored, level->entity_count,
                              level->fixture_count, 1);
        allocate_state_arrays(&measure, &ignored, level->entity_count,
                              level->fixture_count, 1);
    }
    take(&measure, event_capacity, sizeof(game_rules_event), _Alignof(game_rules_event));
    take(&measure, (size_t)level->entity_count + 1U, sizeof(game_rules_event),
         _Alignof(game_rules_event));
    take(&measure, level->cell_count, sizeof(game_rules_c_slide_candidate),
         _Alignof(game_rules_c_slide_candidate));
    take(&measure, level->entity_count, sizeof(game_rules_c_blast_target),
         _Alignof(game_rules_c_blast_target));
    take(&measure, level->entity_count, sizeof(uint64_t), _Alignof(uint64_t));
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
    storage.base = (unsigned char*)session->level_storage;
    storage.capacity = measure.offset;
    session->cells = (game_rules_cell*)take(&storage, level->cell_count,
        sizeof(game_rules_cell), _Alignof(game_rules_cell));
    session->fixtures = (game_rules_fixture*)take(&storage, level->fixture_count,
        sizeof(game_rules_fixture), _Alignof(game_rules_fixture));
    session->fixture_index_by_cell = (uint32_t*)take(&storage, level->cell_count,
        sizeof(uint32_t), _Alignof(uint32_t));
    allocate_state_arrays(&storage, &session->initial_state, level->entity_count,
                          level->fixture_count, 0);
    allocate_state_arrays(&storage, &session->current_state, level->entity_count,
                          level->fixture_count, 1);
    allocate_state_arrays(&storage, &session->scratch_state, level->entity_count,
                          level->fixture_count, 1);
    session->scratch_events = (game_rules_event*)take(&storage, event_capacity,
        sizeof(game_rules_event), _Alignof(game_rules_event));
    session->scratch_event_capacity = event_capacity;
    session->scratch_terminal_events = (game_rules_event*)take(&storage,
        (size_t)level->entity_count + 1U, sizeof(game_rules_event),
        _Alignof(game_rules_event));
    session->scratch_terminal_event_capacity = level->entity_count + 1U;
    session->scratch_slides = (game_rules_c_slide_candidate*)take(&storage,
        level->cell_count, sizeof(game_rules_c_slide_candidate),
        _Alignof(game_rules_c_slide_candidate));
    session->scratch_targets = (game_rules_c_blast_target*)take(&storage,
        level->entity_count, sizeof(game_rules_c_blast_target),
        _Alignof(game_rules_c_blast_target));
    session->scratch_source_ids = (uint64_t*)take(&storage, level->entity_count,
        sizeof(uint64_t), _Alignof(uint64_t));
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
    if (level->cell_count) memcpy(session->cells, level->cells,
                                  level->cell_count * sizeof(game_rules_cell));
    if (level->fixture_count) memcpy(session->fixtures, level->fixtures,
                                     level->fixture_count * sizeof(game_rules_fixture));
    if (level->entity_count) memcpy(session->initial_state.entities, level->entities,
                                    level->entity_count * sizeof(game_rules_entity));
    canonical.cells = session->cells;
    canonical.fixtures = session->fixtures;
    canonical.entities = session->initial_state.entities;
    canonical.view.cell_count = level->cell_count;
    canonical.view.fixture_count = level->fixture_count;
    canonical.view.entity_count = level->entity_count;
    game_rules_c_canonicalize_level(&canonical);
    session->initial_state.entity_count = level->entity_count;
    if (!state_copy(&session->current_state, &session->initial_state)) {
        game_rules_c_destroy_session(session);
        return NULL;
    }
    for (index = 0U; index < session->cell_count; ++index)
        session->fixture_index_by_cell[index] = GAME_RULES_C_NO_INDEX;
    for (index = 0U; index < session->fixture_count; ++index) {
        uint32_t at = cell_index(session, session->fixtures[index].coordinate);
        if (at != GAME_RULES_C_NO_INDEX) session->fixture_index_by_cell[at] = index;
    }
    if (!resolve_initialization(session, allocator)) {
        game_rules_c_destroy_session(session);
        return NULL;
    }
    return session;
}
