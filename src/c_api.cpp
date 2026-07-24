#include "game_rules/c_api.h"

#include "game_rules/engine.hpp"

uint32_t game_rules_api_version(void)
{
    return game_rules::api_version;
}

const char* game_rules_engine_status(void)
{
    return "schema_ready";
}

