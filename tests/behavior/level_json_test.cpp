#include "bomb_box/engine.hpp"
#include "bomb_box/level_json.hpp"
#include "support/bomb_box_printers.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace {

using namespace bomb_box;

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

TEST(LevelJson, RoundTripIsExactAndCanonical)
{
    const LevelDefinition source = representative_level();
    const EncodeLevelJsonResult encoded = encode_level_json(source);
    ASSERT_TRUE(encoded.accepted());
    ASSERT_TRUE(encoded.json.has_value());
    EXPECT_NE(encoded.json->find("\"18446744073709551615\""), std::string::npos);
    EXPECT_EQ(encoded.json->back(), '\n');

    const DecodeLevelJsonResult decoded = decode_level_json(*encoded.json);
    ASSERT_TRUE(decoded.accepted());
    EXPECT_EQ(decoded.level, canonicalize_level(source));

    ASSERT_TRUE(decoded.level.has_value());
    const EncodeLevelJsonResult reencoded = encode_level_json(*decoded.level);
    EXPECT_EQ(reencoded.json, encoded.json);

    LevelDefinition reordered = source;
    std::reverse(reordered.cells.begin(), reordered.cells.end());
    std::reverse(reordered.fixtures.begin(), reordered.fixtures.end());
    std::reverse(reordered.entities.begin(), reordered.entities.end());
    EXPECT_EQ(encode_level_json(reordered).json, encoded.json);
}

TEST(LevelJson, DecodedLevelLoadsThroughExistingBoundary)
{
    const DecodeLevelJsonResult decoded = decode_level_json(minimal_json());
    Engine engine;
    ASSERT_TRUE(decoded.level.has_value());
    EXPECT_TRUE(engine.load_level(*decoded.level).accepted());
    EXPECT_EQ(engine.loaded_level(), decoded.level);
}

TEST(LevelJson, RejectsMalformedJson)
{
    const DecodeLevelJsonResult result = decode_level_json("[");
    ASSERT_FALSE(result.accepted());
    ASSERT_TRUE(result.json_error.has_value());
    EXPECT_EQ(result.json_error->code, LevelJsonErrorCode::invalid_json);
}

TEST(LevelJson, RequiresObjectRoot)
{
    const DecodeLevelJsonResult result = decode_level_json("[]");
    ASSERT_TRUE(result.json_error.has_value());
    EXPECT_EQ(result.json_error->code, LevelJsonErrorCode::root_not_object);
}

TEST(LevelJson, RejectsUnknownMembersWithPath)
{
    const DecodeLevelJsonResult result = decode_level_json(minimal_json(",\"widht\":1"));
    ASSERT_TRUE(result.json_error.has_value());
    EXPECT_EQ(result.json_error->code, LevelJsonErrorCode::unknown_member);
    EXPECT_EQ(result.json_error->path, "/widht");
}

TEST(LevelJson, RejectsDuplicateMembersWithPath)
{
    const DecodeLevelJsonResult result = decode_level_json(minimal_json(",\"width\":1"));
    ASSERT_TRUE(result.json_error.has_value());
    EXPECT_EQ(result.json_error->code, LevelJsonErrorCode::duplicate_member);
    EXPECT_EQ(result.json_error->path, "/width");
}

TEST(LevelJson, RejectsUnsupportedVersion)
{
    std::string input = minimal_json();
    const auto position = input.find("\"version\":1");
    input.replace(position, std::string{"\"version\":1"}.size(), "\"version\":2");
    const DecodeLevelJsonResult result = decode_level_json(input);
    ASSERT_TRUE(result.json_error.has_value());
    EXPECT_EQ(result.json_error->code, LevelJsonErrorCode::unsupported_version);
    EXPECT_EQ(result.json_error->path, "/version");
}

TEST(LevelJson, RejectsReservedEntityId)
{
    std::string input = minimal_json();
    const auto position = input.find("\"id\":\"1\"");
    input.replace(position, std::string{"\"id\":\"1\""}.size(), "\"id\":\"0\"");
    const DecodeLevelJsonResult result = decode_level_json(input);
    ASSERT_TRUE(result.json_error.has_value());
    EXPECT_EQ(result.json_error->code, LevelJsonErrorCode::invalid_entity_id);
}

TEST(LevelJson, RequiresBrowserSafeEntityIdString)
{
    std::string input = minimal_json();
    const auto position = input.find("\"id\":\"1\"");
    input.replace(position, std::string{"\"id\":\"1\""}.size(), "\"id\":1");
    const DecodeLevelJsonResult result = decode_level_json(input);
    ASSERT_TRUE(result.json_error.has_value());
    EXPECT_EQ(result.json_error->code, LevelJsonErrorCode::invalid_member_type);
    EXPECT_EQ(result.json_error->path, "/entities/0/id");
}

TEST(LevelJson, RejectsEntityIdOutsideUint64)
{
    std::string input = minimal_json();
    const auto position = input.find("\"id\":\"1\"");
    input.replace(position,
                  std::string{"\"id\":\"1\""}.size(),
                  "\"id\":\"18446744073709551616\"");
    const DecodeLevelJsonResult result = decode_level_json(input);
    ASSERT_TRUE(result.json_error.has_value());
    EXPECT_EQ(result.json_error->code, LevelJsonErrorCode::invalid_entity_id);
}

TEST(LevelJson, EnforcesDocumentSizeLimit)
{
    const DecodeLevelJsonResult result =
        decode_level_json(minimal_json(), LevelJsonReadOptions{8, 32});
    ASSERT_TRUE(result.json_error.has_value());
    EXPECT_EQ(result.json_error->code, LevelJsonErrorCode::document_too_large);
}

TEST(LevelJson, EnforcesNestingLimit)
{
    const DecodeLevelJsonResult result =
        decode_level_json(minimal_json(), LevelJsonReadOptions{16U * 1024U * 1024U, 2});
    ASSERT_TRUE(result.json_error.has_value());
    EXPECT_EQ(result.json_error->code, LevelJsonErrorCode::nesting_too_deep);
}

TEST(LevelJson, PreservesGameplayValidationBoundary)
{
    std::string input = minimal_json();
    const auto position = input.find("\"type\":\"player\"");
    input.replace(position,
                  std::string{"\"type\":\"player\""}.size(),
                  "\"type\":\"box\"");
    const DecodeLevelJsonResult decoded = decode_level_json(input);
    EXPECT_FALSE(decoded.accepted());
    EXPECT_FALSE(decoded.json_error.has_value());
    EXPECT_TRUE(std::any_of(decoded.validation_errors.begin(),
                            decoded.validation_errors.end(),
                            [](const ValidationError& error) {
                                return error.code == ValidationErrorCode::player_count_not_one;
                            }));

    LevelDefinition invalid = representative_level();
    invalid.entities.clear();
    const EncodeLevelJsonResult encoded = encode_level_json(invalid);
    EXPECT_FALSE(encoded.accepted());
    EXPECT_FALSE(encoded.validation_errors.empty());
}

} // namespace
