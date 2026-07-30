#include "game_rules/c_allocator_api.h"
#include "game_rules/c_api.h"

#include <cstdint>
#include <type_traits>

static_assert(std::is_standard_layout_v<game_rules_coordinate>);
static_assert(std::is_standard_layout_v<game_rules_event>);
static_assert(std::is_standard_layout_v<game_rules_load_result>);
static_assert(std::is_same_v<decltype(&game_rules_api_version), std::uint32_t (*)(void)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_create), game_rules_engine* (*)(void)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_destroy),
                             void (*)(game_rules_engine*)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_status), const char* (*)(void)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_load_level),
                             char* (*)(game_rules_engine*, const char*, std::uint32_t)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_get_state),
                             char* (*)(game_rules_engine*)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_move),
                             char* (*)(game_rules_engine*, std::uint32_t)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_rewind),
                             char* (*)(game_rules_engine*)>);
static_assert(std::is_same_v<decltype(&game_rules_string_free), void (*)(char*)>);
static_assert(std::is_same_v<decltype(&game_rules_data_api_version),
                             std::uint32_t (*)(void)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_get_state_data),
                             std::uint32_t (*)(const game_rules_engine*,
                                               game_rules_state_result*)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_load_level_data),
                             std::uint32_t (*)(game_rules_engine*,
                                               const game_rules_level_definition*,
                                               game_rules_load_result*)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_move_data),
                             std::uint32_t (*)(game_rules_engine*, std::uint32_t,
                                               game_rules_move_result*)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_rewind_data),
                             std::uint32_t (*)(game_rules_engine*,
                                               game_rules_rewind_result*)>);
static_assert(std::is_same_v<decltype(&game_rules_state_result_dispose),
                             void (*)(game_rules_state_result*)>);
static_assert(std::is_same_v<decltype(&game_rules_load_result_dispose),
                             void (*)(game_rules_load_result*)>);
static_assert(std::is_same_v<decltype(&game_rules_move_result_dispose),
                             void (*)(game_rules_move_result*)>);
static_assert(std::is_same_v<decltype(&game_rules_rewind_result_dispose),
                             void (*)(game_rules_rewind_result*)>);
static_assert(std::is_same_v<decltype(&game_rules_allocator_api_version),
                             std::uint32_t (*)(void)>);
static_assert(std::is_same_v<decltype(&game_rules_engine_create_with_allocator_v1),
                             game_rules_engine* (*)(const game_rules_allocator_v1*)>);

int main()
{
    game_rules_engine* const engine = game_rules_engine_create();
    game_rules_engine_destroy(engine);
    return engine == nullptr ? 1 : 0;
}
