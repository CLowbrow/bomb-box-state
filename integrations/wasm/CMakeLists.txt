add_executable(game_rules_wasm wasm_entry.c)
target_link_libraries(game_rules_wasm PRIVATE GameRules::StateC)
target_compile_features(game_rules_wasm PRIVATE c_std_17)

# The production WebAssembly module must exercise the C17 candidate directly.
# Keep this as a configure-time release guard so a future link regression cannot
# be hidden by passing the same JavaScript contract against the C++ reference.
get_target_property(_game_rules_wasm_link_libraries game_rules_wasm LINK_LIBRARIES)
if(NOT _game_rules_wasm_link_libraries STREQUAL "GameRules::StateC")
    message(FATAL_ERROR
        "game_rules_wasm must link GameRules::StateC directly; got: "
        "${_game_rules_wasm_link_libraries}")
endif()

set_target_properties(
    game_rules_wasm
    PROPERTIES
        C_EXTENSIONS OFF
        OUTPUT_NAME game_rules
        SUFFIX ".mjs"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm"
        LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/wasm_api.js"
)

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/browser_smoke.html"
    "${CMAKE_BINARY_DIR}/wasm/browser_smoke.html"
    COPYONLY
)

target_link_options(
    game_rules_wasm
    PRIVATE
        --no-entry
        -sWASM=1
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createGameRulesModule
        -sENVIRONMENT=web,worker,node
        -sFILESYSTEM=0
        "-sEXPORTED_FUNCTIONS=['_game_rules_api_version','_game_rules_engine_status','_game_rules_engine_create','_game_rules_engine_destroy','_game_rules_engine_load_level','_game_rules_engine_get_state','_game_rules_engine_move','_game_rules_engine_rewind','_game_rules_string_free']"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','UTF8ToString','lengthBytesUTF8']"
        "--post-js=${CMAKE_CURRENT_SOURCE_DIR}/wasm_api.js"
)

if(BUILD_TESTING)
    find_program(GAME_RULES_NODE_EXECUTABLE node)
    if(GAME_RULES_NODE_EXECUTABLE)
        add_test(
            NAME game_rules.wasm.smoke
            COMMAND
                "${GAME_RULES_NODE_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.mjs"
                "$<TARGET_FILE:game_rules_wasm>"
                "${PROJECT_SOURCE_DIR}/tests/contracts/browser_vertical_slice/v1/contract.txt"
                "${PROJECT_SOURCE_DIR}/tests/contracts/engine_hardening/v1/lifecycle.txt"
                "${PROJECT_SOURCE_DIR}/tests/contracts/engine_hardening/v1/conflicts.txt"
                "${PROJECT_SOURCE_DIR}/tests/contracts/engine_hardening/v1/rewind-stress.txt"
        )
        set_tests_properties(game_rules.wasm.smoke PROPERTIES LABELS "integration;wasm")
    else()
        message(WARNING "Node.js was not found; the Wasm smoke test will not be registered.")
    endif()
endif()
