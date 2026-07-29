#pragma once

#include "yyjson.h"

yyjson_doc* game_rules_c_yyjson_read(const char* data,
                                     size_t size,
                                     const yyjson_alc* allocator,
                                     yyjson_read_err* error);
void game_rules_c_yyjson_free(yyjson_doc* document);
