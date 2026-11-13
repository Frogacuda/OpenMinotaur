# Apply warnings to project-owned compiled targets, never vendored dependencies.
function(mino_apply_project_policy directory)
    get_property(targets DIRECTORY "${directory}" PROPERTY BUILDSYSTEM_TARGETS)
    foreach(target IN LISTS targets)
        if(target MATCHES "^mino_(glad|miniz|pugixml)$")
            continue()
        endif()
        get_target_property(type ${target} TYPE)
        if(type MATCHES "^(STATIC_LIBRARY|SHARED_LIBRARY|OBJECT_LIBRARY|EXECUTABLE)$")
            if(MSVC)
                # Match GCC/Clang's unused-parameter policy for interface hooks.
                target_compile_options(${target} PRIVATE /W4 /wd4100)
                # Portable C APIs are intentional; validate their results at the
                # call site rather than replacing them with Windows-only APIs.
                target_compile_definitions(${target} PRIVATE _CRT_SECURE_NO_WARNINGS)
            else()
                target_compile_options(${target} PRIVATE -Wall -Wextra -Wno-unused-parameter)
            endif()
            foreach(vendor IN ITEMS stb miniz pugixml glad/include)
                target_include_directories(${target} SYSTEM PRIVATE
                    "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../third_party/${vendor}")
            endforeach()
            if(type STREQUAL "EXECUTABLE" AND target MATCHES "checks|parity|replay|^mino_goatup2_runtime$")
                # The explicit check target owns test builds; normal builds
                # produce applications and their required libraries.
                set_target_properties(${target} PROPERTIES EXCLUDE_FROM_ALL TRUE)
                # Assertions in checks must execute in Release builds too.
                if(MSVC)
                    target_compile_options(${target} PRIVATE
                        "/FI${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/check_assertions.h")
                else()
                    target_compile_options(${target} PRIVATE -include
                        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../tests/check_assertions.h")
                endif()
            endif()
        endif()
    endforeach()
    get_property(children DIRECTORY "${directory}" PROPERTY SUBDIRECTORIES)
    foreach(child IN LISTS children)
        if(child MATCHES "/(third_party|SDL3)(/|$)")
            continue()
        endif()
        mino_apply_project_policy("${child}")
    endforeach()
endfunction()
