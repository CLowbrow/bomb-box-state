#include "game_rules/c_api.h"
#include "game_rules/engine.hpp"

#include <concepts>
#include <cstring>
#include <iostream>

namespace {

template <typename T>
concept EngineLike = requires(const T& value) {
    { value.status() } -> std::same_as<game_rules::EngineStatus>;
};

static_assert(EngineLike<game_rules::Engine>, "The public engine requires C++20 concepts");

int failures = 0;

void expect(const bool condition, const char* const message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

} // namespace

int main()
{
    const game_rules::Engine engine;

    expect(engine.status() == game_rules::EngineStatus::schema_ready,
           "the C++ engine reports its schema-ready status");
    expect(game_rules::to_string(engine.status()) == "schema_ready",
           "the C++ status has a stable string representation");
    expect(game_rules_api_version() == game_rules::api_version,
           "the C and C++ API versions match");
    expect(std::strcmp(game_rules_engine_status(), "schema_ready") == 0,
           "the C ABI is callable");

    if (failures == 0) {
        std::cout << "All production-mode consumer checks passed.\n";
    }

    return failures == 0 ? 0 : 1;
}
