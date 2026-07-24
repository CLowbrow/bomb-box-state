include(FetchContent)

# GoogleTest is a build-only dependency. Pin the exact v1.17.0 release commit
# and archive hash so configuring tests never changes the framework implicitly.
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
set(BUILD_GMOCK OFF CACHE BOOL "" FORCE)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/52eb8108c5bdec04579160ae17225d66034bd723.zip
    URL_HASH SHA256=9a56a54ae784394ff664cd55e8f4c9a03b503ebf0cb99576321c78ab3d87ca84
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

include(GoogleTest)

function(game_rules_add_gtest target label)
    add_executable(${target} ${ARGN})
    target_link_libraries(${target} PRIVATE GameRules::State GTest::gtest_main)
    target_compile_features(${target} PRIVATE cxx_std_20)
    set_target_properties(${target} PROPERTIES CXX_EXTENSIONS OFF)
    target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

    game_rules_set_project_warnings(${target})
    # The engine target retains the production no-exceptions/no-RTTI policy.
    # Test runners use the framework's normal runtime and share sanitizer
    # instrumentation with the engine when the sanitized preset is selected.
    game_rules_enable_sanitizers(${target})

    gtest_discover_tests(
        ${target}
        TEST_PREFIX "game_rules.${label}."
        PROPERTIES LABELS "${label}\;native"
        DISCOVERY_MODE PRE_TEST
    )
endfunction()

game_rules_add_gtest(
    game_rules_unit_tests
    unit
    unit/resolved_state_history_test.cpp
)

game_rules_add_gtest(
    game_rules_behavior_tests
    behavior
    behavior/world_schema_test.cpp
    behavior/level_json_test.cpp
    behavior/resolved_state_history_test.cpp
    behavior/flat_walking_test.cpp
)

# This small consumer stays framework-free and is compiled with the same
# no-exceptions/no-RTTI policy as the core. It proves that the public C++ and C
# headers remain usable under the engine's production compilation constraints.
add_executable(game_rules_production_mode_smoke consumer/production_mode_smoke.cpp)
target_link_libraries(game_rules_production_mode_smoke PRIVATE GameRules::State)
target_compile_features(game_rules_production_mode_smoke PRIVATE cxx_std_20)
set_target_properties(game_rules_production_mode_smoke PROPERTIES CXX_EXTENSIONS OFF)
game_rules_set_project_warnings(game_rules_production_mode_smoke)
game_rules_set_runtime_options(game_rules_production_mode_smoke)

add_test(NAME game_rules.consumer.production_mode COMMAND game_rules_production_mode_smoke)
set_tests_properties(game_rules.consumer.production_mode PROPERTIES LABELS "consumer;native")
