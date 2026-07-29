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

add_library(game_rules_state_c STATIC src/c_api.c)
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

add_executable(game_rules_candidate_lifecycle_smoke tests/lifecycle_smoke.c)
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

add_executable(game_rules_candidate_wasm_smoke tests/wasm_smoke.c)
target_link_libraries(game_rules_candidate_wasm_smoke PRIVATE GameRules::StateC)
target_compile_features(game_rules_candidate_wasm_smoke PRIVATE c_std_17)
set_target_properties(game_rules_candidate_wasm_smoke PROPERTIES C_EXTENSIONS OFF)
game_rules_c_apply_options(game_rules_candidate_wasm_smoke)

if(BUILD_TESTING)
    add_test(NAME game_rules.candidate.lifecycle COMMAND game_rules_candidate_lifecycle_smoke)
    add_test(NAME game_rules.candidate.allocation_failure COMMAND game_rules_candidate_allocation_failure)
    if(EMSCRIPTEN)
        set_tests_properties(game_rules.candidate.lifecycle PROPERTIES LABELS "candidate;wasm")
        set_tests_properties(game_rules.candidate.allocation_failure PROPERTIES LABELS "candidate;wasm")
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
        add_test(NAME game_rules.candidate.wasm_smoke COMMAND game_rules_candidate_wasm_smoke)
        set_tests_properties(game_rules.candidate.wasm_smoke PROPERTIES LABELS "candidate;native;wasm-smoke")
    endif()
endif()

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    install(TARGETS game_rules_state_c ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}")
    install(FILES include/game_rules/c_api.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/game_rules")
    install(FILES include/game_rules/c_allocator_api.h DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/game_rules")
    install(FILES THIRD_PARTY_NOTICES.md DESTINATION "${CMAKE_INSTALL_DATADIR}/GameRulesStateC")
endif()
