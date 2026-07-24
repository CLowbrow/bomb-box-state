#include "bomb_box/c_api.h"
#include "bomb_box/engine.hpp"

#include <concepts>
#include <cstring>
#include <iostream>

namespace {

template <typename T>
concept EngineLike = requires(const T& value) {
    { value.status() } -> std::same_as<bomb_box::EngineStatus>;
};

static_assert(EngineLike<bomb_box::Engine>, "The public engine requires C++20 concepts");

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
    const bomb_box::Engine engine;

    expect(engine.status() == bomb_box::EngineStatus::schema_ready,
           "the C++ engine reports its schema-ready status");
    expect(bomb_box::to_string(engine.status()) == "schema_ready",
           "the C++ status has a stable string representation");
    expect(bomb_box_api_version() == bomb_box::api_version,
           "the C and C++ API versions match");
    expect(std::strcmp(bomb_box_engine_status(), "schema_ready") == 0,
           "the C ABI is callable");

    if (failures == 0) {
        std::cout << "All production-mode consumer checks passed.\n";
    }

    return failures == 0 ? 0 : 1;
}
