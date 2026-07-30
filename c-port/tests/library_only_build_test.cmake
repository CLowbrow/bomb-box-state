if(NOT DEFINED SOURCE_DIR OR NOT DEFINED BINARY_DIR OR NOT DEFINED GENERATOR)
    message(FATAL_ERROR "SOURCE_DIR, BINARY_DIR, and GENERATOR are required")
endif()

file(REMOVE_RECURSE "${BINARY_DIR}")
execute_process(
    COMMAND
        "${CMAKE_COMMAND}"
        -S "${SOURCE_DIR}"
        -B "${BINARY_DIR}"
        -G "${GENERATOR}"
        -DBUILD_TESTING=OFF
        -DGAME_RULES_C_WARNINGS_AS_ERRORS=ON
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)
if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "candidate library-only configure failed:\n${configure_output}\n${configure_error}")
endif()

execute_process(
    COMMAND
        "${CMAKE_COMMAND}" --build "${BINARY_DIR}"
        --target game_rules_candidate_lifecycle_smoke
    RESULT_VARIABLE test_target_result
    OUTPUT_VARIABLE test_target_output
    ERROR_VARIABLE test_target_error
)
if(test_target_result EQUAL 0)
    message(FATAL_ERROR
        "BUILD_TESTING=OFF still exposes candidate test executables:\n"
        "${test_target_output}\n${test_target_error}")
endif()
