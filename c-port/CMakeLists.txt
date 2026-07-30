cmake_minimum_required(VERSION 3.25)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(
        GameRulesStateC
        VERSION 0.1.0
        DESCRIPTION "C17 candidate for the deterministic game-rules state engine"
        LANGUAGES C
    )
    include(CTest)
    include(GNUInstallDirs)
endif()

set(_game_rules_c_sanitizers_default OFF)
if(DEFINED GAME_RULES_ENABLE_SANITIZERS AND GAME_RULES_ENABLE_SANITIZERS)
    set(_game_rules_c_sanitizers_default ON)
endif()
option(GAME_RULES_C_ENABLE_SANITIZERS
       "Enable AddressSanitizer and UndefinedBehaviorSanitizer for the C candidate"
       ${_game_rules_c_sanitizers_default})
option(GAME_RULES_C_WARNINGS_AS_ERRORS
       "Treat C candidate compiler warnings as errors"
       OFF)

add_library(game_rules_c_yyjson OBJECT src/level_json_yyjson.c)
target_compile_features(game_rules_c_yyjson PRIVATE c_std_99)
set_target_properties(game_rules_c_yyjson PROPERTIES C_EXTENSIONS OFF POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(game_rules_c_yyjson PRIVATE
    YYJSON_DISABLE_INCR_READER=1
    YYJSON_DISABLE_NON_STANDARD=1
    YYJSON_DISABLE_UTILS=1
    YYJSON_DISABLE_WRITER=1)
target_include_directories(game_rules_c_yyjson PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/yyjson")
if(EMSCRIPTEN)
    target_compile_options(game_rules_c_yyjson PRIVATE -O1)
endif()

add_library(game_rules_state_c STATIC
    $<TARGET_OBJECTS:game_rules_c_yyjson>
    src/c_api.c
    src/level.c
    src/level_json.c
    src/state.c
    src/api_operations.c)
add_library(GameRules::StateC ALIAS game_rules_state_c)
target_compile_features(game_rules_state_c PUBLIC c_std_17)
set_target_properties(
    game_rules_state_c
    PROPERTIES
        C_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
)
target_include_directories(
    game_rules_state_c
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/yyjson"
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

function(game_rules_c_apply_options target)
    if(CMAKE_C_COMPILER_ID MATCHES "Clang|GNU")
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
        if(GAME_RULES_C_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
        if(GAME_RULES_C_ENABLE_SANITIZERS AND NOT EMSCRIPTEN)
            target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${target} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
endfunction()

game_rules_c_apply_options(game_rules_state_c)

if(BUILD_TESTING)
add_executable(game_rules_candidate_lifecycle_smoke tests/lifecycle_test.c)
target_link_libraries(game_rules_candidate_lifecycle_smoke PRIVATE GameRules::StateC)
target_include_directories(game_rules_candidate_lifecycle_smoke PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(game_rules_candidate_lifecycle_smoke PRIVATE c_std_17)
set_target_properties(game_rules_candidate_lifecycle_smoke PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_lifecycle_smoke)

add_executable(game_rules_candidate_allocation_failure tests/allocation_failure_test.c)
target_link_libraries(game_rules_candidate_allocation_failure PRIVATE GameRules::StateC)
target_include_directories(game_rules_candidate_allocation_failure PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(game_rules_candidate_allocation_failure PRIVATE c_std_17)
set_target_properties(game_rules_candidate_allocation_failure PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_allocation_failure)

add_executable(game_rules_candidate_wasm_smoke tests/wasm_smoke_test.c)
target_link_libraries(game_rules_candidate_wasm_smoke PRIVATE GameRules::StateC)
target_compile_features(game_rules_candidate_wasm_smoke PRIVATE c_std_17)
set_target_properties(game_rules_candidate_wasm_smoke PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_wasm_smoke)

add_executable(game_rules_candidate_level_loading tests/level_loading_test.c)
target_link_libraries(game_rules_candidate_level_loading PRIVATE GameRules::StateC)
target_include_directories(game_rules_candidate_level_loading PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(game_rules_candidate_level_loading PRIVATE c_std_17)
set_target_properties(game_rules_candidate_level_loading PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_level_loading)

add_executable(game_rules_candidate_flat_walking tests/flat_walking_test.c)
target_link_libraries(game_rules_candidate_flat_walking PRIVATE GameRules::StateC)
target_compile_features(game_rules_candidate_flat_walking PRIVATE c_std_17)
set_target_properties(game_rules_candidate_flat_walking PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_flat_walking)

add_executable(game_rules_candidate_player_pushing tests/player_pushing_test.c)
target_link_libraries(game_rules_candidate_player_pushing PRIVATE GameRules::StateC)
target_compile_features(game_rules_candidate_player_pushing PRIVATE c_std_17)
set_target_properties(game_rules_candidate_player_pushing PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_player_pushing)

add_executable(game_rules_candidate_falling tests/falling_test.c)
target_link_libraries(game_rules_candidate_falling PRIVATE GameRules::StateC)
target_include_directories(game_rules_candidate_falling PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(game_rules_candidate_falling PRIVATE c_std_17)
set_target_properties(game_rules_candidate_falling PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_falling)

add_executable(game_rules_candidate_ramps tests/ramps_test.c)
target_link_libraries(game_rules_candidate_ramps PRIVATE GameRules::StateC)
target_compile_features(game_rules_candidate_ramps PRIVATE c_std_17)
set_target_properties(game_rules_candidate_ramps PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_ramps)

add_executable(game_rules_candidate_fixtures tests/fixtures_test.c)
target_link_libraries(game_rules_candidate_fixtures PRIVATE GameRules::StateC)
target_compile_features(game_rules_candidate_fixtures PRIVATE c_std_17)
set_target_properties(game_rules_candidate_fixtures PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_fixtures)

add_executable(game_rules_candidate_explosions tests/explosions_test.c)
target_link_libraries(game_rules_candidate_explosions PRIVATE GameRules::StateC)
target_compile_features(game_rules_candidate_explosions PRIVATE c_std_17)
set_target_properties(game_rules_candidate_explosions PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_explosions)

add_executable(game_rules_candidate_history tests/history_test.c)
target_link_libraries(game_rules_candidate_history PRIVATE GameRules::StateC)
target_include_directories(game_rules_candidate_history PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_compile_features(game_rules_candidate_history PRIVATE c_std_17)
set_target_properties(game_rules_candidate_history PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_history)

add_executable(game_rules_candidate_abi_layout tests/abi_layout_test.c)
target_link_libraries(game_rules_candidate_abi_layout PRIVATE GameRules::StateC)
target_compile_features(game_rules_candidate_abi_layout PRIVATE c_std_17)
set_target_properties(game_rules_candidate_abi_layout PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_abi_layout)

add_executable(game_rules_candidate_boundary_fuzz tests/boundary_fuzz_test.c)
target_link_libraries(game_rules_candidate_boundary_fuzz PRIVATE GameRules::StateC)
target_compile_features(game_rules_candidate_boundary_fuzz PRIVATE c_std_17)
set_target_properties(game_rules_candidate_boundary_fuzz PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_boundary_fuzz)

include(CheckLanguage)
check_language(CXX)
if(CMAKE_CXX_COMPILER)
    enable_language(CXX)
    add_executable(game_rules_candidate_cpp_header tests/header_cpp_compatibility_test.cpp)
    target_link_libraries(game_rules_candidate_cpp_header PRIVATE GameRules::StateC)
    target_compile_features(game_rules_candidate_cpp_header PRIVATE cxx_std_17)
    set_target_properties(game_rules_candidate_cpp_header PROPERTIES CXX_EXTENSIONS OFF)
    game_rules_c_apply_options(game_rules_candidate_cpp_header)
endif()

    if(NOT EMSCRIPTEN)
        add_test(
            NAME game_rules.candidate.library_only_build
            COMMAND
                "${CMAKE_COMMAND}"
                "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
                "-DBINARY_DIR=${CMAKE_CURRENT_BINARY_DIR}/library-only-audit"
                "-DGENERATOR=${CMAKE_GENERATOR}"
                -P "${CMAKE_CURRENT_SOURCE_DIR}/tests/library_only_build_test.cmake"
        )
        set_tests_properties(
            game_rules.candidate.library_only_build
            PROPERTIES LABELS "candidate;native;build;extraction"
        )
    endif()
    add_test(NAME game_rules.candidate.lifecycle COMMAND game_rules_candidate_lifecycle_smoke)
    add_test(NAME game_rules.candidate.allocation_failure COMMAND game_rules_candidate_allocation_failure)
    add_test(NAME game_rules.candidate.level_loading COMMAND game_rules_candidate_level_loading)
    add_test(NAME game_rules.candidate.flat_walking COMMAND game_rules_candidate_flat_walking)
    add_test(NAME game_rules.candidate.player_pushing COMMAND game_rules_candidate_player_pushing)
    add_test(NAME game_rules.candidate.falling COMMAND game_rules_candidate_falling)
    add_test(NAME game_rules.candidate.ramps COMMAND game_rules_candidate_ramps)
    add_test(NAME game_rules.candidate.fixtures COMMAND game_rules_candidate_fixtures)
    add_test(NAME game_rules.candidate.explosions COMMAND game_rules_candidate_explosions)
    add_test(NAME game_rules.candidate.history COMMAND game_rules_candidate_history)
    add_test(NAME game_rules.candidate.abi_layout COMMAND game_rules_candidate_abi_layout)
    add_test(NAME game_rules.candidate.boundary_fuzz COMMAND game_rules_candidate_boundary_fuzz)
    if(TARGET game_rules_candidate_cpp_header)
        add_test(NAME game_rules.candidate.cpp_header COMMAND game_rules_candidate_cpp_header)
    endif()
    if(EMSCRIPTEN)
        set_tests_properties(game_rules.candidate.lifecycle PROPERTIES LABELS "candidate;wasm")
        set_tests_properties(game_rules.candidate.allocation_failure PROPERTIES LABELS "candidate;wasm")
        set_tests_properties(game_rules.candidate.level_loading PROPERTIES LABELS "candidate;wasm")
        set_tests_properties(game_rules.candidate.flat_walking PROPERTIES LABELS "candidate;wasm")
        set_tests_properties(game_rules.candidate.player_pushing PROPERTIES LABELS "candidate;wasm")
        set_tests_properties(game_rules.candidate.falling PROPERTIES LABELS "candidate;wasm;gravity")
        set_tests_properties(game_rules.candidate.ramps PROPERTIES LABELS "candidate;wasm;movement;ramps")
        set_tests_properties(game_rules.candidate.fixtures PROPERTIES LABELS "candidate;wasm;movement;fixtures")
        set_tests_properties(game_rules.candidate.explosions PROPERTIES LABELS "candidate;wasm;movement;explosions")
        set_tests_properties(game_rules.candidate.history PROPERTIES LABELS "candidate;wasm;history;lifecycle")
        set_tests_properties(game_rules.candidate.abi_layout PROPERTIES LABELS "candidate;wasm;abi;contract")
        set_tests_properties(game_rules.candidate.boundary_fuzz PROPERTIES LABELS "candidate;wasm;fuzz;parser;abi")
        if(TARGET game_rules_candidate_cpp_header)
            set_tests_properties(game_rules.candidate.cpp_header PROPERTIES LABELS "candidate;wasm;abi;contract")
        endif()
        find_program(GAME_RULES_C_NODE_EXECUTABLE node)
        if(GAME_RULES_C_NODE_EXECUTABLE)
            add_test(
                NAME game_rules.candidate.wasm_smoke
                COMMAND "${GAME_RULES_C_NODE_EXECUTABLE}" $<TARGET_FILE:game_rules_candidate_wasm_smoke>
            )
            set_tests_properties(game_rules.candidate.wasm_smoke PROPERTIES LABELS "candidate;wasm")
        endif()
    else()
        set_tests_properties(game_rules.candidate.lifecycle PROPERTIES LABELS "candidate;native")
        set_tests_properties(game_rules.candidate.allocation_failure PROPERTIES LABELS "candidate;native")
        set_tests_properties(game_rules.candidate.level_loading PROPERTIES LABELS "candidate;native;parser;validation")
        set_tests_properties(game_rules.candidate.flat_walking PROPERTIES LABELS "candidate;native;movement")
        set_tests_properties(game_rules.candidate.player_pushing PROPERTIES LABELS "candidate;native;movement")
        set_tests_properties(game_rules.candidate.falling PROPERTIES LABELS "candidate;native;movement;gravity")
        set_tests_properties(game_rules.candidate.ramps PROPERTIES LABELS "candidate;native;movement;ramps")
        set_tests_properties(game_rules.candidate.fixtures PROPERTIES LABELS "candidate;native;movement;fixtures")
        set_tests_properties(game_rules.candidate.explosions PROPERTIES LABELS "candidate;native;movement;explosions")
        set_tests_properties(game_rules.candidate.history PROPERTIES LABELS "candidate;native;history;lifecycle")
        set_tests_properties(game_rules.candidate.abi_layout PROPERTIES LABELS "candidate;native;abi;contract")
        set_tests_properties(game_rules.candidate.boundary_fuzz PROPERTIES LABELS "candidate;native;fuzz;parser;abi")
        if(TARGET game_rules_candidate_cpp_header)
            set_tests_properties(game_rules.candidate.cpp_header PROPERTIES LABELS "candidate;native;abi;contract")
        endif()
        add_test(NAME game_rules.candidate.wasm_smoke COMMAND game_rules_candidate_wasm_smoke)
        set_tests_properties(game_rules.candidate.wasm_smoke PROPERTIES LABELS "candidate;native;wasm-smoke")
    endif()
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(TARGETS game_rules_state_c ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(FILES include/game_rules/c_api.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/game_rules")
    install(FILES include/game_rules/c_allocator_api.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/game_rules")
    install(FILES THIRD_PARTY_NOTICES.md DESTINATION "${CMAKE_INSTALL_DATADIR}/GameRulesStateC")
    install(FILES vendor/yyjson/LICENSE vendor/yyjson/README.game-rules.md
            DESTINATION "${CMAKE_INSTALL_DATADIR}/GameRulesStateC/licenses")
endif()
