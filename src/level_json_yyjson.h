#pragma once

// Private bridge to the vendored yyjson 0.12.0 implementation. No yyjson
// types or symbols cross the public game-rules API boundary.
#include "yyjson.h"

#ifdef __cplusplus
extern "C" {
#endif

yyjson_doc *game_rules_internal_yyjson_read(const char *data,
                                          size_t size,
                                          yyjson_read_err *error);
void game_rules_internal_yyjson_free(yyjson_doc *document);

#ifdef __cplusplus
} // extern "C"
#endif
