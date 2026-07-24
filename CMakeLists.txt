cmake_minimum_required(VERSION 3.25)

project(
    GameRulesState
    VERSION 0.1.0
    DESCRIPTION "Deterministic state and rules engine for a puzzle game"
    LANGUAGES C CXX
)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/GameRulesWarnings.cmake)

option(GAME_RULES_BUILD_WASM "Build the Emscripten JavaScript/WebAssembly adapter" OFF)
option(GAME_RULES_ENABLE_EXCEPTIONS "Enable C++ exceptions in the portable core" OFF)
option(GAME_RULES_ENABLE_RTTI "Enable RTTI in the portable core" OFF)
option(GAME_RULES_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(GAME_RULES_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

# yyjson 0.12.0 is vendored unmodified under vendor/yyjson. Keep it isolated
# from project warning flags and out of the public API; see its provenance README.
add_library(game_rules_yyjson OBJECT src/level_json_yyjson.c)
target_compile_features(game_rules_yyjson PRIVATE c_std_99)
set_target_properties(
    game_rules_yyjson
    PROPERTIES
        C_EXTENSIONS OFF
        POSITION_INDEPENDENT_CODE ON
)
target_compile_definitions(
    game_rules_yyjson
    PRIVATE
        YYJSON_DISABLE_INCR_READER=1
        YYJSON_DISABLE_NON_STANDARD=1
        YYJSON_DISABLE_UTILS=1
        YYJSON_DISABLE_WRITER=1
)
target_include_directories(game_rules_yyjson PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/vendor/yyjson")
game_rules_set_runtime_options(game_rules_yyjson)
if(EMSCRIPTEN)
    # At -O0 yyjson_read_opts exceeds WebAssembly's per-function local limit in current
    # Emscripten releases. Keep the project debug build while lowering this vendored parser object.
    target_compile_options(game_rules_yyjson PRIVATE -O1)
endif()

add_library(game_rules_state STATIC)
add_library(GameRules::State ALIAS game_rules_state)

target_sources(
    game_rules_state
    PRIVATE
        $<TARGET_OBJECTS:game_rules_yyjson>
        src/c_api.cpp
        src/engine.cpp
        src/explosions.cpp
        src/fixtures.cpp
        src/gravity.cpp
        src/level_json.cpp
        src/player_movement.cpp
        src/ramps.cpp
        src/world.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/game_rules/c_api.h
            include/game_rules/engine.hpp
            include/game_rules/level_json.hpp
            include/game_rules/types.hpp
            include/game_rules/world.hpp
)

target_compile_features(game_rules_state PUBLIC cxx_std_20)
set_target_properties(
    game_rules_state
    PROPERTIES
        CXX_EXTENSIONS OFF
        EXPORT_NAME State
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(
    game_rules_state
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/vendor/yyjson"
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

target_compile_definitions(
    game_rules_state
    PRIVATE
        YYJSON_DISABLE_INCR_READER=1
        YYJSON_DISABLE_NON_STANDARD=1
        YYJSON_DISABLE_UTILS=1
        YYJSON_DISABLE_WRITER=1
)

game_rules_set_project_warnings(game_rules_state)
game_rules_set_runtime_options(game_rules_state)

if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif()

if(GAME_RULES_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "GAME_RULES_BUILD_WASM requires the Emscripten toolchain. Configure with emcmake.")
    endif()
    add_subdirectory(integrations/wasm)
endif()

install(
    TARGETS game_rules_state
    EXPORT GameRulesStateTargets
    FILE_SET public_headers
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/GameRulesStateConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    cmake/GameRulesStateConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/GameRulesStateConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GameRulesState
)

install(
    EXPORT GameRulesStateTargets
    FILE GameRulesStateTargets.cmake
    NAMESPACE GameRules::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GameRulesState
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/GameRulesStateConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/GameRulesStateConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/GameRulesState
)

install(
    FILES vendor/yyjson/LICENSE
    DESTINATION ${CMAKE_INSTALL_DATADIR}/GameRulesState/licenses
    RENAME yyjson-LICENSE
)

install(
    FILES
        THIRD_PARTY_NOTICES.md
        vendor/yyjson/README.game-rules.md
    DESTINATION ${CMAKE_INSTALL_DATADIR}/GameRulesState/licenses
)
