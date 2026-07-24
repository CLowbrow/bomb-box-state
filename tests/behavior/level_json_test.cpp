#include "bomb_box/engine.hpp"
#include "bomb_box/level_json.hpp"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <limits>
#include <string>
#include <string_view>

namespace {

using namespace bomb_box;

int failures = 0;

void expect(const bool condition, const std::string_view message)
{
    if (!condition) {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

[[nodiscard]] LevelDefinition representative_level()
{
    LevelDefinition level;
    level.coordinates = CoordinateSystem{
        Coordinate{10, -4},
        HorizontalAxisDirection::west,
        VerticalAxisDirection::south,
    };
    level.width = 4;
    level.height = 1;
    // Intentionally non-canonical input order.
    level.cells = {
        Cell{Coordinate{13, -4}, FlatCell{1}},
        Cell{Coordinate{12, -4}, FlatCell{1}},
        Cell{Coordinate{11, -4}, RampCell{Direction::east, 0}},
        Cell{Coordinate{10, -4}, FlatCell{0}},
    };
    level.fixtures = {
        Fixture{Coordinate{13, -4}, ExitTeleporter{}},
        Fixture{Coordinate{12, -4}, Door{SwitchColor::blue}},
        Fixture{Coordinate{10, -4}, Switch{SwitchColor::blue}},
    };
    level.entities = {
        Entity{std::numeric_limits<EntityId>::max(), EntityKind::player,
               Coordinate{12, -4}, Height{4}},
        Entity{7, EntityKind::box, Coordinate{12, -4}, Height{2}},
        Entity{1, EntityKind::barrel, Coordinate{10, -4}, Height{6}},
    };
    return level;
}

[[nodiscard]] std::string minimal_json(const std::string_view extra_root = {})
{
    return std::string{R"({
  "format":"bomb-box-level",
  "version":1,
  "coordinateSystem":{"origin":{"x":0,"y":0},"positiveX":"east","positiveY":"north"},
  "width":1,
  "height":1,
  "cells":[{"coordinate":{"x":0,"y":0},"type":"flat","elevation":0}],
  "fixtures":[],
  "entities":[{"id":"1","type":"player","coordinate":{"x":0,"y":0},"bottomHalfSteps":0}]
)"} + std::string{extra_root} + "}";
}

void round_trip_is_exact_and_canonical()
{
    const LevelDefinition source = representative_level();
    const EncodeLevelJsonResult encoded = encode_level_json(source);
    expect(encoded.accepted(), "a valid level serializes");
    if (!encoded.json.has_value()) {
        return;
    }
    expect(encoded.json->find("\"18446744073709551615\"") != std::string::npos,
           "a uint64 entity ID is a lossless decimal JSON string");
    expect(encoded.json->back() == '\n', "canonical JSON ends in one newline");

    const DecodeLevelJsonResult decoded = decode_level_json(*encoded.json);
    expect(decoded.accepted(), "canonical JSON decodes");
    expect(decoded.level == canonicalize_level(source), "round-trip preserves the complete level");

    if (decoded.level.has_value()) {
        const EncodeLevelJsonResult reencoded = encode_level_json(*decoded.level);
        expect(reencoded.json == encoded.json, "canonical serialization is byte-stable");
    }

    LevelDefinition reordered = source;
    std::reverse(reordered.cells.begin(), reordered.cells.end());
    std::reverse(reordered.fixtures.begin(), reordered.fixtures.end());
    std::reverse(reordered.entities.begin(), reordered.entities.end());
    expect(encode_level_json(reordered).json == encoded.json,
           "input array order cannot affect canonical serialized bytes");
}

void decoded_level_loads_through_the_existing_boundary()
{
    const DecodeLevelJsonResult decoded = decode_level_json(minimal_json());
    Engine engine;
    expect(decoded.level.has_value(), "the minimal document decodes");
    if (decoded.level.has_value()) {
        expect(engine.load_level(*decoded.level).accepted(), "decoded data loads through Engine");
        expect(engine.loaded_level() == decoded.level, "Engine owns the decoded canonical definition");
    }
}

void format_errors_are_precise_and_non_accepting()
{
    {
        const DecodeLevelJsonResult result = decode_level_json("[");
        expect(!result.accepted() && result.json_error.has_value()
                   && result.json_error->code == LevelJsonErrorCode::invalid_json,
               "malformed JSON is rejected as invalid_json");
    }
    {
        const DecodeLevelJsonResult result = decode_level_json("[]");
        expect(result.json_error.has_value()
                   && result.json_error->code == LevelJsonErrorCode::root_not_object,
               "the document root must be an object");
    }
    {
        const std::string input = minimal_json(",\"widht\":1");
        const DecodeLevelJsonResult result = decode_level_json(input);
        expect(result.json_error.has_value()
                   && result.json_error->code == LevelJsonErrorCode::unknown_member
                   && result.json_error->path == "/widht",
               "unknown fields are rejected with a JSON Pointer path");
    }
    {
        const std::string input = minimal_json(",\"width\":1");
        const DecodeLevelJsonResult result = decode_level_json(input);
        expect(result.json_error.has_value()
                   && result.json_error->code == LevelJsonErrorCode::duplicate_member
                   && result.json_error->path == "/width",
               "duplicate fields remain rejected after generic parsing");
    }
    {
        std::string input = minimal_json();
        const auto position = input.find("\"version\":1");
        input.replace(position, std::string{"\"version\":1"}.size(), "\"version\":2");
        const DecodeLevelJsonResult result = decode_level_json(input);
        expect(result.json_error.has_value()
                   && result.json_error->code == LevelJsonErrorCode::unsupported_version
                   && result.json_error->path == "/version",
               "newer format versions are never silently interpreted as version 1");
    }
    {
        std::string input = minimal_json();
        const auto position = input.find("\"id\":\"1\"");
        input.replace(position, std::string{"\"id\":\"1\""}.size(), "\"id\":\"0\"");
        const DecodeLevelJsonResult result = decode_level_json(input);
        expect(result.json_error.has_value()
                   && result.json_error->code == LevelJsonErrorCode::invalid_entity_id,
               "entity ID zero is reserved and rejected by the wire format");
    }
    {
        std::string input = minimal_json();
        const auto position = input.find("\"id\":\"1\"");
        input.replace(position, std::string{"\"id\":\"1\""}.size(), "\"id\":1");
        const DecodeLevelJsonResult result = decode_level_json(input);
        expect(result.json_error.has_value()
                   && result.json_error->code == LevelJsonErrorCode::invalid_member_type
                   && result.json_error->path == "/entities/0/id",
               "entity IDs must use the browser-safe string representation");
    }
    {
        std::string input = minimal_json();
        const auto position = input.find("\"id\":\"1\"");
        input.replace(position, std::string{"\"id\":\"1\""}.size(),
                      "\"id\":\"18446744073709551616\"");
        const DecodeLevelJsonResult result = decode_level_json(input);
        expect(result.json_error.has_value()
                   && result.json_error->code == LevelJsonErrorCode::invalid_entity_id,
               "entity IDs outside uint64 are rejected");
    }
    {
        const DecodeLevelJsonResult result = decode_level_json(
            minimal_json(), LevelJsonReadOptions{8, 32});
        expect(result.json_error.has_value()
                   && result.json_error->code == LevelJsonErrorCode::document_too_large,
               "callers can bound untrusted document size");
    }
    {
        const DecodeLevelJsonResult result = decode_level_json(
            minimal_json(), LevelJsonReadOptions{16U * 1024U * 1024U, 2});
        expect(result.json_error.has_value()
                   && result.json_error->code == LevelJsonErrorCode::nesting_too_deep,
               "callers can bound nesting even though yyjson supports deeper documents");
    }
}

void gameplay_validation_is_preserved()
{
    std::string input = minimal_json();
    const auto position = input.find("\"type\":\"player\"");
    input.replace(position, std::string{"\"type\":\"player\""}.size(), "\"type\":\"box\"");
    const DecodeLevelJsonResult decoded = decode_level_json(input);
    expect(!decoded.accepted() && !decoded.json_error.has_value(),
           "well-formed level JSON can fail gameplay-structure validation separately");
    expect(std::any_of(decoded.validation_errors.begin(), decoded.validation_errors.end(),
                       [](const ValidationError& error) {
                           return error.code == ValidationErrorCode::player_count_not_one;
                       }),
           "decoder reports the shared validation error codes");

    LevelDefinition invalid = representative_level();
    invalid.entities.clear();
    const EncodeLevelJsonResult encoded = encode_level_json(invalid);
    expect(!encoded.accepted() && !encoded.validation_errors.empty(),
           "invalid in-memory definitions cannot be serialized as valid level documents");
}

} // namespace

int main()
{
    round_trip_is_exact_and_canonical();
    decoded_level_loads_through_the_existing_boundary();
    format_errors_are_precise_and_non_accepting();
    gameplay_validation_is_preserved();

    if (failures == 0) {
        std::cout << "All level-JSON behavior checks passed.\n";
    }
    return failures == 0 ? 0 : 1;
}
