#include "game_rules/c_api.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>

namespace {

struct EngineDeleter final {
    void operator()(game_rules_engine* engine) const { game_rules_engine_destroy(engine); }
};

struct StringDeleter final {
    void operator()(char* value) const { game_rules_string_free(value); }
};

[[nodiscard]] std::string read_file(const std::filesystem::path& path)
{
    std::ifstream input{path, std::ios::binary};
    std::ostringstream contents;
    contents << input.rdbuf();
    std::string value = contents.str();
    while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
        value.pop_back();
    }
    return value;
}

[[nodiscard]] std::string take(char* value)
{
    const std::unique_ptr<char, StringDeleter> owned{value};
    return owned == nullptr ? std::string{} : std::string{owned.get()};
}

bool compare(const std::filesystem::path& directory,
             const std::string_view name,
             const std::string& actual)
{
    const std::string expected = read_file(directory / (std::string{name} + ".expected.json"));
    if (actual == expected) {
        return true;
    }
    std::cerr << name << " mismatch\nexpected: " << expected << "\nactual:   " << actual << '\n';
    return false;
}

} // namespace

int main(const int argc, const char* const* argv)
{
    if (argc != 2) {
        std::cerr << "usage: game_rules_c_api_contract <contract-directory>\n";
        return 2;
    }
    const std::filesystem::path directory{argv[1]};
    const std::string level = read_file(directory / "level.json");
    const auto level_length = static_cast<std::uint32_t>(level.size());
    const std::unique_ptr<game_rules_engine, EngineDeleter> engine{game_rules_engine_create()};
    if (engine == nullptr) {
        std::cerr << "engine allocation failed\n";
        return 2;
    }

    bool passed = true;
    passed = compare(directory,
                     "load",
                     take(game_rules_engine_load_level(engine.get(), level.data(), level_length)))
        && passed;
    passed = compare(directory, "initial-state", take(game_rules_engine_get_state(engine.get())))
        && passed;
    passed = compare(directory,
                     "move-east",
                     take(game_rules_engine_move(engine.get(), GAME_RULES_DIRECTION_EAST)))
        && passed;
    passed = compare(directory, "rewind", take(game_rules_engine_rewind(engine.get()))) && passed;
    passed = compare(directory, "rewound-state", take(game_rules_engine_get_state(engine.get())))
        && passed;

    return passed ? 0 : 1;
}
