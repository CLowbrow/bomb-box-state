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
    unit/fixtures_test.cpp
    unit/explosions_test.cpp
    unit/gravity_test.cpp
    unit/resolved_state_history_test.cpp
)

game_rules_add_gtest(
    game_rules_behavior_tests
    behavior
    behavior/world_schema_test.cpp
    behavior/level_json_test.cpp
    behavior/resolved_state_history_test.cpp
    behavior/flat_walking_test.cpp
    behavior/hardening_test.cpp
    behavior/explosions_test.cpp
    behavior/falling_test.cpp
    behavior/fixtures_test.cpp
    behavior/player_push_test.cpp
    behavior/ramps_test.cpp
)

game_rules_add_gtest(
    game_rules_c_api_tests
    integration
    adapter/c_api_test.cpp
    adapter/c_data_api_test.cpp
)

add_executable(
    game_rules_c_api_contract
    contracts/browser_vertical_slice/v1/native_runner.cpp
)
target_link_libraries(game_rules_c_api_contract PRIVATE GameRules::State)
target_compile_features(game_rules_c_api_contract PRIVATE cxx_std_20)
set_target_properties(game_rules_c_api_contract PROPERTIES CXX_EXTENSIONS OFF)
game_rules_set_project_warnings(game_rules_c_api_contract)
game_rules_enable_sanitizers(game_rules_c_api_contract)

add_test(
    NAME game_rules.contract.browser_vertical_slice.v1.native
    COMMAND
        game_rules_c_api_contract
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/browser_vertical_slice/v1/contract.txt"
)
set_tests_properties(
    game_rules.contract.browser_vertical_slice.v1.native
    PROPERTIES LABELS "contract;integration;native"
)

add_test(
    NAME game_rules.contract.engine_hardening.v1.native
    COMMAND
        game_rules_c_api_contract
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/engine_hardening/v1/lifecycle.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/engine_hardening/v1/conflicts.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/engine_hardening/v1/rewind-stress.txt"
)
set_tests_properties(
    game_rules.contract.engine_hardening.v1.native
    PROPERTIES LABELS "contract;integration;native"
)

add_executable(
    game_rules_reference_runner
    c-port/differential_runner.cpp
)
target_link_libraries(game_rules_reference_runner PRIVATE GameRules::State)
target_compile_features(game_rules_reference_runner PRIVATE cxx_std_20)
set_target_properties(game_rules_reference_runner PROPERTIES CXX_EXTENSIONS OFF)
game_rules_set_project_warnings(game_rules_reference_runner)
game_rules_enable_sanitizers(game_rules_reference_runner)

add_executable(
    game_rules_candidate_runner
    c-port/differential_runner.cpp
)
target_link_libraries(game_rules_candidate_runner PRIVATE GameRules::StateC)
target_compile_features(game_rules_candidate_runner PRIVATE cxx_std_20)
set_target_properties(game_rules_candidate_runner PROPERTIES CXX_EXTENSIONS OFF)
game_rules_set_project_warnings(game_rules_candidate_runner)
game_rules_enable_sanitizers(game_rules_candidate_runner)

add_test(
    NAME game_rules.candidate.header_drift
    COMMAND
        "${CMAKE_COMMAND}" -E compare_files
        "${PROJECT_SOURCE_DIR}/include/game_rules/c_api.h"
        "${PROJECT_SOURCE_DIR}/c-port/include/game_rules/c_api.h"
)
set_tests_properties(
    game_rules.candidate.header_drift
    PROPERTIES LABELS "candidate;contract;native"
)

foreach(_yyjson_file IN ITEMS yyjson.c yyjson.h LICENSE)
    string(REPLACE "." "_" _yyjson_test_suffix "${_yyjson_file}")
    add_test(
        NAME "game_rules.candidate.yyjson_${_yyjson_test_suffix}_drift"
        COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${PROJECT_SOURCE_DIR}/vendor/yyjson/${_yyjson_file}"
                "${PROJECT_SOURCE_DIR}/c-port/vendor/yyjson/${_yyjson_file}"
    )
    set_tests_properties(
        "game_rules.candidate.yyjson_${_yyjson_test_suffix}_drift"
        PROPERTIES LABELS "candidate;contract;provenance;native"
    )
endforeach()

add_test(
    NAME game_rules.reference_runner.browser_contract
    COMMAND
        game_rules_reference_runner
        --check-expected
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/browser_vertical_slice/v1/contract.txt"
)
set_tests_properties(
    game_rules.reference_runner.browser_contract
    PROPERTIES LABELS "candidate;contract;integration;native"
)

add_test(
    NAME game_rules.candidate_runner.browser_contract
    COMMAND
        game_rules_candidate_runner
        --check-expected
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/browser_vertical_slice/v1/contract.txt"
)
set_tests_properties(
    game_rules.candidate_runner.browser_contract
    PROPERTIES LABELS "candidate;contract;history;integration;native"
)

add_test(
    NAME game_rules.candidate_runner.hardening_contract
    COMMAND
        game_rules_candidate_runner
        --check-expected
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/engine_hardening/v1/lifecycle.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/engine_hardening/v1/conflicts.txt"
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/engine_hardening/v1/rewind-stress.txt"
)
set_tests_properties(
    game_rules.candidate_runner.hardening_contract
    PROPERTIES LABELS "candidate;contract;history;lifecycle;terminal;integration;native"
)

add_test(
    NAME game_rules.candidate_runner.browser_push_contract
    COMMAND
        game_rules_candidate_runner
        --check-expected
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/browser-push-contract.txt"
)
set_tests_properties(
    game_rules.candidate_runner.browser_push_contract
    PROPERTIES LABELS "candidate;contract;movement;integration;native"
)

add_test(
    NAME game_rules.candidate_runner.explosion_conflicts_contract
    COMMAND
        game_rules_candidate_runner
        --check-expected
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/engine_hardening/v1/conflicts.txt"
)
set_tests_properties(
    game_rules.candidate_runner.explosion_conflicts_contract
    PROPERTIES LABELS "candidate;contract;movement;ramps;explosions;integration;native"
)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(GAME_RULES_C_COMPARATOR "${PROJECT_SOURCE_DIR}/tools/c-port/compare_transcript.py")

add_test(
    NAME game_rules.differential.lifecycle_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/transcripts/lifecycle-parity.txt"
)
set_tests_properties(
    game_rules.differential.lifecycle_parity
    PROPERTIES LABELS "candidate;differential;integration;native"
)

add_test(
    NAME game_rules.differential.level_load_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/level-load-parity.txt"
)
set_tests_properties(
    game_rules.differential.level_load_parity
    PROPERTIES LABELS "candidate;differential;parser;validation;integration;native"
)

add_test(
    NAME game_rules.differential.load_state_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/load-state-parity.txt"
)
set_tests_properties(
    game_rules.differential.load_state_parity
    PROPERTIES LABELS "candidate;differential;initialization;snapshot;integration;native"
)

add_test(
    NAME game_rules.differential.flat_walking_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/flat-walking-parity.txt"
)
set_tests_properties(
    game_rules.differential.flat_walking_parity
    PROPERTIES LABELS "candidate;differential;movement;integration;native"
)

add_test(
    NAME game_rules.differential.player_pushing_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/player-pushing-parity.txt"
)
set_tests_properties(
    game_rules.differential.player_pushing_parity
    PROPERTIES LABELS "candidate;differential;movement;integration;native"
)

add_test(
    NAME game_rules.differential.gravity_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/gravity-parity.txt"
)
set_tests_properties(
    game_rules.differential.gravity_parity
    PROPERTIES LABELS "candidate;differential;movement;gravity;integration;native"
)

add_test(
    NAME game_rules.differential.ramps_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/ramps-parity.txt"
)
set_tests_properties(
    game_rules.differential.ramps_parity
    PROPERTIES LABELS "candidate;differential;movement;ramps;integration;native"
)

add_test(
    NAME game_rules.differential.fixtures_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/fixtures-parity.txt"
)
set_tests_properties(
    game_rules.differential.fixtures_parity
    PROPERTIES LABELS "candidate;differential;movement;gravity;ramps;fixtures;terminal;integration;native"
)

add_test(
    NAME game_rules.differential.explosions_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/explosion_differential_test.py"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${GAME_RULES_C_COMPARATOR}"
)
set_tests_properties(
    game_rules.differential.explosions_parity
    PROPERTIES LABELS "candidate;differential;movement;gravity;ramps;fixtures;explosions;terminal;integration;native"
)

add_test(
    NAME game_rules.differential.browser_history_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/browser_vertical_slice/v1/contract.txt"
)
set_tests_properties(
    game_rules.differential.browser_history_parity
    PROPERTIES LABELS "candidate;differential;history;integration;native"
)

add_test(
    NAME game_rules.differential.hardening_lifecycle_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/engine_hardening/v1/lifecycle.txt"
)
set_tests_properties(
    game_rules.differential.hardening_lifecycle_parity
    PROPERTIES LABELS "candidate;differential;history;lifecycle;terminal;integration;native"
)

add_test(
    NAME game_rules.differential.rewind_stress_parity
    COMMAND
        "${Python3_EXECUTABLE}"
        "${GAME_RULES_C_COMPARATOR}"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${CMAKE_CURRENT_SOURCE_DIR}/contracts/engine_hardening/v1/rewind-stress.txt"
)
set_tests_properties(
    game_rules.differential.rewind_stress_parity
    PROPERTIES LABELS "candidate;differential;history;stress;terminal;integration;native"
)

add_test(
    NAME game_rules.differential.seeded_history_lifecycle
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/history_differential_test.py"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
        "${GAME_RULES_C_COMPARATOR}"
)
set_tests_properties(
    game_rules.differential.seeded_history_lifecycle
    PROPERTIES LABELS "candidate;differential;history;lifecycle;replacement;stress;terminal;integration;native"
)

add_test(
    NAME game_rules.differential.c17_randomized_valid_levels
    COMMAND
        "${Python3_EXECUTABLE}"
        "${CMAKE_CURRENT_SOURCE_DIR}/c-port/randomized_valid_levels_test.py"
        "$<TARGET_FILE:game_rules_reference_runner>"
        "$<TARGET_FILE:game_rules_candidate_runner>"
)
set_tests_properties(
    game_rules.differential.c17_randomized_valid_levels
    PROPERTIES LABELS "candidate;differential;determinism;fuzz;integration;native"
)

# This small consumer stays framework-free and is compiled with the same
# no-exceptions/no-RTTI policy as the core. It proves that the public C++ and C
# headers remain usable under the engine's production compilation constraints.
add_executable(game_rules_production_mode_smoke consumer/production_mode_test.cpp)
target_link_libraries(game_rules_production_mode_smoke PRIVATE GameRules::State)
target_compile_features(game_rules_production_mode_smoke PRIVATE cxx_std_20)
set_target_properties(game_rules_production_mode_smoke PROPERTIES CXX_EXTENSIONS OFF)
game_rules_set_project_warnings(game_rules_production_mode_smoke)
game_rules_set_runtime_options(game_rules_production_mode_smoke)

add_test(NAME game_rules.consumer.production_mode COMMAND game_rules_production_mode_smoke)
set_tests_properties(game_rules.consumer.production_mode PROPERTIES LABELS "consumer;native")

# Compile the public boundary as C (not C++) while using the C++ linker for the implementation.
add_executable(game_rules_c_header_smoke consumer/c_api_header_test.c)
target_link_libraries(game_rules_c_header_smoke PRIVATE GameRules::State)
target_compile_features(game_rules_c_header_smoke PRIVATE c_std_99)
set_target_properties(game_rules_c_header_smoke PROPERTIES C_EXTENSIONS OFF LINKER_LANGUAGE CXX)
game_rules_set_project_warnings(game_rules_c_header_smoke)
game_rules_enable_sanitizers(game_rules_c_header_smoke)

add_test(NAME game_rules.consumer.c_header COMMAND game_rules_c_header_smoke)
set_tests_properties(game_rules.consumer.c_header PROPERTIES LABELS "consumer;integration;native")
