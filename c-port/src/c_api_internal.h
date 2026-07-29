#pragma once

#include "game_rules/c_api.h"

#include <stddef.h>
#include <stdint.h>

/*
 * Private stage-01 seam exercised by lifecycle tests. Stage 02 can build a
 * complete candidate level/history graph off to the side and commit it with
 * the same allocate-then-swap ownership pattern.
 */
uint32_t game_rules_c_engine_replace_session(game_rules_engine* engine,
                                             uint32_t marker,
                                             size_t level_storage_size,
                                             size_t history_storage_size);

uint32_t game_rules_c_engine_session_marker(const game_rules_engine* engine);

void* game_rules_c_engine_allocate_result_storage(const game_rules_engine* engine,
                                                  size_t storage_size);
