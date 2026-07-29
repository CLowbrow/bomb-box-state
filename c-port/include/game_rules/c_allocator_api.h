#pragma once

#include "game_rules/c_api.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Additive allocator extension for the C17 implementation. The frozen v1 API
 * remains unchanged and uses the C runtime allocator. An allocator and its
 * context must remain usable until the engine and every result returned by
 * that engine have been destroyed or disposed. allocate must return storage
 * suitably aligned for any C object type, with malloc-equivalent semantics.
 */
enum {
    GAME_RULES_ALLOCATOR_API_VERSION_1 = 1,
};

typedef void* (*game_rules_allocate_v1_fn)(void* context, size_t size);
typedef void (*game_rules_deallocate_v1_fn)(void* context, void* allocation);

typedef struct game_rules_allocator_v1 {
    uint32_t api_version;
    uint32_t struct_size;
    void* context;
    game_rules_allocate_v1_fn allocate;
    game_rules_deallocate_v1_fn deallocate;
} game_rules_allocator_v1;

uint32_t game_rules_allocator_api_version(void);

/* Returns NULL for an invalid allocator descriptor or allocation failure. */
game_rules_engine*
game_rules_engine_create_with_allocator_v1(const game_rules_allocator_v1* allocator);

#ifdef __cplusplus
} /* extern "C" */
#endif
