#include "bomb_box/c_api.h"

#include "bomb_box/engine.hpp"

uint32_t bomb_box_api_version(void)
{
    return bomb_box::api_version;
}

const char* bomb_box_engine_status(void)
{
    return "schema_ready";
}

