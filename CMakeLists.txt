cmake_minimum_required(VERSION 3.25)

project(
    BombBoxState
    VERSION 0.1.0
    DESCRIPTION "Deterministic state and rules engine for Bomb Box"
    LANGUAGES CXX
)

include(CTest)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
include(cmake/BombBoxWarnings.cmake)

option(BOMB_BOX_BUILD_WASM "Build the Emscripten JavaScript/WebAssembly adapter" OFF)
option(BOMB_BOX_ENABLE_EXCEPTIONS "Enable C++ exceptions in the portable core" OFF)
option(BOMB_BOX_ENABLE_RTTI "Enable RTTI in the portable core" OFF)
option(BOMB_BOX_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(BOMB_BOX_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)

add_library(bomb_box_state STATIC)
add_library(BombBox::State ALIAS bomb_box_state)

target_sources(
    bomb_box_state
    PRIVATE
        src/c_api.cpp
        src/engine.cpp
    PUBLIC
        FILE_SET public_headers
        TYPE HEADERS
        BASE_DIRS include
        FILES
            include/bomb_box/c_api.h
            include/bomb_box/engine.hpp
            include/bomb_box/types.hpp
)

target_compile_features(bomb_box_state PUBLIC cxx_std_20)
set_target_properties(
    bomb_box_state
    PROPERTIES
        CXX_EXTENSIONS OFF
        EXPORT_NAME State
        POSITION_INDEPENDENT_CODE ON
)

target_include_directories(
    bomb_box_state
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)

bomb_box_set_project_warnings(bomb_box_state)
bomb_box_set_runtime_options(bomb_box_state)

if(BUILD_TESTING AND NOT EMSCRIPTEN)
    add_subdirectory(tests)
endif()

if(BOMB_BOX_BUILD_WASM)
    if(NOT EMSCRIPTEN)
        message(FATAL_ERROR "BOMB_BOX_BUILD_WASM requires the Emscripten toolchain. Configure with emcmake.")
    endif()
    add_subdirectory(integrations/wasm)
endif()

install(
    TARGETS bomb_box_state
    EXPORT BombBoxStateTargets
    FILE_SET public_headers
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/BombBoxStateConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    cmake/BombBoxStateConfig.cmake.in
    "${CMAKE_CURRENT_BINARY_DIR}/BombBoxStateConfig.cmake"
    INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BombBoxState
)

install(
    EXPORT BombBoxStateTargets
    FILE BombBoxStateTargets.cmake
    NAMESPACE BombBox::
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BombBoxState
)

install(
    FILES
        "${CMAKE_CURRENT_BINARY_DIR}/BombBoxStateConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/BombBoxStateConfigVersion.cmake"
    DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/BombBoxState
)
