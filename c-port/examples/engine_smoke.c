#include "game_rules/c_api.h"

#include <stddef.h>
#include <stdint.h>
#include <string.h>

int main(void) {
    game_rules_engine* engine;

    if (game_rules_api_version() != 1U || game_rules_data_api_version() != 1U) {
        return 1;
    }
    engine = game_rules_engine_create();
    if (engine == NULL) {
        return 2;
    }
    if (strcmp(game_rules_engine_status(), "schema_ready") != 0) {
        game_rules_engine_destroy(engine);
        return 3;
    }
    game_rules_engine_destroy(engine);
    return 0;
}
