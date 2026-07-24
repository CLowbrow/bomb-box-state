#include "game_rules/c_api.h"

#include <stddef.h>

int main(void)
{
    game_rules_engine* engine = game_rules_engine_create();
    if (engine == NULL || GAME_RULES_DIRECTION_NORTH != 0 || GAME_RULES_DIRECTION_WEST != 3) {
        game_rules_engine_destroy(engine);
        return 1;
    }

    char* state = game_rules_engine_get_state(engine);
    if (state == NULL) {
        game_rules_engine_destroy(engine);
        return 1;
    }

    game_rules_string_free(state);
    game_rules_engine_destroy(engine);
    return 0;
}
