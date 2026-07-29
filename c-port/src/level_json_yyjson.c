/*
 * Private compilation bridge for the unmodified vendored yyjson 0.12.0.
 * Giving upstream entry points internal linkage prevents collisions when an
 * embedding host links another yyjson build.
 */
#define yyjson_api static
#include "yyjson.c"

yyjson_doc* game_rules_c_yyjson_read(const char* data,
                                     size_t size,
                                     const yyjson_alc* allocator,
                                     yyjson_read_err* error)
{
    return yyjson_read_opts((char*)data, size, YYJSON_READ_NOFLAG, allocator, error);
}

void game_rules_c_yyjson_free(yyjson_doc* document)
{
    yyjson_doc_free(document);
}
