#pragma once

// Private bridge to the vendored yyjson 0.12.0 implementation. No yyjson
// types or symbols cross the public Bomb Box API boundary.
#include "yyjson.h"

#ifdef __cplusplus
extern "C" {
#endif

yyjson_doc *bomb_box_internal_yyjson_read(const char *data,
                                          size_t size,
                                          yyjson_read_err *error);
void bomb_box_internal_yyjson_free(yyjson_doc *document);

#ifdef __cplusplus
} // extern "C"
#endif
