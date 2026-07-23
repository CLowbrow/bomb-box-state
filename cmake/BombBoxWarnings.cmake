function(bomb_box_set_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
        if(BOMB_BOX_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(
            ${target}
            PRIVATE
                -Wall
                -Wextra
                -Wpedantic
                -Wconversion
                -Wshadow
                -Wsign-conversion
        )
        if(BOMB_BOX_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

function(bomb_box_set_runtime_options target)
    if(NOT BOMB_BOX_ENABLE_RTTI)
        if(MSVC)
            target_compile_options(${target} PRIVATE /GR-)
        else()
            target_compile_options(${target} PRIVATE -fno-rtti)
        endif()
    endif()

    if(NOT BOMB_BOX_ENABLE_EXCEPTIONS)
        if(MSVC)
            target_compile_options(${target} PRIVATE /EHs-c-)
            target_compile_definitions(${target} PRIVATE _HAS_EXCEPTIONS=0)
        else()
            target_compile_options(${target} PRIVATE -fno-exceptions)
        endif()
    endif()

    if(BOMB_BOX_ENABLE_SANITIZERS)
        if(MSVC)
            message(FATAL_ERROR "The sanitizer preset currently supports Clang and GCC toolchains only.")
        endif()
        if(EMSCRIPTEN)
            message(FATAL_ERROR "Use Emscripten's sanitizer settings explicitly; the native preset is not portable to Wasm.")
        endif()
        target_compile_options(${target} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(${target} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
    endif()
endfunction()

