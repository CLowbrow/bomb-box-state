add_executable(bomb_box_wasm wasm_entry.cpp)
target_link_libraries(bomb_box_wasm PRIVATE BombBox::State)
target_compile_features(bomb_box_wasm PRIVATE cxx_std_20)

set_target_properties(
    bomb_box_wasm
    PROPERTIES
        CXX_EXTENSIONS OFF
        OUTPUT_NAME bomb_box
        SUFFIX ".mjs"
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/wasm"
)

target_link_options(
    bomb_box_wasm
    PRIVATE
        --no-entry
        -sWASM=1
        -sMODULARIZE=1
        -sEXPORT_ES6=1
        -sEXPORT_NAME=createBombBoxModule
        -sENVIRONMENT=web,worker,node
        -sFILESYSTEM=0
        "-sEXPORTED_FUNCTIONS=['_bomb_box_api_version','_bomb_box_engine_status']"
        "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"
)

if(BUILD_TESTING)
    find_program(BOMB_BOX_NODE_EXECUTABLE node)
    if(BOMB_BOX_NODE_EXECUTABLE)
        add_test(
            NAME bomb_box.wasm.smoke
            COMMAND
                "${BOMB_BOX_NODE_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/smoke_test.mjs"
                "$<TARGET_FILE:bomb_box_wasm>"
        )
        set_tests_properties(bomb_box.wasm.smoke PROPERTIES LABELS "integration;wasm")
    else()
        message(WARNING "Node.js was not found; the Wasm smoke test will not be registered.")
    endif()
endif()

