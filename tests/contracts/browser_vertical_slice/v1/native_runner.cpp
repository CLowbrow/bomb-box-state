#include "game_rules/c_api.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

struct EngineDeleter final {
    void operator()(game_rules_engine* engine) const { game_rules_engine_destroy(engine); }
};

struct StringDeleter final {
    void operator()(char* value) const { game_rules_string_free(value); }
};

[[nodiscard]] std::string read_file(const std::filesystem::path& path,
                                    const bool trim_trailing_newline = false)
{
    std::ifstream input{path, std::ios::binary};
    if (!input) {
        return {};
    }
    std::ostringstream contents;
    contents << input.rdbuf();
    std::string value = contents.str();
    if (trim_trailing_newline) {
        while (!value.empty() && (value.back() == '\n' || value.back() == '\r')) {
            value.pop_back();
        }
    }
    return value;
}

[[nodiscard]] std::string take(char* value)
{
    const std::unique_ptr<char, StringDeleter> owned{value};
    return owned == nullptr ? std::string{} : std::string{owned.get()};
}

[[nodiscard]] std::vector<std::string> split_fields(const std::string_view line)
{
    std::vector<std::string> fields;
    std::size_t start = 0;
    while (start <= line.size()) {
        const std::size_t end = line.find('|', start);
        fields.emplace_back(line.substr(start, end == std::string_view::npos
                                                  ? line.size() - start
                                                  : end - start));
        if (end == std::string_view::npos) {
            break;
        }
        start = end + 1;
    }
    return fields;
}

[[nodiscard]] std::uint32_t direction_value(const std::string_view direction)
{
    if (direction == "north") {
        return GAME_RULES_DIRECTION_NORTH;
    }
    if (direction == "east") {
        return GAME_RULES_DIRECTION_EAST;
    }
    if (direction == "south") {
        return GAME_RULES_DIRECTION_SOUTH;
    }
    if (direction == "west") {
        return GAME_RULES_DIRECTION_WEST;
    }
    return 99;
}

[[nodiscard]] bool compare(const std::filesystem::path& script,
                           const std::size_t line_number,
                           const std::filesystem::path& expected_path,
                           const std::string& actual)
{
    const std::string expected = read_file(expected_path, true);
    if (actual == expected) {
        return true;
    }
    std::cerr << script << ':' << line_number << " mismatch for " << expected_path
              << "\nexpected: " << expected << "\nactual:   " << actual << '\n';
    return false;
}

[[nodiscard]] bool run_script(const std::filesystem::path& script)
{
    std::ifstream input{script};
    if (!input) {
        std::cerr << "could not read contract script: " << script << '\n';
        return false;
    }
    const std::unique_ptr<game_rules_engine, EngineDeleter> engine{game_rules_engine_create()};
    if (engine == nullptr) {
        std::cerr << "engine allocation failed\n";
        return false;
    }

    bool passed = true;
    std::string line;
    std::size_t line_number = 0;
    while (std::getline(input, line)) {
        ++line_number;
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        if (line.empty() || line.front() == '#') {
            continue;
        }
        const std::vector<std::string> fields = split_fields(line);
        if (fields.size() < 2 || fields.size() > 3) {
            std::cerr << script << ':' << line_number << " expected 2 or 3 pipe-separated fields\n";
            passed = false;
            continue;
        }

        char* raw_result = nullptr;
        if (fields[0] == "load" && fields.size() == 3) {
            const std::string level = read_file(script.parent_path() / fields[2]);
            raw_result = game_rules_engine_load_level(
                engine.get(), level.data(), static_cast<std::uint32_t>(level.size()));
        } else if (fields[0] == "get-state" && fields.size() == 2) {
            raw_result = game_rules_engine_get_state(engine.get());
        } else if (fields[0] == "move" && fields.size() == 3) {
            raw_result = game_rules_engine_move(engine.get(), direction_value(fields[2]));
        } else if (fields[0] == "rewind" && fields.size() == 2) {
            raw_result = game_rules_engine_rewind(engine.get());
        } else {
            std::cerr << script << ':' << line_number << " invalid contract operation\n";
            passed = false;
            continue;
        }

        passed = compare(script, line_number, script.parent_path() / fields[1], take(raw_result))
            && passed;
    }
    return passed;
}

} // namespace

int main(const int argc, const char* const* argv)
{
    if (argc < 2) {
        std::cerr << "usage: game_rules_c_api_contract <contract-script>...\n";
        return 2;
    }
    bool passed = true;
    for (int index = 1; index < argc; ++index) {
        passed = run_script(argv[index]) && passed;
    }
    return passed ? 0 : 1;
}
