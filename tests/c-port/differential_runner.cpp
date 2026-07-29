#include "game_rules/c_api.h"

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace {

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
    return 99U;
}

[[nodiscard]] std::string take(char* value)
{
    if (value == nullptr) {
        return {};
    }
    const std::string copied{value};
    game_rules_string_free(value);
    return copied;
}

[[nodiscard]] bool emit_and_optionally_check(const std::filesystem::path& script,
                                             const std::size_t line_number,
                                             const std::string_view expected_field,
                                             const std::string& response,
                                             const bool check_expected)
{
    if (response.empty()) {
        std::cerr << script << ':' << line_number << " operation returned no JSON\n";
        return false;
    }
    std::cout << response << '\n';
    if (!check_expected || expected_field == "-") {
        return true;
    }
    const std::filesystem::path expected_path = script.parent_path() / expected_field;
    const std::string expected = read_file(expected_path, true);
    if (response == expected) {
        return true;
    }
    std::cerr << script << ':' << line_number << " mismatch for " << expected_path
              << "\nexpected: " << expected << "\nactual:   " << response << '\n';
    return false;
}

[[nodiscard]] bool run_script(const std::filesystem::path& script, const bool check_expected)
{
    std::ifstream input{script};
    if (!input) {
        std::cerr << "could not read transcript: " << script << '\n';
        return false;
    }

    game_rules_engine* engine = game_rules_engine_create();
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
        if (fields.size() < 2U || fields.size() > 3U) {
            std::cerr << script << ':' << line_number << " expected 2 or 3 pipe-separated fields\n";
            passed = false;
            continue;
        }

        std::string response;
        if (fields[0] == "load" && fields.size() == 3U) {
            const std::string level = read_file(script.parent_path() / fields[2]);
            response = take(game_rules_engine_load_level(
                engine, level.data(), static_cast<std::uint32_t>(level.size())));
        } else if (fields[0] == "get-state" && fields.size() == 2U) {
            response = take(game_rules_engine_get_state(engine));
        } else if (fields[0] == "move" && fields.size() == 3U) {
            response = take(game_rules_engine_move(engine, direction_value(fields[2])));
        } else if (fields[0] == "rewind" && fields.size() == 2U) {
            response = take(game_rules_engine_rewind(engine));
        } else if (fields[0] == "destroy-engine" && fields.size() == 2U) {
            game_rules_engine_destroy(engine);
            engine = nullptr;
            response = "{\"apiVersion\":1,\"operation\":\"destroyEngine\",\"status\":\"destroyed\",\"state\":null}";
        } else if (fields[0] == "create-engine" && fields.size() == 2U) {
            game_rules_engine_destroy(engine);
            engine = game_rules_engine_create();
            response = engine == nullptr
                         ? "{\"apiVersion\":1,\"operation\":\"createEngine\",\"status\":\"allocation_failed\",\"state\":null}"
                         : "{\"apiVersion\":1,\"operation\":\"createEngine\",\"status\":\"created\",\"state\":null}";
        } else {
            std::cerr << script << ':' << line_number << " invalid transcript operation\n";
            passed = false;
            continue;
        }

        passed = emit_and_optionally_check(
                     script, line_number, fields[1], response, check_expected) &&
                 passed;
    }

    game_rules_engine_destroy(engine);
    return passed;
}

} // namespace

int main(int argc, const char* const* argv)
{
    bool check_expected = false;
    int first_script = 1;
    if (argc > 1 && std::string_view{argv[1]} == "--check-expected") {
        check_expected = true;
        first_script = 2;
    }
    if (argc <= first_script) {
        std::cerr << "usage: differential_runner [--check-expected] <transcript>...\n";
        return 2;
    }
    bool passed = true;
    for (int index = first_script; index < argc; ++index) {
        passed = run_script(argv[index], check_expected) && passed;
    }
    return passed ? 0 : 1;
}
