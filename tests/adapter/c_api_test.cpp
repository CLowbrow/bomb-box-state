#include "game_rules/c_api.h"

#include <gtest/gtest.h>

#include <cstdint>
#include <memory>
#include <string>

namespace {

struct EngineDeleter final {
    void operator()(game_rules_engine* engine) const { game_rules_engine_destroy(engine); }
};

struct StringDeleter final {
    void operator()(char* value) const { game_rules_string_free(value); }
};

using EnginePointer = std::unique_ptr<game_rules_engine, EngineDeleter>;
using StringPointer = std::unique_ptr<char, StringDeleter>;

[[nodiscard]] std::string take(char* value)
{
    StringPointer owned{value};
    return owned == nullptr ? std::string{} : std::string{owned.get()};
}

constexpr char valid_level[] = R"({
  "format":"game-rules-level",
  "version":1,
  "coordinateSystem":{"origin":{"x":0,"y":0},"positiveX":"east","positiveY":"north"},
  "width":2,
  "height":1,
  "cells":[
    {"coordinate":{"x":0,"y":0},"type":"flat","elevation":0},
    {"coordinate":{"x":1,"y":0},"type":"flat","elevation":0}
  ],
  "fixtures":[],
  "entities":[{"id":"18446744073709551615","type":"player","coordinate":{"x":0,"y":0},"bottomHalfSteps":0}]
})";

TEST(CApi, HandlesLifecycleNullsAndCallerOwnedResults)
{
    EnginePointer engine{game_rules_engine_create()};
    ASSERT_NE(engine, nullptr);

    const std::string empty = take(game_rules_engine_get_state(engine.get()));
    EXPECT_EQ(empty,
              R"({"apiVersion":1,"operation":"getState","status":"no_level","state":null})");

    EXPECT_EQ(take(game_rules_engine_get_state(nullptr)),
              R"({"apiVersion":1,"operation":"getState","status":"invalid_engine","state":null})");
    EXPECT_EQ(take(game_rules_engine_load_level(engine.get(), nullptr, 0)),
              R"({"apiVersion":1,"operation":"loadLevel","status":"invalid_argument","state":null})");

    game_rules_string_free(nullptr);
    game_rules_engine_destroy(nullptr);
}

TEST(CApi, LoadsMovesRewindsAndPreservesUint64EntityIdsAsStrings)
{
    EnginePointer engine{game_rules_engine_create()};
    ASSERT_NE(engine, nullptr);
    const auto level_length = static_cast<std::uint32_t>(sizeof(valid_level) - 1U);
    const std::string loaded =
        take(game_rules_engine_load_level(engine.get(), valid_level, level_length));
    EXPECT_NE(loaded.find(R"("status":"loaded")"), std::string::npos);
    EXPECT_NE(loaded.find(R"("id":"18446744073709551615")"), std::string::npos);
    EXPECT_NE(loaded.find(R"("cells":[)"), std::string::npos);
    EXPECT_NE(loaded.find(R"("fixtures":[])"), std::string::npos);

    const std::string moved =
        take(game_rules_engine_move(engine.get(), GAME_RULES_DIRECTION_EAST));
    EXPECT_NE(moved.find(R"("status":"moved","accepted":true)"), std::string::npos);
    EXPECT_NE(moved.find(R"("type":"entityMoved")"), std::string::npos);
    EXPECT_NE(moved.find(R"("ticks":[{"index":0)"), std::string::npos);
    EXPECT_NE(moved.find(R"("coordinate":{"x":1,"y":0})"), std::string::npos);

    const std::string rewound = take(game_rules_engine_rewind(engine.get()));
    EXPECT_NE(rewound.find(R"("status":"rewound","accepted":true)"), std::string::npos);
    EXPECT_NE(rewound.find(R"("type":"stateRewound")"), std::string::npos);
}

TEST(CApi, RejectsMalformedInputAndDirectionWithoutReplacingState)
{
    EnginePointer engine{game_rules_engine_create()};
    ASSERT_NE(engine, nullptr);
    const auto level_length = static_cast<std::uint32_t>(sizeof(valid_level) - 1U);
    ASSERT_NE(take(game_rules_engine_load_level(engine.get(), valid_level, level_length))
                  .find(R"("status":"loaded")"),
              std::string::npos);

    constexpr char malformed[] = "{";
    const std::string rejected =
        take(game_rules_engine_load_level(
            engine.get(), malformed, static_cast<std::uint32_t>(sizeof(malformed) - 1U)));
    EXPECT_NE(rejected.find(R"("status":"invalid_json")"), std::string::npos);
    EXPECT_NE(rejected.find(R"("error":{"code":"invalid_json")"), std::string::npos);
    EXPECT_NE(rejected.find(R"("id":"18446744073709551615")"), std::string::npos);

    const std::string invalid_direction = take(game_rules_engine_move(engine.get(), 99));
    EXPECT_NE(invalid_direction.find(R"("status":"invalid_direction","accepted":false)"),
              std::string::npos);
    EXPECT_NE(invalid_direction.find(R"("direction":null)"), std::string::npos);
    EXPECT_NE(invalid_direction.find(R"("state":{)"), std::string::npos);
    EXPECT_NE(take(game_rules_engine_rewind(engine.get())).find(R"("status":"history_empty")"),
              std::string::npos);
}

} // namespace
