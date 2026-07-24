/*
 * Private compilation bridge for vendored yyjson 0.12.0.
 *
 * yyjson.c and yyjson.h are unmodified MIT-licensed upstream files. Defining
 * yyjson_api as static gives upstream's externally implemented functions
 * internal linkage in this translation unit, preventing symbol collisions
 * when an embedding host also links yyjson. See vendor/yyjson/README.game-rules.md.
 */
#define yyjson_api static
#include "yyjson.c"

yyjson_doc *game_rules_internal_yyjson_read(const char *data,
                                          size_t size,
                                          yyjson_read_err *error)
{
    /* YYJSON_READ_NOFLAG never modifies data; upstream documents this cast. */
    return yyjson_read_opts((char *)data, size, YYJSON_READ_NOFLAG, NULL, error);
}

void game_rules_internal_yyjson_free(yyjson_doc *document)
{
    yyjson_doc_free(document);
}
