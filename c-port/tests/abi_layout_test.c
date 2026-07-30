#include "game_rules/c_allocator_api.h"
#include "game_rules/c_api.h"

#include <stddef.h>
#include <stdint.h>

#define ASSERT_FIELD(type, field, expected_offset, expected_size) \
    _Static_assert(offsetof(type, field) == (expected_offset), "ABI offset drift: " #type "." #field); \
    _Static_assert(sizeof(((type*)0)->field) == (expected_size), "ABI width drift: " #type "." #field)

#define ASSERT_LAYOUT(type, expected_size, expected_alignment) \
    _Static_assert(sizeof(type) == (expected_size), "ABI size drift: " #type); \
    _Static_assert(_Alignof(type) == (expected_alignment), "ABI alignment drift: " #type)

_Static_assert(sizeof(uint32_t) == 4U, "the data ABI requires 32-bit uint32_t");
_Static_assert(sizeof(int32_t) == 4U, "the data ABI requires 32-bit int32_t");
_Static_assert(sizeof(uint64_t) == 8U, "the data ABI requires 64-bit uint64_t");
_Static_assert(sizeof(GAME_RULES_DIRECTION_NORTH) == sizeof(int),
               "public enum constants must retain the C int representation");
_Static_assert(sizeof(GAME_RULES_CALL_OK) == sizeof(int),
               "call-status enum constants must retain the C int representation");
_Static_assert(sizeof(GAME_RULES_EVENT_MOVE_BLOCKED) == sizeof(int),
               "event enum constants must retain the C int representation");

_Static_assert(GAME_RULES_DIRECTION_NORTH == 0, "direction enum drift");
_Static_assert(GAME_RULES_DIRECTION_WEST == 3, "direction enum drift");
_Static_assert(GAME_RULES_CALL_ALLOCATION_FAILED == 3, "call enum drift");
_Static_assert(GAME_RULES_MOVE_LEVEL_TERMINAL == 10, "move enum drift");
_Static_assert(GAME_RULES_VALIDATION_INVALID_ENTITY_ID == 20, "validation enum drift");
_Static_assert(GAME_RULES_EVENT_LEVEL_LOST == 10, "event enum drift");
_Static_assert(GAME_RULES_MOVEMENT_SLIDE == 3, "movement enum drift");

ASSERT_LAYOUT(game_rules_coordinate, 8U, 4U);
ASSERT_FIELD(game_rules_coordinate, x, 0U, 4U);
ASSERT_FIELD(game_rules_coordinate, y, 4U, 4U);
ASSERT_LAYOUT(game_rules_coordinate_system, 16U, 4U);
ASSERT_FIELD(game_rules_coordinate_system, origin, 0U, 8U);
ASSERT_FIELD(game_rules_coordinate_system, positive_x, 8U, 4U);
ASSERT_FIELD(game_rules_coordinate_system, positive_y, 12U, 4U);
ASSERT_LAYOUT(game_rules_cell, 20U, 4U);
ASSERT_FIELD(game_rules_cell, coordinate, 0U, 8U);
ASSERT_FIELD(game_rules_cell, kind, 8U, 4U);
ASSERT_FIELD(game_rules_cell, elevation, 12U, 4U);
ASSERT_FIELD(game_rules_cell, low_direction, 16U, 4U);
ASSERT_LAYOUT(game_rules_fixture, 16U, 4U);
ASSERT_FIELD(game_rules_fixture, coordinate, 0U, 8U);
ASSERT_FIELD(game_rules_fixture, kind, 8U, 4U);
ASSERT_FIELD(game_rules_fixture, color, 12U, 4U);
ASSERT_FIELD(game_rules_entity, id, 0U, 8U);
ASSERT_FIELD(game_rules_entity, kind, 8U, 4U);
ASSERT_FIELD(game_rules_entity, coordinate, 12U, 8U);
ASSERT_FIELD(game_rules_entity, bottom_half_steps, 20U, 4U);
ASSERT_FIELD(game_rules_validation_error, code, 0U, 4U);
ASSERT_FIELD(game_rules_validation_error, coordinate, 4U, 8U);
ASSERT_FIELD(game_rules_event, kind, 0U, 4U);
ASSERT_FIELD(game_rules_event, direction, 4U, 4U);
ASSERT_FIELD(game_rules_event, move_status, 8U, 4U);

#if UINTPTR_MAX == UINT64_MAX
ASSERT_LAYOUT(game_rules_entity, 24U, 8U);
ASSERT_LAYOUT(game_rules_validation_error, 24U, 8U);
ASSERT_FIELD(game_rules_validation_error, entity_id, 16U, 8U);
ASSERT_LAYOUT(game_rules_event, 80U, 8U);
ASSERT_FIELD(game_rules_event, entity_id, 16U, 8U);
ASSERT_FIELD(game_rules_event, other_entity_id, 24U, 8U);
ASSERT_FIELD(game_rules_event, from, 32U, 8U);
ASSERT_FIELD(game_rules_event, to, 40U, 8U);
ASSERT_FIELD(game_rules_event, coordinate, 48U, 8U);
ASSERT_FIELD(game_rules_event, old_bottom_half_steps, 56U, 4U);
ASSERT_FIELD(game_rules_event, new_bottom_half_steps, 60U, 4U);
ASSERT_FIELD(game_rules_event, bottom_half_steps, 64U, 4U);
ASSERT_FIELD(game_rules_event, movement_cause, 68U, 4U);
ASSERT_FIELD(game_rules_event, color, 72U, 4U);
ASSERT_FIELD(game_rules_event, active, 76U, 4U);
ASSERT_LAYOUT(game_rules_level_definition, 72U, 8U);
ASSERT_FIELD(game_rules_level_definition, cells, 24U, 8U);
ASSERT_FIELD(game_rules_level_definition, cell_count, 32U, 4U);
ASSERT_FIELD(game_rules_level_definition, fixtures, 40U, 8U);
ASSERT_FIELD(game_rules_level_definition, fixture_count, 48U, 4U);
ASSERT_FIELD(game_rules_level_definition, entities, 56U, 8U);
ASSERT_FIELD(game_rules_level_definition, entity_count, 64U, 4U);
ASSERT_LAYOUT(game_rules_resolved_state, 64U, 8U);
ASSERT_FIELD(game_rules_resolved_state, entities, 0U, 8U);
ASSERT_FIELD(game_rules_resolved_state, entity_count, 8U, 4U);
ASSERT_FIELD(game_rules_resolved_state, armed_barrel_ids, 16U, 8U);
ASSERT_FIELD(game_rules_resolved_state, armed_barrel_count, 24U, 4U);
ASSERT_FIELD(game_rules_resolved_state, active_switch_colors, 32U, 8U);
ASSERT_FIELD(game_rules_resolved_state, active_switch_color_count, 40U, 4U);
ASSERT_FIELD(game_rules_resolved_state, open_doors, 48U, 8U);
ASSERT_FIELD(game_rules_resolved_state, open_door_count, 56U, 4U);
ASSERT_FIELD(game_rules_resolved_state, outcome, 60U, 4U);
ASSERT_LAYOUT(game_rules_level, 56U, 8U);
ASSERT_LAYOUT(game_rules_snapshot, 120U, 8U);
ASSERT_FIELD(game_rules_snapshot, level, 0U, 56U);
ASSERT_FIELD(game_rules_snapshot, resolved, 56U, 64U);
ASSERT_LAYOUT(game_rules_tick, 88U, 8U);
ASSERT_FIELD(game_rules_tick, index, 0U, 4U);
ASSERT_FIELD(game_rules_tick, events, 8U, 8U);
ASSERT_FIELD(game_rules_tick, event_count, 16U, 4U);
ASSERT_FIELD(game_rules_tick, state_after, 24U, 64U);
ASSERT_LAYOUT(game_rules_state_result, 136U, 8U);
ASSERT_FIELD(game_rules_state_result, state, 8U, 120U);
ASSERT_FIELD(game_rules_state_result, owned_storage, 128U, 8U);
ASSERT_LAYOUT(game_rules_load_result, 312U, 8U);
ASSERT_FIELD(game_rules_load_result, initial_state, 24U, 64U);
ASSERT_FIELD(game_rules_load_result, ticks, 88U, 8U);
ASSERT_FIELD(game_rules_load_result, final_state, 104U, 64U);
ASSERT_FIELD(game_rules_load_result, state, 176U, 120U);
ASSERT_FIELD(game_rules_load_result, owned_storage, 304U, 8U);
ASSERT_LAYOUT(game_rules_move_result, 320U, 8U);
ASSERT_FIELD(game_rules_move_result, events, 16U, 8U);
ASSERT_FIELD(game_rules_move_result, initial_state, 32U, 64U);
ASSERT_FIELD(game_rules_move_result, ticks, 96U, 8U);
ASSERT_FIELD(game_rules_move_result, final_state, 112U, 64U);
ASSERT_FIELD(game_rules_move_result, state, 184U, 120U);
ASSERT_FIELD(game_rules_move_result, owned_storage, 312U, 8U);
ASSERT_LAYOUT(game_rules_rewind_result, 232U, 8U);
ASSERT_FIELD(game_rules_rewind_result, events, 8U, 8U);
ASSERT_FIELD(game_rules_rewind_result, restored_state, 24U, 64U);
ASSERT_FIELD(game_rules_rewind_result, state, 96U, 120U);
ASSERT_FIELD(game_rules_rewind_result, owned_storage, 224U, 8U);
ASSERT_LAYOUT(game_rules_allocator_v1, 32U, 8U);
ASSERT_FIELD(game_rules_allocator_v1, context, 8U, 8U);
ASSERT_FIELD(game_rules_allocator_v1, allocate, 16U, 8U);
ASSERT_FIELD(game_rules_allocator_v1, deallocate, 24U, 8U);
#elif defined(__wasm32__)
ASSERT_LAYOUT(game_rules_entity, 24U, 8U);
ASSERT_LAYOUT(game_rules_validation_error, 24U, 8U);
ASSERT_FIELD(game_rules_validation_error, entity_id, 16U, 8U);
ASSERT_LAYOUT(game_rules_event, 80U, 8U);
ASSERT_FIELD(game_rules_event, entity_id, 16U, 8U);
ASSERT_FIELD(game_rules_event, active, 76U, 4U);
ASSERT_LAYOUT(game_rules_level_definition, 48U, 4U);
ASSERT_LAYOUT(game_rules_resolved_state, 36U, 4U);
ASSERT_LAYOUT(game_rules_level, 40U, 4U);
ASSERT_LAYOUT(game_rules_snapshot, 76U, 4U);
ASSERT_LAYOUT(game_rules_tick, 48U, 4U);
ASSERT_LAYOUT(game_rules_state_result, 84U, 4U);
ASSERT_LAYOUT(game_rules_load_result, 196U, 4U);
ASSERT_LAYOUT(game_rules_move_result, 204U, 4U);
ASSERT_LAYOUT(game_rules_rewind_result, 148U, 4U);
ASSERT_LAYOUT(game_rules_allocator_v1, 20U, 4U);
#else
_Static_assert(sizeof(void*) == 4U, "unsupported pointer model");
_Static_assert(offsetof(game_rules_level_definition, cells) == 24U,
               "32-bit level-definition prefix drift");
_Static_assert(sizeof(((game_rules_level_definition*)0)->cell_count) == 4U,
               "32-bit count width drift");
#endif

typedef uint32_t (*version_fn)(void);
typedef const char* (*status_fn)(void);
typedef game_rules_engine* (*create_fn)(void);
typedef void (*destroy_fn)(game_rules_engine*);
typedef char* (*load_json_fn)(game_rules_engine*, const char*, uint32_t);
typedef uint32_t (*load_data_fn)(game_rules_engine*, const game_rules_level_definition*,
                                 game_rules_load_result*);

static version_fn const api_version_pointer = &game_rules_api_version;
static status_fn const status_pointer = &game_rules_engine_status;
static create_fn const create_pointer = &game_rules_engine_create;
static destroy_fn const destroy_pointer = &game_rules_engine_destroy;
static load_json_fn const load_json_pointer = &game_rules_engine_load_level;
static load_data_fn const load_data_pointer = &game_rules_engine_load_level_data;

int main(void)
{
    game_rules_engine* engine;
    if (api_version_pointer() != 1U || status_pointer() == NULL ||
        load_json_pointer == NULL || load_data_pointer == NULL) {
        return 1;
    }
    engine = create_pointer();
    destroy_pointer(engine);
    return engine == NULL ? 1 : 0;
}
