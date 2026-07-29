#include "game_rules/c_api.h"

int main(void)
{
    game_rules_engine* const engine = game_rules_engine_create();
    if (engine == 0) {
        return 1;
    }
    game_rules_engine_destroy(engine);
    return game_rules_api_version() == 1U ? 0 : 2;
}
